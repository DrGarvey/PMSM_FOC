/*
 * FOCProtect.h
 *
 *  Created on: 2026年9月16日
 *      Author: jwzho
 */

#ifndef USER_INC_FOCPROTECT_H_
#define USER_INC_FOCPROTECT_H_

/*==============================================================================
  故障保护模块

  职责：
    1) 初始化阶段把 EPWM1/2/3 的 Trip-Zone 动作配成"跳闸即把 A、B 两路输出拉低"；
    2) 每个中断周期检查各类故障判据，任一命中立即封锁 PWM 输出并记录故障码；
    3) 提供显式的恢复入口（不自动重试）。

  封锁是双重的：
    硬件 —— EPWM_forceTripZoneEvent() 触发一次性跳闸(OST)，由 TZCTL 寄存器
            把 EPWMxA/EPWMxB 强制拉低并锁定，必须显式清标志才能恢复。
    软件 —— 同时把三路 CMPA 写成 0，即使 Trip-Zone 路径异常，占空比也归零。

  故障记录策略：
    FaultCode 只记录"首个"故障码，后续故障不覆盖。连续多故障时第一个往往是根因。
==============================================================================*/

/*------------------------------------------------------------------------------
                        保护模块故障码定义
------------------------------------------------------------------------------*/
#define PROT_FAULT_NONE        0U   /* 无故障 */
#define PROT_FAULT_OVERCURRENT 1U   /* 过流 */
#define PROT_FAULT_UDC_OVER    2U   /* 母线过压 */
#define PROT_FAULT_UDC_UNDER   3U   /* 母线欠压 */
#define PROT_FAULT_ENC_JUMP    4U   /* 编码器角度跳变异常（断线或强干扰）*/
#define PROT_FAULT_ENC_ZLOST   5U   /* 编码器 Z 信号丢失 */
#define PROT_FAULT_OVERSPEED   6U   /* 超速 */
#define PROT_FAULT_AD_TIMEOUT  7U   /* ADC 转换完成标志连续多拍未置位（触发丢失）*/
#define PROT_FAULT_AD_STUCK    8U   /* 采样值长期不变（传感器断线）*/

/*------------------------------------------------------------------------------
                        保护判据的默认阈值

  量纲说明（很重要）：
    motor1.Ia / Ib / Udc 是片内 ADC 的读数经 ADC_CODE_TO_PU() 换算的结果，
    即 "12 位码 / 2048 - 1"，范围约 (-1, +1)，1.0 对应 ADC 满量程（板载
    VREFHI = 3.0V）。它【不是】BASE_CURRENT(9.9A) / BASE_VOLTAGE(236.14V)
    的标幺值，除非你在 FOCADC.h 里把 ADC_IA_SCALE 之类的增益按实际前端折算好。

    电流通道：在 FOCADC.h 里设好 ADC_IA_SCALE / ADC_IB_SCALE 之后，
              阈值可以直接按安培折算 —— 0.5 对应 0.5 * 9.9A ≈ 4.95A。
              若暂时保持增益为 1.0，则阈值就是"ADC 归一化值"口径。
    电压通道：模拟前端的电阻分压比决定了 1.0 对应多少伏。换算示例：
              若 Udc 通道为 1.0 <-> 400V，则母线 360V 对应 0.90，
              把 PROT_UDC_OVER_LIMIT 改成 _IQ(0.90) 即可。
------------------------------------------------------------------------------*/
#define PROT_IOC_LIMIT         _IQ(0.50)  /* 过流：0.5 pu ≈ 4.95A */
#define PROT_UDC_OVER_LIMIT    _IQ(0.95)  /* 母线过压（归一化值，待按分压比换算）*/
#define PROT_UDC_UNDER_LIMIT   _IQ(0.30)  /* 母线欠压（归一化值，待按分压比换算）*/
#define PROT_UDC_TRIP_DELAY    10U        /* 连续超限拍数才报，抗上电跌落 (1ms @10kHz) */
#define PROT_AD_STUCK_LIMIT    5000U      /* 采样值连续不变拍数判线 (0.5s @10kHz) */
#define PROT_ANGLE_JUMP_LIMIT  _IQ(0.20)  /* 电气角度单拍跳变上限 0.2 pu = 72° 电气角 */
#define PROT_ANGLE_JUMP_TOL    3U         /* 连续跳变异常拍数才报 */
#define PROT_SPEED_LIMIT_RPM   3000       /* 超速阈值 (rpm) */
#define PROT_Z_LOST_LIMIT      30000U     /* 连续收不到 Z 信号的判线 (3s @10kHz) */
#define PROT_Z_LOST_SPEED_MIN  30         /* 低于此转速(绝对值)不做 Z 丢失判据；
                                             低速时一个机械周期本来就长，避免误判 */

/*------------------------------------------------------------------------------
                        保护模块输入快照

  把本拍参与判据的量集中传入，使保护模块不依赖其它编译单元的全局变量
  （VariablesInit.h 只在 main.c 里展开，user/src 下的 .c 文件不得再次包含它，
    否则全局变量会重复定义）。
  各项的 Valid 标志表示该路数据本拍是否有效，无效时对应判据自动跳过。
------------------------------------------------------------------------------*/
typedef struct {
    _iq     Ialpha;         /* Clarke 变换输出的 alpha 轴电流 */
    _iq     Ibeta;          /* Clarke 变换输出的 beta  轴电流 */
    _iq     Udc;            /* 已扣除零漂的母线电压 */
    _iq     ElecTheta;      /* 电气角度 (pu) */
    int32   SpeedRpm;       /* 转速 (rpm)，带符号，超速与 Z 丢失判据内部取绝对值 */
    Uint16  SampleValid;    /* 1 = 采样数据有效，可做过流/母线电压/断线判据 */
    Uint16  QepValid;       /* 1 = 本拍已运行 QEP_MACRO，可做角度判据 */
    Uint16  IndexSeen;      /* 1 = 本拍收到编码器 Z 信号 */
    Uint16  AdTimeout;      /* 1 = ADC 转换完成标志连续多拍未置位 */
} PROT_INPUT;

/*------------------------------------------------------------------------------
                        保护对象
------------------------------------------------------------------------------*/
typedef struct {
    /* ---- 参数：由 Protect_INIT 按 FOCProtect.h 的默认值写入 ---- */
    _iq     IocLimit;         /* 过流阈值 */
    _iq     UdcOverLimit;     /* 母线过压阈值 */
    _iq     UdcUnderLimit;    /* 母线欠压阈值 */
    Uint16  UdcTripDelay;     /* 母线电压超限去抖拍数 */
    Uint16  AdStuckLimit;     /* 采样值不变拍数判线 */
    _iq     AngleJumpLimit;   /* 电气角度单拍跳变上限 */
    Uint16  AngleJumpTol;     /* 角度跳变去抖拍数 */
    Uint32  SpeedLimitRpm;    /* 超速阈值 */
    Uint32  ZLostLimit;       /* Z 信号丢失拍数判线 */

    /* ---- 输出：调试期间在 CCS Expressions 窗口观察 ---- */
    Uint16  FaultCode;        /* 首个故障码，PROT_FAULT_NONE 表示无故障 */
    Uint16  Tripped;          /* 1 = 已跳闸，PWM 已被硬件封锁 */
    Uint32  TripCount;        /* 累计跳闸次数 */
    Uint16  RecoverReq;       /* 置 1 请求恢复；恢复成功后自动清 0 */

    /* ---- 内部变量：无需人工干预 ---- */
    _iq     OldElecTheta;     /* 上一拍电气角度，用于角度跳变判据 */
    Uint16  UdcDelayCnt;      /* 母线电压超限连续计数 */
    Uint16  AdStuckCnt;       /* 采样值不变连续计数 */
    Uint16  AngleJumpCnt;     /* 角度跳变异常连续计数 */
    Uint32  ZLostCnt;         /* 未收到 Z 信号的连续计数 */
    _iq     OldIa;            /* 上一拍 A 相电流，用于断线判据 */
    _iq     OldIb;            /* 上一拍 B 相电流 */
    _iq     OldUdc;           /* 上一拍母线电压 */
} PROTECT;

/*------------------------------------------------------------------------------
  保护对象的默认初始化值（顺序必须与 PROTECT 结构体成员一一对应）
------------------------------------------------------------------------------*/
#define PROTECT_DEFAULTS {                    \
        PROT_IOC_LIMIT,          /* IocLimit        */ \
        PROT_UDC_OVER_LIMIT,     /* UdcOverLimit    */ \
        PROT_UDC_UNDER_LIMIT,    /* UdcUnderLimit   */ \
        PROT_UDC_TRIP_DELAY,     /* UdcTripDelay    */ \
        PROT_AD_STUCK_LIMIT,     /* AdStuckLimit    */ \
        PROT_ANGLE_JUMP_LIMIT,   /* AngleJumpLimit  */ \
        PROT_ANGLE_JUMP_TOL,     /* AngleJumpTol    */ \
        PROT_SPEED_LIMIT_RPM,    /* SpeedLimitRpm   */ \
        PROT_Z_LOST_LIMIT,       /* ZLostLimit      */ \
        0,                       /* FaultCode       */ \
        0,                       /* Tripped         */ \
        0,                       /* TripCount       */ \
        0,                       /* RecoverReq      */ \
        0,                       /* OldElecTheta    */ \
        0,                       /* UdcDelayCnt     */ \
        0,                       /* AdStuckCnt      */ \
        0,                       /* AngleJumpCnt    */ \
        0,                       /* ZLostCnt        */ \
        0,                       /* OldIa           */ \
        0,                       /* OldIb           */ \
        0                        /* OldUdc           */ \
                         }

/*------------------------------------------------------------------------------
  接口函数
------------------------------------------------------------------------------*/
void    Protect_INIT(PROTECT *v);
void    Protect_MACRO(PROTECT *v, const PROT_INPUT *in);
void    Protect_Trip(PROTECT *v, Uint16 code);
Uint16  Protect_Recover(PROTECT *v);

#endif /* USER_INC_FOCPROTECT_H_ */
