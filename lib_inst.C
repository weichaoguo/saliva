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
//  lib_inst.C
//  Saliva
//
//  Created by Weichao Guo <guoweichao@mail.com> on 11/22/12.
//
/*
 * The instrumentation library for the progress trace tool Saliva. Provides
 * functions for initialization, exit functions and basic
 * blocks for progress tracing in function & basic block level.
 */

#include<cstdlib>
#include<cstdio>
#include<unistd.h>
#include<fcntl.h>

#include<iostream>

#define BUFFER_SIZE 128

using namespace std;

static int fd;

void initTrace(const char *dir)
{
    int pid = getpid();
    char filename[BUFFER_SIZE];
    sprintf(filename, "%s/%d", dir, pid);
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
    return;
}
void exitTrace()
{
    close(fd);
    return;
}
void footprintTrace(int traceId)
{
    write(fd, &traceId, sizeof(traceId));
    return;
}
