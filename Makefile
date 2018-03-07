
help:
	# Makefile for the One-Way Memory Design
	# Targets:
	#   make test
	#   make hardware
	#

test:
	sbt "test:runMain s4noc.ScheduleTester"
	sbt "test:runMain s4noc.RouterTester"
	sbt "test:runMain s4noc.NetworkTester"
	sbt "test:runMain s4noc.NetworkCompare"
	sbt "test:runMain oneway.OneWayMemTester"

testone:
	sbt "test:runMain oneway.OneWayMemTester"

network:
	sbt "test:runMain s4noc.NetworkTester"

router:
	sbt "runMain s4noc.Router"

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
	cp src/main/scala/s4noc/*.scala $(T-CREST)/patmos/hardware/src/main/scala/s4noc
	cp src/test/scala/oneway/*.scala $(T-CREST)/patmos/hardware/src/test/scala/oneway
	cp src/test/scala/s4noc/*.scala $(T-CREST)/patmos/hardware/src/test/scala/s4noc
