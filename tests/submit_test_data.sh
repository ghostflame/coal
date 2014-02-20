#!/bin/bash

# defaults
path='foo.bar'
ts=$(date +%s)
data=0
port=3801
show=0
submit=1
count=1
dset=''

function set_data( )
{
	if [ -n "$dset" ]; then
		data=$dset
	else
		data=$(( 50 + ( $RANDOM % 50 ) ))
	fi
}


while getopts "Ssp:d:t:P:c:" o; do
	case $o in
		p)
			path=$OPTARG
			;;
		d)
			dset=$OPTARG
			;;
		t)
			ts=$OPTARG
			;;
		P)
			port=$OPTARG
			;;
		c)
			count=$OPTARG
			;;
		s)
			show=1
			;;
		S)
			show=1
			submit=0
			;;
	esac
done

if [ $show -gt 0 ]; then
	set_data
	echo "($count of) $path $data $ts"
	exit 0
fi

if [ $submit -gt 0 ]; then
	(
		i=0
		while [ $i -lt $count ]; do
			set_data
			echo "$path $data $ts"
			((i++))
		done
	) | nc 127.0.0.1 $port
fi

