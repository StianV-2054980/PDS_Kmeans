import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

########## SERIAL VS PARALLEL ##########
# Read data from csv file
df_serialvsparallel = pd.read_csv('serialvsparallel.csv')

measurements = df_serialvsparallel['measurement'].values
serial = df_serialvsparallel['Ts'].values
parallel = df_serialvsparallel['Tp'].values
avg_serial = np.mean(serial)
avg_parallel = np.mean(parallel)
stdev_serial = np.std(serial)
stdev_parallel = np.std(parallel)

# Plot average data
plt.figure("AVG data", figsize=(1,4), )
plt.errorbar([1], avg_serial, stdev_serial, capsize=3, marker="_", label='Serial time')
plt.errorbar([1], avg_parallel, stdev_parallel, capsize=3, marker="_", label='Parallel time')
plt.xlabel('Number of samples')
plt.ylabel('Time (s)')
plt.legend()

# Plot absolute data
plt.figure("Absolute data", figsize=(12,4))
plt.plot(measurements, serial, label='Serial time')
plt.plot(measurements, parallel, label='Parallel time')
plt.xlabel('Number of samples')
plt.ylabel('Time (s)')
plt.legend()

########## SPEEDUP ##########
df_speedup = pd.read_csv('speedup.csv')
cores = df_speedup['Cores'].values

avg_speedup = df_speedup['avgSpeedup'].values
stdev_speedup = df_speedup['stdevSpeedup'].values

plt.figure("Speedup", figsize=(12,4))
plt.errorbar(cores, avg_speedup, stdev_speedup, capsize=3, label='Speedup')
plt.xlabel('Number of cores')
plt.ylabel('Speedup')
plt.legend()

########## EFFICIENCY ##########
avg_efficiency = df_speedup['avgEfficiency'].values
stdev_efficiency = df_speedup['stdevEfficiency'].values

plt.figure("Efficiency", figsize=(12,4))
plt.errorbar(cores, avg_efficiency, stdev_efficiency, capsize=3, label='Efficiency')
plt.xlabel('Number of cores')
plt.ylabel('Efficiency')
plt.legend()
plt.show()