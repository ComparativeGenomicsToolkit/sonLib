#!/usr/bin/env python

#Copyright (C) 2006-2012 by Benedict Paten (benedictpaten@gmail.com)
#
#Released under the MIT license, see LICENSE.txt
import unittest
import time
import bioioTest
import cigarsTest
import treeTest
try:
    import networkx as NX
    networkx_installed = True
    import nxtreeTest
    import nxnewickTest
except ImportError:
    networkx_installed = False

from sonLib.bioio import system
from sonLib.bioio import parseSuiteTestOptions
from sonLib.bioio import getLogLevelString
from subprocess import check_call, Popen

def needs(program):
    """Decorator: Run this test only if "program" is available."""
    def wrap(fn):
        print fn
        print program
        try:
            check_call([program, "--version"])
        except:
            return unittest.skip(program + ' is missing')(fn)
        else:
            return fn
    return wrap

class TestCase(unittest.TestCase):
    def testSonLibCTests(self):
        """Run most of the sonLib CuTests, fail if any of them fail."""
        system("sonLibTests %s" % getLogLevelString())

    @needs('ktserver')
    def testSonLibKTTests(self):
        """Run the sonLib KyotoTycoon interface tests."""
        ktserver = Popen(["ktserver", "-le"])
        # Give the server ample time to start up
        time.sleep(3)
        try:
            check_call(["sonLib_kvDatabaseTest", "-t", "kyototycoon", "--port", "1978"])
        finally:
            ktserver.kill()

    @needs('redis-server')
    def testSonLibRedisTests(self):
        """Run the sonLib Redis interface tests."""
        redis = Popen(["redis-server"])
        # Give the server ample time to start up
        time.sleep(3)
        try:
            check_call(["sonLib_kvDatabaseTest", "-t", "redis", "--port", "6379"])
        finally:
            redis.kill()

    def testSonLibKVTokyoCabinet(self):
        """Run the sonLib TokyoCabinet interface tests."""
        system("sonLib_kvDatabaseTest --type=tokyocabinet")

def allSuites():
    bioioSuite = unittest.makeSuite(bioioTest.TestCase, 'test')
    cigarsSuite = unittest.makeSuite(cigarsTest.TestCase, 'test')
    treeSuite = unittest.makeSuite(treeTest.TestCase, 'test')
    cuTestsSuite = unittest.makeSuite(TestCase, 'test')
    if not networkx_installed:
        allTests = unittest.TestSuite((bioioSuite, cigarsSuite, treeSuite, cuTestsSuite))
    else:
        nxtreeSuite = unittest.makeSuite(nxtreeTest.TestCase, 'test')
        nxnewickSuite = unittest.makeSuite(nxnewickTest.TestCase, 'test')
        allTests = unittest.TestSuite((bioioSuite, cigarsSuite, treeSuite, cuTestsSuite,
                                       nxtreeSuite, nxnewickSuite))
    return allTests
        
def main():
    parseSuiteTestOptions()
    
    suite = allSuites()
    runner = unittest.TextTestRunner(verbosity=2)
    i = runner.run(suite)
    return len(i.failures) + len(i.errors)
        
if __name__ == '__main__':
    import sys
    sys.exit(main())
