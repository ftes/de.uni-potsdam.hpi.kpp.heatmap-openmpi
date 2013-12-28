#include <utility>
#define __NO_STD_VECTOR // Use cl::vector instead of STL version
#include <CL/cl.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <iterator>

#include "Hotspot.h"
#include "Coordinate.h"

using namespace std;

int width;
int height;
int numberOfRounds;
int currentRound = 0;
bool writeCoords = false;
vector<Coordinate> coords;
vector<Hotspot> hotspots;
vector<vector<vector<Coordinate>>> *ranges;

vector<vector<double >> *oldHeatmap;
vector<vector<double >> *currentHeatmap;

string outFile = "output.txt";

void setHotspots() {
    for (Hotspot h : hotspots) {
        if (currentRound >= h.startRound && currentRound < h.endRound) {
            (*currentHeatmap)[h.y][h.x] = 1;
        }
    }
}

vector<vector<int >> parseCsv(string fileName) {
    ifstream file(fileName.c_str());
    vector < vector<int >> result;
    string line;
    //ignore first line
    getline(file, line);
    while (getline(file, line)) {
        vector<int> items;
        stringstream ss(line);
        int i;
        while (ss >> i) {
            items.push_back(i);
            if (ss.peek() == ',') ss.ignore();
        }
        result.push_back(items);
    }
    file.close();
    return result;
}

string getOutputValue(double cell) {
    string value;
    if (cell > 0.9) {
        value = "X";
    } else {
        value = to_string((int) floor((cell + 0.09) * 10));
    }
    return value;
}

void printHeatmap(vector<vector<double >> *heatmap) {
    for (vector<double> row : *heatmap) {
        for (double cell : row) {
            printf("%s ", getOutputValue(cell).c_str());
        }
        printf("\n");
    }
}

int max(int in, int maxValue) {
	return in > maxValue ? maxValue : in;
}

int min(int in, int minValue) {
	return in < minValue ? minValue : in;
}

void performRound() {
    auto tmp = oldHeatmap;
    oldHeatmap = currentHeatmap;
    currentHeatmap = tmp;

	for (int j = 0; j < height; j++) {
	    for (int i = 0; i < width; i++) {
			double sum = 0;
			int fromX = (*ranges)[j][i][0].x;
			int toX = (*ranges)[j][i][1].x;
			int fromY = (*ranges)[j][i][0].y;
			int toY = (*ranges)[j][i][1].y;

			for (int y = fromY; y < toY; y++) {
	    		for (int x = fromX; x < toX; x++) {
	        		sum += (*oldHeatmap)[y][x];
	    		}
			}
	    	(*currentHeatmap)[j][i] = sum/9;
	    }
	}

    currentRound++;
    setHotspots();
}

void initializeRanges() {
	ranges = new vector<vector<vector<Coordinate>>>(height, vector<vector<Coordinate>>(width, vector<Coordinate>()));
	for (int j = 0; j < height; j++) {
	    for (int i = 0; i < width; i++) {
	    	Coordinate from(min(i-1, 0), min(j-1, 0));
	    	Coordinate to(max(i+2, width), max(j+2, height));
	    	(*ranges)[j][i].push_back(from);
	    	(*ranges)[j][i].push_back(to);
		}
	}
}

// have to use *& to pass pointer as reference, otherwise we get a copy
// of the pointer, and if we assign a new memory location to it this is local

void initializeHeatmap(vector<vector<double >> *&heatmap) {
    heatmap = new vector < vector<double >> (height, vector<double>(width, 0));
}

void writeOutput() {
    remove(outFile.c_str());
    ofstream output(outFile.c_str());
    if (!writeCoords) {
        for (int j=0; j<height; j++) {
            for (int i=0; i<width; i++) {
            	string out = getOutputValue((*currentHeatmap)[j][i]);
                output << out.c_str();

                if (i == width - 1) {
                	output << "\n";
                }
            }
        }
    } else {
        for (Coordinate coord : coords) {
            double cell = (*currentHeatmap)[coord.y][coord.x];
            output << cell <<"\n";
        }
    }
    output.close();
}

int main(int argc, char* argv[]) {
    //read input
    width = atoi(argv[1]);
    height = atoi(argv[2]);
    numberOfRounds = atoi(argv[3]);

    string hotspotsFile = string(argv[4]);

    if (argc == 6) {
        string coordFile = string(argv[5]);
        for (vector<int> line : parseCsv(coordFile)) {
            Coordinate coord(line.at(0), line.at(1));
            coords.push_back(coord);
        }
        writeCoords = true;
    }

    //read hotspots
    for (vector<int> line : parseCsv(hotspotsFile)) {
        Hotspot hotspot(line.at(0), line.at(1), line.at(2), line.at(3));
        hotspots.push_back(hotspot);
    }

    initializeRanges();

    initializeHeatmap(currentHeatmap);
    initializeHeatmap(oldHeatmap);
    setHotspots();

    while (currentRound < numberOfRounds) {
        performRound();
    }

    writeOutput();

    exit(0);
}
