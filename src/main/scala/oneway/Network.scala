/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 * 
 * A network for the S4NOC.
 * 
 */

package oneway

import Chisel._
import Const._

class Network {

}

class NetworkOfFour() extends Module {
  val io = new Bundle {
    val local = Vec(new Channel(), 4)
  }

  val schedule = Schedule.getSchedule(Schedule.FourNodes)
  val net = new Array[Router](4)
  for (i <- 0 until 4) {
    net(i) = Module(new Router(schedule))
    io.local(i).out := net(i).io.ports(LOCAL).out
    net(i).io.ports(LOCAL).in := io.local(i).in
  }

  // Router indexes:
  // 0 - 1
  // |   |
  // 2 - 3
  def connect(r1: Int, p1: Int, r2: Int, p2: Int): Unit = {

    net(r1).io.ports(p1).in := net(r2).io.ports(p2).out
    net(r2).io.ports(p2).in := net(r1).io.ports(p1).out
  }

  connect(0, NORTH, 2, SOUTH)
  connect(0, EAST, 1, WEST)
  connect(0, SOUTH, 2, NORTH)
  connect(0, WEST, 1, EAST)

  connect(1, NORTH, 3, SOUTH)
  connect(1, SOUTH, 3, NORTH)

  connect(2, EAST, 3, WEST)
  connect(2, WEST, 3, EAST)
}