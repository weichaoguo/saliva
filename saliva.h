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
//  saliva.h
//  Saliva
//
//  Created by Weichao Guo <guoweichao@mail.com> on 11/20/12.
//
#ifndef SALIVA_H_
#define SALIVA_H_

#define BUFFER_SIZE 128

void initSkipLibraries();
bool parseArgs(int argc, char *argv[]);
bool traceFunc(BPatch_addressSpace *app, BPatch_function *func, int *traceId);
bool traceAll(BPatch_addressSpace *app);
// returns function with the name 'funcName', NULL for not found.
BPatch_function * findFuncByName(BPatch_image *appImage, const char *funcName);

#endif // SALIVA_H_
