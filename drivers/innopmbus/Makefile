INNOPMBUS_INCLUDE :=-I$(INNOGPU_PRO_PATH)/innopmbus
INNOPMBUS_OBJS = \
innopmbus/innopmbus_drv.o \

ifeq ($(SEPARATE_BUILD),y)
  innopmbus-y := $(INNOPMBUS_OBJS)
  obj-m += innopmbus.o
else
  INNOPMBUS := $(INNOPMBUS_OBJS)
endif
