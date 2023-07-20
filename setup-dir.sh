#!bin/bash

cd ./detection

find . -name "output_images" -exec rm -dr {} \;

mkdir output_images && cd output_images
grep -o '^[A-Z]* ' ../rtps.txt | while read temp; do mkdir "$temp" ; done
