In a new vm, run:

# Get the new submodules
cd ~/chipyard
git checkout lnic-arg-passing
git submodule update --init --recursive software/ray-tracing
# Make sure all other submodules are up to date too, using "git status"

# Install a modern cmake, along with other ray tracer dependencies
sudo yum install cmake3
sudo ym install lz4-devel

# Build and install a modern gcc
cd ~
mkdir tool_installs
cd tool_installs
wget https://ftp.gnu.org/gnu/gcc/gcc-9.2.0/gcc-9.2.0.tar.gz
tar -xzf gcc-9.2.0.tar.gz
cd gcc-9.2.0
./contrib/download_prerequisites
cd ..
mkdir gcc_objdir
cd gcc_objdir
../gcc-9.2.0/configure --prefix=/usr --enable-languages=c,c++ --disable-multilib
make -j16
sudo make install

# Build and install a modern openssl
cd ~/tool_installs
wget https://www.openssl.org/source/openssl-1.1.1f.tar.gz
tar -xzf openssl-1.1.1f.tar.gz
cd openssl-1.1.1f
./config --prefix=/usr/local/ssl --openssldir=/usr/local/ssl no-shared zlib
make -j16
sudo make install
# Open /etc/ld.so.conf.d/openssl-1.1.1d.conf and add "/usr/local/ssl/lib"
sudo ldconfig
# Open ~/.bashrc and add "export OPENSSL_PATH=/usr/local/ssl/bin" and "export PATH=$PATH:$OPENSSL_PATH" and "export OPENSSL_ROOT_DIR=/usr/local/ssl"
cd /usr/local/include
sudo ln -s ../ssl/include/openssl .
source ~/.bashrc

# Build host protobuf (needed for cross compile)
cd ~/chipyard/software/ray-tracing
cd protobuf-tools/host-protobuf
./autogen.sh
./configure --prefix=/usr
make -j16
sudo make install
sudo ldconfig

# Build host ray tracer driver. Driver binary will be in build directory with name "split-rays". Optional host-side ray tracer is also built in "ray-client".
cd ../../host-driver
mkdir build
cd build
cmake3 ..
make -j16

# Generate scene bvh
cd ..
mkdir treelets
cd build
./lib/pbrt/pbrt --dumpscene ../treelets/ --nomaterial ../lib/pbrt/scenes/killeroo-dump.pbrt

# Build spike
cd ~/chipyard/toolchains/riscv-tools/riscv-isa-sim/build
make -j16
make install

# Build riscv protobuf
cd ../riscv-protobuf
./autogen.sh
./configure --prefix=/home/vagrant/chipyard/riscv-tools-install/riscv64-unknown-elf --host=riscv64-unknown-elf CC=riscv64-unknown-elf-gcc CXX=riscv64-unknown-elf-g++ CXXFLAGS='-std=c++17 -mcmodel=medany -fno-builtin-printf' --with-protoc=/usr/bin/protoc
make -j16
make install
sudo ldconfig

# Rebuild riscv libm with the patched changes.
cd ~/chipyard/toolchains/riscv-tools/riscv-gnu-toolchain/build
make -j16
make install

# Or, to just update changes in newlib (our version of libc) go to ~/chipyard/toolchains/riscv-tools/riscv-gnu-toolchain/build/build-newlib, and run
make -j16
make install

# Build riscv ray tracer. Ray tracer binary will be in build directory with name "nanopu-ray-client".
cd ../../riscv-tracer
mkdir build
cd build
cmake -DCMAKE_CHOOSE_CROSS=riscv -DCMAKE_TOOLCHAIN_FILE=../riscv.cmake ..
make -j16

# Run the host driver
cd ~/chipyard/software/ray-tracing/host-driver/build
./split-rays ../treelets/ 1

# Open four separate terminals, and in each, run the following, where N is a number from 0 to 3:
cd ~/chipyard/software/ray-tracing/riscv-tracer/build
spike --treelet_id=N -m8192 nanopu-ray-client a 1 N 4

# The output file will be saved as killeroo-dump.png in ~/chipyard/software/ray-tracing/host-driver/build, and will be updated once every 10,000 samples.
