################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
user/inc/%.obj: ../user/inc/%.asm $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"D:/Programs/Research/ti/ccs/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -O4 --fp_mode=relaxed --include_path="D:/Program Documents/CCS projects/PMSM_FOC_28377" --include_path="D:/Program Documents/CCS projects/PMSM_FOC_28377/FOC" --include_path="D:/Program Documents/CCS projects/PMSM_FOC_28377/user/inc" --include_path="D:/Program Documents/CCS projects/PMSM_FOC_28377/driver" --include_path="D:/Program Documents/CCS projects/PMSM_FOC_28377/driverlib" --include_path="D:/Program Documents/CCS projects/PMSM_FOC_28377/driverlib/inc" --include_path="D:/Programs/Research/ti/ccs/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/include" --define=CPU1 --define=_LAUNCHXL_F28379D -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --preproc_with_compile --preproc_dependency="user/inc/$(basename $(<F)).d_raw" --obj_directory="user/inc" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


