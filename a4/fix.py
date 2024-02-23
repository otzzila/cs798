import sys

filename = sys.argv[1]


lines = []

with open(filename, 'r') as f:
    lines = f.readlines()
    
nlines = len(lines)

section_size = nlines // 3
    
with open("new_"+filename, 'w') as f:
    for i, line in enumerate(lines):
        line_junk = line.split(',')
        line_junk[0] = line_junk[0].replace('16', 'sixteen').replace('4', 'four').replace('1', 'one')
        line = ','.join(line_junk)
        print(line, end='', file=f)