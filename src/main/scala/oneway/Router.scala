/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 * 
 * A router for the S4NOC.
 * 
 */

package oneway

import Chisel._
import scala.util.Random

/**
 * Channel directions
 */
object Const {
  val NORTH = 0
  val EAST = 1
  val SOUTH = 2
  val WEST = 3
  val LOCAL = 4
  val NR_OF_PORTS = 5
}
import Const._

class SingleChannel extends Bundle {
  val data = UInt(width = 32)
  // we could extend with a: val valid = Bool()
}

class Channel extends Bundle {
  val out = new SingleChannel().asOutput
  val in = new SingleChannel().asInput
}

class RouterPorts extends Bundle {
  val ports = Vec(new Channel(), Const.NR_OF_PORTS)
}

class Router(schedule: List[List[Int]]) extends Module {
  val io = new RouterPorts

  val regCounter = RegInit(init = UInt(0, 8))

  regCounter := Mux(regCounter === UInt(schedule.length - 1), UInt(0), regCounter + UInt(1))

  // Schedule table as a Chisel types table
  val sched = Vec(schedule.length, Vec(Const.NR_OF_PORTS, UInt(width = 3)))
  for (i <- 0 until schedule.length) {
    for (j <- 0 until Const.NR_OF_PORTS) {
      sched(i)(j) := UInt(schedule(i)(j), 3)
    }
  }

  val currentSched = sched(regCounter)

  // This Mux is a little bit wasteful, as it allows from all
  // ports to all ports. A 4:1 instead of the 5:1 is way cheaper in an 4-bit LUT FPGA
  for (i <- 0 until Const.NR_OF_PORTS) {
    // just Reg(io...) is optimized away, next is needed. Why?
    io.ports(i).out := Reg(next = io.ports(currentSched(i)).in)
  }
}

class NetworkOfFour() extends Module {
  val io = new Bundle {
    val local = Vec(new Channel(), 4)    
  }
  
  val schedule = Router.gen2x2Schedule()
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
  def connect(r1: Int, p1: Int, r2: Int, p2:Int): Unit = {
  
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

object Router extends App {

  def genRandomSchedule(slen: Int) = {
    var schedule = List[List[Int]]()
    for (i <- 0 until slen) {
      var oneSlot = List[Int]()
      for (j <- 0 until Const.NR_OF_PORTS) {
        oneSlot = Random.nextInt(5) :: oneSlot
      }
      schedule = oneSlot :: schedule
    }
    schedule
  }

  /* A 2x2 schedule is as follows:
ne
  n
   e
 */

  def gen2x2Schedule() = {
    List(List(LOCAL, 0, 0, 0, 0),  // P1: enter from local and exit to north register
      List(0, SOUTH, 0, 0, 0),     // P1: enter from south and exit to east register
      List(LOCAL, 0, 0, 0, WEST),  // P2: local to north, P1: from west to local
      List(0, LOCAL, 0, 0, SOUTH), // P3: local to east, P2: south to local
      List(0, 0, 0, 0, WEST))      // P3: from west to local
      // The last drain increases the schedule length by 1, but could be overlapped.
      // Which means having it in the first slot, as there is no exit in the first slot
  }
  
  chiselMain(Array("--backend", "v", "--targetDir", "generated"),
    () => Module(new Router(genRandomSchedule(7))))
}
