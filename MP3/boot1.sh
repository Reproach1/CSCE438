#!/bin/bash

./coordinator -p 8000 &

./server -cip localhost -cp 8000 -p 8100 -id 1 -t master &
./server -cip localhost -cp 8000 -p 8110 -id 1 -t slave &
./synchronizer -cip localhost -cp 8000 -p 8120 -id 1