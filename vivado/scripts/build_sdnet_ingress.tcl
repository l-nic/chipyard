
# Build SDNet IP - Ingress Pipeline

set p4_file $::env(CHIPYARD_ROOT)/vivado/p4src/lnic_ingress.p4
set sdnet sdnet_ingress

source build_sdnet.tcl

