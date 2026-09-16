/*
 * externalda.h
 *
 * 外部 DAC 输出驱动。
 */

#ifndef HARDWARE_INC_EXTERNALDA_H_
#define HARDWARE_INC_EXTERNALDA_H_

#define DA_ADD0      (volatile Uint16 *)0x00330000
#define DA_ADD1      (volatile Uint16 *)0x00330001
#define DA_ADD2      (volatile Uint16 *)0x00330002
#define DA_ADD3      (volatile Uint16 *)0x00330003

void DA_Ctrl(float32 data_a, float32 data_b, float32 data_c, float32 data_d);

#endif /* HARDWARE_INC_EXTERNALDA_H_ */
