
help:
	# Makefile for the One-Way Memory Design
	# Targets:
	#   make test
	#   make hardware
	#

test:
	sbt "test:runMain oneway.ScheduleTester"
	sbt "test:runMain oneway.RouterTester"
	sbt "test:runMain oneway.NetworkTester"
	sbt "test:runMain oneway.NetworkCompare"
	sbt "test:runMain oneway.OneWayMemTester"

testone:
	sbt "test:runMain oneway.OneWayMemTester"

network:
	sbt "test:runMain oneway.NetworkTester"

router:
	sbt "runMain oneway.Router"

hardware:
	sbt "runMain oneway.OneWayMem"


clean:
	-rm -rf generated
	-rm -rf target
	-rm -rf project

# format the schedule tables from the GitHub repo of s4noc.
# see: ScheduleTable.scala for the url

format:
	sbt "runMain oneway.ScheduleTable ../../t-crest/s4noc/noc/vhdl/generated/bt2x2/bt2x2.shd"

# copy the source over to the Patmos project


T-CREST=$(HOME)/t-crest

to-patmos:
	cp src/main/scala/oneway/*.scala $(T-CREST)/patmos/hardware/src/main/scala/oneway
