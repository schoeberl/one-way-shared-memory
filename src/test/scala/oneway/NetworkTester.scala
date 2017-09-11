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
class NetworkTester(dut: NetworkOfFour) extends Tester(dut) {

  for (i <- 0 until 8) {
    for (j <- 0 until 4) {
      poke(dut.io.local(j).in.data, 0x10 * (j + 1) + i)
    }
    step(1)
    println(peek(dut.io.local))
  }
  expect(dut.io.local(0).out.data, 0x45)
}

object NetworkTester {
  def main(args: Array[String]): Unit = {
    chiselMainTest(Array("--genHarness", "--test", "--backend", "c",
      "--compile", "--targetDir", "generated"),
      () => Module(new NetworkOfFour())) {
        c => new NetworkTester(c)
      }
  }
}
