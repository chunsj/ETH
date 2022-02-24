#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/opencl.h>

#include <sys/time.h>
#include <time.h>

double
myGetTimestamp (void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_usec + tv.tv_sec*1e6;
}

#define MAX_NUM_PLATFORMS 16
#define SZ_PLATFORM_VENDOR 1024
#define PLATFORM_VENDOR_XILINX "Xilinx"
#define MAX_NUM_DEVICES 64
#define SZ_DEVICE_NAME 1024

#define HOST_VERBOSE 0

static int
loadFileToMemory (const char *filename, char **bytes) {
  uint size = 0;
  FILE *pf = fopen(filename, "rb");
  if (!pf) {
    *bytes = NULL;
    return -1;
  }

  fseek(pf, 0, SEEK_END);
  size = ftell(pf);

  fseek(pf, 0, SEEK_SET);
  *bytes = (char *)malloc(size);
  if (!*bytes) {
    *bytes = NULL;
    return -2;
  }

  if (size != fread(*bytes, sizeof(char), size, pf)) {
    free(*bytes);
    return -3;
  }

  fclose(pf);

  //(*bytes)[size] = 0;

  return size;
}

int
myFindXillinxPlatform (cl_platform_id *platform_id_ptr) {
  cl_platform_id platforms[MAX_NUM_PLATFORMS];
  cl_platform_id xilinx_platform_id = NULL;
  unsigned int platform_count = 0;
  int err = 0;

  err = clGetPlatformIDs(MAX_NUM_PLATFORMS, platforms, &platform_count);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clGetPlatformIDs: %d\n", err);
    return err;
  }

  if (HOST_VERBOSE)
    printf("NUMBER OF PLATFORMS FOUND: %d\n", platform_count);

  for (unsigned int i = 0; i < platform_count; i++) {
    char platform_vendor[SZ_PLATFORM_VENDOR];
    err = clGetPlatformInfo(platforms[i], CL_PLATFORM_VENDOR, SZ_PLATFORM_VENDOR-1,
                            (void *)platform_vendor, NULL);
    if (err == CL_SUCCESS) {
      if (strcmp(platform_vendor, PLATFORM_VENDOR_XILINX) == 0) {
        xilinx_platform_id = platforms[i];
      }
    }
  }

  if (xilinx_platform_id) {
    *platform_id_ptr = xilinx_platform_id;
    if (HOST_VERBOSE)
      printf("XILINX PLATFORM FOUND\n");
    return CL_SUCCESS;
  }
  else {
    return -1;
  }
}

int
myGetFirstDevice (cl_platform_id platform_id, cl_device_id *device_id_ptr) {
  cl_device_id devices[MAX_NUM_DEVICES];
  char device_name[SZ_DEVICE_NAME];
  unsigned int device_count = 0;
  int err;

  err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ACCELERATOR, MAX_NUM_DEVICES,
                       devices, &device_count);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clGetDeviceIDs: %d\n", err);
    return err;
  }

  if (!device_count) {
    fprintf(stderr, "NO DEVICE AVAIALABLE\n");
    return -1;
  }

  if (HOST_VERBOSE)
    printf("NUMBER OF DEVICES FOUND: %d\n", device_count);

  for (unsigned int i = 0; i < device_count; i++) {
    err = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, SZ_DEVICE_NAME-1, device_name, NULL);
    if (err == CL_SUCCESS) {
      if (HOST_VERBOSE)
        printf("DEVICE NAME[%d]: %s\n", i, device_name);
    }
  }

  cl_device_id xilinx_device = devices[0];
  *device_id_ptr = xilinx_device;

  return CL_SUCCESS;
}

int
myGetFirstXilinxDevice (cl_device_id *device_id_ptr) {
  cl_platform_id xilinx_platform_id = NULL;
  int err = 0;

  err = myFindXillinxPlatform(&xilinx_platform_id);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN myFindXilinxPlatform: %d\n", err);
    return -1;
  }

  if (!xilinx_platform_id) {
    printf("XILINX PLATFORM NOT FOUND\n");
    return -1;
  }

  cl_device_id xilinx_device = NULL;
  err = myGetFirstDevice(xilinx_platform_id, &xilinx_device);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN myGetFirstDevice: %d\n", err);
    return -1;
  }

  *device_id_ptr = xilinx_device;

  return err;
}

cl_context
myCreateContext (cl_device_id *device_id_ptr, int *err_ptr) {
  cl_context context;
  //cl_context_properties context_properties = 0;
  uint num_devices = 1;

  context = clCreateContext(NULL, num_devices, device_id_ptr, NULL, NULL, err_ptr);
  if (*err_ptr != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clCreateContext: %d\n", *err_ptr);
    return NULL;
  }

  return context;
}

cl_command_queue
myCreateCommandQueue (cl_context context, cl_device_id device_id, int *err_ptr) {
  cl_command_queue command_queue;
  cl_command_queue_properties command_queue_properties = 0;

  command_queue = clCreateCommandQueue(context, device_id, command_queue_properties, err_ptr);
  if (*err_ptr != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clCreateCommandQueue: %d\n", *err_ptr);
    return NULL;
  }

  return command_queue;
}

cl_program
myCreateProgram (cl_context context, cl_device_id *device_id_ptr, const char *path, int *err_ptr) {
  unsigned char *kernel_binary = NULL;

  int sz = loadFileToMemory(path, (char **)&kernel_binary);
  if (sz <= 0) {
    fprintf(stderr, "ERROR IN myLoadFileToMemory: %d\n", sz);
    return NULL;
  }

  size_t binary_size = sz;
  int binary_status = 0;
  int num_devices = 1;

  cl_program program = clCreateProgramWithBinary(context,
                                                 num_devices,
                                                 device_id_ptr,
                                                 &binary_size,
                                                 (const unsigned char**)&kernel_binary,
                                                 &binary_status,
                                                 err_ptr);
  if (*err_ptr != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clCreateProgramWithBinary: %d\n", *err_ptr);
    return NULL;
  }

  return program;
}

int
myOpenCLMain (const char *path, int (*userMain) (cl_context, cl_command_queue, cl_program)) {
  cl_device_id xilinx_device = NULL;
  int err = myGetFirstXilinxDevice(&xilinx_device);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN myGetFirstXilinxDevice: %d\n", err);
    return -1;
  }

  cl_context context = myCreateContext(&xilinx_device, &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN myCreateContext: %d\n", err);
    return -1;
  }
  else {
    if (HOST_VERBOSE)
      printf("CONTEXT HAS BEEN CREATED\n");
  }

  cl_command_queue command_queue = myCreateCommandQueue(context, xilinx_device, &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN myCreateCommandQueue: %d\n", err);
    return -1;
  }
  else {
    if (HOST_VERBOSE)
      printf("COMMAND QUEUE HAS BEEN CREATED: %p\n", (unsigned int *)command_queue);
  }

  cl_program program = myCreateProgram(context,
                                       &xilinx_device,
                                       path,
                                       &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN myCreateProgram: %d\n", err);
    return -1;
  }
  else {
    if (HOST_VERBOSE)
      printf("PROGRAM HAS BEEN CREATED: %p\n", (unsigned int *)program);
  }

  int isOK = userMain(context, command_queue, program);

  clReleaseProgram(program);
  clReleaseCommandQueue(command_queue);
  clReleaseContext(context);

  return isOK;
}
