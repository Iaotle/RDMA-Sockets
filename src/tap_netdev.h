/*
 * Copyright [2020] [Animesh Trivedi]
 *
 * This code is part of the Advanced Network Programming (ANP) course
 * at VU Amsterdam.
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

#ifndef ANP_DEV_MANAGEMENT_H
#define ANP_DEV_MANAGEMENT_H

#include "systems_headers.h"

struct tap_netdev {
    // tun device file descriptor
    int tun_fd;
    // device name
    char *devname;
};


char *get_tdev_name();
void tdev_init(void);
int tdev_read(char *buf, int len);
int tdev_write(char *buf, int len);

#endif // ANP_DEV_MANAGEMENT_H
