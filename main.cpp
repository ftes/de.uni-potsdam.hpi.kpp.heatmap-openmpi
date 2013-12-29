#include <iostream>
#include <fstream>
#include "cl.hpp"
#include <sstream>
#include <math.h>

#include "Hotspot.h"
#include "Coordinate.h"

using namespace std;


string outFile = "output.txt";
int width, height;
bool writeCoords = false;
vector<Coordinate> coords;


// OpenCL functions
string readFile(string filename)
{
    ifstream file(filename.c_str());
    string *content = new string((istreambuf_iterator<char>(file)),
                                 istreambuf_iterator<char>());

    return *content;
}

cl::Program::Sources readSources(string fileName)
{
    string prog = readFile(fileName);
    cl::Program::Sources sources;
    sources.push_back( {prog.c_str(), prog.length()});
    return sources;
}

vector<cl::Platform> getPlatforms()
{
    vector<cl::Platform> all_platforms;
    cl::Platform::get(&all_platforms);
    return all_platforms;
}

vector<cl::Device> getDevices(cl::Platform platform, cl_device_type deviceType = CL_DEVICE_TYPE_ALL)
{
    vector<cl::Device> all_devices;
    if (platform.getDevices(deviceType, &all_devices) != CL_SUCCESS) {
        cout << "Preferred device type not found!\n";
        platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
    }
    return all_devices;
}

cl::Device findFirstDeviceOfType(cl_device_type deviceType) {
    vector<cl::Device> all_devices;
    for (cl::Platform platform : getPlatforms()) {
        if (platform.getDevices(deviceType, &all_devices) == CL_SUCCESS)
            return all_devices[0];
    }
    cout << "Preferred device type not found!\n";
    return getDevices(getPlatforms()[0])[0];
}

cl::Context getContext(cl::Device device)
{
    cl::Context context {device};
    return context;
}

void listDevices()
{
    for (cl::Platform platform : getPlatforms())
    {
        cout << " - Platform: " << platform.getInfo<CL_PLATFORM_NAME>() << "\n";
        for (cl::Device device : getDevices(platform))
        {
            cout << "   - " << "Device: " << device.getInfo<CL_DEVICE_NAME>() << "\n";
        }
    }
}






// Heatmap functions
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

void writeOutput(float data[]) {
    remove(outFile.c_str());
    ofstream output(outFile.c_str());
    if (!writeCoords) {
        for (int j=0; j<height; j++) {
            for (int i=0; i<width; i++) {
            	string out = getOutputValue(data[i + j * width]);
                output << out.c_str();

                if (i == width - 1) {
                	output << "\n";
                }
            }
        }
    } else {
        for (Coordinate coord : coords) {
            double cell = data[coord.x + coord.y * width];
            output << cell <<"\n";
        }
    }
    output.close();
}




int main(int argc, char* argv[])
{
    //TODO set hotspots
    //read input
    width = atoi(argv[1]);
    height = atoi(argv[2]);
    int numberOfRounds = atoi(argv[3]);

    string hotspotsFile = string(argv[4]);

    if (argc == 6) {
        string coordFile = string(argv[5]);
        for (vector<int> line : parseCsv(coordFile)) {
            Coordinate coord(line.at(0), line.at(1));
            coords.push_back(coord);
        }
        writeCoords = true;
    }

    unsigned int hotspotsStartData[width * height];
    unsigned int hotspotsEndData[width * height];
    float startData[width * height];

    //read hotspots
    for (vector<int> line : parseCsv(hotspotsFile)) {
        Hotspot hotspot(line.at(0), line.at(1), line.at(2), line.at(3));
        int index = hotspot.x + hotspot.y * width;
        hotspotsStartData[index] = hotspot.startRound;
        hotspotsEndData[index] = hotspot.endRound;
        if (hotspot.startRound == 0 && hotspot.endRound > 0) {
            startData[index] = 1.f;
        }
    }




    //OpenCL preparations
    //listDevices();

    //get device
    cl::Device device = findFirstDeviceOfType(CL_DEVICE_TYPE_GPU);
    cout << "Using device: " << device.getInfo<CL_DEVICE_NAME>() << "\n";

    //does device have image support?
    cl_bool result;
    device.getInfo(CL_DEVICE_IMAGE_SUPPORT, &result);
    if (! result)
        cout << "No image support!\n";

    //get context for communicating with device
    cl::Context context = getContext(device);

    //read kernel (program for device) from file
    cl::Program::Sources sources = readSources("kernel.cl");

    //compile the program for the device
    cl::Program program(context, sources);
    if(program.build( {device}) != CL_SUCCESS)
    {
        std::cout<<" Error building: "<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device)<<"\n";
        exit(1);
    }

    // Create an OpenCL Image for the hotspots, shall be copied to device
    cl::Image2D hotspotsStartImage = cl::Image2D(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        cl::ImageFormat(CL_R, CL_UNSIGNED_INT32), width, height, 0, hotspotsStartData);

    // Create an OpenCL Image for the hotspots, shall be copied to device
    cl::Image2D hotspotsEndImage = cl::Image2D(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        cl::ImageFormat(CL_R, CL_UNSIGNED_INT32), width, height, 0, hotspotsEndData);

    // Create an OpenCL Image for the input data, shall be copied to device
    cl::Image2D startImage = cl::Image2D(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        cl::ImageFormat(CL_R, CL_FLOAT), width, height, 0, startData);

    // Create an OpenCL Image for the output data
    cl::Image2D otherStartImage = cl::Image2D(context, CL_MEM_READ_WRITE,
        cl::ImageFormat(CL_R, CL_FLOAT), width, height, 0);

    //create queue to which we will push commands for the device.
    cl::CommandQueue queue(context, device);

    //push kernel to the device, with the buffers as parameter values
    auto kernel = cl::make_kernel<unsigned int, cl::Image2D, cl::Image2D, cl::Image2D, cl::Image2D>(program, "heatmap");
    //ranges: global offset, global (global number of work items), local (number of work items per work group)
    cl::EnqueueArgs eargs(queue,cl::NullRange,cl::NDRange(width, height), cl::NullRange);

    // assign "wrong way round" because they are swapped back again in the for loop
    cl::Image2D *oldHeatmapImage = &otherStartImage;
    cl::Image2D *newHeatmapImage = &startImage;



    //perform rounds
    for (int i=0; i<numberOfRounds; i++) {
        //swap heatmaps
        cl::Image2D *tmp = oldHeatmapImage;
        oldHeatmapImage = newHeatmapImage;
        newHeatmapImage = tmp;

        //run kernel
        kernel(eargs, 1, hotspotsStartImage, hotspotsEndImage, *oldHeatmapImage, *newHeatmapImage).wait();
    }

    //read output back from device
    cl::size_t<3> origin, region;
    origin[0] = 0; origin[1] = 0; origin[2] = 0;
    region[0] = width; region[1] = height; region[2] = 1;
    queue.enqueueReadImage(*newHeatmapImage, true, origin, region, 0, 0, startData);

    writeOutput(startData);

    exit(0);
}
