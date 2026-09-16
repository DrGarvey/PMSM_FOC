/*
 * globals.h
 *
 * 全局变量声明（extern）。定义见 globals.c。
 * 原 VariablesInit.h 中的变量定义已拆分：声明放这里，定义放 globals.c，
 * 避免在头文件内定义变量（否则被多个 .c 包含会导致重复定义）。
 */

#ifndef CONTROL_GLOBALS_H_
#define CONTROL_GLOBALS_H_

#include "hardware.h"        // 依赖 QEP/CLARKE/PARK/... 等结构体类型

/* ---- 程序使能 / 构建级别 ---- */
extern volatile Uint16 EnableFlag;   // 程序使能，烧录前须置 TRUE
extern Uint32 BUILDLEVEL;            // 决定 ISR 中执行哪一级控制算法

/* ---- 运行状态标志 ---- */
extern Uint16 lsw;                   // 运行状态切换标志 (0=抱轴,1=电流环,2=速度环)
extern Uint16 TripFlagDMC;           // PWM 保护状态
extern Uint32 Init_IFlag;            // 编码器 index 同步标志

/* ---- 零漂 / 滤波 ---- */
extern _iq Udc;                      // 母线电压
extern _iq OffsetFlag;
extern _iq offsetA;
extern _iq offsetB;
extern _iq offsetUdc;
extern _iq K1;                       // 零漂低通滤波系数
extern _iq K2;

/* ---- DA 观测输出 ---- */
extern _iq ExDA_A;
extern _iq ExDA_B;
extern _iq ExDA_C;
extern _iq ExDA_D;

/* ---- 闭环测试给定值（标幺） ---- */
extern _iq VdTesting;
extern _iq VqTesting;
extern _iq IdRef;
extern _iq IqRef;
extern _iq IdLockRef;
extern _iq SpeedRef;

/* ---- 速度环分频 ---- */
extern Uint16 SpeedLoopPrescaler;
extern Uint16 SpeedLoopCount;

/* ---- 调试计数器 ---- */
extern Uint32 IsrTicker;
extern Uint16 BackTicker;
extern Uint32 MainIsrTicker;
extern Uint32 OffsetIsrTicker;
extern Uint32 LV1Ticker;
extern Uint32 LV2Ticker;
extern Uint32 LV3Ticker;
extern Uint32 LV5Ticker;
extern Uint32 TestTicker;
extern Uint32 TestTicker1;

/* ---- FOC 算法对象 ---- */
extern QEP qep1;
extern CLARKE clarke1;
extern PARK park1;
extern IPARK ipark1;
extern PI_CONTROLLER pi_spd;
extern PI_CONTROLLER pi_id;
extern PI_CONTROLLER pi_iq;
extern PWMGEN pwm1;
extern SVGEN svgen1;
extern RMPCNTL rc1;
extern RAMPGEN rg1;
extern PHASEVOLTAGE volt1;
extern SPEED_MEAS_QEP speed1;
extern MOTOR motor1;

#endif /* CONTROL_GLOBALS_H_ */
