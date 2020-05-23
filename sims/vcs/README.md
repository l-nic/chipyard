# Compile
make CONFIG=LNICRocketConfig -j16 debug
make CONFIG=SimNetworkLNICGPRConfig TOP=TopWithLNIC -j16 debug

rm -rf csrc && vcs -full64 -notice -line  riscv_lib.TestDriver -o /home/vagrant/chipyard/sims/vcs/simv-example-SimNetworkLNICGPRConfig-debug -debug_access+designer -V

# Run
./simv-example-SimNetworkLNICGPRConfig-debug +permissive -sv_lib riscv_dpi +vcdfile=SimNetworkLNICGPRConfig.vcd +permissive-off ../../tests/hello.riscv
