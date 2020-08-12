To build the chipyard submodules (after configuring other dependencies and running git submodule update --init --recursive from within each of these submodules):

# Build host protobuf (needed for cross compile)
cd ~/chipyard/software/ray-tracing
cd protobuf-tools/host-protobuf
./autogen.sh
./configure --prefix=/usr
make -j16
sudo make install
sudo ldconfig

# Build riscv protobuf
cd ../riscv-protobuf
./autogen.sh
./configure --prefix=/home/USERNAME/chipyard/riscv-tools-install/riscv64-unknown-elf --host=riscv64-unknown-elf CC=riscv64-unknown-elf-gcc CXX=riscv64-unknown-elf-g++ CXXFLAGS=-std=c++17 --with-protoc=/usr/bin/protoc
make -j16
make install
sudo ldconfig

# Build host ray tracer driver. Driver binary will be in build directory with name "split-rays". Optional host-side ray tracer is also built in "ray-client".
cd ../../host-driver
mkdir build
cd build
cmake ..
make -j16

# Generate scene bvh
cd ..
mkdir treelets
cd build
./lib/pbrt/pbrt --dumpscene ../treelets/ --nomaterial ../lib/pbrt/scenes/killeroo-dump.pbrt

# Rebuild riscv libm with the patched changes.
cd ~/chipyard/toolchains/riscv-tools/riscv-gnu-toolchain/build
make -j16
make install

# Build riscv ray tracer. Ray tracer binary will be in build directory with name "riscv-ray-client".
cd ../../riscv-tracer
mkdir build
cd build
cmake -DCMAKE_CHOOSE_CROSS=riscv -DCMAKE_TOOLCHAIN_FILE=../riscv.cmake ..
make -j16

# Build spike
cd ~/chipyard/toolchains/riscv-tools/riscv-isa-sim/build
make -j16
make install

# Run the host driver
cd ~/chipyard/software/ray-tracing/host-driver/build
./split-rays ../treelets/ 1

# Open four separate terminals, and in each, run the following, where N is a number from 0 to 3:
cd ~/chipyard/software/ray-tracing/riscv-tracer/build
spike --treelet_id=N pk riscv-ray-client a 1 N 4

# The output file will be saved as killeroo-dump.png in ~/chipyard/software/ray-tracing/host-driver/build, and will be updated once every 10,000 samples.
