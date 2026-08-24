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

static int enumeration_main(void) {
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
static const char *cl_loader_name(void) {
    const char *e = getenv("PROBE_OPENCL_LOADER");
    return e && *e ? e : "libOpenCL.so.1";
}

/* =====================================================================
 * Execution mode: minimal verifiable compute (element-wise add kernel).
 * Usage: probe-opencl-devices exec [elements]
 * Loads ICD loader -> selects a GPU device (rejects CPU-only) -> context +
 * queue -> buffers -> build+run add kernel -> blocking readback -> verify
 * every element -> release everything. Machine-readable output;
 * exit: 0=PASS 2=loader 3=no-GPU-device 4=context/queue/buffer 5=build 6=run 7=verify.
 * ===================================================================== */

typedef void *cl_context, *cl_command_queue, *cl_mem, *cl_program, *cl_kernel;
#define CL_DEVICE_TYPE_GPU   (1u << 2)
#define CL_MEM_READ_WRITE    (1u << 0)
#define CL_MEM_COPY_HOST_PTR (1u << 5)
#define CL_TRUE 1
#define CL_PROGRAM_BUILD_STATUS 0x1181
#define CL_PROGRAM_BUILD_LOG    0x1183
#define CL_SUCCESS 0

typedef cl_context (*clCreateContext_t)(const void *, cl_uint, const cl_device_id *, void (*)(const char *, const void *, size_t, void *), void *, cl_int *);
typedef cl_command_queue (*clCreateCommandQueue_t)(cl_context, cl_device_id, cl_ulong, cl_int *);
typedef cl_mem (*clCreateBuffer_t)(cl_context, cl_ulong, size_t, void *, cl_int *);
typedef cl_program (*clCreateProgramWithSource_t)(cl_context, cl_uint, const char **, const size_t *, cl_int *);
typedef cl_int (*clBuildProgram_t)(cl_program, cl_uint, const cl_device_id *, const char *, void (*)(cl_program, void *), void *);
typedef cl_int (*clGetProgramBuildInfo_t)(cl_program, cl_device_id, cl_uint, size_t, void *, size_t *);
typedef cl_kernel (*clCreateKernel_t)(cl_program, const char *, cl_int *);
typedef cl_int (*clSetKernelArg_t)(cl_kernel, cl_uint, size_t, const void *);
typedef cl_int (*clEnqueueNDRangeKernel_t)(cl_command_queue, cl_kernel, cl_uint, const size_t *, const size_t *, const size_t *, cl_uint, const void *, const void *);
typedef cl_int (*clEnqueueReadBuffer_t)(cl_command_queue, cl_mem, int, size_t, size_t, void *, cl_uint, const void *, const void *);
typedef cl_int (*clFinish_t)(cl_command_queue);
typedef cl_int (*clReleaseMemObject_t)(cl_mem);
typedef cl_int (*clReleaseKernel_t)(cl_kernel);
typedef cl_int (*clReleaseProgram_t)(cl_program);
typedef cl_int (*clReleaseCommandQueue_t)(cl_command_queue);
typedef cl_int (*clReleaseContext_t)(cl_context);

static int exec_main(int argc, char **argv) {
    size_t n = 1024;
    if (argc > 2) { long v = atol(argv[2]); if (v > 0 && v < (1 << 24)) n = (size_t) v; }

    void *lib = dlopen(cl_loader_name(), RTLD_NOW | RTLD_GLOBAL);
    if (!lib) { fprintf(stderr, "opencl_exec_loader=fail reason=%s\n", dlerror()); return 2; }
    clGetPlatformIDs_t fnPlat = (clGetPlatformIDs_t) dlsym(lib, "clGetPlatformIDs");
    clGetDeviceIDs_t fnDev = (clGetDeviceIDs_t) dlsym(lib, "clGetDeviceIDs");
    clGetDeviceInfo_t fnDevInfo = (clGetDeviceInfo_t) dlsym(lib, "clGetDeviceInfo");
    if (!fnPlat || !fnDev || !fnDevInfo) { fprintf(stderr, "opencl_exec_loader=fail reason=entry points missing\n"); dlclose(lib); return 2; }
    printf("opencl_exec_loader=ok\n");

    cl_context ctx = 0; cl_command_queue q = 0; cl_mem ma = 0, mb = 0, mc = 0;
    cl_program prog = 0; cl_kernel kern = 0;
    float *a = NULL, *b = NULL, *c = NULL;
    int rc = 3;

    cl_uint np = 0;
    if (fnPlat(0, NULL, &np) != 0) { fprintf(stderr, "opencl_exec_platform=fail reason=clGetPlatformIDs\n"); goto cleanup; }
    cl_platform_id *plats = calloc(np ? np : 1, sizeof *plats);
    cl_device_id gpu = 0; cl_platform_id gpu_plat = 0;
    cl_ulong gpu_vendor = 0;
    if (np) fnPlat(np, plats, NULL);
    for (cl_uint p = 0; p < np && !gpu; p++) {
        cl_uint nd = 0;
        if (fnDev(plats[p], CL_DEVICE_TYPE_GPU, 0, NULL, &nd) == 0 && nd) {
            cl_device_id *devs = calloc(nd, sizeof *devs);
            if (fnDev(plats[p], CL_DEVICE_TYPE_GPU, nd, devs, NULL) == 0) {
                for (cl_uint d = 0; d < nd && !gpu; d++) {
                    cl_ulong vendor = 0;
                    fnDevInfo(devs[d], CL_DEVICE_VENDOR_ID, sizeof vendor, &vendor, NULL);
                    /* 仅接受 Innosilicon 0x1ec8；vendor==0 不视为真实硬件 */
                    if (vendor == 0x1ec8) { gpu = devs[d]; gpu_plat = plats[p]; gpu_vendor = vendor; }
                }
            }
            free(devs);
        }
    }
    free(plats);
    if (!gpu) {
        fprintf(stderr, "opencl_exec_device=fail reason=no confirmed Innosilicon GPU (vendor 0x1ec8)%s\n", np ? "" : " (no platforms)");
        goto cleanup;
    }
    char dname[256] = ""; fnDevInfo(gpu, CL_DEVICE_NAME, sizeof dname, dname, NULL);
    printf("opencl_exec_device=%s vendor=0x%04lx\n", dname, (unsigned long) gpu_vendor);

    clCreateContext_t fnCtx = (clCreateContext_t) dlsym(lib, "clCreateContext");
    clCreateCommandQueue_t fnQueue = (clCreateCommandQueue_t) dlsym(lib, "clCreateCommandQueue");
    clCreateBuffer_t fnBuf = (clCreateBuffer_t) dlsym(lib, "clCreateBuffer");
    clCreateProgramWithSource_t fnSrc = (clCreateProgramWithSource_t) dlsym(lib, "clCreateProgramWithSource");
    clBuildProgram_t fnBuild = (clBuildProgram_t) dlsym(lib, "clBuildProgram");
    clGetProgramBuildInfo_t fnBuildInfo = (clGetProgramBuildInfo_t) dlsym(lib, "clGetProgramBuildInfo");
    clCreateKernel_t fnKernel = (clCreateKernel_t) dlsym(lib, "clCreateKernel");
    clSetKernelArg_t fnArg = (clSetKernelArg_t) dlsym(lib, "clSetKernelArg");
    clEnqueueNDRangeKernel_t fnK = (clEnqueueNDRangeKernel_t) dlsym(lib, "clEnqueueNDRangeKernel");
    clEnqueueReadBuffer_t fnRead = (clEnqueueReadBuffer_t) dlsym(lib, "clEnqueueReadBuffer");
    clFinish_t fnFinish = (clFinish_t) dlsym(lib, "clFinish");
    clReleaseMemObject_t fnRM = (clReleaseMemObject_t) dlsym(lib, "clReleaseMemObject");
    clReleaseKernel_t fnRK = (clReleaseKernel_t) dlsym(lib, "clReleaseKernel");
    clReleaseProgram_t fnRP = (clReleaseProgram_t) dlsym(lib, "clReleaseProgram");
    clReleaseCommandQueue_t fnRQ = (clReleaseCommandQueue_t) dlsym(lib, "clReleaseCommandQueue");
    clReleaseContext_t fnRC = (clReleaseContext_t) dlsym(lib, "clReleaseContext");
    if (!fnCtx || !fnQueue || !fnBuf || !fnSrc || !fnBuild || !fnKernel || !fnArg ||
        !fnK || !fnRead || !fnFinish || !fnRM || !fnRK || !fnRP || !fnRQ || !fnRC) {
        fprintf(stderr, "opencl_exec_context=fail reason=entry points missing\n"); rc = 4; goto cleanup;
    }

    cl_int err = 0;
    ctx = fnCtx(NULL, 1, &gpu, NULL, NULL, &err);
    if (!ctx || err != CL_SUCCESS) { fprintf(stderr, "opencl_exec_context=fail reason=clCreateContext err=%d\n", err); rc = 4; goto cleanup; }
    q = fnQueue(ctx, gpu, 0, &err);
    if (!q || err != CL_SUCCESS) { fprintf(stderr, "opencl_exec_queue=fail reason=clCreateCommandQueue err=%d\n", err); rc = 4; goto cleanup; }
    printf("opencl_exec_context=ok\nopencl_exec_queue=ok\n");

    size_t bytes = n * sizeof(float);
    a = malloc(bytes); b = malloc(bytes); c = malloc(bytes);
    if (!a || !b || !c) { fprintf(stderr, "opencl_exec_buffer=fail reason=host malloc\n"); rc = 4; goto cleanup; }
    for (size_t i = 0; i < n; i++) { a[i] = (float) i; b[i] = (float) (2 * i); c[i] = -1.0f; }
    ma = fnBuf(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bytes, a, &err);
    mb = fnBuf(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bytes, b, &err);
    mc = fnBuf(ctx, CL_MEM_READ_WRITE, bytes, NULL, &err);
    if (!ma || !mb || !mc) { fprintf(stderr, "opencl_exec_buffer=fail reason=clCreateBuffer err=%d\n", err); rc = 4; goto cleanup; }
    printf("opencl_exec_buffer=ok\n");

    const char *src = "__kernel void add(__global const float *a, __global const float *b, __global float *c, unsigned int n) {"
"    unsigned int i = get_global_id(0);"
"    if (i < n) c[i] = a[i] + b[i];"
"}\n";
    prog = fnSrc(ctx, 1, &src, NULL, &err);
    if (!prog || err != CL_SUCCESS) { fprintf(stderr, "opencl_exec_program=fail reason=clCreateProgramWithSource err=%d\n", err); rc = 5; goto cleanup; }
    err = fnBuild(prog, 1, &gpu, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        char log[4096] = ""; size_t lsz = 0;
        if (fnBuildInfo) fnBuildInfo(prog, gpu, CL_PROGRAM_BUILD_LOG, sizeof log, log, &lsz);
        fprintf(stderr, "opencl_exec_build=fail reason=clBuildProgram err=%d log=%s\n", err, log);
        rc = 5; goto cleanup;
    }
    kern = fnKernel(prog, "add", &err);
    if (!kern || err != CL_SUCCESS) { fprintf(stderr, "opencl_exec_kernel=fail reason=clCreateKernel err=%d\n", err); rc = 6; goto cleanup; }
    unsigned int un = (unsigned int) n;
    if (fnArg(kern, 0, sizeof(cl_mem), &ma) != 0 || fnArg(kern, 1, sizeof(cl_mem), &mb) != 0 ||
        fnArg(kern, 2, sizeof(cl_mem), &mc) != 0 || fnArg(kern, 3, sizeof un, &un) != 0) {
        fprintf(stderr, "opencl_exec_kernel=fail reason=clSetKernelArg\n"); rc = 6; goto cleanup;
    }
    size_t gsz = n;
    err = fnK(q, kern, 1, NULL, &gsz, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) { fprintf(stderr, "opencl_exec_run=fail reason=clEnqueueNDRangeKernel err=%d\n", err); rc = 6; goto cleanup; }
    err = fnRead(q, mc, CL_TRUE, 0, bytes, c, 0, NULL, NULL);
    if (err != CL_SUCCESS) { fprintf(stderr, "opencl_exec_readback=fail reason=clEnqueueReadBuffer err=%d\n", err); rc = 6; goto cleanup; }
    if (fnFinish(q) != CL_SUCCESS) { fprintf(stderr, "opencl_exec_finish=fail reason=clFinish\n"); rc = 6; goto cleanup; }
    printf("opencl_exec_run=ok\n");

    size_t bad = 0; size_t first_bad = n;
    for (size_t i = 0; i < n; i++) {
        float want = a[i] + b[i];
        if (c[i] != want) { if (!bad) first_bad = i; bad++; }
    }
    if (bad) {
        fprintf(stderr, "opencl_exec_verify=fail reason=%zu/%zu elements wrong, first at %zu (got %g want %g)\n", bad, n, first_bad, (double) c[first_bad], (double) (a[first_bad] + b[first_bad]));
        rc = 7; goto cleanup;
    }
    printf("opencl_exec_verify=ok\nopencl_exec_pass=1\n");
    rc = 0;

cleanup:
    /* 逆序释放：kernel -> program -> buffers -> queue -> context -> loader；并释放 host 内存 */
    if (kern && fnRK) fnRK(kern);
    if (prog && fnRP) fnRP(prog);
    if (mc && fnRM) fnRM(mc); if (mb && fnRM) fnRM(mb); if (ma && fnRM) fnRM(ma);
    if (q && fnRQ) fnRQ(q);
    if (ctx && fnRC) fnRC(ctx);
    free(a); free(b); free(c);
    dlclose(lib);
    return rc;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "exec") == 0) return exec_main(argc, argv);
    return enumeration_main();
}
