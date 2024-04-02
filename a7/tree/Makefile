GPP = g++-9
FLAGS = -O3 -g -mrtm -std=c++2a -fconcepts
FLAGS += -I../common -Ibronson_pext_bst_occ/common -Ibronson_pext_bst_occ/common/recordmgr
#FLAGS += -DNDEBUG
LDFLAGS = -pthread

PROGRAMS = benchmark

all: $(PROGRAMS)

build:
	mkdir -p $@

benchmark: build
	$(GPP) $(FLAGS) -MMD -MP -MF build/$@.d -o $@ $@.cpp $(LDFLAGS)


-include $(addprefix build/,$(addsuffix .d, $(PROGRAMS)))

clean:
	rm -rf $(PROGRAMS) build


