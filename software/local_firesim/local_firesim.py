import sys
from fabric.api import *
import time
import datetime
import os

link_latency = 280 # 140
switch_latency = 0
# Buffer size: 93Pkt=101184 or 57KB=58751 
# Note that we leave extra room for ctrl packets
high_priority_obuf = 93*1088+1 # 4375
low_priority_obuf = 93*1088+1 # 4375
# RTO of 6 us = 19200
timeout_cycles = 19200 # 2240 
rtt_pkts=10
enable_waveforms = True
simulator = 'VFireSim-debug' if enable_waveforms else 'VFireSim'

# cmdline args for load generator RISC-V program
# NOTE: the test type must match what is specified in switchconfig1.h!

#test_type = "ONE_CONTEXT_FOUR_CORES"
#test_type = "FOUR_CONTEXTS_FOUR_CORES"

test_type = "DIF_PRIORITY_LNIC_DRIVEN"
#test_type = "DIF_PRIORITY_TIMER_DRIVEN"

#test_type = "HIGH_PRIORITY_C1_STALL"
#test_type = "LOW_PRIORITY_C1_STALL"
c1_stall_factor = 10
c1_stall_freq = 5

# NOTE: if use_load_prog is set then MUST pass in the lnic-evaluation.riscv binary
use_load_prog = False
# server_ip_addrs = ["10.0.0.2", "10.0.0.3", "10.0.0.4"]
load_gen_args = "{} {} {}".format(test_type, c1_stall_factor, c1_stall_freq) if use_load_prog else ""
                # else "{} {} {}".format(server_ip_addrs[0], server_ip_addrs[1], server_ip_addrs[2])

# Note that this doesn't rebuild the simulator by default, since that can be pretty slow even when nothing has changed.
def build_components(num_sims, current_run):
    with cd("chipyard/software/local_firesim/PcapPlusPlus"):
        run("make -j16")
        run("sudo make install")
    with cd("chipyard/software/local_firesim/switch"):
        run("make")
    with cd("chipyard/software/local_firesim/logs/"):
        run("mkdir " + current_run)
        run("rm -f recent")
        run("ln -s " + current_run + " recent")
    with cd("chipyard/tests-lnic"):
        run("make -j16")

def launch_switch(num_sims, current_run):
    with cd("chipyard/software/local_firesim/switch"):
        run("script -f -c \'sudo ./switch " + str(link_latency) + " " + str(switch_latency) + " 200 " + \
             str(high_priority_obuf) + " " + str(low_priority_obuf) + "\' ../logs/" + current_run + "switchlog > /dev/null")

def launch_sim(sim, current_run, test_name):
    time.sleep(1)
    with cd("chipyard/sims/firesim/sim/generated-src/f1/FireSim-DDR3FRFCFSLLC4MB_FireSimLNICSingleRocketConfig-F90MHz_BaseF1Config"):
        # TODO: The nic_mac_addr0 generation below is not generic enough!
        run("script -f -c \'sudo ./" + simulator +\
            " +permissive +vcs+initreg+0 +vcs+initmem+0 +fesvr-step-size=128 "\
            "+mm_relaxFunctionalModel=0 +mm_openPagePolicy=1 +mm_backendLatency=2 "\
            "+mm_schedulerWindowSize=8 +mm_transactionQueueDepth=8 +mm_dramTimings_tAL=0 "\
            "+mm_dramTimings_tCAS=14 +mm_dramTimings_tCMD=1 +mm_dramTimings_tCWD=10 "\
            "+mm_dramTimings_tCCD=4 +mm_dramTimings_tFAW=25 +mm_dramTimings_tRAS=33 "\
            "+mm_dramTimings_tREFI=7800 +mm_dramTimings_tRC=47 +mm_dramTimings_tRCD=14 "\
            "+mm_dramTimings_tRFC=160 +mm_dramTimings_tRRD=8 +mm_dramTimings_tRP=14 "\
            "+mm_dramTimings_tRTP=8 +mm_dramTimings_tRTRS=2 +mm_dramTimings_tWR=15 "\
            "+mm_dramTimings_tWTR=8 +mm_rowAddr_offset=18 +mm_rowAddr_mask=65535 "\
            "+mm_rankAddr_offset=16 +mm_rankAddr_mask=3 +mm_bankAddr_offset=13 "\
            "+mm_bankAddr_mask=7 +mm_llc_wayBits=3 +mm_llc_setBits=12 +mm_llc_blockBits=7 +mm_llc_activeMSHRs=8 "\
            "+shmemportname0=00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000%02d" % (sim + 2,) +\
            " +nic_mac_addr0=00:26:E1:00:00:0" + (hex(sim + 2))[2:].upper() + \
            " +switch_mac_addr0=08:55:66:77:88:08 +nic_ip_addr0=10.0.0." + str(sim + 2) + \
            " +timeout_cycles0=" + str(timeout_cycles) + " +rtt_pkts0=" + str(rtt_pkts) + \
            " +niclog0=../../../../../../software/local_firesim/logs/" + current_run + "niclog" + str(sim) + \
            " +linklatency0=" + str(link_latency) + \
            " +waveform=../../../../../../software/local_firesim/logs/" + current_run + "wavelog" + str(sim) + \
            " +netbw0=200 +netburst0=8 +tracefile0=TRACEFILE0 +blkdev-in-mem0=128 "\
            "+blkdev-log0=blkdev-log0 +autocounter-readrate0=1000 "\
            "+autocounter-filename0=AUTOCOUNTERFILE0 +dramsim +permissive-off " + \
            test_name + " 1 10.0.0." + str(sim + 2) + " " + load_gen_args + \
            " 3>&1 1>&2 2>&3 | ~/chipyard/riscv-tools-install/bin/spike-dasm > "\
            "../../../../../../software/local_firesim/logs/" + current_run + "simlog" + str(sim) + \
            "\' ../../../../../../software/local_firesim/logs/" + current_run + "uartlog" + str(sim) + \
            " > /dev/null")

def main():
    if len(sys.argv) < 3:
        print "This program requires passing in the number of simulations to run and the name of the test binary"
        exit(-1)
    num_sims = int(sys.argv[1])
    test_name = os.path.abspath(sys.argv[2])

    env.password = "vagrant"
    current_run = "local_firesim_" + str(datetime.datetime.now().strftime("%Y_%m_%d-%H_%M_%S")) + "/"

    # make sure the switch binary is re-compiled with the most recent switchconfig
    os.system('rm -f switch/switch')
    # use the correct switchconfig
    os.system('rm -f switch/switchconfig.h && ln -s switchconfig{}.h switch/switchconfig.h'.format(num_sims))

    execute(build_components, num_sims, current_run, hosts=["127.0.0.1"])

    local_addr_id_map = {}
    hosts = []
    hosts.append("127.0.0.1")
    local_addr_id_map["127.0.0.1"] = num_sims
    for i in range(num_sims):
        localhost_addr = "127.0.0." + str(i + 2)
        local_addr_id_map[localhost_addr] = i
        hosts.append(localhost_addr)


    @parallel
    def launch_wrapper(local_addr_id_map, current_run, test_name):
        if env.host_string == "127.0.0.1":
            launch_switch(local_addr_id_map[env.host_string], current_run)
        else:
            launch_sim(local_addr_id_map[env.host_string], current_run, test_name)
    execute(launch_wrapper, local_addr_id_map, current_run, test_name, hosts=hosts)




if __name__ == "__main__":
    main()
