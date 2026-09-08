# PMSM_FOC — 永磁同步电机矢量控制（Demo）

基于 TI **TMS320F28377D** 的永磁同步电机（PMSM）磁场定向控制（FOC）演示工程。

- 中断由 EPWM1 在计数到零（TBCTR=0）时触发，频率 **10 kHz**。
- 计算采用 IQmath 定点标幺值系统。
- 采用 **BUILDLEVEL 分级调试验证法**，从 PWM 开环逐步上电到速度闭环。

---

## 硬件平台

| 部件 | 说明 |
| ---- | ---- |
| 主控 | TMS320F28377D（傅里叶 28377D 开发板） |
| 电机 | Estun 伺服电机（FSPM，8 极） |
| 电流采样 | 外置 AD7606（EMIF 并行接口，未用片内 ADC） |
| 位置/速度 | QEP 编码器（2500 线） |
| 观测输出 | 外置 12 位 DAC（4 通道） |

---

## 目录结构

```
PMSM_FOC/
├── main.c                  # 主程序：外设初始化 + 参数设定 + 主循环
├── app/                    # 应用层
│   ├── isr.c / isr.h       # 中断服务程序（OffsetISR / MainISR）
│   ├── globals.c / globals.h  # 全局变量（定义 / extern 声明）
│   └── settings.h          # 系统与电机参数（编译期常量）
├── foc/                    # FOC 算法库（纯数学，无硬件依赖）
│   ├── clarke.h  park.h  ipark.h  pi.h  svgen.h
│   └── rampgen.h  rmp_cntl.h  volt_calc.h  speed_fr.h
├── bsp/                    # 板级支持包
│   ├── inc/                #   bsp.h、focpwm.h、focqep.h、ad7606.h、externalda.h
│   └── src/                #   focpwm.c、focqep.c、ad7606.c、externalda.c
├── driver/  driverlib/     # TI 驱动库（源码随工程拷贝，便于换机调试）
├── targetConfigs/          # 仿真器目标配置（.ccxml）
└── *.cmd  F2837xD_CodeStartBranch.asm   # 链接脚本与启动文件
```

---

## BUILDLEVEL 分级说明

| BUILDLEVEL | 控制方式 | 角度来源 | 用途 |
| ---------- | -------- | -------- | ---- |
| 0 | 保护模式（关闭 PWM） | — | 安全保护 |
| 1 | SVPWM 开环 | 虚拟角度 | 验证 PWM 端口与基础计算（不接电机） |
| 2 | VF 开环 | 虚拟角度 | 验证 ADC 与坐标变换 |
| 3 | 电流闭环 | 虚拟角度 | 验证电流环 PI 与速度计算 |
| 5 | 速度闭环 | QEP 真实角度 | 完整速度环 |

`BUILDLEVEL` 定义在 [app/globals.c](app/globals.c)，烧录前修改。

### lsw 运行状态标志

| lsw | 状态 |
| --- | ---- |
| 0 | 抱轴 / 锁定（转子拉至电气 0 位） |
| 1 | 电流环运行（虚拟角度） |
| 2 | 速度环运行（仅 LEVEL 5） |
| 3 | 跳过执行（仅 LEVEL 3） |

---

## 使用步骤

1. **修改使能标志**：[app/globals.c](app/globals.c) 中 `EnableFlag` 置 `TRUE`，否则主程序空转。
2. **选择 BUILDLEVEL**：按上表从低到高逐级上电验证。
3. **编译烧录**：RAM 调试使用 `2837xD_RAM_lnk_cpu1.cmd`；正式运行使用 FLASH 链接脚本。
4. **上电顺序**（LEVEL 2/3/5）：
   - 先接好控制板与驱动/电机，`lsw = 0` 抱轴，调节母线电压使相电流约 1A；
   - 再切 `lsw = 1`（电流环）或 `lsw = 2`（速度环）。

> ⚠️ LEVEL 1 仅验证 PWM，**不要接主电路/电机**；切换 lsw 前请确认电路接线正确。

---

## 参数调校

- **系统/电机/标幺常量**：见 [app/settings.h](app/settings.h)（时钟频率、极数、电阻电感、基准量、编码器线数等）。
- **闭环给定值（标幺）**：`VdTesting / VqTesting / IdRef / IqRef / IdLockRef / SpeedRef`，见 [app/globals.c](app/globals.c)。
- **PI 参数**：在 [main.c](main.c) 的 `main()` 中初始化（`pi_spd` / `pi_id` / `pi_iq`）。

---

## 已知注意事项

- AD7606 采样在 [bsp/src/ad7606.c](bsp/src/ad7606.c) 中，6 次读取均来自同一地址 `ADCS1`（通道未区分），多通道采集待完善。
- 外部 DAC 输出在 [app/isr.c](app/isr.c) 中，用于观测电气角/机械角/Iq 给定/反馈，超出 ±1 会被钳位。
