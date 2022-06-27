/*
 * Copyright [2020] [Animesh Trivedi]
 *
 
 
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *        http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdio.h>

#include "anpwrapper.h"
#include "systems_headers.h"

extern char **environ;

#define THREAD_RX 0
#define THREAD_TIMER 1
#define THREAD_MAX 2



void __attribute__((constructor)) _init_anp_netstack() {
    // https://stackoverflow.com/questions/3275015/ld-preload-affects-new-child-even-after-unsetenvld-preload
    //  uff, what a mess. So, if there are exec (which is in the system call, it fork bombs, hence it is
    //  quite important to unset thr LD_PRELOAD once we are here
#ifdef ANP_DEBUG
    int i;
    printf("Unsetting LD_PRELOAD: %x\n", unsetenv("LD_PRELOAD"));
    printf("LD_PRELOAD: \"%s\"\n", getenv("LD_PRELOAD"));
    printf("Environ: %lx\n", environ);
    printf("unsetenv: %lx\n", unsetenv);
    for (i = 0; environ[i]; i++) printf("env: %s\n", environ[i]);
    fflush(stdout);
#else
    unsetenv("LD_PRELOAD");
#endif
    printf(
        "\x1b[34m"
        "Hello there, I am ANP networking stack!\n"
        "\x1b[0m");
    _function_override_init();
}