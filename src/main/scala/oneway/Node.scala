/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 */

package oneway

import Chisel._

/**
 * One NoC node, connected to the router, containing the memory,
 * providing the NI machinery.
 */
class Node(n: Int, size: Int) extends Module {
  val io = new Bundle {
    val local = new Channel()
    val memPort = new DualPort(size)
  }

  val st = Schedule.getSchedule(n)
  val scheduleLength = st._1.length
  val validTab = Vec(st._2.map(Bool(_)))

  val regTdmCounter = Reg(init = UInt(0, log2Up(scheduleLength)))
  val end = regTdmCounter === UInt(scheduleLength - 1)
  regTdmCounter := Mux(end, UInt(0), regTdmCounter + UInt(1))

  val nChannles = n * n - 1

  // Send data to the NoC
  val regTxAddrLower = Reg(init = UInt(0, log2Up(scheduleLength)))
  // For quicker testing use only 4 words per connection
  // TODO: get this constant from somewhere
  val regTxAddrUpper = Reg(init = UInt(0, 2))

  val valid = validTab(regTdmCounter)
  when(valid) {
    when(regTxAddrUpper === UInt(nChannles - 1)) {
      regTxAddrUpper := UInt(0)
      regTxAddrLower := regTxAddrLower + UInt(1)
    }.otherwise {
      regTxAddrUpper := regTxAddrUpper + UInt(1)
    }
  }

  val txAddr = Cat(regTxAddrUpper, regTxAddrLower)

  val memTx = Module(new DualPortMemory(size))
  memTx.io.port.wrAddr := io.memPort.wrAddr
  memTx.io.port.wrData := io.memPort.wrData
  memTx.io.port.wrEna := io.memPort.wrEna
  memTx.io.port.rdAddr := txAddr
  io.local.out.data := memTx.io.port.rdData
  // TDM schedule starts one cycle late for read data delay
  io.local.out.valid := RegNext(valid)

  // Receive data from the NoC  
  val regRxAddrLower = Reg(init = UInt(0, log2Up(scheduleLength)))
  // For quicker testing use only 4 words per connection
  // TODO: get this constant from somewhere
  val regRxAddrUpper = Reg(init = UInt(0, 2))

  val validRx = io.local.in.valid
  
  when(validRx) {
    when(regRxAddrUpper === UInt(nChannles - 1)) {
      regRxAddrUpper := UInt(0)
      regRxAddrLower := regRxAddrLower + UInt(1)
    }.otherwise {
      regRxAddrUpper := regRxAddrUpper + UInt(1)
    }
  }

  val rxAddr = Cat(regRxAddrUpper, regRxAddrLower)

  val memRx = Module(new DualPortMemory(size))
  memRx.io.port.wrAddr := txAddr
  memRx.io.port.wrData := io.local.in.data
  memRx.io.port.wrEna := validRx
  memRx.io.port.rdAddr := io.memPort.rdAddr
  io.memPort.rdData := memRx.io.port.rdData

}
