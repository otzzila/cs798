GPP = g++-9
FLAGS = -O3 -g -mrtm
FLAGS += -I../common
FLAGS += -std=c++2a -fconcepts
FLAGS += -fopenmp
LDFLAGS = -L. -pthread -ltle

all: benchmark benchmark_debug

.PHONY: benchmark
benchmark:
	$(GPP) $(FLAGS) -o $@ $@.cpp $(LDFLAGS) -DNDEBUG #### NOTE: THIS DISABLES ASSERTIONS!!!

.PHONY: benchmark_debug
benchmark_debug:
	$(GPP) $(FLAGS) -o $@ benchmark.cpp -DTRACE=if\(1\) -fsanitize=address -static-libasan $(LDFLAGS)

clean:
	rm -f *.out
