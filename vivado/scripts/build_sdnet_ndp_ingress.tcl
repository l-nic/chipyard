
# Build SDNet IP - NDP Ingress Pipeline

set p4_file $::env(CHIPYARD_ROOT)/vivado/p4src/ndp_ingress.p4
set sdnet sdnet_ndp_ingress

source build_sdnet.tcl

