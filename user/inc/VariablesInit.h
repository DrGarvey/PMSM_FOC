/*
 * VariablesInit.h
 *
 *  Created on: 2024年6月21日
 *      Author: jwzho
 */

#ifndef USER_INC_VARIABLESINIT_H_
#define USER_INC_VARIABLESINIT_H_

//#include "user.h"

// 全局变量的定义
// _iq 是IQmath的数据类型，与int，float类似
// 注意：本工程的 IQmathLib.h 里 MATH_TYPE 定义为 FLOAT_MATH，
// 因此 _iq 实际就是 float，_IQ()/_IQ30() 等宏都是恒等展开，
// 各头文件里标注的 "Q24/Q30/Q21" 只是 TI 原始库的遗留说明，对本工程不适用。

_iq OffsetFlag=0;    //零漂采集标志位
_iq offsetA=0;          //ABC三相的零漂
_iq offsetB=0;
_iq offsetUdc=0;

_iq ExDA_A = 0;
_iq ExDA_B = 0;
_iq ExDA_C = 0;
_iq ExDA_D = 0;

_iq ExDA_A_Test = 0.5;
_iq ExDA_B_Test = -0.5;
_iq ExDA_C_Test = 0.2;
_iq ExDA_D_Test = 0;

//卡尔曼滤波器参数设定，卡尔曼滤波主要在采集系统零漂时工作，起到低通滤波器的效果
_iq K1=_IQ(0.998);      //零漂滤波系数 K1：0.05/(T+0.05)；K1是卡尔曼滤波器的一个参数，由采样时间T决定
_iq K2=_IQ(0.001999);   //零漂滤波系数 K2：T/(T+0.05)；K2是卡尔曼滤波器的另一个参数

// 说明：TI 原始例程在这里声明 IQsinTable[]/IQcosTable[] 正弦查表，
// 那是 IQ_MATH 模式下的实现；本工程用 FLOAT_MATH，_IQsinPU/_IQcosPU 直接映射到
// 硬件三角函数指令，不再使用查表，故这两条声明已删除。

//闭环测试时使用的给定值，该数值为标幺值，非实际值，范围为(-1,1)
//标幺的标准可以参考HVPM_Sensorless-Settings.h文件中的设定
_iq VdTesting = _IQ(0.0);           // Vd 给定值 (标幺)
_iq VqTesting = _IQ(0.2);           // Vq 给定值 (标幺)
_iq IdRef = _IQ(0.0);               // Id 给定值 (标幺)
_iq IqRef = _IQ(0.04);               // Iq 给定值 (标幺)
_iq IdLockRef = _IQ(0.01);
//速度也是标幺的
_iq  SpeedRef = _IQ(0.025);           // 用于闭环测试

//采样时间
float32 T = 0.001/ISR_FREQUENCY;    // 采样周期 (秒)，见 parameter.h

//一些程序中用到的标志位
Uint32 IsrTicker = 0;
Uint16 BackTicker = 0;
Uint32 MainIsrTicker = 0;
Uint32 OffsetIsrTicker = 0;
Uint16 lsw=0;                       //lsw标志位比较重要，是切换程序运行状态的重要标志位
                                    //语义已统一：0=抱轴/停止 1=电流环 2=速度环 3=跳过计算
                                    //（原 TripFlagDMC 已被 protect1 取代，见文件末尾）
Uint32 Init_IFlag=0;                //编码器 index 标定完成标志；lsw 由 0 切到非 0 时会自动清零重标

Uint32 BUILDLEVEL = 0;

Uint32 LV1Ticker = 0;
Uint32 LV2Ticker = 0;
Uint32 LV3Ticker = 0;
Uint32 LV5Ticker = 0;
Uint32 TestTicker=0;
Uint32 TestTicker1=0;

// 由于电机的机械时间常数要大于电气时间常数，因此速度环计算的频率一般要比电流环低
// 通过对SpeedLoopPrescaler的设置，可以设定n次电流环计算后进行一次速度环计算
Uint16 SpeedLoopPrescaler = 10;      // 速度环预分频
Uint16 SpeedLoopCount = 1;           // 速度环计数器

// QEP变量初始化
// QEP是自定义的一个结构体，结构体包含的内容见f2833xqep.h
// qep1是自定义的变量名
// QEP_DEFAULTS是初始化的赋值，内容见f2833xqep.h
// 该语句的含义是，定义一个QEP类型的结构体变量qep1，并赋初值
// 下面程序中类似的语句含义是相似的，不做具体介绍
QEP qep1 = QEP_DEFAULTS;

// 坐标变换变量初始化
CLARKE clarke1 = CLARKE_DEFAULTS;
PARK park1 = PARK_DEFAULTS;
IPARK ipark1 = IPARK_DEFAULTS;

// PI变量初始化
PI_CONTROLLER pi_spd = PI_CONTROLLER_DEFAULTS;
PI_CONTROLLER pi_id  = PI_CONTROLLER_DEFAULTS;
PI_CONTROLLER pi_iq  = PI_CONTROLLER_DEFAULTS;

// PWM变量初始化，主要是对PWM硬件模块寄存器所需要的数值进行初始化
PWMGEN pwm1 = PWMGEN_DEFAULTS;

// Instance a Space Vector PWM modulator. This modulator generates a, b and c
// phases based on the d and q stationery reference frame inputs
// SVPWM变量初始化，主要是对SVPWM算法所需要的变量进行初始化
SVGEN svgen1 = SVGEN_DEFAULTS;

// Instance a ramp controller to smoothly ramp the frequency
// 仿真中一般给阶跃信号，实际系统中为了防止阶跃信号过大导致的问题，加入了一个过度环节，
// 将阶跃信号转换为一个梯度信号，由于DSP计算速度比较快，因此该梯度信号可以近似为一个阶跃信号
RMPCNTL rc1 = RMPCNTL_DEFAULTS;

// 该结构体变量主要为了产生一个三角波以计算出角度信息，为系统的VF控制提供一个虚拟的角度信号
//  Instance a ramp generator to simulate an Anglele
RAMPGEN rg1 = RAMPGEN_DEFAULTS;

// 电压
//  Instance a phase voltage calculation
PHASEVOLTAGE volt1 = PHASEVOLTAGE_DEFAULTS;

// 速度
// Instance a speed calculator based on QEP
SPEED_MEAS_QEP speed1 = SPEED_MEAS_QEP_DEFAULTS;

MOTOR motor1 = MOTOR_DEFAULTS;

// 故障保护对象
// 运行期间在 CCS 的 Expressions 窗口观察这几个成员：
//   protect1.FaultCode  —— 故障码（0=无故障，含义见 FOCProtect.h）
//   protect1.Tripped    —— 1 = 已跳闸，PWM 被硬件封锁
//   protect1.TripCount  —— 累计跳闸次数
// 恢复方法：排查完故障原因后，手工把 protect1.RecoverReq 置 1。
// 恢复后 lsw 会被强制清 0（回到抱轴态），必须重新置位才能继续运行。
PROTECT protect1 = PROTECT_DEFAULTS;

#endif /* USER_INC_VARIABLESINIT_H_ */
