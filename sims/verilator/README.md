# Compiling the Simulation Binary

## L-NIC
Compilation command:
```
# To compile w/ NDP support:
make debug CONFIG=LNICNDPSimNetworkSingleRocketConfig -j16

# To compile w/ Homa support:
make debug CONFIG=LNICHomaSimNetworkSingleRocketConfig -j16
```

## IceNIC
Compilation command:
```
make debug CONFIG=IceNICSimNetworkRocketConfig TOP=TopIceNIC MODEL=TestHarnessIceNIC VLOG_MODEL=TestHarnessIceNIC -j16
```

# Running the Simulation

# L-NIC
A simple hello world program:
```
./simulator-chipyard-LNICNDPSimNetworkSingleRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 ../../tests-lnic/hello.riscv
```

L-NIC load evaluation program:
```
./simulator-chipyard-LNICSimNetworkQuadRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 ../../tests-lnic/lnic-evaluation.riscv 1 10.0.0.2 DIF_PRIORITY_LNIC_DRIVEN 10 5
```

# IceNIC
Hello world example:
```
./simulator-chipyard-IceNICSimNetworkRocketConfig-debug --vcd=IceNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 ../../tests-icenic/hello.riscv
```

