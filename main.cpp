#include <iostream>
#include <fstream>
#include "cl.hpp"

using namespace std;

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

int main()
{
    //listDevices();

    //get device
    cl::Device device = findFirstDeviceOfType(CL_DEVICE_TYPE_GPU);
    cout << "Using device: " << device.getInfo<CL_DEVICE_NAME>() << "\n";

    //get
    cl::Context context = getContext(device);

    cl::Program::Sources sources = readSources("kernel.cl");

    cl::Program program(context,sources);
    if(program.build( {device})!=CL_SUCCESS)
    {
        std::cout<<" Error building: "<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device)<<"\n";
        exit(1);
    }


    // create buffers on the device
    cl::Buffer buffer_A(context,CL_MEM_READ_ONLY,sizeof(int)*10);
    cl::Buffer buffer_B(context,CL_MEM_READ_ONLY,sizeof(int)*10);
    cl::Buffer buffer_C(context,CL_MEM_WRITE_ONLY,sizeof(int)*10);

    int A[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int B[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0};

    //create queue to which we will push commands for the device.
    cl::CommandQueue queue(context, device);

    //write arrays A and B to the device
    queue.enqueueWriteBuffer(buffer_A,CL_TRUE,0,sizeof(int)*10,A);
    queue.enqueueWriteBuffer(buffer_B,CL_TRUE,0,sizeof(int)*10,B);

    auto simple_add = cl::make_kernel<cl::Buffer&, cl::Buffer&, cl::Buffer&>(program,"simple_add");
    cl::EnqueueArgs eargs(queue,cl::NullRange,cl::NDRange(10),cl::NullRange);
    simple_add(eargs, buffer_A,buffer_B,buffer_C).wait();

    int C[10];
    //read result C from the device to array C
    queue.enqueueReadBuffer(buffer_C,CL_TRUE,0,sizeof(int)*10,C);

    cout<<" result: \n";
    for(int i=0; i<10; i++)
    {
        cout<<C[i]<<" ";
    }
    cout << "\n";

    return 0;
}
