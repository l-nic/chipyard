# Compile
make debug CONFIG=LNICSimNetworkRocketConfig -j16

# Run
./simv-chipyard-LNICSimNetworkRocketConfig-debug +permissive -sv_lib riscv_dpi +vcdfile=LNICSimNetworkRocketConfig.vcd +permissive-off ../../tests/hello.riscv
