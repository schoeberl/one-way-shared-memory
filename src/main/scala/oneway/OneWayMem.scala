/*
 * Copyright: 2017, Technical University of Denmark, DTU Compute
 * Author: Martin Schoeberl (martin@jopdesign.com)
 * License: Simplified BSD License
 * 
 * Start with a clone of a simple counter.
 * 
 */

package oneway

import Chisel._
import scala.util.Random

object Const {
  val NORTH = 0
  val EAST = 1
  val SOUTH = 2
  val WEST = 3
  val LOCAL = 4
  val NR_OF_PORTS = 5
}

class SingleChannel extends Bundle {
  val data = UInt(width = 32)
  val valid = Bool()
}

class Channel extends Bundle {
  // val out = new SingleChannel().asOutput
  // val in = new SingleChannel().asInput
  val out = UInt(width = 32).asOutput
  val in = UInt(width = 32).asInput
}

class RouterPorts extends Bundle {
  val ports = Vec(new Channel(), Const.NR_OF_PORTS)
}

class Router(schedule: Array[Array[Int]]) extends Module {
  val io = new RouterPorts

  val regCounter = RegInit(UInt(0, 16))
  // Reg of Vec o Vec of Reg?
  // to check how to do a SingleChannel
  val regOut = Vec(Const.NR_OF_PORTS, Reg(init = UInt(0, 32)))//new SingleChannel()))

  regCounter := Mux(regCounter === UInt(schedule.length - 1), UInt(0), regCounter + UInt(1))

  // Schedule table as a Chisel table
  val sched = Vec(schedule.length, Vec(Const.NR_OF_PORTS, UInt(width=3)))
  for (i <- 0 until schedule.length) {
    val a = new Array[UInt](Const.NR_OF_PORTS)
    for (j <- 0 until Const.NR_OF_PORTS) a(j) = UInt(sched(i)(j), 3)
    sched(i) := Vec[UInt](a)
  }

  val currentSched = sched(regCounter)

  // This Mux is a little bit wasteful, as it allows from all
  // ports to all ports
  for (i <- 0 until Const.NR_OF_PORTS) {
    regOut(i) := io.ports(currentSched(i)).in
    io.ports(i).out := regOut(i)
    // the following shorter version does not consume any LCs. Why?
    // But the upper version also not much? Just the additional reset?
    // Is this indexing into a Vec really the Mux?
    // io.ports(i).out := Reg(next = io.ports(currentSched(i)).in)
  }
}

object Router extends App {

  // Generate a schedule
  val slen = 7
  val schedule = new Array[Array[Int]](slen)
  for (i <- 0 until slen) {
    val oneSlot = new Array[Int](Const.NR_OF_PORTS)
    for (j <- 0 until Const.NR_OF_PORTS) {
      oneSlot(j) = Random.nextInt(5)
    }
    schedule(i) = oneSlot
  }

  chiselMain(Array("--backend", "v", "--targetDir", "generated"),
    () => Module(new Router(schedule)))

}

/**
 * A simple, configurable counter that wraps around.
 */
class OneWayMem(size: Int) extends Module {
  val io = new Bundle {
    val out = UInt(OUTPUT, size)
  }

  val r1 = Reg(init = UInt(0, size))
  r1 := r1 + UInt(1)

  io.out := r1
}
