################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
%.obj: ../%.asm $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"D:/Programs/ti/ccs/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -O4 --fp_mode=relaxed --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/FOC" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/user/inc" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/driver" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/driverlib" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/driverlib/inc" --include_path="D:/Programs/ti/ccs/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/include" --define=CPU1 -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

%.obj: ../%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"D:/Programs/ti/ccs/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla1 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu2 -O4 --fp_mode=relaxed --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/FOC" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/user/inc" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/driver" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/driverlib" --include_path="D:/OneDrive/3-Research/3-My projects/CCS Projects/PMSM_FOC/driverlib/inc" --include_path="D:/Programs/ti/ccs/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/include" --define=CPU1 -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --preproc_with_compile --preproc_dependency="$(basename $(<F)).d_raw" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


