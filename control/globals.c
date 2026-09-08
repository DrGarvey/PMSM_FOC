/*
 * globals.c
 *
 * 全局变量定义。声明见 globals.h。
 * 原 VariablesInit.h 中的变量定义迁移至此。
 */

#include "globals.h"

/* ---- 程序使能 / 构建级别 ---- */
volatile Uint16 EnableFlag = TRUE;
Uint32 BUILDLEVEL = 0;

/* ---- 运行状态标志 ---- */
Uint16 lsw = 0;
Uint16 TripFlagDMC = 0;
Uint32 Init_IFlag = 0;

/* ---- 零漂 / 滤波 ---- */
_iq Udc = 0;
_iq OffsetFlag = 0;
_iq offsetA = 0;
_iq offsetB = 0;
_iq offsetUdc = 0;
_iq K1 = _IQ(0.998);          // 零漂低通滤波系数 K1
_iq K2 = _IQ(0.001999);       // 零漂低通滤波系数 K2

/* ---- DA 观测输出 ---- */
_iq ExDA_A = 0;
_iq ExDA_B = 0;
_iq ExDA_C = 0;
_iq ExDA_D = 0;

/* ---- 闭环测试给定值（标幺） ---- */
_iq VdTesting = _IQ(0.0);
_iq VqTesting = _IQ(0.2);
_iq IdRef = _IQ(0.0);
_iq IqRef = _IQ(0.04);
_iq IdLockRef = _IQ(0.01);
_iq SpeedRef = _IQ(0.025);

/* ---- 速度环分频 ---- */
Uint16 SpeedLoopPrescaler = 10;
Uint16 SpeedLoopCount = 1;

/* ---- 调试计数器 ---- */
Uint32 IsrTicker = 0;
Uint16 BackTicker = 0;
Uint32 MainIsrTicker = 0;
Uint32 OffsetIsrTicker = 0;
Uint32 LV1Ticker = 0;
Uint32 LV2Ticker = 0;
Uint32 LV3Ticker = 0;
Uint32 LV5Ticker = 0;
Uint32 TestTicker = 0;
Uint32 TestTicker1 = 0;

/* ---- FOC 算法对象 ---- */
QEP qep1 = QEP_DEFAULTS;
CLARKE clarke1 = CLARKE_DEFAULTS;
PARK park1 = PARK_DEFAULTS;
IPARK ipark1 = IPARK_DEFAULTS;
PI_CONTROLLER pi_spd = PI_CONTROLLER_DEFAULTS;
PI_CONTROLLER pi_id = PI_CONTROLLER_DEFAULTS;
PI_CONTROLLER pi_iq = PI_CONTROLLER_DEFAULTS;
PWMGEN pwm1 = PWMGEN_DEFAULTS;
SVGEN svgen1 = SVGEN_DEFAULTS;
RMPCNTL rc1 = RMPCNTL_DEFAULTS;
RAMPGEN rg1 = RAMPGEN_DEFAULTS;
PHASEVOLTAGE volt1 = PHASEVOLTAGE_DEFAULTS;
SPEED_MEAS_QEP speed1 = SPEED_MEAS_QEP_DEFAULTS;
MOTOR motor1 = MOTOR_DEFAULTS;
