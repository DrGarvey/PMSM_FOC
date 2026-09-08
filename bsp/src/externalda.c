/*
 * ExternalDA.c
 *
 *  外部 DAC 输出驱动：将内部标幺值(pu, 范围约 -1~+1)转换为外部 12 位 DAC 的
 *  单极性输出码。
 *
 *  映射关系：DA 输出码 = data * 2048 + 2048
 *    - data = -1.0  -> 输出码 0    (0V)
 *    - data =  0.0  -> 输出码 2048 (半量程/中点)
 *    - data = +1.0  -> 输出码 4096 (满量程，但 12 位 DAC 上限为 4095)
 *  因此调用前将输入钳位在 ±0.99，保证输出码落在 [20, 4075]，留出裕量避免饱和。
 *
 *  在 main.c 的 MainISR 中用于观察：ElecTheta、MechTheta、IqRef、IqFbk。
 *  注意：若传入量超过 ±1(如 ExDA_C = IqRef*10)，会被钳位到 ±0.99 而饱和。
 */

#include "bsp.h"

void DA_Ctrl(float32 data_a, float32 data_b, float32 data_c, float32 data_d)
{
	// 逐通道钳位到 ±0.99，防止超出 12 位 DAC 量程
	if(data_a>=0.99)	data_a=0.99;
	if(data_a<=-0.99)	data_a=-0.99;
	*DA_ADD0  = data_a * 2048 + 2048;

	if(data_b>=0.99)	data_b=0.99;
	if(data_b<=-0.99)	data_b=-0.99;
	*DA_ADD1  = data_b * 2048 + 2048;

	if(data_c>=0.99)	data_c=0.99;
	if(data_c<=-0.99)	data_c=-0.99;
	*DA_ADD2  = data_c * 2048 + 2048;

	if(data_d>=0.99)	data_d=0.99;
	if(data_d<=-0.99)	data_d=-0.99;
	*DA_ADD3  = data_d * 2048 + 2048;
}
