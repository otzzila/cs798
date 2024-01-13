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
data = data[['DS_TYPENAME', 'TOTAL_THREADS', 'total_throughput']]

lines = []
labels = []
for data_structure, group in data.groupby('DS_TYPENAME'):
    plt.scatter(group['TOTAL_THREADS'], group['total_throughput'], label=data_structure, marker='.')
    # Average for a line
    averaged = group[['TOTAL_THREADS', 'total_throughput']].groupby( 'TOTAL_THREADS', as_index=False).mean()
    line, = plt.plot(averaged['TOTAL_THREADS'], averaged['total_throughput'], label=data_structure)
    lines.append(line)
    labels.append(data_structure)


plt.xlabel('Total Threads')
plt.ylabel('Total Throughput')
plt.legend(lines, labels)
# lines = plt.scatter(*plot_points[:2])

if args.output is None:
    plt.show()
else:
    plt.savefig(args.output)
