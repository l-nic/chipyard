GCC=riscv64-unknown-elf-gcc
OBJDUMP=riscv64-unknown-elf-objdump
CFLAGS= -std=gnu99 -O2 -fno-common -fno-builtin-printf -Wall -ffixed-x30 -ffixed-x31
LDFLAGS=-static -nostdlib -nostartfiles -lgcc

# TODO(sibanez): do I need this here? What is it?
include libgloss.mk

PROGRAMS = hello pwm blkdev accum charcount nic-loopback big-blkdev pingd
PROGRAMS += lnic-nic-loopback-gpr
PROGRAMS += lnic-nic-loopback-csr
PROGRAMS += icenic-nic-loopback
PROGRAMS += lnic-cpu-loopback-gpr
PROGRAMS += lnic-cpu-loopback-csr
PROGRAMS += icenic-cpu-loopback
PROGRAMS += simple-icenic-cpu-loopback
PROGRAMS += lnic-throughput-gpr
PROGRAMS += icenic-throughput
PROGRAMS += lnic-nn-inference-gpr
PROGRAMS += icenic-nn-inference
PROGRAMS += lnic-othello-gpr
PROGRAMS += lnic-scheduler
PROGRAMS += icenic-othello
PROGRAMS += lnic-stream-gpr
PROGRAMS += icenic-stream
PROGRAMS += lnic-multi-thread-basic
PROGRAMS += lnic-nbody-node-gpr
PROGRAMS += icenic-nbody-node
PROGRAMS += lnic-send-recv

.DEFAULT_GOAL := default

.PHONY: default
default: $(addsuffix .riscv,$(PROGRAMS))

.PHONY: dumps
dumps: $(addsuffix .dump,$(PROGRAMS))

%.o: %.S
	$(GCC) $(CFLAGS) -D__ASSEMBLY__=1 -c $< -o $@

%.o: %.c mmio.h
	$(GCC) $(CFLAGS) -c $< -o $@

%.riscv: %.o $(libgloss)
	$(GCC) $(LDFLAGS) $< -o $@

%.dump: %.riscv
	$(OBJDUMP) -D $< > $@


.PHONY: clean
clean:
	rm -f *.riscv *.o *.dump
	$(if $(libgloss),rm -rf $(libgloss_builddir)/)
