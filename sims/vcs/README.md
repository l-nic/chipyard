# Compile
make debug CONFIG=LNICRocketConfig -j16

# Run
./simv-chipyard-LNICRocketConfig-debug +permissive -sv_lib riscv_dpi +vcdfile=LNICRocketConfig.vcd +permissive-off ../../tests/hello.riscv
