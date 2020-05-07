
# Build SDNet IP

set build_dir $::env(CHIPYARD_ROOT)/vivado/ip
set proj_dir ${build_dir}/managed_ip_project
set xci_file ${build_dir}/${sdnet}/${sdnet}.xci

open_project ${proj_dir}/managed_ip_project.xpr

# Delete the sdnet IP if it already exists
if {![string equal [get_ips $sdnet] ""]} {
    puts "INFO: Deleting current ${sdnet} IP"
    remove_files ${xci_file}
    file delete -force ${build_dir}/${sdnet}
}

create_ip -name sdnet -vendor xilinx.com -library ip -version 2.1 -module_name $sdnet -dir $build_dir
set_property -dict [subst {
    CONFIG.TDATA_NUM_BYTES {64}
    CONFIG.P4_FILE {${p4_file}}
    CONFIG.DEBUG_IO_CAPTURE_ENABLE {true}
    CONFIG.CAM_DEBUG_HW_LOOKUP {true}
    CONFIG.CAM_MEM_CLK_FREQ_MHZ {250.0}
    CONFIG.AXIS_CLK_FREQ_MHZ {250.0}
    CONFIG.PKT_RATE {250.0}
    CONFIG.DIRECT_TABLE_PARAMS {}
    CONFIG.CAM_TABLE_PARAMS {}
}] [get_ips $sdnet]
generate_target all [get_ips $sdnet]

export_ip_user_files -of_objects [get_files ${xci_file}] -no_script -sync -force -quiet
export_simulation -absolute_path -of_objects [get_files ${xci_file}] -directory ${build_dir}/ip_user_files/sim_scripts -ip_user_files_dir ${build_dir}/ip_user_files -ipstatic_source_dir ${build_dir}/ip_user_files/ipstatic -lib_map_path vcs=${build_dir}/managed_ip_project/managed_ip_project.cache/compile_simlib/vcs -use_ip_compiled_libs -force -quiet

