# we do specific stuff for specific host for now.
HOSTNAME = $(shell hostname)
MACH = $(shell uname -m)
SYS =  $(shell uname -s)
PYTHON = python3

##
# For GCC, the C++ 11 aplication binary interface must match the version
# that HDF5 uses.  Change the ABI version can address errors about undefined functions that vary
# by string type, such as 
#   std::__cxx11::basic_string vs std::basic_string
#
# Specify one of:
#   CXX_ABI_DEF = -D_GLIBCXX_USE_CXX11_ABI=0
# or
#   CXX_ABI_DEF = -D_GLIBCXX_USE_CXX11_ABI=1
#
# in include.local.mk or the environment will change the API.
# You must do a make clean if you change this variable.

# special handling to get C++ ABI right on UCSC Centos 6 servers
ifeq (${CXX_ABI_DEF},)
ifneq ($(wildcard /etc/redhat-release),)
ifeq ($(shell hostname -d), gi.ucsc.edu)
    export CXX_ABI_DEF = -D_GLIBCXX_USE_CXX11_ABI=0
endif
endif
endif
cppflags += ${CXX_ABI_DEF}


#C compiler. FIXME: for some reason the cxx variable is used, which
#typically means C++ compiler.
ifeq (${CC},cc)
  ifeq (${SYS},FreeBSD)
    # default FreeBSD gcc (4.2.1) has warning bug
    #cxx = gcc46 -std=c99 -Wno-unused-but-set-variable
    cxx = gcc34 -std=c99 -Wno-unused-but-set-variable
  else ifeq ($(SYS),Darwin) #This is to deal with the Mavericks replacing gcc with clang fully
	cxx = clang -std=c99 
  else
    cxx = gcc -std=c99
  endif
else
  cxx = ${CC} -std=c99
endif

# C++ compiler. FIXME: for some reason the cpp variable is used, which
# typically refers to the C preprocessor.
ifndef CXX
  ifeq (${SYS},FreeBSD)
    # default FreeBSD gcc (4.2.1) has warning bug
    cpp = g++
  else ifeq ($(SYS),Darwin) #This is to deal with the Mavericks replacing gcc with clang fully
    cpp = clang++
  else
    cpp = g++ 
  endif
else
  cpp = ${CXX} -std=gnu++98
endif

# -Wno-unused-result

# Compiler flags.
# DO NOT put static library -l options here. Those must be specified *after*
# linker input files. See <http://stackoverflow.com/a/8266512/402891>.

#Release compiler flags
cflags_opt = -O3 -g -Wall --pedantic -funroll-loops -DNDEBUG 
#-fopenmp
cppflags_opt = -O3 -g -Wall -funroll-loops -DNDEBUG

#Debug flags (slow)
cflags_dbg = -Wall -O0 -Werror --pedantic -g -fno-inline -UNDEBUG -Wno-error=unused-result
cppflags_dbg = -Wall -g -O0 -fno-inline -UNDEBUG

#Ultra Debug flags (really slow, checks for memory issues)
cflags_ultraDbg = -Wall -Werror --pedantic -g -O1 -fno-inline -fno-omit-frame-pointer -fsanitize=address
cppflags_ultraDbg = -g -O1 -fno-inline -fno-omit-frame-pointer -fsanitize=address

#Profile flags
cflags_prof = -Wall -Werror --pedantic -pg -O3 -g -Wno-error=unused-result
cppflags_prof = -pg -O3 -g -Wall -funroll-loops -DNDEBUG

#Flags to use
ifneq (${CGL_PROF},)
  cppflags += ${cppflags_prof}
  cflags += ${cflags_prof}
else ifneq (${CGL_DEBUG},)
  ifeq (${CGL_DEBUG},ultra)
    cppflags += ${cppflags_ultraDbg}
    cflags += ${cflags_ultraDbg}
  else
    cppflags += ${cppflags_dbg}
    cflags += ${cflags_dbg}
  endif
else
  cppflags += ${cppflags_opt}
  cflags += ${cflags_opt}
endif
# location of Tokyo cabinet
ifndef tokyoCabinetLib
HAVE_TOKYO_CABINET = $(shell pkg-config --exists tokyocabinet; echo $$?)
ifneq ($(wildcard /hive/groups/recon/local/include/tcbdb.h),)
   # hgwdev hive install
   tcPrefix = /hive/groups/recon/local
   tokyoCabinetIncl = -I${tcPrefix}/include -DHAVE_TOKYO_CABINET=1
   tokyoCabinetLib = -L${tcPrefix}/lib -Wl,-rpath,${tcPrefix}/lib -ltokyocabinet -lz -lbz2 -lpthread -lm
else ifneq ($(wildcard /opt/local/include/tcbdb.h),)
   # OS/X with TC installed from MacPorts
   tcPrefix = /opt/local
   tokyoCabinetIncl = -I${tcPrefix}/include -DHAVE_TOKYO_CABINET=1
   tokyoCabinetLib = -L${tcPrefix}/lib -Wl,-rpath,${tcPrefix}/lib -ltokyocabinet -lz -lbz2 -lpthread -lm
else ifneq ($(wildcard /usr/local/include/tcbdb.h),)
   # /usr/local install (FreeBSD, etc)
   tcPrefix = /usr/local
   tokyoCabinetIncl = -I${tcPrefix}/include -DHAVE_TOKYO_CABINET=1
   tokyoCabinetLib = -L${tcPrefix}/lib -Wl,-rpath,${tcPrefix}/lib -ltokyocabinet -lz -lbz2 -lpthread -lm
else ifneq ($(wildcard /usr/include/tcbdb.h),)
   # /usr install (Ubuntu, and probably most Debain-based systems)
   tcPrefix = /usr
   tokyoCabinetIncl = -I${tcPrefix}/include -DHAVE_TOKYO_CABINET=1
   tokyoCabinetLib = -L${tcPrefix}/lib -Wl,-rpath,${tcPrefix}/lib -ltokyocabinet -lz -lbz2 -lpthread -lm
else ifeq (${HAVE_TOKYO_CABINET},0)
   # Install registered with pkg-config
   tokyoCabinetIncl = $(shell pkg-config --cflags tokyocabinet) -DHAVE_TOKYO_CABINET=1
   tokyoCabinetLib = $(shell pkg-config --libs-only-L tokyocabinet) -Wl,-rpath,$(shell pkg-config --variable=libdir tokyocabinet) $(shell pkg-config --libs-only-l --static tokyocabinet)
endif
endif

# location of Kyoto Tycoon
ifndef kyotoTycoonLib
HAVE_KYOTO_TYCOON = $(shell pkg-config --exists kyototycoon; echo $$?)
ifneq ($(wildcard /hive/groups/recon/local/include/ktcommon.h),)
   # hgwdev hive install
   ttPrefix = /hive/groups/recon/local
   kyotoTycoonIncl = -I${ttPrefix}/include -DHAVE_KYOTO_TYCOON=1
   kyotoTycoonLib = -L${ttPrefix}/lib -Wl,-rpath,${ttPrefix}/lib -lkyototycoon -lkyotocabinet -lz -lbz2 -lpthread -lm -lstdc++
else ifneq ($(wildcard /opt/local/include/ktcommon.h),)
   # OS/X with TC installed from MacPorts
   ttPrefix = /opt/local
   kyotoTycoonIncl = -I${ttPrefix}/include -DHAVE_KYOTO_TYCOON=1 
   kyotoTycoonLib = -L${ttPrefix}/lib -Wl,-rpath,${ttPrefix}/lib -lkyototycoon -lkyotocabinet -lz -lbz2 -lpthread -lm -lstdc++ 
else ifneq ($(wildcard /usr/local/include/ktcommon.h),)
   # /usr/local install (FreeBSD, etc)
   ttPrefix = /usr/local
   kyotoTycoonIncl = -I${ttPrefix}/include -DHAVE_KYOTO_TYCOON=1 
   kyotoTycoonLib = -L${ttPrefix}/lib -Wl,-rpath,${ttPrefix}/lib -lkyototycoon -lkyotocabinet -lz -lbz2 -lpthread -lm -lstdc++
else ifneq ($(wildcard /usr/include/ktcommon.h),)
   # /usr install (Ubuntu)
   ttPrefix = /usr
   kyotoTycoonIncl = -I${ttPrefix}/include -DHAVE_KYOTO_TYCOON=1 
   kyotoTycoonLib = -L${ttPrefix}/lib -Wl,-rpath,${ttPrefix}/lib -lkyototycoon -lkyotocabinet -lz -lbz2 -lpthread -lm -lstdc++
else ifeq (${HAVE_KYOTO_TYCOON},0)
   # Install registered with pkg-config
   kyotoTycoonIncl = $(shell pkg-config --cflags kyototycoon) -DHAVE_KYOTO_TYCOON=1
   kyotoTycoonLib = $(shell pkg-config --libs-only-L kyototycoon) -Wl,-rpath,$(shell pkg-config --variable=libdir kyototycoon) $(shell pkg-config --libs-only-l --static kyototycoon)
endif
endif

# location of hiredis
ifndef hiRedisLib
  HAVE_REDIS = $(shell pkg-config --exists hiredis; echo $$?)
  ifeq (${HAVE_REDIS},0)
    hiRedisLib = $(shell pkg-config --libs hiredis)
    incs = $(shell pkg-config --cflags hiredis)
    ifeq ($(findstring -I,${incs}),)
      # Broken 14.04 package
      hiRedisIncl = ${incs} -DHAVE_REDIS=1 -I/usr/include/hiredis
    else
      hiRedisIncl = ${incs} -DHAVE_REDIS=1
    endif
  endif
endif

dblibs = ${tokyoCabinetLib} ${kyotoTycoonLib} ${hiRedisLib} -lz -lm

