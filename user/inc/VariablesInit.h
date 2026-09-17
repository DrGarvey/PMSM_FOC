/*
 * VariablesInit.h
 *
 *  Created on: 2024年6月21日
 *      Author: jwzho
 *
 *  全局变量声明。
 *
 *  ----------------------------------------------------------------
 *  本文件只放 extern 声明，实际定义在 user/src/VariablesInit.c
 *  ----------------------------------------------------------------
 *  这样任何 .c 文件都可以 include 本头文件来访问这些全局量，
 *  不会产生重复定义。
 *
 *  （早期版本是把定义直接写在本头文件里的，那时只有 main.c 能包含它，
 *    其它模块要用全局量就只能自己打包一份快照结构体传进去。
 *    随模块增多这种方式难以为继，故改成标准的声明/定义分离。）
 *
 *  _iq 是 IQmath 的数据类型。注意本工程的 IQmathLib.h 里 MATH_TYPE 定义为
 *  FLOAT_MATH，因此 _iq 实际上就是 float，_IQ()/_IQ30() 等宏都是恒等展开，
 *  各头文件里标注的 "Q24/Q30/Q21" 只是 TI 原始库的遗留说明，对本工程不适用。
 */

#ifndef USER_INC_VARIABLESINIT_H_
#define USER_INC_VARIABLESINIT_H_

/*------------------------------------------------------------------------------
  零漂（上电标定，见 main.c 的 OffsetISR）
------------------------------------------------------------------------------*/
extern _iq OffsetFlag;              //零漂采集标志位
extern _iq offsetA;                 //A 相零漂
extern _iq offsetB;                 //B 相零漂
extern _iq offsetUdc;               //母线电压零漂

/*------------------------------------------------------------------------------
  模拟观测输出（由 Observe_Run 每拍刷新，DAC 模块负责输出）
------------------------------------------------------------------------------*/
extern _iq ExDA_A;
extern _iq ExDA_B;
extern _iq ExDA_C;
extern _iq ExDA_D;

extern _iq ExDA_A_Test;
extern _iq ExDA_B_Test;
extern _iq ExDA_C_Test;
extern _iq ExDA_D_Test;

/*------------------------------------------------------------------------------
  零漂标定用的一阶低通系数（见 OffsetISR）
  y = K1*y + K2*x，K1 + K2 = 1，时间常数 0.05s
------------------------------------------------------------------------------*/
extern _iq K1;
extern _iq K2;

/*------------------------------------------------------------------------------
  开环/闭环测试的给定值（标幺，范围 (-1,1)）
  VdTesting / VqTesting 用于 L1/L2 的开环电压矢量；
  IdRef / IqRef 用于 L3 的 IF 控制电流给定；
  IdLockRef 用于抱轴时把转子吸附到电气零位；
  SpeedRef 是速度给定（标幺）。
------------------------------------------------------------------------------*/
extern _iq VdTesting;
extern _iq VqTesting;
extern _iq IdRef;
extern _iq IqRef;
extern _iq IdLockRef;
extern _iq SpeedRef;

/*------------------------------------------------------------------------------
  采样周期（秒），由 ISR_FREQUENCY 推导
------------------------------------------------------------------------------*/
extern float32 T;

/*------------------------------------------------------------------------------
  运行标志与计数器
------------------------------------------------------------------------------*/
extern Uint32 IsrTicker;
extern Uint16 BackTicker;
extern Uint32 MainIsrTicker;
extern Uint32 OffsetIsrTicker;

// lsw 是切换程序运行状态的重要标志位，语义已统一：
//   0 = 抱轴标定 / 停止输出
//   1 = 电流环运行
//   2 = 速度环运行（仅 Level 4 有意义）
//   3 = 跳过控制计算（输出压 0）
extern Uint16 lsw;

// 编码器 index 标定完成标志；lsw 由 0 切到非 0 时会自动清零重新标定
extern Uint32 Init_IFlag;

// 当前控制级别，决定 LevelCfg[] 表中用哪一行配置（见 FOCLevel.h）
extern Uint32 BUILDLEVEL;

extern Uint32 LV1Ticker;
extern Uint32 LV2Ticker;
extern Uint32 LV3Ticker;
extern Uint32 LV5Ticker;
extern Uint32 TestTicker;
extern Uint32 TestTicker1;

// 速度环预分频：机械时间常数远大于电气时间常数，速度环按此降频运行
// （SpeedLoopPrescaler = 10 时，10kHz 电流环对应 1kHz 速度环）
extern Uint16 SpeedLoopPrescaler;
extern Uint16 SpeedLoopCount;

/*------------------------------------------------------------------------------
  FOC 运算对象
------------------------------------------------------------------------------*/
extern QEP qep1;                    // 编码器

extern CLARKE clarke1;              // Clarke 变换（abc -> alpha-beta）
extern PARK park1;                  // Park 变换（alpha-beta -> dq）
extern IPARK ipark1;                // 逆 Park 变换（dq -> alpha-beta）

extern PI_CONTROLLER pi_spd;        // 速度环，输出作为 Iq 给定
extern PI_CONTROLLER pi_id;         // 电流环 d 轴（励磁分量）
extern PI_CONTROLLER pi_iq;         // 电流环 q 轴（转矩分量）

extern PWMGEN pwm1;                 // PWM 输出
extern SVGEN svgen1;                // 空间矢量调制
extern RMPCNTL rc1;                 // 斜坡给定
extern RAMPGEN rg1;                 // 虚拟角度发生器
extern PHASEVOLTAGE volt1;          // 相电压计算（仅作参考，当前无消费者）

extern SPEED_MEAS_QEP speed1;       // 基于 QEP 的速度计算
extern MOTOR motor1;                // 采样结果（电流 / 母线电压）

/*------------------------------------------------------------------------------
  故障保护对象

  运行期间在 CCS 的 Expressions 窗口观察这几个成员：
    protect1.FaultCode  —— 故障码（0=无故障，含义见 FOCProtect.h）
    protect1.Tripped    —— 1 = 已跳闸，PWM 被硬件封锁
    protect1.TripCount  —— 累计跳闸次数
  恢复方法：排查完故障原因后，手工把 protect1.RecoverReq 置 1。
  恢复后 lsw 会被强制清 0（回到抱轴态），必须重新置位才能继续运行。
------------------------------------------------------------------------------*/
extern PROTECT protect1;

#endif /* USER_INC_VARIABLESINIT_H_ */
