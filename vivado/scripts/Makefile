
IP_DIR=../ip/ip_user_files/sim_scripts

all: ndp_ingress ndp_egress

ndp_ingress:
	vivado -nolog -nojournal -mode batch -source ./build_sdnet_ndp_ingress.tcl
	sed -i 's/xil_defaultlib/sdnet_ndp_ingress_lib/g' ${IP_DIR}/sdnet_ndp_ingress/vcs/sdnet_ndp_ingress.sh
	pushd ${IP_DIR}/sdnet_ndp_ingress/vcs && ./sdnet_ndp_ingress.sh && popd

ndp_egress:
	vivado -nolog -nojournal -mode batch -source ./build_sdnet_ndp_egress.tcl
	sed -i 's/xil_defaultlib/sdnet_ndp_egress_lib/g' ${IP_DIR}/sdnet_ndp_egress/vcs/sdnet_ndp_egress.sh
	pushd ${IP_DIR}/sdnet_ndp_egress/vcs && ./sdnet_ndp_egress.sh && popd

