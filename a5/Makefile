GPP = g++-9
FLAGS = -O3 -g -std=c++2a
#FLAGS += -DNDEBUG
#FLAGS += -DTRACE=if\(1\) -DUSE_TRACE
LDFLAGS = -pthread

PROGRAMS = benchmark benchmark_sanitize

all: $(PROGRAMS)

build:
	mkdir -p $@
	cp -f libjemalloc.so $@/.

benchmark: build
	$(GPP) $(FLAGS) -MMD -MP -MF build/$@.d -o $@ $@.cpp $(LDFLAGS)

-include $(addprefix build/,$(addsuffix .d, $(PROGRAMS)))

benchmark_sanitize:
	$(GPP) $(FLAGS) -MMD -MP -MF build/$@.d -o $@ benchmark.cpp $(LDFLAGS) -fsanitize=address -static-libasan
	
-include $(addprefix build/,$(addsuffix .d, $(PROGRAMS)))

clean:
	rm -rf $(PROGRAMS) build


