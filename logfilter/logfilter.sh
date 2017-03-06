#!/bin/sh

if [ $# -eq 0 ]; then
	echo $SCRIPT_MSG"Usage: $ ./logfilter.sh [MEASURES|ANALOG|DIGITAL] [Messages_file]"
	exit 1
fi

TYPE=$1

INPUT=`cat $2 | grep "\[$TYPE\] Missing"`
INPUT=`echo "$INPUT" | sed 's/  / /g' | sed 's/,//g' | sed 's/).//g'`

case "$TYPE" in
    "MEASURES")
        SELECTED_FIELDS='1-3,7,13,15'
        FIELDS=`echo "$INPUT" | cut -d ' ' -f $SELECTED_FIELDS`
        FORMATED=`echo "$FIELDS" | awk '{print $4 " " $1 " " $2 " " $3 "\t" $5 "\t" $6 "\t" $5-$6}'`
        ;;
    "ANALOG")
        SELECTED_FIELDS='1-3,7,12,14'
        INPUT=`echo "$INPUT" | sed 's/)//g'`
        FIELDS=`echo "$INPUT" | cut -d ' ' -f $SELECTED_FIELDS`
        FORMATED=`echo "$FIELDS" | awk '{print $4 " " $1 " " $2 " " $3 "\t" $5 "\t" $6 "\t" $5-$6 "\t" ($5-$6)/128}'`
        ;;
    "DIGITAL")
        SELECTED_FIELDS='1-3,7,12,14'
        INPUT=`echo "$INPUT" | sed 's/)//g'`
        FIELDS=`echo "$INPUT" | cut -d ' ' -f $SELECTED_FIELDS`
        FORMATED=`echo "$FIELDS" | awk '{print $4 " " $1 " " $2 " " $3 "\t" $5 "\t" $6 "\t" $5-$6 "\t" ($5-$6)/128}'`
        ;;
    *)
        exit 1
        ;;
esac

OUTPUT=$FORMATED
echo "$OUTPUT"

