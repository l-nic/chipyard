#!/bin/bash

sed -i 's/xil_defaultlib/sdnet_ndp_ingress_lib/g' ../ip/ip_user_files/sim_scripts/sdnet_ndp_ingress/vcs/sdnet_ndp_ingress.sh
sed -i 's/xil_defaultlib/sdnet_ndp_egress_lib/g' ../ip/ip_user_files/sim_scripts/sdnet_ndp_egress/vcs/sdnet_ndp_egress.sh
