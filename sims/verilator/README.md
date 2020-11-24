# Compiling the Simulation Binary

## L-NIC
Compilation command:
```
make debug CONFIG=LNICSimNetworkQuadRocketConfig -j16
```

## IceNIC
To use IceNIC, we currently need to make a few manual changes to a few source files:
* `chipyard/generators/chipyard/src/main/scala/Top.scala` - uncomment IceNIC lines and comment out LNIC lines
* `chipyard/tests/crt.S`
```
--- a/tests/crt.S
+++ b/tests/crt.S
@@ -145,8 +145,9 @@ _start:
   sll sp, sp, STKSHIFT
   add sp, sp, tp

+  # Do not do this when using IceNIC!
   # start timer on NIC to track msg processing time
-  csrwi 0x54, 0x4
+#  csrwi 0x54, 0x4
```
* `chipyard/tests/syscalls.c`
```
--- a/tests/syscalls.c
+++ b/tests/syscalls.c
@@ -486,8 +486,9 @@ void _init(int cid, int nc)
     }
   }

+  // NOTE: do not do this when using IceNIC!
   // wait for lnicrdy CSR to be set
-  while (read_csr(0x057) == 0);
+//  while (read_csr(0x057) == 0);

   thread_entry(cid, nc);
```

Make sure to `make clean && make` in `chipyard/tests` to recompile the test programs with these changes.

Then compile the simulator binary:
```
make debug CONFIG=IceNICSimNetworkRocketConfig -j16
```

# Running the Simulation

# L-NIC
A simple hello world program:
```
./simulator-chipyard-LNICSimNetworkQuadRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 ../../tests/hello.riscv
```
L-NIC load evaluation program:
```
./simulator-chipyard-LNICSimNetworkQuadRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 ../../tests/lnic-evaluation.riscv 1 10.0.0.2 DIF_PRIORITY_LNIC_DRIVEN 10 5
```

# IceNIC
Hello world example:
```
./simulator-chipyard-IceNICSimNetworkRocketConfig-debug --vcd=IceNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 ../../tests/hello.riscv
```

