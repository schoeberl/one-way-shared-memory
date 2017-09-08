
help:
	# Makefile for the One-Way Memory Design
	# Targets:
	#   make test
	#   make hardware
	#

test:
	sbt "test:runMain oneway.RouterTester"

network:
	sbt "test:runMain oneway.NetworkTester"

hardware:
	sbt "runMain oneway.Router"


clean:
	-rm -rf generated
	-rm -rf target
	-rm -rf project

