
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

network:
	sbt "test:runMain oneway.NetworkTester"

hardware:
	sbt "runMain oneway.Router"


clean:
	-rm -rf generated
	-rm -rf target
	-rm -rf project

