#!/bin/bash

script_dir=$(dirname $0)

echo -n "" >> results.csv

for file in step*.txt
do
    $script_dir/parse_line.sh $file >> results.csv
done