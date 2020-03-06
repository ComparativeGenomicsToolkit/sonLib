include ../include.mk
binPath = ../bin
libPath = ../lib
#judySrc = impl/judy-1.0.5/src
exOut = impl

JUDY = ./impl/judy-1.0.5
JUDY_INCDIR = 	$(JUDY)/src
JUDY_LIBDIR = 	$(JUDY)/lib
JUDY_HEADER = 	$(JUDY_INCDIR)/Judy.h
JUDY_LIBBASE = 	Judy
JUDY_LIB = 	$(JUDY_LIBDIR)/lib$(JUDY_LIBBASE).a



libSources = impl/*.c
cppSources = impl/*.cpp
libHeaders = $(wildcard inc/*.h)
libInternalHeaders = impl/*.h
libTests = tests/sonLib*.c tests/st*
quickTreeObjects = ../externalTools/quicktree_1.1/obj/buildtree.o ../externalTools/quicktree_1.1/obj/cluster.o ../externalTools/quicktree_1.1/obj/distancemat.o ../externalTools/quicktree_1.1/obj/options.o ../externalTools/quicktree_1.1/obj/sequence.o ../externalTools/quicktree_1.1/obj/tree.o ../externalTools/quicktree_1.1/obj/util.o
quickTreeLibPath = ../externalTools/quicktree_1.1/include/

testProgs = ${binPath}/sonLibTests ${binPath}/sonLib_kvDatabaseTest ${binPath}/sonLib_cigarTest ${binPath}/sonLib_fastaCTest ${binPath}/hashBenchmark

dbInclFlags = ${tokyoCabinetIncl} ${kyotoTycoonIncl} ${tokyoTyrantIncl} ${mysqlIncl} ${pgsqlIncl} -I${quickTreeLibPath} ${hiRedisIncl}
cflags += ${dbInclFlags}
cppflags += ${dbInclFlags}

all : ${libPath}/sonLib.a ${testProgs}

clean : 
	rm -rf  ${binPath} ${libPath}

${binPath}/sonLibTests : ${libTests} ${libInternalHeaders} ${libPath}/sonLib.a ${libPath}/cuTest.a tests/allTests.c
	@mkdir -p $(dir $@)
	${cxx} ${cflags} -I inc -I ${libPath} -o $@.tmp tests/allTests.c ${libTests} ${libPath}/sonLib.a ${libPath}/cuTest.a ${dblibs} ${mysqlLibs} -lm -lstdc++ -lpthread
	mv $@.tmp $@

${binPath}/sonLib_kvDatabaseTest : ${libTests} ${libInternalHeaders} ${libPath}/sonLib.a ${libPath}/cuTest.a tests/kvDatabaseTest.c tests/kvDatabaseTestCommon.c
	@mkdir -p $(dir $@)
	${cxx} ${cflags} -I inc -I ${libPath} -I tests -o $@.tmp tests/kvDatabaseTest.c tests/kvDatabaseTestCommon.c ${libPath}/sonLib.a ${libPath}/cuTest.a ${dblibs} ${mysqlLibs} -lm
	mv $@.tmp $@

${binPath}/sonLib_cigarTest : tests/cigarsTest.c ${libTests} ${libInternalHeaders} ${libPath}/sonLib.a 
	@mkdir -p $(dir $@)
	${cxx} ${cflags} -I inc -I ${libPath} -o $@.tmp tests/cigarsTest.c ${libPath}/sonLib.a -lm
	mv $@.tmp $@

${binPath}/sonLib_fastaCTest : tests/fastaCTest.c ${libTests} ${libInternalHeaders} ${libPath}/sonLib.a
	@mkdir -p $(dir $@)
	${cxx} ${cflags} -I inc -I ${libPath} -o $@.tmp tests/fastaCTest.c ${libPath}/sonLib.a -lm
	mv $@.tmp $@

${binPath}/kt_connect_test : tests/kt_connect_test.cpp ${libTests} ${libInternalHeaders} ${libPath}/sonLib.a
	@mkdir -p $(dir $@)
	${cpp} ${cppflags} -I inc -I ${libPath}/ -o $@.tmp tests/kt_connect_test.cpp ${libPath}/sonLib.a ${dblibs} -lm
	mv $@.tmp $@

${libPath}/%.h: inc/%.h
	@mkdir -p $(dir $@)
	cp $< $@.tmp
	mv $@.tmp $@

${binPath}/hashBenchmark: tests/hashBenchmark.c ${libPath}/sonLib.a
	${cxx} ${cflags} -I inc -I ${libPath} -o $@.tmp tests/hashBenchmark.c ${libPath}/sonLib.a -lm
	mv $@.tmp $@

#judySandbox: impl/judySandbox.c 
#	#${cxx} ${flags} -I ${judySrc} -o judySandbox impl/judySandbox.c
#	$(CC) -lJudy -o $@ $^ $(judySrc)

#judySandbox: impl/judySandbox.c $(JUDY_HEADER) $(JUDY_LIB)
#	$(CC) $(CFLAGS) -I $(JUDY_INCDIR) $(@F).c -L$(JUDY_LIBDIR) -l$(JUDY_LIBBASE) -o $@

myJudyTest: #impl/myJudyTest.c
	gcc -o myJudyTest impl/myJudyTest.c -lJudy
	./myJudyTest
	rm myJudyTest

${libPath}/sonLib.a : ${libSources} ${cppSources} $(foreach l, ${libHeaders}, ${libPath}/$(notdir $l)) ${libInternalHeaders}
	@mkdir -p $(dir $@)
	${cxx} ${cflags} -I inc -I ${libPath}/  -c ${libSources}
	${cpp} ${cppflags} -I inc -I ${libPath}/ -c ${cppSources}
	ar rc sonLib.a *.o ${quickTreeObjects}
	ranlib sonLib.a 
	rm *.o
	mv sonLib.a ${libPath}/

test:
	${PYTHON} allTests.py --testLength=SHORT --logLevel CRITICAL
