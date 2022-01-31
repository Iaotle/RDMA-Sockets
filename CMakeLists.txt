#
# Copyright [2020] [Animesh Trivedi]
#
# This code is part of the Advanced Network Programming (ANP) course
# at VU Amsterdam.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#        http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.10)
project(anpnetstack VERSION 1.0.1 DESCRIPTION "ANP networking stack")
include(GNUInstallDirs)

# -g is needed to print the debugging symbols in the backtrace, you can later remove it with -O2
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -shared -g")

set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib/)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

add_library(anpnetstack SHARED
        src/init.c
        src/tap_netdev.c
        src/utilities.c
        src/anp_netdev.c
        src/anpwrapper.c
        src/subuff.c
        src/route.c
        src/timer.c
        src/arp/arp.c
        src/ip/ip_rx.c
        src/ip/ip_tx.c
        src/icmp/icmp.c
        src/tcp/tcp_tx.c
        src/tcp/tcp_rx.c
        src/tcp/tcb.c
        src/tcp/tcb_store.c
        src/tcp/isn.c
        src/tcp/ringbuffer.c)

find_package(Threads)
target_link_libraries(anpnetstack ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(anpnetstack dl)

set_target_properties(anpnetstack PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER include/anpnetstack.h)

target_include_directories(anpnetstack PRIVATE src)
target_include_directories(anpnetstack PRIVATE include)

install(TARGETS anpnetstack
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_executable(anp_server server-client/tcp_server.c server-client/common.h server-client/common.c)
add_executable(anp_client server-client/tcp_client.c server-client/common.h server-client/common.c)


add_executable(anp_server_mult server-client/tcp_server_mult.c server-client/common.h server-client/common.c)
add_executable(anp_client_mult server-client/tcp_client_mult.c server-client/common.h server-client/common.c)
