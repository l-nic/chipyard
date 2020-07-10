# Compile
make debug CONFIG=LNICSimNetworkRocketConfig -j16

# Run
./simv-chipyard-LNICSimNetworkRocketConfig-debug +permissive -sv_lib riscv_dpi +vcdfile=LNICRocketConfig.vcd +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000001 +verbose +permissive-off ../../tests/hello.riscv
