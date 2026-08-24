/*************************************************************************/ /*!
@File		probe-vulkan-devices.c
@Title		Minimal Vulkan instance/device enumerator (no Vulkan headers)
@Description	dlopen the Vulkan loader, enumerate instance version/extensions,
		physical devices and their properties. Read-only; creates no
		device/queue/context and performs no rendering.
@License	Public domain / project tools convention
*/ /**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>

typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef uintptr_t VkInstance;
typedef uintptr_t VkPhysicalDevice;
typedef uintptr_t PFN_vkVoidFunction;
typedef PFN_vkVoidFunction (*pfn_vkGetInstanceProcAddr)(VkInstance, const char *);

#define VK_STRUCTURE_TYPE_APPLICATION_INFO     0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define VK_API_VERSION_1_3 (0x00403000u)
#define VK_API_VERSION_1_2 (0x00402000u)
#define VK_API_VERSION_1_1 (0x00401000u)
#define VK_API_VERSION_1_0 (0x00400000u)

typedef struct VkApplicationInfo {
    uint32_t sType; const void *pNext; const char *pApplicationName;
    uint32_t applicationVersion; const char *pEngineName;
    uint32_t engineVersion; uint32_t apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
    uint32_t sType; const void *pNext; VkFlags flags;
    const VkApplicationInfo *pApplicationInfo;
    uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames;
    uint32_t enabledLayerCount; const char *const *ppEnabledLayerNames;
} VkInstanceCreateInfo;

typedef struct VkExtensionProperties {
    char extensionName[256]; uint32_t specVersion;
} VkExtensionProperties;

/* We only read the leading fields; the driver writes the whole struct, so pad. */
typedef struct VkPhysicalDeviceProperties {
    uint32_t apiVersion; uint32_t driverVersion; uint32_t vendorID; uint32_t deviceID;
    int deviceType; char deviceName[256]; uint8_t pipelineCacheUUID[16];
    char _pad[4096];
} VkPhysicalDeviceProperties;

typedef struct VkExtent3D { uint32_t width, height, depth; } VkExtent3D;
typedef struct VkQueueFamilyProperties {
    VkFlags queueFlags; uint32_t queueCount; uint32_t timestampValidBits; VkExtent3D minImageTransferGranularity;
} VkQueueFamilyProperties;

#define LOAD(name) (name ## _t) dlsym(lib, #name)
typedef int (*vkEnumerateInstanceVersion_t)(uint32_t *);
typedef int (*vkEnumerateInstanceExtensionProperties_t)(const char *, uint32_t *, VkExtensionProperties *);
typedef int (*vkCreateInstance_t)(const VkInstanceCreateInfo *, const void *, VkInstance *);
typedef int (*vkDestroyInstance_t)(VkInstance, const void *);
typedef int (*vkEnumeratePhysicalDevices_t)(VkInstance, uint32_t *, VkPhysicalDevice *);
typedef void (*vkGetPhysicalDeviceProperties_t)(VkPhysicalDevice, VkPhysicalDeviceProperties *);
typedef void (*vkGetPhysicalDeviceQueueFamilyProperties_t)(VkPhysicalDevice, uint32_t *, VkQueueFamilyProperties *);
typedef int (*vkEnumerateDeviceExtensionProperties_t)(VkPhysicalDevice, const char *, uint32_t *, VkExtensionProperties *);

static const char *dev_type_name(int t) {
    switch (t) { case 0: return "OTHER"; case 1: return "INTEGRATED_GPU";
    case 2: return "DISCRETE_GPU"; case 3: return "VIRTUAL_GPU";
    case 4: return "CPU"; default: return "?"; }
}
static void dump_api(uint32_t v, char *buf, size_t n) {
    snprintf(buf, n, "%u.%u.%u", (v >> 22) & 0x7f, (v >> 12) & 0x3ff, v & 0xfff);
}

static const char *loader_name(void) {
    const char *e = getenv("PROBE_VULKAN_LOADER");
    return e && *e ? e : "libvulkan.so.1";
}

static int enumeration_main(void) {
    void *lib = dlopen(loader_name(), RTLD_NOW | RTLD_GLOBAL);
    if (!lib) { fprintf(stderr, "FATAL: libvulkan.so.1 not loadable: %s\n", dlerror()); return 2; }
    pfn_vkGetInstanceProcAddr gpa = (pfn_vkGetInstanceProcAddr) dlsym(lib, "vkGetInstanceProcAddr");
    if (!gpa) { fprintf(stderr, "FATAL: no vkGetInstanceProcAddr in loader\n"); return 2; }

    /* vkGetInstanceProcAddr is version-independent; obtain entry points for a NULL instance. */
    vkEnumerateInstanceVersion_t fnEnuVer = (vkEnumerateInstanceVersion_t) gpa((VkInstance) 0, "vkEnumerateInstanceVersion");
    uint32_t instVer = 0;
    if (fnEnuVer && fnEnuVer(&instVer) == 0) { char b[32]; dump_api(instVer, b, sizeof b); printf("Loader instance version: %s\n", b); }
    else printf("Loader instance version: (not exposed)\n");

    vkEnumerateInstanceExtensionProperties_t fnInstExt = (vkEnumerateInstanceExtensionProperties_t) gpa((VkInstance) 0, "vkEnumerateInstanceExtensionProperties");
    if (fnInstExt) {
        uint32_t n = 0; fnInstExt(NULL, &n, NULL);
        VkExtensionProperties *e = calloc(n ? n : 1, sizeof *e);
        if (n && fnInstExt(NULL, &n, e) == 0) {
            printf("Instance extensions (%u):\n", n);
            for (uint32_t i = 0; i < n; i++) printf("  %s (spec %u)\n", e[i].extensionName, e[i].specVersion);
        }
        free(e);
    }

    uint32_t apiCandidates[] = { VK_API_VERSION_1_3, VK_API_VERSION_1_2, VK_API_VERSION_1_1, VK_API_VERSION_1_0 };
    VkInstance inst = 0; int created = 0;
    for (unsigned c = 0; c < sizeof apiCandidates / sizeof *apiCandidates && !created; c++) {
        VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = apiCandidates[c] };
        VkInstanceCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
        vkCreateInstance_t fnCreate = (vkCreateInstance_t) gpa((VkInstance) 0, "vkCreateInstance");
        if (fnCreate && fnCreate(&ci, NULL, &inst) == 0) created = 1;
    }
    if (!created) { printf("WARN: could not create a Vulkan instance (no ICD loaded?)\n"); dlclose(lib); return 0; }

    vkEnumeratePhysicalDevices_t fnPhys = (vkEnumeratePhysicalDevices_t) gpa(inst, "vkEnumeratePhysicalDevices");
    vkGetPhysicalDeviceProperties_t fnProps = (vkGetPhysicalDeviceProperties_t) gpa(inst, "vkGetPhysicalDeviceProperties");
    vkGetPhysicalDeviceQueueFamilyProperties_t fnQf = (vkGetPhysicalDeviceQueueFamilyProperties_t) gpa(inst, "vkGetPhysicalDeviceQueueFamilyProperties");
    vkEnumerateDeviceExtensionProperties_t fnDevExt = (vkEnumerateDeviceExtensionProperties_t) gpa(inst, "vkEnumerateDeviceExtensionProperties");

    uint32_t ndev = 0;
    if (!fnPhys || fnPhys(inst, &ndev, NULL) != 0) { printf("WARN: no physical devices enumerable\n"); }
    printf("Physical devices: %u\n", ndev);
    VkPhysicalDevice *devs = calloc(ndev ? ndev : 1, sizeof *devs);
    if (ndev && fnPhys(inst, &ndev, devs) == 0) {
        for (uint32_t d = 0; d < ndev; d++) {
            VkPhysicalDeviceProperties p; memset(&p, 0, sizeof p);
            fnProps(devs[d], &p);
            char av[32], dv[32]; dump_api(p.apiVersion, av, sizeof av); dump_api(p.driverVersion, dv, sizeof dv);
            printf("  [%u] %s  type=%s vendor=0x%04x device=0x%04x\n"
                   "       apiVersion=%s driverVersion=%s\n",
                   d, p.deviceName, dev_type_name(p.deviceType), p.vendorID, p.deviceID, av, dv);
            if (fnQf) {
                uint32_t nq = 0; fnQf(devs[d], &nq, NULL);
                VkQueueFamilyProperties *q = calloc(nq ? nq : 1, sizeof *q);
                fnQf(devs[d], &nq, q);
                printf("       queue families: %u (", nq);
                for (uint32_t i = 0; i < nq; i++) printf("%s%u queues/0x%x", i ? "," : "", q[i].queueCount, q[i].queueFlags);
                printf(")\n");
                free(q);
            }
            if (fnDevExt) {
                uint32_t ne = 0; fnDevExt(devs[d], NULL, &ne, NULL);
                VkExtensionProperties *e = calloc(ne ? ne : 1, sizeof *e);
                if (ne && fnDevExt(devs[d], NULL, &ne, e) == 0) {
                    printf("       device extensions (%u):\n", ne);
                    for (uint32_t i = 0; i < ne; i++) printf("         %s\n", e[i].extensionName);
                }
                free(e);
            }
        }
    }
    free(devs);
    vkDestroyInstance_t fnDestroy = (vkDestroyInstance_t) gpa(inst, "vkDestroyInstance");
    if (fnDestroy) fnDestroy(inst, NULL);
    dlclose(lib);
    return 0;
}

/* =====================================================================
 * Execution mode: minimal verifiable queue/sync operation.
 * Usage: probe-vulkan-devices exec [timeout_ms]
 * Creates instance -> selects a GPU physical device (rejects CPU-only) ->
 * logical device + queue -> submits an empty command buffer with a fence
 * -> waits (bounded) -> destroys everything. Machine-readable output;
 * exit: 0=PASS 2=loader 3=no-suitable-device/init 4=device/queue 5=submit/wait.
 * ===================================================================== */

typedef uintptr_t VkDevice, VkQueue, VkCommandPool, VkCommandBuffer, VkFence;

#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 2
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO        3
#define VK_STRUCTURE_TYPE_SUBMIT_INFO               4
#define VK_STRUCTURE_TYPE_FENCE_CREATE_INFO         8
#define VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO 20
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 21
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO 22
#define VK_QUEUE_GRAPHICS_BIT 0x00000001u
#define VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 0x00000001u
#define VK_COMMAND_BUFFER_LEVEL_PRIMARY 0u
#define VK_SUCCESS 0
#define VK_TIMEOUT 2

typedef struct VkDeviceQueueCreateInfo {
    uint32_t sType; const void *pNext; VkFlags flags;
    uint32_t queueFamilyIndex; uint32_t queueCount; const float *pQueuePriorities;
} VkDeviceQueueCreateInfo;
typedef struct VkDeviceCreateInfo {
    uint32_t sType; const void *pNext; VkFlags flags;
    uint32_t queueCreateInfoCount; const VkDeviceQueueCreateInfo *pQueueCreateInfos;
    uint32_t enabledLayerCount; const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames;
    const void *pEnabledFeatures;
} VkDeviceCreateInfo;
typedef struct VkFenceCreateInfo { uint32_t sType; const void *pNext; VkFlags flags; } VkFenceCreateInfo;
typedef struct VkCommandPoolCreateInfo {
    uint32_t sType; const void *pNext; VkFlags flags; uint32_t queueFamilyIndex;
} VkCommandPoolCreateInfo;
typedef struct VkCommandBufferAllocateInfo {
    uint32_t sType; const void *pNext;
    VkCommandPool commandPool; uint32_t level; uint32_t commandBufferCount;
} VkCommandBufferAllocateInfo;
typedef struct VkCommandBufferBeginInfo {
    uint32_t sType; const void *pNext; VkFlags flags; const void *pInheritanceInfo;
} VkCommandBufferBeginInfo;
typedef struct VkSubmitInfo {
    uint32_t sType; const void *pNext;
    uint32_t waitSemaphoreCount; const void *pWaitSemaphores; const uint32_t *pWaitDstStageMask;
    uint32_t commandBufferCount; VkCommandBuffer *pCommandBuffers;
    uint32_t signalSemaphoreCount; const void *pSignalSemaphores;
} VkSubmitInfo;

typedef PFN_vkVoidFunction (*pfn_vkGetDeviceProcAddr)(VkDevice, const char *);
typedef int (*vkCreateDevice_t)(VkPhysicalDevice, const VkDeviceCreateInfo *, const void *, VkDevice *);
typedef void (*vkGetDeviceQueue_t)(VkDevice, uint32_t, uint32_t, VkQueue *);
typedef void (*vkDestroyDevice_t)(VkDevice, const void *);
typedef int (*vkCreateCommandPool_t)(VkDevice, const VkCommandPoolCreateInfo *, const void *, VkCommandPool *);
typedef int (*vkAllocateCommandBuffers_t)(VkDevice, const VkCommandBufferAllocateInfo *, VkCommandBuffer *);
typedef int (*vkBeginCommandBuffer_t)(VkCommandBuffer, const VkCommandBufferBeginInfo *);
typedef int (*vkEndCommandBuffer_t)(VkCommandBuffer);
typedef void (*vkFreeCommandBuffers_t)(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer *);
typedef void (*vkDestroyCommandPool_t)(VkDevice, VkCommandPool, const void *);
typedef int (*vkCreateFence_t)(VkDevice, const VkFenceCreateInfo *, const void *, VkFence *);
typedef int (*vkWaitForFences_t)(VkDevice, uint32_t, const VkFence *, int, uint64_t);
typedef void (*vkDestroyFence_t)(VkDevice, VkFence, const void *);
typedef int (*vkQueueSubmit_t)(VkQueue, uint32_t, const VkSubmitInfo *, VkFence);
typedef int (*vkDeviceWaitIdle_t)(VkDevice);

static int exec_main(int argc, char **argv) {
    uint64_t timeout_ns = 5000000000ull; /* 5s default */
    if (argc > 2) { long ms = atol(argv[2]); if (ms > 0) timeout_ns = (uint64_t) ms * 1000000ull; }

    void *lib = dlopen(loader_name(), RTLD_NOW | RTLD_GLOBAL);
    if (!lib) { fprintf(stderr, "vulkan_exec_loader=fail reason=%s\n", dlerror()); return 2; }
    pfn_vkGetInstanceProcAddr gpa = (pfn_vkGetInstanceProcAddr) dlsym(lib, "vkGetInstanceProcAddr");
    if (!gpa) { fprintf(stderr, "vulkan_exec_loader=fail reason=no vkGetInstanceProcAddr\n"); dlclose(lib); return 2; }
    printf("vulkan_exec_loader=ok\n");

    VkInstance inst = 0; VkDevice dev = 0; VkQueue queue = 0;
    VkCommandPool pool = 0; VkCommandBuffer cmd = 0; VkFence fence = 0;
    int rc = 3;

    uint32_t apiCandidates[] = { VK_API_VERSION_1_3, VK_API_VERSION_1_2, VK_API_VERSION_1_1, VK_API_VERSION_1_0 };
    int created = 0;
    for (unsigned c = 0; c < sizeof apiCandidates / sizeof *apiCandidates && !created; c++) {
        VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = apiCandidates[c] };
        VkInstanceCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app };
        vkCreateInstance_t fnCreate = (vkCreateInstance_t) gpa((VkInstance) 0, "vkCreateInstance");
        if (fnCreate && fnCreate(&ci, NULL, &inst) == 0) created = 1;
    }
    if (!created) { fprintf(stderr, "vulkan_exec_instance=fail reason=no ICD or instance creation failed\n"); goto cleanup; }
    printf("vulkan_exec_instance=ok\n");

    vkEnumeratePhysicalDevices_t fnPhys = (vkEnumeratePhysicalDevices_t) gpa(inst, "vkEnumeratePhysicalDevices");
    vkGetPhysicalDeviceProperties_t fnProps = (vkGetPhysicalDeviceProperties_t) gpa(inst, "vkGetPhysicalDeviceProperties");
    vkGetPhysicalDeviceQueueFamilyProperties_t fnQf = (vkGetPhysicalDeviceQueueFamilyProperties_t) gpa(inst, "vkGetPhysicalDeviceQueueFamilyProperties");
    if (!fnPhys || !fnProps || !fnQf) { fprintf(stderr, "vulkan_exec_device=fail reason=missing device entry points\n"); goto cleanup; }
    uint32_t ndev = 0;
    if (fnPhys(inst, &ndev, NULL) != 0) ndev = 0;
    VkPhysicalDevice *devs = calloc(ndev ? ndev : 1, sizeof *devs);
    VkPhysicalDevice chosen = 0; int chosen_idx = -1; int saw_gpu = 0;
    if (ndev && fnPhys(inst, &ndev, devs) == 0) {
        for (uint32_t d = 0; d < ndev; d++) {
            VkPhysicalDeviceProperties p; memset(&p, 0, sizeof p);
            fnProps(devs[d], &p);
            if (p.deviceType != 4) saw_gpu = 1;
            if (p.vendorID == 0x1ec8) { chosen = devs[d]; chosen_idx = (int) d; break; }
            if (chosen_idx < 0 && p.deviceType != 4) { chosen = devs[d]; chosen_idx = (int) d; }
        }
    }
    free(devs);
    if (chosen_idx < 0) {
        const char *why = ndev ? (saw_gpu ? "" : " (CPU/software only)") : " (no physical devices)";
        fprintf(stderr, "vulkan_exec_device=fail reason=no GPU device%s\n", why);
        goto cleanup;
    }
    VkPhysicalDeviceProperties cp; memset(&cp, 0, sizeof cp); fnProps(chosen, &cp);
    printf("vulkan_exec_device=%s vendor=0x%04x device=0x%04x\n", cp.deviceName, cp.vendorID, cp.deviceID);

    uint32_t nq = 0; fnQf(chosen, &nq, NULL);
    VkQueueFamilyProperties *qf = calloc(nq ? nq : 1, sizeof *qf);
    if (nq) fnQf(chosen, &nq, qf);
    int qfam = -1;
    for (uint32_t i = 0; i < nq; i++) if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { qfam = (int) i; break; }
    free(qf);
    if (qfam < 0) { fprintf(stderr, "vulkan_exec_queue=fail reason=no graphics queue family\n"); rc = 4; goto cleanup; }

    /* 先取 vkGetDeviceProcAddr 并判空，避免空函数指针调用 */
    pfn_vkGetDeviceProcAddr gdpa = (pfn_vkGetDeviceProcAddr) gpa(inst, "vkGetDeviceProcAddr");
    if (!gdpa) { fprintf(stderr, "vulkan_exec_device=fail reason=no vkGetDeviceProcAddr\n"); rc = 4; goto cleanup; }
    vkCreateDevice_t fnCreateDev = (vkCreateDevice_t) gdpa((VkDevice) 0, "vkCreateDevice");
    vkGetDeviceQueue_t fnGetQueue = (vkGetDeviceQueue_t) gdpa((VkDevice) 0, "vkGetDeviceQueue");
    vkDestroyDevice_t fnDestroyDev = (vkDestroyDevice_t) gdpa((VkDevice) 0, "vkDestroyDevice");
    if (!fnCreateDev || !fnGetQueue || !fnDestroyDev) { fprintf(stderr, "vulkan_exec_device=fail reason=device entry points unavailable\n"); rc = 4; goto cleanup; }
    float prio = 1.0f;
    VkDeviceQueueCreateInfo dq = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                   .queueFamilyIndex = (uint32_t) qfam, .queueCount = 1, .pQueuePriorities = &prio };
    VkDeviceCreateInfo dc = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &dq };
    if (fnCreateDev(chosen, &dc, NULL, &dev) != 0 || !dev) {
        fprintf(stderr, "vulkan_exec_device=fail reason=vkCreateDevice failed\n"); rc = 4; goto cleanup;
    }

    /* 设备级函数指针（在 dev 创建后获取）*/
    vkCreateCommandPool_t fnCpool = (vkCreateCommandPool_t) gdpa(dev, "vkCreateCommandPool");
    vkAllocateCommandBuffers_t fnAlloc = (vkAllocateCommandBuffers_t) gdpa(dev, "vkAllocateCommandBuffers");
    vkBeginCommandBuffer_t fnBegin = (vkBeginCommandBuffer_t) gdpa(dev, "vkBeginCommandBuffer");
    vkEndCommandBuffer_t fnEnd = (vkEndCommandBuffer_t) gdpa(dev, "vkEndCommandBuffer");
    vkFreeCommandBuffers_t fnFree = (vkFreeCommandBuffers_t) gdpa(dev, "vkFreeCommandBuffers");
    vkDestroyCommandPool_t fnDestroyPool = (vkDestroyCommandPool_t) gdpa(dev, "vkDestroyCommandPool");
    vkCreateFence_t fnFence = (vkCreateFence_t) gdpa(dev, "vkCreateFence");
    vkWaitForFences_t fnWait = (vkWaitForFences_t) gdpa(dev, "vkWaitForFences");
    vkDestroyFence_t fnDestroyFence = (vkDestroyFence_t) gdpa(dev, "vkDestroyFence");
    vkQueueSubmit_t fnSubmit = (vkQueueSubmit_t) gdpa(dev, "vkQueueSubmit");
    if (!fnCpool || !fnAlloc || !fnBegin || !fnEnd || !fnFree || !fnDestroyPool ||
        !fnFence || !fnWait || !fnDestroyFence || !fnSubmit) {
        fprintf(stderr, "vulkan_exec_submit=fail reason=command/fence entry points unavailable\n"); rc = 5; goto cleanup;
    }
    fnGetQueue(dev, (uint32_t) qfam, 0, &queue);
    if (!queue) { fprintf(stderr, "vulkan_exec_queue=fail reason=vkGetDeviceQueue failed\n"); rc = 4; goto cleanup; }
    printf("vulkan_exec_queue=ok family=%d\n", qfam);

    VkCommandPoolCreateInfo pci = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = (uint32_t) qfam };
    if (fnCpool(dev, &pci, NULL, &pool) != 0 || !pool) { fprintf(stderr, "vulkan_exec_submit=fail reason=command pool creation\n"); rc = 5; goto cleanup; }
    VkCommandBufferAllocateInfo abi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
    if (fnAlloc(dev, &abi, &cmd) != 0 || !cmd) { fprintf(stderr, "vulkan_exec_submit=fail reason=command buffer allocation\n"); rc = 5; goto cleanup; }
    VkCommandBufferBeginInfo bbi = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    if (fnBegin(cmd, &bbi) != 0 || fnEnd(cmd) != 0) { fprintf(stderr, "vulkan_exec_submit=fail reason=begin/end command buffer\n"); rc = 5; goto cleanup; }
    VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = 0 };
    if (fnFence(dev, &fci, NULL, &fence) != 0 || !fence) { fprintf(stderr, "vulkan_exec_submit=fail reason=fence creation\n"); rc = 5; goto cleanup; }
    VkSubmitInfo si = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
    int sr = fnSubmit(queue, 1, &si, fence);
    if (sr != 0) { fprintf(stderr, "vulkan_exec_submit=fail reason=vkQueueSubmit rc=%d\n", sr); rc = 5; goto cleanup; }
    int wr = fnWait(dev, 1, &fence, 1, timeout_ns);
    if (wr == VK_TIMEOUT) { fprintf(stderr, "vulkan_exec_wait=fail reason=timeout after %llu ns\n", (unsigned long long) timeout_ns); rc = 5; goto cleanup; }
    if (wr != 0) { fprintf(stderr, "vulkan_exec_wait=fail reason=vkWaitForFences rc=%d\n", wr); rc = 5; goto cleanup; }
    printf("vulkan_exec_submit=ok\nvulkan_exec_wait=ok\nvulkan_exec_pass=1\n");
    rc = 0;

cleanup:
    /* 逆序释放：fence -> command buffer -> pool -> device -> instance -> loader */
    if (dev && fence && fnDestroyFence) fnDestroyFence(dev, fence, NULL);
    if (dev && pool && cmd && fnFree) fnFree(dev, pool, 1, &cmd);
    if (dev && pool && fnDestroyPool) fnDestroyPool(dev, pool, NULL);
    if (dev && fnDestroyDev) fnDestroyDev(dev, NULL);
    if (inst) { vkDestroyInstance_t fnDestroyInst = (vkDestroyInstance_t) gpa(inst, "vkDestroyInstance"); if (fnDestroyInst) fnDestroyInst(inst, NULL); }
    dlclose(lib);
    return rc;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "exec") == 0) return exec_main(argc, argv);
    return enumeration_main();
}
