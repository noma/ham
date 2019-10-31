# HAM - Heterogeneous Active Messages for Offloading

[![Licence](https://img.shields.io/badge/license-BSL-blue.svg?style=flat)](LICENSE_1_0.txt)

|          | master | devel |
|----------|--------|-------|
| Build    | [![Build Status](https://travis-ci.org/noma/ham.svg?branch=master)](https://travis-ci.org/noma/ham) | [![Build Status](https://travis-ci.org/noma/ham.svg?branch=devel)](https://travis-ci.org/noma/ham) |
| Coverity | [![Coverity Scan Build Status](https://scan.coverity.com/projects/8451/badge.svg)](https://scan.coverity.com/projects/noma-ham) | NA |

HAM (Heterogeneous Active Messages) is a research project on efficient active
messaging between heterogeneous architectures and binary files. Its original
motivation is to efficiently offload computations onto accelerators like the
Intel Xeon Phi - locally as well as remotely over a fabric.

The design goals are:
- a simple API unifying intra- and internode offloading,
- minimising the overhead cost of offloading a computation to another
  compute device, and
- to implement offloading in pure C++, i.e. not introducing yet another
  language or compiler extension or tool dependencies


The outcome of this project are the two libraries in this repository:
- **HAM**, a Heterogeneous Active Messages C++ library providing RPC (Remote
  Procedure Call) functionality based on template meta-programming
- **HAM-Offload**, an implementation of the offload programming model based on
  HAM with support for a wide variety of host/target hardware, including
  x86-64, **Intel Xeon Phi (KNC/KNL)**, **NEC SX-Aurora TSUBASA**, and ARM64

In general, HAM-Offload can be used to create multi-process applications, where
one process is a logical host running the program's main, and the other 
processes being offload targets (asynchronously) executing computations, i.e. 
function calls, that were offloaded by the host process. The API also provides
functionality to move data between the processes. The participating processes 
communicate through one of the supported back-end libraries.

These capabilites can be combined into different application patterns, like 
local offloading, offload over fabric, offloading to another (remote) CPU
instead of an accelerator, offloading from an x86 host to a remote ARM64 node
or vice versa, or reverse offloading from an accelerator running the host 
process onto one multiple CPUs running the target processes - be creative.     

**See the [Wiki](https://github.com/noma/ham/wiki) for further documentation.**

Feedback like bug reports, feature requests, or performance results are very
welcome. Please create an [Issue](https://github.com/noma/ham/issues/new) or
send an [e-mail](mailto:ma.noack.pr@gmail.com).

## Dependencies

|                   | requirement                                                    | version |
|-------------------|----------------------------------------------------------------|---------|
|**Compiler**       | Standard C++ compiler                                          | ≥ C++11 |
|**Build System**   | [CMake](https://cmake.org)                                     | ≥ 3.2   |
|**Libraries**      | At least one supported communication library:                  |         |
|                   | - **MPI** (MPICH, OpenMPI, Intel MPI, NEC MPI, ...)                | ≥ MPI-2 |
|                   | - for Intel Xeon Phi (KNC): **MPSS with SCIF** (or MPI)             | any     |
|                   | - for NEC SX-Aurora TSUBASA, Vector Engine: **NEC VEOS+VEO**       | ≥ 2.x   |
|                   | Everything else is header-only and shipped in [ham/thirdparty](https://github.com/noma/ham/tree/master/ham/thirdparty) |         |

## Quickstart

NOTE: we assume some MPI implementation to be available here.

Download:
```terminal
git clone https://github.com/noma/ham.git

cd ham
```

Build:
```terminal
mkdir build
cd build
cmake ../ham
make -j

```

Run:
```terminal
mpirun -n 2 ./inner_product_mpi
```

## Examples

A simple example that demonstrates the usage of HAM-Offload can be found in:
[ham/src/inner\_product_cpp](https://github.com/noma/ham/blob/master/ham/src/inner_product.cpp).

An example project with a `CMakeLists.txt` can be found in:
[examples/hello\_world](https://github.com/noma/ham/tree/master/examples/hello_world).

## Version History

| version | notes                                                             |
|---------|-------------------------------------------------------------------|
|    v0.1 | - initial release with Xeon Phi (KNC) support via SCIF and MPI    |
|    v0.2 | - minor fixes                                                     |
|    v0.3 | - added support for the NEC SX-Aurora TSUBASA                     |
|         | - replaced Boost Program.Options with CLI11 (header-only)         |

## Publications and Citing

- M. Noack,
  **Heterogeneous Active Messages (HAM): Implementing Lightweight Remote Procedure Calls in C++**,
  IWOCL/DHPCC++'19,
  [DOI: 10.1145/3318170.3318195](http://dx.doi.org/10.1145/3318170.3318195)


- M. Noack, E. Focht, Th. Steinke,
  **Heterogeneous Active Messages for Offloading on the NEC SX-Aurora TSUBASA**
  IPDPS/HCW'19,
  [DOI: IPDPSW.2019.00014](https://doi.org/10.1109/IPDPSW.2019.00014)

- M. Noack, F. Wende, Th. Steinke, F. Cordes,
  **A Unified Programming Model for Intra- and Inter-Node Offloading on Xeon Phi Clusters**,
  SC'14,
  [DOI: 10.1109/SC.2014.22](https://doi.org/10.1109/SC.2014.22)

## Acknowledgements

This work is supported by the
[ZIB (Zuse Institute Berlin)](http://www.zib.de/en/home.html)
and the
[IPCC (Intel Parallel Computing Center)](https://software.intel.com/en-us/articles/konrad-zuse-zentrum-fur-informationstechnik-berlin-zib)
activities there.

The architecture of the system was influenced by earlier work with and on
the [TACO framework](https://www.b-tu.de/fg-betriebssysteme/forschung/projekte/sonstige-projekte-des-fachgebiets/taco).
