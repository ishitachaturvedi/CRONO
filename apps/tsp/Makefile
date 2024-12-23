
TRGS = tsp_work_stealing

LIBS += -lpthread -lrt -static

CXXFLAGS = -g --std=c++0x -O3
CXXFLAGS += -Wall -Werror

all: $(TRGS)

tsp: tsp.cc

clean:
	rm -f $(TRGS) *.o *.dot

%: %.cc ;
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS) -I/home/ishita/gem5-GPU_killer/include -L/home/ishita/gem5-GPU_killer/util/m5/build/arm64/out -lm5
