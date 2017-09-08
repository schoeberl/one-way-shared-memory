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
class RouterTester(c: Router) extends Tester(c) {

  for (i <- 0 until 5) {
    poke(c.io.ports(0).in.data, 0x10+i)
    poke(c.io.ports(1).in.data, 0x20+i)
    poke(c.io.ports(2).in.data, 0x30+i)
    poke(c.io.ports(3).in.data, 0x40+i)
    poke(c.io.ports(4).in.data, 0x50+i)
    step(1)
    println(peek(c.io.ports))
  }
}

/**
 * Create a counter and a tester.
 */
object RouterTester {
  def main(args: Array[String]): Unit = {
    chiselMainTest(Array("--genHarness", "--test", "--backend", "c",
      "--compile", "--targetDir", "generated"),
      () => Module(new Router(Router.genRandomSchedule(7)))) {
        c => new RouterTester(c)
      }
  }
}
