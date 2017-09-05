/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 * 
 * Start with a clone of a simple counter.
 * 
 */

package oneway

import Chisel._

/**
 * A simple, configurable counter that wraps around.
 */
class OneWayMem(size: Int) extends Module {
  val io = new Bundle {
    val out = UInt(OUTPUT, size)
  }

  val r1 = Reg(init = UInt(0, size))
  r1 := r1 + UInt(1)

  io.out := r1
}
