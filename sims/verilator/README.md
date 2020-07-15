# Compile
make debug CONFIG=LNICSimNetworkRocketConfig -j16

# Run
./simulator-chipyard-LNICSimNetworkRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000001 ../../tests/hello.riscv
