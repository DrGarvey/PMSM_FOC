/*
 * FOCPWM.h
 *
 *  Created on: 2024年6月20日
 *      Author: jwzho
 */

#ifndef USER_INC_FOCPWM_H_
#define USER_INC_FOCPWM_H_

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

#endif /* USER_INC_FOCPWM_H_ */
