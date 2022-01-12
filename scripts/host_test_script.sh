#!/bin/bash

if [[ $# -lt 1 ]]
then
    echo "Usage: $0 input-file"
    exit 1
fi

if [[ ! -e $1 ]]
then
    echo "$1 does not exist"
    exit 1
fi

./host-test -i $1 -o ./output1.txt -t number=10,portion-size=4096 -I granularity=1 -r capacity=4 -p capacity=1024,count=1024 -k mask=8 -v mask=7 -a KEY-VALUE
#./host-test -i ./output1.txt -o ./output2.txt -t number=10,portion-size=4096 -I granularity=1 -r capacity=4 -p capacity=1024,count=1024 -k mask=8 -v mask=7 -c min=5,max=60 -a SELECT
./host-test -i ./output1.txt -o ./output2.txt -t number=10,portion-size=4096 -I granularity=1 -r capacity=4 -p capacity=1024,count=1024 -k mask=8 -v mask=7 -a SORT
./host-test -i ./output2.txt -o ./output3.txt -t number=10,portion-size=4096 -I granularity=1 -r capacity=4 -p capacity=1024,count=1024 -k mask=8 -v mask=7 -a TOTAL
