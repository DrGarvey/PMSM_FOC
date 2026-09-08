/*
 * focpwm.h
 *
 * PWM 模块：EPWM 初始化、PWM 输出宏，以及逆变器使能/保护。
 *
 *  Created on: 2024年6月20日
 *      Author: jwzho
 */

#ifndef HARDWARE_INC_FOCPWM_H_
#define HARDWARE_INC_FOCPWM_H_

/*------------------------------------------------------------------------------
            F2833X PWMGEN 对象的默认初始化值
------------------------------------------------------------------------------*/
#define F2833X_FC_PWM_GEN    { 2500,  \
                               1000,  \
                               200,  \
                               0x4000, \
                               0x4000, \
                               0x4000, \
                              }

#define PWMGEN_DEFAULTS     F2833X_FC_PWM_GEN

void PWM_INIT(PWMGEN *v);
void PWM_MACRO(PWMGEN *v);
void InitEpwmGPIO(void);
void InverterRST_Init(void);
void InverterProtect(PWMGEN *v);

#endif /* HARDWARE_INC_FOCPWM_H_ */
