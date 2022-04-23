#!/bin/bash

./coordinator -p 8000 &

./server -cip localhost -cp 8000 -p 8100 -id 1 -t master &
./server -cip localhost -cp 8000 -p 8110 -id 1 -t slave &
./synchronizer -cip localhost -cp 8000 -p 8120 -id 1 &

./server -cip localhost -cp 8000 -p 8200 -id 2 -t master &
./server -cip localhost -cp 8000 -p 8210 -id 2 -t slave &
./synchronizer -cip localhost -cp 8000 -p 8220 -id 2 &

./server -cip localhost -cp 8000 -p 8300 -id 3 -t master &
./server -cip localhost -cp 8000 -p 8310 -id 3 -t slave &
./synchronizer -cip localhost -cp 8000 -p 8320 -id 3