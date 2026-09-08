/*
 * ad7606.h
 *
 * AD7606 外部 ADC 驱动：EMIF 并行接口采样。
 *
 *  Created on: 2023年3月7日
 *      Author: Jy
 */

#ifndef HARDWARE_INC_AD7606_H_
#define HARDWARE_INC_AD7606_H_

#define ADCS1        (volatile Uint16 *)0x00310000
#define ADCS2        (volatile Uint16 *)0x00320000

#define SCALE  10.0  //量程为10.0V，   量程为10V或5V可以通过板子上P11端口的跳线帽来设置
#define BUF_SIZE  6

void AD7606_CTRL(MOTOR *v);
void AD7606_INIT(void);
void InitAD7606_EMIF_GPIO(void);
void InitAD7606_EMIF(void);

extern int16 addat[BUF_SIZE];
extern float32 exadc[BUF_SIZE];
extern float32 offseta;
extern float32 offsetb;
extern float32 offsetc;
extern float32 offsetf;

#endif /* HARDWARE_INC_AD7606_H_ */
