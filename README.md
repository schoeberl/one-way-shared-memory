# One-Way Shared Memory

This repository contains the source code of the architecture for following paper:

*Martin Schoeberl, One-Way Shared Memory, accepted for publication, DATE 2018*

We will continue to work on the project. Therefore, to reproduce the exact
same results as presented in the DATE paper, use following tag:

03cacf682c01b35ceab72003488fadc7ae731cc2


## Abstract

This paper presents a novel idea for on-chip communication between processing cores.
Each processor has a local memory (scratchpad memory (SPM)). A simple network-on-chip
moves data from a region in the local SPM to a SPM in a remote processing core.
This is done continually, which means that data is continually updated.
This update is in one direction only, therefore we call it a one-way shared memory.

The implementation of the network-on-chip to support this one-way shared memory
is cheap as there is no dynamic arbitration, no buffering, and no setup of
messages to be sent.
The hardware simply provides guarantees that data will be copied in a bounded time
from one SPM to a remote SPM to support real-time systems.
Higher level communication primitives can be built in software on top of this
hardware primitive.

The evaluation of a prototype hardware implementation shows that the resource
consumptions of the NoC routers and the network interface is very small compared
to other NoC solutions. Furthermore, we provide bounds of the communication latency
between processing cores.

## Setup

The hardware is described in [Chisel](https://chisel.eecs.berkeley.edu/)
and needs just a Java runtime and `sbt` installed. All other needed packages
will be automatically downloaded by `sbt`.


On a Mac with OS X `sbt` can be installed, assuming using [homebrew](http://brew.sh/)
as your package management tool, with:
```
brew install sbt
```

On a Debian based Linux machine, such as Ubuntu, you can install `sbt` with:
```
echo "deb https://dl.bintray.com/sbt/debian /" | sudo tee -a /etc/apt/sources.list.d/sbt.list
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 \
  --recv 2EE0EA64E40A89B84B2DF73499E82A75642AC823
sudo apt-get update
sudo apt-get install sbt
```

For the Chisel based tests compiler with gcc like interface is needed.

## Running the Tests and Hardware Generation

A small `Makefile` contains shortcuts to run different tasks.

A single router can be tested with:
```
make test
```

Verilog code can be generated with:
```
make hardware
```
The code is placed in folder `generated` and can by synthesized with your
favorite synthesize tool. A Quartus project is available in folder `quartus`.
