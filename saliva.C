/*
 * Copyright (C) 2012 Weichao Guo <guoweichao@mail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//
//  saliva.C
//  Saliva
//
//  Created by Weichao Guo <guoweichao@mail.com> on 11/17/12.
//
/*
 *  saliva of Anaconda
 *
 *  A simple task progress tracing tool using DyninstAPI
 *
 *  This tool uses DyninstAPI to instrument the functions and basic blocks
 *  in an executable and its shared libraries in order to trace task
 *  progress in production running. The progress trace is exposed at
 *  /dev/shm/pid for performance consideration.
 *
 *  The intent of this tool is to benifit the scheduling of Anaconda which
 *  is a cluster scheduler.
 */


#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>

using namespace std;

// Command line parsing
#include <getopt.h>

// DyninstAPI includes
#include "BPatch.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_function.h"

#include "saliva.h"
#include "lib_inst.h"

using namespace Dyninst;

static const char *USAGE = 
  " [-vsb] <binary>\n \
   -v: output verbose instrumentation\n \
   -s: Instrument shared libraries also\n \
   -b: trace basic block\n";

static const char *OPT_STR = "vsa";

// configuration options
char *inBinary = NULL;
bool includeSharedLib = false;
bool verbose = false;
bool bbTrace = false;
char **binaryArgv = NULL;

char outBinary[BUFFER_SIZE];
set<string> skipLibraries;

void initSkipLibraries() {
  /* List of shared libraries to skip instrumenting */
  skipLibraries.insert("libc.so.6");
  skipLibraries.insert("libc.so.7");
  skipLibraries.insert("ld-2.5.so");
  skipLibraries.insert("ld-linux.so.2");
  skipLibraries.insert("ld-lsb.so.3");
  skipLibraries.insert("ld-linux-x86-64.so.2");
  skipLibraries.insert("ld-lsb-x86-64.so");
  skipLibraries.insert("ld-elf.so.1");
  skipLibraries.insert("ld-elf32.so.1");
  skipLibraries.insert("libstdc++.so.6");
  return;
}

bool parseArgs(int argc, char *argv[]) {
    int c;
    while( (c = getopt(argc, argv, OPT_STR)) != -1 ) {
        switch((char)c) {
        case 'v':
            verbose = true;
            break;
        case 's':
            // libraries linked to the binary will be instrumented 
            includeSharedLib = true;
            break;
        case 'b':
            // trace without optimizing
            bbTrace = true;
            break;
        default:
            cerr << "Usage: " << argv[0] << USAGE;
            return false;
        }
    }

    int endArgs = optind;

    if( endArgs >= argc ) {
        cerr << "Input binary not specified." << endl
             << "Usage: " << argv[0] << USAGE;
        return false;
    }
    // Input Binary & Output Binary
    inBinary = argv[endArgs];
    strcpy(outBinary, inBinary);
    strcat(outBinary, ".inst");

    endArgs++;
    binaryArgv = argv + endArgs;

    return true;
}

BPatch_function * findFuncByName(BPatch_image *appImage, const char *funcName)
{
    BPatch_Vector < BPatch_function * >funcs;
    if (NULL == appImage->findFunction(funcName, funcs) || !funcs.size () || NULL == funcs[0]) {
        std::cerr << "Failed to find " << funcName << std::endl;
        return NULL;
    }
    return funcs[0];
}

bool traceBasicBlock(BPatch_addressSpace *app, BPatch_function *func, int *traceId)
{
    char funcName[BUFFER_SIZE];
    func->getName(funcName, BUFFER_SIZE);
    BPatch_image *appImage = app->getImage();
    BPatch_function * footprintFunc = findFuncByName(appImage, (char *)"footprintTrace");
    //CFG
    BPatch_flowGraph *CFG = func->getCFG();
    if (! CFG) {
        cerr << "Failed to find CFG for function " << funcName << endl;
        return false;
    }
    BPatch_Vector < BPatch_basicBlockLoop * > loops;
    if (!CFG->getOuterLoops(loops) || loops.size()==0) {
        cerr << "Failed to find loops or No loops in " << funcName << endl;
        return false;
    }
    BPatch_Vector < BPatch_basicBlockLoop * >::iterator loopIter;
    for (loopIter=loops.begin(); loopIter!=loops.end(); ++loopIter)
    {
        BPatch_basicBlockLoop * loop = *loopIter;
        BPatch_Vector<BPatch_point*> *loopEntry = CFG->findLoopInstPoints(BPatch_locLoopEntry, loop);
        BPatch_Vector<BPatch_point*> *loopEndIter = CFG->findLoopInstPoints(BPatch_locLoopEndIter, loop);
        // insert the footprint snippet
        std::vector<BPatch_snippet *> args;
        args.push_back(new BPatch_constExpr((*traceId)++));
        BPatch_funcCallExpr *footprint = new BPatch_funcCallExpr(*footprintFunc,args);

        if (! app->insertSnippet(*footprint, *loopEntry, BPatch_callBefore, BPatch_lastSnippet)) {
            cerr << "Failed to insert instrumention at loop entry of " << funcName << endl;
            return false;
        }
        if (! app->insertSnippet(*footprint, *loopEndIter)) {
            cerr << "Failed to insert instrumention at loop iter end of " << funcName << endl;
            return false;
        }
    }
    return true;
}

bool traceFunc(BPatch_addressSpace *app, BPatch_function *func, int *traceId)
{
    char moduleName[BUFFER_SIZE];
    func->getModuleName(moduleName, BUFFER_SIZE);
    char funcName[BUFFER_SIZE];
    func->getName(funcName, BUFFER_SIZE);

    // if includeSharedLib is not set, skip dependent libraries
    if (func->isSharedLib()) {
        if (!includeSharedLib || skipLibraries.find(moduleName) != skipLibraries.end()) {
            if (verbose)
                cout << "Skipping library: " << moduleName << "'s " << funcName << endl ;
            return false;
        }
    }
    // if is not lib func & trace basic block
    else if (bbTrace)
        traceBasicBlock(app, func, traceId);

    BPatch_image *appImage = app->getImage();
    BPatch_function * footprintFunc = findFuncByName(appImage, (char *)"footprintTrace");
    /* Insert the snippet at function entry */
    vector < BPatch_point * >*funcEntry = func->findPoint(BPatch_entry);
    if (verbose)
        cout << "Inserting instrumention at function entry of " << funcName << endl;
    if (NULL == funcEntry) {
        cerr << "Failed to find entry for function " << funcName << endl;
        return false;
    }
    /* Insert the snippet at function exit */
    vector < BPatch_point * >*funcExit = func->findPoint(BPatch_exit);
    if (verbose)
        cout << "Inserting instrumention at function exit of " << funcName << endl;
    if (NULL == funcExit) {
        cerr << "Failed to find exit for function " << funcName << endl;
        return false;
    }
    // insert the footprint snippet
    std::vector<BPatch_snippet *> args;
    args.push_back(new BPatch_constExpr((*traceId)++));
    BPatch_funcCallExpr *footprint = new BPatch_funcCallExpr(*footprintFunc,args);
    if(! app->insertSnippet(*footprint, *funcEntry, BPatch_callBefore, BPatch_lastSnippet)) {
        cerr << "Failed to insert instrumention at function entry of " << funcName << endl;
    return false;
    }

    return true;
}

bool traceAll(BPatch_addressSpace *app)
{
    int traceId = 0;
    BPatch_module * defaultModule;
    BPatch_image *appImage = app->getImage();
    //add lib_inst.so as the binary image's dynamic library
    const char * instLibrary = "./lib_inst.so";
    if (!app->loadLibrary (instLibrary)) {
        cerr << "Failed to open instrumentation library" << endl;
        return EXIT_FAILURE;
    }
    BPatch_function * initFunc = findFuncByName(appImage, (char *)"initTrace");
    BPatch_function * exitFunc = findFuncByName(appImage, (char *)"exitTrace");

    //BPatch_variableExpr *fd = app->malloc(*(appImage->findType("int")));
    //BPatch_variableExpr *traceId = app->malloc(*(appImage->findType("long")));

    /* To instrument every function in the binary
     * --> iterate over all the modules in the binary 
     * --> iterate over all functions in each modules */
    vector < BPatch_module * > * modules = appImage->getModules();
    vector < BPatch_module * >::iterator moduleIter;

    for (moduleIter = modules->begin (); moduleIter != modules->end (); ++moduleIter)
    {
        char moduleName[BUFFER_SIZE];
        (*moduleIter)->getName(moduleName, BUFFER_SIZE);

        // find the default module
        if( std::string(moduleName).find("DEFAULT_MODULE") != std::string::npos ) {
            defaultModule = (*moduleIter);
        }
        if((*moduleIter)->isSharedLib()) {
            if (!includeSharedLib || skipLibraries.find(moduleName) != skipLibraries.end()) {
                if (verbose)
                    cout << "Skipping library: " << moduleName << endl ;
                continue;
            }
        }
        if (verbose)
            cout << "Instrumenting module: " << moduleName << endl;
        vector < BPatch_function * > * allFunctions = (*moduleIter)->getProcedures ();
        vector < BPatch_function * >::iterator funcIter;

        for (funcIter = allFunctions->begin (); funcIter != allFunctions->end (); ++funcIter)
        {
            BPatch_function * curFunc = *funcIter;
            traceFunc(app, curFunc, &traceId);
        }
    }
    // insert the initialization snippet & fini snippet
    std::vector<BPatch_snippet *> args;
    BPatch_funcCallExpr *exitSnippet = new BPatch_funcCallExpr(*exitFunc,args);
    args.push_back(new BPatch_constExpr("/dev/shm"));
    BPatch_funcCallExpr *initSnippet = new BPatch_funcCallExpr(*initFunc,args);
    BPatch_Vector < BPatch_snippet *> initSequenceVec;
    initSequenceVec.push_back(initSnippet);
    BPatch_sequence init(initSequenceVec);
    BPatch_Vector < BPatch_snippet *> exitSequenceVec;
    initSequenceVec.push_back(exitSnippet);
    BPatch_sequence exit(exitSequenceVec);
    if (!defaultModule->insertInitCallback(init)) {
        cerr << "Failed to insert init function in the module" << endl;
        return EXIT_FAILURE;
    }
    if (!defaultModule->insertFiniCallback(exit)) {
        cerr << "Failed to insert exit function in the module" << endl;
        return EXIT_FAILURE;
    }
    return true;
}

int main (int argc, char *argv[])
{
    if(!parseArgs(argc, argv)) return EXIT_FAILURE;

    initSkipLibraries();

    BPatch bpatch;
    // Open the specified binary
    BPatch_binaryEdit * appBin = bpatch.openBinary(inBinary, true);
    if (appBin == NULL) {
        cerr << "Failed to open Binary or create process" << endl;
        return EXIT_FAILURE;
    }

    traceAll(appBin);

    // Output the instrumented binary
    printf("outBinary : %s\n", outBinary);
    if (! appBin->writeFile(outBinary)) {
        cerr << "Failed to write output file: " << outBinary << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
