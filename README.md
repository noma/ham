# HAM - Heterogeneous Active Messages

[![Licence](https://img.shields.io/badge/license-BSL-blue.svg?style=flat)](LICENSE_1_0.txt)

|          | master | devel |
|----------|--------|-------|
| Build    | [![Build Status](https://travis-ci.org/noma/ham.svg?branch=master)](https://travis-ci.org/noma/ham) | [![Build Status](https://travis-ci.org/noma/ham.svg?branch=devel)](https://travis-ci.org/noma/ham) |
| Coverity | [![Coverity Scan Build Status](https://scan.coverity.com/projects/8451/badge.svg)](https://scan.coverity.com/projects/noma-ham) | NA |


HAM (Heterogeneous Active Messages) is a research project on efficient active
messaging between heterogeneous binary files and architectures. Its original
motivation is to efficiently offload computations to accelerators like the
**Intel Xeon Phi**, or nodes of a distributed system in general. Efficiently
means with a minimal amount of overhead. An additional goal was to implement
this functionality in pure C++ without introducing yet another language or
compiler extension.

The outcome of this project are two libraries:
- **HAM**, a Heterogeneous Active Messages C++ library
- **HAM-Offload**, an implementation of the offload programming model based on
  HAM with support for the Xeon Phi

The code originates from different research prototypes and branches whose
features were integrated into a coherent code-base that resides in this
repository. The source under active development and should be ready to be used
in real-world applications. So don't hesitate to give it a try - and don't
forget to provide some feedback, like bug reports, issues, feature requests,
or performance results.

There's a paper available, including a case study with a real world
application, presented at SC'14:
- _A Unified Programming Model for Intra- and Inter-Node Offloading
  on Xeon Phi Clusters_, http://dl.acm.org/citation.cfm?id=2683616


## Why should I use HAM or HAM-Offload?

If you are looking for a way to efficiently create and exchange (heterogeneous)
active message in a distributed or parallel system using C++, **HAM** is your
friend.

The **HAM** library allows to create active messages that can be transferred
between heterogeneous binary files. Heterogeneous here means, compiled with
different options and possibly for different architectures, but usually from
the same source. The latter can be stretched to sources that at least share the
relevant parts (everything connected to the creation and execution of active
messages). Different architectures means _not too different_ with respect to
the used C++ ABI. HAM performs efficient and transparent code address
translation for active message handlers and provides the hooks to add pre- and
post-processing for transferred objects on a per-type basis as needed. Please
refer to the documentation and publications for more details.
Currently, HAM was successfully tested and used with the _x86-64_, _k1om_
(Intel MIC), and _ARM aarch64_ architectures.

If you want to use the offload programming model for the Xeon Phi, have a look
at **HAM-Offload**.

**HAM-Offload** uses HAM and combines it with a communication layer to
implement an offload programming model. The HAM-Offload API provides similar
functionality as vendor provided solutions like Intel LEO (Language Extensions
for Offload) or the offload features of OpenMP 4.0. In contrast to these
solutions, HAM-Offload is pure C++ 11 instead of a language extension based on
pragma directives. This means no additional tools or compiler-dependencies are
introduced. HAM-Offload supports _remote offloading_, _reverse offloading_, and
_direct data transfers_ between offload targets, each of which are currently
not possible with LEO or OpenMP (Update: LEO supports remote offload over
Mellanox InfiniBand and Intel OmniPath since Intel Compiler version 17).
In general, **arbitrary offload patterns** within a distributed system are
possible. Any host or accelerator can be the _logical host_  in terms of the
offload programming model, and any set of hosts and accelerators can act as
offload targets. Due to the drastically reduced offload overhead, *fine-grained
offloading* becomes affordable. Currently, there are two communication
back-ends, one using SCIF (intra-node) and one using MPI (intra- and
inter-node). Additional implementations based on MPI-3.1 one-sided
communication are work in progress.

## Example

A simple example that demonstrates the HAM-Offload API can be found in
[src/inner\_product_cpp](src/inner_product.cpp).

## Building

|                   | requirement                                                    | version                       |
|-------------------|----------------------------------------------------------------|-------------------------------|
|**Compiler**       | Standard C++ compiler                                          | C++11                         |
|**Build System**   | [CMake](https://cmake.org)                                     | ≥ 3.2                         |
|**Dependencies**   | [Boost](http://www.boost.org/) (Program Options, Preprocessor) | ≥ 1.58 (Clang), >= 1.55 (GCC) |
|                   | MPI implementation (e.g. MPICH, OpenMPI, Intel MPI, ...)       | >= MPI-2                      |
|                   | Optional for Xeon Phi accelerators (KNC): MPSS for SCIF        | any                           |

### Building and Installing Boost

**NOTE:** Check if pre-build packages are available for your operating system.
If not proceed like follows:

1. Download Boost: http://www.boost.org/users/download/, e.g.:

	```terminal
	$ wget http://downloads.sourceforge.net/project/boost/boost/1.66.0/boost_1_66_0.tar.bz2
	```

2. Open the [tools/install_boost.sh](tools/install_boost.sh) script in an
   editor of your choice. Read the comments, perform modifications as needed,
   and run it. This might take some time. If nothing goes wrong, you should
   end up with a working Boost installation.

### Building HAM-Offload

The core HAM library is header-only and is located in
[include/ham/msg](include/ham/msg).

The different HAM-Offload libraries can be built with CMake. They also depend
on Boost Program Options, which means you need a Boost installation. You can
either use the packages of your distribution (including the developer packages
and Boost.Build), or install Boost from the sources. Let's assume you decide
for the latter, since you most likely want Xeon Phi support for which you won't
find  packages. If you want to use the MPI back-end, you will also need an MPI
installation.

1. Checkout the GitHub repository:

    ```terminal
    $ git clone https://github.com/noma/ham.git
    ```

2. Create a build directory:

    ```terminal
    $ cd ham # change into repository
    $ mkdir build
    $ cmake ../src -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=CC -DBUILD_SHARED_LIBS=OFF
    ```

    - `-DCMAKE_BUILD_TYPE=Release`
    - `-DCMAKE_CXX_COMPILER=CC` specifies the compiler. On a CRAY-System you
      should use the `CC` wrapper, because it will have all MPI-related flags
      already set. You could also specify `g++`, `clang++` or `icpc`.
    - `-DBUILD_SHARED_LIBS=OFF` will build ham as a static library (e.g. `libham_offload_mpi.a`).

3. Build everything:

	```terminal
	$ make -j 8 # do 8 builds in parallel
	```

	If nothing went wrong, the libraries are in `build`.
	For each back-end, there are two libraries. The one with the `_explicit`
	suffix is intended for library authors, who need to explicitly initialise
	and finalise HAM-Offload.

	|                           | `scif.h` found                                              | MPI found                                                 |
	|---------------------------|-------------------------------------------------------------|-----------------------------------------------------------|
	| `BUILD_SHARED_LIBS=True`  | `libham_offload_scif.so`, `libham_offload_scif_explicit.so` | `libham_offload_mpi.so`, `libham_offload_mpi_explicit.so` |
	| `BUILD_SHARED_LIBS=False` | `libham_offload_scif.a`, `libham_offload_scif_explicit.a`   | `libham_offload_mpi.a`, `libham_offload_mpi_explicit.a`   |

4. You can now link or copy the libraries and headers either to your system or
   project, or just use them where they are (in which case you have to add
   their path to `LD_LIBRARY_PATH` environment variable).

### Building HAM-Offload Applications

Benchmarks and examples can be found in [src](src). They are automatically
built with CMake.

## Running HAM-Offload Applications

How you start your applications depends on the back-end you use. In case of
MPI, `mpirun` will do the trick. In case of SCIF, the job's processes need to
be started manually. There will be a `hamrun` tool in the near future, that
unifies and simplifies this task. For the time being, here is how you start
your jobs. This documentation assumes that you have a shared home across your
system including the Xeon Phis.

### MPI

Simply use `mpirun` as for any other MPI job. Rank 0 will be the process that
is your host, all other processes perform offloaded tasks. The node-ids inside
the HAM-Offload API directly map to the MPI ranks. Here are generic examples:
```terminal
# start two processes
$ mpirun -n 2 <application_binary>
# this is the syntax for starting jobs with different binaries
$ mpirun -host localhost -n 1 <application_binary> : -host mic0 -n 1 <application_binary_mic>
```

The [inner_product](src/inner_product.cpp) example can be run using one host
and one Xeon Phi process like this:
```terminal
$ mpirun -n 1 -host localhost bin/intel-linux/release/inlining-on/threading-multi/inner_product_mpi : -n 1 -host mic0 bin/intel-linux/release_mic/inlining-on/threading-multi/inner_product_mpi
```

### SCIF

With SCIF, you have to start the host process (either on a Xeon Phi, or on a
host) first, followed by the other processes that are offload targets. During
initialisation all the offload targets connect to the host in a server-client
like fashion. There is a set of options that can be passed either via the
environment (`HAM_OPTIONS`) or via the command-line (if it does not interfere
with the application code). These are the options:

| Option                    | Description                                                                                                                              |
|---------------------------|------------------------------------------------------------------------------------------------------------------------------------------|
| `--ham-cpu-affinity arg`  | sets the CPU affinity of the process to arg (no affinity is set if omitted)                                                              |
| `--ham-process-count arg` | sets the number of processes of your application                                                                                         |
| `--ham-address arg`       | sets an individual address for each process (between 0 and `ham-process-count` - 1)                                                      |
| `--ham-host-address`      | optional: use a host address other than 0                                                                                                |
| `--ham-host-node`         | optional: specifies the SCIF node (physical host or MIC) where the host process is spawned on (physical host node is always 0 = default) |

Example (host to Xeon Phi offload):  
```terminal
# Use different terminals or start jobs in background (by appending &)
# host process 
$ <application_binary> --ham-process-count 2 --ham-address 0 
# MIC process
$ ssh mic0 `pwd`<application_binary_mic> --ham-process-count 2 --ham-address 1
```

The same Example using the environment to pass the options:  
```terminal
# host process 
$ HAM_OPTIONS="--ham-process-count 2 --ham-address 0" <application_binary> 
# MIC process
$ ssh mic0 env HAM_OPTIONS="--ham-process-count 2 --ham-address 1" `pwd`<application_binary_mic>
```

Example (reverse offload):
The MIC process is the logical host (address 0, and started first). The host's
node address of the SCIF network must be explicitly specified this time.
```terminal
# MIC process
$ ssh mic0 `pwd`<application_binary_mic> --ham-process-count 2 --ham-address 0 --ham-host-node 1
# host process:
$ <application_binary> --ham-process-count 2 --ham-address 0 --ham-host-node 1
```

The [inner_product](src/inner_product.cpp) example can be started like this.
The `env LD_LIBRARY_PATH=$MIC_LD_LIBRARY_PATH` may or may not be necessary
depending on your configuration.
```terminal
$ bin/intel-linux/release/inlining-on/threading-multi/inner_product_scif --ham-process-count 2 --ham-address 0 &
$ ssh mic0 env LD_LIBRARY_PATH=$MIC_LD_LIBRARY_PATH `pwd`/bin/intel-linux/release_mic/inlining-on/threading-multi/inner_product_scif --ham-process-count 2 --ham-address 1
```


## Benchmarking

See the wiki page [Benchmarking](https://github.com/noma/ham/wiki/Benchmarking).

## Acknowledgements

This work is supported by the
[ZIB (Zuse Institute Berlin)](http://www.zib.de/en/home.html)
and the
[IPCC (Intel Parallel Computing Center)](https://software.intel.com/en-us/articles/konrad-zuse-zentrum-fur-informationstechnik-berlin-zib)
activities there.

The architecture of the system was influenced by my earlier work with and on
the [TACO framework](https://www.b-tu.de/fg-betriebssysteme/forschung/projekte/sonstige-projekte-des-fachgebiets/taco).

## Citing

Please cite the following paper in publications related to HAM:
- _A Unified Programming Model for Intra- and Inter-Node Offloading on Xeon Phi
  Clusters_, http://dl.acm.org/citation.cfm?id=2683616
