/*
 * FOCDAC.c
 *
 *  Created on: 2026年9月16日
 *      Author: jwzho
 */

#include "user.h"
#include "FOCDAC.h"

/* PWM 滤波通道的比较值下限。不用 0 是因为 CMPA=0 在动作限定器里属于
   边界情况（零事件与比较事件同时发生），输出波形不确定。取 1 时占空比
   约 0.1%，经 RC 后约 3mV，可以当作 0V 用。 */
#define FOCDAC_PWM_CMP_MIN  1U

/*------------------------------------------------------------------------------
  内部函数：标幺值 -> 12 位 DAC 输出码
  data ∈ (-1,+1)  →  code ∈ (0, 4095)，data = 0 时输出中点 2048
------------------------------------------------------------------------------*/
static Uint16 DacCode(float data)
{
    float code = data * 2048.0f + 2048.0f;

    if (code < (float)FOCDAC_CODE_MIN)
    {
        return FOCDAC_CODE_MIN;
    }
    if (code > (float)FOCDAC_CODE_MAX)
    {
        return FOCDAC_CODE_MAX;
    }
    return (Uint16)code;
}

/*------------------------------------------------------------------------------
  内部函数：标幺值 -> PWM-DAC 的比较值
  data ∈ (-1,+1)  →  CMPA ∈ [1, TBPRD]；占空比 = CMPA / TBPRD
------------------------------------------------------------------------------*/
static Uint16 PwmDacCmp(float data)
{
    /* 占空比 = (data + 1) / 2，再乘以周期计数 */
    float cmp = (data + 1.0f) * 0.5f * (float)FOCDAC_PWM_TBPRD;

    if (cmp < (float)FOCDAC_PWM_CMP_MIN)
    {
        return FOCDAC_PWM_CMP_MIN;
    }
    if (cmp > (float)FOCDAC_PWM_TBPRD)
    {
        return FOCDAC_PWM_TBPRD;
    }
    return (Uint16)cmp;
}

/*==============================================================================
                            初始化

  调用时机：必须在 PWM_INIT() 之后。原因有两个：
    1) EPWMCLK 的分频（EPWMCLK = SYSCLK/2 = 100MHz）在 PWM_INIT 里设置，
       下面 PWM-DAC 的周期值 TBPRD 是按 100MHz 算出来的；
    2) EPWM 模块的时钟同步由 PWM_INIT 统一处理。
==============================================================================*/
void DAC_INIT(void)
{
    /*======================================================================
      一、两路片上缓冲 DAC（通道 A / B）
    ======================================================================*/
    DAC_setReferenceVoltage(DACA_BASE, FOCDAC_REFERENCE);

    /* 同步加载模式：写影子寄存器后在下个 SYSCLK 生效。
       要用 PWM 同步加载可以改成 DAC_LOAD_PWMSYNC + DAC_setPWMSyncSignal()，
       本工程是观测用途，对更新时刻不敏感，用立即加载即可。 */
    DAC_setLoadMode(DACA_BASE, DAC_LOAD_SYSCLK);

    /* 参考电压不是 2.5V 时调一次偏置修调（只能在 Device_cal 之后调一次） */
    DAC_tuneOffsetTrim(DACA_BASE, FOCDAC_REF_VOLTS);

    /* 输出使能。复位后输出是关断的，不调用这一句量不到任何电压 */
    DAC_enableOutput(DACA_BASE);

    /* 上电先输出中点，避免开机瞬间在示波器上打出跳变 */
    DAC_setShadowValue(DACA_BASE, 2048U);

    /* DACB 同上 */
    DAC_setReferenceVoltage(DACB_BASE, FOCDAC_REFERENCE);
    DAC_setLoadMode(DACB_BASE, DAC_LOAD_SYSCLK);
    DAC_tuneOffsetTrim(DACB_BASE, FOCDAC_REF_VOLTS);
    DAC_enableOutput(DACB_BASE);
    DAC_setShadowValue(DACB_BASE, 2048U);

    /*======================================================================
      二、两路 PWM + 外部 RC（通道 C / D，用 EPWM4 的 A、B 两路）

      原理：用固定载波的 PWM 输出，经外部 RC 低通后取出平均值。
        占空比 = CMPA / TBPRD，输出平均电压 = 占空比 x VREFHI
      载波 100kHz（TBPRD = 100MHz / 100kHz = 1000），
      RC 截止频率建议 1.6kHz，载波被衰减约 36dB，纹波约 3mV。
    ======================================================================*/
    GPIO_setPinConfig(GPIO_6_EPWM4A);       /* J6 第 10 脚，通道 C */
    GPIO_setPinConfig(GPIO_7_EPWM4B);       /* J6 第 9 脚，通道 D */

    /* 递增计数模式，时钟不分频（TBCLK = EPWMCLK = 100MHz） */
    EPWM_setTimeBaseCounterMode(EPWM4_BASE, EPWM_COUNTER_MODE_UP);
    EPWM_setClockPrescaler(EPWM4_BASE,
                           EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(EPWM4_BASE, FOCDAC_PWM_TBPRD);

    /* 动作限定器：计数到 0 时置高，计到 CMPA 时置低
       → 高电平持续 CMPA 个计数 → 占空比 = CMPA / TBPRD */
    EPWM_setActionQualifierActionComplete(EPWM4_BASE, EPWM_AQ_OUTPUT_A,
            (EPWM_AQ_OUTPUT_HIGH_ZERO | EPWM_AQ_OUTPUT_LOW_UP_CMPA));
    EPWM_setActionQualifierActionComplete(EPWM4_BASE, EPWM_AQ_OUTPUT_B,
            (EPWM_AQ_OUTPUT_HIGH_ZERO | EPWM_AQ_OUTPUT_LOW_UP_CMPA));

    /* 比较值在计数归零时装载，保证一个载波周期内占空比不突变 */
    EPWM_setCounterCompareShadowLoadMode(EPWM4_BASE, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareShadowLoadMode(EPWM4_BASE, EPWM_COUNTER_COMPARE_B,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);

    /* 上电先给 50% 占空比（对应中点电压），避免开机跳变 */
    EPWM_setCounterCompareValue(EPWM4_BASE, EPWM_COUNTER_COMPARE_A,
                                FOCDAC_PWM_TBPRD / 2U);
    EPWM_setCounterCompareValue(EPWM4_BASE, EPWM_COUNTER_COMPARE_B,
                                FOCDAC_PWM_TBPRD / 2U);
}

/*==============================================================================
                            每拍输出

  由 main.c 的 Observe_Run() 在每次中断末尾调用。
  两路缓冲 DAC 是两次寄存器写，两路 PWM-DAC 也是两次寄存器写，
  合计四次，开销可忽略。

  注意：这里不能有任何等待。DAC 影子寄存器的写入是即时的，
  PWM 比较值的装载由硬件在计数归零时完成，都不需要软件同步。
==============================================================================*/
void DA_Ctrl(float data_a, float data_b, float data_c, float data_d)
{
    /* 通道 A / B：片上缓冲 DAC */
    DAC_setShadowValue(DACA_BASE, DacCode(data_a));
    DAC_setShadowValue(DACB_BASE, DacCode(data_b));

    /* 通道 C / D：PWM-DAC */
    EPWM_setCounterCompareValue(EPWM4_BASE, EPWM_COUNTER_COMPARE_A,
                                PwmDacCmp(data_c));
    EPWM_setCounterCompareValue(EPWM4_BASE, EPWM_COUNTER_COMPARE_B,
                                PwmDacCmp(data_d));
}
