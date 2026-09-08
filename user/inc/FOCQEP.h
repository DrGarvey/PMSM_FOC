/*
 * FOCQEP.h
 *
 *  Created on: 2024年6月21日
 *      Author: jwzho
 */

#ifndef USER_INC_FOCQEP_H_
#define USER_INC_FOCQEP_H_


/*-----------------------------------------------------------------------------
QEP 对象的默认初始化值
-----------------------------------------------------------------------------*/

#define QEP_DEFAULTS { 0x0,0x0,0x0,0x0,0x0,0x0,0x00020000,0x0,2,0,0x0}



void QEP_INIT(QEP *v);
void InitEqepGPIO(void);
void QEP_MACRO(QEP *v);




#endif /* USER_INC_FOCQEP_H_ */
