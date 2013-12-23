#!/bin/bash

for ((i = 0; i < 50; i++))
do
   echo $i
   #./klient $(($RANDOM % 1 + 1)) $(($RANDOM % 10 + 1)) $(($RANDOM % 1 + 1)) &
   ./klient 1 1 $(($RANDOM % 1 + 1)) &
   #./klient 2 $(($RANDOM % 10 + 1)) $(($RANDOM % 1 + 1)) &
done

./klient 1 25 10 > plik1.out &
./klient 1 25 10 > plik2.out &

for ((i = 0; i < 5000; i++))
do
   ./klient 1 1 0 &
done