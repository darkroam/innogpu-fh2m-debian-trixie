INNODMA_INCLUDE :=-I$(INNOGPU_PRO_PATH)/innodma
INNODMA_OBJS = \
innodma/virt_dma.o \
innodma/inno_axi_dma_drv.o \
innodma/inno_pcie_dma_drv.o \
innodma/innodma_debug.o \
innodma/innodma_drv.o \
innodma/innodma.o

ifeq ($(SEPARATE_BUILD),y)
  innodma-y := $(INNODMA_OBJS)
  obj-m += innodma.o
else
  INNODMA := $(INNODMA_OBJS)
endif
