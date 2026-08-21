INNOSMMU_INCLUDE :=-I$(INNOGPU_PRO_PATH)/innosmmu
INNOSMMU_OBJS = \
innosmmu/innosmmu_drv.o \
innosmmu/innosmmu.o

ifeq ($(SEPARATE_BUILD),y)
  innosmmu-y := $(INNOSMMU_OBJS)
  obj-m += innosmmu.o
else
  INNOSMMU := $(INNOSMMU_OBJS)
endif
