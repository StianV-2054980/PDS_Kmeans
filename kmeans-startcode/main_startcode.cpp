#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <tuple>
#include <mpi.h>
#include "CSVReader.hpp"
#include "CSVWriter.hpp"
#include "rng.h"
#include "timer.h"

void usage()
{
	std::cerr << R"XYZ(
Usage:

  kmeans --input inputfile.csv --output outputfile.csv --k numclusters --repetitions numrepetitions --seed seed [--blocks numblocks] [--threads numthreads] [--trace clusteridxdebug.csv] [--centroidtrace centroiddebug.csv]

Arguments:

 --input:
 
   Specifies input CSV file, number of rows represents number of points, the
   number of columns is the dimension of each point.

 --output:

   Output CSV file, just a single row, with as many entries as the number of
   points in the input file. Each entry is the index of the cluster to which
   the point belongs. The script 'visualize_clusters.py' can show this final
   clustering.

 --k:

   The number of clusters that should be identified.

 --repetitions:

   The number of times the k-means algorithm is repeated; the best clustering
   is kept.

 --blocks:

   Only relevant in CUDA version, specifies the number of blocks that can be
   used.

 --threads:

   Not relevant for the serial version. For the OpenMP version, this number 
   of threads should be used. For the CUDA version, this is the number of 
   threads per block. For the MPI executable, this should be ignored, but
   the wrapper script 'mpiwrapper.sh' can inspect this to run 'mpirun' with
   the correct number of processes.

 --seed:

   Specifies a seed for the random number generator, to be able to get 
   reproducible results.

 --trace:

   Debug option - do NOT use this when timing your program!

   For each repetition, the k-means algorithm goes through a sequence of 
   increasingly better cluster assignments. If this option is specified, this
   sequence of cluster assignments should be written to a CSV file, similar
   to the '--output' option. Instead of only having one line, there will be
   as many lines as steps in this sequence. If multiple repetitions are
   specified, only the results of the first repetition should be logged
   for clarity. The 'visualize_clusters.py' program can help to visualize
   the data logged in this file.

 --centroidtrace:

   Debug option - do NOT use this when timing your program!

   Should also only log data during the first repetition. The resulting CSV 
   file first logs the randomly chosen centroids from the input data, and for
   each step in the sequence, the updated centroids are logged. The program 
   'visualize_centroids.py' can be used to visualize how the centroids change.
   
)XYZ";
	exit(-1);
}

// Helper function to read input file into allData, setting number of detected
// rows and columns. Feel free to use, adapt or ignore
void readData(std::ifstream &input, std::vector<double> &allData, size_t &numRows, size_t &numCols)
{
	if (!input.is_open())
		throw std::runtime_error("Input file is not open");

	allData.resize(0);
	numRows = 0;
	numCols = -1;

	CSVReader inReader(input);
	int numColsExpected = -1;
	int line = 1;
	std::vector<double> row;

	while (inReader.read(row))
	{
		if (numColsExpected == -1)
		{
			numColsExpected = row.size();
			if (numColsExpected <= 0)
				throw std::runtime_error("Unexpected error: 0 columns");
		}
		else if (numColsExpected != (int)row.size())
			throw std::runtime_error("Incompatible number of colums read in line " + std::to_string(line) + ": expecting " + std::to_string(numColsExpected) + " but got " + std::to_string(row.size()));

		for (auto x : row)
			allData.push_back(x);

		line++;
	}

	numRows = (size_t)allData.size()/numColsExpected;
	numCols = (size_t)numColsExpected;
}

FileCSVWriter openDebugFile(const std::string &n)
{
	FileCSVWriter f;

	if (n.length() != 0)
	{
		f.open(n);
		if (!f.is_open())
			std::cerr << "WARNING: Unable to open debug file " << n << std::endl;
	}
	return f;
}

std::vector<double> chooseCentroidsAtRandom(size_t numClusters, size_t numRows, size_t numCols, std::vector<double> &allData, Rng &rng) {
	// Use rng to pick numCluster random points
	std::vector<size_t> centroidsIndices(numClusters);
	rng.pickRandomIndices(numRows, centroidsIndices);
	std::vector<double> centroids(numClusters * numCols);
	for(size_t centroidindex = 0; centroidindex < numClusters; centroidindex++){
		for(size_t col = 0; col < numCols; col++){
				centroids[centroidindex * numCols + col] = allData[centroidsIndices[centroidindex] * numCols + col];
		}
	}
	return centroids;
}

std::tuple<size_t, double> findClosestCentroidIndexAndDistance(const size_t row, const std::vector<double>& centroids, const size_t numCols, const std::vector<double>& allData, size_t numClusters) {
	size_t closestCentroidIndex = 0;
	double closestDistance = std::numeric_limits<double>::max();

	// openmp here results in infinite loop
	for (size_t centroidindex = 0; centroidindex < numClusters; centroidindex++) {
		double distance = 0;
		for (size_t col = 0; col < numCols; col++) {
			double diff = allData[row * numCols + col] - centroids[centroidindex * numCols + col];
			distance += (diff * diff);
		}
		if (distance < closestDistance) {
			closestDistance = distance;
			closestCentroidIndex = centroidindex;
		}
	}
	return std::make_tuple(closestCentroidIndex, closestDistance);
}

std::vector<double> averageOfPointsWithCluster(size_t centroidIndex, size_t numCols, std::vector<size_t>& clusters, std::vector<double>& allData, int rank, int size){
	std::vector<double> newCentroid(numCols);
	for(size_t col = 0; col < numCols; col++){
		size_t numPoints = 0;
		double sum = 0;

		int count = clusters.size() / size;
		int start = rank * count;
		int stop = start + count;
		for (size_t i = start; i < stop; i++) {
			if (clusters[i] == centroidIndex) {
				numPoints++;
				sum += allData[i * numCols + col];
			}
		}
		double globalSum = 0;
		int globalNumPoints = 0;
		MPI_Allreduce(&sum, &globalSum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
		MPI_Allreduce(&numPoints, &globalNumPoints, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
		sum = globalSum;
		numPoints = globalNumPoints;

		newCentroid[col] = sum/numPoints;
	}
	return newCentroid;
}

int kmeans(Rng &rng, const std::string &inputFile, const std::string &outputFileName,
           int numClusters, int repetitions, int numBlocks, int numThreads,
           const std::string &centroidDebugFileName, const std::string &clusterDebugFileName)
{
	// If debug filenames are specified, this opens them. The is_open method
	// can be used to check if they are actually open and should be written to.
	FileCSVWriter centroidDebugFile = openDebugFile(centroidDebugFileName);
	FileCSVWriter clustersDebugFile = openDebugFile(clusterDebugFileName);

	FileCSVWriter csvOutputFile(outputFileName);
	if (!csvOutputFile.is_open())
	{
		std::cerr << "Unable to open output file " << outputFileName << std::endl;
		return -1;
	}

	std::ifstream input(inputFile);
	if (!input.is_open())
	{
		std::cerr << "Unable to open input file " << inputFile << std::endl;
		return -1;
	}
	CSVReader inReader(input);
	std::vector<double> allData;
	size_t numRows = 0, numCols = 0;
	readData(input, allData, numRows, numCols);

	// This is a basic timer from std::chrono ; feel free to use the appropriate timer for
	// each of the technologies, e.g. OpenMP has omp_get_wtime()
	Timer timer;

	double bestDistSquaredSum = std::numeric_limits<double>::max(); // can only get better
	std::vector<size_t> stepsPerRepetition(repetitions); // to save the number of steps each rep needed
	std::vector<size_t> bestClusters(numRows, -1); // to save the best clustering

	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	timer.start();

    // Do the k-means routine a number of times, each time starting from
    // different random centroids (use Rng::pickRandomIndices), and keep 
    // the best result of these repetitions.
	for (int r = 0 ; r < repetitions ; r++)
	{
		size_t numSteps = 0;
 
		std::vector<double> centroids = chooseCentroidsAtRandom(numClusters, numRows, numCols, allData, rng);	
		std::vector<size_t> clusters(numRows, -1);

		bool changed = true;
		while (changed)
		{
			changed = false;
			double distanceSquaredSum = 0;

			// Log centroids
			if(centroidDebugFile.is_open() && rank == 0)
				centroidDebugFile.write(centroids, numCols);

			// For dividing the work over the processes
			int count = numRows / size;
			int start = rank * count;
			int stop = start + count;

			//#pragma omp parallel for schedule(static, 100) reduction(+:distanceSquaredSum) reduction(||:changed)
			for (int row = start; row < stop; row++) {
				size_t newCluster;
				double distance;
				std::tie(newCluster, distance) = findClosestCentroidIndexAndDistance(row, centroids, numCols, allData, numClusters);
				distanceSquaredSum += distance;
				
				if (newCluster != clusters[row]) {
					changed = true;
					clusters[row] = newCluster;
				}
			}

			double globalDistanceSquaredSum = 0;
			MPI_Allreduce(&distanceSquaredSum, &globalDistanceSquaredSum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
			bool globalChanged = false;
			MPI_Allreduce(&changed, &globalChanged, 1, MPI_C_BOOL, MPI_LOR, MPI_COMM_WORLD);
			distanceSquaredSum = globalDistanceSquaredSum;
			changed = globalChanged;

			std::vector<size_t> globalClusters(numRows);
			MPI_Allgather(clusters.data() + start, count, MPI_UNSIGNED_LONG, globalClusters.data(), count, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);
			clusters = globalClusters;

			if (changed) {
				// recalculate centroids based on current clustering
				//#pragma omp parallel for schedule(static, 1) // Static 1 because not that much clusters!
				for (int centroidIndex = 0; centroidIndex < numClusters; centroidIndex++) {
					std::vector<double> newCentroids = averageOfPointsWithCluster(centroidIndex, numCols, clusters, allData, rank, size);
					for(int col = 0 ; col < numCols; col++){
						centroids[centroidIndex * numCols + col] = newCentroids[col];
					}
				}
			}

			// keep track of best clustering
			if (distanceSquaredSum < bestDistSquaredSum) {
				bestDistSquaredSum = distanceSquaredSum;
				bestClusters = clusters;
			}
			numSteps++;

			
			// Log clusters
			if(clustersDebugFile.is_open() && rank == 0)
				clustersDebugFile.write(clusters);
		}

		stepsPerRepetition[r] = numSteps;

		// Make sure debug logging is only done on first iteration ; subsequent checks
		// with is_open will indicate that no logging needs to be done anymore.
		centroidDebugFile.close();
		clustersDebugFile.close();
	}

	timer.stop();

	// Some example output, of course you can log your timing data anyway you like.
	if(rank == 0) {
		std::cerr << "# Type,blocks,threads,file,seed,clusters,repetitions,bestdistsquared,timeinseconds" << std::endl;
		std::cout << "sequential," << numBlocks << "," << numThreads << "," << inputFile << "," 
					<< rng.getUsedSeed() << "," << numClusters << ","
					<< repetitions << "," << bestDistSquaredSum << "," << timer.durationNanoSeconds()/1e9
					<< std::endl;
	

		// Write the number of steps per repetition, kind of a signature of the work involved
		csvOutputFile.write(stepsPerRepetition, "# Steps: ");
		csvOutputFile.write(bestClusters);
	}
	
	return 0;
}

int mainCxx(const std::vector<std::string> &args)
{
	if (args.size()%2 != 0)
		usage();

	std::string inputFileName, outputFileName, centroidTraceFileName, clusterTraceFileName;
	unsigned long seed = 0;

	int numClusters = -1, repetitions = -1;
	int numBlocks = 1, numThreads = 1;
	for (int i = 0 ; i < args.size() ; i += 2)
	{
		if (args[i] == "--input")
			inputFileName = args[i+1];
		else if (args[i] == "--output")
			outputFileName = args[i+1];
		else if (args[i] == "--centroidtrace")
			centroidTraceFileName = args[i+1];
		else if (args[i] == "--trace")
			clusterTraceFileName = args[i+1];
		else if (args[i] == "--k")
			numClusters = stoi(args[i+1]);
		else if (args[i] == "--repetitions")
			repetitions = stoi(args[i+1]);
		else if (args[i] == "--seed")
			seed = stoul(args[i+1]);
		else if (args[i] == "--blocks")
			numBlocks = stoi(args[i+1]);
		else if (args[i] == "--threads")
			numThreads = stoi(args[i+1]);
		else
		{
			std::cerr << "Unknown argument '" << args[i] << "'" << std::endl;
			return -1;
		}
	}

	if (inputFileName.length() == 0 || outputFileName.length() == 0 || numClusters < 1 || repetitions < 1 || seed == 0)
		usage();
	
	Rng rng(seed);

	return kmeans(rng, inputFileName, outputFileName, numClusters, repetitions,
			      numBlocks, numThreads, centroidTraceFileName, clusterTraceFileName);
}

int main(int argc, char *argv[])
{
	MPI_Init(&argc, &argv);
	std::vector<std::string> args;
	for (int i = 1 ; i < argc ; i++)
		args.push_back(argv[i]);

	int returnCode = mainCxx(args);
	MPI_Finalize();
	return returnCode;
}
