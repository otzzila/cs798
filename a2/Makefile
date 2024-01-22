ARGS=-O3 -pthread -g -std=c++2a

all: workload_timed

workload_%:
	g++-9 $@.cpp -o $@.out $(ARGS)
