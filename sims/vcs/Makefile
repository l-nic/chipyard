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
	-error=noZMMCM \
	-assert svaext \
	-sverilog \
	+libext+.v \
	+incdir+$(build_dir) \
	-f $(sim_common_files) \
	$(sim_vsrcs)

VCS_DEFINES = \
	+define+VCS \
	+define+CLOCK_PERIOD=1.0 \
	+define+PRINTF_COND=$(TB).printf_cond \
	+define+STOP_COND=!$(TB).reset \
	+define+RANDOMIZE_MEM_INIT \
	+define+RANDOMIZE_REG_INIT \
	+define+RANDOMIZE_GARBAGE_ASSIGN \
	+define+RANDOMIZE_INVALID_ASSIGN

VCS_OPTS = -notice -line

#########################################################################################
# vcs simulator rules
#########################################################################################
$(sim): $(sim_vsrcs) $(sim_common_files) riscv_dpi.so
	rm -rf riscv_lib && $(VLOGAN) $(VLOGAN_OPTS) $(VCS_DEFINES) -work riscv_lib
	rm -rf csrc && $(VCS) $(VCS_OPTS) riscv_lib.TestDriver -o $@ -debug_access+designer

$(sim_debug): $(sim_vsrcs) $(sim_common_files) riscv_dpi.so
	rm -rf riscv_lib && $(VLOGAN) $(VLOGAN_OPTS) $(VCS_DEFINES) +define+DEBUG -work riscv_lib
	rm -rf csrc && $(VCS) $(VCS_OPTS) riscv_lib.TestDriver -o $@ -debug_access+designer

riscv_dpi.so: $(sim_common_files)
	g++ -std=c++11 -fPIC -Wall -I$(VCS_HOME)/include -I$(RISCV)/include -I$(dramsim_dir) \
		$(build_dir)/*.cc \
		-shared -o riscv_dpi.so -Wl,--whole-archive $(RISCV)/lib/libfesvr.a -Wl,--no-whole-archive

#########################################################################################
# create a vcs vpd rule
#########################################################################################
.PRECIOUS: $(output_dir)/%.vpd %.vpd
$(output_dir)/%.vpd: $(output_dir)/% $(sim_debug)
	(set -o pipefail && $(sim_debug) $(PERMISSIVE_ON) +max-cycles=$(timeout_cycles) $(SIM_FLAGS) $(VERBOSE_FLAGS) +vcdplusfile=$@ $(PERMISSIVE_OFF) $< </dev/null 2> >(spike-dasm > $<.out) | tee $<.log)

#########################################################################################
# general cleanup rule
#########################################################################################
.PHONY: clean
clean:
	rm -rf $(gen_dir) riscv_lib *.so csrc 64/ $(sim_prefix)-* ucli.key vc_hdrs.h *.log
