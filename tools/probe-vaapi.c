/*************************************************************************/ /*!
@File		probe-vaapi.c
@Title		Minimal VA-API driver/profile enumerator (no libva headers)
@Description	Open the DRM render node, dlopen libva, and enumerate the VA-API
		driver, supported profiles and their entrypoints. Read-only; creates
		no surfaces/contexts and performs no encoding/decoding.
@License	Public domain / project tools convention
*/ /**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdint.h>

typedef void *VADisplay;
typedef int VAStatus;

/* entry points / profile constants (subset of va/va.h) */
#define VAEntrypointVLD          1
#define VAEntrypointEncSlice     7
#define VAEntrypointEncPicture    8
#define VAEntrypointVideoProc    10
#define VAEntrypointProtectedContent 12

typedef VADisplay (*vaGetDisplayDRM_t)(int fd);
typedef VAStatus (*vaInitialize_t)(VADisplay, int *, int *);
typedef VAStatus (*vaTerminate_t)(VADisplay);
typedef const char *(*vaQueryVendorString_t)(VADisplay);
typedef int (*vaMaxNumEntrypoints_t)(VADisplay);
typedef VAStatus (*vaQueryConfigProfiles_t)(VADisplay, int *, int *);
typedef VAStatus (*vaQueryConfigEntrypoints_t)(VADisplay, int, int *, int *);

static const char *profile_name(int p) {
    /* Values per the authoritative libva VAProfile enum (va/va.h). */
    switch (p) {
        case -1: return "None(VideoProc)";
        case 0: return "MPEG2Simple";
        case 1: return "MPEG2Main";
        case 2: return "MPEG4Simple";
        case 3: return "MPEG4AdvancedSimple";
        case 4: return "MPEG4Main";
        case 5: return "H264Baseline(deprecated)";
        case 6: return "H264Main";
        case 7: return "H264High";
        case 8: return "VC1Simple";
        case 9: return "VC1Main";
        case 10: return "VC1Advanced";
        case 11: return "H263Baseline";
        case 12: return "JPEGBaseline";
        case 13: return "H264ConstrainedBaseline";
        case 14: return "VP8Version0_3";
        case 15: return "H264MultiviewHigh";
        case 16: return "H264StereoHigh";
        case 17: return "HEVCMain";
        case 18: return "HEVCMain10";
        case 19: return "VP9Profile0";
        case 20: return "VP9Profile1";
        case 21: return "VP9Profile2";
        case 22: return "VP9Profile3";
        case 23: return "HEVCMain12";
        case 24: return "HEVCMain422_10";
        case 25: return "HEVCMain422_12";
        case 26: return "HEVCMain444";
        case 27: return "HEVCMain444_10";
        case 28: return "HEVCMain444_12";
        case 29: return "HEVCSccMain";
        case 30: return "HEVCSccMain10";
        case 31: return "HEVCSccMain444";
        case 32: return "AV1Profile0";
        case 33: return "AV1Profile1";
        case 34: return "HEVCSccMain444_10";
        case 35: return "Protected";
        case 36: return "H264High10";
        case 37: return "VVCMain10";
        case 38: return "VVCMultilayerMain10";
        case 39: return "AV1Profile2";
        case 40: return "H264High422";
        default: return "?";
    }
}
static const char *entry_name(int e) {
    switch (e) {
        case 1: return "VLD(dec)";
        case 7: return "EncSlice";
        case 8: return "EncPicture";
        case 10: return "VideoProc";
        case 12: return "ProtectedContent";
        default: return "?";
    }
}

int main(void) {
    const char *nodes[] = { "/dev/dri/renderD128", "/dev/dri/card0", "/dev/dri/card1" };
    int fd = -1;
    for (unsigned i = 0; i < sizeof nodes / sizeof *nodes; i++) {
        fd = open(nodes[i], O_RDWR);
        if (fd >= 0) { printf("DRM node: %s\n", nodes[i]); break; }
    }
    if (fd < 0) { printf("FATAL: no openable DRM node (no /dev/dri access)\n"); return 2; }

    void *va = dlopen("libva.so.2", RTLD_NOW | RTLD_GLOBAL);
    void *vadrm = dlopen("libva-drm.so.2", RTLD_NOW | RTLD_GLOBAL);
    if (!va || !vadrm) {
        printf("FATAL: libva not loadable: %s\n", va ? dlerror() : "libva.so.2 missing");
        close(fd);
        return 2;
    }
    vaGetDisplayDRM_t getdisp = (vaGetDisplayDRM_t) dlsym(vadrm, "vaGetDisplayDRM");
    vaInitialize_t init = (vaInitialize_t) dlsym(va, "vaInitialize");
    vaTerminate_t term = (vaTerminate_t) dlsym(va, "vaTerminate");
    vaQueryVendorString_t vendor = (vaQueryVendorString_t) dlsym(va, "vaQueryVendorString");
    vaMaxNumEntrypoints_t maxep = (vaMaxNumEntrypoints_t) dlsym(va, "vaMaxNumEntrypoints");
    vaQueryConfigProfiles_t profiles = (vaQueryConfigProfiles_t) dlsym(va, "vaQueryConfigProfiles");
    vaQueryConfigEntrypoints_t entrypoints = (vaQueryConfigEntrypoints_t) dlsym(va, "vaQueryConfigEntrypoints");
    if (!getdisp || !init || !vendor || !profiles || !entrypoints) {
        printf("FATAL: missing libva entry points\n");
        close(fd);
        return 2;
    }
    VADisplay disp = getdisp(fd);
    if (!disp) { printf("FATAL: vaGetDisplayDRM failed\n"); close(fd); return 2; }
    int major = 0, minor = 0;
    VAStatus st = init(disp, &major, &minor);
    printf("vaInitialize: status=%d libva=%d.%d\n", st, major, minor);
    if (st != 0) { close(fd); return 1; }
    printf("vendor: %s\n", vendor(disp));

    int nprof = 0;
    st = profiles(disp, NULL, &nprof);
    if (st != 0 || nprof <= 0) {
        /* Some drivers do not support the count-first pattern; retry with a
         * fixed-size buffer so the profile query still works. */
        printf("count query returned status=%d, retrying with fixed buffer\n", st);
        nprof = 64;
        int *try_list = calloc((size_t) nprof, sizeof(int));
        st = profiles(disp, try_list, &nprof);
        if (st != 0 || nprof <= 0) {
            printf("no profiles (status=%d)\n", st);
            free(try_list);
            term(disp); close(fd); return 0;
        }
        free(try_list);
    }
    int *plist = calloc((size_t) nprof, sizeof(int));
    st = profiles(disp, plist, &nprof);
    printf("profiles (%d):\n", nprof);
    for (int i = 0; i < nprof; i++) {
        int nep = 0;
        if (entrypoints && maxep) {
            int cap = maxep(disp);
            int *elist = calloc((size_t) cap ? (size_t) cap : 1, sizeof(int));
            nep = cap;
            VAStatus es = entrypoints(disp, plist[i], elist, &nep);
            printf("  %-22s [%d] entrypoints:", profile_name(plist[i]), plist[i]);
            if (es == 0) {
                for (int e = 0; e < nep; e++) printf(" %s", entry_name(elist[e]));
            } else {
                printf(" (err %d)", es);
            }
            printf("\n");
            free(elist);
        } else {
            printf("  %s [%d]\n", profile_name(plist[i]), plist[i]);
        }
    }
    free(plist);

    /* Probe common profiles directly even if not advertised, to check whether
     * encode/decode is available for typical codecs. */
    static const int probe_profiles[] = {
        6 /*H264Main*/, 7 /*H264High*/, 13 /*H264ConstrainedBaseline*/,
        17 /*HEVCMain*/, 18 /*HEVCMain10*/, 19 /*VP9Profile0*/,
        32 /*AV1Profile0*/, 12 /*JPEGBaseline*/
    };
    printf("direct probes (even if not advertised):\n");
    for (unsigned i = 0; i < sizeof probe_profiles / sizeof *probe_profiles; i++) {
        int cap = maxep ? maxep(disp) : 32;
        int *elist = calloc((size_t) cap ? (size_t) cap : 1, sizeof(int));
        int nep = cap;
        VAStatus es = entrypoints(disp, probe_profiles[i], elist, &nep);
        printf("  %-22s [%d]:", profile_name(probe_profiles[i]), probe_profiles[i]);
        if (es == 0 && nep > 0) {
            for (int e = 0; e < nep; e++) printf(" %s", entry_name(elist[e]));
            printf("\n");
        } else {
            printf(" (status=%d)\n", es);
        }
        free(elist);
    }
    term(disp);
    close(fd);
    return 0;
}
