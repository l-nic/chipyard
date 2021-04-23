
# Build SDNet IP - Homa Ingress Pipeline

set p4_file $::env(CHIPYARD_ROOT)/vivado/p4src/homa_ingress.p4
set sdnet sdnet_homa_ingress

source build_sdnet.tcl

