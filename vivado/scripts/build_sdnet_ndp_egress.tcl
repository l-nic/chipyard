
# Build SDNet IP - NDP Egress Pipeline

set p4_file $::env(CHIPYARD_ROOT)/vivado/p4src/ndp_egress.p4
set sdnet sdnet_ndp_egress

source build_sdnet.tcl

