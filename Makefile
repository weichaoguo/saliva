#
# A GNU Makefile
#
# This Makefile builds the Task Progress Tracing Dyninst mutator 
# 'the saliva of Anaconda'
#

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

all: saliva testcc

saliva: saliva.o
	$(CXX) $(CXXFLAGS) -L$(DYNINST_LIB) -L$(LOCAL_LIBS_DIR) -lcommon -ldyninstAPI -liberty -lpthread -o saliva saliva.o

saliva.o: saliva.C
	$(CXX) $(CXXFLAGS) -I$(LOCAL_INC_DIR) -I$(DYNINST_INCLUDE) -c saliva.C

libtestcc.so: libtestcc.c libtestcc.h
	$(CC) $(CFLAGS) $(LIBFLAGS) -o libtestcc.so libtestcc.c

testcc: libtestcc.so testcc.c
	$(CC) $(CFLAGS) -o testcc testcc.c ./libtestcc.so

clean:
	rm -f saliva testcc *.so *.o *.inst
