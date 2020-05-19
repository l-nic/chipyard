Initial Repository Setup
========================================================

Requirements
-------------------------------------------

Chipyard is developed and tested on Linux-based systems.

.. Warning:: It is possible to use this on macOS or other BSD-based systems, although GNU tools will need to be installed; it is also recommended to install the RISC-V toolchain from ``brew``.

.. Warning:: Working under Windows is not recommended.


In CentOS-based platforms, we recommend installing the following dependencies:

.. include:: /../scripts/centos-req.sh
   :code: bash

In Ubuntu/Debian-based platforms (Ubuntu), we recommend installing the following dependencies:

.. include:: /../scripts/ubuntu-req.sh
   :code: bash

.. Note:: When running on an Amazon Web Services EC2 FPGA-development instance (for FireSim), FireSim includes a machine setup script that will install all of the aforementioned dependencies (and some additional ones).

Checking out the sources
------------------------

After cloning this repo, you will need to initialize all of the submodules.

.. code-block:: shell

    git clone https://github.com/ucb-bar/chipyard.git
    cd chipyard
    ./scripts/init-submodules-no-riscv-tools.sh

When updating Chipyard to a new version, you will also want to rerun this script to update the submodules.
Using git directly will try to initialize all submodules; this is not recommended unless you expressly desire this behavior.

.. _build-toolchains:

Building a Toolchain
------------------------

The `toolchains` directory contains toolchains that include a cross-compiler toolchain, frontend server, and proxy kernel, which you will need in order to compile code to RISC-V instructions and run them on your design.
Currently there are two toolchains, one for normal RISC-V programs, and another for Hwacha (``esp-tools``).
For custom installations, Each tool within the toolchains contains individual installation procedures within its README file.
To get a basic installation (which is the only thing needed for most Chipyard use-cases), just the following steps are necessary.

.. code-block:: shell

    ./scripts/build-toolchains.sh riscv-tools # for a normal risc-v toolchain

    # OR

    ./scripts/build-toolchains.sh esp-tools # for a modified risc-v toolchain with Hwacha vector instructions

Once the script is run, a ``env.sh`` file is emitted that sets the ``PATH``, ``RISCV``, and ``LD_LIBRARY_PATH`` environment variables.
You can put this in your ``.bashrc`` or equivalent environment setup file to get the proper variables.
These variables need to be set for the ``make`` system to work properly.
