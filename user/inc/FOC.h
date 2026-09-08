/*
 * FOC.h
 *
 *  Created on: 2024年6月20日
 *      Author: jwzho
 */

#ifndef USER_INC_FOC_H_
#define USER_INC_FOC_H_

//#include "user.h"




//#ifndef BUILDLEVEL
//#error  Critical: BUILDLEVEL must be defined !!
//#endif  // BUILDLEVEL


#define PI 3.14159265358979

// 定义电机电气参数 (Estun 伺服电机)
#define RS      1.191251//0.2022099         // 定子电阻 (欧姆)
#define RR                              // 转子电阻 (欧姆)
#define LS      0.011956 //0.0006719129     // 定子电感 (H)
#define LR                              // 转子电感 (H)
#define LM                              // 励磁电感 (H)
#define POLES   8                   // 极数

// 定义基准量
#define BASE_VOLTAGE    236.14        // 相电压峰值基准 (伏), Vdc/sqrt(3)
#define BASE_CURRENT    9.9            // 相电流峰值基准 (安), 最大可测峰值电流
#define BASE_TORQUE                   // 转矩基准 (N.m)
#define BASE_FLUX                     // 磁链基准 (volt.sec/rad)
#define BASE_FREQ       200           // 电气频率基准 (Hz)




#endif /* USER_INC_FOC_H_ */
