/*
 * FOCPWM.c
 *
 *  Created on: 2024年6月20日
 *      Author: jwzho
 */

#include "hardware.h"


void PWM_INIT(PWMGEN *v)
{
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
