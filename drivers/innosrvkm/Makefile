INNOSRVKM_INCLUDE :=-I$(INNOGPU_PRO_PATH)/innosrvkm/include
INNOSRVKM_OBJS = \
innosrvkm/trace_events.o \
innosrvkm/event.o \
innosrvkm/pvr_bridge_k.o \
innosrvkm/pvr_buffer_sync.o \
innosrvkm/pvr_counting_timeline.o \
innosrvkm/pvr_drm.o \
innosrvkm/pvr_fence.o \
innosrvkm/pvr_export_fence.o \
innosrvkm/pvr_gputrace.o \
innosrvkm/pvr_platform_drv.o \
innosrvkm/pvr_sw_fence.o \
innosrvkm/pvr_sync_file.o \
innosrvkm/pvr_dvfs_device.o \
innosrvkm/pvr_sync_ioctl_common.o \
innosrvkm/pvr_sync_ioctl_drm.o \
innosrvkm/inno_physmem_dmabuf_helper.o \
innosrvkm/inno_debugfs.o \
innosrvkm/inno_pmr.o \
innosrvkm/inno_trace.o \
innosrvkm/inno_pvr_spec.o \
innosrvkm/inno_apphint.o \
innosrvkm/pvr_debugfs.o \
innosrvkm/pvr_procfs.o \
innosrvkm/innodpu_dp.o \
innosrvkm/innodpu_hdmi_audio_api.o \
innosrvkm/innodpu_dp_audio_api.o \
innosrvkm/innodpu_hdmi.o \
innosrvkm/innodpu_audio_api.o \
innosrvkm/innodpu_compatibility.o \
innosrvkm/innodpu_dpu.o \
innosrvkm/innodpu_drm_drv.o \
innosrvkm/innodpu_drm_fb.o \
innosrvkm/innodpu_drm_gem.o \
innosrvkm/innodpu_drm_pm.o \
innosrvkm/innodpu_drm_modeset.o \
innosrvkm/pdp0_crtc.o \
innosrvkm/pdp0_drv.o \
innosrvkm/pdp0_plane.o \
innosrvkm/pdp0_common.o \
innosrvkm/innodpu_dp_common.o \
innosrvkm/innodpu_hdmi_common.o \
innosrvkm/innodpu_parse_edid.o \
innosrvkm/innodpu_connector.o \
innosrvkm/innodpu_hdmi_debugfs.o \
innosrvkm/innodpu_drm_debugfs.o \
innosrvkm/innodpu_dp_debugfs.o \
innosrvkm/innodpu_vga.o \
innosrvkm/innodpu_vkms.o \
innosrvkm/innodpu_vga_debugfs.o \
innosrvkm/innodpu_module_param.o \
innosrvkm/innodpu_panel_backlight.o \
innosrvkm/innodpu_common_drm_panel.o \
innosrvkm/innodpu_panel_pwr.o \
innosrvkm/g3_ne_hdmi.o \
innosrvkm/gen_g3_ne_hdmi.o \
innosrvkm/innodpu_pmbus_fpga.o \
innosrvkm/g3_pdp0_hw.o \
innosrvkm/innogpu_drm.o \
innosrvkm/innoaudio_chip_common.o \
innosrvkm/innoaudio_drv.o \
innosrvkm/innoaudio_print.o \
innosrvkm/innosrvkm.o

ifeq ($(SEPARATE_BUILD),y)
  innosrvkm-y := $(INNOSRVKM_OBJS)
  obj-m += innosrvkm.o
else
  INNOSRVKM := $(INNOSRVKM_OBJS)
endif
