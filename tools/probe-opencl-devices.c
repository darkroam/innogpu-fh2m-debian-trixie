/*************************************************************************/ /*!
@File		probe-opencl-devices.c
@Title		Minimal OpenCL platform/device enumerator (no OpenCL headers)
@Description	dlopen the OpenCL ICD loader, enumerate platforms and devices and
		print key capability info. Read-only; creates no context/queue and
		performs no compute.
@License	Public domain / project tools convention
*/ /**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef int    cl_int;
typedef unsigned int cl_uint;
typedef unsigned long cl_ulong;
typedef void *cl_platform_id;
typedef void *cl_device_id;
typedef cl_ulong cl_device_type;

#define CL_DEVICE_TYPE_DEFAULT   (1u << 0)
#define CL_DEVICE_TYPE_GPU       (1u << 2)
#define CL_DEVICE_TYPE_ALL       0xFFFFFFFFu

#define CL_PLATFORM_PROFILE      0x0900
#define CL_PLATFORM_VERSION      0x0901
#define CL_PLATFORM_NAME         0x0902
#define CL_PLATFORM_VENDOR       0x0903
#define CL_PLATFORM_EXTENSIONS   0x0904
#define CL_DEVICE_TYPE           0x1000
#define CL_DEVICE_VENDOR_ID      0x1001
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_MAX_WORK_GROUP_SIZE 0x1004
#define CL_DEVICE_MAX_MEM_ALLOC_SIZE 0x1010
#define CL_DEVICE_IMAGE_SUPPORT  0x1015
#define CL_DEVICE_LOCAL_MEM_SIZE 0x1016
#define CL_DEVICE_GLOBAL_MEM_SIZE 0x1020
#define CL_DEVICE_NAME           0x102B
#define CL_DEVICE_VENDOR         0x102C
#define CL_DEVICE_EXTENSIONS     0x1030
#define CL_DEVICE_DOUBLE_FP_CONFIG 0x1032
#define CL_DEVICE_OPENCL_C_VERSION 0x103D
#define CL_DEVICE_ADDRESS_BITS   0x100D
#define CL_DEVICE_MAX_CLOCK_FREQUENCY 0x100C
#define CL_DEVICE_VERSION        0x108F

typedef cl_int (*clGetPlatformIDs_t)(cl_uint, cl_platform_id *, cl_uint *);
typedef cl_int (*clGetPlatformInfo_t)(cl_platform_id, cl_uint, size_t, void *, size_t *);
typedef cl_int (*clGetDeviceIDs_t)(cl_platform_id, cl_device_type, cl_uint, cl_device_id *, cl_uint *);
typedef cl_int (*clGetDeviceInfo_t)(cl_device_id, cl_uint, size_t, void *, size_t *);

static const char *dev_type_str(cl_device_type t) {
    static char b[64];
    snprintf(b, sizeof b, "default=%d gpu=%d cpu=%d accel=%d", !!(t & 1), !!(t & 4), !!(t & 2), !!(t & 8));
    return b;
}

int main(void) {
    void *lib = dlopen("libOpenCL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!lib) { fprintf(stderr, "FATAL: libOpenCL.so.1 not loadable: %s\n", dlerror()); return 2; }
    clGetPlatformIDs_t fnPlat = (clGetPlatformIDs_t) dlsym(lib, "clGetPlatformIDs");
    clGetPlatformInfo_t fnPlatInfo = (clGetPlatformInfo_t) dlsym(lib, "clGetPlatformInfo");
    clGetDeviceIDs_t fnDev = (clGetDeviceIDs_t) dlsym(lib, "clGetDeviceIDs");
    clGetDeviceInfo_t fnDevInfo = (clGetDeviceInfo_t) dlsym(lib, "clGetDeviceInfo");
    if (!fnPlat || !fnPlatInfo || !fnDev || !fnDevInfo) {
        fprintf(stderr, "FATAL: required cl* entry points missing\n"); dlclose(lib); return 2;
    }
    cl_uint np = 0;
    if (fnPlat(0, NULL, &np) != 0) { printf("WARN: clGetPlatformIDs failed\n"); dlclose(lib); return 0; }
    printf("OpenCL platforms: %u\n", np);
    cl_platform_id *plats = calloc(np ? np : 1, sizeof *plats);
    if (np) fnPlat(np, plats, NULL);
    for (cl_uint p = 0; p < np; p++) {
        char name[256] = "", ver[256] = "", vendor[256] = "", prof[128] = "", ext[4096] = "";
        size_t got = 0;
        fnPlatInfo(plats[p], CL_PLATFORM_NAME, sizeof name, name, &got);
        fnPlatInfo(plats[p], CL_PLATFORM_VERSION, sizeof ver, ver, &got);
        fnPlatInfo(plats[p], CL_PLATFORM_VENDOR, sizeof vendor, vendor, &got);
        fnPlatInfo(plats[p], CL_PLATFORM_PROFILE, sizeof prof, prof, &got);
        fnPlatInfo(plats[p], CL_PLATFORM_EXTENSIONS, sizeof ext, ext, &got);
        printf("  [%u] %s (%s) vendor=%s profile=%s\n  extensions: %s\n", p, name, ver, vendor, prof, ext);

        cl_uint nd = 0;
        cl_device_type all = CL_DEVICE_TYPE_ALL;
        if (fnDev(plats[p], all, 0, NULL, &nd) == 0 && nd) {
            cl_device_id *devs = calloc(nd, sizeof *devs);
            fnDev(plats[p], all, nd, devs, NULL);
            printf("  devices: %u\n", nd);
            for (cl_uint d = 0; d < nd; d++) {
                char dname[256] = "", dver[256] = "", dvendor[256] = "", cver[128] = "", dext[8192] = "";
                cl_ulong v = 0, gmem = 0, lmem = 0, maxalloc = 0, wg = 0;
                cl_uint cu = 0, clock = 0, addrbits = 0, image = 0, dfp = 0, dtype = 0;
                fnDevInfo(devs[d], CL_DEVICE_NAME, sizeof dname, dname, NULL);
                fnDevInfo(devs[d], CL_DEVICE_VERSION, sizeof dver, dver, NULL);
                fnDevInfo(devs[d], CL_DEVICE_VENDOR, sizeof dvendor, dvendor, NULL);
                fnDevInfo(devs[d], CL_DEVICE_OPENCL_C_VERSION, sizeof cver, cver, NULL);
                fnDevInfo(devs[d], CL_DEVICE_EXTENSIONS, sizeof dext, dext, NULL);
                fnDevInfo(devs[d], CL_DEVICE_TYPE, sizeof dtype, &dtype, NULL);
                fnDevInfo(devs[d], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof cu, &cu, NULL);
                fnDevInfo(devs[d], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof clock, &clock, NULL);
                fnDevInfo(devs[d], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof gmem, &gmem, NULL);
                fnDevInfo(devs[d], CL_DEVICE_LOCAL_MEM_SIZE, sizeof lmem, &lmem, NULL);
                fnDevInfo(devs[d], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof maxalloc, &maxalloc, NULL);
                fnDevInfo(devs[d], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof wg, &wg, NULL);
                fnDevInfo(devs[d], CL_DEVICE_ADDRESS_BITS, sizeof addrbits, &addrbits, NULL);
                fnDevInfo(devs[d], CL_DEVICE_IMAGE_SUPPORT, sizeof image, &image, NULL);
                fnDevInfo(devs[d], CL_DEVICE_DOUBLE_FP_CONFIG, sizeof dfp, &dfp, NULL);
                printf("    [%u] %s  type(%s)\n", d, dname, dev_type_str((cl_device_type) dtype));
                printf("      version=%s c_version=%s vendor=%s\n", dver, cver, dvendor);
                printf("      compute_units=%u clock=%uMHz address_bits=%u image_support=%u\n", cu, clock, addrbits, image);
                printf("      global_mem=%luMB max_alloc=%luMB local_mem=%luKB max_wg=%lu double_fp=0x%x\n",
                       (unsigned long)(gmem >> 20), (unsigned long)(maxalloc >> 20), (unsigned long)(lmem >> 10),
                       (unsigned long) wg, dfp);
                printf("      extensions: %s\n", dext);
            }
            free(devs);
        } else {
            printf("  devices: none\n");
        }
    }
    free(plats);
    dlclose(lib);
    return 0;
}
