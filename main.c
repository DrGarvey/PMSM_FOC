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
//   关键全局标志(定义于 VariablesInit.h)：
//     EnableFlag —— 程序使能，烧录前须置 TRUE，否则主程序一直空转
//     BUILDLEVEL —— 决定 ISR 中执行哪一级控制算法
//     lsw        —— 运行状态切换标志(0=抱轴/锁定，1=电流环，2=速度环等)
//#############################################################################
//
// git test

//#include "gpio.h"
#include "user.h"
#include "VariablesInit.h"



__interrupt void MainISR(void);
__interrupt void OffsetISR(void);

//烧写到 RAM，所以没用到 MemCopy 函数
void MemCopy();

void InverterRST_Init();
void InverterProtect(PWMGEN *v);


// 程序使能，需要在程序烧录前修改为 TRUE，否则程序始终循环不执行
volatile Uint16 EnableFlag = TRUE;

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
    // 对应代码公式：SYSTEM_FREQUENCY * 1e6 * T / 4 = 200e6 * 0.0001 / 4 = 5000
    pwm1.PeriodMax = SYSTEM_FREQUENCY * 1000000 * T / 4; // 预分频 X1 (T1)，ISR 周期 = T x 1
    // 占空比 50% 对应的比较值 = PeriodMax / 2，即 CMPA = HalfPerMax 时输出 50% 占空比
    pwm1.HalfPerMax = pwm1.PeriodMax / 2;
    // 死区时间设定：2.0us 对应的 TBCLK 计数 = 2.0us * TBCLK 频率 = 2.0 * (SYSCLK/2) = 200 counts
    pwm1.Deadband = 2.0 * SYSTEM_FREQUENCY / 2; // 200 counts -> 2.0 usec
    // 执行 PWM 初始化程序
    PWM_INIT(&pwm1);

    //AD 采样初始化
    AD7606_INIT();

    // 编码器参数设定
    qep1.LineEncoder = 2500;        //2500 线编码器
    //机械分辨率，QEP 模块一般是上下沿计数，A、B 两个信号的上下沿共有四个
    //所以一圈内有 4*2500 个计数，分辨率就是计数的倒数
    qep1.MechScaler = _IQ30(0.25 / qep1.LineEncoder);
    //qep1.PolePairs = POLES / 2;     //极对数
    qep1.PolePairs = POLES / 2;     //FSPM----10 对极
    qep1.CalibratedAngle = 0;    //记录 Z 信号和电气零位置偏差值
    QEP_INIT(&qep1);
    //EQEP_setLatchMode(EQEP2_BASE, EQEP_LATCH_CNT_READ_BY_CPU);

    // 初始化基于 QEP 的速度计算模块
    speed1.K1 = _IQ21(1/(BASE_FREQ*T));
    speed1.K2 = _IQ(1 / (1 + T * 2 * PI * 5));  // 低通滤波截止频率
    speed1.K3 = _IQ(1) - speed1.K2;
    speed1.BaseRpm = 120 * (BASE_FREQ / POLES);

    // 初始化 RAMPGEN 模块
    rg1.StepAngleMax = _IQ(BASE_FREQ*T);

    // 初始化 Id 的 PI 模块
    pi_spd.Kp = _IQ(0.05);
    //pi_spd.Ki = _IQ(T * SpeedLoopPrescaler / 0.2);
    pi_spd.Ki = _IQ(0.0);
    pi_spd.Umax = _IQ(0.95);
    pi_spd.Umin = _IQ(-0.95);

    // 初始化 Iq 的 PI 模块
    pi_id.Kp = _IQ(1.0);
    pi_id.Ki = _IQ(T / 0.04);
    pi_id.Umax = _IQ(0.4);
    pi_id.Umin = _IQ(-0.4);

    // 初始化速度的 PI 模块
    pi_iq.Kp = _IQ(1.0);
    pi_iq.Ki = _IQ(T / 0.04);
    pi_iq.Umax = _IQ(0.8);
    pi_iq.Umin = _IQ(-0.8);

    //调用 HVDMC 保护函数
    //HVDMC_Protection();

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

__interrupt void MainISR(void)
{
    // 验证 ISR
    MainIsrTicker++;
    //IsrTicker++;

    // =============================== LEVEL 1 ======================================
    //    ！！！不接主电路，不接电机！！！！
    //    该程序段主要是为了检测 PWM 端口输出是否正常，以及程序基础计算是否正常
    // ==============================================================================

    if (BUILDLEVEL == 1)
    {
        LV1Ticker++;
        // ------------------------------------------------------------------------------
        //  斜坡函数，为避免给定值大幅度快速变化影响系统稳定性，设置一个斜坡函数，使得两个给定值之间变化缓慢些
        // ------------------------------------------------------------------------------
        rc1.TargetValue = SpeedRef;
        RC_MACRO(rc1)

        // ------------------------------------------------------------------------------
        //  虚拟角度生成函数，会根据斜坡函数的输出值自动生成 0-2pi 变化的角度信号
        //  一般用于 VF 控制，或前期代码及硬件设备有效性验证
        // ------------------------------------------------------------------------------
        rg1.Freq = rc1.SetpointValue;
        //rg1.Freq = 0.1;
        RG_MACRO(rg1)

        // ------------------------------------------------------------------------------
        //  Park 逆变换的电压信号输入人为给定，角度信号输入为虚拟角度。由于本程序采用标幺值系统，因此数值需小于 1
        // ------------------------------------------------------------------------------
        ipark1.Ds = VdTesting;
        ipark1.Qs = VqTesting;

        // ------------------------------------------------------------------------------
        //  Park 逆变换计算
        // ------------------------------------------------------------------------------
        ipark1.Sine = _IQsinPU(rg1.Out);
        ipark1.Cosine = _IQcosPU(rg1.Out);
        IPARK_MACRO(ipark1)

        // ------------------------------------------------------------------------------
        //  根据 Park 逆变换的输出，进行 SVPWM 计算
        // ------------------------------------------------------------------------------
        svgen1.Ualpha = ipark1.Alpha;
        svgen1.Ubeta = ipark1.Beta;
        SVGENDQ_MACRO(svgen1)

        // ------------------------------------------------------------------------------
        //  将 SVPWM 计算结果输送给 PWM 模块输出
        // ------------------------------------------------------------------------------
        pwm1.MfuncC1 = svgen1.Ta;
        pwm1.MfuncC2 = svgen1.Tb;
        pwm1.MfuncC3 = svgen1.Tc;
        PWM_MACRO(&pwm1);

        //endif // (BUILDLEVEL==LEVEL1)
    }

    // =============================== LEVEL 2 ======================================
    // ！将控制板与驱动系统及电机连接！
    // 该程序段为 VF 控制，角度信号由虚拟模块产生
    // 电流由传感器采样，但不进入闭环计算，仅用于坐标变换的验证
    // 电压不宜给过高，一般电机给 40-60V 即可，
    // 该程序段的目的时测试 ADC 和坐标变换功能是否正常
    // 需要注意的是，某些情况下电机的轴较重，VF 的给定频率如果很大的话，转子跟不上定子磁场，会发生抖动
    // 因为该程序段主要是为了测试，所以推荐频率为 5Hz，对于 4 对极电机，也有 75rpm
    // ===============================工况说明=======================================
    // lsw = 0:
    // 未防止电压设置不恰当，程序启动后即大电流抱轴，设定一个 lsw 标志位，置 1 后才执行下面的程序段
    // 修改 lsw 之前请务必检查电路设置是否正确
    // lsw != 0：
    // VF 运行，角度信号由虚拟模块产生。
    // 该工况不建议高速运行，请合理设置参数
    // ==============================================================================

    else if (BUILDLEVEL == 2)
    {
        LV2Ticker++;

        // ------------------------------------------------------------------------------

        // ------------------------------------------------------------------------------
        if (lsw != 0)
        {
            // ------------------------------------------------------------------------------
            //  斜坡函数，为避免给定值大幅度快速变化影响系统稳定性，设置一个斜坡函数，使得两个给定值之间变化缓慢些
            // ------------------------------------------------------------------------------

            rc1.TargetValue = SpeedRef;
            RC_MACRO(rc1)

            // ------------------------------------------------------------------------------
            //  虚拟角度生成函数，会根据斜坡函数的输出值自动生成 0-2pi 变化的角度信号
            //  一般用于 VF 控制，或前期代码及硬件设备有效性验证
            // ------------------------------------------------------------------------------
            rg1.Freq = rc1.SetpointValue;
            RG_MACRO(rg1)

            // ------------------------------------------------------------------------------
            // 进行 AD 采样，采样 A、B 相电流用于 Clarke 计算
            // Clarke 变换，将电流从 abc 坐标系变为 alpha-beta 坐标系下
            // 由于本程序为标幺值计算，所以采样值的范围应为 (-1,+1).
            // 由于采用了傅里叶的 28377D 开发板，因此 AD 采用外设的 AD7606 芯片，没有用芯片内置的 ADC 模块
            // ------------------------------------------------------------------------------
            AD7606_CTRL(&motor1);
            clarke1.As = motor1.Ia - offsetA;
            clarke1.Bs = motor1.Ib - offsetB;
            CLARKE_MACRO(clarke1)

            // ------------------------------------------------------------------------------
            //  Park 变换，将电流从 alpha-beta 坐标系变为 dq 坐标系下
            // ------------------------------------------------------------------------------
            park1.Alpha = clarke1.Alpha;
            park1.Beta = clarke1.Beta;
            park1.Angle = rg1.Out;
            park1.Sine = _IQsinPU(park1.Angle);
            park1.Cosine = _IQcosPU(park1.Angle);
            PARK_MACRO(park1)

            // ------------------------------------------------------------------------------
            // Park 逆变换的电压输入由 Clarke 计算得出，角度信号输入为虚拟角度
            // VqTesting 确定输出电压的幅值，幅值较低时产生的电流不够驱动，可以略微调高些
            // 在 IPM 上，设为 0.2 左右可以起动，0.3 左右可以稳定开环运行
            // ------------------------------------------------------------------------------
            ipark1.Ds = VdTesting;
            ipark1.Qs = VqTesting;
            ipark1.Sine = park1.Sine;
            ipark1.Cosine = park1.Cosine;
            IPARK_MACRO(ipark1)

            // ------------------------------------------------------------------------------
            // 利用实际的电压值计算 SVPWM
            // 由于该例程都在标幺值下进行计算，因此电压计算环节并没有在控制环节中起作用，仅提供一个计算参考，Udc 可以不用设置
            // ------------------------------------------------------------------------------
            // volt1.DcBusVolt = motor1.Udc - offsetUdc;
            // volt1.MfuncV1 = svgen1.Ta;
            // volt1.MfuncV2 = svgen1.Tb;
            // volt1.MfuncV3 = svgen1.Tc;
            // PHASEVOLT_MACRO(volt1)

            // ------------------------------------------------------------------------------
            //  根据 Park 逆变换的输出，进行 SVPWM 计算
            // ------------------------------------------------------------------------------
            svgen1.Ualpha = ipark1.Alpha;
            svgen1.Ubeta = ipark1.Beta;
            SVGENDQ_MACRO(svgen1)

            // ------------------------------------------------------------------------------
            //  将 SVPWM 计算结果输送给 PWM 模块输出
            // ------------------------------------------------------------------------------
            pwm1.MfuncC1 = svgen1.Ta;
            pwm1.MfuncC2 = svgen1.Tb;
            pwm1.MfuncC3 = svgen1.Tc;
            PWM_MACRO(&pwm1);
            // 计算新的 PWM 比较值
        }

        // #endif // (BUILDLEVEL==LEVEL2)
    }

    // =============================== LEVEL 3 ======================================
    // ！将控制板与驱动系统及电机连接！
    // 该程序段为电流闭环控制，角度信号由虚拟模块产生
    // 该程序段的目的时测试 PI 计算模块及速度计算模块是否正常
    // 该程序段是为了测试电流环控制情况
    // ===============================工况说明=======================================
    // lsw = 0:
    // 抱轴，将电机强制吸附至电气 0 位置，电流大小由 IdLockRef 控制
    // 母线电压越小，电流越大，调节母线电压。使得实际电流约为 1A 时，常规电机的转子都能够拉动了。
    // 注意：实际电流为 1A 并不是 IdTestRef=1，IdTestRef 的数值是不用改动的，只需要人为调节母线电压即可
    // lsw = 1：
    // 电流闭环运行，角度信号由虚拟模块产生。此时可以调节电流环参数。
    // 该工况不建议高速运行，请合理设置 IqRef 及 SpeedRef 的值
    // 注意：如果速度为负数，则将电机的任意两个相线交换位置即可
    // lsw = 3；
    // 跳过执行程序
    // ==============================================================================

    else if (BUILDLEVEL == 3)
    {
        LV3Ticker++;
        // ------------------------------------------------------------------------------
        //  连接 RMP 模块的输入并调用斜坡控制宏
        // ------------------------------------------------------------------------------
        if (lsw != 3)
        {
            if (lsw == 0)
                rc1.TargetValue = 0;
            else
                rc1.TargetValue = SpeedRef;
            RC_MACRO(rc1)

            // ------------------------------------------------------------------------------
            //  连接 RAMP GEN 模块的输入并调用斜坡发生器宏
            // ------------------------------------------------------------------------------
            rg1.Freq = rc1.SetpointValue;
            RG_MACRO(rg1)

            // ------------------------------------------------------------------------------
            //  测量相电流，减去零漂并从 (-0.5,+0.5) 归一化到 (-1,+1)。
            //  连接 CLARKE 模块的输入并调用 Clarke 变换宏
            // ------------------------------------------------------------------------------
            AD7606_CTRL(&motor1);
            //clarke1.As = ((AdcMirror.ADCRESULT3) * 0.00024414 - offsetA) * 2 * 0.909; // A 相电流
            //clarke1.Bs = ((AdcMirror.ADCRESULT4) * 0.00024414 - offsetB) * 2 * 0.909; // B 相电流
            //clarke1.As = ((AdcMirror.ADCRESULT3) * 0.00024414 - offsetA) * 2; // A 相电流
            //clarke1.Bs = ((AdcMirror.ADCRESULT4) * 0.00024414 - offsetB) * 2; // B 相电流
            // ((ADCmeas(q12)/2^12)-offset)*2*(3.0/3.3)
            clarke1.As = motor1.Ia - offsetA;
            clarke1.Bs = motor1.Ib - offsetB;
            CLARKE_MACRO(clarke1)

            // ------------------------------------------------------------------------------
            //  连接 PARK 模块的输入并调用 Park 变换宏
            // ------------------------------------------------------------------------------
            park1.Alpha = clarke1.Alpha;
            park1.Beta = clarke1.Beta;
            if (lsw == 0)
                park1.Angle = 0;
            else if (lsw == 1)
                park1.Angle = rg1.Out;

            park1.Sine = _IQsinPU(park1.Angle);
            park1.Cosine = _IQcosPU(park1.Angle);

            PARK_MACRO(park1)

            // ------------------------------------------------------------------------------
            //  连接 PI 模块的输入并调用 PI Iq 控制器宏
            // ------------------------------------------------------------------------------
            if (lsw == 0)
                pi_iq.Ref = 0;
            else if (lsw == 1)
                pi_iq.Ref = IqRef;
            pi_iq.Fbk = park1.Qs;
            PI_MACRO(pi_iq)

            // ------------------------------------------------------------------------------
            //  连接 PI 模块的输入并调用 PI Id 控制器宏
            // ------------------------------------------------------------------------------
            if (lsw == 0)
                pi_id.Ref = IdLockRef;
            else
                pi_id.Ref = IdRef;
            pi_id.Fbk = park1.Ds;
            PI_MACRO(pi_id)

            // ------------------------------------------------------------------------------
            //  连接 INV_PARK 模块的输入并调用 Park 逆变换宏
            // ------------------------------------------------------------------------------
            ipark1.Ds = pi_id.Out;
            ipark1.Qs = pi_iq.Out;
            ipark1.Sine = park1.Sine;
            ipark1.Cosine = park1.Cosine;
            IPARK_MACRO(ipark1)

            // ------------------------------------------------------------------------------
            //    检测校准角度(可选)并调用 QEP 模块
            // ------------------------------------------------------------------------------
            // Z 信号偏移计算模块，当 lsw=0，电机拉至 A 相位置（也叫零位置）时，将编码器计数器清 0
            // 当 lsw 从 0 切换至 1，电机电流环开始工作，电机第一次转至 Z 信号位置，会记录此时编码器计数器的数值，
            // 该数值就是电机 0 位置和编码器 Z 信号位置的偏差值，此后每次遇到 Z 信号计数器就清零一次，避免计数器溢出
            // 清零的同时，将这个偏差值加入，以或者正确的角度信息
            if (lsw == 0)
            {
                EQEP_setPosition(myQEP_BASE, 0U);
                EQEP_clearInterruptStatus(myQEP_BASE, EQEP_INT_INDEX_EVNT_LATCH);
                //EQep1Regs.QPOSCNT = 0;
                //EQep1Regs.QCLR.bit.IEL = 1;
                TestTicker++;
            } // 复位位置计数

            if ((EQEP_getInterruptStatus(myQEP_BASE) & EQEP_INT_INDEX_EVNT_LATCH)
                    && Init_IFlag == 0) // 检查首次 index 信号
            {
                qep1.CalibratedAngle = EQEP_getIndexPositionLatch(myQEP_BASE);
                //qep1.CalibratedAngle = EQep1Regs.QPOSILAT;
                Init_IFlag++;
            }   // 保持锁存的位置

            if (lsw != 0)
                QEP_MACRO(&qep1);

            //// ------------------------------------------------------------------------------
            ////    调用 QEP 计算模块
            //// ------------------------------------------------------------------------------
            //    QEP_MACRO(1,qep1);

            // ------------------------------------------------------------------------------
            //    连接 SPEED_FR 模块的输入并调用速度计算宏
            // ------------------------------------------------------------------------------
            speed1.ElecTheta = qep1.ElecTheta;
            speed1.DirectionQep = (int32) (qep1.DirectionQep);
            SPEED_FR_MACRO(speed1)

            // ------------------------------------------------------------------------------
            //  连接 VOLT_CALC 模块的输入并调用相电压计算宏
            // ------------------------------------------------------------------------------
            //  volt1.DcBusVolt = ((AdcMirror.ADCRESULT7)*0.00024414)*0.909; // 母线电压测量
            volt1.DcBusVolt = motor1.Udc;
            volt1.MfuncV1 = svgen1.Ta;
            volt1.MfuncV2 = svgen1.Tb;
            volt1.MfuncV3 = svgen1.Tc;
            PHASEVOLT_MACRO(volt1)

            // ------------------------------------------------------------------------------
            //  连接 SVGEN_DQ 模块的输入并调用空间矢量生成宏
            // ------------------------------------------------------------------------------
            svgen1.Ualpha = ipark1.Alpha;
            svgen1.Ubeta = ipark1.Beta;
            SVGENDQ_MACRO(svgen1)

            // ------------------------------------------------------------------------------
            //  连接 PWM_DRV 模块的输入并调用 PWM 信号生成宏
            // ------------------------------------------------------------------------------
            pwm1.MfuncC1 = svgen1.Ta;
            pwm1.MfuncC2 = svgen1.Tb;
            pwm1.MfuncC3 = svgen1.Tc;
            PWM_MACRO(&pwm1);
            // 计算新的 PWM 比较值

        }

        // #endif // (BUILDLEVEL==LEVEL3)

    }

    // =============================== LEVEL 5 ======================================
    //    Level 5 验证由 PI 模块完成的速度调节器。
    //    系统速度环通过使用测得的速度作为反馈来闭环。
    //    该程序段为速度闭环程序
    //    第一步与第二步与上述步骤一致，对于非首次运行的电机，确定速度计算无误后，第二步可以忽略，
    //    执行完第一步后，将 IdTestRef 设为 0，然后切换 lsw=2
    //
    //    lsw=2 为速度闭环，参数根据需要调节即可
    // ==============================================================================
    //  lsw=0: 锁定电机转子
    //  lsw=1: 闭合电流环

    else if (BUILDLEVEL == 5)
    {
        LV5Ticker++;
        // ------------------------------------------------------------------------------
        //  连接 RMP 模块的输入并调用斜坡控制宏
        // ------------------------------------------------------------------------------
        if (lsw == 0)
            rc1.TargetValue = 0;
        else
            rc1.TargetValue = SpeedRef;
        RC_MACRO(rc1)

        // ------------------------------------------------------------------------------
        //  连接 RAMP GEN 模块的输入并调用斜坡发生器宏
        // ------------------------------------------------------------------------------
        rg1.Freq = rc1.SetpointValue;
        RG_MACRO(rg1)

        // ------------------------------------------------------------------------------
        //  测量相电流，减去零漂并从 (-0.5,+0.5) 归一化到 (-1,+1)。
        //  连接 CLARKE 模块的输入并调用 Clarke 变换宏
        // ------------------------------------------------------------------------------
        AD7606_CTRL(&motor1);
        clarke1.As = motor1.Ia - offsetA;
        clarke1.Bs = motor1.Ib - offsetB;
        CLARKE_MACRO(clarke1)

        // ------------------------------------------------------------------------------
        //  连接 PARK 模块的输入并调用 Park 变换宏
        // ------------------------------------------------------------------------------
        park1.Alpha = clarke1.Alpha;
        park1.Beta = clarke1.Beta;

        if (lsw == 0)
            park1.Angle = 0;
        else if (lsw == 1)
            park1.Angle = rg1.Out;
        else
            park1.Angle = qep1.ElecTheta;

        park1.Sine = _IQsinPU(park1.Angle);
        park1.Cosine = _IQcosPU(park1.Angle);

        PARK_MACRO(park1)

        // ------------------------------------------------------------------------------
        //    连接 PI 模块的输入并调用 PI 速度控制器宏
        // ------------------------------------------------------------------------------
        if (SpeedLoopCount == SpeedLoopPrescaler)
        {
            pi_spd.Ref = rc1.SetpointValue;
            pi_spd.Fbk = speed1.Speed;
            PI_MACRO(pi_spd);
            SpeedLoopCount = 1;
        }
        else
            SpeedLoopCount++;

        if (lsw == 0 || lsw == 1)
        {
            pi_spd.ui = 0;
            pi_spd.i1 = 0;
        }

        // ------------------------------------------------------------------------------
        //    连接 PI 模块的输入并调用 PI Iq 控制器宏
        // ------------------------------------------------------------------------------
        if (lsw == 0)
            pi_iq.Ref = 0;
        else if (lsw == 1)
            pi_iq.Ref = IqRef;
        else
            pi_iq.Ref = pi_spd.Out;
            //pi_iq.Ref = IqRef;


        pi_iq.Fbk = park1.Qs;
        PI_MACRO(pi_iq)

        // ------------------------------------------------------------------------------
        //    连接 PI 模块的输入并调用 PI Id 控制器宏
        // ------------------------------------------------------------------------------
        if (lsw == 0)
            pi_id.Ref = IdLockRef;
        else
            pi_id.Ref = 0;
        pi_id.Fbk = park1.Ds;
        PI_MACRO(pi_id)

        // ------------------------------------------------------------------------------
        //  连接 INV_PARK 模块的输入并调用 Park 逆变换宏
        // ------------------------------------------------------------------------------
        ipark1.Ds = pi_id.Out;
        ipark1.Qs = pi_iq.Out;
        ipark1.Sine = park1.Sine;
        ipark1.Cosine = park1.Cosine;
        IPARK_MACRO(ipark1)

        // ------------------------------------------------------------------------------
        //    检测校准角度(可选)并调用 QEP 模块
        // ------------------------------------------------------------------------------
            if (lsw == 0)
            {
                EQEP_setPosition(myQEP_BASE, 0U);
                EQEP_clearInterruptStatus(myQEP_BASE,
                                          EQEP_INT_INDEX_EVNT_LATCH);
                //EQep1Regs.QPOSCNT = 0;
                //EQep1Regs.QCLR.bit.IEL = 1;
                TestTicker++;
            } // 复位位置计数

            if ((EQEP_getInterruptStatus(myQEP_BASE) & EQEP_INT_INDEX_EVNT_LATCH)
                    && Init_IFlag == 0) // 检查首次 index 信号
            {
                qep1.CalibratedAngle = EQEP_getIndexPositionLatch(myQEP_BASE);
                //qep1.CalibratedAngle = EQep1Regs.QPOSILAT;
                Init_IFlag++;
            }   // 保持锁存的位置

            if (lsw != 0)
                QEP_MACRO(&qep1);

        // ------------------------------------------------------------------------------
        //    连接 SPEED_FR 模块的输入并调用速度计算宏
        // ------------------------------------------------------------------------------
        speed1.ElecTheta = qep1.ElecTheta;
        speed1.DirectionQep = (int32) (qep1.DirectionQep);
        SPEED_FR_MACRO(speed1)

        // ------------------------------------------------------------------------------
        //    连接 VOLT_CALC 模块的输入并调用相电压宏
        // ------------------------------------------------------------------------------
        //  volt1.DcBusVolt = ((AdcMirror.ADCRESULT7)*0.00024414)*0.909; // 母线电压测量
        volt1.DcBusVolt = Udc;
        volt1.MfuncV1 = svgen1.Ta;
        volt1.MfuncV2 = svgen1.Tb;
        volt1.MfuncV3 = svgen1.Tc;
        PHASEVOLT_MACRO(volt1)

        // ------------------------------------------------------------------------------
        //  连接 SVGEN_DQ 模块的输入并调用空间矢量生成宏
        // ------------------------------------------------------------------------------
        svgen1.Ualpha = ipark1.Alpha;
        svgen1.Ubeta = ipark1.Beta;
        SVGENDQ_MACRO(svgen1)

        // ------------------------------------------------------------------------------
        //  连接 PWM_DRV 模块的输入并调用 PWM 信号生成宏
        // ------------------------------------------------------------------------------
        pwm1.MfuncC1 = svgen1.Ta;
        pwm1.MfuncC2 = svgen1.Tb;
        pwm1.MfuncC3 = svgen1.Tc;
        PWM_MACRO(&pwm1);
        // 计算新的 PWM 比较值

        // #endif // (BUILDLEVEL==LEVEL5)
    }


    if (BUILDLEVEL == 0)
    {
        InverterProtect(&pwm1);
    }

    ExDA_A = qep1.ElecTheta;
    ExDA_B = qep1.MechTheta;
    ExDA_C = pi_iq.Ref * 10;        //需确保 IqRef < 0.1
    ExDA_D = pi_iq.Fbk * 10;    //
    // ExDA_A = ExDA_A_Test;
    // ExDA_B = ExDA_B_Test;
    // ExDA_C = ExDA_C_Test;
    // ExDA_D = ExDA_D_Test;

    DA_Ctrl(ExDA_A,ExDA_B,ExDA_C,ExDA_D);
    // 使能来自该定时器的更多中断
    //EPwm1Regs.ETCLR.bit.INT = 1;
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);

    // 应答中断以接收来自 PIE 第 3 组的更多中断
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);

}

__interrupt void OffsetISR(void)
{
    // 验证 ISR
    OffsetIsrTicker++;
    IsrTicker++;

    // ADC 直流零漂测量

    if (IsrTicker >= 1000)
    {
        AD7606_CTRL(&motor1);
//        offsetA = K1 * offsetA + K2 * (motor1.Ia) * 0.00024414; //A 相零漂
//        offsetB = K1 * offsetB + K2 * (motor1.Ib) * 0.00024414; //B 相零漂
//        offsetUdc = K1 * offsetUdc + K2 * (motor1.Udc) * 0.00024414; //C 相零漂
        offsetA = K1 * offsetA + K2 * (motor1.Ia) ; //A 相零漂
        offsetB = K1 * offsetB + K2 * (motor1.Ib) ; //B 相零漂
        offsetUdc = K1 * offsetUdc + K2 * (motor1.Udc) ; //C 相零漂

    }

    if (IsrTicker > 30000)
    {
        EALLOW;
        Interrupt_register(INT_EPWM1, &MainISR);               //确定中断类型和中断服务程序地址
        EDIS;
    }

    // 使能来自该定时器的更多中断
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);

    // 应答中断以接收来自 PIE 第 3 组的更多中断
    // PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

void MemCopy(Uint16 *SourceAddr, Uint16 *SourceEndAddr, Uint16 *DestAddr)
{
    while (SourceAddr < SourceEndAddr)
    {
        *DestAddr++ = *SourceAddr++;
    }
    return;
}

void InverterRST_Init(void)
{
    GPIO_setPinConfig(GPIO_8_GPIO8);
    GPIO_setDirectionMode(8, GPIO_DIR_MODE_OUT);    // GPIO43 = 输出
}

void InverterProtect(PWMGEN *v)
{
    _iq ClosePWM = -1.0;

    EPWM_setCounterCompareValue(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
    EPWM_setCounterCompareValue(EPWM2_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
    EPWM_setCounterCompareValue(EPWM3_BASE, EPWM_COUNTER_COMPARE_A, _IQmpy(ClosePWM,v->HalfPerMax) + v->HalfPerMax);
}
