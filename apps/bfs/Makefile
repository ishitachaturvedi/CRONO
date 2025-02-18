
TRGS = bfs bfs_atomic bfs_work_stealing_copy  bfs_work_stealing

LIBS +=  -static -lpthread 

CXXFLAGS = -g -O3
CXXFLAGS += -Wall

all: $(TRGS)

bfs: bfs.cc
bfs_atomic: bfs_atomic.cc

clean:
	rm -f $(TRGS) *.o *.dot
	rm -f myfile.txt

%: %.cc ;
	$(CXX) $(CXXFLAGS) $< -o $@  $(LIBS) -I/home/ishita/gem5-GPU_killer/include -L/home/ishita/gem5-GPU_killer/util/m5/build/arm64/out -lm5
