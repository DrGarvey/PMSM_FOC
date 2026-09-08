/*
 * isr.c
 *
 * EPWM1 中断服务程序。中断在计数到零(TBCTR=0)时触发，频率 10kHz。
 *
 * 两个中断服务函数：
 *   1) OffsetISR：上电后前 30000 个中断周期内运行，采集电流/母线电压零漂(offset)
 *      标定。完成后动态将中断服务函数切换为 MainISR。
 *   2) MainISR：FOC 主中断，根据 BUILDLEVEL 分级执行不同控制算法：
 *        - BUILDLEVEL 1：SVPWM 开环输出
 *        - BUILDLEVEL 2：VF 开环控制（虚拟角度）
 *        - BUILDLEVEL 3：电流闭环（虚拟角度）
 *        - BUILDLEVEL 5：速度闭环（QEP 真实角度）
 *        - BUILDLEVEL 0：关闭 PWM 输出(保护模式)
 */

#include "globals.h"
#include "isr.h"

__interrupt void MainISR(void)
{
    // 验证 ISR
    MainIsrTicker++;

    // =============================== LEVEL 1 ======================================
    //    ！！！不接主电路，不接电机！！！！
    //    该程序段主要是为了检测 PWM 端口输出是否正常，以及程序基础计算是否正常
    // ==============================================================================

    if (BUILDLEVEL == 1)
    {
        LV1Ticker++;
        // 斜坡函数：使两个给定值之间缓慢变化，避免给定大幅跳变影响稳定性
        rc1.TargetValue = SpeedRef;
        RC_MACRO(rc1)

        // 虚拟角度生成：根据斜坡输出自动生成 0-2pi 角度信号，用于 VF 控制或前期验证
        rg1.Freq = rc1.SetpointValue;
        RG_MACRO(rg1)

        // Park 逆变换电压给定（标幺值，需小于 1），角度为虚拟角度
        ipark1.Ds = VdTesting;
        ipark1.Qs = VqTesting;

        // Park 逆变换计算
        ipark1.Sine = _IQsinPU(rg1.Out);
        ipark1.Cosine = _IQcosPU(rg1.Out);
        IPARK_MACRO(ipark1)

        // 根据 Park 逆变换输出进行 SVPWM 计算
        svgen1.Ualpha = ipark1.Alpha;
        svgen1.Ubeta = ipark1.Beta;
        SVGENDQ_MACRO(svgen1)

        // 将 SVPWM 结果输出到 PWM
        pwm1.MfuncC1 = svgen1.Ta;
        pwm1.MfuncC2 = svgen1.Tb;
        pwm1.MfuncC3 = svgen1.Tc;
        PWM_MACRO(&pwm1);
    }

    // =============================== LEVEL 2 ======================================
    // ！将控制板与驱动系统及电机连接！
    // VF 控制，角度由虚拟模块产生；电流由传感器采样但不进入闭环，仅用于坐标变换验证。
    // 电压不宜过高，一般 40-60V。频率推荐 5Hz。
    // lsw = 0：不执行（防启动大电流抱轴）；lsw != 0：VF 运行。
    // ==============================================================================

    else if (BUILDLEVEL == 2)
    {
        LV2Ticker++;

        if (lsw != 0)
        {
            rc1.TargetValue = SpeedRef;
            RC_MACRO(rc1)

            rg1.Freq = rc1.SetpointValue;
            RG_MACRO(rg1)

            // AD 采样 A、B 相电流，用于 Clarke 计算（标幺值范围 -1~+1）
            AD7606_CTRL(&motor1);
            clarke1.As = motor1.Ia - offsetA;
            clarke1.Bs = motor1.Ib - offsetB;
            CLARKE_MACRO(clarke1)

            // Park 变换：abc -> dq
            park1.Alpha = clarke1.Alpha;
            park1.Beta = clarke1.Beta;
            park1.Angle = rg1.Out;
            park1.Sine = _IQsinPU(park1.Angle);
            park1.Cosine = _IQcosPU(park1.Angle);
            PARK_MACRO(park1)

            // Park 逆变换：VqTesting 确定输出电压幅值
            ipark1.Ds = VdTesting;
            ipark1.Qs = VqTesting;
            ipark1.Sine = park1.Sine;
            ipark1.Cosine = park1.Cosine;
            IPARK_MACRO(ipark1)

            // SVPWM 计算
            svgen1.Ualpha = ipark1.Alpha;
            svgen1.Ubeta = ipark1.Beta;
            SVGENDQ_MACRO(svgen1)

            // PWM 输出
            pwm1.MfuncC1 = svgen1.Ta;
            pwm1.MfuncC2 = svgen1.Tb;
            pwm1.MfuncC3 = svgen1.Tc;
            PWM_MACRO(&pwm1);
        }
    }

    // =============================== LEVEL 3 ======================================
    // ！将控制板与驱动系统及电机连接！
    // 电流闭环控制，角度由虚拟模块产生，用于测试 PI 与速度计算模块。
    // lsw = 0：抱轴（强制吸附至电气 0 位，电流由 IdLockRef 控制）
    // lsw = 1：电流闭环运行（虚拟角度）
    // lsw = 3：跳过执行
    // ==============================================================================

    else if (BUILDLEVEL == 3)
    {
        LV3Ticker++;

        if (lsw != 3)
        {
            if (lsw == 0)
                rc1.TargetValue = 0;
            else
                rc1.TargetValue = SpeedRef;
            RC_MACRO(rc1)

            rg1.Freq = rc1.SetpointValue;
            RG_MACRO(rg1)

            // 采样相电流，减去零漂，Clarke 变换
            AD7606_CTRL(&motor1);
            clarke1.As = motor1.Ia - offsetA;
            clarke1.Bs = motor1.Ib - offsetB;
            CLARKE_MACRO(clarke1)

            // Park 变换
            park1.Alpha = clarke1.Alpha;
            park1.Beta = clarke1.Beta;
            if (lsw == 0)
                park1.Angle = 0;
            else if (lsw == 1)
                park1.Angle = rg1.Out;

            park1.Sine = _IQsinPU(park1.Angle);
            park1.Cosine = _IQcosPU(park1.Angle);
            PARK_MACRO(park1)

            // Iq 电流环 PI
            if (lsw == 0)
                pi_iq.Ref = 0;
            else if (lsw == 1)
                pi_iq.Ref = IqRef;
            pi_iq.Fbk = park1.Qs;
            PI_MACRO(pi_iq)

            // Id 电流环 PI
            if (lsw == 0)
                pi_id.Ref = IdLockRef;
            else
                pi_id.Ref = IdRef;
            pi_id.Fbk = park1.Ds;
            PI_MACRO(pi_id)

            // Park 逆变换
            ipark1.Ds = pi_id.Out;
            ipark1.Qs = pi_iq.Out;
            ipark1.Sine = park1.Sine;
            ipark1.Cosine = park1.Cosine;
            IPARK_MACRO(ipark1)

            // QEP 编码器角度校准
            if (lsw == 0)
            {
                EQEP_setPosition(myQEP_BASE, 0U);
                EQEP_clearInterruptStatus(myQEP_BASE, EQEP_INT_INDEX_EVNT_LATCH);
                TestTicker++;
            }

            if ((EQEP_getInterruptStatus(myQEP_BASE) & EQEP_INT_INDEX_EVNT_LATCH)
                    && Init_IFlag == 0)
            {
                qep1.CalibratedAngle = EQEP_getIndexPositionLatch(myQEP_BASE);
                Init_IFlag++;
            }

            if (lsw != 0)
                QEP_MACRO(&qep1);

            // 速度计算
            speed1.ElecTheta = qep1.ElecTheta;
            speed1.DirectionQep = (int32) (qep1.DirectionQep);
            SPEED_FR_MACRO(speed1)

            // 相电压计算
            volt1.DcBusVolt = motor1.Udc;
            volt1.MfuncV1 = svgen1.Ta;
            volt1.MfuncV2 = svgen1.Tb;
            volt1.MfuncV3 = svgen1.Tc;
            PHASEVOLT_MACRO(volt1)

            // SVPWM
            svgen1.Ualpha = ipark1.Alpha;
            svgen1.Ubeta = ipark1.Beta;
            SVGENDQ_MACRO(svgen1)

            // PWM 输出
            pwm1.MfuncC1 = svgen1.Ta;
            pwm1.MfuncC2 = svgen1.Tb;
            pwm1.MfuncC3 = svgen1.Tc;
            PWM_MACRO(&pwm1);
        }
    }

    // =============================== LEVEL 5 ======================================
    // 速度闭环：验证由 PI 模块完成的速度调节器，速度环用测得速度作为反馈闭环。
    // lsw = 0：锁定电机转子；lsw = 1：闭合电流环；lsw = 2：速度闭环。
    // ==============================================================================

    else if (BUILDLEVEL == 5)
    {
        LV5Ticker++;

        if (lsw == 0)
            rc1.TargetValue = 0;
        else
            rc1.TargetValue = SpeedRef;
        RC_MACRO(rc1)

        rg1.Freq = rc1.SetpointValue;
        RG_MACRO(rg1)

        AD7606_CTRL(&motor1);
        clarke1.As = motor1.Ia - offsetA;
        clarke1.Bs = motor1.Ib - offsetB;
        CLARKE_MACRO(clarke1)

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

        // 速度环 PI（按预分频降低计算频率）
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

        // Iq 电流环 PI
        if (lsw == 0)
            pi_iq.Ref = 0;
        else if (lsw == 1)
            pi_iq.Ref = IqRef;
        else
            pi_iq.Ref = pi_spd.Out;

        pi_iq.Fbk = park1.Qs;
        PI_MACRO(pi_iq)

        // Id 电流环 PI
        if (lsw == 0)
            pi_id.Ref = IdLockRef;
        else
            pi_id.Ref = 0;
        pi_id.Fbk = park1.Ds;
        PI_MACRO(pi_id)

        // Park 逆变换
        ipark1.Ds = pi_id.Out;
        ipark1.Qs = pi_iq.Out;
        ipark1.Sine = park1.Sine;
        ipark1.Cosine = park1.Cosine;
        IPARK_MACRO(ipark1)

        // QEP 编码器角度校准
        if (lsw == 0)
        {
            EQEP_setPosition(myQEP_BASE, 0U);
            EQEP_clearInterruptStatus(myQEP_BASE, EQEP_INT_INDEX_EVNT_LATCH);
            TestTicker++;
        }

        if ((EQEP_getInterruptStatus(myQEP_BASE) & EQEP_INT_INDEX_EVNT_LATCH)
                && Init_IFlag == 0)
        {
            qep1.CalibratedAngle = EQEP_getIndexPositionLatch(myQEP_BASE);
            Init_IFlag++;
        }

        if (lsw != 0)
            QEP_MACRO(&qep1);

        // 速度计算
        speed1.ElecTheta = qep1.ElecTheta;
        speed1.DirectionQep = (int32) (qep1.DirectionQep);
        SPEED_FR_MACRO(speed1)

        // 相电压计算
        volt1.DcBusVolt = Udc;
        volt1.MfuncV1 = svgen1.Ta;
        volt1.MfuncV2 = svgen1.Tb;
        volt1.MfuncV3 = svgen1.Tc;
        PHASEVOLT_MACRO(volt1)

        // SVPWM
        svgen1.Ualpha = ipark1.Alpha;
        svgen1.Ubeta = ipark1.Beta;
        SVGENDQ_MACRO(svgen1)

        // PWM 输出
        pwm1.MfuncC1 = svgen1.Ta;
        pwm1.MfuncC2 = svgen1.Tb;
        pwm1.MfuncC3 = svgen1.Tc;
        PWM_MACRO(&pwm1);
    }

    // =============================== LEVEL 0 ======================================
    // 保护模式：关闭 PWM 输出
    // ==============================================================================
    if (BUILDLEVEL == 0)
    {
        InverterProtect(&pwm1);
    }

    // 外部 DAC 观测输出
    ExDA_A = qep1.ElecTheta;
    ExDA_B = qep1.MechTheta;
    ExDA_C = pi_iq.Ref * 10;        // 需确保 IqRef < 0.1
    ExDA_D = pi_iq.Fbk * 10;
    DA_Ctrl(ExDA_A, ExDA_B, ExDA_C, ExDA_D);

    // 使能来自该定时器的更多中断
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);

    // 应答中断以接收来自 PIE 第 3 组的更多中断
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}

__interrupt void OffsetISR(void)
{
    // 验证 ISR
    OffsetIsrTicker++;
    IsrTicker++;

    // ADC 直流零漂测量（低通滤波）
    if (IsrTicker >= 1000)
    {
        AD7606_CTRL(&motor1);
        offsetA = K1 * offsetA + K2 * (motor1.Ia);   // A 相零漂
        offsetB = K1 * offsetB + K2 * (motor1.Ib);   // B 相零漂
        offsetUdc = K1 * offsetUdc + K2 * (motor1.Udc);   // 母线电压零漂
    }

    // 运行满 30000 个周期后，动态切换中断服务函数为 MainISR
    if (IsrTicker > 30000)
    {
        EALLOW;
        Interrupt_register(INT_EPWM1, &MainISR);
        EDIS;
    }

    // 使能来自该定时器的更多中断
    EPWM_clearEventTriggerInterruptFlag(EPWM1_BASE);

    // 应答中断以接收来自 PIE 第 3 组的更多中断
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP3);
}
