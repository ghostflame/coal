#!/bin/bash

SECONDS=$(date +%s)
PORT=3801

while [ true ]; do

	max=$(( 10 + ( $RANDOM % 191 ) ))
	i=0

	( while [ $i -lt $max ]; do
		val=$(( 50 + ( $RANDOM % 51 ) ))
		echo "foo.bar $val $SECONDS"
		((i++))
		sleep 3
	done ) | nc 127.0.0.1 $PORT
	echo "Wrote $max points."

done


