#!/usr/bin/env python2

import argparse
import sys, os
import time
import subprocess
import shlex

PYTHON_CMD = 'python -m unittest transport-tests.{test_class}.{test_name}'
HOMA_CMD = '../../sims/vcs/simv-chipyard-LNICP4HomaSimNetworkSingleRocketConfig-debug +permissive -sv_lib ../../sims/vcs/riscv_dpi +vcdfile=LNICRocketConfig.vcd +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 +verbose +permissive-off ../../tests-lnic/{binary}'
NDP_CMD = '../../sims/vcs/simv-chipyard-LNICP4NDPSimNetworkSingleRocketConfig-debug +permissive -sv_lib ../../sims/vcs/riscv_dpi +vcdfile=LNICRocketConfig.vcd +nic_mac_addr=081122334408 +switch_mac_addr=085566778808 +nic_ip_addr=A000002 +timeout_cycles=30000 +rtt_pkts=2 +verbose +permissive-off ../../tests-lnic/{binary}'

# default cmdline args
cmd_parser = argparse.ArgumentParser()
cmd_parser.add_argument('--all', action='store_true', help='Run sims for all figures', required=False)
cmd_parser.add_argument('--latency', action='store_true', help='Run sim for Loopback Latency', required=False)
cmd_parser.add_argument('--throughput', action='store_true', help='Run sim for Loopback Throughput', required=False)
cmd_parser.add_argument('--stream_throughput', action='store_true', help='Run sim for Stateless Processing Throughput', required=False)

def start_proc(cmd):
    print '------------------- Starting Process -------------------'
    print '$ {}'.format(cmd)
    print '--------------------------------------------------------'
    return subprocess.Popen(cmd, shell=True, stdout=sys.stdout, stderr=sys.stderr)

def run_latency_sim():
    print 'Running sim for Loopback Latency ...'
    test_name = 'test_latency'
    #### NDP simulation
    test_class = 'NdpTest'
    # start python test
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name=test_name))
    # start simulation
    sim = start_proc(NDP_CMD.format(binary='lnic-loopback-latency.riscv'))
    # wait for test to complete
    test.wait()
    # kill the simulation
    sim.kill()

    #### Homa simulation
    test_class = 'HomaTest'
    # start python test
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name=test_name))
    # start simulation
    sim = start_proc(HOMA_CMD.format(binary='lnic-loopback-latency.riscv'))
    # wait for test to complete
    test.wait()
    # kill the simulation
    sim.kill()

def run_throughput_sim():
    print 'Running sim for Loopback Throughput ...'
    test_name = 'test_throughput'
    #### NDP simulation
    test_class = 'NdpTest'
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name=test_name))
    sim = start_proc(NDP_CMD.format(binary='lnic-loopback-throughput.riscv'))
    test.wait()
    sim.kill()

    #### Homa simulation
    test_class = 'HomaTest'
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name=test_name))
    sim = start_proc(HOMA_CMD.format(binary='lnic-loopback-throughput.riscv'))
    test.wait()
    sim.kill()

def run_stream_throughput_sim():
    print 'Running sim for Stateless Processing Throughput ...'
    test_name = 'test_stream_throughput'
    #### NDP simulation
    test_class = 'NdpTest'
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name=test_name))
    sim = start_proc(NDP_CMD.format(binary='lnic-stream-throughput.riscv'))
    test.wait()
    sim.kill()

    #### Homa simulation
    test_class = 'HomaTest'
    test = start_proc(PYTHON_CMD.format(test_class=test_class, test_name=test_name))
    sim = start_proc(HOMA_CMD.format(binary='lnic-stream-throughput.riscv'))
    test.wait()
    sim.kill()

def main():
    args = cmd_parser.parse_args()
    # Run the simulations
    if args.all or args.latency:
        run_latency_sim()
    if args.all or args.throughput:
        run_throughput_sim()
    if args.all or args.stream_throughput:
        run_stream_throughput_sim()

if __name__ == '__main__':
    main()
