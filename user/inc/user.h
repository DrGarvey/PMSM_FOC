/*
 * user.h
 *
 *  Created on: 2024年6月20日
 *      Author: jwzho
 */

#ifndef USER_INC_USER_H_
#define USER_INC_USER_H_

#include "driverlib.h"
#include "device.h"
#include "IQmathLib.h"
#include "math.h"


typedef int                 int16;
typedef long                int32;
typedef long long           int64;
typedef unsigned int        Uint16;
typedef unsigned long       Uint32;
typedef unsigned long long  Uint64;
typedef float               float32;
typedef long double         float64;

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


#define CPU_RATE   5.00L   // 200MHz CPU 时钟速度 (SYSCLKOUT)
//#define CPU_RATE   5.263L   // 190MHz CPU 时钟速度  (SYSCLKOUT)
//#define CPU_RATE   5.556L   // 180MHz CPU 时钟速度  (SYSCLKOUT)
//#define CPU_RATE   5.882L   // 170MHz CPU 时钟速度  (SYSCLKOUT)
//#define CPU_RATE   6.250L   // 160MHz CPU 时钟速度  (SYSCLKOUT)
//#define CPU_RATE   6.667L   // 150MHz CPU 时钟速度  (SYSCLKOUT)
//#define CPU_RATE   7.143L   // 140MHz CPU 时钟速度  (SYSCLKOUT)
//#define CPU_RATE   7.692L   // 130MHz CPU 时钟速度  (SYSCLKOUT)
//#define CPU_RATE   8.333L   // 120MHz CPU 时钟速度  (SYSCLKOUT)

extern void F28x_usDelay(long LoopCount);

#define DELAY_US(A)  F28x_usDelay(((((long double) A * 1000.0L) / (long double)CPU_RATE) - 9.0L) / 5.0L)
// 定义 ISR 频率 (kHz)
#define ISR_FREQUENCY 10
#define SYSTEM_FREQUENCY 200


typedef struct {
                    _iq Ia;
                    _iq Ib;
                    _iq Ic;
                    _iq Udc;
                    _iq ADRdTicker;
                    _iq ADWaitTicker;

                } MOTOR;


#define MOTOR_DEFAULTS {0, 0, 0, 0, 0, 0}

typedef struct {
                    _iq ElecTheta;        // 输出：电机电气角度 (Q24)
                    _iq MechTheta;        // 输出：电机机械角度 (Q24)
                    uint16_t DirectionQep;    // 输出：电机旋转方向 (Q0)
                    uint16_t QepPeriod;       // 输出：QEP 信号的捕获周期，以 EQEP 捕获定时器(QCTMR)周期数表示 (Q0)
                    uint32_t QepCountIndex;   // 变量：编码器计数器索引 (Q0)
                    int32 RawTheta;        // 变量：来自 EQEP 位置计数器的原始角度 (Q0)
                    _iq MechScaler;      // 参数：0.9999/总计数 (Q30)
                    uint16_t LineEncoder;     // 参数：线编码器线数 (Q0)
                    uint16_t PolePairs;       // 参数：极对数 (Q0)
                    uint32_t CalibratedAngle; // 参数：编码器 index 与 a 相之间的原始角度偏移 (Q0)
                    uint16_t IndexSyncFlag;   // 输出：index 同步状态 (Q0)
                }  QEP;

#define myQEP_BASE EQEP2_BASE

typedef struct {
                    Uint16 PeriodMax;   // 参数：PWM 半周期，以 CPU 时钟周期数表示 (Q0)
                    Uint16 HalfPerMax;  // 参数：PeriodMax 的一半 (Q0)
                    Uint16 Deadband;    // 参数：PWM 死区，以 CPU 时钟周期数表示 (Q0)
                    _iq MfuncC1;        // 输入：EPWM1 A&B 占空比 (Q24)
                    _iq MfuncC2;        // 输入：EPWM2 A&B 占空比 (Q24)
                    _iq MfuncC3;        // 输入：EPWM3 A&B 占空比 (Q24)
                } PWMGEN ;



#include "FOC.h"

//======================FOC计算相关头文件=====================================
#include "park.h"               // 包含 PARK 对象的头文件
#include "ipark.h"              // 包含 IPARK 对象的头文件
#include "pi.h"         // 包含 PIDREG3 对象的头文件
#include "clarke.h"             // 包含 CLARKE 对象的头文件
#include "svgen.h"          // 包含 SVGENDQ 对象的头文件
#include "rampgen.h"            // 包含 RAMPGEN 对象的头文件
#include "rmp_cntl.h"           // 包含 RMPCNTL 对象的头文件
#include "volt_calc.h"          // 包含 PHASEVOLTAGE 对象的头文件
#include "speed_fr.h"           // 包含 SPEED_MEAS_QEP 对象的头文件

//=====================FOC计算对硬件的配置======================================
#include "FOCPWM.h"
#include "FOCQEP.h"


//=====================Others======================================
#include "AD7606.h"
#include "ExternalDA.h"

#endif /* USER_INC_USER_H_ */
