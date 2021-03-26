#!/usr/bin/env python2

import argparse
import sys, os
import time
import subprocess
import shlex

PYTHON_CMD = 'python -m unittest net-app-tester.{test_class}.{test_name}'
LNIC_CMD = '../../sims/verilator/simulator-chipyard-LNICSimNetworkQuadRocketConfig-debug --vcd=LNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 ../../tests-lnic/{binary}'
ICENIC_CMD = '../../sims/verilator/simulator-chipyard-IceNICSimNetworkRocketConfig-debug --vcd=IceNICRocketConfig.vcd +verbose +nic_mac_addr=081122334408 ../../tests-icenic/{binary}'

# default cmdline args
cmd_parser = argparse.ArgumentParser()
cmd_parser.add_argument('--test', action='store_true', help='Run simple test simulation', required=False)
cmd_parser.add_argument('--all', action='store_true', help='Run sims for all figures', required=False)
cmd_parser.add_argument('--fig3', action='store_true', help='Run sim for figure 3 (Loopback Latency)', required=False)
cmd_parser.add_argument('--fig4', action='store_true', help='Run sim for figure 4 (Loopback Throughput)', required=False)
cmd_parser.add_argument('--fig5', action='store_true', help='Run sim for figure 5 (Stateless Processing Throughput)', required=False)
cmd_parser.add_argument('--fig6', action='store_true', help='Run sim for figure 6 (Dot Product Microbenchmark)', required=False)

def start_proc(cmd):
    print '------------------- Starting Process -------------------'
    print '$ {}'.format(cmd)
    print '--------------------------------------------------------'
    return subprocess.Popen(cmd, shell=True, stdout=sys.stdout, stderr=sys.stderr)

def run_test():
    print 'Running basic test simulation ...'
    test_class = 'LoopbackTest'
    #### LNIC simulation
    # start python test
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_latency_basic_lnic'))
    time.sleep(1)
    # start simulation
    sim = start_proc(LNIC_CMD.format(binary='lnic-loopback-latency.riscv'))
    # wait for test to complete
    test.wait()
    # kill the simulation
    sim.kill()
    time.sleep(1)

    #### IceNIC simulation
    # start python test
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_latency_basic_icenic'))
    time.sleep(1)
    # start simulation
    sim = start_proc(ICENIC_CMD.format(binary='icenic-loopback-latency.riscv'))
    # wait for test to complete
    test.wait()
    # kill the simulation
    sim.kill()

def run_fig3():
    print 'Running sim for Figure 3 (Loopback Latency) ...'
    test_class = 'LoopbackTest'
    #### LNIC simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_latency_lnic'))
    time.sleep(1)
    sim = start_proc(LNIC_CMD.format(binary='lnic-loopback-latency.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

    #### IceNIC simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_latency_icenic'))
    time.sleep(1)
    sim = start_proc(ICENIC_CMD.format(binary='icenic-loopback-latency.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

def run_fig4():
    print 'Running sim for Figure 4 (Loopback Throughput) ...'
    test_class = 'LoopbackTest'
    # LNIC simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_throughput_lnic'))
    time.sleep(1)
    sim = start_proc(LNIC_CMD.format(binary='lnic-loopback-throughput.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

    # IceNIC simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_throughput_icenic'))
    time.sleep(1)
    sim = start_proc(ICENIC_CMD.format(binary='icenic-loopback-throughput-batch.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

def run_fig5():
    print 'Running sim for Figure 5 (Stateless Processing Throughput) ...'
    test_class = 'StreamTest'
    # LNIC simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_throughput_lnic'))
    time.sleep(1)
    sim = start_proc(LNIC_CMD.format(binary='lnic-stream-throughput.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

    # IceNIC simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_throughput_icenic'))
    time.sleep(1)
    sim = start_proc(ICENIC_CMD.format(binary='icenic-stream-batch.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

def run_fig6():      
    print 'Running sim for Figure 6 (Dot Product Microbenchmark) ...'
    test_class = 'DotProdTest'
    # LNIC Optimal simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_num_words_lnic_opt'))
    time.sleep(1)
    sim = start_proc(LNIC_CMD.format(binary='lnic-dot-product-opt.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

    # LNIC Naive simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_num_words_lnic_naive'))
    time.sleep(1)
    sim = start_proc(LNIC_CMD.format(binary='lnic-dot-product-naive.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

    # IceNIC simulation
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name='test_num_words_icenic'))
    time.sleep(1)
    sim = start_proc(ICENIC_CMD.format(binary='icenic-dot-product-batch.riscv'))
    test.wait()
    sim.kill()
    time.sleep(1)

def main():
    args = cmd_parser.parse_args()
    # Run the simulations
    if args.test:
        run_test()
    if args.all or args.fig3:
        run_fig3()
    if args.all or args.fig4:
        run_fig4()
    if args.all or args.fig5:
        run_fig5()
    if args.all or args.fig6:
        run_fig6()
    print "Simulations Complete!"

if __name__ == '__main__':
    main()
