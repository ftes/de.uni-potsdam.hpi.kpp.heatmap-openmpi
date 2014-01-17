#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>
#include <vector>
#include <mpi.h>

#include "Hotspot.h"
#include "Coordinate.h"
#include "timing.hpp"

using namespace std;


string outFile = "output.txt";
int width, height;
bool writeCoords = false;
vector<Coordinate> coords;



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

void writeOutput(float data[])
{
    remove(outFile.c_str());
    ofstream output(outFile.c_str());
    if (!writeCoords)
    {
        for (int j=0; j<height; j++)
        {
            for (int i=0; i<width; i++)
            {
                string out = getOutputValue(data[i + j * width]);
                output << out.c_str();
            }
            output << "\n";
        }
    }
    else
    {
        for (Coordinate coord : coords)
        {
            double cell = data[coord.x + coord.y * width];
            output << cell <<"\n";
        }
    }
    output.close();
}





int main(int argc, char* argv[])
{
    int numprocessors, rank, namelen;
    char processor_name[MPI_MAX_PROCESSOR_NAME];

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocessors);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(processor_name, &namelen);

    if ( rank == 0 )
    {
        std::cout << "Processor name: " << processor_name << "\n";
    std::cout << "master (" << rank << "/" << numprocessors << ")\n";
    } else {
        std::cout << "slave  (" << rank << "/" << numprocessors << ")\n";
   }
   MPI_Finalize();
   return 0;
}
/*
void oldMain(int argc, char* argv[])   {

    timeval start = startTiming();

    //read input
    width = atoi(argv[1]);
    height = atoi(argv[2]);
    int numberOfRounds = atoi(argv[3]);

    string hotspotsFile = string(argv[4]);

    if (argc == 6)
    {
        string coordFile = string(argv[5]);
        for (vector<int> line : parseCsv(coordFile))
        {
            Coordinate coord(line.at(0), line.at(1));
            coords.push_back(coord);
        }
        writeCoords = true;
    }

    unsigned int hotspotsStartData[width * height];
    fill_n(hotspotsStartData, width*height, 0);
    unsigned int hotspotsEndData[width * height];
    fill_n(hotspotsEndData, width*height, 0);
    float startData[width * height];
    fill_n(startData, width*height, 0.f);

    //read hotspots
    //problem: if activated several times
    for (vector<int> line : parseCsv(hotspotsFile))
    {
        Hotspot hotspot(line.at(0), line.at(1), line.at(2), line.at(3));
        int index = hotspot.x + hotspot.y * width;
        hotspotsStartData[index] = hotspot.startRound;
        hotspotsEndData[index] = hotspot.endRound;
        if (hotspot.startRound == 0 && hotspot.endRound > 0)
        {
            startData[index] = 1.f;
        }
    }



    //perform rounds
    //the images remain in the device memory instead of reading and writing to host memory every round
    for (int i=0; i<numberOfRounds; i++)
    {
        //swap heatmaps
        //cl::Image2D *tmp = oldHeatmapImage;
        //oldHeatmapImage = newHeatmapImage;
        //newHeatmapImage = tmp;

        //run kernel
        //kernel(eargs, i+1, hotspotsStartImage, hotspotsEndImage, *oldHeatmapImage, *newHeatmapImage).wait();
    }

    writeOutput(startData);

    cout << "Overall Runtime: " << getElapsedSec(start) << "\n";

    exit(0);
}*/
