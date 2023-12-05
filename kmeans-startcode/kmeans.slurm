#!/bin/bash -l
#SBATCH --cluster=wice
#SBATCH --account=llp_h_pds
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=8
#SBATCH --time=00:10:00
#SBATCH --error="%x.e%A"
#SBATCH --output="%x.o%A"
#SBATCH --partition=gpu
#SBATCH --gpus-per-node=1

source /data/leuven/303/vsc30380/slurmhooks

module purge
module load GCC/10.3.0
module load CUDA/11.7.1

make

./kmeans --input 100000x5.csv --output someoutput.csv --seed 1337 --k 10 --repetitions 20 --threads 32 --blocks 8