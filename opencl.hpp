#ifndef MY_OPENCL
#define MY_OPENCL

#include <vector>
#include "cl.hpp"

using namespace std;

string readFile(string filename);
cl::Program::Sources readSources(string fileName);
cl::Program::Binaries readBinaries(string fileName);
vector<cl::Platform> getPlatforms();
vector<cl::Device> getDevices(cl::Platform platform, cl_device_type deviceType);
cl::Device findFirstDeviceOfType(cl_device_type deviceType);
cl::Context getContext(cl::Device device);
void listDevices();
void writeBinaries(cl::Program program, string filename);
void printBinariesBase64(cl::Program program);
cl::Program loadProgram(cl::Device decive, cl::Context context);

#endif
