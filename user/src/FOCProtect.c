/*
 * FOCProtect.c
 *
 *  Created on: 2026年9月16日
 *      Author: jwzho
 */

#include "user.h"
#include "FOCProtect.h"

/*------------------------------------------------------------------------------
  参与跳闸的三路 EPWM 模块（三相逆变桥）
------------------------------------------------------------------------------*/
static const uint32_t ProtectEpwmBases[3] =
{
    EPWM1_BASE,
    EPWM2_BASE,
    EPWM3_BASE
};

/* 需要清除的 Trip-Zone 标志：上电残留或历史跳闸都要清干净，
   否则 EPWM 一上电就处于封锁态，看不到任何输出 */
#define PROTECT_ALL_TZ_FLAGS                                       \
    (EPWM_TZ_FLAG_CBC    | EPWM_TZ_FLAG_OST     |                  \
     EPWM_TZ_FLAG_DCAEVT1 | EPWM_TZ_FLAG_DCAEVT2 |                 \
     EPWM_TZ_FLAG_DCBEVT1 | EPWM_TZ_FLAG_DCBEVT2)

/*------------------------------------------------------------------------------
  内部函数：清掉三路 EPWM 的全部 Trip-Zone 标志
------------------------------------------------------------------------------*/
static void Protect_ClearTripFlags(void)
{
    Uint16 i;

    for (i = 0; i < 3; i++)
    {
        EPWM_clearTripZoneFlag(ProtectEpwmBases[i], PROTECT_ALL_TZ_FLAGS);

        /* TZOSTCLR 用于重新武装"一次性跳闸源"。本工程虽然只走软件 TZFRC.OST，
           但一并清掉可以保证从任何历史状态干净复位。 */
        EPWM_clearOneShotTripZoneFlag(ProtectEpwmBases[i],
                                      (EPWM_TZ_OST_FLAG_OST1 | EPWM_TZ_OST_FLAG_OST2 |
                                       EPWM_TZ_OST_FLAG_OST3 | EPWM_TZ_OST_FLAG_OST4 |
                                       EPWM_TZ_OST_FLAG_OST5 | EPWM_TZ_OST_FLAG_OST6 |
                                       EPWM_TZ_OST_FLAG_DCAEVT1 | EPWM_TZ_OST_FLAG_DCBEVT1));
    }
}

/*------------------------------------------------------------------------------
  内部函数：把三路 EPWM 的比较值写成 0
  这是纯软件兜底——即使 Trip-Zone 路径因任何原因没生效，占空比也会归零。
  （原 InverterProtect() 取 ClosePWM = -1.0 得到 -1*HalfPerMax + HalfPerMax = 0，
    这里直接写 0，效果等价但不依赖 pwm1 对象）
------------------------------------------------------------------------------*/
static void Protect_ForceDutyZero(void)
{
    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, 0U);
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, 0U);
    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, 0U);
}

/*==============================================================================
                           初始化
==============================================================================*/
void Protect_INIT(PROTECT *v)
{
    Uint16 i;

    /* ---- 配置 Trip-Zone 动作：跳闸时把 A、B 两路输出全部强制拉低 ----
       高、低侧功率管同时关断，相绕组经续流二极管自由续流，这是逆变桥的安全态。
       注意必须 TZA、TZB 都设：EPWMxB 是由死区模块从 EPWMxA 生成的，
       只设 TZA 的话 B 路会保持跳闸前的状态，可能造成上下管直通。 */
    for (i = 0; i < 3; i++)
    {
        EPWM_setTripZoneAction(ProtectEpwmBases[i],
                               EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_LOW);
        EPWM_setTripZoneAction(ProtectEpwmBases[i],
                               EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_LOW);
    }

    /* ---- 清掉上电可能残留的跳闸标志，确保初始处于未封锁状态 ---- */
    Protect_ClearTripFlags();

    /* ---- 参数：取头文件里的默认阈值，运行时可在 Expressions 窗口直接改 ---- */
    v->IocLimit       = PROT_IOC_LIMIT;
    v->UdcOverLimit   = PROT_UDC_OVER_LIMIT;
    v->UdcUnderLimit  = PROT_UDC_UNDER_LIMIT;
    v->UdcTripDelay   = PROT_UDC_TRIP_DELAY;
    v->AdStuckLimit   = PROT_AD_STUCK_LIMIT;
    v->AngleJumpLimit = PROT_ANGLE_JUMP_LIMIT;
    v->AngleJumpTol   = PROT_ANGLE_JUMP_TOL;
    v->SpeedLimitRpm  = PROT_SPEED_LIMIT_RPM;
    v->ZLostLimit     = PROT_Z_LOST_LIMIT;

    /* ---- 状态清零 ---- */
    v->FaultCode    = PROT_FAULT_NONE;
    v->Tripped      = 0U;
    v->TripCount    = 0U;
    v->RecoverReq   = 0U;
    v->OldElecTheta = 0;
    v->UdcDelayCnt  = 0U;
    v->AdStuckCnt   = 0U;
    v->AngleJumpCnt = 0U;
    v->ZLostCnt     = 0U;
    v->OldIa        = 0;
    v->OldIb        = 0;
    v->OldUdc       = 0;
}

/*==============================================================================
                           跳闸

  由各判据命中后调用，也可由调试器手工触发。
  只记录首个故障码：连续多故障时第一个往往是根因，后续故障不覆盖。
==============================================================================*/
void Protect_Trip(PROTECT *v, Uint16 code)
{
    if (v->FaultCode == PROT_FAULT_NONE)
    {
        v->FaultCode = code;
    }

    /* 硬件封锁：对三路 EPWM 触发一次性跳闸(OST)，TZCTL 已配成 LOW，
       输出被强制拉低并锁定，必须显式清标志才能恢复 */
    EPWM_forceTripZoneEvent(EPWM1_BASE, EPWM_TZ_FORCE_EVENT_OST);
    EPWM_forceTripZoneEvent(EPWM2_BASE, EPWM_TZ_FORCE_EVENT_OST);
    EPWM_forceTripZoneEvent(EPWM3_BASE, EPWM_TZ_FORCE_EVENT_OST);

    /* 软件兜底：占空比同时归零 */
    Protect_ForceDutyZero();

    v->Tripped = 1U;
    v->TripCount++;
}

/*==============================================================================
                           恢复

  不自动重试——恢复必须由人在调试器里把 protect1.RecoverReq 置 1。
  这样做的原因：实验室驱动一旦跳闸，通常需要先排查原因（接线、参数、负载），
  自动重试会让电机反复冲击，反而更危险。

  返回值：1 = 本次恢复成功，0 = 未请求或未跳闸，未做任何动作。
==============================================================================*/
Uint16 Protect_Recover(PROTECT *v)
{
    if (v->RecoverReq == 0U)
    {
        return 0U;                      /* 未请求恢复 */
    }

    if (v->Tripped == 0U)
    {
        v->RecoverReq = 0U;             /* 本就没跳闸，清掉请求即可 */
        return 0U;
    }

    /* 清掉三路 EPWM 的 Trip-Zone 标志，解除硬件封锁 */
    Protect_ClearTripFlags();

    /* 占空比保持在 0，等调用方重新进入正常控制流程后再由 PWM_MACRO 接管 */
    Protect_ForceDutyZero();

    v->FaultCode    = PROT_FAULT_NONE;
    v->Tripped      = 0U;
    v->RecoverReq   = 0U;

    /* 判据的连续计数一并清零，避免恢复瞬间带着故障期间的累计值再次跳闸 */
    v->UdcDelayCnt  = 0U;
    v->AdStuckCnt   = 0U;
    v->AngleJumpCnt = 0U;
    v->ZLostCnt     = 0U;

    return 1U;
}

/*==============================================================================
                           每拍故障检测

  按顺序逐项检查，任一命中立即跳闸并返回。
  每项判据都有自己的 Valid 门控：数据不可用时自动跳过，不会误报。
==============================================================================*/
void Protect_MACRO(PROTECT *v, const PROT_INPUT *in)
{
    _iq     iMag;               /* dq 电流矢量幅值 */
    _iq     dTheta;             /* 单拍电气角度变化量 */

    /*--------------------------------------------------------------------------
      故障 1：过流
      用 Clarke 输出的 alpha/beta 合成矢量幅值判断，而不是只看单相——
      只盯 Ia 会漏掉另外两相同时过流的情况。
    --------------------------------------------------------------------------*/
    if (in->SampleValid != 0U)
    {
        iMag = _IQsqrt(_IQmpy(in->Ialpha, in->Ialpha) +
                       _IQmpy(in->Ibeta,  in->Ibeta));

        if (iMag > v->IocLimit)
        {
            Protect_Trip(v, PROT_FAULT_OVERCURRENT);
            return;
        }
    }

    /*--------------------------------------------------------------------------
      故障 2：母线过压 / 欠压
      加去抖计数：上电瞬间母线跌落、负载突变都会引起短暂越界，
      连续 UdcTripDelay 拍都越界才认为是真故障。
    --------------------------------------------------------------------------*/
    if (in->SampleValid != 0U)
    {
        if ((in->Udc > v->UdcOverLimit) || (in->Udc < v->UdcUnderLimit))
        {
            v->UdcDelayCnt++;
            if (v->UdcDelayCnt >= v->UdcTripDelay)
            {
                Protect_Trip(v, (in->Udc > v->UdcOverLimit) ?
                                PROT_FAULT_UDC_OVER : PROT_FAULT_UDC_UNDER);
                return;
            }
        }
        else
        {
            v->UdcDelayCnt = 0U;
        }
    }

    /*--------------------------------------------------------------------------
      故障 3：编码器电气角度跳变
      正常情况下电气角度的单拍变化量由转速决定，非常小。
      突然出现大跳变说明编码器断线、A/B 相干扰或 Z 信号异常。
      角度是 pu 值(0~1)，需要先卷绕到 ±0.5 再比较，否则 0.99→0.01 会被误判为大跳变。
    --------------------------------------------------------------------------*/
    if (in->QepValid != 0U)
    {
        dTheta = in->ElecTheta - v->OldElecTheta;

        if (dTheta < _IQ(-0.5))
        {
            dTheta += _IQ(1.0);
        }
        else if (dTheta > _IQ(0.5))
        {
            dTheta -= _IQ(1.0);
        }

        if ((dTheta > v->AngleJumpLimit) || (dTheta < -v->AngleJumpLimit))
        {
            v->AngleJumpCnt++;
            if (v->AngleJumpCnt >= v->AngleJumpTol)
            {
                Protect_Trip(v, PROT_FAULT_ENC_JUMP);
                return;
            }
        }
        else
        {
            v->AngleJumpCnt = 0U;
        }

        v->OldElecTheta = in->ElecTheta;

        /*--------------------------------------------------------------------------
          故障 4：编码器 Z 信号丢失
          只在电机确实在转的时候判——低速时一个机械周期本来就很长，
          按拍计数会把正常情况误判成故障。
        --------------------------------------------------------------------------*/
        if ((in->SpeedRpm > PROT_Z_LOST_SPEED_MIN) ||
            (in->SpeedRpm < -PROT_Z_LOST_SPEED_MIN))
        {
            if (in->IndexSeen != 0U)
            {
                v->ZLostCnt = 0U;
            }
            else
            {
                v->ZLostCnt++;
                if (v->ZLostCnt >= v->ZLostLimit)
                {
                    Protect_Trip(v, PROT_FAULT_ENC_ZLOST);
                    return;
                }
            }
        }
        else
        {
            v->ZLostCnt = 0U;
        }

        /*--------------------------------------------------------------------------
          故障 5：超速
        --------------------------------------------------------------------------*/
        if ((in->SpeedRpm > v->SpeedLimitRpm) ||
            (in->SpeedRpm < -v->SpeedLimitRpm))
        {
            Protect_Trip(v, PROT_FAULT_OVERSPEED);
            return;
        }
    }

    /*--------------------------------------------------------------------------
      故障 6：ADC 转换完成标志未置位
      三相采样由 EPWM1 硬件触发。若触发丢失（EPWM 配置被改、ADC 模块被
      意外关闭），完成标志会一直不置位，此处据此跳闸。
      采样函数本身不阻塞，这里只负责跳闸和记录。
    --------------------------------------------------------------------------*/
    if (in->AdTimeout != 0U)
    {
        Protect_Trip(v, PROT_FAULT_AD_TIMEOUT);
        return;
    }

    /*--------------------------------------------------------------------------
      故障 7：采样链路断线
      真实的 AD 转换结果带噪声，连续多拍完全相等（浮点逐位相等）只可能是
      通道卡死或传感器断线。用完全相等而不是容差比较，避免正常小信号误判。
    --------------------------------------------------------------------------*/
    if (in->SampleValid != 0U)
    {
        if ((in->Ialpha == v->OldIa) && (in->Ibeta == v->OldIb) &&
            (in->Udc == v->OldUdc))
        {
            v->AdStuckCnt++;
            if (v->AdStuckCnt >= v->AdStuckLimit)
            {
                Protect_Trip(v, PROT_FAULT_AD_STUCK);
                return;
            }
        }
        else
        {
            v->AdStuckCnt = 0U;
        }

        v->OldIa  = in->Ialpha;
        v->OldIb  = in->Ibeta;
        v->OldUdc = in->Udc;
    }
}
