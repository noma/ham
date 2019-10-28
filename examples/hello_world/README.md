# HAM-Offlad Example: Hello World

This folder contains a most simple HAM-Offload example project with a CMakeLists.txt file for building the exectuable using different target architectures, i.e. HAM-Offload communication backends.

HAM-Offload is included via a CMake add_subdirectory() command. This assumes a copy of the "ham"-directory from the ham repository inside the "hellow-world"-directory. We use a symbolic link here. **If you copy this project and use it as blueprint, replace this link with a copy or modified link.**

## Building and Running

You can either create a build directy where this README.md file resides, or do a out-of-source build. The latter would look as follows:#

### MPI

Using MPI as a backend should be straight forward.

```terminal
cd ..
mkdir build.hello_world
cd build.hello_world
cmake ../hello_world
make -j
```

The program is started like any other MPI program. Rank 0 ist the host, all other ranks act as offload targets:

```terminal
mpirun -n 2 ./hello_world_mpi
```

Exptected Output:
```
Calling hello(1337) on target 1
	Hello from target 1, got: 1337
hello() returned: 42
```

### NEC Vector Engine

Building requires the NEC compiler and software environment.

There are two back-ends for the NEC VE, where "VEO+VEDMA" is the faster one and "VEO-only" acts as a reference. **The NEC VE backend requires two builds**, one for the Vector Engine (VE) and one for the Vector Host (VH), i.e. we need to create two build folders with CMake.

```terminal
cd ..
```

**Vector Host:** any C++ compiler should do:
```terminal
mkdir build.hello_world.vh
cd build.hello_world.vh
cmake ../hello_world
make -j
```

**Vector Engine:** we need to use the NEC compiler:
```terminal
mkdir build.hello_world.ve
cd build.hello_world.ve
CC=`which ncc` CXX=`which nc++` cmake -DCMAKE_CXX_COMPILE_FEATURES='cxx_auto_type;cxx_range_for;cxx_variadic_templates' ../cmake ../hello_world
make -j
```

We start the host executable, and pass the binary generated for the Vector Engine as command line argument (using 2 processes in total, and the 0th Vector Engine:

```terminal
cd ..
build.hello_world.vh/hello_world_vedma_vh --ham-process-count 2 --ham-veo-ve-nodes 0 --ham-veo-ve-lib build.hello_world.ve/veorun_hello_world_vedma
```

