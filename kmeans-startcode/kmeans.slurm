#!/bin/bash -l
#SBATCH --cluster=wice
#SBATCH --partition=gpu
#SBATCH --ntasks-per-node=1
#SBATCH --gpus-per-node=1
#SBATCH --account=llp_h_pds
#SBATCH --nodes=1
#SBATCH --time=00:10:00
#SBATCH --error="%x.e%A"
#SBATCH --output="%x.o%A"

source /data/leuven/303/vsc30380/slurmhooks

module purge
module load GCC/10.3.0
module load CUDA/11.7.1

make

./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 8 --blocks 8
./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 16 --blocks 8
./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 32 --blocks 8
./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 64 --blocks 8
./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 128 --blocks 8
./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 256 --blocks 8
./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 512 --blocks 8
./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 1024 --blocks 8
./kmeans --input 100000x5.csv --output someoutput.csv --seed 2054980 --k 10 --repetitions 20 --threads 2048 --blocks 8