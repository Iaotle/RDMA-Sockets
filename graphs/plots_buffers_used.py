# Import libraries
import matplotlib.pyplot as plt
import numpy as np
from statistics import median
tcp = [
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    2626560,
    3677184,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
    4194304,
]

tcp_recv = [
    1061056,
    1061056,
    1061056,
    1061056,
    1061056,
    1061056,
    1061056,
    1061056,
    1061056,
    2226252,
    4976328,
    5762064,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
    6291456,
]
rdma = [
    2,
    4,
    8,
    16,
    32,
    64,
    128,
    256,
    512,
    1024,
    2048,
    4096,
    8192,
    16384,
    32768,
    65536,
    131072,
    262144,
    524288,
    1048576,
    2097152,
    4194304,
    8388608,
    16777216,
    33554432,
    67108864,
    134217728,
    268435456,
    536870912,
    1073741824,
]

tcp = np.divide(tcp, 1024*1024)
tcp_recv = np.divide(tcp_recv, 1024*1024)
rdma = np.divide(rdma, 1024*1024)


fig = plt.figure(figsize=(7, 7))
ax = fig.add_subplot(111)
X = ['2B', '4B', '8B', '16B', '32B', '64B', '128B', '256B', '512B', '1KB', '2KB', '4KB', '8KB', '16KB', '32KB',
     '64KB', '128KB', '256KB', '512KB', '1MB', '2MB', '4MB', '8MB', '16MB', '32MB', '64MB', '128MB', '256MB', '512MB',
     '1GB']
X_axis = np.arange(1, len(X) + 1)
#
tcp_avg = tcp
rdma_avg = rdma
numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
           30]



bp = ax.plot(X, tcp_avg, label='TCP SNDBUF', linewidth=3)
cp = ax.plot(X, rdma_avg, label='RDMA Socket', linewidth=3)
dp = ax.plot(X, tcp_recv, label='TCP RCVBUF', linewidth=3)
plt.xlabel("Message size (Bytes)")
plt.ylabel("Memory (MB), log scale")
plt.legend()
plt.yscale("log")
plt.xticks(rotation=60)
#
plt.savefig('buffer_size_plot_todo_ylabels.pdf', bbox_inches='tight')

plt.show()

