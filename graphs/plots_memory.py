# Import libraries
import matplotlib.pyplot as plt
import numpy as np
from statistics import median
tcp = [
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.1,
    20.2,
    20.5,
    21,
    22,
    24,
    28,
    36,
    52,
    84,
    148,
    276,
    532,
    1044,
]
rdma = [
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.8,
    46.9,
    47.2,
    47.7,
    48.7,
    50.7,
    54.7,
    62.7,
    78.7,
    110.7,
    174.7,
    302.7,
    558.7,
    1070.7,
]
fig = plt.figure(figsize=(7, 7))
ax = fig.add_subplot(111)
X = ['2B', '4B', '8B', '16B', '32B', '64B', '128B', '256B', '512B', '1KB', '2KB', '4KB', '8KB', '16KB', '32KB',
     '64KB', '128KB', '256KB', '512KB', '1MB', '2MB', '4MB', '8MB', '16MB', '32MB', '64MB', '128MB', '256MB', '512MB',
     '1GB']
X_axis = np.arange(1, len(X) + 1)
tcp_avg = tcp
rdma_avg = rdma
numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
           30]



bp = ax.plot(X, tcp_avg, label='Linux Socket', linewidth=3)
cp = ax.plot(X, rdma_avg, label='RDMA Socket', linewidth=3)
plt.xlabel("Message size (Bytes)")
plt.ylabel("Memory (MB), log scale")
plt.legend()
plt.yscale("log")
plt.yticks([20, 46, 1024], ['20MB', '46MB', '1GB'])
plt.xticks(rotation=60)
#
plt.savefig('memory_plot.pdf', bbox_inches='tight')

plt.show()

