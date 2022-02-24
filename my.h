#ifndef __MY_HOST_HEADER__
#define __MY_HOST_HEADER__ 1

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/opencl.h>

double myGetTimestamp (void);
int myOpenCLMain (const char *path, int (*userMain) (cl_context, cl_command_queue, cl_program));

#endif /* __MY_HOST_HEADER__ */
