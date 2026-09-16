/*
 * hardware.h
 *
 * 硬件驱动总头文件：基础类型、系统宏、电机/编码器/PWM 结构体，
 * 并汇总包含 FOC 算法库与各外设驱动模块。
 *
 *  Created on: 2024年6月20日
 *      Author: jwzho
 */

#ifndef HARDWARE_INC_HARDWARE_H_
#define HARDWARE_INC_HARDWARE_H_

/*==============================================================================
  板级自检：本工程按 LAUNCHXL-F28379D 编写，必须定义 _LAUNCHXL_F28379D

  driver/device.h 里有一处 #ifdef _LAUNCHXL_F28379D，决定 PLL 按哪种晶振配置：
    · 定义了这个符号   → 按 10MHz 晶振配置（LaunchPad 用的就是这个）
    · 没定义           → 按 20MHz 晶振配置（TMDSCNCD28379D 控制卡）

  LaunchPad 上是 10MHz 晶振。如果漏定义，PLL 会按 20MHz 算，结果是
  PLLSYSCLK 只有 100MHz 而不是 200MHz——而本工程的 CPU_RATE、ISR_FREQUENCY、
  SYSTEM_FREQUENCY、pwm1.PeriodMax 推导、DELAY_US 全都按 200MHz 写死。
  后果是中断频率、死区时间、各处延时统统错一倍，而 device.c 里那句
  时钟断言不会报错（因为等式两边一起变了），排查起来非常痛苦。

  所以这里用 #error 直接把它拦在编译期。

  添加方法：工程属性 → Build → C2000 Compiler → Predefined Symbols，
  加入 _LAUNCHXL_F28379D；或者用 ccs-project 的 setToolFlags 工具。
  注意 Debug 和 Release 两个配置都要加。
==============================================================================*/
#ifndef _LAUNCHXL_F28379D
#error "请在工程里定义预定义符号 _LAUNCHXL_F28379D（LaunchPad 是 10MHz 晶振，否则芯片只跑 100MHz）。详见 user.h 注释。"
#endif

#include "driverlib.h"
#include "device.h"
#include "IQmathLib.h"
#include "math.h"
#include "settings.h"

/* ===================== 基础类型 ===================== */
typedef int                 int16;
typedef long                int32;
typedef long long           int64;
typedef unsigned int        Uint16;
typedef unsigned long       Uint32;
typedef unsigned long long  Uint64;
typedef float               float32;
typedef long double         float64;

/* ===================== CPU / 中断指令宏 ===================== */
#define  EINT   __asm(" clrc INTM")
#define  DINT   __asm(" setc INTM")
#define  ERTM   __asm(" clrc DBGM")
#define  DRTM   __asm(" setc DBGM")
#ifndef  EALLOW
#define  EALLOW __asm(" EALLOW")
#endif
#ifndef  EDIS
#define  EDIS   __asm(" EDIS")
#endif
#define  ESTOP0 __asm(" ESTOP0")

#define M_INT1  0x0001
#define M_INT2  0x0002
#define M_INT3  0x0004
#define M_INT4  0x0008
#define M_INT5  0x0010
#define M_INT6  0x0020
#define M_INT7  0x0040
#define M_INT8  0x0080
#define M_INT9  0x0100
#define M_INT10 0x0200
#define M_INT11 0x0400
#define M_INT12 0x0800
#define M_INT13 0x1000
#define M_INT14 0x2000
#define M_DLOG  0x4000
#define M_RTOS  0x8000

#define BIT0    0x0001
#define BIT1    0x0002
#define BIT2    0x0004
#define BIT3    0x0008
#define BIT4    0x0010
#define BIT5    0x0020
#define BIT6    0x0040
#define BIT7    0x0080
#define BIT8    0x0100
#define BIT9    0x0200
#define BIT10   0x0400
#define BIT11   0x0800
#define BIT12   0x1000
#define BIT13   0x2000
#define BIT14   0x4000
#define BIT15   0x8000

extern void F28x_usDelay(long LoopCount);

#define DELAY_US(A)  F28x_usDelay(((((long double) A * 1000.0L) / (long double)CPU_RATE) - 9.0L) / 5.0L)
<<<<<<< HEAD:user/inc/user.h
// 定义 ISR 频率 (kHz)
#define ISR_FREQUENCY 10
#define SYSTEM_FREQUENCY 200

// 采样结果。三个量都由 user/src/FOCADC.c 从片上 ADC 读回并换算成 ±1 的
// 双极性标幺值（12 位码 2048 对应 0.0），与原来 AD7606 方案的语义一致，
// 因此 clarke1.As = motor1.Ia - offsetA 这条下游链路完全不用改。
=======

/* ===================== 电机采样结构体 ===================== */
>>>>>>> a5d651262611fda8af1a97cacb628d203cf7bf3e:hardware/inc/hardware.h
typedef struct {
                    _iq     Ia;         // A 相电流（已换算为标幺值）
                    _iq     Ib;         // B 相电流
                    _iq     Udc;        // 母线电压
                    Uint16  ADTimeout;  // 1 = ADC 转换完成标志连续多拍未置位（供保护判据使用）
                } MOTOR;

<<<<<<< HEAD:user/inc/user.h

#define MOTOR_DEFAULTS {0, 0, 0, 0}
=======
#define MOTOR_DEFAULTS {0, 0, 0, 0, 0, 0}
>>>>>>> a5d651262611fda8af1a97cacb628d203cf7bf3e:hardware/inc/hardware.h

/* ===================== QEP 编码器结构体 ===================== */
typedef struct {
                    _iq ElecTheta;        // 输出：电机电气角度 (Q24)
                    _iq MechTheta;        // 输出：电机机械角度 (Q24)
                    Uint16 DirectionQep;    // 输出：电机旋转方向 (Q0)
                    Uint16 QepPeriod;       // 输出：QEP 信号的捕获周期，以 EQEP 捕获定时器(QCTMR)周期数表示 (Q0)
                    Uint32 QepCountIndex;   // 变量：编码器计数器索引 (Q0)
                    int32 RawTheta;         // 变量：来自 EQEP 位置计数器的原始角度 (Q0)
                    _iq MechScaler;         // 参数：0.9999/总计数 (Q30)
                    Uint16 LineEncoder;     // 参数：线编码器线数 (Q0)
                    Uint16 PolePairs;       // 参数：极对数 (Q0)
                    Uint32 CalibratedAngle; // 参数：编码器 index 与 a 相之间的原始角度偏移 (Q0)
                    Uint16 IndexSyncFlag;   // 输出：index 同步状态 (Q0)
                }  QEP;

#define myQEP_BASE EQEP2_BASE

/* ===================== PWM 结构体 ===================== */
typedef struct {
                    Uint16 PeriodMax;   // 参数：PWM 半周期，以 CPU 时钟周期数表示 (Q0)
                    Uint16 HalfPerMax;  // 参数：PeriodMax 的一半 (Q0)
                    Uint16 Deadband;    // 参数：PWM 死区，以 CPU 时钟周期数表示 (Q0)
                    _iq MfuncC1;        // 输入：EPWM1 A&B 占空比 (Q24)
                    _iq MfuncC2;        // 输入：EPWM2 A&B 占空比 (Q24)
                    _iq MfuncC3;        // 输入：EPWM3 A&B 占空比 (Q24)
                } PWMGEN;

/* ===================== FOC 算法库 ===================== */
#include "park.h"               // 包含 PARK 对象的头文件
#include "ipark.h"              // 包含 IPARK 对象的头文件
#include "pi.h"                 // 包含 PIDREG3 对象的头文件
#include "clarke.h"             // 包含 CLARKE 对象的头文件
#include "svgen.h"              // 包含 SVGENDQ 对象的头文件
#include "rampgen.h"            // 包含 RAMPGEN 对象的头文件
#include "rmp_cntl.h"           // 包含 RMPCNTL 对象的头文件
#include "volt_calc.h"          // 包含 PHASEVOLTAGE 对象的头文件
#include "speed_fr.h"           // 包含 SPEED_MEAS_QEP 对象的头文件

/* ===================== FOC 硬件配置模块 ===================== */
#include "focpwm.h"
#include "focqep.h"

<<<<<<< HEAD:user/inc/user.h
//=====================片上外设：采样 / 模拟输出 / 串口=========================
// 这三块对应原来自制板上的"外部 AD7606 + EMIF 并行 DAC"，
// 换成 LAUNCHXL-F28379D 后改用芯片自带的 ADC / DAC / SCI。
#include "FOCADC.h"
#include "FOCDAC.h"
#include "FOCSCI.h"

//=====================故障保护================================================
#include "FOCProtect.h"

#endif /* USER_INC_USER_H_ */
=======
/* ===================== 外设驱动 ===================== */
#include "ad7606.h"
#include "externalda.h"

#endif /* HARDWARE_INC_HARDWARE_H_ */
>>>>>>> a5d651262611fda8af1a97cacb628d203cf7bf3e:hardware/inc/hardware.h
