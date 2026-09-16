//#############################################################################
//
// FILE:   main.c
//
// TITLE:  PMSM_FOC_28377 主程序
//
// Author: jwzhou
//
// Rev. : 2.0
//
// 说明：
//   基于 TMS320F28377D 的永磁同步电机(PMSM)矢量控制(FOC)主程序。
//   中断由 EPWM1 在计数到零(TBCTR=0)时触发，频率 10kHz。
//
//   程序运行分为两个中断：
//     1) OffsetISR：上电后前 30000 个中断周期内运行，用于采集电流/母线电压的
//        零漂(offset)标定。零漂标定完成后，将中断服务函数动态切换为 MainISR。
//     2) MainISR：FOC 主中断，按下面的固定流水线执行。
//
//   ---- MainISR 流水线（2.0 版重构）----
//   以前是四个 if/else if 分支各写一整套 ISR 体，Level 3 与 Level 5 约 80% 的代码
//   逐字重复，导致"只在某一级正确"的缺陷。现在改成一条公共流水线，每一级只在
//   LevelCfg[] 表里声明"启用哪些环节"，具体差异由配置项驱动：
//
//     第 1 段  Level_Apply + Sample_Run + Protect_Run
//              片上 ADC 采样、Clarke 变换、故障检测与跳闸。所有级共用，最先执行。
//     第 2 段  Angle_Run      斜坡给定、虚拟角度或编码器角度、速度计算
//     第 3 段  Control_Run    Park 变换、速度环、电流环（或直接给开环电压矢量）
//     第 4 段  Modulate_Run   逆 Park、SVPWM、写 PWM 比较值
//     第 5 段  Observe_Run    DA 观测输出与中断应答
//
//   ---- 关键全局标志(定义于 VariablesInit.h) ----
//     EnableFlag —— 程序使能，烧录前须置 TRUE，否则主程序一直空转
//     BUILDLEVEL —— 决定使用 LevelCfg[] 表中的哪一行配置
//     lsw        —— 运行状态切换标志，语义已统一，见下方 LSW_xxx 定义
//
//   ---- 故障保护 ----
//     见 user/inc/FOCProtect.h。任一故障命中即由硬件 Trip-Zone 封锁三相 PWM 输出，
//     同时把占空比写 0 作软件兜底。恢复必须手工把 protect1.RecoverReq 置 1。
//
//#############################################################################

#include "user.h"
#include "VariablesInit.h"


/*==============================================================================
                        lsw 运行状态标志（语义已统一）

  三个 BUILDLEVEL 现在使用同一套 lsw 语义，不再各自解释：

    LSW_LOCK    = 0   抱轴标定 / 停止输出
    LSW_CURRENT = 1   电流环运行
    LSW_SPEED   = 2   速度环运行（仅 Level 5 有意义）
    LSW_SKIP    = 3   跳过控制计算（输出压 0）
==============================================================================*/
#define LSW_LOCK     0U
#define LSW_CURRENT  1U
#define LSW_SPEED    2U
#define LSW_SKIP     3U

/*==============================================================================
                        lsw 处理方式（每级配置表用）

    L1 台架测试完全不接主电路，需要 lsw 保持默认 0 也能输出波形，所以单独给它
    LSWMODE_IGNORE。其余各级都必须显式置位 lsw 才会运行，防止上电即大电流。
==============================================================================*/
#define LSWMODE_IGNORE  0U   /* 完全忽略 lsw（仅 Level 1 台架测试）*/
#define LSWMODE_STOP    1U   /* lsw == 0 时不运行控制，占空比压 0 */
#define LSWMODE_LOCK    2U   /* lsw == 0 时抱轴锁定，把转子吸附到电气零位 */

/*==============================================================================
                        BUILDLEVEL 配置表

   每一行描述对应级别启用哪些环节。下标即 BUILDLEVEL 的取值。

   字段含义：
     SampleEnable     1 = 执行片上 ADC 采样与 Clarke 变换
     QepEnable        1 = 运行 QEP（含 index 标定与速度计算）
     LswMode          LSWMODE_xxx，决定 lsw == 0 时的行为
     AngleFromQep     1 = lsw >= 2 时改用编码器真实角度；0 = 始终用虚拟角度
     DqFromPI         1 = dq 电压取自电流环 PI 输出；0 = 取 VdTesting/VqTesting 开环
     IqRefFromSpeed   1 = Iq 给定取自速度环输出
     SpeedLoopEnable  1 = 运行速度环
==============================================================================*/
typedef struct {
    Uint16 SampleEnable;
    Uint16 QepEnable;
    Uint16 LswMode;
    Uint16 AngleFromQep;
    Uint16 DqFromPI;
    Uint16 IqRefFromSpeed;
    Uint16 SpeedLoopEnable;
} LEVEL_CFG;

static const LEVEL_CFG LevelCfg[] =
{
    /* L0 关闭 PWM 输出（保护态），所有环节都不启用 */
    { 0U, 0U, LSWMODE_STOP,   0U, 0U, 0U, 0U },

    /* L1 SVPWM 开环输出，！！！不接主电路、不接电机！！！
         只在台架上验证 PWM 端口与基础计算，因此不采样、不用编码器、忽略 lsw */
    { 0U, 0U, LSWMODE_IGNORE, 0U, 0U, 0U, 0U },

    /* L2 VF 开环控制（虚拟角度），验证 AD 采样与坐标变换
         电流采样但不开环，角度用虚拟角度发生器 */
    { 1U, 0U, LSWMODE_STOP,   0U, 0U, 0U, 0U },

    /* L3 电流闭环（虚拟角度），验证电流环 PI
         lsw=0 抱轴标定，lsw=1 电流环运行 */
    { 1U, 1U, LSWMODE_LOCK,   0U, 1U, 0U, 0U },

    /* L4 保留（等同 L0），保持与 TI 原始分级编号一致 */
    { 0U, 0U, LSWMODE_STOP,   0U, 0U, 0U, 0U },

    /* L5 速度闭环（QEP 真实角度）
         lsw=0 抱轴标定，lsw=1 电流环（虚拟角），lsw>=2 速度环（真实角） */
    { 1U, 1U, LSWMODE_LOCK,   1U, 1U, 1U, 1U },
};

#define LEVEL_CFG_COUNT  (sizeof(LevelCfg) / sizeof(LevelCfg[0]))


/*------------------------------------------------------------------------------
  每拍刷新一次的本拍运行视图
------------------------------------------------------------------------------*/
static const LEVEL_CFG *cfg;        /* 指向当前级别的配置行 */
static PROT_INPUT       protIn;     /* 传给保护模块的输入快照 */
static SCI_TELEM        sciTelem;   /* 传给串口模块的遥测快照 */
static Uint16           IndexSeen;  /* 本拍是否收到编码器 Z 信号 */
static Uint16           OldLsw;     /* 上一拍的 lsw，用于检测 0 -> 非0 跳变 */

/*------------------------------------------------------------------------------
  中断服务程序与辅助函数声明
------------------------------------------------------------------------------*/
__interrupt void MainISR(void);
__interrupt void OffsetISR(void);

//烧写到 RAM，所以没用到 MemCopy 函数（见 main() 里的说明）
void MemCopy();

void InverterProtect(PWMGEN *v);


// 程序使能，需要在程序烧录前修改为 TRUE，否则程序始终循环不执行
volatile Uint16 EnableFlag = TRUE;


/*==============================================================================
                        第 1 段：配置刷新与采样
==============================================================================*/

/*------------------------------------------------------------------------------
  根据 BUILDLEVEL 选出本拍的配置行。

  BUILDLEVEL 取到表中没有的值（例如 4）时不再"所有分支都不进"——那样会让 PWM
  比较值冻结在最后一次的值上，而中断照常应答，从外部完全看不出异常。
  这里统一落到 L0（保护态），占空比被压 0。
------------------------------------------------------------------------------*/
static void Level_Apply(void)
{
    Uint32 level = BUILDLEVEL;

    if (level >= LEVEL_CFG_COUNT)
    {
        level = 0U;
    }

    cfg = &LevelCfg[level];
}

/*------------------------------------------------------------------------------
  第 1 段：采样。读回片上 ADC 的结果，扣除上电标定的零漂，再算 Clarke 变换。

  采样本身由 EPWM1 在计数器峰值用硬件触发（见 FOCPWM.c 的 PWM_INIT），
  所以这里只有三次结果寄存器读取，没有任何等待。

  若 ADC 停摆（触发丢失、模块未使能），结果寄存器会一直不变，
  ADC_CTRL 会连续计数并置起 motor1.ADTimeout，交由保护模块跳闸。
------------------------------------------------------------------------------*/
static void Sample_Run(void)
{
    /* 片上 ADC：转换由 EPWM1 在计数器峰值硬件触发，这里只读结果寄存器，
       不等待、不轮询（原 AD7606 方案要自旋等 BUSY，这里已经没有了）。 */
    ADC_CTRL(&motor1);

    clarke1.As = motor1.Ia - offsetA;
    clarke1.Bs = motor1.Ib - offsetB;
    CLARKE_MACRO(clarke1)
}

/*------------------------------------------------------------------------------
  第 1 段：故障检测。把本拍参与判据的量打包成一个快照传给保护模块，
  这样保护模块不必依赖其它编译单元的全局变量。

  QepValid 在 lsw == LSW_LOCK 时为 0：抱轴时编码器没在转，角度判据无意义。
------------------------------------------------------------------------------*/
static void Protect_Run(void)
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
  第 2 段：产生本拍的角度，并更新速度。

  角度来源由配置与 lsw 共同决定，所有分支都有默认值，不会出现"角度保持上一拍"
  的情况：
    lsw == 0 且该级支持抱轴  ->  电气零位（角度固定 0）
    lsw >= 2 且该级用编码器  ->  编码器真实电气角度
    其余                     ->  虚拟角度（斜坡发生器输出）

  注意 QEP_MACRO 被提到 Park 变换之前：原来它在 Park 之后才执行，
  导致正、逆 Park 都用上一拍的角度（100us，在 200Hz 基准下相当于 7.2 度电气角）。
  提前之后正、逆变换仍共用同一组 sin/cos，保持自洽。
------------------------------------------------------------------------------*/
static void Angle_Run(void)
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
               工况说明：电机被强制吸附至电气 0 位置后，此处持续清零计数器，
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
  第 3 段：Park 变换 + 速度环 + 电流环。

  DqFromPI == 0 的级别（L1/L2）走开环，直接给固定电压矢量 VdTesting/VqTesting，
  不进入任何 PI 运算。
------------------------------------------------------------------------------*/
static void Control_Run(void)
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

    /* ---- 速度环（仅 Level 5）----
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
  第 4 段：逆 Park 变换 -> SVPWM -> 写 PWM 比较值。

  逆 Park 复用 park1 的 sin/cos，与第 3 段的 Park 变换用同一个角度，变换对自洽。
------------------------------------------------------------------------------*/
static void Modulate_Run(void)
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
                        第 5 段：观测与中断应答
==============================================================================*/

/*------------------------------------------------------------------------------
  第 5 段：模拟观测输出 + 串口遥测。

  四个观测通道按级别输出不同的量，方便用示波器直接看关键波形。
  所有量都归一化在 (-1,+1) 内，DAC 模块内部会再钳位一次。
  通道的具体实现（哪两路用片上 DAC、哪两路用 PWM 滤波）见 FOCDAC.h。

    DA_A / DA_B：随级别切换
    DA_C       ：电气角度（0~1 对应 0~360 度电气角）
    DA_D       ：故障码 x 0.1，无故障时为 0，便于示波器直接判读故障类型

  串口遥测走 SCIA（板载 USB 虚拟串口），帧格式见 FOCSCI.h。
------------------------------------------------------------------------------*/
static void Observe_Run(void)
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
            /* 电流闭环：看 Iq 的给定与反馈 */
            ExDA_A = pi_iq.Ref;
            ExDA_B = pi_iq.Fbk;
            break;

        case 5U:
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
       传一份数据快照给串口模块（同样因为 VariablesInit.h 不能在
       user/src 下的文件里展开）。模块内部自带发送分频，
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

/*------------------------------------------------------------------------------
  中断收尾：清 EPWM1 中断标志 + 应答 PIE 第 3 组。
  所有从 MainISR 返回的路径都必须经过这里，否则中断不再产生。
------------------------------------------------------------------------------*/
static void Isr_Finish(void)
{
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

/*------------------------------------------------------------------------------
  清掉三个 PI 的积分器与饱和记录。
  故障恢复后必须调用，避免带着故障期间积累的量重启。
------------------------------------------------------------------------------*/
static void PI_ClearAll(void)
{
    pi_spd.i1 = 0;  pi_spd.ui = 0;  pi_spd.v1 = 0;  pi_spd.Out = 0;
    pi_id.i1  = 0;  pi_id.ui  = 0;  pi_id.v1  = 0;  pi_id.Out  = 0;
    pi_iq.i1  = 0;  pi_iq.ui  = 0;  pi_iq.v1  = 0;  pi_iq.Out  = 0;
}

/*==============================================================================
                        主程序
==============================================================================*/
/**
 * main.c
 */
void main(void)
{
    Device_init();                                              //设备初始化

    Device_initGPIO();                                          //GPIO 初始化

    Interrupt_initModule();                                     //中断初始化

    Interrupt_initVectorTable();                                //中断向量表初始化

    //如果程序未手动使能，则一直在此循环
    while (EnableFlag == FALSE)
    {
        BackTicker++;
    }

    // 说明：原来自制板上有一个 GPIO8 用于控制逆变器使能/继电器，
    // 换成 LAUNCHXL-F28379D 后由 BoosterPack 功率级自己管理使能，这个引脚已移除。

    // ================= PWM 模块参数设定 =================
    // 采用上下计数模式(UP-DOWN)，时钟分频为 1，即 TBCLK = EPWMCLK = SYSCLK/2 = 100MHz
    // 上下计数模式下 PWM 周期 = 2 * PeriodMax * TBCLK，中断在计数到零(TBCTR=0)时触发
    // 因此 PeriodMax = TBCLK / 采样频率 / 2 = (100MHz / 10kHz) / 2 = 5000
    // 对应代码公式：SYSTEM_FREQUENCY * 1e6 * T / 4 = 200e6 * 0.0001 / 4 = 5000
    pwm1.PeriodMax = SYSTEM_FREQUENCY * 1000000 * T / 4; // 预分频 X1 (T1)，ISR 周期 = T x 1
    // 占空比 50% 对应的比较值 = PeriodMax / 2，即 CMPA = HalfPerMax 时输出 50% 占空比
    pwm1.HalfPerMax = pwm1.PeriodMax / 2;
    // 死区时间设定：2.0us 对应的 TBCLK 计数 = 2.0us * TBCLK 频率 = 2.0 * (SYSCLK/2) = 200 counts
    pwm1.Deadband = 2.0 * SYSTEM_FREQUENCY / 2; // 200 counts -> 2.0 usec
    // 执行 PWM 初始化程序
    PWM_INIT(&pwm1);

    // PWM_INIT 只配置时基/死区/动作限定器，并不写 CMPA——CMPA 保持复位值 0。
    // 这里显式把占空比钉到 0，避免上电到 MainISR 接管之间输出处于未定义状态。
    InverterProtect(&pwm1);

    // 片外设初始化。顺序有依赖，不要调换：
    //   ADC_INIT 依赖 EPWM1 已配置好（采样由 EPWM1 的 SOCA 事件触发）；
    //   DAC_INIT 依赖 EPWMCLK 已分频（PWM 滤波 DAC 的周期值按 100MHz 算）
    //            —— 这两件事都在上面的 PWM_INIT 里完成。
    ADC_INIT();
    DAC_INIT();
    SCI_INIT();

    // 故障保护初始化：配置 EPWM Trip-Zone 动作并清零故障状态。
    // 必须放在外设初始化之后、中断使能之前，
    // 这样第一次中断到来时保护已经就位。
    Protect_INIT(&protect1);

    // 编码器参数设定
    qep1.LineEncoder = 2500;        //2500 线编码器
    //机械分辨率，QEP 模块一般是上下沿计数，A、B 两个信号的上下沿共有四个
    //所以一圈内有 4*2500 个计数，分辨率就是计数的倒数
    qep1.MechScaler = _IQ30(0.25 / qep1.LineEncoder);
    qep1.PolePairs = POLES / 2;     // 4 对极（FOC.h 中 POLES = 8）
    qep1.CalibratedAngle = 0;    //记录 Z 信号和电气零位置偏差值
    QEP_INIT(&qep1);
    //EQEP_setLatchMode(myQEP_BASE, EQEP_LATCH_CNT_READ_BY_CPU);

    // 初始化基于 QEP 的速度计算模块
    speed1.K1 = _IQ21(1/(BASE_FREQ*T));
    speed1.K2 = _IQ(1 / (1 + T * 2 * PI * 5));  // 低通滤波截止频率
    speed1.K3 = _IQ(1) - speed1.K2;
    speed1.BaseRpm = 120 * (BASE_FREQ / POLES);

    // 初始化 RAMPGEN 模块
    rg1.StepAngleMax = _IQ(BASE_FREQ*T);

    // ==================== PI 参数初始化 ====================
    // 注意下面每个环路的变量名：
    //   pi_spd —— 速度环，输出作为 Iq 的给定，所以限幅用标幺值 ±0.95
    //   pi_id  —— 电流环 d 轴（励磁分量），本工程正常运行给 0，限幅 ±0.4
    //   pi_iq  —— 电流环 q 轴（转矩分量），限幅 ±0.8

    // 速度环 pi_spd
    pi_spd.Kp = _IQ(0.05);
    //pi_spd.Ki = _IQ(T * SpeedLoopPrescaler / 0.2);
    pi_spd.Ki = _IQ(0.0);           // 目前是纯比例速度环，放开上一行即可启用积分
    pi_spd.Umax = _IQ(0.95);
    pi_spd.Umin = _IQ(-0.95);

    // 电流环 d 轴 pi_id
    pi_id.Kp = _IQ(1.0);
    pi_id.Ki = _IQ(T / 0.04);
    pi_id.Umax = _IQ(0.4);
    pi_id.Umin = _IQ(-0.4);

    // 电流环 q 轴 pi_iq
    pi_iq.Kp = _IQ(1.0);
    pi_iq.Ki = _IQ(T / 0.04);
    pi_iq.Umax = _IQ(0.8);
    pi_iq.Umin = _IQ(-0.8);

    // 先注册 OffsetISR 作为 EPWM1 的中断服务程序，用于上电后的零漂(offset)标定；
    // OffsetISR 内部在运行满 30000 个周期后，会动态将中断服务函数切换为 MainISR。
    Interrupt_register(INT_EPWM1, &OffsetISR);                 //确定中断类型和中断服务程序地址

    // 使能 EPWM1_INT 对应的 CPU INT3 中断：
    Interrupt_enable(INT_EPWM1);
    //
    // 使能全局中断 (INTM) 和实时中断 (DBGM)
    //
    EINT;
    ERTM;

    for (;;)  //无限循环
    {
        ;
    }

}


/*==============================================================================
                        FOC 主中断
==============================================================================*/
__interrupt void MainISR(void)
{
    // 验证 ISR
    MainIsrTicker++;

    // ==================== 第 1 段：配置刷新 + 采样 + 保护 ====================
    Level_Apply();

    if (cfg->SampleEnable != 0U)
    {
        Sample_Run();
    }

    Protect_Run();

    // ---- 已跳闸：不再做任何控制计算，等待人工恢复 ----
    if (protect1.Tripped != 0U)
    {
        /* 仍然轮询恢复请求：排查完故障原因后，在调试器里把 protect1.RecoverReq 置 1 */
        if (Protect_Recover(&protect1) != 0U)
        {
            /* 恢复成功。清掉 PI 的积分量，并把 lsw 强制退回抱轴态——
               绝不允许带着负载直接重启，必须重新置位 lsw 才会输出。 */
            PI_ClearAll();
            lsw = LSW_LOCK;
        }

        Observe_Run();      // 故障期间仍刷新 DA，便于示波器抓故障瞬间的波形
        Isr_Finish();
        return;
    }

    // ==================== lsw 运行门控 ====================
    // 检测 lsw 从 0 切到非 0：这个跳变是"松轴进入运行"的时刻，
    // 此时重新开始一次编码器 index 标定。
    if ((OldLsw == LSW_LOCK) && (lsw != LSW_LOCK))
    {
        Init_IFlag = 0;
        qep1.CalibratedAngle = 0;
    }
    OldLsw = lsw;

    // lsw == 3：按统一语义跳过控制计算。输出压 0 而不是让 CMPA 冻结在上一次的值，
    // 后者会让电机继续以最后占空比运行，从外部看不出异常。
    if (lsw == LSW_SKIP)
    {
        InverterProtect(&pwm1);
        Observe_Run();
        Isr_Finish();
        return;
    }

    // LSWMODE_STOP 的级别（L0/L2）要求 lsw != 0 才运行，防止上电即大电流
    if ((cfg->LswMode == LSWMODE_STOP) && (lsw == LSW_LOCK))
    {
        InverterProtect(&pwm1);
        Observe_Run();
        Isr_Finish();
        return;
    }

    // ==================== 第 2~4 段：FOC 流水线 ====================
    Angle_Run();
    Control_Run();
    Modulate_Run();

    // ==================== 第 5 段：观测与中断应答 ====================
    Observe_Run();
    Isr_Finish();
}

/*==============================================================================
                        零漂标定中断

  上电后前 30000 个中断周期（3 秒 @10kHz）运行。
  前 1000 拍丢弃，等模拟前端与 ADC 稳定；之后用一阶低通持续平均，
  得到电流与母线电压通道的直流零点。标定完成后把中断向量换成 MainISR。

  标定期间 PWM 已在运行，但 CMPA 已被 InverterProtect 压到 0，
  所以此时逆变桥不会输出有效电压。

  为什么需要零漂标定：电流传感器是双极性输出，静态时输出在中点
  （对应 ADC 码 2048、换算后 0.0），但器件与分压电阻的偏差会让它偏离零点。
  这里测出这个偏差，主循环里再减掉（clarke1.As = motor1.Ia - offsetA）。
==============================================================================*/
__interrupt void OffsetISR(void)
{
    // 验证 ISR
    OffsetIsrTicker++;
    IsrTicker++;

    // ADC 直流零漂测量
    if (IsrTicker >= 1000)
    {
        // 一阶低通（时间常数 0.05s）：y = K1*y + K2*x，K1 + K2 = 1
        ADC_CTRL(&motor1);
        offsetA   = K1 * offsetA   + K2 * (motor1.Ia);   //A 相零漂
        offsetB   = K1 * offsetB   + K2 * (motor1.Ib);   //B 相零漂
        offsetUdc = K1 * offsetUdc + K2 * (motor1.Udc);  //母线电压零漂
    }

    if (IsrTicker > 30000)
    {
        EALLOW;
        Interrupt_register(INT_EPWM1, &MainISR);               //确定中断类型和中断服务程序地址
        EDIS;
    }

    Isr_Finish();
}

/*==============================================================================
                        辅助函数
==============================================================================*/

/*------------------------------------------------------------------------------
  程序烧写到 RAM 运行，不需要把 ramfuncs 从 FLASH 搬到 RAM，
  所以这个函数当前没有被调用，保留是为了将来改成 FLASH 运行时直接启用。
------------------------------------------------------------------------------*/
void MemCopy(Uint16 *SourceAddr, Uint16 *SourceEndAddr, Uint16 *DestAddr)
{
    while (SourceAddr < SourceEndAddr)
    {
        *DestAddr++ = *SourceAddr++;
    }
    return;
}

/*------------------------------------------------------------------------------
  把三路 PWM 的比较值写成 0（对应 0% 占空比），三相输出全部关断。

  这是纯软件封锁，用于：
    1) 上电后 PWM_INIT 与 MainISR 接管之间的过渡期；
    2) lsw == 3 跳过控制计算时；
    3) 保护模块 Protect_Trip 的软件兜底（硬件 Trip-Zone 之外的二重保险）。
------------------------------------------------------------------------------*/
void InverterProtect(PWMGEN *v)
{
    _iq ClosePWM = -1.0;

    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
}
