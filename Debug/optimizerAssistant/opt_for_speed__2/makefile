################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := D:/Program Files/Professional/TI/CCS1/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"./F2837xD_CodeStartBranch.obj" \
"./main.obj" \
"./driver/device.obj" \
"./driverlib/adc.obj" \
"./driverlib/asysctl.obj" \
"./driverlib/can.obj" \
"./driverlib/cla.obj" \
"./driverlib/clb.obj" \
"./driverlib/cmpss.obj" \
"./driverlib/cputimer.obj" \
"./driverlib/dac.obj" \
"./driverlib/dcsm.obj" \
"./driverlib/dma.obj" \
"./driverlib/ecap.obj" \
"./driverlib/emif.obj" \
"./driverlib/epwm.obj" \
"./driverlib/eqep.obj" \
"./driverlib/flash.obj" \
"./driverlib/gpio.obj" \
"./driverlib/hrpwm.obj" \
"./driverlib/i2c.obj" \
"./driverlib/interrupt.obj" \
"./driverlib/ipc.obj" \
"./driverlib/mcbsp.obj" \
"./driverlib/memcfg.obj" \
"./driverlib/sci.obj" \
"./driverlib/sdfm.obj" \
"./driverlib/spi.obj" \
"./driverlib/sysctl.obj" \
"./driverlib/upp.obj" \
"./driverlib/usb.obj" \
"./driverlib/version.obj" \
"./driverlib/xbar.obj" \
"./user/inc/F2837xD_usDelay.obj" \
"./user/src/AD7606.obj" \
"./user/src/FOCPWM.obj" \
"./user/src/FOCQEP.obj" \
"../2837xD_RAM_lnk_cpu1.cmd" \
"../2837x_FLASH_lnk_cpu1.cmd" \
"../driverlib/ccs/Debug/driverlib.lib" \
$(GEN_CMDS__FLAG) \
-llibc.a \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include CPU1_RAM/subdir_vars.mk
-include driver/subdir_vars.mk
-include driverlib/subdir_vars.mk
-include driverlib/ccs/Debug/subdir_vars.mk
-include user/inc/subdir_vars.mk
-include user/src/subdir_vars.mk
-include subdir_rules.mk
-include CPU1_RAM/subdir_rules.mk
-include driver/subdir_rules.mk
-include driverlib/subdir_rules.mk
-include driverlib/ccs/Debug/subdir_rules.mk
-include user/inc/subdir_rules.mk
-include user/src/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(INO_DEPS)),)
-include $(INO_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(PDE_DEPS)),)
-include $(PDE_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
PMSM_FOC_28377.out 

EXE_OUTPUTS__QUOTED += \
"PMSM_FOC_28377.out" 


# All Target
all: $(OBJS) $(CMD_SRCS) $(LIB_SRCS) $(GEN_CMDS)
	@$(MAKE) --no-print-directory -Onone "PMSM_FOC_28377.out"

# Tool invocations
PMSM_FOC_28377.out: $(OBJS) $(CMD_SRCS) $(LIB_SRCS) $(GEN_CMDS)
	@echo 'Building target: "$@"'
	@echo 'Invoking: C2000 Linker'
	"D:/Program Files/Professional/TI/CCS1/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 --opt_for_speed=2 --fp_mode=relaxed --define=CPU1 -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi -z -m"PMSM_FOC_28377.map" --stack_size=0x200 --warn_sections -i"D:/Program Files/Professional/TI/CCS1/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/lib" -i"D:/Program Files/Professional/TI/CCS1/ccs/tools/compiler/ti-cgt-c2000_22.6.1.LTS/include" --reread_libs --diag_wrap=off --display_error_number --xml_link_info="PMSM_FOC_28377_linkInfo.xml" --rom_model -o "PMSM_FOC_28377.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXE_OUTPUTS__QUOTED)
	-$(RM) "F2837xD_CodeStartBranch.obj" "main.obj" "driver\device.obj" "driverlib\adc.obj" "driverlib\asysctl.obj" "driverlib\can.obj" "driverlib\cla.obj" "driverlib\clb.obj" "driverlib\cmpss.obj" "driverlib\cputimer.obj" "driverlib\dac.obj" "driverlib\dcsm.obj" "driverlib\dma.obj" "driverlib\ecap.obj" "driverlib\emif.obj" "driverlib\epwm.obj" "driverlib\eqep.obj" "driverlib\flash.obj" "driverlib\gpio.obj" "driverlib\hrpwm.obj" "driverlib\i2c.obj" "driverlib\interrupt.obj" "driverlib\ipc.obj" "driverlib\mcbsp.obj" "driverlib\memcfg.obj" "driverlib\sci.obj" "driverlib\sdfm.obj" "driverlib\spi.obj" "driverlib\sysctl.obj" "driverlib\upp.obj" "driverlib\usb.obj" "driverlib\version.obj" "driverlib\xbar.obj" "user\inc\F2837xD_usDelay.obj" "user\src\AD7606.obj" "user\src\FOCPWM.obj" "user\src\FOCQEP.obj" 
	-$(RM) "main.d" "driver\device.d" "driverlib\adc.d" "driverlib\asysctl.d" "driverlib\can.d" "driverlib\cla.d" "driverlib\clb.d" "driverlib\cmpss.d" "driverlib\cputimer.d" "driverlib\dac.d" "driverlib\dcsm.d" "driverlib\dma.d" "driverlib\ecap.d" "driverlib\emif.d" "driverlib\epwm.d" "driverlib\eqep.d" "driverlib\flash.d" "driverlib\gpio.d" "driverlib\hrpwm.d" "driverlib\i2c.d" "driverlib\interrupt.d" "driverlib\ipc.d" "driverlib\mcbsp.d" "driverlib\memcfg.d" "driverlib\sci.d" "driverlib\sdfm.d" "driverlib\spi.d" "driverlib\sysctl.d" "driverlib\upp.d" "driverlib\usb.d" "driverlib\version.d" "driverlib\xbar.d" "user\src\AD7606.d" "user\src\FOCPWM.d" "user\src\FOCQEP.d" 
	-$(RM) "F2837xD_CodeStartBranch.d" "user\inc\F2837xD_usDelay.d" 
	-@echo 'Finished clean'
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

