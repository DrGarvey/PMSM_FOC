/*
 * isr.h
 *
 * 中断服务程序声明。实现见 isr.c。
 */

#ifndef APP_ISR_H_
#define APP_ISR_H_

__interrupt void MainISR(void);
__interrupt void OffsetISR(void);

#endif /* APP_ISR_H_ */
