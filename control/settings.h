/*
 * settings.h
 *
 * 集中定义系统与电机参数（编译期常量）。
 * 原分散于 user/inc/FOC.h、user/inc/user.h、main.c 中的参数统一收敛到此文件。
 */

#ifndef CONTROL_SETTINGS_H_
#define CONTROL_SETTINGS_H_

/* ===================== 系统时钟 ===================== */
#define CPU_RATE          5.00L                      // CPU 时钟 (SYSCLKOUT)，200MHz
#define SYSTEM_FREQUENCY  200                        // 系统时钟频率 (MHz)
#define ISR_FREQUENCY     10                         // 中断(ISR)频率 (kHz)
#define SAMPLE_TIME       (0.001 / ISR_FREQUENCY)    // 采样周期 (秒)

/* ===================== 数学常数 ===================== */
#define PI                3.14159265358979

/* ===================== 电机电气参数 (Estun 伺服电机) ===================== */
#define RS                1.191251                   // 定子电阻 (Ω)
#define LS                0.011956                   // 定子电感 (H)
#define POLES             8                          // 极数

/* ===================== 标幺基准量 ===================== */
#define BASE_VOLTAGE      236.14                     // 相电压峰值基准 (V)
#define BASE_CURRENT      9.9                        // 相电流峰值基准 (A)
#define BASE_FREQ         200                        // 电气频率基准 (Hz)

/* ===================== 编码器 ===================== */
#define LINE_ENCODER      2500                       // 编码器线数 (线)

#endif /* CONTROL_SETTINGS_H_ */
