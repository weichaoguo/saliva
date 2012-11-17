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

#define BUFFER_SIZE 1024

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

using namespace Dyninst;

static const char *USAGE = 
  " [-vsa] <binary> <binary args>\n \
   -v: output verbose instrumentation\n \
   -s: Instrument shared libraries also\n \
   -a: trace all fucntions & loops without optimizing\n";

static const char *OPT_STR = "vsa";

// configuration options
char *inBinary = NULL;
bool includeSharedLib = false;
bool verbose = false;
bool allTrace = false;
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
        case 'a':
            // trace without optimizing
            allTrace = true;
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

// returns function with the name 'funcName', NULL for not found.
BPatch_function * findFuncByName(BPatch_image *appImage, const char *funcName)
{
    BPatch_Vector < BPatch_function * >funcs;
    if (NULL == appImage->findFunction(funcName, funcs) || !funcs.size () || NULL == funcs[0]) {
      cerr << "Failed to find " << funcName << endl;
      return NULL;
    }
    return funcs[0];
}

bool trace(BPatch_addressSpace *app, BPatch_function *func)
{
    char printfFmt[BUFFER_SIZE];
    char funcName[BUFFER_SIZE];
    func->getName(funcName, BUFFER_SIZE);

    //find functions
    BPatch_image *appImage = app->getImage();
    BPatch_function *clockFunc = findFuncByName(appImage, "clock");
    BPatch_function *printfFunc = findFuncByName(appImage, "printf");
    //BPatch_function *fprintfFunc = findFuncByName(appImage, "fprintf");

    //make code snippets for function entry & exit :
    //ENTRY : clockCounter = clock();
    //... function ...
    //EXIT  : clockCounter = clock() - clockCounter;
    //        printf("funcName : %d\n", clockCounter);
    std::vector<BPatch_snippet *> args;
    BPatch_variableExpr *clockCounter = app->malloc(*(appImage->findType("int")));
    BPatch_arithExpr getClock(BPatch_assign, *clockCounter, BPatch_funcCallExpr(*clockFunc, args));

    BPatch_arithExpr getInterval(BPatch_assign, *clockCounter, BPatch_arithExpr(BPatch_minus, BPatch_funcCallExpr(*clockFunc, args), *clockCounter));
    
    std::vector<BPatch_snippet *> printfArgs;
    strcpy(printfFmt, funcName);
    strcat(printfFmt, " : %d\n");
    BPatch_snippet *fmt = new BPatch_constExpr(printfFmt);
    printfArgs.push_back(fmt);
    printfArgs.push_back(clockCounter);
    BPatch_funcCallExpr printfCall(*printfFunc, printfArgs);
    
    BPatch_Vector < BPatch_snippet *> entrySequenceVec;
    entrySequenceVec.push_back(&getClock);
    entrySequenceVec.push_back(&printfCall);
    BPatch_Vector < BPatch_snippet *> exitSequenceVec;
    exitSequenceVec.push_back(&getClock);
    exitSequenceVec.push_back(&printfCall);

    /* Insert the snippet at function entry */
    vector < BPatch_point * >*funcEntry = func->findPoint(BPatch_entry);
    cout << "Inserting instrumention at function entry of " << funcName << endl;
    if (NULL == funcEntry) {
        cerr << "Failed to find entry for function " << funcName << endl;
        return false;
    }

    if(! app->insertSnippet(BPatch_sequence(entrySequenceVec), *funcEntry, BPatch_callBefore, BPatch_lastSnippet)) {
        cerr << "Failed to insert instrumention at function entry of " << funcName << endl;
    return false;
    }
    /* Insert the snippet at function exit */
    vector < BPatch_point * >*funcExit = func->findPoint(BPatch_exit);
    cout << "Inserting instrumention at function exit of " << funcName << endl;
    if (NULL == funcExit) {
        cerr << "Failed to find exit for function " << funcName << endl;
        return false;
    }

    if (! app->insertSnippet(BPatch_sequence(exitSequenceVec), *funcExit, BPatch_callAfter, BPatch_firstSnippet)) {
        cerr << "Failed to insert instrumention at function exit of " << funcName << endl;
        return false;
    }
    return true;
}
int main (int argc, char *argv[])
{
    if(!parseArgs(argc, argv)) return EXIT_FAILURE;

    initSkipLibraries();

    BPatch bpatch;

    // Open the specified binary
    BPatch_process * appProc = bpatch.processCreate(inBinary, (const char **)binaryArgv);
    BPatch_binaryEdit * appBin = bpatch.openBinary(inBinary, true);
    if (appProc == NULL || appBin == NULL) {
        cerr << "Failed to open Binary or create process" << endl;
        return EXIT_FAILURE;
    }

    BPatch_image *appImage = appProc->getImage();

    vector < BPatch_module * >*modules = appImage->getModules();
    vector < BPatch_module * >::iterator moduleIter;

    BPatch_module * defaultModule;

    for (moduleIter = modules->begin(); moduleIter != modules->end(); ++moduleIter) {
        char moduleName[BUFFER_SIZE];
        (*moduleIter)->getName(moduleName, BUFFER_SIZE);

        // find the default module
        if( string(moduleName).find("DEFAULT_MODULE") != string::npos ) {
            defaultModule = (*moduleIter);
            break;
        }

    }

    BPatch_function *mainFunc = findFuncByName(appImage, "main");
    trace(appProc, mainFunc);
    trace(appBin, findFuncByName(appBin->getImage(), "main"));
    appProc->continueExecution();
    while (!appProc->isTerminated()) {
        bpatch.waitForStatusChange();
    }

    // initial snippet
    //
    // insert the initialization snippet
    // insert the fini snippet
    // Output the instrumented binary
    printf("outBinary : %s\n", outBinary);
    if (! appBin->writeFile(outBinary)) {
        cerr << "Failed to write output file: " << outBinary << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
