#########################################################################################
# vcs makefile
#########################################################################################

#########################################################################################
# general path variables
#########################################################################################
base_dir=$(abspath ../..)
sim_dir=$(abspath .)

#########################################################################################
# include shared variables
#########################################################################################
include $(base_dir)/variables.mk

#########################################################################################
# name of simulator (used to generate *.f arguments file)
#########################################################################################
sim_name = vcs

#########################################################################################
# vcs simulator types and rules
#########################################################################################
sim_prefix = simv
sim = $(sim_dir)/$(sim_prefix)-$(MODEL_PACKAGE)-$(CONFIG)
sim_debug = $(sim_dir)/$(sim_prefix)-$(MODEL_PACKAGE)-$(CONFIG)-debug

PERMISSIVE_ON=+permissive
PERMISSIVE_OFF=+permissive-off

WAVEFORM_FLAG=+vcdplusfile=$(sim_out_name).vpd

.PHONY: default debug
default: $(sim)
debug: $(sim_debug)

#########################################################################################
# import other necessary rules and variables
#########################################################################################
include $(base_dir)/common.mk

#########################################################################################
# vcs binary and arguments
#########################################################################################
VLOGAN = vlogan -full64
VCS = vcs -full64

sim_vsrcs += /home/vagrant/chipyard/vivado/vsrc/SDNetWrapper.sv

VLOGAN_OPTS = \
	+lint=all,noVCDE,noONGS,noUI \
	-error=PCWM-L \
	-timescale=1ns/10ps \
	-quiet \
	-q \
	+rad \
	+v2k \
	+vcs+lic+wait \
	+vc+list \
	-f $(sim_common_files) \
	-sverilog \
	+incdir+$(build_dir) \
	+define+CLOCK_PERIOD=1.0 \
	$(sim_vsrcs) \
	+define+PRINTF_COND=$(TB).printf_cond \
	+define+STOP_COND=!$(TB).reset \
	+define+RANDOMIZE_MEM_INIT \
	+define+RANDOMIZE_REG_INIT \
	+define+RANDOMIZE_GARBAGE_ASSIGN \
	+define+RANDOMIZE_INVALID_ASSIGN \
	+libext+.v

#VCS_CC_OPTS = \
#	-CC "-I$(VCS_HOME)/include" \
#	-CC "-I$(RISCV)/include" \
#	-CC "-std=c++11" \
#	$(RISCV)/lib/libfesvr.a

#VCS_OPTS = -notice -line 
VCS_OPTS = 

#########################################################################################
# vcs simulator rules
#########################################################################################
$(sim): riscv_lib riscv_dpi.so
	rm -rf csrc && $(VCS) $(VCS_OPTS) riscv_lib.TestDriver -o $@ \
	-debug_pp

$(sim_debug): riscv_lib riscv_dpi.so
	rm -rf csrc && $(VCS) $(VCS_OPTS) riscv_lib.TestDriver -o $@ \
	+define+DEBUG -debug_pp

riscv_lib: $(sim_vsrcs) $(sim_common_files)
	rm -rf riscv_lib && $(VLOGAN) $(VLOGAN_OPTS) -work riscv_lib

riscv_dpi.so: $(sim_common_files)
	g++ -std=c++11 -fPIC -Wall -I$(VCS_HOME)/include -I$(RISCV)/include\
		/home/vagrant/chipyard/sims/vcs/generated-src/example.TestHarness.SimNetworkLNICGPRConfig/device.cc \
		/home/vagrant/chipyard/sims/vcs/generated-src/example.TestHarness.SimNetworkLNICGPRConfig/remote_bitbang.cc \
		/home/vagrant/chipyard/sims/vcs/generated-src/example.TestHarness.SimNetworkLNICGPRConfig/SimDTM.cc \
		/home/vagrant/chipyard/sims/vcs/generated-src/example.TestHarness.SimNetworkLNICGPRConfig/SimJTAG.cc \
		/home/vagrant/chipyard/sims/vcs/generated-src/example.TestHarness.SimNetworkLNICGPRConfig/SimNetwork.cc \
		/home/vagrant/chipyard/sims/vcs/generated-src/example.TestHarness.SimNetworkLNICGPRConfig/SimSerial.cc \
		/home/vagrant/chipyard/sims/vcs/generated-src/example.TestHarness.SimNetworkLNICGPRConfig/switch.cc \
		-shared -o riscv_dpi.so -Wl,--whole-archive $(RISCV)/lib/libfesvr.a -Wl,--no-whole-archive


#########################################################################################
# create a vcs vpd rule
#########################################################################################
.PRECIOUS: $(output_dir)/%.vpd %.vpd
$(output_dir)/%.vpd: $(output_dir)/% $(sim_debug)
	(set -o pipefail && $(sim_debug) $(PERMISSIVE_ON) +max-cycles=$(timeout_cycles) $(SIM_FLAGS) $(VERBOSE_FLAGS) +vcdplusfile=$@ $(PERMISSIVE_OFF) $< 3>&1 1>&2 2>&3 | spike-dasm > $<.out)

#########################################################################################
# general cleanup rule
#########################################################################################
.PHONY: clean
clean:
	rm -rf $(gen_dir) csrc $(sim_prefix)-* ucli.key vc_hdrs.h *.log
