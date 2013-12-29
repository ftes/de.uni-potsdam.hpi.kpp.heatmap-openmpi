#include <iostream>
#include <fstream>
#include "cl.hpp"

#include "Hotspot.h"
#include "Coordinate.h"

/*int width;
int height;
int numberOfRounds;
int currentRound = 0;
bool writeCoords = false;
vector<Coordinate> coords;
vector<Hotspot> hotspots;
vector<vector<vector<Coordinate>>> *ranges;

string outFile = "output.txt";*/

using namespace std;


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

int main()
{
    //TODO set hotspots


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

    int width = 2;
    int height = 2;
    float data[] = {0,1,0,1};
    unsigned int hotspotsStartData[] = {0, 0, 0, 0};
    unsigned int hotspotsEndData[] = {2,0,0,0};

    // Create an OpenCL Image for the hotspots, shall be copied to device
    cl::Image2D hotspotsStart = cl::Image2D(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        cl::ImageFormat(CL_R, CL_UNSIGNED_INT32), width, height, 0, hotspotsStartData);

    // Create an OpenCL Image for the hotspots, shall be copied to device
    cl::Image2D hotspotsEnd = cl::Image2D(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        cl::ImageFormat(CL_R, CL_UNSIGNED_INT32), width, height, 0, hotspotsEndData);

    // Create an OpenCL Image for the input data, shall be copied to device
    cl::Image2D *in = new cl::Image2D(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        cl::ImageFormat(CL_R, CL_FLOAT), width, height, 0, data);

    // Create an OpenCL Image for the output data
    cl::Image2D *out = new cl::Image2D(context, CL_MEM_READ_WRITE,
        cl::ImageFormat(CL_R, CL_FLOAT), width, height, 0);

    //create queue to which we will push commands for the device.
    cl::CommandQueue queue(context, device);

    //push kernel to the device, with the buffers as parameter values
    auto kernel = cl::make_kernel<unsigned int, cl::Image2D, cl::Image2D, cl::Image2D, cl::Image2D>(program, "heatmap");
    //ranges: global offset, global (global number of work items), local (number of work items per work group)
    cl::EnqueueArgs eargs(queue,cl::NullRange,cl::NDRange(width, height), cl::NullRange);
    kernel(eargs, 1, hotspotsStart, hotspotsEnd, *in, *out).wait();

    //read output image back from device
    cl::size_t<3> origin, region;
    origin[0] = 0; origin[1] = 0; origin[2] = 0;
    region[0] = width; region[1] = height; region[2] = 1;
    queue.enqueueReadImage(out, true, origin, region, 0, 0, data);

    cout << data[0] << "\n";

    return 0;
}
