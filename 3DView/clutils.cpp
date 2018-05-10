#include"clutils.h"
#include<string.h>
#include<string>
#include<cl/opencl.h>

static int ConvertSMVer2Cores(int major,int minor){
	// Defines for GPU Architecture types (using the SM version to determine the # of cores per SM
	typedef struct{
		int SM; // 0xMm (hexidecimal notation),M = SM Major version,and m = SM minor version
		int Cores;
	} sSMtoCores;

	sSMtoCores nGpuArchCoresPerSM[]={
		{ 0x10,8  },// Tesla Generation (SM 1.0) G80 class
		{ 0x11,8  },// Tesla Generation (SM 1.1) G8x class
		{ 0x12,8  },// Tesla Generation (SM 1.2) G9x class
		{ 0x13,8  },// Tesla Generation (SM 1.3) GT200 class
		{ 0x20,32 },// Fermi Generation (SM 2.0) GF100 class
		{ 0x21,48 },// Fermi Generation (SM 2.1) GF10x class
		{ 0x30,192},// Fermi Generation (SM 3.0) GK10x class
		{   -1,-1 }
	};

	int index = 0;
	while(nGpuArchCoresPerSM[index].SM!=-1){
		if(nGpuArchCoresPerSM[index].SM==((major<<4)+minor)){
			return nGpuArchCoresPerSM[index].Cores;
		}
		index++;
	}
	printf("MapSMtoCores SM %d.%d is undefined (please update to the latest SDK)!\n",major,minor);
	return -1;
}

cl_platform_id GetPlatformID(){
	cl_uint nplat=0;
	cl_int clerr=clGetPlatformIDs(0,NULL,&nplat);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clGetPlatformIDs Call !!!\n\n",clerr);
		return NULL;
	}

	if(nplat==0){
		printf("No OpenCL platform found!\n\n");
		return NULL;
	}

	cl_platform_id *clpids=(cl_platform_id*)malloc(nplat*sizeof(cl_platform_id));
	// if there's a platform or more,make space for ID's
	if(clpids==NULL){
		printf("Failed to allocate memory for cl_platform ID's!\n\n");
		return NULL;
	}
	
	clerr=clGetPlatformIDs(nplat,clpids,NULL);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clGetPlatformIDs Call !!!\n\n",clerr);
		free(clpids);
		return NULL;
	}

	char chBuffer[1024];
	cl_platform_id clpid=NULL;
	// get platform info for each platform and trap the NVIDIA platform if found
	for(cl_uint i=0;i<nplat;++i){
		clerr=clGetPlatformInfo(clpids[i],CL_PLATFORM_NAME,1024,&chBuffer,NULL);
		if(clerr==CL_SUCCESS){
			if(strstr(chBuffer,"NVIDIA")!=NULL){
				clpid=clpids[i];
				break;
			}
		}
	}
	
	// default to zeroeth platform if NVIDIA not found
	if(clpid==NULL){
		printf("WARNING: NVIDIA OpenCL platform not found - defaulting to first platform!\n\n");
		clpid=clpids[0];
	}
	
	free(clpids);

	return clpid;
}

void PrintDevInfo(cl_device_id device){
	char device_string[1024];
	bool nv_device_attibute_query=false;

	// CL_DEVICE_NAME
	clGetDeviceInfo(device,CL_DEVICE_NAME,sizeof(device_string),&device_string,NULL);
	printf("  CL_DEVICE_NAME: \t\t\t%s\n",device_string);

	// CL_DEVICE_VENDOR
	clGetDeviceInfo(device,CL_DEVICE_VENDOR,sizeof(device_string),&device_string,NULL);
	printf("  CL_DEVICE_VENDOR: \t\t\t%s\n",device_string);

	// CL_DRIVER_VERSION
	clGetDeviceInfo(device,CL_DRIVER_VERSION,sizeof(device_string),&device_string,NULL);
	printf("  CL_DRIVER_VERSION: \t\t\t%s\n",device_string);

	// CL_DEVICE_VERSION
	clGetDeviceInfo(device,CL_DEVICE_VERSION,sizeof(device_string),&device_string,NULL);
	printf("  CL_DEVICE_VERSION: \t\t\t%s\n",device_string);

#if !defined(__APPLE__) && !defined(__MACOSX)
	// CL_DEVICE_OPENCL_C_VERSION (if CL_DEVICE_VERSION version > 1.0)
	if(strncmp("OpenCL 1.0",device_string,10) != 0){
		// This code is unused for devices reporting OpenCL 1.0,but a def is needed anyway to allow compilation using v 1.0 headers 
		// This constant isn't #defined in 1.0
#ifndef CL_DEVICE_OPENCL_C_VERSION
#define CL_DEVICE_OPENCL_C_VERSION 0x103D   
#endif
		clGetDeviceInfo(device,CL_DEVICE_OPENCL_C_VERSION,sizeof(device_string),&device_string,NULL);
		printf("  CL_DEVICE_OPENCL_C_VERSION: \t\t%s\n",device_string);
	}
#endif

	// CL_DEVICE_TYPE
	cl_device_type type;
	clGetDeviceInfo(device,CL_DEVICE_TYPE,sizeof(type),&type,NULL);
	if( type & CL_DEVICE_TYPE_CPU )
		printf("  CL_DEVICE_TYPE:\t\t\t%s\n","CL_DEVICE_TYPE_CPU");
	if( type & CL_DEVICE_TYPE_GPU )
		printf("  CL_DEVICE_TYPE:\t\t\t%s\n","CL_DEVICE_TYPE_GPU");
	if( type & CL_DEVICE_TYPE_ACCELERATOR )
		printf("  CL_DEVICE_TYPE:\t\t\t%s\n","CL_DEVICE_TYPE_ACCELERATOR");
	if( type & CL_DEVICE_TYPE_DEFAULT )
		printf("  CL_DEVICE_TYPE:\t\t\t%s\n","CL_DEVICE_TYPE_DEFAULT");
	
	// CL_DEVICE_MAX_COMPUTE_UNITS
	cl_uint compute_units;
	clGetDeviceInfo(device,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(compute_units),&compute_units,NULL);
	printf("  CL_DEVICE_MAX_COMPUTE_UNITS:\t\t%u\n",compute_units);

	// CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS
	size_t workitem_dims;
	clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,sizeof(workitem_dims),&workitem_dims,NULL);
	printf("  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS:\t%u\n",workitem_dims);

	// CL_DEVICE_MAX_WORK_ITEM_SIZES
	size_t workitem_size[3];
	clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_ITEM_SIZES,sizeof(workitem_size),&workitem_size,NULL);
	printf("  CL_DEVICE_MAX_WORK_ITEM_SIZES:\t%u / %u / %u \n",workitem_size[0],workitem_size[1],workitem_size[2]);
	
	// CL_DEVICE_MAX_WORK_GROUP_SIZE
	size_t workgroup_size;
	clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_GROUP_SIZE,sizeof(workgroup_size),&workgroup_size,NULL);
	printf("  CL_DEVICE_MAX_WORK_GROUP_SIZE:\t%u\n",workgroup_size);

	// CL_DEVICE_MAX_CLOCK_FREQUENCY
	cl_uint clock_frequency;
	clGetDeviceInfo(device,CL_DEVICE_MAX_CLOCK_FREQUENCY,sizeof(clock_frequency),&clock_frequency,NULL);
	printf("  CL_DEVICE_MAX_CLOCK_FREQUENCY:\t%u MHz\n",clock_frequency);

	// CL_DEVICE_ADDRESS_BITS
	cl_uint addr_bits;
	clGetDeviceInfo(device,CL_DEVICE_ADDRESS_BITS,sizeof(addr_bits),&addr_bits,NULL);
	printf("  CL_DEVICE_ADDRESS_BITS:\t\t%u\n",addr_bits);

	// CL_DEVICE_MAX_MEM_ALLOC_SIZE
	cl_ulong max_mem_alloc_size;
	clGetDeviceInfo(device,CL_DEVICE_MAX_MEM_ALLOC_SIZE,sizeof(max_mem_alloc_size),&max_mem_alloc_size,NULL);
	printf("  CL_DEVICE_MAX_MEM_ALLOC_SIZE:\t\t%u MByte\n",(unsigned int)(max_mem_alloc_size / (1024 * 1024)));

	// CL_DEVICE_GLOBAL_MEM_SIZE
	cl_ulong mem_size;
	clGetDeviceInfo(device,CL_DEVICE_GLOBAL_MEM_SIZE,sizeof(mem_size),&mem_size,NULL);
	printf("  CL_DEVICE_GLOBAL_MEM_SIZE:\t\t%u MByte\n",(unsigned int)(mem_size / (1024 * 1024)));

	// CL_DEVICE_ERROR_CORRECTION_SUPPORT
	cl_bool error_correction_support;
	clGetDeviceInfo(device,CL_DEVICE_ERROR_CORRECTION_SUPPORT,sizeof(error_correction_support),&error_correction_support,NULL);
	printf("  CL_DEVICE_ERROR_CORRECTION_SUPPORT:\t%s\n",error_correction_support==CL_TRUE?"yes":"no");

	// CL_DEVICE_LOCAL_MEM_TYPE
	cl_device_local_mem_type local_mem_type;
	clGetDeviceInfo(device,CL_DEVICE_LOCAL_MEM_TYPE,sizeof(local_mem_type),&local_mem_type,NULL);
	printf("  CL_DEVICE_LOCAL_MEM_TYPE:\t\t%s\n",local_mem_type==1?"local":"global");

	// CL_DEVICE_LOCAL_MEM_SIZE
	clGetDeviceInfo(device,CL_DEVICE_LOCAL_MEM_SIZE,sizeof(mem_size),&mem_size,NULL);
	printf("  CL_DEVICE_LOCAL_MEM_SIZE:\t\t%u KByte\n",(unsigned int)(mem_size / 1024));

	// CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE
	clGetDeviceInfo(device,CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,sizeof(mem_size),&mem_size,NULL);
	printf("  CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE:\t%u KByte\n",(unsigned int)(mem_size / 1024));

	// CL_DEVICE_QUEUE_PROPERTIES
	cl_command_queue_properties queue_properties;
	clGetDeviceInfo(device,CL_DEVICE_QUEUE_PROPERTIES,sizeof(queue_properties),&queue_properties,NULL);
	if( queue_properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE )
		printf("  CL_DEVICE_QUEUE_PROPERTIES:\t\t%s\n","CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE");	
	if( queue_properties & CL_QUEUE_PROFILING_ENABLE )
		printf("  CL_DEVICE_QUEUE_PROPERTIES:\t\t%s\n","CL_QUEUE_PROFILING_ENABLE");

	// CL_DEVICE_IMAGE_SUPPORT
	cl_bool image_support;
	clGetDeviceInfo(device,CL_DEVICE_IMAGE_SUPPORT,sizeof(image_support),&image_support,NULL);
	printf("  CL_DEVICE_IMAGE_SUPPORT:\t\t%u\n",image_support);

	// CL_DEVICE_MAX_READ_IMAGE_ARGS
	cl_uint max_read_image_args;
	clGetDeviceInfo(device,CL_DEVICE_MAX_READ_IMAGE_ARGS,sizeof(max_read_image_args),&max_read_image_args,NULL);
	printf("  CL_DEVICE_MAX_READ_IMAGE_ARGS:\t%u\n",max_read_image_args);

	// CL_DEVICE_MAX_WRITE_IMAGE_ARGS
	cl_uint max_write_image_args;
	clGetDeviceInfo(device,CL_DEVICE_MAX_WRITE_IMAGE_ARGS,sizeof(max_write_image_args),&max_write_image_args,NULL);
	printf("  CL_DEVICE_MAX_WRITE_IMAGE_ARGS:\t%u\n",max_write_image_args);

	// CL_DEVICE_SINGLE_FP_CONFIG
	cl_device_fp_config fp_config;
	clGetDeviceInfo(device,CL_DEVICE_SINGLE_FP_CONFIG,sizeof(cl_device_fp_config),&fp_config,NULL);
	printf("  CL_DEVICE_SINGLE_FP_CONFIG:\t\t%s%s%s%s%s%s\n",
		fp_config & CL_FP_DENORM?"denorms ":"",
		fp_config & CL_FP_INF_NAN?"INF-quietNaNs ":"",
		fp_config & CL_FP_ROUND_TO_NEAREST?"round-to-nearest ":"",
		fp_config & CL_FP_ROUND_TO_ZERO?"round-to-zero ":"",
		fp_config & CL_FP_ROUND_TO_INF?"round-to-inf ":"",
		fp_config & CL_FP_FMA?"fma ":"");
	
	// CL_DEVICE_IMAGE2D_MAX_WIDTH,CL_DEVICE_IMAGE2D_MAX_HEIGHT,CL_DEVICE_IMAGE3D_MAX_WIDTH,CL_DEVICE_IMAGE3D_MAX_HEIGHT,CL_DEVICE_IMAGE3D_MAX_DEPTH
	size_t szMaxDims[5];
	printf("\n  CL_DEVICE_IMAGE <dim>"); 
	clGetDeviceInfo(device,CL_DEVICE_IMAGE2D_MAX_WIDTH,sizeof(size_t),&szMaxDims[0],NULL);
	printf("\t\t\t2D_MAX_WIDTH\t %u\n",szMaxDims[0]);
	clGetDeviceInfo(device,CL_DEVICE_IMAGE2D_MAX_HEIGHT,sizeof(size_t),&szMaxDims[1],NULL);
	printf("\t\t\t\t\t2D_MAX_HEIGHT\t %u\n",szMaxDims[1]);
	clGetDeviceInfo(device,CL_DEVICE_IMAGE3D_MAX_WIDTH,sizeof(size_t),&szMaxDims[2],NULL);
	printf("\t\t\t\t\t3D_MAX_WIDTH\t %u\n",szMaxDims[2]);
	clGetDeviceInfo(device,CL_DEVICE_IMAGE3D_MAX_HEIGHT,sizeof(size_t),&szMaxDims[3],NULL);
	printf("\t\t\t\t\t3D_MAX_HEIGHT\t %u\n",szMaxDims[3]);
	clGetDeviceInfo(device,CL_DEVICE_IMAGE3D_MAX_DEPTH,sizeof(size_t),&szMaxDims[4],NULL);
	printf("\t\t\t\t\t3D_MAX_DEPTH\t %u\n",szMaxDims[4]);

	// CL_DEVICE_EXTENSIONS: get device extensions,and if any then parse & log the string onto separate lines
	clGetDeviceInfo(device,CL_DEVICE_EXTENSIONS,sizeof(device_string),&device_string,NULL);
	if (device_string != 0){
		printf("\n  CL_DEVICE_EXTENSIONS:");
		std::string stdDevString;
		stdDevString = std::string(device_string);
		size_t szOldPos = 0;
		size_t szSpacePos = stdDevString.find(' ',szOldPos); // extensions string is space delimited
		while (szSpacePos != stdDevString.npos){
			if( strcmp("cl_nv_device_attribute_query",stdDevString.substr(szOldPos,szSpacePos - szOldPos).c_str())==0 )
				nv_device_attibute_query = true;

			if (szOldPos > 0){
				printf("\t\t");
			}
			printf("\t\t\t%s\n",stdDevString.substr(szOldPos,szSpacePos - szOldPos).c_str());
			
			do{
				szOldPos = szSpacePos + 1;
				szSpacePos = stdDevString.find(' ',szOldPos);
			} while (szSpacePos==szOldPos);
		}
		printf("\n");
	}
	else{
		printf("  CL_DEVICE_EXTENSIONS: None\n");
	}

	if(nv_device_attibute_query){
		cl_uint compute_capability_major,compute_capability_minor;
		clGetDeviceInfo(device,CL_DEVICE_COMPUTE_CAPABILITY_MAJOR_NV,sizeof(cl_uint),&compute_capability_major,NULL);
		clGetDeviceInfo(device,CL_DEVICE_COMPUTE_CAPABILITY_MINOR_NV,sizeof(cl_uint),&compute_capability_minor,NULL);
		printf("\n  CL_DEVICE_COMPUTE_CAPABILITY_NV:\t%u.%u\n",compute_capability_major,compute_capability_minor);		

		printf("  NUMBER OF MULTIPROCESSORS:\t\t%u\n",compute_units); // this is the same value reported by CL_DEVICE_MAX_COMPUTE_UNITS
		printf("  NUMBER OF CUDA CORES:\t\t\t%u\n",ConvertSMVer2Cores(compute_capability_major,compute_capability_minor)*compute_units);

		cl_uint regs_per_block;
		clGetDeviceInfo(device,CL_DEVICE_REGISTERS_PER_BLOCK_NV,sizeof(cl_uint),&regs_per_block,NULL);
		printf("  CL_DEVICE_REGISTERS_PER_BLOCK_NV:\t%u\n",regs_per_block);		

		cl_uint warp_size;
		clGetDeviceInfo(device,CL_DEVICE_WARP_SIZE_NV,sizeof(cl_uint),&warp_size,NULL);
		printf("  CL_DEVICE_WARP_SIZE_NV:\t\t%u\n",warp_size);		

		cl_bool gpu_overlap;
		clGetDeviceInfo(device,CL_DEVICE_GPU_OVERLAP_NV,sizeof(cl_bool),&gpu_overlap,NULL);
		printf("  CL_DEVICE_GPU_OVERLAP_NV:\t\t%s\n",gpu_overlap==CL_TRUE?"CL_TRUE":"CL_FALSE");		

		cl_bool exec_timeout;
		clGetDeviceInfo(device,CL_DEVICE_KERNEL_EXEC_TIMEOUT_NV,sizeof(cl_bool),&exec_timeout,NULL);
		printf("  CL_DEVICE_KERNEL_EXEC_TIMEOUT_NV:\t%s\n",exec_timeout==CL_TRUE?"CL_TRUE":"CL_FALSE");		

		cl_bool integrated_memory;
		clGetDeviceInfo(device,CL_DEVICE_INTEGRATED_MEMORY_NV,sizeof(cl_bool),&integrated_memory,NULL);
		printf("  CL_DEVICE_INTEGRATED_MEMORY_NV:\t%s\n",integrated_memory==CL_TRUE?"CL_TRUE":"CL_FALSE");		
	}

	// CL_DEVICE_PREFERRED_VECTOR_WIDTH_<type>
	printf("  CL_DEVICE_PREFERRED_VECTOR_WIDTH_<t>\t"); 
	cl_uint vec_width [6];
	clGetDeviceInfo(device,CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,  sizeof(cl_uint),&vec_width[0],NULL);
	clGetDeviceInfo(device,CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, sizeof(cl_uint),&vec_width[1],NULL);
	clGetDeviceInfo(device,CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,   sizeof(cl_uint),&vec_width[2],NULL);
	clGetDeviceInfo(device,CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,  sizeof(cl_uint),&vec_width[3],NULL);
	clGetDeviceInfo(device,CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, sizeof(cl_uint),&vec_width[4],NULL);
	clGetDeviceInfo(device,CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE,sizeof(cl_uint),&vec_width[5],NULL);
	printf("CHAR %u,SHORT %u,INT %u,LONG %u,FLOAT %u,DOUBLE %u\n\n\n",
		   vec_width[0],vec_width[1],vec_width[2],vec_width[3],vec_width[4],vec_width[5]); 
}

cl_device_id GetDeviceID(){
	cl_platform_id clpid=GetPlatformID();
	if(!clpid)return NULL;

	//Get all the devices
	cl_int clerr;
	cl_uint ndev=0;
	clerr=clGetDeviceIDs(clpid,CL_DEVICE_TYPE_GPU,0,NULL,&ndev);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clGetDeviceIDs Call !!!\n\n",clerr);
		return NULL;
	}
	if(!ndev){
		printf("No OpenCL device found!\n\n");
		return NULL;
	}
	cl_device_id *cldids=(cl_device_id*)malloc(ndev*sizeof(cl_device_id));
	if(!cldids){
		printf("Failed to allocate memory for cl_device ID's!\n\n");
		return NULL;
	}

	clerr=clGetDeviceIDs(clpid,CL_DEVICE_TYPE_GPU,ndev,cldids,NULL);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clGetDeviceIDs Call !!!\n\n",clerr);
		free(cldids);
		return NULL;
	}
	
	//select a device
	cl_device_id cldid=cldids[0];
	PrintDevInfo(cldid);
	return cldid;
}

size_t CreateKernel(const char *srcfile,cl_context clctx,cl_device_id cldev,const char *kernel_name,cl_kernel *kernel){
    // Read the kernel in from file
	FILE *src=fopen(srcfile,"rb");
	if(!src){
		printf(" Cannot open srcfile.\n\n");
		return 0;
	}

    _fseeki64(src,0,SEEK_END); 
    size_t lensrc=_ftelli64(src);
    _fseeki64(src,0,SEEK_SET); 
	
    // allocate a buffer for the source code string and read it in
    char *strsrc=(char*)malloc(lensrc+1);
	if(!strsrc){
		printf(" bad alloc.\n\n");
		return 0;
	}
    if(fread(strsrc,lensrc,1,src)!=1){
		printf(" Cannot read srcfile.\n\n");
		fclose(src);
		free(strsrc);
		return 0;
	}
    strsrc[lensrc]=0;
	fclose(src);
	
    // create the program
    cl_int clerr=CL_SUCCESS;
	cl_program cpProgram=clCreateProgramWithSource(clctx,1,(const char **)&strsrc,&lensrc,&clerr);
	if(clerr!=CL_SUCCESS){
		printf(" Error %i in clCreateProgramWithSource Call !!!\n\n",clerr);
		free(strsrc);
		return 0;
	}

	char *flags="-cl-fast-relaxed-math";
    clerr=clBuildProgram(cpProgram,0,NULL,flags,NULL,NULL);
    if(clerr!=CL_SUCCESS){
		printf(" Error %i in clBuildProgram Call !!!\n\n",clerr);
		free(strsrc);
		size_t loglen;
		clGetProgramBuildInfo(cpProgram,cldev,CL_PROGRAM_BUILD_LOG,0,NULL,&loglen);
		char *logstr=(char*)malloc(loglen);
		clGetProgramBuildInfo(cpProgram,cldev,CL_PROGRAM_BUILD_LOG,loglen,logstr,NULL);
		printf("%s\n",logstr);
		free(logstr);
		return 0;
    }

    // create the kernel
    *kernel=clCreateKernel(cpProgram,kernel_name,&clerr);
    if(clerr!=CL_SUCCESS){
		printf(" Error %i in clCreateKernel Call !!!\n\n",clerr);
		free(strsrc);
		return 0;
    }

	size_t wgSize;
	clerr=clGetKernelWorkGroupInfo(*kernel,cldev,CL_KERNEL_WORK_GROUP_SIZE,sizeof(size_t),&wgSize,NULL);
    if(clerr!=CL_SUCCESS){
		printf(" Error %i in clGetKernelWorkGroupInfo Call !!!\n\n",clerr);
		free(strsrc);
		return 0;
    }

	free(strsrc);
	return wgSize;
}