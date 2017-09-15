/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 */

package oneway

import Chisel._

class Port(size: Int) extends Bundle {
  val addr = UInt(width = log2Up(size)).asInput
  val din = UInt(width = 32).asInput
  val dout = UInt(width = 32).asOutput
  val wren = Bool().asInput
}

class DualPort(size: Int) extends Bundle {
  val rdAddr = UInt(width = log2Up(size)).asInput
  val wrAddr = UInt(width = log2Up(size)).asInput
  val din = UInt(width = 32).asInput
  val dout = UInt(width = 32).asOutput
  val wren = Bool().asInput
}

class DualPortMemory(size: Int) extends Module {
  val io = new Bundle {
    val port = new DualPort(size)
  }

  val mem = Mem(UInt(width = 32), size, seqRead = true)

  io.port.dout := mem(Reg(next = io.port.rdAddr))
  when(io.port.wren) {
    mem(io.port.wrAddr) := io.port.din
  }

}

/**
 * This true dual port memory generates registers.
 */
class TrueDualPortMemory(size: Int) extends Module {
  val io = new Bundle {
    val portA = new Port(size)
    val portB = new Port(size)
  }

  val mem = Mem(UInt(width = 32), size, seqRead = true)

  val regAddrA = Reg(io.portA.addr)
  when(io.portA.wren) {
    mem(io.portA.addr) := io.portA.din
  }.otherwise {
    regAddrA := io.portA.addr
  }
  io.portA.dout := mem(regAddrA)

  // This does not generate a true dual-ported memory,
  // but a register based implementation
  val regAddrB = Reg(io.portB.addr)
  when(io.portB.wren) {
    mem(io.portB.addr) := io.portB.din
  }.otherwise {
    regAddrB := io.portB.addr
  }
  io.portB.dout := mem(regAddrB)
}
