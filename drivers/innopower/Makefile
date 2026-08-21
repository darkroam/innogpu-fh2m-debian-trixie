INNOPOWER_INCLUDE :=-I$(INNOGPU_PRO_PATH)/innopower
INNOPOWER_OBJS = \
innopower/inno_mp2979a.o \
innopower/inno_mpq8645p.o \
innopower/inno_virture_chip.o \
innopower/inno_xdpe12284.o \
innopower/inno_gs8601.o \
innopower/inno_is6608a.o \
innopower/power_hw_info.o \
innopower/inno_devfreq_gov.o \
innopower/inno_input_event.o \
innopower/innopower.o \

ifeq ($(SEPARATE_BUILD),y)
  innopower-y := $(INNOPOWER_OBJS)
  obj-m += innopower.o
else
  INNOPOWER := $(INNOPOWER_OBJS)
endif
