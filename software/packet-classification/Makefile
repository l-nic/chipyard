CXX=riscv64-unknown-elf-g++
CXXFLAGS=-std=c++11 -Wall -fPIC -DNOPYTHON -DNO_RQRMI_OPT
CXXFLAGS+=-mcmodel=medany -I. -O2 -ffixed-x30 -ffixed-x31 -fno-common -fno-builtin-printf -fno-use-cxa-atexit
CXXFLAGS+=-I nuevomatch/include -I nuevomatch/vendor -I ../../tests
LDFLAGS=-T ../../tests/link.ld -static -nostartfiles -lgcc

.SUFFIXES:

nuevomatch-cutsplit.riscv: nuevomatch-cutsplit.cpp \
    ../../tests/crt.o ../../tests/syscalls.o \
    nuevomatch/bin/vector_list.o nuevomatch/bin/rule_db.o nuevomatch/bin/simd_aux.o \
    nuevomatch/bin/matrix_operations.o nuevomatch/bin/rqrmi_model.o nuevomatch/bin/interval_set.o \
    nuevomatch/bin/logging.o nuevomatch/bin/serial_nuevomatch.o nuevomatch/bin/rqrmi_fast.o \
    nuevomatch/bin/object_io.o nuevomatch/bin/cut_split.o nuevomatch/bin/hyper_split.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

clean:
	rm -f nuevomatch-cutsplit.riscv

all: nuevomatch-cutsplit.riscv
