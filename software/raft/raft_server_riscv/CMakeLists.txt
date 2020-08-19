cmake_minimum_required(VERSION 2.8)

project(raft_server_riscv)

add_executable(raft_server_riscv ../../../tests/crt.o ../../../tests/syscalls.o main.cc)
target_link_libraries(raft_server_riscv raft -T /home/vagrant/chipyard/tests/link.ld -nostartfiles -lgcc -mcmodel=medany)
