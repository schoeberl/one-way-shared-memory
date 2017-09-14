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
  
  // just some dummy computation to keep synthesizer
  // from optimzing everything away
  val regAccu = Reg(init=UInt(n, 32))
  regAccu := regAccu + io.local.in.data
  io.local.out.data := regAccu
  io.dout := regAccu
}
