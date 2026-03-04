#!/bin/bash
# chmod +x tomaTiempos.sh
# ./tomaTiempos.sh

# @file tomaTiempos.sh
# @author Carlos Mendez & Ana Olsson
# @brief Script to time miner with multiple threads
# @version 3.0
# @date 2026-03-01

echo "Select the target"
read target
echo "Select the number of rounds"
read n_rounds
echo "Select the number of threads"
read n_threads

echo "target n_rounds n_threads time" > tiempos.data

make clean
make all

for((i = 1; i <= n_threads; i++)); do
  ./miner $target $n_rounds $i
done

echo "-------------------------- Done! ----------------------------------"