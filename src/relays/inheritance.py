# Distributed under version 3 of the Gnu Affero GPL software license, 
# see the accompanying file COPYING for details.


from random import sample
import numpy as np
import pylab
import sys

executor_offsets = [9, 11, 17, 22, 29, 31]

nodes = range(600)   

successors = [((n - 1) % 600) if (n % 60) != 0 else (n + 59) % len(nodes)
              for n in nodes]
successors = dict(zip(nodes, successors))


def have_five_executors(n, some_nodes):
    num_executors_present = sum([1 for offset in executor_offsets
                                 if (n - offset) % len(nodes) in some_nodes])
    return num_executors_present >= 5

nodes_to_sample_from = []

for i in range(0, len(nodes), 100):
    hours_ago = (600 - i) / 100
    if 'timeweighted' in sys.argv:
        weighting = max(4 - hours_ago, 1)  # 75% loss after 3 hours
    else:
        weighting = 1
    nodes_to_sample_from.extend(nodes[i:i + 100] * weighting)


count = 0
def final_fraction(fraction):
    global count
    my_nodes = set([])
    size = int(fraction * len(nodes))
    while len(my_nodes) < size:
        new_nodes = set(sample(nodes_to_sample_from, size - len(my_nodes)))
        my_nodes = my_nodes.union(new_nodes)

    last_size = 0

    while len(my_nodes) != last_size:
        last_size = len(my_nodes)
        new_nodes = [n for n in nodes 
                     if successors[n] in my_nodes
                     and have_five_executors(n, my_nodes)]
        my_nodes = set(list(my_nodes) + new_nodes)
    
    count += 1
    if count % 100 == 0:
        print count, "points generated"

    return len(my_nodes) * 1.0 / len(nodes)

if __name__ == '__main__':
    fractions = np.random.rand(1000)
    final_fractions = map(final_fraction, fractions)

    fig = pylab.figure()
    ax = fig.add_subplot(111)
    ax.grid(True)
    ax.set_xlabel("Original Fraction of Network Controlled")
    ax.set_ylabel("Fraction Controlled After Repeated Inheritance")
    ax.set_ylim([-0.1, 1.1])
    ax.set_xlim([-0.1, 1.1])
    ax.plot(fractions, final_fractions, marker='o', linewidth=0, color='k')
    ax.set_title("Effect of Recovering all Possible Private "
                 "Keys Through Inheritance", y=1.03)
    pylab.show()
