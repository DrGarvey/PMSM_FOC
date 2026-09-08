/*
 * AD7606.h
 *
 *  Created on: 2023年3月7日
 *      Author: Jy
 */

#ifndef USER_INCLUDE_AD7606_H_
#define USER_INCLUDE_AD7606_H_



#define ADCS1        (volatile Uint16 *)0x00310000
#define ADCS2        (volatile Uint16 *)0x00320000

#define SCALE  10.0  //量程为10.0V，   量程为10V或5V可以通过板子上P11端口的跳线帽来设置
#define BUF_SIZE  6

void AD7606_CTRL(MOTOR *v);
void AD7606_INIT(void);
void InitAD7606_EMIF_GPIO(void);
void InitAD7606_EMIF(void);


extern int16 addat[BUF_SIZE];
extern float exadc[BUF_SIZE];
extern float offseta;
extern float offsetb;
extern float offsetc;
extern float offsetf;

#endif /* USER_INCLUDE_AD7606_H_ */
