import matplotlib.pyplot as plt
import pandas

import sys

import argparse

parser = argparse.ArgumentParser(
    prog='graphy',
    description='makes a graph for A1',
)

parser.add_argument('filename', help='The csv file to plot')
parser.add_argument('-o', '--output', help='A .png to output to instead of rendering', required=False)

args = parser.parse_args()

# Open the file
file_path = args.filename

columns = [
    'DS_TYPENAME', 'TOTAL_THREADS', 'MAXKEY', 'INS', 'DEL', 'prefill_elapsed_ms', 'tree_stats_numNodes', 'total_queries',
    'query_throughput', 'total_inserts', 'total_deletes', 'update_throughput', 'total_ops', 'total_throughput'
    ]

data = pandas.read_csv(file_path,names=columns)

# Filter maxkey
data = data[data['MAXKEY'] == 2_000_000]

# Select needed columns
data = data[['DS_TYPENAME', 'INS', 'TOTAL_THREADS', 'total_throughput']]

insertions = data['INS'].unique()
num_insertions = len(insertions)

fig, axs = plt.subplots(num_insertions//2, 2, sharex=True, sharey=True,)

lines = []
labels = []
for i, insertion in enumerate(insertions):

    
    for data_structure, group in data[data['INS']==insertion].groupby('DS_TYPENAME'):
        ax = axs.flat[i]
        line, = ax.plot(group['TOTAL_THREADS'], group['total_throughput'], label=data_structure)
        ax.scatter(group['TOTAL_THREADS'], group['total_throughput'])
        #plt.scatter(group['TOTAL_THREADS'], group['total_throughput'], label=data_structure, marker='.')
        # Average for a line
        #averaged = group[['TOTAL_THREADS', 'total_throughput']].groupby( 'TOTAL_THREADS', as_index=False).mean()
        #line, = plt.plot(averaged['TOTAL_THREADS'], averaged['total_throughput'], label=data_structure)
        if i == 1:
            lines.append(line)
            labels.append(data_structure)
        if i % 2 == 0:
            ax.set(ylabel='Total Throughput')
        if i >= num_insertions - 2:
            ax.set(xlabel= 'Total Threads')
        ax.set_title(f'Insertion percentage: {insertion}')

w, h = fig.get_size_inches()

fig.set_size_inches(w, h*1.3)
fig.tight_layout()
fig.subplots_adjust(top=.8, hspace=.3)
fig.legend(lines, labels)




# lines = plt.scatter(*plot_points[:2])

if args.output is None:
    plt.show()
else:
    plt.savefig(args.output)
