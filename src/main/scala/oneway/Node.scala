/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 */

package oneway

import Chisel._

/**
 * One NoC node, connected to the router, containing the memory,
 * providing the NI machinery, and adding a tester.
 * Provide some data in dout to get synthesize results.
 */
class Node(n: Int) extends Module {
  val io = new Bundle {
    val local = new Channel()
    val dout = UInt(width = 32).asOutput
  }

  // TODO: shall get TDM schedule length from the actual schedule
  // for 2x2 it is 5
  val scheduleLength = 5
  val regTdmCounter = Reg(init = UInt(0, log2Up(scheduleLength)))

  val end = regTdmCounter === UInt(scheduleLength-1)
  regTdmCounter := Mux(end, UInt(0), regTdmCounter + UInt(1))
  
  val regAddrCounter = Reg(init = UInt(0, 4))
  when (end) {
    regAddrCounter := regAddrCounter + UInt(1)
  }
  
  val address = Cat(regAddrCounter, regTdmCounter)
  
  val dummy = io.local.in.data
  
  // Receive data from the NoC
  val memRx = Module(new DualPortMemory(1024))
  
  memRx.io.port.wrAddr := address
  memRx.io.port.din := io.local.in.data
  memRx.io.port.wren := Bool(true)

  memRx.io.port.rdAddr := dummy
  val d1 = memRx.io.port.dout

  // Send data to the NoC
  val memTx = Module(new DualPortMemory(1024))

  memTx.io.port.wrAddr := dummy
  memTx.io.port.din := dummy
  memTx.io.port.wren := dummy(0)

  memTx.io.port.rdAddr := address
  io.local.out.data := memTx.io.port.dout
  
  
  // just some dummy computation to keep synthesizer
  // from optimzing everything away
  val regAccu = Reg(init=UInt(n, 32))
  regAccu := regAccu + d1
  io.local.out.data := regAccu
  io.dout := regAccu
}
