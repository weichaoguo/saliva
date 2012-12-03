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
#include <unistd.h>
#include <fcntl.h>

using namespace std;

// Command line parsing
#include <getopt.h>

// DyninstAPI includes
#include "BPatch.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_function.h"
#include "dynC.h"

#include "saliva.h"

using namespace Dyninst;
using namespace dynC_API;

//code coverage samples
std::map <string, int> codeCoverageList;
int ccsThreshold = 1024;

static const char *USAGE = 
  " [-cv] <binary>\n \
   -c: use codeCoverage sampling data\n \
   -v: output verbose instrumentation\n";

static const char *OPT_STR = "cv";

// configuration options
char *inBinary = NULL;
bool verbose = false;
bool ccs = false;

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
        case 'c':
            ccs = true;
            break;
        case 'v':
            verbose = true;
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

bool traceFunc(BPatch_addressSpace *app, BPatch_function *func)
{
    char moduleName[BUFFER_SIZE];
    func->getModuleName(moduleName, BUFFER_SIZE);
    char funcName[BUFFER_SIZE];
    func->getName(funcName, BUFFER_SIZE);

    // skip dependent share libraries
    if (func->isSharedLib()) {
        if (skipLibraries.find(moduleName) != skipLibraries.end()) {
            if (verbose)
                cout << "Skipping library: " << moduleName << "'s " << funcName << endl ;
            return false;
        }
    }
    if (ccs && codeCoverageList[funcName] > ccsThreshold) {
        if (verbose)
            cout << "Skipping library: " << moduleName << "'s " << funcName << endl ;
        return false;
    }

    // Insert the snippet at function entry
    vector < BPatch_point * >*funcEntry = func->findPoint(BPatch_entry);
    if (verbose)
        cout << "Inserting instrumention at function entry of " << funcName << endl;
    if (NULL == funcEntry) {
        cerr << "Failed to find entry for function " << funcName << endl;
        return false;
    }
    BPatch_snippet *footprintSnippet = createSnippet(fopen("footprint_dynC_snippet.txt", "r"), *((*funcEntry)[0]), "footprintSnippet");
    if(! app->insertSnippet(*footprintSnippet, *funcEntry, BPatch_callBefore, BPatch_lastSnippet)) {
        cerr << "Failed to insert instrumention at function entry of " << funcName << endl;
        return false;
    }

    return true;
}

bool traceAll(BPatch_addressSpace *app)
{
    BPatch_module * defaultModule;
    BPatch_image *appImage = app->getImage();
    app->malloc(*(appImage->findType("int")), "fd");
    /* To instrument every function in the binary
     * --> iterate over all the modules in the binary 
     * --> iterate over all functions in each modules */
    vector < BPatch_module * > * modules = appImage->getModules();
    vector < BPatch_module * >::iterator moduleIter;

    for (moduleIter = modules->begin(); moduleIter != modules->end(); ++moduleIter)
    {
        char moduleName[BUFFER_SIZE];
        (*moduleIter)->getName(moduleName, BUFFER_SIZE);

        // find the default module
        if( std::string(moduleName).find("DEFAULT_MODULE") != std::string::npos ) {
            defaultModule = (*moduleIter);
        }
        if((*moduleIter)->isSharedLib()) {
            if (skipLibraries.find(moduleName) != skipLibraries.end()) {
                if (verbose)
                    cout << "Skipping library: " << moduleName << endl ;
                continue;
            }
        }
        if (verbose)
            cout << "Instrumenting module: " << moduleName << endl;
        vector < BPatch_function * > * allFunctions = (*moduleIter)->getProcedures ();
        vector < BPatch_function * >::iterator funcIter;

        for (funcIter = allFunctions->begin(); funcIter != allFunctions->end(); ++funcIter)
        {
            BPatch_function * curFunc = *funcIter;
            traceFunc(app, curFunc);
        }
    }
    // insert the initialization snippet & fini snippet
    BPatch_snippet *initSnippet = createSnippet(fopen("init_dynC_snippet.txt", "r"), *app, "initSnippet");
    BPatch_snippet *finiSnippet = createSnippet(fopen("fini_dynC_snippet.txt", "r"), *app, "finiSnippet");
    if (!defaultModule->insertInitCallback(*initSnippet)) {
        cerr << "Failed to insert init function in the module" << endl;
        return EXIT_FAILURE;
    }
    if (!defaultModule->insertFiniCallback(*finiSnippet)) {
        cerr << "Failed to insert exit function in the module" << endl;
        return EXIT_FAILURE;
    }
    return true;
}

int main (int argc, char *argv[])
{
    if(!parseArgs(argc, argv)) return EXIT_FAILURE;

    initSkipLibraries();

    //load code coverage sampling data
    if (ccs) {
        char samplePath[BUFFER_SIZE];
        sprintf(samplePath, "%s.ccsf", inBinary);
        ifstream fin(samplePath);
        string key;
        int value, threshold=0;
        while ( fin >> key >> value ) {
            codeCoverageList[key] = value;
            threshold += value;
        }
        //a simple way to choose the threshold for appropriate graind
        threshold /= codeCoverageList.size();
        ccsThreshold = ccsThreshold < threshold ? threshold : ccsThreshold;
    }

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
