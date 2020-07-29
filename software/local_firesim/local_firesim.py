import sys
from fabric.api import *
import time
import datetime
import os

# Note that this doesn't rebuild the simulator by default, since that can be pretty slow even when nothing has changed.
def build_components(num_sims, current_run):
    with cd("chipyard/software/local_firesim/PcapPlusPlus"):
        run("make -j16")
        run("sudo make install")
    with cd("chipyard/software/local_firesim/switch"):
        run("make")
    with cd("chipyard/software/local_firesim/load_generator"):
        run("make")
    with cd("chipyard/software/local_firesim/logs/"):
        run("mkdir " + current_run)
        run("rm -f recent")
        run("ln -s " + current_run + " recent")
    with cd("chipyard/tests"):
        run("make -j16")

def launch_switch(num_sims, current_run, use_load_gen, distribution_type, test_type, poisson_lambda):
    if use_load_gen:
        with cd("chipyard/software/local_firesim/load_generator"):
            run("script -f -c \'sudo ./load_generator 147 10 200 8192 8192 " + distribution_type + " " + test_type + " " + poisson_lambda + "\' ../logs/" + current_run + "loadgenlog > /dev/null")
    else:
        with cd("chipyard/software/local_firesim/switch"):
            run("script -f -c \'sudo ./switch 147 10 200 8192 8182\' ../logs/" + current_run + "switchlog > /dev/null")

def launch_sim(sim, current_run, test_name):
    time.sleep(1)
    with cd("chipyard/sims/firesim/sim/generated-src/f1/FireSim-DDR3FRFCFSLLC4MB_FireSimLNICQuadRocketConfig-F90MHz_BaseF1Config"):
        run("script -f -c \'sudo ./VFireSim-debug +permissive +vcs+initreg+0 +vcs+initmem+0 +fesvr-step-size=128 +mm_relaxFunctionalModel=0 +mm_openPagePolicy=1 +mm_backendLatency=2 +mm_schedulerWindowSize=8 +mm_transactionQueueDepth=8 +mm_dramTimings_tAL=0 +mm_dramTimings_tCAS=14 +mm_dramTimings_tCMD=1 +mm_dramTimings_tCWD=10 +mm_dramTimings_tCCD=4 +mm_dramTimings_tFAW=25 +mm_dramTimings_tRAS=33 +mm_dramTimings_tREFI=7800 +mm_dramTimings_tRC=47 +mm_dramTimings_tRCD=14 +mm_dramTimings_tRFC=160 +mm_dramTimings_tRRD=8 +mm_dramTimings_tRP=14 +mm_dramTimings_tRTP=8 +mm_dramTimings_tRTRS=2 +mm_dramTimings_tWR=15 +mm_dramTimings_tWTR=8 +mm_rowAddr_offset=18 +mm_rowAddr_mask=65535 +mm_rankAddr_offset=16 +mm_rankAddr_mask=3 +mm_bankAddr_offset=13 +mm_bankAddr_mask=7 +mm_llc_wayBits=3 +mm_llc_setBits=12 +mm_llc_blockBits=7 +mm_llc_activeMSHRs=8 +shmemportname0=000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" + str(sim + 2) + " +nic_mac_addr0=00:26:E1:00:00:0" + str(sim + 2) + " +switch_mac_addr0=08:55:66:77:88:08 +nic_ip_addr0=10.0.0." + str(sim + 2) + "  +timeout_cycles0=30000  +rtt_pkts0=2 +niclog0=../../../../../../software/local_firesim/logs/" + current_run + "niclog" + str(sim) + " +linklatency0=147 +waveform=../../../../../../software/local_firesim/logs/" + current_run + "wavelog" + str(sim) + " +netbw0=200 +netburst0=8 +tracefile0=TRACEFILE0 +blkdev-in-mem0=128 +blkdev-log0=blkdev-log0 +autocounter-readrate0=1000 +autocounter-filename0=AUTOCOUNTERFILE0 +dramsim +permissive-off " + test_name + " 1 10.0.0." + str(sim + 2) + " 3>&1 1>&2 2>&3 | ~/chipyard/riscv-tools-install/bin/spike-dasm > ../../../../../../software/local_firesim/logs/" + current_run + "simlog" + str(sim) + "\' ../../../../../../software/local_firesim/logs/" + current_run + "uartlog" + str(sim) + " > /dev/null")

def main():
    if len(sys.argv) < 3:
        print "This program requires passing in the number of simulations to run and the name of the test binary"
        exit(-1)
    num_sims = int(sys.argv[1])
    test_name = os.path.abspath(sys.argv[2])
    use_load_gen = False
    distribution_type = None
    test_type = None
    poisson_lambda = None
    if len(sys.argv) >= 4:
        if sys.argv[3] == "load_gen":
            use_load_gen = True
            if len(sys.argv) < 7:
                print "The load generator requires passing in the number of simulations to run, the name of the test binary, the load_gen flag, the distribution type, the test type, and the mean time between generated requests (lambda)"
                exit(-1)
            distribution_type = sys.argv[4]
            test_type = sys.argv[5]
            poisson_lambda = sys.argv[6]

    env.password = "vagrant"
    current_run = "local_firesim_" + str(datetime.datetime.now().strftime("%Y_%m_%d-%H_%M_%S")) + "/"

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
    def launch_wrapper(local_addr_id_map, current_run, test_name, use_load_gen, distribution_type, test_type, poisson_lambda):
        if env.host_string == "127.0.0.1":
            launch_switch(local_addr_id_map[env.host_string], current_run, use_load_gen, distribution_type, test_type, poisson_lambda)
        else:
            launch_sim(local_addr_id_map[env.host_string], current_run, test_name)
    execute(launch_wrapper, local_addr_id_map, current_run, test_name, use_load_gen, distribution_type, test_type, poisson_lambda, hosts=hosts)




if __name__ == "__main__":
    main()