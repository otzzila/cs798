#!/bin/bash

for marker in DS_TYPENAME TOTAL_THREADS MAXKEY INS DEL prefill_elapsed_ms tree_stats_numNodes total_queries query_throughput total_inserts total_deletes update_throughput total_ops
do
    cat $1 | grep "^$marker=" | cut -d "=" -f2 | tr "\n" ","
done
cat $1 | grep "^total_throughput=" | cut -d "=" -f2