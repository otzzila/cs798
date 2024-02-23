#!/bin/bash

output="results10000000on100000.txt"
range=10000000
size=100000

n=3


make

for threads in 1 4 16
do
    for method in A B C D
    do
	for ((time = 1000; time <= 3000; time = time + 1000))
	do
	    for ((i = 0 ; i < n ; i = i + 1))
	    do
		echo -n "$method@$threads,$time," >> $output
		LD_PRELOAD=./libjemalloc.so taskset -c 0-$((threads-1)) ./benchmark.out -a $method -sT $size -sR $range -m $time -t $threads | grep ^throughput | cut -d":" -f2 | xargs >> $output
		echo "$method $i"
	    done
	done
    done
done
