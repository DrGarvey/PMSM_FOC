/*
 * isr.h
 *
 * 中断服务程序声明。实现见 isr.c。
 */

#ifndef CONTROL_ISR_H_
#define CONTROL_ISR_H_

__interrupt void MainISR(void);
__interrupt void OffsetISR(void);

#endif /* CONTROL_ISR_H_ */
