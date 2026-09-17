//#############################################################################
//
// FILE:   main.c
//
// TITLE:  PMSM_FOC_28377 主程序
//
// Author: jwzhou
//
// Rev. : 2.1
//
// 说明：
//   基于 TMS320F28377D / LAUNCHXL-F28379D 的永磁同步电机(PMSM)矢量控制(FOC)
//   程序。中断由 EPWM1 在计数到零(TBCTR=0)时触发，频率 10kHz。
//
//   ---- 本文件只放"程序入口与中断编排" ----
//   具体计算都已拆分到下面的模块，本文件保持精简以便阅读：
//
//     main()        初始化各外设 -> 注册中断 -> 进入空循环
//     MainISR()     FOC 主中断：按顺序调用流水线各段，并处理跳闸/恢复与 lsw 门控
//     OffsetISR()   上电零漂标定中断，3 秒后把中断向量换成 MainISR
//
//   ---- 工程文件布局 ----
//     main.c                       程序入口与中断编排（本文件）
//     user/inc/FOCLevel.h           BUILDLEVEL 分级策略（配置表）
//     user/src/FOCLevel.c
//     user/inc/FOCTask.h            FOC 流水线各段（采样/角度/控制/调制/观测）
//     user/src/FOCTask.c
//     user/inc/FOCPWM.h  FOCQEP.h   PWM 与编码器驱动
//     user/src/FOCPWM.c  FOCQEP.c
//     user/inc/FOCADC.h  FOCDAC.h  FOCSCI.h   片上采样 / 模拟输出 / 串口
//     user/src/FOCADC.c  FOCDAC.c  FOCSCI.c
//     user/inc/FOCProtect.h         故障检测与硬件封锁
//     user/src/FOCProtect.c
//     user/inc/VariablesInit.h      全局变量声明（定义在 user/src/VariablesInit.c）
//     user/inc/user.h               类型定义、系统常量、各模块头文件的汇总入口
//     foc/                          TI 原始 FOC 算法宏（Clarke/Park/PI/SVPWM 等）
//     driver/  driverlib/           TI 器件初始化与驱动库，未改动
//
//   ---- FOC 主中断的流水线 ----
//     第 1 段  采样与保护    Sample_Run / Protect_Run
//     第 2 段  角度与速度    Angle_Run
//     第 3 段  控制器        Control_Run
//     第 4 段  调制与输出    Modulate_Run
//     第 5 段  观测          Observe_Run
//
//   每一级启用哪些环节由 FOCLevel.c 里的配置表决定，而不是靠
//   if (BUILDLEVEL == x) 分支判断，避免同一段逻辑在多处重复。
//
//   ---- 关键全局标志(声明于 VariablesInit.h) ----
//     EnableFlag —— 程序使能，烧录前须置 TRUE，否则主程序一直空转
//     BUILDLEVEL —— 决定使用配置表里的哪一行
//     lsw        —— 运行状态切换标志，语义见 FOCLevel.h
//
//   ---- 故障保护 ----
//     见 user/inc/FOCProtect.h。任一故障命中即由硬件 Trip-Zone 封锁三相 PWM，
//     同时把占空比写 0 作软件兜底。恢复必须手工把 protect1.RecoverReq 置 1。
//
//#############################################################################

#include "user.h"
#include "VariablesInit.h"
#include "FOCLevel.h"
#include "FOCTask.h"


/*------------------------------------------------------------------------------
  中断服务程序与辅助函数声明
------------------------------------------------------------------------------*/
__interrupt void MainISR(void);
__interrupt void OffsetISR(void);

static void Isr_Finish(void);

//烧写到 RAM，所以没用到 MemCopy 函数（见文件末尾的说明）
void MemCopy();

void InverterProtect(PWMGEN *v);


// 程序使能，需要在程序烧录前修改为 TRUE，否则程序始终循环不执行
volatile Uint16 EnableFlag = TRUE;

// 上一拍的 lsw，用于检测 0 -> 非0 的跳变
static Uint16 OldLsw = LSW_LOCK;


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

    // 片上外设初始化。顺序有依赖，不要调换：
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

  只做编排：按顺序调用流水线各段，并在每一处提前返回之前做好中断应答。
  各段的实现见 user/src/FOCTask.c。
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
                        中断收尾与辅助函数
==============================================================================*/

/*------------------------------------------------------------------------------
  中断收尾：清 EPWM1 中断标志 + 应答 PIE 第 3 组。

  每个从 MainISR / OffsetISR 返回的路径都必须经过这里（包括所有提前 return），
  否则中断不再产生，程序会静默停住。
------------------------------------------------------------------------------*/
static void Isr_Finish(void)
{
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

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
