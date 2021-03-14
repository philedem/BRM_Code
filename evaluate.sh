#!/bin/bash

P=11
C=$((2**P))
M=$1
K=$2
i=0
DS=$(date +"%H%M%S")

echo "$DS"
while [ $i -lt $C ]
do
	echo "$i"
	./main "$P" "$1" "$2" "$i" "eval"
	i=$[$i+1]
done
echo "$DS"
