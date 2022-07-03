# Import libraries
import matplotlib.pyplot as plt
import numpy as np
from statistics import median
tcp = [
1,
1,
1,
1,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.2,
1.3,
1.3,
1.3,
1.3,
1.3,
1.3,
1.3,
1.3,
1.3,
1.3,
]
rdma = [
    1.63671875,
    1.63671875,
    1.63671875,
    1.63671875,
    1.63671875,
    1.63671875,
    1.63671875,
    1.63671875,
    1.63671875,
    1.63671875,
    1.71875,
    1.71875,
    1.71875,
    1.71875,
    1.71875,
    1.71875,
    1.73828125,
    1.97265625,
    2.22265625,
    2.6875,
    3.71875,
    5.72265625,
    9.5546875,
    17.60546875,
    33.640625,
    65.64453125,
    129.7226563,
    257.5859375,
    513.6679688,
    1001,
]
fig = plt.figure(figsize=(7, 7))
ax = fig.add_subplot(111)
X = ['2B', '4B', '8B', '16B', '32B', '64B', '128B', '256B', '512B', '1KB', '2KB', '4KB', '8KB', '16KB', '32KB',
     '64KB', '128KB', '256KB', '512KB', '1MB', '2MB', '4MB', '8MB', '16MB', '32MB', '64MB', '128MB', '256MB', '512MB',
     '1GB']
X_axis = np.arange(1, len(X) + 1)
# plt.errorbar(X_axis - 0.2, tcp, 0.4, label='TCP')
# plt.errorbar(X_axis + 0.2, rdma, 0.4, label='RDMA')
#
tcp_avg = tcp
rdma_avg = rdma
# for i in range(len(tcp)):
#     tcp_avg.append(median(tcp[i]))
#
# for i in range(len(rdma)):
#     rdma_avg.append(median(rdma[i]))
numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
           30]



bp = ax.plot(X, tcp_avg, label='Linux Socket', linewidth=3)
cp = ax.plot(X, rdma_avg, label='RDMA Socket', linewidth=3)
# dp = ax.plot(numbers, pure_rdma_avg, label='RDMA Write', linewidth=3)
# bp = ax.boxplot(tcp, sym='bx')
# bx = fig.add_subplot(211)
# cp = ax.boxplot(rdma, sym='orange')
# x-axis labels
# ax.set_xticklabels(X)
# bx.set_xticklabels(X)
# plt.xticks(X_axis, X)
plt.xticks(rotation=60)
plt.xlabel("Message size (Bytes)")
plt.ylabel("Memory (MB), log scale")
# plt.title("Memory vs Message size")
plt.legend()
plt.yscale("log")
plt.yticks([1, 1.3, 1.7, 100, 1000], ['1MB', '1.3MB', '1.7MB', '100MB', '1GB'])

#
# for i in range (len(rdma)):
#     print(rdma[i] / tcp[i])
# show plot
plt.savefig('resident_memory_plot.pdf', bbox_inches='tight')

plt.show()
