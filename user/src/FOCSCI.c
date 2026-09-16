/*
 * FOCSCI.c
 *
 *  Created on: 2026年9月16日
 *      Author: jwzho
 */

#include "user.h"
#include "FOCSCI.h"

/*==============================================================================
                        发送环形缓冲
==============================================================================*/
static Uint16 SciTxBuf[SCI_TX_BUF_SIZE];
static Uint16 SciTxHead = 0U;       /* 写入位置 */
static Uint16 SciTxTail = 0U;       /* 读出位置 */
static Uint16 SciTelemCnt = 0U;     /* 遥测分频计数 */

/*------------------------------------------------------------------------------
  内部函数：把一个字符放进环形缓冲

  缓冲满时直接丢弃。遥测是观测功能，丢一帧不影响控制，
  但绝不能因为串口发不出去而拖慢 10kHz 中断。
------------------------------------------------------------------------------*/
static void SCI_Queue(Uint16 c)
{
    Uint16 next = (Uint16)((SciTxHead + 1U) % SCI_TX_BUF_SIZE);

    if (next == SciTxTail)
    {
        return;                     /* 缓冲满，丢弃 */
    }

    SciTxBuf[SciTxHead] = c;
    SciTxHead = next;
}

/*------------------------------------------------------------------------------
  内部函数：把环形缓冲里的数据搬进发送 FIFO

  每个中断周期调用一次。搬多少由 FIFO 的空位决定，
  FIFO 满就一个都不搬，下一拍再继续，全程不等待。
------------------------------------------------------------------------------*/
static void SCI_Drain(void)
{
    while ((SciTxTail != SciTxHead) &&
           (SCI_getTxFIFOStatus(SCIA_BASE) != SCI_FIFO_TX16))
    {
        SCI_writeCharNonBlocking(SCIA_BASE, SciTxBuf[SciTxTail]);
        SciTxTail = (Uint16)((SciTxTail + 1U) % SCI_TX_BUF_SIZE);
    }
}

/*==============================================================================
                        数字格式化

  不用 printf —— 在 C28x 上它会拖进大量 libc 代码（缓冲区、浮点格式化等），
  而这里只需要把几个数转成 ASCII，自己写更小更快。
==============================================================================*/

/*------------------------------------------------------------------------------
  内部函数：输出一个 32 位有符号整数的十进制形式，带负号
------------------------------------------------------------------------------*/
static void SCI_PutInt(int32 value)
{
    char    buf[12];
    int     n = 0;
    Uint32  u;

    if (value < 0)
    {
        SCI_Queue((Uint16)'-');
        u = (Uint32)(-value);
    }
    else
    {
        u = (Uint32)value;
    }

    if (u == 0U)
    {
        SCI_Queue((Uint16)'0');
        return;
    }

    /* 从低位往高位逐位取出，再倒序输出 */
    while ((u > 0U) && (n < 11))
    {
        buf[n] = (char)('0' + (u % 10U));
        u /= 10U;
        n++;
    }

    while (n > 0)
    {
        n--;
        SCI_Queue((Uint16)buf[n]);
    }
}

/*------------------------------------------------------------------------------
  内部函数：输出一个标幺值，放大 1000 倍后按整数输出

  例：0.5 → "500"，-0.123 → "-123"。
  这样上位机收到的是定点整数，不需要解析浮点，也避免了浮点格式化的开销。
  放大倍率固定用 1000，与遥测帧里的其它字段一致。
------------------------------------------------------------------------------*/
static void SCI_PutPu(_iq value)
{
    SCI_PutInt((int32)(value * 1000.0f));
}

/*==============================================================================
                        初始化
==============================================================================*/
void SCI_INIT(void)
{
    /* 引脚复用：GPIO42 发送、GPIO43 接收（板载 FTDI 后背通道） */
    GPIO_setPinConfig(GPIO_42_SCITXDA);
    GPIO_setPinConfig(GPIO_43_SCIRXDA);

    /* 配置：LSPCLK = 50MHz，115200 波特率，8 位数据、1 位停止、无校验
       （不传校验位即为无校验，SCI_CONFIG_PAR_NONE 的数值就是 0） */
    SCI_setConfig(SCIA_BASE, DEVICE_LSPCLK_FREQ, SCI_BAUDRATE,
                  (SCI_CONFIG_WLEN_8 | SCI_CONFIG_STOP_ONE |
                   SCI_CONFIG_PAR_NONE));

    SCI_enableFIFO(SCIA_BASE);
    SCI_resetTxFIFO(SCIA_BASE);
    SCI_enableModule(SCIA_BASE);

    SciTxHead   = 0U;
    SciTxTail   = 0U;
    SciTelemCnt = 0U;

    SCI_SendString("PMSM_FOC_28377 ready\r\n");
}

/*==============================================================================
                        每拍调用

  由 main.c 的 Observe_Run() 调用。内部已经做了分频，
  所以每个中断周期调用它是安全的。
==============================================================================*/
void SCI_MACRO(const SCI_TELEM *t)
{
    /* ---- 先搬缓冲，保证上一帧能发完 ---- */
    SCI_Drain();

    /* ---- 分频：每 SCI_TELEM_PRESCALER 个中断周期组织一帧 ---- */
    SciTelemCnt++;
    if (SciTelemCnt < SCI_TELEM_PRESCALER)
    {
        return;
    }
    SciTelemCnt = 0U;

    /* ---- 组帧：逗号分隔的 ASCII，行尾 CRLF ----
       字段顺序：
         BUILDLEVEL, lsw, 故障码, 跳闸标志, 电气角度, 转速,
         Iq给定, Iq反馈, 母线电压, A相电流, B相电流
       角度与各标幺量放大 1000 倍，转速是整数 rpm。 */
    SCI_PutInt((int32)t->BuildLevel);   SCI_Queue((Uint16)',');
    SCI_PutInt((int32)t->Lsw);          SCI_Queue((Uint16)',');
    SCI_PutInt((int32)t->FaultCode);    SCI_Queue((Uint16)',');
    SCI_PutInt((int32)t->Tripped);      SCI_Queue((Uint16)',');
    SCI_PutPu(t->ElecTheta);            SCI_Queue((Uint16)',');
    SCI_PutInt(t->SpeedRpm);            SCI_Queue((Uint16)',');
    SCI_PutPu(t->IqRef);                SCI_Queue((Uint16)',');
    SCI_PutPu(t->IqFbk);                SCI_Queue((Uint16)',');
    SCI_PutPu(t->Udc);                  SCI_Queue((Uint16)',');
    SCI_PutPu(t->Ia);                   SCI_Queue((Uint16)',');
    SCI_PutPu(t->Ib);
    SCI_Queue((Uint16)'\r');
    SCI_Queue((Uint16)'\n');
}

/*==============================================================================
                        直接发送字符串

  仅用于非中断上下文（例如 main() 启动阶段打印版本信息）。
  内部会等 FIFO 有空位——此时系统还没进入控制循环，等待是安全的。

  **绝对不要在中断里调用它**，长字符串会把中断拖住好几毫秒。
==============================================================================*/
void SCI_SendString(const char *s)
{
    while (*s != '\0')
    {
        while (SCI_getTxFIFOStatus(SCIA_BASE) == SCI_FIFO_TX16)
        {
            ;                       /* 等 FIFO 腾出空位 */
        }
        SCI_writeCharNonBlocking(SCIA_BASE, (Uint16)(*s));
        s++;
    }
}
