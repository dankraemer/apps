#!/bin/bash

if [ $# -eq 0 ]; then
	echo $SCRIPT_MSG"Usage: $ ./logfilter.sh [MEASURES|ANALOG|DIGITAL] [Messages_file]"
	exit 1
fi

echo $1
echo $2

#grep 'MEASURES' messages | sed 's/  / /g' | cut -d ' ' -f 1-3,7,13,15 | sed 's/,//g' | sed 's/).//g' | awk '{print $4 " " $1 " " $2 " " $3 "\t" $5 "\t" $6 "\t" $5-$6}'
