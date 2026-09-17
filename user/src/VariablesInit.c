/*
 * VariablesInit.c
 *
 *  Created on: 2024年6月21日
 *      Author: jwzho
 *
 *  全局变量的实际定义。
 *  声明集中在 user/inc/VariablesInit.h，那边写明了每个量的用途。
 *
 *  本文件在工程里只编译一次，所以这里的变量全工程唯一。
 */

#include "user.h"
#include "VariablesInit.h"

/*------------------------------------------------------------------------------
  零漂
------------------------------------------------------------------------------*/
_iq OffsetFlag = 0;                 //零漂采集标志位
_iq offsetA   = 0;                  //A 相零漂
_iq offsetB   = 0;                  //B 相零漂
_iq offsetUdc = 0;                  //母线电压零漂

/*------------------------------------------------------------------------------
  模拟观测输出
------------------------------------------------------------------------------*/
_iq ExDA_A = 0;
_iq ExDA_B = 0;
_iq ExDA_C = 0;
_iq ExDA_D = 0;

_iq ExDA_A_Test = 0.5;
_iq ExDA_B_Test = -0.5;
_iq ExDA_C_Test = 0.2;
_iq ExDA_D_Test = 0;

/*------------------------------------------------------------------------------
  零漂标定用的一阶低通系数：K1 = τ/(T+τ)，K2 = T/(T+τ)，τ = 0.05s
  两者之和为 1，构成 y = K1*y + K2*x 的直流平均滤波器
------------------------------------------------------------------------------*/
_iq K1 = _IQ(0.998);                //K1：0.05/(T+0.05)
_iq K2 = _IQ(0.001999);             //K2：T/(T+0.05)

/*------------------------------------------------------------------------------
  开环/闭环测试给定值（标幺）
------------------------------------------------------------------------------*/
_iq VdTesting = _IQ(0.0);           // Vd 给定值 (标幺)
_iq VqTesting = _IQ(0.2);           // Vq 给定值 (标幺)
_iq IdRef     = _IQ(0.0);           // Id 给定值 (标幺)
_iq IqRef     = _IQ(0.04);          // Iq 给定值 (标幺)
_iq IdLockRef = _IQ(0.01);          // 抱轴时把转子吸附到电气零位用的 Id 给定
_iq SpeedRef  = _IQ(0.025);         // 速度给定（标幺），用于闭环测试

/*------------------------------------------------------------------------------
  采样周期（秒）
  10kHz 中断对应 100us。写成 0.001/ISR_FREQUENCY 是为了让 ISR_FREQUENCY
  （单位 kHz，定义在 user.h）变动时这里自动跟着变。
------------------------------------------------------------------------------*/
float32 T = 0.001/ISR_FREQUENCY;    // 采样周期 (秒)

/*------------------------------------------------------------------------------
  运行标志与计数器
------------------------------------------------------------------------------*/
Uint32 IsrTicker       = 0;
Uint16 BackTicker      = 0;
Uint32 MainIsrTicker   = 0;
Uint32 OffsetIsrTicker = 0;

Uint16 lsw       = 0;               //运行状态标志，语义见 VariablesInit.h
Uint32 Init_IFlag = 0;              //编码器 index 标定完成标志

Uint32 BUILDLEVEL = 0;              //当前控制级别，决定 LevelCfg 表用哪一行

Uint32 LV1Ticker   = 0;
Uint32 LV2Ticker   = 0;
Uint32 LV3Ticker   = 0;
Uint32 LV5Ticker   = 0;
Uint32 TestTicker  = 0;
Uint32 TestTicker1 = 0;

/*------------------------------------------------------------------------------
  速度环预分频
------------------------------------------------------------------------------*/
Uint16 SpeedLoopPrescaler = 10;     // 速度环预分频
Uint16 SpeedLoopCount     = 1;      // 速度环计数器

/*------------------------------------------------------------------------------
  FOC 运算对象（各结构体的 _DEFAULTS 见对应的头文件）
------------------------------------------------------------------------------*/

// 编码器
QEP qep1 = QEP_DEFAULTS;

// 坐标变换
CLARKE clarke1 = CLARKE_DEFAULTS;
PARK   park1   = PARK_DEFAULTS;
IPARK  ipark1  = IPARK_DEFAULTS;

// PI 控制器
PI_CONTROLLER pi_spd = PI_CONTROLLER_DEFAULTS;
PI_CONTROLLER pi_id  = PI_CONTROLLER_DEFAULTS;
PI_CONTROLLER pi_iq  = PI_CONTROLLER_DEFAULTS;

// PWM 输出
PWMGEN pwm1 = PWMGEN_DEFAULTS;

// 空间矢量调制
SVGEN svgen1 = SVGEN_DEFAULTS;

// 斜坡给定：实际系统中为防止阶跃给定过大引起的问题，在给定值前加一个斜坡环节
// 把阶跃变成梯度。由于 DSP 计算很快，这个梯度可以近似看作阶跃
RMPCNTL rc1 = RMPCNTL_DEFAULTS;

// 虚拟角度发生器：产生 0~1 循环的角度，为 VF 控制提供虚拟角度信号
RAMPGEN rg1 = RAMPGEN_DEFAULTS;

// 相电压计算
PHASEVOLTAGE volt1 = PHASEVOLTAGE_DEFAULTS;

// 基于 QEP 的速度计算
SPEED_MEAS_QEP speed1 = SPEED_MEAS_QEP_DEFAULTS;

// 采样结果
MOTOR motor1 = MOTOR_DEFAULTS;

/*------------------------------------------------------------------------------
  故障保护对象
------------------------------------------------------------------------------*/
PROTECT protect1 = PROTECT_DEFAULTS;
