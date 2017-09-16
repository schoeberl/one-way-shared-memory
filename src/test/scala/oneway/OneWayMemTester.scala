/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 */

package oneway

import Chisel._

/**
 * Test a 2x2 Network.
 */
class OneWayMemTester(dut: OneWayMem) extends Tester(dut) {

  def read(n: Int) = {
    for (i <- 0 until 32) {
      poke(dut.io.memPorts(n).rdAddr, i)
      step(3)
      peek(dut.io.memPorts(n).rdData)
    }
  }
  
  for (i <- 0 until 32) {
    for (j <- 0 until 4) {
      poke(dut.io.memPorts(j).wrAddr, i)
      poke(dut.io.memPorts(j).wrData, 0x100 * (j + 1) + i)
      poke(dut.io.memPorts(j).wrEna, 1)
    }
    step(1)
  }
  // read(0)
  step(300)
  read(1)
}

object OneWayMemTester {
  def main(args: Array[String]): Unit = {
    chiselMainTest(Array("--genHarness", "--test", "--backend", "c",
      "--compile", "--targetDir", "generated"),
      () => Module(new OneWayMem(2, 1024))) {
        c => new OneWayMemTester(c)
      }
  }
}
