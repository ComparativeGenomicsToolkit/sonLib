include include.mk
binPath = ./bin

.PHONY: all clean cP cP.clean externalToolsP.clean test linkLib

all : cP ${binPath}/sonLib_daemonize.py linkLib

# FIXME: python library was moved to the source directory without
# fixing the other modules that depend on this.
libFiles = $(notdir $(wildcard src/sonLib/*.py))
linkLib: ${libFiles}

%.py: src/sonLib/%.py
	ln -sf $< $@

clean : cP.clean externalToolsP.clean
	rm -f ${binPath}/sonLib_daemonize.py
	rm -rf ${libFiles} ${libFiles:%=%c}


cP: externalToolsP
	cd C && ${MAKE} all

externalToolsP:
	cd externalTools && ${MAKE} all

cP.clean :
	cd C && ${MAKE} clean

externalToolsP.clean :
	cd externalTools && ${MAKE} clean

test : all
	PYTHONPATH=src:src/sonLib PATH=`pwd`/bin:$$PATH python allTests.py --testLength=SHORT --logLevel=CRITICAL

${binPath}/sonLib_daemonize.py : sonLib_daemonize.py cP
	cp sonLib_daemonize.py ${binPath}/sonLib_daemonize.py
	chmod +x ${binPath}/sonLib_daemonize.py
