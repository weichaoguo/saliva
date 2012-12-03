#
# A GNU Makefile
#
# This Makefile builds the codeCoverage Dyninst mutator
#

# Make sure to set the DYNINST_INCLUDE environment variable to the include directory where
# Dyninst is installed DYNINST_LIB and lib directory

DYNINST_INCLUDE = /usr/include/dyninst
DYNINST_LIB =  /usr/lib64/dyninst

# These should point to where libelf and libdwarf are installed
LOCAL_INC_DIR = /usr/include
LOCAL_LIBS_DIR = /usr/lib

CXX = g++
CXXFLAGS = -g -Wall
LIBFLAGS = -fpic -shared

CC = gcc
CFLAGS = -Wall -pedantic -g -std=gnu99

all: codeCoverage libInst.so testcc

codeCoverage: codeCoverage.o
	$(CXX) $(CXXFLAGS) -L$(DYNINST_LIB) -L$(LOCAL_LIBS_DIR) -lcommon -ldyninstAPI -liberty -lpthread -o codeCoverage codeCoverage.o

libInst.so: libInst.C
	$(CXX) $(CXXFLAGS) $(LIBFLAGS) libInst.C -o libInst.so  

codeCoverage.o: codeCoverage.C
	$(CXX) $(CXXFLAGS) -I$(LOCAL_INC_DIR) -I$(DYNINST_INCLUDE) -c codeCoverage.C

libtestcc.so: libtestcc.c libtestcc.h
	$(CC) $(CFLAGS) $(LIBFLAGS) -o libtestcc.so libtestcc.c

testcc: libtestcc.so testcc.c
	$(CC) $(CFLAGS) -o testcc testcc.c ./libtestcc.so

clean:
	rm -f codeCoverage testcc *.so *.o *.instcc
