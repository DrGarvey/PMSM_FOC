/*
 * AD7606.c
 *
 *  Created on: 2023年3月7日
 *      Author: Jy
 */

#include "bsp.h"

int16 addat[BUF_SIZE];
float32 exadc[BUF_SIZE];
float32 offseta;
float32 offsetb;
float32 offsetc;
float32 offsetf;


void AD7606_INIT(void)
{

    //AD7606采样后通过外设接口并行将数据传输至DSP，因此需要配置相关端口的功能为EMIF模式
    //EMIF = External Memory InterFace
    InitAD7606_EMIF_GPIO();
    //配置完端口后，需要对EMIF寄存器进行设置，设定EMIF的工作模式
    InitAD7606_EMIF();

	//ADCON
    GPIO_setPadConfig(43, GPIO_PIN_TYPE_STD);        // GPIO43设为标准模式，非pushup模式
    GPIO_setQualificationMode(43,GPIO_QUAL_SYNC);    // 将GPIO的采样模式设为同步，确保采样发生在GPIO时钟上升沿
    GPIO_setPinConfig(GPIO_43_GPIO43);               // 确定GPIO43的复用功能为GPIO43
    GPIO_setDirectionMode(43, GPIO_DIR_MODE_OUT);    // GPIO43 = 输出
    GPIO_setControllerCore(43, GPIO_CORE_CPU1);      // 由于28377D有两个CPU，所以设置GPIO43由CPU1控制

    //ADINT
    GPIO_setPadConfig(68, GPIO_PIN_TYPE_STD);        // 同GPIO43
    GPIO_setQualificationMode(68,GPIO_QUAL_SYNC);    // 同GPIO43
    GPIO_setPinConfig(GPIO_68_GPIO68);               // 同GPIO43
    GPIO_setDirectionMode(68, GPIO_DIR_MODE_OUT);    // 同GPIO43
    GPIO_setControllerCore(68, GPIO_CORE_CPU1);      // 同GPIO43

}

void AD7606_CTRL(MOTOR *v)
{
    int i;

//**************采样开始-启动ADC7606**************/////////////
//==============AD7606的采样开始标志为GPIO43先拉高再拉低再拉高=====
    GPIO_writePin(43, 1U);
    DELAY_US(0.2L);

    GPIO_writePin(43, 0U);
    DELAY_US(0.2L);

    GPIO_writePin(43, 1U);
    //DELAY_US(2L);

    while(GPIO_readPin(68) != 0)
    {
       // v->ADWaitTicker++;
    }
    if(GPIO_readPin(68) == 0)//AD_BUSY
    {
        //v->ADRdTicker++;
        v->ADRdTicker++;
        // TODO：当前 6 次读取均来自同一地址 ADCS1(通道未区分)。开发板集成两块 AD7606
        // (ADCS1=0x310000, ADCS2=0x320000)，并行接口通过地址线 A[2:0] 选择通道，
        // 若需采集 Ia/Ib/Udc 等不同通道，应改为读取不同地址(通道)，此处疑似占位未完成。
        addat[0] =   *ADCS1;
        addat[1] =   *ADCS1;
        addat[2] =   *ADCS1;
        addat[3] =   *ADCS1;
        addat[4] =   *ADCS1;
        addat[5] =   *ADCS1;
        //傅里叶的开发板上集成了两块AD7606芯片，每块芯片可以采集8路信号
    }
    for(i=0;i<4;i++)//i<BUF_SIZE   // 仅标幺化前 4 路；addat[4]/[5] 未使用
    {
    	//exadc[i]=8.33333*(addat[i]*0.00030518);//Um=Rm*Is=Rm*（3/1000)//1.5152

        //AD7606采样回的信号格式为int16，是一个有符号数，因此最高位表示正负
        //数据的范围为[-32768,32768]
        //为便于程序移植，本程序段内所有信号都进行了标幺化处理
        exadc[i]=addat[i] / 32768.0;
    }

    //离线测试时发现1，3，5路信号更为稳定
    v->Ia=exadc[0];
    v->Ib=exadc[1];
    v->Udc=exadc[2];
}

void InitAD7606_EMIF_GPIO(void)
{
    int i;

    GPIO_setPinConfig(GPIO_92_EM1BA1);
    GPIO_setPinConfig(GPIO_91_EM1A18);
    GPIO_setPinConfig(GPIO_90_EM1A17);
    GPIO_setPinConfig(GPIO_89_EM1A16);
    GPIO_setPinConfig(GPIO_88_EM1A15);
    GPIO_setPinConfig(GPIO_87_EM1A14);
    GPIO_setPinConfig(GPIO_86_EM1A13);

    GPIO_setPinConfig(GPIO_52_EM1A12);
    GPIO_setPinConfig(GPIO_51_EM1A11);
    GPIO_setPinConfig(GPIO_50_EM1A10);
    GPIO_setPinConfig(GPIO_49_EM1A9);
    GPIO_setPinConfig(GPIO_48_EM1A8);
    GPIO_setPinConfig(GPIO_47_EM1A7);
    GPIO_setPinConfig(GPIO_46_EM1A6);
    GPIO_setPinConfig(GPIO_45_EM1A5);

    GPIO_setPinConfig(GPIO_44_EM1A4);
    GPIO_setPinConfig(GPIO_41_EM1A3);
    GPIO_setPinConfig(GPIO_40_EM1A2);
    GPIO_setPinConfig(GPIO_39_EM1A1);
    GPIO_setPinConfig(GPIO_38_EM1A0);

    GPIO_setPinConfig(GPIO_69_EM1D15);
    GPIO_setPinConfig(GPIO_70_EM1D14);
    GPIO_setPinConfig(GPIO_71_EM1D13);
    GPIO_setPinConfig(GPIO_72_EM1D12);
    GPIO_setPinConfig(GPIO_73_EM1D11);
    GPIO_setPinConfig(GPIO_74_EM1D10);
    GPIO_setPinConfig(GPIO_75_EM1D9);
    GPIO_setPinConfig(GPIO_76_EM1D8);
    GPIO_setPinConfig(GPIO_77_EM1D7);
    GPIO_setPinConfig(GPIO_78_EM1D6);
    GPIO_setPinConfig(GPIO_79_EM1D5);
    GPIO_setPinConfig(GPIO_80_EM1D4);
    GPIO_setPinConfig(GPIO_81_EM1D3);
    GPIO_setPinConfig(GPIO_82_EM1D2);
    GPIO_setPinConfig(GPIO_83_EM1D1);
    GPIO_setPinConfig(GPIO_85_EM1D0);

    GPIO_setPinConfig(GPIO_35_EM1CS3N);
    GPIO_setPinConfig(GPIO_34_EM1CS2N);
    GPIO_setPinConfig(GPIO_31_EM1WEN);
    GPIO_setPinConfig(GPIO_37_EM1OEN);


    GPIO_setDirectionMode(31, GPIO_DIR_MODE_OUT);

    GPIO_setDirectionMode(34, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(35, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(37, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(38, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(39, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(40, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(41, GPIO_DIR_MODE_OUT);

    GPIO_setDirectionMode(44, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(45, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(46, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(47, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(48, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(49, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(50, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(51, GPIO_DIR_MODE_IN);
    GPIO_setDirectionMode(52, GPIO_DIR_MODE_OUT);

    GPIO_setDirectionMode(86, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(87, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(88, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(89, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(90, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(91, GPIO_DIR_MODE_OUT);


    for (i=69; i<=85; i++)
    {
        if (i != 84)
        {
            GPIO_setDirectionMode(i, GPIO_DIR_MODE_IN);
            GPIO_setPadConfig(i, GPIO_PIN_TYPE_PULLUP);
            GPIO_setQualificationMode(i, GPIO_QUAL_ASYNC);
        }
    }

}

void InitAD7606_EMIF(void)
{
    SysCtl_setEMIF1ClockDivider(SYSCTL_EMIF1CLK_DIV_1);
    SysCtl_setEPWMClockDivider(SYSCTL_EPWMCLK_DIV_2);

    EMIF_selectController(EMIF1CONFIG_BASE, EMIF_CONTROLLER_CPU1_G);

    EMIF_setAsyncMode(EMIF1_BASE, EMIF_ASYNC_CS2_OFFSET,
                      EMIF_ASYNC_NORMAL_MODE);
    //EMIF_enableAsyncExtendedWait(EMIF1_BASE, EMIF_O_ASYNC_CS2_CR);

    EMIF_AsyncTimingParams tParams1 = { .rSetup = 0,    // 示例值，根据实际需要调整
            .rStrobe = 7,   // 示例值，根据实际需要调整
            .rHold = 0,      // 示例值，根据实际需要调整
            .wSetup = 0,     // 示例值，根据实际需要调整
            .wStrobe = 7,   // 示例值，根据实际需要调整
            .wHold = 0,      // 示例值，根据实际需要调整
            .turnArnd = 0    // 示例值，根据实际需要调整
            };

    EMIF_setAsyncTimingParams(EMIF1_BASE, EMIF_ASYNC_CS2_OFFSET, &tParams1);
    EMIF_setAsyncDataBusWidth(EMIF1_BASE, EMIF_ASYNC_CS2_OFFSET,
                              EMIF_ASYNC_DATA_WIDTH_16);

    EMIF_setAsyncMode(EMIF1_BASE, EMIF_ASYNC_CS3_OFFSET,
                      EMIF_ASYNC_NORMAL_MODE);
    //EMIF_enableAsyncExtendedWait(EMIF1_BASE, EMIF_O_ASYNC_CS2_CR);

    EMIF_AsyncTimingParams tParams2 = { .rSetup = 0,    // 示例值，根据实际需要调整
            .rStrobe = 11,   // 示例值，根据实际需要调整
            .rHold = 0,      // 示例值，根据实际需要调整
            .wSetup = 0,     // 示例值，根据实际需要调整
            .wStrobe = 4,   // 示例值，根据实际需要调整
            .wHold = 0,      // 示例值，根据实际需要调整
            .turnArnd = 0    // 示例值，根据实际需要调整
            };

    EMIF_setAsyncTimingParams(EMIF1_BASE, EMIF_ASYNC_CS3_OFFSET, &tParams2);
    EMIF_setAsyncDataBusWidth(EMIF1_BASE, EMIF_ASYNC_CS3_OFFSET,
                              EMIF_ASYNC_DATA_WIDTH_16);

}
