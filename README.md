saliva
======

A static instrumentation tool for tracing task progress based on DynInst.
In the version, saliva use the DynC_API for a more efficient instrument.
It's a sub project of Anaconda, "saliva" of Anaconda means making tasks
speedup in Anaconda's cluster.

Also included in this directory is a toy program that demonstrates a use
of the saliva: testcc and libtestcc. They can be built with the provided
Makefile.

The provided Makefile can be used to build the program. The DYNINST_INCLUDE
environment variable should be set to the Dyninst include directory and the
DYNINST_LIB should be set to the lib directory with the Dyninst libraries.
Also, make sure to set the LD_LIBRARY_PATH environment variable to include
the library directory and set the DYNINSTAPI_RT_LIB environment variable to
${DYNINST_LIBRARY_DIRECTORY}/libdyninstAPI_RT.so.

Saliva can also using codeCoverage sampling first before instrument for
a better control with a appropriate grained. Use a '-c' option to make
it availble. The included codeCoverage directory is a sampling tool
modified for saliva which can help binary saving function samplings as
files with '.ccsf' suffix.

An example usage of the saliva with this program follows.

It is assumed that the following binaries have been built: saliva,
libtestcc, testcc.

First, execute ./saliva without any arguments to see the usage.

% ./saliva    
Input binary not specified.    
Usage: ./saliva [-cv] <binary>    
    -c: use codeCoverage sampling data    
    -v: output verbose instrumentation    

Now, pass the testcc executable as input, instrumenting with verbose info
and using code coverage samplings which is named '<binary>.ccsf'.(execute
codeCoverage with the '-f' option to get the sampling file)
% ./saliva -cv ./testcc    

Then, run the testcc.inst program to generate process tracing data in the
directory '/dev/shm' with processid as filename. Using 'cat /dev/shm/<pid>'
to see the tracing output. An abridged version of the output follows.

% ./testcc.inst    

% ./cat /dev/shm/<pid>....

_init call_gmon_start frame_dummy __do_global_ctors_aux main two one two
fini __do_global_dtors_aux

'instIt' is a shell script that helps do codeCoverage first, then execute
the binary to get the sampling file with '.ccsf' suffix, and then do the
process tracing instrumentation with saliva. Executing all these manually
for a more fessible instrumentation.
