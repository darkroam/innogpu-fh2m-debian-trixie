INNOVPU_INCLUDE :=-I$(INNOGPU_PRO_PATH)/innovpu
INNOVPU_OBJS = \
innovpu/innovpu_drv.o \
innovpu/innovpu_dmabuf.o \
innovpu/innovpu.o

ifeq ($(SEPARATE_BUILD),y)
  innovpu-y := $(INNOVPU_OBJS)
  obj-m += innovpu.o
else
  INNOVPU := $(INNOVPU_OBJS)
endif
