/*
 * FOCTask.c
 *
 *  Created on: 2026年9月16日
 *      Author: jwzho
 */

#include "user.h"
#include "VariablesInit.h"
#include "FOCLevel.h"
#include "FOCTask.h"

/*------------------------------------------------------------------------------
  本模块内部使用的每拍变量
------------------------------------------------------------------------------*/
static PROT_INPUT protIn;       /* 传给保护模块的输入快照 */
static SCI_TELEM  sciTelem;     /* 传给串口模块的遥测快照 */
static Uint16     IndexSeen;    /* 本拍是否收到编码器 Z 信号 */

/*==============================================================================
                        第 1 段：采样与保护
==============================================================================*/

/*------------------------------------------------------------------------------
  采样。读回片上 ADC 的结果，扣除上电标定的零漂，再算 Clarke 变换。

  采样本身由 EPWM1 在计数器峰值用硬件触发（见 FOCPWM.c 的 PWM_INIT），
  所以这里只有三次结果寄存器读取，没有任何等待。

  若 ADC 停摆（触发丢失、模块未使能），结果寄存器会一直不变，
  ADC_CTRL 会连续计数并置起 motor1.ADTimeout，交由保护模块跳闸。
------------------------------------------------------------------------------*/
void Sample_Run(void)
{
    ADC_CTRL(&motor1);

    clarke1.As = motor1.Ia - offsetA;
    clarke1.Bs = motor1.Ib - offsetB;
    CLARKE_MACRO(clarke1)
}

/*------------------------------------------------------------------------------
  故障检测。把本拍参与判据的量打包成一个快照传给保护模块，
  这样保护模块只依赖这个结构体，不直接读本工程的全局量，便于单独测试。

  QepValid 在 lsw == LSW_LOCK 时为 0：抱轴时编码器没在转，角度判据无意义。
------------------------------------------------------------------------------*/
void Protect_Run(void)
{
    protIn.Ialpha      = clarke1.Alpha;
    protIn.Ibeta       = clarke1.Beta;
    protIn.Udc         = motor1.Udc - offsetUdc;
    protIn.ElecTheta   = qep1.ElecTheta;
    protIn.SpeedRpm    = speed1.SpeedRpm;
    protIn.IndexSeen   = IndexSeen;
    protIn.SampleValid = cfg->SampleEnable;
    protIn.QepValid    = ((cfg->QepEnable != 0U) && (lsw != LSW_LOCK)) ? 1U : 0U;
    protIn.AdTimeout   = motor1.ADTimeout;

    Protect_MACRO(&protect1, &protIn);
}

/*==============================================================================
                        第 2 段：角度与速度
==============================================================================*/

/*------------------------------------------------------------------------------
  产生本拍的角度，并更新速度。

  角度来源由配置与 lsw 共同决定，所有分支都有默认值，不会出现"角度保持上一拍"
  的情况：
    lsw == 0 且该级支持抱轴  ->  电气零位（角度固定 0）
    lsw >= 2 且该级用编码器  ->  编码器真实电气角度
    其余                     ->  虚拟角度（斜坡发生器输出）

  注意 QEP_MACRO 被提到 Park 变换之前：原来它在 Park 之后才执行，
  导致正、逆 Park 都用上一拍的角度（100us，在 200Hz 基准下相当于 7.2 度电气角）。
  提前之后正、逆变换仍共用同一组 sin/cos，保持自洽。
------------------------------------------------------------------------------*/
void Angle_Run(void)
{
    /* ---- 斜坡给定与虚拟角度发生器 ----
       抱轴时把给定拉到 0，让斜坡平滑回零，避免松轴瞬间的冲击 */
    if ((cfg->LswMode == LSWMODE_LOCK) && (lsw == LSW_LOCK))
    {
        rc1.TargetValue = 0;
    }
    else
    {
        rc1.TargetValue = SpeedRef;
    }
    RC_MACRO(rc1)

    rg1.Freq = rc1.SetpointValue;
    RG_MACRO(rg1)

    /* ---- 编码器 ---- */
    if (cfg->QepEnable != 0U)
    {
        if ((cfg->LswMode == LSWMODE_LOCK) && (lsw == LSW_LOCK))
        {
            /* 抱轴：把位置计数器钉在 0，并清掉 index 锁存标志。
               电机被强制吸附至电气 0 位置后，此处持续清零计数器，
               作为后续 index 标定的起点。 */
            EQEP_setPosition(myQEP_BASE, 0U);
            EQEP_clearInterruptStatus(myQEP_BASE, EQEP_INT_INDEX_EVNT_LATCH);
            TestTicker++;
        }

        /* Z 信号快照。index 锁存标志在下面会被清掉，所以必须先取一份。
           这个快照用于保护模块的"Z 信号丢失"判据。 */
        IndexSeen = ((EQEP_getInterruptStatus(myQEP_BASE) &
                      EQEP_INT_INDEX_EVNT_LATCH) != 0U) ? 1U : 0U;

        /* 首次收到 Z 信号时记录编码器 index 与电机电气零位的偏差。
           只标定一次：Init_IFlag 非 0 后不再进入。
           需要重新标定时，把 lsw 从 0 切到非 0，MainISR 会把它清零。 */
        if ((IndexSeen != 0U) && (Init_IFlag == 0U))
        {
            qep1.CalibratedAngle = EQEP_getIndexPositionLatch(myQEP_BASE);
            Init_IFlag++;
        }

        if (!((cfg->LswMode == LSWMODE_LOCK) && (lsw == LSW_LOCK)))
        {
            QEP_MACRO(&qep1);
        }

        /* 速度计算。在固定采样率上做微分+低通，与速度环的预分频无关 */
        speed1.ElecTheta    = qep1.ElecTheta;
        speed1.DirectionQep = (Uint32)(qep1.DirectionQep);
        SPEED_FR_MACRO(speed1)
    }

    /* ---- 角度源选择 ---- */
    if ((cfg->LswMode == LSWMODE_LOCK) && (lsw == LSW_LOCK))
    {
        park1.Angle = 0;                            /* 抱轴：电气零位 */
    }
    else if ((cfg->AngleFromQep != 0U) && (lsw >= LSW_SPEED))
    {
        park1.Angle = qep1.ElecTheta;               /* 速度环：编码器真实角度 */
    }
    else
    {
        park1.Angle = rg1.Out;                      /* 其余：虚拟角度 */
    }

    /* 正、逆 Park 共用同一组 sin/cos，保证变换对自洽 */
    park1.Sine   = _IQsinPU(park1.Angle);
    park1.Cosine = _IQcosPU(park1.Angle);
}

/*==============================================================================
                        第 3 段：控制器
==============================================================================*/

/*------------------------------------------------------------------------------
  Park 变换 + 速度环 + 电流环。

  DqFromPI == 0 的级别（L1/L2）走开环，直接给固定电压矢量 VdTesting/VqTesting，
  不进入任何 PI 运算。
------------------------------------------------------------------------------*/
void Control_Run(void)
{
    /* ---- Park 变换：静止坐标系 -> 旋转坐标系 ----
       只在有采样数据时做。L1 不采样，跳过以免用陈旧数据算出无意义的 Ds/Qs */
    if (cfg->SampleEnable != 0U)
    {
        park1.Alpha = clarke1.Alpha;
        park1.Beta  = clarke1.Beta;
        PARK_MACRO(park1)
    }

    if (cfg->DqFromPI == 0U)
    {
        /* 开环：电压矢量人为给定，幅值需小于 1（标幺值系统）*/
        ipark1.Ds = VdTesting;
        ipark1.Qs = VqTesting;
        return;
    }

    /* ---- 速度环（仅 Level 4）----
       机械时间常数远大于电气时间常数，所以速度环按 SpeedLoopPrescaler 降频运行：
       每 10 个电流环周期执行一次，即 1kHz。 */
    if (cfg->SpeedLoopEnable != 0U)
    {
        if (SpeedLoopCount == SpeedLoopPrescaler)
        {
            pi_spd.Ref = rc1.SetpointValue;
            pi_spd.Fbk = speed1.Speed;
            PI_MACRO(pi_spd);
            SpeedLoopCount = 1;
        }
        else
        {
            SpeedLoopCount++;
        }

        /* 抱轴与电流环调试工况下清掉速度环积分，
           避免带着这两种工况积累的量进入真正的速度闭环 */
        if ((lsw == LSW_LOCK) || (lsw == LSW_CURRENT))
        {
            pi_spd.ui = 0;
            pi_spd.i1 = 0;
        }
    }

    /* ---- 电流环 Iq（转矩分量）----
       lsw=0 抱轴：给定 0，不产生转矩
       lsw=1 电流环调试：给定固定 IqRef
       速度环工况：给定取自速度环输出
       其余（含未定义的 lsw 值）：退回固定 IqRef，不再出现"保持上一拍给定" */
    if (lsw == LSW_LOCK)
    {
        pi_iq.Ref = 0;
    }
    else if (lsw == LSW_CURRENT)
    {
        pi_iq.Ref = IqRef;
    }
    else if (cfg->IqRefFromSpeed != 0U)
    {
        pi_iq.Ref = pi_spd.Out;
    }
    else
    {
        pi_iq.Ref = IqRef;
    }
    pi_iq.Fbk = park1.Qs;
    PI_MACRO(pi_iq)

    /* ---- 电流环 Id（励磁分量）----
       lsw=0 抱轴：给 IdLockRef 把转子吸附到电气零位。
       母线电压越小电流越大，调节母线电压使实际电流约 1A 时转子即可被拉动 */
    if (lsw == LSW_LOCK)
    {
        pi_id.Ref = IdLockRef;
    }
    else
    {
        pi_id.Ref = 0;
    }
    pi_id.Fbk = park1.Ds;
    PI_MACRO(pi_id)

    /* ---- 逆 Park 变换的输入：电流环输出 ---- */
    ipark1.Ds = pi_id.Out;
    ipark1.Qs = pi_iq.Out;
}

/*==============================================================================
                        第 4 段：调制与输出
==============================================================================*/

/*------------------------------------------------------------------------------
  逆 Park 变换 -> SVPWM -> 写 PWM 比较值。

  逆 Park 复用 park1 的 sin/cos，与第 3 段的 Park 变换用同一个角度，变换对自洽。
------------------------------------------------------------------------------*/
void Modulate_Run(void)
{
    /* ---- 逆 Park 变换 ---- */
    ipark1.Sine   = park1.Sine;
    ipark1.Cosine = park1.Cosine;
    IPARK_MACRO(ipark1)

    /* ---- 空间矢量调制 ---- */
    svgen1.Ualpha = ipark1.Alpha;
    svgen1.Ubeta  = ipark1.Beta;
    SVGENDQ_MACRO(svgen1)

    /* ---- 写 PWM 比较值 ---- */
    pwm1.MfuncC1 = svgen1.Ta;
    pwm1.MfuncC2 = svgen1.Tb;
    pwm1.MfuncC3 = svgen1.Tc;
    PWM_MACRO(&pwm1);

    /* ---- 相电压计算 ----
       仅作计算参考：本工程全程在标幺值下运算，电压幅值并不反馈到控制环节，
       所以 volt1 的结果当前没有任何消费者，它只是保留下来便于以后做电压观测。
       注意必须放在 SVGENDQ_MACRO 之后，否则取到的是上一拍的调制波
       （原代码放在之前，永远滞后一拍）。 */
    if (cfg->SampleEnable != 0U)
    {
        volt1.DcBusVolt = motor1.Udc - offsetUdc;
        volt1.MfuncV1 = svgen1.Ta;
        volt1.MfuncV2 = svgen1.Tb;
        volt1.MfuncV3 = svgen1.Tc;
        PHASEVOLT_MACRO(volt1)
    }
}

/*==============================================================================
                        第 5 段：观测
==============================================================================*/

/*------------------------------------------------------------------------------
  模拟观测输出 + 串口遥测。

  四个观测通道按级别输出不同的量，方便用示波器直接看关键波形。
  所有量都归一化在 (-1,+1) 内，DAC 模块内部会再钳位一次。
  通道的具体实现（哪两路用片上 DAC、哪两路用 PWM 滤波）见 FOCDAC.h。

    DA_A / DA_B：随级别切换
    DA_C       ：电气角度（0~1 对应 0~360 度电气角）
    DA_D       ：故障码 x 0.1，无故障时为 0，便于示波器直接判读故障类型

  串口遥测走 SCIA（板载 USB 虚拟串口），帧格式见 FOCSCI.h。
------------------------------------------------------------------------------*/
void Observe_Run(void)
{
    switch (BUILDLEVEL)
    {
        case 1U:
            /* 台架：看虚拟角度与开环电压给定 */
            ExDA_A = rg1.Out;
            ExDA_B = VqTesting;
            break;

        case 2U:
            /* VF 开环：看 Clarke 输出与 Park 输出，用于验证 ADC 与坐标变换 */
            ExDA_A = clarke1.Alpha;
            ExDA_B = park1.Ds;
            break;

        case 3U:
            /* IF 控制：看 Iq 的给定与反馈 */
            ExDA_A = pi_iq.Ref;
            ExDA_B = pi_iq.Fbk;
            break;

        case 4U:
            /* 速度闭环：看转速与速度环输出 */
            ExDA_A = speed1.Speed;
            ExDA_B = pi_spd.Out;
            break;

        default:
            ExDA_A = 0;
            ExDA_B = 0;
            break;
    }

    ExDA_C = qep1.ElecTheta;
    ExDA_D = (protect1.Tripped != 0U) ? ((float)protect1.FaultCode * 0.1f) : 0.0f;

    DA_Ctrl(ExDA_A, ExDA_B, ExDA_C, ExDA_D);

    /* ---- 串口遥测 ----
       传给串口模块一份数据快照。模块内部自带发送分频，
       每个中断周期调用它是安全的。 */
    sciTelem.BuildLevel = BUILDLEVEL;
    sciTelem.Lsw        = lsw;
    sciTelem.FaultCode  = protect1.FaultCode;
    sciTelem.Tripped    = protect1.Tripped;
    sciTelem.ElecTheta  = qep1.ElecTheta;
    sciTelem.SpeedRpm   = speed1.SpeedRpm;
    sciTelem.IqRef      = pi_iq.Ref;
    sciTelem.IqFbk      = pi_iq.Fbk;
    sciTelem.Udc        = motor1.Udc - offsetUdc;
    sciTelem.Ia         = motor1.Ia - offsetA;
    sciTelem.Ib         = motor1.Ib - offsetB;

    SCI_MACRO(&sciTelem);
}

/*==============================================================================
                        辅助
==============================================================================*/

/*------------------------------------------------------------------------------
  清掉三个 PI 的积分器与饱和记录。
  故障恢复后必须调用，避免带着故障期间积累的量重启。
------------------------------------------------------------------------------*/
void PI_ClearAll(void)
{
    pi_spd.i1 = 0;  pi_spd.ui = 0;  pi_spd.v1 = 0;  pi_spd.Out = 0;
    pi_id.i1  = 0;  pi_id.ui  = 0;  pi_id.v1  = 0;  pi_id.Out  = 0;
    pi_iq.i1  = 0;  pi_iq.ui  = 0;  pi_iq.v1  = 0;  pi_iq.Out  = 0;
}
