#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <mpi.h>
#include <float.h>

#include "timing.hpp"

using namespace std;

string outFile = "output.txt";
int width, height;
bool writeCoords = false;
int *outputCoords;
int outputCoordsLength;
int *hotspots;
int hotspotsLength;
vector<int> hotspotsV;
vector<int> outputCoordsV;



vector<vector<int >> parseCsv(string fileName)
{
    ifstream file(fileName.c_str());
    vector < vector<int >> result;
    string line;
    //ignore first line
    getline(file, line);
    while (getline(file, line))
    {
        vector<int> items;
        stringstream ss(line);
        int i;
        while (ss >> i)
        {
            items.push_back(i);
            if (ss.peek() == ',') ss.ignore();
        }
        result.push_back(items);
    }
    file.close();
    return result;
}

string getOutputValue(double cell)
{
    string value;
    if (cell > 0.9)
    {
        value = "X";
    }
    else
    {
        value = to_string((int) floor((cell + 0.09) * 10));
    }
    return value;
}

void printData(unsigned int data[])
{
    for (int j=0; j<height; j++)
    {
        for (int i=0; i<width; i++)
        {
            printf("%3d ", data[i + j*width]);
        }
        printf("\n");
    }
}

void printHeatmap(float data[])
{
    for (int j=0; j<height; j++)
    {
        for (int i=0; i<width; i++)
        {
            printf("%s ", getOutputValue(data[i + j*width]).c_str());
        }
        printf("\n");
    }
}

//goal: maxmize area in relation to circumference -> more computation, less communication
//but more important: use all processors
//ideally square
//{y,x}
vector<int> getIdealGridSize(int width, int height, int numberOfProcessors)
{
    vector<int> result {1,1};

    float idealArea = ((float) width * height) / numberOfProcessors;
    float idealSideLength = sqrt(idealArea);

    if (width < idealSideLength)
    {
        result[0] = numberOfProcessors;
    }
    else if (height < idealSideLength)
    {
        result[1] = numberOfProcessors;
    }
    else
    {
        //find closest fit of grid ratio to ratio of possible numberOfProcessors factors
        float gridRatio = ((float) width) / height;
        float minRatioDiff = FLT_MAX;

        for (int i=1; i<=floor(sqrt(numberOfProcessors)); i++)
        {
            if (numberOfProcessors % i == 0)
            {
                int j = numberOfProcessors / i;
                float ratioDiff = abs(gridRatio - ((float) i) / j);
                if (ratioDiff < minRatioDiff)
                {
                    minRatioDiff = ratioDiff;
                    result[1] = i;
                    result[0] = j;
                }

                ratioDiff = abs(gridRatio - ((float) j) / i);
                if (ratioDiff < minRatioDiff)
                {
                    minRatioDiff = ratioDiff;
                    result[1] = j;
                    result[0] = i;
                }
            }
        }
    }
    return result;
}

int getLocalProblemSize(int problemSize, int topologySize, int processorCoord)
{
    return ceil(((float) problemSize) / topologySize);
}

int getLocalFrom(int problemSize, int topologySize, int processorCoord)
{
    return processorCoord * getLocalProblemSize(problemSize, topologySize, processorCoord);
}

int getLocalTo(int problemSize, int topologySize, int processorCoord)
{
    int to = (processorCoord + 1) * getLocalProblemSize(problemSize, topologySize, processorCoord) - 1;
    return to <= problemSize - 1 ? to : problemSize - 1;
}

int getNonCutLocalTo(int problemSize, int topologySize, int processorCoord)
{
    return (processorCoord + 1) * getLocalProblemSize(problemSize, topologySize, processorCoord) - 1;
}

inline int getIndex(int x, int y, int rowLength)
{
    return y * rowLength + x;
}

void setHotspots(float *grid, vector<int> localHotspots, int round)
{
    for (int i=0; i<localHotspots.size(); i+=3)
        if (localHotspots[i+1] >= round && localHotspots[i+2] < round)
            grid[localHotspots[i]] = 1.f;
}

int getNeighbour(MPI_Comm comm, vector<int> dims, int ownX, int ownY, int deltaX, int deltaY)
{
    int rank;
    //int coords[] = {ownY + deltaY, ownX + deltaX};
    //if (MPI_Cart_rank(comm, coords, &rank) == MPI_ERR_RANK) rank = MPI_PROC_NULL;
    int x = ownX + deltaX;
    int y = ownY + deltaY;
    if (x >= dims[1] || x<0 || y >= dims[0] || y < 0) return MPI_PROC_NULL;
    return y * dims[1] + x;
    //return rank;
}

MPI_Status rowTo(MPI_Comm comm, int fromRank, int toRank, int localWidth, float *sendingRow, float *receivingRow)
{
    MPI_Status status;
    MPI_Sendrecv(sendingRow, localWidth, MPI_INT, toRank, 0,
                 receivingRow, localWidth, MPI_INT, fromRank, 0, comm, &status);
    return status;
}

MPI_Status rowToTop(MPI_Comm comm, int fromRank, int toRank, float *grid, int localWidth, int localHeight)
{
    return rowTo(comm, fromRank, toRank, localWidth,
                 grid + getIndex(1, 1, localWidth),                  //sending row
                 grid + getIndex(1, localHeight - 1, localWidth));   //receiving row
}

MPI_Status rowToBottom(MPI_Comm comm, int fromRank, int toRank, float *grid, int localWidth, int localHeight)
{
    return rowTo(comm, fromRank, toRank, localWidth,
                 grid + getIndex(1, localHeight - 2, localWidth),    //sending row
                 grid + getIndex(1, 0, localWidth));                 //receiving row
}

MPI_Status colTo(MPI_Comm comm, int fromRank, int toRank, float *grid, int localWidth, int localHeight, int sendingColIndex, int receivingColIndex)
{
    float sendingCol[localHeight - 2];
    float receivingCol[localHeight - 2];
    for (int i=0; i<localHeight - 2; i++)
    {
        sendingCol[i] = grid[getIndex(i+1, sendingColIndex, localWidth)];
    }

    MPI_Status status = rowTo(comm, fromRank, toRank, localWidth, sendingCol, receivingCol);

    for (int i=0; i<localHeight - 2; i++)
    {
        grid[getIndex(i+1, receivingColIndex, localWidth)] = receivingCol[i];
    }

    return status;
}

MPI_Status colToLeft(MPI_Comm comm, int fromRank, int toRank, float *grid, int localWidth, int localHeight)
{
    return colTo(comm, fromRank, toRank, grid, localWidth, localHeight, 1, localHeight - 1);
}

MPI_Status colToRight(MPI_Comm comm, int fromRank, int toRank, float *grid, int localWidth, int localHeight)
{
    return colTo(comm, fromRank, toRank, grid, localWidth, localHeight, localHeight - 2, 0);
}

MPI_Status cellTo(MPI_Comm comm, int fromRank, int toRank, float *grid, int localWidth, int xFrom, int yFrom, int xTo, int yTo)
{
    MPI_Status status;
    MPI_Sendrecv(grid + getIndex(xFrom, yFrom, localWidth), localWidth, MPI_INT, toRank, 0,
                 grid + getIndex(xTo, yTo, localWidth), localWidth, MPI_INT, fromRank, 0, comm, &status);
    return status;
}


int main(int argc, char* argv[])
{
    int numprocessors, rank, namelen;

    char processor_name[MPI_MAX_PROCESSOR_NAME];

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocessors);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(processor_name, &namelen);

    timeval start;

    width = atoi(argv[1]);
    height = atoi(argv[2]);
    int numberOfRounds = atoi(argv[3]);

    //determine optimum grid size
    int coords[2];
    vector<int> dims = getIdealGridSize(width, height, numprocessors);
    int periods[] = {false, false};
    int reorder = false;
    MPI_Comm comm;
    comm = MPI_COMM_WORLD;
    //MPI_Cart_create(MPI_COMM_WORLD, 2, &dims.front(), periods, reorder, &comm);
    MPI_Errhandler_set(comm, MPI_ERRORS_RETURN);
    //MPI_Cart_coords(comm, rank, 2, coords);
    coords[1] = rank / dims[0];
    coords[0] = rank % dims[0];
    //printf("%d: (%d,%d) of (%d,%d)\n", rank, coords[1], coords[0], dims[1], dims[0]);

    if ( rank == 0 )
    {
        std::cout << "master (" << rank << "/" << numprocessors << ")\n";
        start = startTiming();

        //read input
        string hotspotsFile = string(argv[4]);

        //read hotspots
        for (vector<int> line : parseCsv(hotspotsFile))
        {
            hotspotsV.insert(hotspotsV.end(), line.begin(), line.end());
        }

        hotspots = &hotspotsV.front();
        hotspotsLength = hotspotsV.size();
    }
    else std::cout << "slave  (" << rank << "/" << numprocessors << ")\n";

    //read coords
    if (argc == 6)
    {
        if (rank == 0)
        {
            string coordFile = string(argv[5]);
            for (vector<int> line : parseCsv(coordFile))
            {
                outputCoordsV.insert(outputCoordsV.end(), line.begin(), line.end());
            }

            outputCoords = &outputCoordsV.front();
            outputCoordsLength = outputCoordsV.size();
        }
        writeCoords = true;

        MPI_Bcast(&outputCoordsLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (rank != 0) outputCoords = new int[outputCoordsLength];
        MPI_Bcast(outputCoords, outputCoordsLength, MPI_INT, 0, MPI_COMM_WORLD);
    }

    //broadcast hotspots
    MPI_Bcast(&hotspotsLength, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) hotspots = new int[hotspotsLength];
    MPI_Bcast(hotspots, hotspotsLength, MPI_INT, 0, MPI_COMM_WORLD);

    int fromX = getLocalFrom(width, dims[1], coords[1]);
    int fromY = getLocalFrom(height, dims[0], coords[0]);
    int toX = getLocalTo(width, dims[1], coords[1]);
    int toY = getLocalTo(height, dims[0], coords[0]);
    int nonCutToX = getNonCutLocalTo(width, dims[1], coords[1]);
    int nonCutToY = getNonCutLocalTo(height, dims[0], coords[0]);

    int left, right, top, bottom, topLeft, topRight, bottomLeft, bottomRight;
    left = getNeighbour(comm, dims, coords[1], coords[0], -1, 0);
    right = getNeighbour(comm, dims, coords[1], coords[0], 1, 0);
    top = getNeighbour(comm, dims, coords[1], coords[0], 0, -1);
    bottom = getNeighbour(comm, dims, coords[1], coords[0], 0, 1);
    topLeft = getNeighbour(comm, dims, coords[1], coords[0], -1, -1);
    topRight = getNeighbour(comm, dims, coords[1], coords[0], 1, -1);
    bottomLeft = getNeighbour(comm, dims, coords[1], coords[0], -1, 1);
    bottomRight = getNeighbour(comm, dims, coords[1], coords[0], 1, 1);

    //printf("%d neighbours: %d, %d, %d, %d, %d, %d, %d, %d, %d\n", rank, topLeft, top, topRight, left, rank, right, bottomLeft, bottom, bottomRight);

    //add borders (either 0, or come from neighbour)
    int localWidth = toX - fromX + 2;
    int localHeight = toY - fromY + 2;
    //don't use smaller grid for last rank, as this would make transferring back to root difficult
    int nonCutLocalWidth = nonCutToX - fromX + 2;
    int nonCutLocalHeight = nonCutToY - fromY + 2;
    float localGridA[nonCutLocalWidth * nonCutLocalHeight];
    float localGridB[nonCutLocalWidth * nonCutLocalHeight];
    fill_n(localGridA, nonCutLocalWidth * nonCutLocalHeight, 0.f);
    fill_n(localGridB, nonCutLocalWidth * nonCutLocalHeight, 0.f);

    //find local hotspots
    vector<int> localHotspots;
    for (int i=0; i<hotspotsLength; i+=4)
    {
        if (hotspots[0] >= fromX && hotspots[0] <= toX && hotspots[1] <= fromY && hotspots[1] >= toY)
        {
            localHotspots.push_back(getIndex(hotspots[0] - fromX + 1, hotspots[1] - fromY + 1, localWidth));
            localHotspots.push_back(hotspots[2]);
            localHotspots.push_back(hotspots[3]);
        }
    }

    float *previous = localGridA;
    float *current = localGridB;

    //perform rounds
    for (int i=0; i<numberOfRounds; i++)
    {
        setHotspots(current, localHotspots, i);

        //transfer data to neighbours
        colToLeft(comm, right, left, current, localWidth, localHeight);
        colToRight(comm, left, right, current, localWidth, localHeight);
        rowToTop(comm, bottom, top, current, localWidth, localHeight);
        rowToBottom(comm, top, bottom, current, localWidth, localHeight);
        cellTo(comm, bottomRight, topLeft, current, localWidth, 1, 1, localWidth - 1, localHeight - 1); // to top left
        cellTo(comm, bottomLeft, topRight, current, localWidth, localWidth - 2, 1, 0, localHeight - 1); // to top right
        cellTo(comm, topRight, bottomLeft, current, localWidth, 1, localHeight - 2, localWidth - 1, 0); // to bottom left
        cellTo(comm, topLeft, bottomRight, current, localWidth, localWidth - 2, localHeight - 2, 0, 0); // to bottom right

        //swap heatmaps
        float *tmp = previous;
        previous = current;
        current = tmp;

        //calculate
        for (int y=1; y<localHeight-1; y++)
        {
            for (int x=1; x<localWidth-1; x++)
            {
                int i = getIndex(x, y, localWidth);
                current[i] = ( previous[i-localWidth-1] + previous[i-localWidth] + previous[i-localWidth+1] +
                               previous[i-1] + previous[i] + previous[i+1] +
                               previous[i+localWidth-1] + previous[i+localWidth] + previous[i+localWidth+1] ) / 9;
            }
        }
    }

    printf("%d done with rounds\n", rank);

    setHotspots(current, localHotspots, numberOfRounds);

    ofstream *output;

    if (rank == 0)
    {
        remove(outFile.c_str());
        output = new ofstream(outFile.c_str());
    }

    if (!writeCoords)
    {
        //transfer data back to root
        float *resultGrid;
        int resultGridSize = nonCutLocalHeight*nonCutLocalWidth*numprocessors;
        if (rank == 0)
        {
            resultGrid = new float[resultGridSize];
        }

        //row major probably -> x's should be directly next to each other
        MPI_Gather(current, nonCutLocalHeight*nonCutLocalWidth, MPI_FLOAT,
                  resultGrid, resultGridSize, MPI_FLOAT, 0, comm);

        if (rank == 0)
        {
            for (int yProc=0; yProc<dims[0]; yProc++)
            {
                for (int yItem=1; yItem<nonCutLocalHeight-1; yItem++)
                {
                    for (int xProc=0; xProc<dims[1]; xProc++)
                    {
                        for (int xItem=1; xItem<nonCutLocalWidth-1; xItem++)
                        {
                            int y = yProc * nonCutLocalHeight + yItem - 1;
                            int x = xProc * nonCutLocalWidth + xItem - 1;
                            if (x >= width || y >= height) continue;
                            string out = getOutputValue(resultGrid[
                                                            yProc * (localWidth * localHeight * dims[1])
                                                            + xProc * (localWidth * localHeight)
                                                            + yItem * localWidth
                                                            + xItem ]);
                            *output << out.c_str();
                        }
                    }
                    *output << "\n";
                }
            }
        } //if rank == 0
    }


    else   // if writeCoords
    {
        for (int i=0; i<outputCoordsLength; i+=2)
        {
            int x = outputCoords[i];
            int y = outputCoords[i+1];
            float value;

            if (rank == 0) {
                if (x >= fromX && x <= toX && y >= fromY && y <= toY) {
                    value = current[getIndex(x - fromX - 1, y - fromY - 1, localWidth)];
                } else {
                    MPI_Status status;
                    MPI_Recv(&value, 1, MPI_FLOAT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
                }
                *output << value <<"\n";
            } else {
                if (x >= fromX && x <= toX && y >= fromY && y <= toY) {
                    MPI_Send(current + getIndex(x - fromX - 1, y - fromY - 1, localWidth), 1, MPI_FLOAT, 0, 0, MPI_COMM_WORLD);
                }
            }
        }

    }

    if (rank == 0)
    {
        output->close();
    }

    if (rank == 0)
    {
        cout << "Overall Runtime: " << getElapsedSec(start) << "\n";
    }

    MPI_Finalize();
    exit(0);
}
