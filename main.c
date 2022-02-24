#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "my.h"

#define DATA_FILE_NAME "/home/Sungjin/Documents/FPGA/REFS/ETH/ETHASH-DATA/data"

#define HEADER_HASH_STRING "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
#define HEADER_HASH_STRING_LEN 64

#define RES_MIX "a3676e668d4a4a9de2d4688dfd9cc84c65464990fc58f2885e1b8f8bfd028b5a"
#define RES_HASH "a7ea1de3a8007134900cd2c86f7e55af68a1d3e4537438a0a966b6cbafa23c90"

int
myReadFileToMemory (const char *filename, uint8_t *bytes) {
  uint size = 0;
  FILE *pf = fopen(filename, "rb");
  if (!pf) {
    return -1;
  }

  fseek(pf, 0, SEEK_END);
  size = ftell(pf);

  fseek(pf, 0, SEEK_SET);

  if (size != fread(bytes, sizeof(uint8_t), size, pf)) {
    return -2;
  }

  fclose(pf);

  return size;
}

static char
nibbleToChar (unsigned nibble) {
  return (char)((nibble >= 10 ? 'a' - 10 : '0') + nibble);
}

static uint8_t
charToNibble (char ch) {
  if (ch >= '0' && ch <= '9')
    return (uint8_t)(ch - '0');
  if (ch >= 'a' && ch <= 'z')
    return (uint8_t)(ch - 'a' + 10);
  if (ch >= 'A' && ch <= 'Z')
    return (uint8_t)(ch - 'A' + 10);

  return 0;
}

static void
hexStringToBytes (const char *pszString, uint length, uint8_t *bytes) {
  uint size = length >> 1;
  for (uint i = 0; i != size; i++) {
    bytes[i] = charToNibble(pszString[i * 2 | 0]) << 4;
    bytes[i] |= charToNibble(pszString[i * 2 | 1]);
  }
}

static void
bytesToHexString (const uint8_t *bytes, uint size, char *pszString) {
  uint idx = 0;
  for (uint i = 0; i != size; i++) {
    pszString[idx++] = nibbleToChar(bytes[i] >> 4);
    pszString[idx++] = nibbleToChar(bytes[i] & 0xf);
  }
}

int
myUserMain (cl_context context, cl_command_queue command_queue, cl_program program) {
  int err;

  cl_kernel kernel = clCreateKernel(program, "krnl_ethash", &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clCreateKernel: %d\n", err);
    return -1;
  }

  size_t sizeDAG = 1073739904U;
  size_t sizeHash = 32U;
  uint nNonce = 0;

  cl_mem bufResMix = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeHash, NULL, NULL);
  cl_mem bufResHash = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeHash, NULL, NULL);
  cl_mem bufDAG = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeDAG, NULL, NULL);
  cl_mem bufHeader = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeHash, NULL, NULL);

  uint nKernelARGC = 0;
  clSetKernelArg(kernel, nKernelARGC++, sizeof(cl_mem), &bufResMix);
  clSetKernelArg(kernel, nKernelARGC++, sizeof(cl_mem), &bufResHash);
  clSetKernelArg(kernel, nKernelARGC++, sizeof(cl_mem), &bufDAG);
  clSetKernelArg(kernel, nKernelARGC++, sizeof(cl_mem), &bufHeader);
  clSetKernelArg(kernel, nKernelARGC++, sizeof(uint), &nNonce);

  uint8_t *pResMix = (uint8_t *)clEnqueueMapBuffer(command_queue,
                                                   bufResMix,
                                                   CL_TRUE,
                                                   CL_MAP_READ,
                                                   0,
                                                   sizeHash,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clEnqueueMapBuffer[bufResMix]: %d\n", err);
    return -1;
  }
  uint8_t *pResHash = (uint8_t *)clEnqueueMapBuffer(command_queue,
                                                    bufResHash,
                                                    CL_TRUE,
                                                    CL_MAP_READ,
                                                    0,
                                                    sizeHash,
                                                    0,
                                                    NULL,
                                                    NULL,
                                                    &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clEnqueueMapBuffer[bufResHash]: %d\n", err);
    return -1;
  }
  uint8_t *pDAG = (uint8_t *)clEnqueueMapBuffer(command_queue,
                                                bufDAG,
                                                CL_TRUE,
                                                CL_MAP_WRITE,
                                                0,
                                                sizeDAG,
                                                0,
                                                NULL,
                                                NULL,
                                                &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clEnqueueMapBuffer[bufDAG]: %d\n", err);
    return -1;
  }
  uint8_t *pHeader = (uint8_t *)clEnqueueMapBuffer(command_queue,
                                                   bufHeader,
                                                   CL_TRUE,
                                                   CL_MAP_WRITE,
                                                   0,
                                                   sizeHash,
                                                   0,
                                                   NULL,
                                                   NULL,
                                                   &err);
  if (err != CL_SUCCESS) {
    fprintf(stderr, "ERROR IN clEnqueueMapBuffer[bufHeader]: %d\n", err);
    return -1;
  }

  memset(pDAG, 0, sizeDAG);
  myReadFileToMemory(DATA_FILE_NAME, pDAG);

  uint8_t headerBytes[HEADER_HASH_STRING_LEN/2];
  hexStringToBytes(HEADER_HASH_STRING, HEADER_HASH_STRING_LEN, headerBytes);
  memcpy(pHeader, headerBytes, HEADER_HASH_STRING_LEN/2);

  double hardware_start = myGetTimestamp();

  cl_mem inputBuffers[] = {bufDAG, bufHeader};
  unsigned int nInputBuffersLength = sizeof(inputBuffers) / sizeof(inputBuffers[0]);
  clEnqueueMigrateMemObjects(command_queue,
                             nInputBuffersLength,
                             inputBuffers,
                             0,
                             0,
                             NULL,
                             NULL);

  clEnqueueTask(command_queue, kernel, 0, NULL, NULL);

  cl_mem outputBuffers[] = {bufResMix, bufResHash};
  unsigned int nOutputBuffersLength = sizeof(outputBuffers) / sizeof(outputBuffers[0]);
  clEnqueueMigrateMemObjects(command_queue,
                             nOutputBuffersLength,
                             outputBuffers,
                             CL_MIGRATE_MEM_OBJECT_HOST,
                             0,
                             NULL,
                             NULL);

  clFinish(command_queue);

  double hardware_end = myGetTimestamp();

  printf("EXECUTION TIME IN KERNEL: %.4lf usec\n", (hardware_end - hardware_start));

  int isOK = 1;

  char rchHexString[HEADER_HASH_STRING_LEN + 1];
  rchHexString[HEADER_HASH_STRING_LEN] = '\0';

  memset(rchHexString, 0, HEADER_HASH_STRING_LEN + 1);
  bytesToHexString((const uint8_t *)pResMix, HEADER_HASH_STRING_LEN/2, rchHexString);
  if (strcmp(rchHexString, RES_MIX)) isOK = 0;
  printf("MIX: %s\n", rchHexString);
  printf("MX0: %s\n\n", RES_MIX);

  memset(rchHexString, 0, HEADER_HASH_STRING_LEN + 1);
  bytesToHexString((const uint8_t *)pResHash, HEADER_HASH_STRING_LEN/2, rchHexString);
  if (strcmp(rchHexString, RES_HASH)) isOK = 0;
  printf("HSH: %s\n", rchHexString);
  printf("HS0: %s\n", RES_HASH);

  fflush(stdout);

  clEnqueueUnmapMemObject(command_queue,
                          bufResMix,
                          pResMix,
                          0, NULL, NULL);
  clEnqueueUnmapMemObject(command_queue,
                          bufResHash,
                          pResHash,
                          0, NULL, NULL);
  clEnqueueUnmapMemObject(command_queue,
                          bufDAG,
                          pDAG,
                          0, NULL, NULL);
  clEnqueueUnmapMemObject(command_queue,
                          bufHeader,
                          pHeader,
                          0, NULL, NULL);
  clFinish(command_queue);

  clReleaseKernel(kernel);

  return isOK;
}

int
main (int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "ERROR IN argc: %d\n", argc);
    return -1;
  }

  int isOK = myOpenCLMain(argv[1], myUserMain);

  if (isOK) {
    printf("TEST PASSED\n");
    fflush(stdout);
    return 0;
  }
  else {
    fprintf(stderr, "ERROR: VALUE MISMATCH OCCURRED!\n");
    return -1;
  }
}
