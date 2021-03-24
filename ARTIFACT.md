
# nanoPU Artifact

## Overview

This is the top level repository for the nanoPU source code.

## AWS Setup Instructions

These instructions are adapted from the [Firesim documentation](https://docs.fires.im/en/latest/Initial-Setup/index.html).

## Running the Simulations

### Verilator Simulations

Compile RISC-V test programs:
```
$ cd ~/chipyard/tests-lnic
$ make
$ cd ~/chipyard/tests-icenic
$ make
```

Compile nanoPU (i.e., L-NIC) and Traditional (i.e., IceNIC) Verilator simulation models:
```
$ cd ~/chipyard/sims/verilator
$ make debug CONFIG=LNICSimNetworkQuadRocketConfig -j16
$ make debug CONFIG=IceNICSimNetworkRocketConfig TOP=TopIceNIC MODEL=TestHarnessIceNIC VLOG_MODEL=TestHarnessIceNIC -j16
```

Run all the simulations:
```
$ cd ~/chipyard/software/net-app-tester/
$ sudo bash
# ./run_verilator_sims.py --all
```

Use juypter notebook to generate and view the results.
Start jupyter notebook:
```
$ cd ~
$ jupyter notebook
```

Forward connection to port 8888 over SSH so we can connect to jupyter notebook:
```
$ ssh -i ~/.ssh/firesim.pem -L 8888:localhost:8888 centos@<instance-IP>
```

Visit: http://localhost:8888 in your browser.

Open the `Verilator-Evals.ipynb` notebook located in the `chipyard/software/net-app-tester/` directory.

Select the option to clear and run all cells.

At this point, you should be able to view all the plots created from the Verlator simulation results.

### Firesim Simulations

