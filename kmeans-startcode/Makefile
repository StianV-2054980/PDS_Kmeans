# -std=c++14: we're limiting ourselves to c++14, since that's what the 
#             GCC compiler on the VSC supports.
# -DNDEBUG: turns off e.g. assertion checks
# -O3: enables optimizations in the compiler

# Settings for optimized build
FLAGS= -O3 -DNDEBUG -std=c++11

# Settings for a debug build
#FLAGS=-g -std=c++14

all: kmeans

clean:
	rm -f kmeans

kmeans: main_startcode.cu rng.cpp
	nvcc $(FLAGS) -gencode arch=compute_80,code=sm_80 -o kmeans main_startcode.cu rng.cpp
