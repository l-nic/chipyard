# Compile P4-NDP
make debug CONFIG=LNICP4NDPSimNetworkSingleRocketConfig -j16

# Compile P4-Homa
make debug CONFIG=LNICP4HomaSimNetworkSingleRocketConfig -j16

# Run
./simv-chipyard-LNICP4NDPSimNetworkSingleRocketConfig-debug +permissive -sv_lib riscv_dpi +vcdfile=LNICRocketConfig.vcd +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 +verbose +permissive-off ../../tests-lnic/hello.riscv
