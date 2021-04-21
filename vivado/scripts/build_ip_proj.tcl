
# Build Managed IP project and Compile Xilinx simulation libraries with VCS

set vcs_path /usr/synopsys/vcs/P-2019.06/bin
set proj_dir $::env(CHIPYARD_ROOT)/vivado/ip/managed_ip_project
# use the AWS FPGA
set part xcvu9p-flgb2104-2-i

create_project managed_ip_project ${proj_dir} -part $part -ip
set_property target_simulator VCS [current_project]

# Compile simulation libraries
compile_simlib -simulator vcs -simulator_exec_path ${vcs_path} -family all -language all -library all -dir ${proj_dir}/managed_ip_project.cache/compile_simlib/vcs -force

