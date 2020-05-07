#!/bin/bash

sed -i 's/xil_defaultlib/sdnet_ingress_lib/g' ../ip/ip_user_files/sim_scripts/sdnet_ingress/vcs/sdnet_ingress.sh
sed -i 's/xil_defaultlib/sdnet_egress_lib/g' ../ip/ip_user_files/sim_scripts/sdnet_egress/vcs/sdnet_egress.sh
