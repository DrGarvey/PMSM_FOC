/*
 * FOCQEP.c
 *
 *  Created on: 2024年6月21日
 *      Author: jwzho
 */


#include "eqep.h"
#include "pin_map.h"
#include "hardware.h"

void InitEqepGPIO(void);

void QEP_INIT(QEP *v)
{
    InitEqepGPIO();

    EQEP_setEmulationMode(myQEP_BASE, EQEP_EMULATIONMODE_RUNFREE);

    //QEPCTL.PCRM = 0 | QPOSMAX = 4*2500
    //位置计数器计到 QPOSMAX 时回零，即每转一圈（4*线数 个计数）自然翻转一次。
    //（原注释写的是"index 信号到来时清零"，那是 EQEP_POSITION_RESET_IDX 的语义，
    //  与下面的代码不符。这里用的 RESET_MAX_POS 与 TI 官方 qep_pos_speed 例程一致，
    //  index 的偏差单独由 QEP_MACRO 里的 CalibratedAngle 补偿。）
    //EQEP_setPositionCounterConfig(myQEP_BASE, EQEP_POSITION_RESET_IDX, 4*v->LineEncoder);
    EQEP_setPositionCounterConfig(myQEP_BASE, EQEP_POSITION_RESET_MAX_POS, 4*v->LineEncoder);
    
    //QEPCTL.QCLM = 1
    //每隔一个单位时间锁存一些信息至QPOSLAT,QCTMRLAT,QCPRDLAT寄存器内
    //QEPCTL.IEL = 1
    //index信号到来时，将位置计数器数据锁存至QPOSILAT，将方向标志位锁存至QEPSTS[QDLF]位
    EQEP_setLatchMode(myQEP_BASE, (EQEP_LATCH_RISING_INDEX | EQEP_LATCH_UNIT_TIME_OUT));

    //QEPCTL.UTE = 1
    //使能单位时间计数器
    EQEP_enableUnitTimer(myQEP_BASE, 2000000U);      //该数值需要修改，2000000在200MHZ是100Hz

    //QEPCTL.IEI = 2
    //EQEPI信号上升沿更新位置计数器
    //EQEP_setPositionInitMode(myQEP_BASE, EQEP_INIT_RISING_INDEX);

    //QEPCTL.QPEN = 1
    //使能QEP
    EQEP_enableModule(myQEP_BASE);

    //QCAPCTL.UPPS = 5
    //QCAPCTL.CCPS = 0x70
    EQEP_setCaptureConfig(myQEP_BASE, EQEP_CAPTURE_CLK_DIV_64, EQEP_UNIT_POS_EVNT_DIV_32);
    //QCAPCTL.CEN = 1
    EQEP_enableCapture(myQEP_BASE);
    
//    //允许QEP2中断
//    EQEP_enableInterrupt(EQEP2_BASE, EQEP_INT_INDEX_EVNT_LATCH);
}

void QEP_MACRO(QEP *v)
{
    /* 检查旋转方向 */
    v->DirectionQep = EQEP_getDirection(myQEP_BASE);
    /* 读取 EQEP2 的位置计数器 */
    v->RawTheta = EQEP_getPosition(myQEP_BASE) + v->CalibratedAngle;

    if (v->RawTheta < 0)
        v->RawTheta = v->RawTheta + 4*v->LineEncoder;
    else if (v->RawTheta > 4*v->LineEncoder)
        v->RawTheta = v->RawTheta - 4*v->LineEncoder;

    /* 计算机械角度 */
    v->MechTheta= v->MechScaler*v->RawTheta;
    /* 计算电气角度  */
    v->ElecTheta = (v->PolePairs*v->MechTheta) -floor(v->PolePairs*v->MechTheta);

    /* 检查 index 信号 */
    if (EQEP_getInterruptStatus(myQEP_BASE) & EQEP_INT_INDEX_EVNT_LATCH) //QFLG.bit.IEL == 1
    {
        v->IndexSyncFlag = 0x00F0U;
        v->QepCountIndex = EQEP_getIndexPositionLatch(myQEP_BASE);
        EQEP_clearInterruptStatus(myQEP_BASE, EQEP_INT_INDEX_EVNT_LATCH);
    }

    /* 检查单位超时事件以进行速度计算： */
    /* 单位定时器在 INIT 函数中配置为 100Hz */
    if (EQEP_getInterruptStatus(myQEP_BASE) & EQEP_INT_UNIT_TIME_OUT) // QFLG.bit.UTO == 1
    {
        if ((EQEP_getStatus(myQEP_BASE) & EQEP_STS_CAP_OVRFLW_ERROR) || ((EQEP_getStatus(myQEP_BASE) & EQEP_STS_CAP_DIR_ERROR)))
        // QEPSTS.COEF == 1 || QEPSTS.CDEF == 1
        {
            EQEP_clearStatus(myQEP_BASE, (EQEP_STS_CAP_OVRFLW_ERROR | EQEP_STS_CAP_DIR_ERROR));
        }
        else if(EQEP_getCapturePeriodLatch(myQEP_BASE) != 0xFFFFU)
            v->QepPeriod = EQEP_getCapturePeriodLatch(myQEP_BASE);
    }

}

void InitEqepGPIO(void)
{

    // 本工程实际使用的是 EQEP2（见 user.h 的 myQEP_BASE 定义）
    GPIO_setPinConfig(GPIO_24_EQEP2A);                // GPIO24 = EQEP2A
    GPIO_setPinConfig(GPIO_25_EQEP2B);                // GPIO25 = EQEP2B
    GPIO_setPinConfig(GPIO_26_EQEP2I);                // GPIO26 = EQEP2I

    // 下面三行把 GPIO20/21/23 复用成 EQEP1，但 EQEP1 模块本工程从未使能或读取，
    // 属于预留（将来接第二路编码器时可直接使用），当前不影响功能
    GPIO_setPinConfig(GPIO_20_EQEP1A);                // GPIO20 = EQEP1A
    GPIO_setPinConfig(GPIO_21_EQEP1B);                // GPIO21 = EQEP1B
    GPIO_setPinConfig(GPIO_23_EQEP1I);                // GPIO23 = EQEP1I

}
