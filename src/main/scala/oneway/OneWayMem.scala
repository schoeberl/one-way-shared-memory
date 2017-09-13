/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 */

package oneway

import Chisel._
import Const._

/**
 * Create and connect a n x n NoC.
 */
class OneWayMem() extends Module {
  val io = new Bundle {
    val dout = UInt(width = 32).asOutput
    val local = Vec(new Channel(), 4)
  }

  val net = Module(new Network(2))
  for (i <- 0 until 4) {
    net.io.local(i).in := io.local(i).in
    io.local(i).out := net.io.local(i).out
  }
}

object OneWayMem extends App {

  chiselMain(Array("--backend", "v", "--targetDir", "generated"),
    () => Module(new OneWayMem))
}