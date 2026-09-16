/*
 * FOCPWM.c
 *
 *  Created on: 2024年6月20日
 *      Author: jwzho
 */

#include "hardware.h"


void PWM_INIT(PWMGEN *v)
{
    // EPWMCLK 分频：EPWMCLK = SYSCLK / 2 = 100MHz
    // 这一句原先在 AD7606 的 EMIF 初始化里（当时顺手设的），AD7606 移除后挪到这里。
    // 放在 EPWM 配置之前才是正确顺序——时基周期、死区计数的换算都以此为基准，
    // 原先把时钟分频设在 EPWM 配置之后属于隐患（当时能工作只是因为复位值恰好也是 /2）。
    SysCtl_setEPWMClockDivider(SYSCTL_EPWMCLK_DIV_2);

    InitEpwmGPIO();

    //TBCTL.CTRMODE = 2
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBaseCounterMode(EPWM2_BASE, EPWM_COUNTER_MODE_UP_DOWN);
    EPWM_setTimeBaseCounterMode(EPWM3_BASE, EPWM_COUNTER_MODE_UP_DOWN);

    //TBCTL.CLKDIV = 0 & TBCTL.HSPCLKDIV = 0
    //TBCLK = EPWMCLK / (ClockDiv * HSClockDiv);
    //需要注意的是，默认情况下，28377D的EPWMCLK = SYSCLKOUT / 2，而28335的EPWMCLK = SYSCLKOUT
    EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setClockPrescaler(EPWM2_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setClockPrescaler(EPWM3_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);

    //TBCTL.PHSDIR = 1
    EPWM_setCountModeAfterSync(EPWM1_BASE,EPWM_COUNT_MODE_UP_AFTER_SYNC);
    EPWM_setCountModeAfterSync(EPWM2_BASE,EPWM_COUNT_MODE_UP_AFTER_SYNC);
    EPWM_setCountModeAfterSync(EPWM3_BASE,EPWM_COUNT_MODE_UP_AFTER_SYNC);

    //TBPRD
    EPWM_setTimeBasePeriod(EPWM1_BASE, v->PeriodMax);
    EPWM_setTimeBasePeriod(EPWM2_BASE, v->PeriodMax);
    EPWM_setTimeBasePeriod(EPWM3_BASE, v->PeriodMax);

    //TBCTL.SYNCOSEL = 0
    EPWM_setSyncOutPulseMode(EPWM1_BASE, EPWM_SYNC_OUT_PULSE_ON_SOFTWARE);
    EPWM_setSyncOutPulseMode(EPWM2_BASE, EPWM_SYNC_OUT_PULSE_ON_SOFTWARE);
    EPWM_setSyncOutPulseMode(EPWM3_BASE, EPWM_SYNC_OUT_PULSE_ON_SOFTWARE);

    //TBCTL.PHSEN = 0
    EPWM_disablePhaseShiftLoad(EPWM1_BASE);
    EPWM_disablePhaseShiftLoad(EPWM2_BASE);
    EPWM_disablePhaseShiftLoad(EPWM3_BASE);

    //TBCTL.FREE_SOFT = 2
    EPWM_setEmulationMode(EPWM1_BASE, EPWM_EMULATION_FREE_RUN);
    EPWM_setEmulationMode(EPWM2_BASE, EPWM_EMULATION_FREE_RUN);
    EPWM_setEmulationMode(EPWM3_BASE, EPWM_EMULATION_FREE_RUN);

    //TBCTL.PRDLD = 1
//    EPWM_setPeriodLoadMode(EPWM1_BASE, EPWM_PERIOD_DIRECT_LOAD);
//    EPWM_setPeriodLoadMode(EPWM2_BASE, EPWM_PERIOD_DIRECT_LOAD);
//    EPWM_setPeriodLoadMode(EPWM3_BASE, EPWM_PERIOD_DIRECT_LOAD);

    //CPMCTL.LOADAMODE = 0
    EPWM_setCounterCompareShadowLoadMode(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareShadowLoadMode(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareShadowLoadMode(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);

    //AQCTLA寄存器设置
    EPWM_setActionQualifierActionComplete(EPWM1_BASE,EPWM_AQ_OUTPUT_A,(EPWM_AQ_OUTPUT_LOW_UP_CMPA | EPWM_AQ_OUTPUT_HIGH_DOWN_CMPA));
    EPWM_setActionQualifierActionComplete(EPWM2_BASE,EPWM_AQ_OUTPUT_A,(EPWM_AQ_OUTPUT_LOW_UP_CMPA | EPWM_AQ_OUTPUT_HIGH_DOWN_CMPA));
    EPWM_setActionQualifierActionComplete(EPWM3_BASE,EPWM_AQ_OUTPUT_A,(EPWM_AQ_OUTPUT_LOW_UP_CMPA | EPWM_AQ_OUTPUT_HIGH_DOWN_CMPA));

    //DBCTL.OUTMODE
    EPWM_setDeadBandDelayMode(EPWM1_BASE, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(EPWM1_BASE, EPWM_DB_FED, true);
    EPWM_setDeadBandDelayMode(EPWM2_BASE, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(EPWM2_BASE, EPWM_DB_FED, true);
    EPWM_setDeadBandDelayMode(EPWM3_BASE, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(EPWM3_BASE, EPWM_DB_FED, true);

    //DBCTL.IN_MODE = 0;
    EPWM_setRisingEdgeDeadBandDelayInput(EPWM1_BASE, EPWM_DB_INPUT_EPWMA);
    EPWM_setRisingEdgeDeadBandDelayInput(EPWM2_BASE, EPWM_DB_INPUT_EPWMA);
    EPWM_setRisingEdgeDeadBandDelayInput(EPWM3_BASE, EPWM_DB_INPUT_EPWMA);

    //DBCTL.POLSEL
    EPWM_setDeadBandDelayPolarity(EPWM1_BASE,EPWM_DB_FED,EPWM_DB_POLARITY_ACTIVE_LOW);
    EPWM_setDeadBandDelayPolarity(EPWM2_BASE,EPWM_DB_FED,EPWM_DB_POLARITY_ACTIVE_LOW);
    EPWM_setDeadBandDelayPolarity(EPWM3_BASE,EPWM_DB_FED,EPWM_DB_POLARITY_ACTIVE_LOW);

    EPWM_setRisingEdgeDelayCount(EPWM1_BASE, v->Deadband);
    EPWM_setFallingEdgeDelayCount(EPWM1_BASE, v->Deadband);
    EPWM_setRisingEdgeDelayCount(EPWM2_BASE, v->Deadband);
    EPWM_setFallingEdgeDelayCount(EPWM2_BASE, v->Deadband);
    EPWM_setRisingEdgeDelayCount(EPWM3_BASE, v->Deadband);
    EPWM_setFallingEdgeDelayCount(EPWM3_BASE, v->Deadband);

    //
    EPWM_setInterruptSource(EPWM1_BASE,EPWM_INT_TBCTR_ZERO);
    EPWM_setInterruptEventCount(EPWM1_BASE, 1U);
    EPWM_enableInterrupt(EPWM1_BASE);

    // ================= ADC 采样触发 =================
    // 计数器到达峰值(TBCTR = TBPRD)时发出 ADCSOCA，硬件触发三个 ADC 同时启动转换。
    //
    // 为什么放在峰值而不是与中断同在谷值：
    //   中心对齐 PWM 下，相电流在计数器谷值和峰值两处都等于其平均值，
    //   所以两个时刻采样都准确。但主中断在谷值运行(TBCTR_ZERO)，
    //   若采样也放在谷值，两者同时刻发生，中断只能读到上一整周期的数据(100us)；
    //   放在峰值则数据只滞后半个周期(50us)，控制延迟减半。
    EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_PERIOD);
    EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, 1U);
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);

}


void PWM_MACRO(PWMGEN *v)
{
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(v->MfuncC1,v->HalfPerMax) + v->HalfPerMax);
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(v->MfuncC2,v->HalfPerMax) + v->HalfPerMax);
    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(v->MfuncC3,v->HalfPerMax) + v->HalfPerMax);
}


void InitEpwmGPIO(void)
{
    GPIO_setPinConfig(GPIO_0_EPWM1A);
    GPIO_setPinConfig(GPIO_1_EPWM1B);
    GPIO_setPinConfig(GPIO_2_EPWM2A);
    GPIO_setPinConfig(GPIO_3_EPWM2B);
    GPIO_setPinConfig(GPIO_4_EPWM3A);
    GPIO_setPinConfig(GPIO_5_EPWM3B);
}

// 逆变器使能/复位引脚初始化 (GPIO8)
void InverterRST_Init(void)
{
    GPIO_setPinConfig(GPIO_8_GPIO8);
    GPIO_setDirectionMode(8, GPIO_DIR_MODE_OUT);    // GPIO8 = 输出
}

// 逆变器保护：关闭三路 PWM 输出
void InverterProtect(PWMGEN *v)
{
    _iq ClosePWM = -1.0;

    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
}
