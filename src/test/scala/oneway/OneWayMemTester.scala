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
 * Test the counter by printing out the value at each clock cycle.
 */
class OneWayMemTester(c: OneWayMem) extends Tester(c) {

  for (i <- 0 until 5) {
    println(i)
    println(peek(c.io.out))
    step(1)
  }
}

/**
 * Create a counter and a tester.
 */
object OneWayMemTester {
  def main(args: Array[String]): Unit = {
    chiselMainTest(Array("--genHarness", "--test", "--backend", "c",
      "--compile", "--targetDir", "generated"),
      () => Module(new OneWayMem(4))) {
        c => new OneWayMemTester(c)
      }
  }
}
