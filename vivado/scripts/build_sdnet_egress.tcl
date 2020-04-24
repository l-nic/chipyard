
# Build SDNet IP - Egress Pipeline

set p4_file $::env(CHIPYARD_ROOT)/vivado/p4src/lnic_egress.p4
set sdnet sdnet_egress

source build_sdnet.tcl

