/*
 * FOCADC.c
 *
 *  Created on: 2026年9月16日
 *      Author: jwzho
 */

#include "user.h"
#include "FOCADC.h"

/* 三个 ADC 模块的基地址数组，初始化时循环处理 */
static const uint32_t AdcBases[3] =
{
    ADCA_BASE,
    ADCB_BASE,
    ADCC_BASE
};

/* 转换完成标志连续未置位的计数，用于判 ADC 停摆 */
static Uint16 AdcEocMissCnt = 0;

/*==============================================================================
                            初始化

  调用时机：必须在 EPWM 初始化之后（因为要依赖 EPWM1 的 SOC 触发），
            且在中断使能之前。上电到第一次采样之间需要 ≥500us 的稳定时间，
            见下方 ADC_enableConverter 的注释。
==============================================================================*/
void ADC_INIT(void)
{
    Uint16 i;

    for (i = 0; i < 3; i++)
    {
        /* ---- 1) ADC 时钟分频 ----
           必须配置：复位值是 1 分频，即 SYSCLK=200MHz 直接进 ADC，
           远超 F2837xD ADC 允许的 50MHz 上限。 */
        ADC_setPrescaler(AdcBases[i], ADC_CLK_DIVIDER);

        /* ---- 2) 分辨率与信号模式 ----
           单端模式在 driverlib 里被断言强制只能配 12 位，这是器件限制。
           本函数同时会把器件 OTP 里的偏置/线性度修调值装载进去，
           所以不需要再单独调用 ADC_setOffsetTrim / ADC_setINLTrim。 */
        ADC_setMode(AdcBases[i], ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);

        /* ---- 3) 中断脉冲位置：转换结束 ----
           这里只把标志位当状态用（判断本轮转换是否完成），
           不会产生 CPU 中断——因为下面只调 ADC_enableInterrupt 置使能位，
           没有调用 Interrupt_enable(INT_ADCAx) 去打通 PIE 通路。 */
        ADC_setInterruptPulseMode(AdcBases[i], ADC_PULSE_END_OF_CONV);

        /* ---- 4) ADC 上电 ----
           上电后需要至少 500us 才能开始可靠采样。本函数在 main() 里
           被调用，之后还要配置编码器、PI、串口等，再使能中断，
           时间上远超过 500us，无需额外延时。 */
        ADC_enableConverter(AdcBases[i]);

        /* ---- 5) SOC 优先级 ----
           默认是轮询（round-robin），多个 SOC 同优先级时会按顺序轮转，
           可能打乱"三相同时采样"的意图。这里统一设为高优先级。 */
        ADC_setSOCPriority(AdcBases[i], ADC_PRI_ALL_HIPRI);
    }

    /* ---- 6) SOC0：由 EPWM1 的 ADCSOCA 事件触发 ----
       三个模块用同一个 SOC 编号、同一个触发源，
       硬件上就会在 EPWM1 发出 SOCA 的那一刻同时启动转换。 */
    ADC_setupSOC(ADC_IA_BASE,  ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA,
                 ADC_IA_CHANNEL,  ADC_SAMPLE_WINDOW);
    ADC_setupSOC(ADC_IB_BASE,  ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA,
                 ADC_IB_CHANNEL,  ADC_SAMPLE_WINDOW);
    ADC_setupSOC(ADC_UDC_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA,
                 ADC_UDC_CHANNEL, ADC_SAMPLE_WINDOW);

    /* ---- 7) 转换完成标志源：SOC0 的转换结束 ----
       锁存住以便主中断查询；查询后要清一次，否则一直保持置位。 */
    for (i = 0; i < 3; i++)
    {
        ADC_setInterruptSource(AdcBases[i], ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
        ADC_enableInterrupt(AdcBases[i], ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(AdcBases[i], ADC_INT_NUMBER1);
    }

    AdcEocMissCnt = 0;
}

/*==============================================================================
                            采样

  在 MainISR 的第 1 段调用。只做三次结果寄存器读取与换算，不阻塞。

  关于"读到的是上一轮的转换结果"：这是有意设计。EPWM1 在计数器峰值
  发出 SOC 启动转换，主中断在计数器谷值运行，两者相隔 50us，
  12 位转换最长约 2us，所以读取时结果必然已经就绪。
==============================================================================*/
void ADC_CTRL(MOTOR *v)
{
    Uint16 eocOk;

    /* 三个模块只要有一个转换完成标志置位，就说明本轮的 EPWM 触发生效了。
       标志在读取后清除，下一轮由硬件重新置位。 */
    eocOk = 0U;
    if (ADC_getInterruptStatus(ADC_IA_BASE, ADC_INT_NUMBER1) == true)
    {
        eocOk = 1U;
        ADC_clearInterruptStatus(ADC_IA_BASE, ADC_INT_NUMBER1);
    }

    /* ADCB / ADCC 的结果一并读取，标志同样清掉，避免残留累积 */
    ADC_clearInterruptStatus(ADC_IB_BASE,  ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADC_UDC_BASE, ADC_INT_NUMBER1);

    /* ---- 读数与换算 ---- */
    v->Ia = ADC_CODE_TO_PU(ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0))
            * ADC_IA_SCALE;

    v->Ib = ADC_CODE_TO_PU(ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0))
            * ADC_IB_SCALE;

    v->Udc = ADC_CODE_TO_PU(ADC_readResult(ADCCRESULT_BASE, ADC_SOC_NUMBER0))
             * ADC_UDC_SCALE;

    /* ---- ADC 停摆检测 ----
       正常情况下标志每拍都会被硬件重新置位。连续多拍都没置位，
       说明 SOC 触发或 ADC 模块出了问题，置位交给保护模块跳闸。
       注意这里不阻塞、不等待，只是计数。 */
    if (eocOk != 0U)
    {
        AdcEocMissCnt = 0U;
        v->ADTimeout = 0U;
    }
    else
    {
        AdcEocMissCnt++;
        if (AdcEocMissCnt >= ADC_EOC_TIMEOUT_TICKS)
        {
            v->ADTimeout = 1U;
        }
    }
}
