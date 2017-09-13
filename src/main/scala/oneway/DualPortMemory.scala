/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 */

package oneway

import Chisel._

class Port(n: Int) extends Bundle {
  val addr = UInt(width = n).asInput
  val din = UInt(width = 32).asInput
  val dout = UInt(width = 32).asOutput
  val wren = Bool()
}

class DualPortMemory(n: Int) extends Module {
  val io = new Bundle {
    val portA = new Port(n)
    val portB = new Port(n)
  }
  
  
}
