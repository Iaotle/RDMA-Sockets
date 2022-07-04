# RDMA Sockets
This is the code for my RDMA Sockets Bachelors' Thesis.
Uses the call hijacking framework created by Animesh and Lin.

Benchmark parameters can be customized in `server-client/copmmon.h`

Parameters:
| Parameter Name | Default Value | Parameter Function |
| -------------- | ------------------ |
| NUM_TESTS      | 100 | Runs this many tests for each message size from INIT_SIZE to MAX_SIZE |
| NUM_ITERATIONS | 100 | Runs this many iterations of send/recv per test |
| INIT_SIZE | 2 | Initial message size to start testing |
| MAX_SIZE | 2^30 | Final message size to test |


## Author
Vadim Isakov

## Framework Authors
Animesh Trivedi, Lin Wang and the teaching team 
VU Amsterdam 