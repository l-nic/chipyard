# Compile
make debug CONFIG=LNICSimNetworkQuadRocketConfig -j16

# Run
./simulator-chipyard-LNICSimNetworkQuadRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 ../../tests/hello.riscv

# LNIC Load Evaluation Program
./simulator-chipyard-LNICSimNetworkQuadRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 ../../tests/lnic-load-balance.riscv 1 10.0.0.2 DIF_PRIORITY_LNIC_DRIVEN 10 5

