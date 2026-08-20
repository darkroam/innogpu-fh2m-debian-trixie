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

int main(void) {
    void *lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_GLOBAL);
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
