#include "opencl.hpp"
#include <fstream>
#include <iostream>
#include <numeric>
#include "base64.hpp"
#include "timing.hpp"

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

cl::Program::Binaries readBinaries(string fileName)
{
    FILE * pFile;
    long lSize;
    char * buffer;
    size_t result;

    pFile = fopen ( fileName.c_str() , "rb" );
    if (pFile==NULL)
    {
        fputs ("File error",stderr);
        exit (1);
    }

    // obtain file size:
    fseek (pFile , 0 , SEEK_END);
    lSize = ftell (pFile);
    rewind (pFile);

    // allocate memory to contain the whole file:
    buffer = (char*) malloc (sizeof(char)*lSize);
    if (buffer == NULL)
    {
        fputs ("Memory error",stderr);
        exit (2);
    }

    // copy the file into the buffer:
    result = fread (buffer,1,lSize,pFile);
    if (result != lSize)
    {
        fputs ("Reading error",stderr);
        exit (3);
    }

    /* the whole file is now loaded in the memory buffer. */

    // terminate
    fclose (pFile);

    cl::Program::Binaries binaries;
    binaries.push_back( {buffer, sizeof(char)*lSize});
    return binaries;
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
    if (platform.getDevices(deviceType, &all_devices) != CL_SUCCESS)
    {
        cout << "Preferred device type not found!\n";
        platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);
    }
    return all_devices;
}

cl::Device findFirstDeviceOfType(cl_device_type deviceType)
{
    vector<cl::Device> all_devices;
    for (cl::Platform platform : getPlatforms())
    {
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

void writeBinaries(cl::Program program, string filename)
{
    const vector<unsigned long> binSizes = program.getInfo<CL_PROGRAM_BINARY_SIZES>();
    vector<char> binData (accumulate(binSizes.begin(),binSizes.end(),0));
    char* binChunk = &binData[0];

    //A list of pointers to the binary data
    vector<char*> binaries;
    for(unsigned int i = 0; i<binSizes.size(); ++i)
    {
        binaries.push_back(binChunk) ;
        binChunk += binSizes[i] ;
    }

    program.getInfo(CL_PROGRAM_BINARIES , &binaries[0] ) ;
    ofstream binaryfile(filename.c_str(), ios::binary);
    for (unsigned int i = 0; i < binaries.size(); ++i)
        binaryfile.write(binaries[i], binSizes[i]);
}

void printBinariesBase64(cl::Program program)
{
    writeBinaries(program, "tmp.bin");
    string binaries = base64_encode_file("tmp.bin");
    cout << binaries << "\n";
}

bool doesFileExist(string fileName)
{
    ifstream ifile(fileName.c_str());
    return ifile;
}

cl::Program loadProgram(cl::Device device, cl::Context context)
{
    string deviceName = device.getInfo<CL_DEVICE_NAME>();
    string binFileName = deviceName + ".bin";
    bool found = false;
    cl::Program program;

    if (doesFileExist(binFileName))
    {
        found = true;
        cl::Program::Binaries binaries = readBinaries(binFileName);
        program = cl::Program(context, {device}, binaries);
    }
    else
    {
        cl::Program::Sources sources = readSources("kernel.cl");
        program = cl::Program(context, sources);
    }

    timeval start = startTiming();
    if(program.build({device}) != CL_SUCCESS)
    {
        std::cout<<" Error building: "<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device)<<"\n";
        exit(1);
    }
    cout << "Runtime for build: " << getElapsedSec(start) << "\n";

    if (!found)
    {
        writeBinaries(program, binFileName);
        //printBinariesBase64(program);
    }
    return program;
}
