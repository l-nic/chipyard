# Compile
make CONFIG=LNICRocketConfig -j16 debug

# Run
./simv-chipyard-LNICRocketConfig-debug +permissive -sv_lib riscv_dpi +vcdfile=SimNetworkLNICGPRConfig.vcd +permissive-off ../../tests/hello.riscv
