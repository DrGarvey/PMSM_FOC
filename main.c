//#############################################################################
//
// FILE:   main.c
//
// TITLE:  PMSM_FOC_28377 主程序
//
// Author: jwzhou
//
// Rev. : 1.0
//
// 说明：
//   基于 TMS320F28377D 的永磁同步电机(PMSM)矢量控制(FOC)主程序。
//   中断由 EPWM1 在计数到零(TBCTR=0)时触发，频率 10kHz。
//
//   程序运行分为两级：
//     1) OffsetISR：上电后前 30000 个中断周期内运行，用于采集电流/母线电压的
//        零漂(offset)标定。零漂标定完成后，将中断服务函数动态切换为 MainISR。
//     2) MainISR：FOC 主中断，根据 BUILDLEVEL 分级执行不同控制算法：
//          - BUILDLEVEL 1：SVPWM 开环输出，仅验证 PWM 端口与基础计算
//          - BUILDLEVEL 2：VF 开环控制（虚拟角度），验证 ADC/坐标变换
//          - BUILDLEVEL 3：电流闭环（虚拟角度），验证电流环 PI
//          - BUILDLEVEL 5：速度闭环（QEP 真实角度）
//          - BUILDLEVEL 0：关闭 PWM 输出(保护模式)
//
//   模块划分：
//     app/    —— 应用层：main.c、isr.c(中断)、globals.c(全局变量)、settings.h(参数)
//     foc/    —— FOC 算法库（Clarke/Park/PI/SVPWM 等纯数学模块）
//     bsp/    —— 板级支持包（PWM/QEP/AD7606/外部DAC 驱动）
//
//   关键全局标志(定义于 app/globals.c)：
//     EnableFlag —— 程序使能，烧录前须置 TRUE，否则主程序一直空转
//     BUILDLEVEL —— 决定 ISR 中执行哪一级控制算法
//     lsw        —— 运行状态切换标志(0=抱轴/锁定，1=电流环，2=速度环等)
//#############################################################################
//

#include "bsp.h"
#include "globals.h"
#include "isr.h"

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

    InverterRST_Init();
    GPIO_writePin(8U, 1U);

    // ================= PWM 模块参数设定 =================
    // 采用上下计数模式(UP-DOWN)，时钟分频为 1，即 TBCLK = EPWMCLK = SYSCLK/2 = 100MHz
    // 上下计数模式下 PWM 周期 = 2 * PeriodMax * TBCLK，中断在计数到零(TBCTR=0)时触发
    // 因此 PeriodMax = TBCLK / 采样频率 / 2 = (100MHz / 10kHz) / 2 = 5000
    // 对应代码公式：SYSTEM_FREQUENCY * 1e6 * SAMPLE_TIME / 4 = 200e6 * 0.0001 / 4 = 5000
    pwm1.PeriodMax = SYSTEM_FREQUENCY * 1000000 * SAMPLE_TIME / 4; // 预分频 X1 (T1)，ISR 周期 = SAMPLE_TIME
    // 占空比 50% 对应的比较值 = PeriodMax / 2，即 CMPA = HalfPerMax 时输出 50% 占空比
    pwm1.HalfPerMax = pwm1.PeriodMax / 2;
    // 死区时间设定：2.0us 对应的 TBCLK 计数 = 2.0us * TBCLK 频率 = 2.0 * (SYSCLK/2) = 200 counts
    pwm1.Deadband = 2.0 * SYSTEM_FREQUENCY / 2; // 200 counts -> 2.0 usec
    // 执行 PWM 初始化程序
    PWM_INIT(&pwm1);

    //AD 采样初始化
    AD7606_INIT();

    // 编码器参数设定
    qep1.LineEncoder = LINE_ENCODER;        //2500 线编码器
    //机械分辨率，QEP 模块一般是上下沿计数，A、B 两个信号的上下沿共有四个
    //所以一圈内有 4*2500 个计数，分辨率就是计数的倒数
    qep1.MechScaler = _IQ30(0.25 / qep1.LineEncoder);
    qep1.PolePairs = POLES / 2;     //极对数
    qep1.CalibratedAngle = 0;    //记录 Z 信号和电气零位置偏差值
    QEP_INIT(&qep1);

    // 初始化基于 QEP 的速度计算模块
    speed1.K1 = _IQ21(1/(BASE_FREQ*SAMPLE_TIME));
    speed1.K2 = _IQ(1 / (1 + SAMPLE_TIME * 2 * PI * 5));  // 低通滤波截止频率
    speed1.K3 = _IQ(1) - speed1.K2;
    speed1.BaseRpm = 120 * (BASE_FREQ / POLES);

    // 初始化 RAMPGEN 模块
    rg1.StepAngleMax = _IQ(BASE_FREQ*SAMPLE_TIME);

    // 初始化 Id 的 PI 模块
    pi_spd.Kp = _IQ(0.05);
    pi_spd.Ki = _IQ(0.0);
    pi_spd.Umax = _IQ(0.95);
    pi_spd.Umin = _IQ(-0.95);

    // 初始化 Iq 的 PI 模块
    pi_id.Kp = _IQ(1.0);
    pi_id.Ki = _IQ(SAMPLE_TIME / 0.04);
    pi_id.Umax = _IQ(0.4);
    pi_id.Umin = _IQ(-0.4);

    // 初始化速度的 PI 模块
    pi_iq.Kp = _IQ(1.0);
    pi_iq.Ki = _IQ(SAMPLE_TIME / 0.04);
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
