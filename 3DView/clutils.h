#pragma once
#pragma comment(lib,"opencl.lib")
#include<cl/opencl.h>

cl_platform_id GetPlatformID();
void PrintDevInfo(cl_device_id device);
cl_device_id GetDeviceID();
size_t CreateKernel(const char *srcfile,cl_context clctx,cl_device_id cldev,const char *kernel_name,cl_kernel *kernel);
