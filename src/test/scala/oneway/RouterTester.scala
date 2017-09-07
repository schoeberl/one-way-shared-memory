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
class RouterTester(c: Router) extends Tester(c) {

  for (i <- 0 until 5) {
    poke(c.io.ports(0).in, 1)
    poke(c.io.ports(1).in, 2)
    poke(c.io.ports(2).in, 3)
    poke(c.io.ports(3).in, 4)
    poke(c.io.ports(4).in, 5)
    println(i)
    println(peek(c.io.ports))
    step(1)
  }
}

/**
 * Create a counter and a tester.
 */
object RouterTester {
  def main(args: Array[String]): Unit = {
    chiselMainTest(Array("--genHarness", "--test", "--backend", "c",
      "--compile", "--targetDir", "generated"),
      () => Module(new Router(Router.genSchedule()))) {
        c => new RouterTester(c)
      }
  }
}
