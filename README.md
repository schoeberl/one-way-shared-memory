# One-Way Shared Memory

This project is about a novel idea for on-chip communication between processing cores.
Each processor has a local memory (scratch pad memory (SPM)). A simple network-on-chip
moves data from a region in the local SPM to a SPM in a remote processing core.
Tihis is done continually, which means that data is continually updated.
This update is in direction, therefore a one-way shared memory.

The implementation of the NoC to support this one-way shared memory is cheap as
there is no dynamic arbitration, no buffering, and no setup of messages to be sent.
The hardware simply provides guarantees that data will be copied in a bounded time
from one SPM to a remote SPM.
Higher level communcation primitives can be built in software on top of this
hardware primitive.

The evaluation of a prototype hardware implementation shows that the resource
consumptions of the NoC routers and the network interface is very small compared
to other NoC solutions.