# Start with a tester to get stuff going

SBT = sbt

counter:
	$(SBT) "test:runMain oneway.OneWayMemTester"

