#ifndef GPU_INFO_INNOML_KM_H
#define GPU_INFO_INNOML_KM_H

#define INNOGPU_MAX_DEV_NUM 32
#define INNOGPU_DEVICE_NAME "innoml"
#define INNOGPU_DEVICE_FORMAT "/dev/innoml%d"

#define INNOGPU_UNSUPPORT_FIELD 0xFFFFFFFF
#define INNOML_DEVICE_PCI_BUS_ID_FMT                  "%08X:%02X:%02X.0"

#define MAX_GPU_CORES_PER_PCI_DEV 4

#define DEVICE_HEALTH_REASON_ASSERTED "Asserted"
#define DEVICE_HEALTH_REASON_POLL_FAILING "Poll failing"
#define DEVICE_HEALTH_REASON_TIMEOUTS "Global Event Object timeouts rising"
#define DEVICE_HEALTH_REASON_QUEUE_CORRUPT "KCCB offset invalid"
#define DEVICE_HEALTH_REASON_QUEUE_STALLED "KCCB stalled"
#define DEVICE_HEALTH_REASON_IDLING "Idling"
#define DEVICE_HEALTH_REASON_RESTARTING "Restarting"
#define DEVICE_HEALTH_REASON_MISSING_INTERRUPTS "Missing interrupts"
#define DEVICE_HEALTH_REASON_UNKNOW	"Unknown reason"

#define INNOGPU_IOC_NRBITS	    8
#define INNOGPU_IOC_TYPEBITS	8
#define INNOGPU_IOC_SIZEBITS	14
#define INNOGPU_IOC_DIRBITS     2

#define INNOGPU_IOC_NRMASK      ((1 << INNOGPU_IOC_NRBITS)-1)
#define INNOGPU_IOC_TYPEMASK    ((1 << INNOGPU_IOC_TYPEBITS)-1)
#define INNOGPU_IOC_SIZEMASK    ((1 << INNOGPU_IOC_SIZEBITS)-1)
#define INNOGPU_IOC_DIRMASK     ((1 << INNOGPU_IOC_DIRBITS)- 1)

#define INNOGPU_IOC_NRSHIFT     0
#define INNOGPU_IOC_TYPESHIFT   (INNOGPU_IOC_NRSHIFT + INNOGPU_IOC_NRBITS)
#define INNOGPU_IOC_SIZESHIFT   (INNOGPU_IOC_TYPESHIFT + INNOGPU_IOC_TYPEBITS)
#define INNOGPU_IOC_DIRSHIFT    (INNOGPU_IOC_SIZESHIFT + INNOGPU_IOC_SIZEBITS)

#define INNOGPU_IOC_NONE        0U
#define INNOGPU_IOC_READ        1U
#define INNOGPU_IOC_WRITE       2U

#define INNOGPU_IOC(dir,type,nr,size) \
        (((dir)  << INNOGPU_IOC_DIRSHIFT) | \
         ((type) << INNOGPU_IOC_TYPESHIFT) | \
         ((nr)   << INNOGPU_IOC_NRSHIFT) | \
         ((size) << INNOGPU_IOC_SIZESHIFT))

#define INNOGPU_IO(type,nr)        INNOGPU_IOC(INNOGPU_IOC_NONE,(type),(nr),0)
#define INNOGPU_IOR(type,nr,size)  INNOGPU_IOC(INNOGPU_IOC_READ,(type),(nr),sizeof(size))
#define INNOGPU_IOW(type,nr,size)  INNOGPU_IOC(INNOGPU_IOC_WRITE,(type),(nr),sizeof(size))
#define INNOGPU_IOWR(type,nr,size) INNOGPU_IOC(INNOGPU_IOC_READ|INNOGPU_IOC_WRITE,(type),(nr),sizeof(size))

#define INNOGPU_IOC_NR(nr)         (((nr) >> INNOGPU_IOC_NRSHIFT) & INNOGPU_IOC_NRMASK)
#define INNOGPU_IOC_SIZE(nr)  (((nr) >> INNOGPU_IOC_SIZESHIFT) & INNOGPU_IOC_SIZEMASK)

#define INNOGPU_IOC_IN          (INNOGPU_IOC_WRITE << INNOGPU_IOC_DIRSHIFT)
#define INNOGPU_IOC_OUT         (INNOGPU_IOC_READ << INNOGPU_IOC_DIRSHIFT)
#define INNOGPU_IOC_INOUT       ((INNOGPU_IOC_WRITE|INNOGPU_IOC_READ) << INNOGPU_IOC_DIRSHIFT)

#define INNOML_MAGIC 'i'
#define INNOML_IO(nr)			INNOGPU_IO(INNOML_MAGIC,nr)
#define INNOML_IOR(nr,type)		INNOGPU_IOR(INNOML_MAGIC,nr,type)
#define INNOML_IOW(nr,type)		INNOGPU_IOW(INNOML_MAGIC,nr,type)
#define INNOML_IOWR(nr,type)	INNOGPU_IOWR(INNOML_MAGIC,nr,type)

typedef struct innomlIoctlVersion_tag
{
    unsigned char major;
    unsigned char minor;
    unsigned char revision;
} InnomlIoctlVersion;

typedef struct innomlVersion_tag
{
    char devName[16];
    char bvnc[32];
    char driverVersion[32];
    char driverReleaseTime[32];
    char fwVersion[16];
    char fwReleaseTime[32];
    char vbiosVersion[16];
    char pcbVersion[28];
    char hwinfoODMVersion[32];
    char customODMVersion[32];
} InnomlVersion;

typedef enum _innoGpuArch
{
    INNO_G0 = 0,
    INNO_G0M,
    INNO_G1,
    INNO_G1P,
    INNO_UNKNOW_ARCH
} InnoGpuArch;
typedef struct innomlPciInfo_tag
{
    unsigned int domain;             //!< The PCI domain on which the device's bus resides, 0 to 0xffffffff
    unsigned int bus;                //!< The bus on which the device resides, 0 to 0xff
    unsigned int device;             //!< The device's id on the bus, 0 to 31

    unsigned int pciDeviceId;        //!< The combined 16-bit device id and 16-bit vendor id
    unsigned int pciSubSystemId;     //!< The 32-bit Sub System Device ID

    unsigned int baseClass;          //!< The 8-bit PCI base class code
    unsigned int subClass;           //!< The 8-bit PCI sub class code

    unsigned int pciLinkLanes;
    unsigned int pciLinkStatus;

    unsigned long memBarAddr;
    unsigned long memBarLen;

    unsigned long regBarAddr;
    unsigned long regBarLen;
    char busId[32]; //!< The tuple domain:bus:device.function PCI identifier (&amp; NULL terminator)
    unsigned int maxPciLinkLanes;
    unsigned int generation;
    unsigned int speed;
    unsigned int maxGeneration;
    unsigned int maxSpeed;
} InnomlPciInfo;

typedef enum _deviceStatus
{
    DEVICE_OK = 0,
    NOT_RESPONDING,
    DEAD,
    FAULT,
    UNDEFINED,
    UNKNOWN
} deviceStatus;

typedef enum _deviceHealthReason
{
    DEVICE_NONE = 0,
    DEVICE_ASSERTED,
    DEVICE_POLL_FAILING,
    DEVICE_TIMEOUTS,
    DEVICE_QUEUE_CORRUPT,
    DEVICE_QUEUE_STALLED,
    DEVICE_IDLING,
    DEVICE_RESTARTING,
    DEVICE_MISSING_INTERRUPTS,
    DEVICE_UNKNOW_REASON
} deviceHealthReason;

typedef struct innomlGpuStatus_item_tag
{
    deviceStatus devStatus;
    deviceHealthReason devHealthReason;
    unsigned int serverEventCount;
    unsigned int hwrEventCount;
    unsigned int crrErrCount;
    unsigned int slrErrCount;
    unsigned int wgpErrCount;
    unsigned int trpErrCount;
    unsigned int fwfErrCount;
    unsigned int apmErrCount;
} InnomlGpuStatus_item;

typedef struct innomlGpuStatus_tag {
	InnomlGpuStatus_item gpustatus[MAX_GPU_CORES_PER_PCI_DEV];
} InnomlGpuStatus;

typedef struct innomlGpuUtilization_item_tag
{
    unsigned int gpuUtilization;
    unsigned int tdmUtilization;
    unsigned int geomUtilization;
    unsigned int threedUtilization;
    unsigned int cdmUtilization;
    unsigned int rayUtilization;
    unsigned int geom2Utilization;
} InnomlGpuUtilization_item;

typedef struct innomlGpuUtilization_tag {
	InnomlGpuUtilization_item gpuUtil[MAX_GPU_CORES_PER_PCI_DEV];
} InnomlGpuUtilization;

typedef struct cpuAffinity_tag
{
    int first_cpu;
	int last_cpu;
} CpuAffinity;

typedef struct innomlGpuInfo_item_tag {
	// printable device name
	char deviceName[32];
	unsigned int gpuId;
	unsigned int renderId;
} InnomlGpuInfo_item;

typedef struct innomlGpuInfo_tag
{
    // printable gpu name
    char name[16];
    // Uuid of the physical GPU
    char uuid[16];
    InnomlGpuInfo_item gpuinfo[MAX_GPU_CORES_PER_PCI_DEV];
    unsigned int gpuCoreNum;
    unsigned int gpuArch;
    unsigned int enableGtt;
    unsigned int numaId;
    CpuAffinity cpuAffinity;
    char manufactoryName[32];
    char productName[32];
    char productNumber[28];
    char serialNumber[32];
} InnomlGpuInfo;

typedef struct innomlTemperatureInfo_tag
{
    int boardTemperature;
    int chipTemperature;
    int fanSpeed;
} InnomlTemperatureInfo;

typedef struct innomlTemperatureState_tag
{
    int overTemperatureState;  // over-temperature shutdown
    int highTemperatureState;  // high temperature warning
    int boardTemperatureState; // read board temperature error
    int boardFanState;         // read board fan error
} InnomlTemperatureState;

typedef struct innomlClocksInfo_tag
{
    int gpuClock;  // Mhz
    int memoryClock;  // Mhz
    int videoClock; // Mhz
    int maxGpuClock;  // Mhz
    int minGpuClock;  // Mhz
} InnomlClocksInfo;

typedef enum {
    INNO_GPU,
    INNO_MEM,
    INNO_VIDEO
} INNOGPU_CLOCK_IP;

typedef struct innomlSetClockParams_tag
{
    INNOGPU_CLOCK_IP ip;
    int clockRank;
} InnomlSetClockParams;

typedef struct innomlVoltageInfo_tag
{
    int gpuVoltage;  // mV
} InnomlVoltageInfo;

typedef struct innomlPowerInfo_tag
{
    int gpuPower;  // W
} InnomlPowerInfo;

typedef struct innomlVramInfo_item_tag
{
	unsigned long long totalSize;
	unsigned long long used;
	unsigned long long free;
} InnomlVramInfo_item;

typedef struct innomlVramInfo_tag
{
    InnomlVramInfo_item vraminfo[MAX_GPU_CORES_PER_PCI_DEV];
    char name[32];
    char type[16];
    unsigned int speed;
    unsigned int number;
    unsigned int sideFlag;
	unsigned  int bitWidth;
} InnomlVramInfo;

#define MAX_VPU_NUM 6
typedef struct innomlVpuMemInfo_tag
{
    unsigned long long totalSize;
    unsigned long long used;
    unsigned long long free;
    unsigned int encUtil[MAX_VPU_NUM];
    unsigned int encUtilNum;
    unsigned int decUtil[MAX_VPU_NUM];
    unsigned int decUtilNum;
} InnomlVpuMemInfo;

typedef enum {
    PATH_SELF,     // Local (myself)
    PATH_INNOLINK, // Connection traversing innolink
    PATH_PIX,      // Connection traversing at most a single PCIe bridge
    PATH_PXB,      // Connection traversing multiple PCIe bridges (without traversing the PCIe Host Bridge)
    PATH_PHB,      // Connection traversing multiple PCIe bridges (without traversing the PCIe Host Bridge)
    PATH_SYS,      // Connection traversing PCIe as well as the SMP interconnect between NUMA nodes (e.g., QPI/UPI)
} INNOGPU_P2P_LINKTYPE;


typedef struct innomlP2PCaps_tag
{
    int targetGpuId;
    int p2pEnable;
    INNOGPU_P2P_LINKTYPE linkType;
    int p2pAtomicEnable;
} InnomlP2PCaps;

#define MAX_PID_NUM 64

typedef struct innomlPids_tag
{
    int pid[MAX_PID_NUM];
    int count;
} InnomlPids;

typedef struct innomlPidMemUsage_tag
{
    int pid;
    unsigned long long vramUsed;
    unsigned long long gttUsed;
	char pidName[256];
} InnomlPidMemInfo;

typedef enum {
    INNOGPU_POWER_STATE_DEFAULT,
    INNOGPU_POWER_STATE_OFF,
    INNOGPU_POWER_STATE_ON,
    INNOGPU_POWER_STATE_UNKNOW,
} INNOGPU_POWER_STATE;

typedef struct innomlPowerState_tag
{
    INNOGPU_POWER_STATE state;
} InnomlPowerState;

typedef enum {
	INNOGPU_RESET_SUCCESS,
    INNOGPU_RESET_FAIL,
    INNOGPU_RESET_IN_PROGRESS,
    INNOGPU_RESET_POLL_FAIL,
} INNOGPU_RESET_STATE;

typedef struct InnomlResetGPUResult_tag
{
    INNOGPU_RESET_STATE status[MAX_GPU_CORES_PER_PCI_DEV];
} InnomlResetGPUResult;

#define INNOGPU_IOCTL_GET_IOCTL_VERSION   INNOML_IOR(0x0, InnomlIoctlVersion)
#define INNOGPU_IOCTL_GET_VERSION         INNOML_IOR(0x1, InnomlVersion)
#define INNOGPU_IOCTL_GET_GPU_STATUS      INNOML_IOR(0x2, InnomlGpuStatus)
#define INNOGPU_IOCTL_GET_PCI_INFO        INNOML_IOR(0x3, InnomlPciInfo)
#define INNOGPU_IOCTL_GET_GPU_INFO        INNOML_IOR(0x4, InnomlGpuInfo)
#define INNOGPU_IOCTL_GET_TEMP_INFO       INNOML_IOR(0x5, InnomlTemperatureInfo)
#define INNOGPU_IOCTL_GET_TEMP_STATE      INNOML_IOR(0x6, InnomlTemperatureState)
#define INNOGPU_IOCTL_GET_CLOCKS_INFO     INNOML_IOR(0x7, InnomlClocksInfo)
#define INNOGPU_IOCTL_GET_GPU_UTILIZATION INNOML_IOR(0x8, InnomlGpuUtilization)
#define INNOGPU_IOCTL_GET_VOLTAGE_INFO    INNOML_IOR(0x9, InnomlVoltageInfo)
#define INNOGPU_IOCTL_GET_POWER_INFO      INNOML_IOR(0xa, InnomlPowerInfo)
#define INNOGPU_IOCTL_GET_VRAM_INFO       INNOML_IOR(0xb, InnomlVramInfo)
#define INNOGPU_IOCTL_GET_VPU_MEM_INFO    INNOML_IOR(0xc, InnomlVpuMemInfo)
#define INNOGPU_IOCTL_SET_CLOCK           INNOML_IOW(0xd, InnomlSetClockParams)
#define INNOGPU_IOCTL_GET_P2P_CAPS        INNOML_IOWR(0xe, InnomlP2PCaps)
#define INNOGPU_IOCTL_GET_PIDS            INNOML_IOR(0xf, InnomlPids)
#define INNOGPU_IOCTL_GET_PID_MEM_USAGE   INNOML_IOWR(0x10, InnomlPidMemInfo)
#define INNOGPU_IOCTL_GET_POWER_STATE     INNOML_IOWR(0x11, InnomlPowerState)
#define INNOGPU_IOCTL_RESET               INNOML_IOWR(0x12, InnomlResetGPUResult)


/********************************************
 * the INNOML_IOCTL_VERSION* is must be set,
 * when the file is changed.
 * If you are not sure whether your changed to
 * affect compatibility, please increase
 * the VERSION_REVISION whatever any-changed.
 ********************************************/
//Framework changed
#define INNOML_IOCTL_VERSION_MAJOR     (1U)
//Function changed or added
#define INNOML_IOCTL_VERSION_MINOR     (0U)
//any-changed about innoml
#define INNOML_IOCTL_VERSION_REVISION  (0U)

#endif
