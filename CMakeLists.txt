#
# Copyright [2020] [Animesh Trivedi]
#
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
project(rdmanetstack VERSION 1.1.0 DESCRIPTION "RDMA networking stack")
include(GNUInstallDirs)

# -g is needed to print the debugging symbols in the backtrace, you can later remove it with -O2
set(CMAKE_C_FLAGS "-std=gnu99 -Ofast -march=native -finline-functions -funswitch-loops -pipe")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -shared -g -Ofast -march=native -finline-functions -funswitch-loops -pipe")

set(PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/lib/)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

add_library(rdmanetstack SHARED
        src/init.c
		src/rdma_common.c
		src/socklist.c
        src/rdmawrapper.c
		src/util.c)

find_package(Threads)
find_library(IBVERBS_LIBRARY ibverbs HINTS /home/atr/local/lib)
find_library(RDMACM_LIBRARY rdmacm HINTS /home/atr/local/lib)
target_link_libraries(rdmanetstack ${CMAKE_THREAD_LIBS_INIT} ${IBVERBS_LIBRARY} ${RDMACM_LIBRARY})
target_link_libraries(rdmanetstack dl)

link_libraries(pthread ${IBVERBS_LIBRARY} ${RDMACM_LIBRARY})

set_target_properties(rdmanetstack PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER include/rdmanetstack.h)

target_include_directories(rdmanetstack PRIVATE src)
target_include_directories(rdmanetstack PRIVATE include)


install(TARGETS rdmanetstack
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_executable(tcp_server server-client/tcp_server.c server-client/common.c src/util.c)
add_executable(tcp_client server-client/tcp_client.c server-client/common.c src/util.c)
