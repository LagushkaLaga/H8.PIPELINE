#!bin/bash

cd /media/ssd

find / -name "output_images" -exec rm -dr {} \;

mkdir output_images && cd output_images
grep -o '^[A-Z]* ' /home/pi/scripts/H8.PIPELINE/detection/rtps.txt | while read temp; do mkdir "$temp" ; done
