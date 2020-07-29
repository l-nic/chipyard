include /usr/local/etc/PcapPlusPlus.mk

all: load_generator

load_generator: load_generator.cpp baseport.h shmemport.h flit.h socketport.h switchconfig.h lnic_layer.cpp lnic_layer.h app_layer.cpp app_layer.h
	g++ -g3 -O3 -std=gnu++11 $(PCAPPP_BUILD_FLAGS) $(PCAPPP_INCLUDES) -o load_generator load_generator.cpp lnic_layer.cpp app_layer.cpp -fopenmp -lrt -l:libPcap++.a -l:libPacket++.a -l:libCommon++.a


sim:
	python emitconfig.py

run_load_generator:
	echo "removing old /dev/shm/*"
	rm -rf /dev/shm/*
	./load_generator

clean:
	rm -rf load_generator*-build/
	rm -rf /dev/shm/*
