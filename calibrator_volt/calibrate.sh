#!/bin/sh

echo "Calibrate board 0"
calibrator -b 0 -t 10 -f 60

echo "Calibrate board 1"
calibrator -b 1 -t 10 -f 60

echo "Calibrate board 2"
calibrator -b 2 -t 10 -f 60

echo "Calibrate board 3"
calibrator -b 3 -t 10 -f 60
