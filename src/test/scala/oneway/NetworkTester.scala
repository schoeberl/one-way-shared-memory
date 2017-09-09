/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 * 
 * Start with a little bit of testing.
 * 
 */

package oneway

import Chisel._

/**
 * Test the router by printing out the value at each clock cycle.
 */
class NetworkTester(c: NetworkOfFour) extends Tester(c) {

  for (i <- 0 until 8) {
    poke(c.io.local(0).in.data, 0x10+i)
    poke(c.io.local(1).in.data, 0x20+i)
    poke(c.io.local(2).in.data, 0x30+i)
    poke(c.io.local(3).in.data, 0x40+i)
    step(1)
    println(peek(c.io.local))
  }
  expect(c.io.local(0).out.data, 0x45)

}

/**
 * Create a counter and a tester.
 */
object NetworkTester {
  def main(args: Array[String]): Unit = {
    chiselMainTest(Array("--genHarness", "--test", "--backend", "c",
      "--compile", "--targetDir", "generated"),
      () => Module(new NetworkOfFour())) {
        c => new NetworkTester(c)
      }
  }
}
