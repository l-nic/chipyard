# Compile
make debug CONFIG=LNICSimNetworkQuadRocketConfig -j16

# Run
./simulator-chipyard-LNICSimNetworkQuadRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000001 ../../tests/hello.riscv
