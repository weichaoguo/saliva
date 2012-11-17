#!/bin/sh

export DYNINSTAPI_RT_LIB=/usr/lib64/dyninst/libdyninstAPI_RT.so.7
export LD_LIBRARY_PATH=/usr/lib64/dyninst
./saliva testcc
