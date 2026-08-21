/*************************************************************************/ /*!
@File           hal_interface.c
@Title
@Copyright      Copyright (c) Innosilicon Technology Ltd. All Rights Reserved
@Description
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "hal.h"
#include "hal_interface.h"
#include "inno_misc.h"
#include "hal_bmc.h"

INNO_EXT_SYM(fh2m_hal_get_pll);
INNO_EXT_SYM(fh2m_hal_set_pll);
INNO_EXT_SYM(fh2m_hal_set_pll_by_name);
INNO_EXT_SYM(fh2m_hal_get_gpu_irq_by_mode);
INNO_EXT_SYM(fh2m_hal_get_chip_alias);
INNO_EXT_SYM(fh2m_hal_is_low_power_chip);
INNO_EXT_SYM(fh2m_hal_get_gpu_utils);
INNO_EXT_SYM(fh2m_hal_set_gpu_utils_ops);
INNO_EXT_SYM(fh2m_hal_set_gpudrv_clkchange_ops);
INNO_EXT_SYM(fh2m_hal_gpudrv_clkchange);
INNO_EXT_SYM(fh2m_hal_power_init);
INNO_EXT_SYM(fh2m_hal_vol_is_digital);
INNO_EXT_SYM(fh2m_hal_pcie_speed_max_cap);
INNO_EXT_SYM(fh2m_is_support_update_voltage);
INNO_EXT_SYM(fh2m_hal_get_mod_pcie_drop_timeout);
INNO_EXT_SYM(fh2m_mod_update_voltage_disable);
INNO_EXT_SYM(fh2m_hal_power_deinit);
INNO_EXT_SYM(fh2m_hal_get_mcufw_release_time);
INNO_EXT_SYM(fh2m_hal_get_mem_chip);
INNO_EXT_SYM(fh2m_hal_mcufw_comm_get_version);
INNO_EXT_SYM(fh2m_hal_init_mcufw_comm);
INNO_EXT_SYM(fh2m_hal_deinit_mcufw_comm);
INNO_EXT_SYM(fh2m_hal_mcufw_comm_msg_xfer);
INNO_EXT_SYM(fh2m_hal_inv_vram_free);
INNO_EXT_SYM(fh2m_hal_get_vram_stats);
INNO_EXT_SYM(fh2m_hal_get_vram_layout);
INNO_EXT_SYM(fh2m_hal_get_bar0_reg_base);
INNO_EXT_SYM(fh2m_hal_get_bar0_reg_paddr);
INNO_EXT_SYM(fh2m_hal_get_maxsize_by_role);
INNO_EXT_SYM(fh2m_hal_get_inv_mem_total_size);
INNO_EXT_SYM(fh2m_hal_get_visable_mem_total_size);
INNO_EXT_SYM(fh2m_hal_get_vram_pcie_base);
INNO_EXT_SYM(fh2m_hal_get_gpu_core_nums);
INNO_EXT_SYM(fh2m_hal_get_gpu_mc_mode);
INNO_EXT_SYM(fh2m_hal_get_gpu_core_index);
INNO_EXT_SYM(fh2m_hal_get_nulldisplay);
INNO_EXT_SYM(fh2m_hal_get_nulldisplay_drm_pipe_num);
INNO_EXT_SYM(fh2m_hal_get_dpu_hw_support_gtt);
#if defined(__G0M_SOC__)
INNO_EXT_SYM(fh2m_hal_get_prohibit_umd_gtt_alloc);
#endif
INNO_EXT_SYM(fh2m_hal_set_gPVRDebugLevel);
INNO_EXT_SYM(fh2m_hal_get_gPVRDebugLevel);
INNO_EXT_SYM(fh2m_hal_get_inno_gfp_kernel);
INNO_EXT_SYM(fh2m_hal_get_s_dpu_debug);
INNO_EXT_SYM(fh2m_hal_get_s_max_width);
INNO_EXT_SYM(fh2m_hal_get_s_max_height);
INNO_EXT_SYM(fh2m_hal_get_s_vkms_width);
INNO_EXT_SYM(fh2m_hal_get_s_vkms_height);
INNO_EXT_SYM(fh2m_hal_audio_set_pll);
INNO_EXT_SYM(fh2m_hal_pdp_sys_reset);
INNO_EXT_SYM(fh2m_hal_pdp_video_set);
INNO_EXT_SYM(fh2m_hal_pdp_video_set_noclock);
INNO_EXT_SYM(fh2m_hal_subpdp_video_set);
INNO_EXT_SYM(fh2m_hal_get_dev_nums);
INNO_EXT_SYM(fh2m_hal_get_chiptype);
INNO_EXT_SYM(fh2m_hal_get_multi_memory_regions_en);
INNO_EXT_SYM(fh2m_hal_get_vfid);
INNO_EXT_SYM(fh2m_hal_get_memsize);
INNO_EXT_SYM(fh2m_hal_get_pcie_dma_stat);
INNO_EXT_SYM(fh2m_hal_getflag_dp2vga);
INNO_EXT_SYM(fh2m_hal_getflag_hdmi2dvi);
INNO_EXT_SYM(fh2m_hal_init_vram_total_size);
INNO_EXT_SYM(fh2m_hal_init_reserved_vram);
INNO_EXT_SYM(fh2m_hal_deinit_reserved_vram);
INNO_EXT_SYM(fh2m_hal_get_chip_static_info);
INNO_EXT_SYM(fh2m_hal_get_chip_gpuinfo);
INNO_EXT_SYM(fh2m_hal_get_chip_dyn_info);
INNO_EXT_SYM(fh2m_hal_get_chip_temperature);
INNO_EXT_SYM(fh2m_hal_get_board_temperature);
INNO_EXT_SYM(fh2m_hal_get_driver_info);
INNO_EXT_SYM(fh2m_hal_get_fw_env);
INNO_EXT_SYM(fh2m_hal_get_mcu_reserved_vram);
INNO_EXT_SYM(fh2m_hal_get_env_reserved_vram);
INNO_EXT_SYM(fh2m_hal_pmbus_nr_set);
INNO_EXT_SYM(fh2m_hal_pmbus_nr_get);
INNO_EXT_SYM(fh2m_hal_set_pmbus_adapter_and_funcs);
INNO_EXT_SYM(fh2m_hal_get_pmbus_adapter);
INNO_EXT_SYM(fh2m_hal_set_pmbus_freq);
INNO_EXT_SYM(fh2m_hal_get_volctrl_hwinfo);
INNO_EXT_SYM(fh2m_hal_get_init_gpuvol);
INNO_EXT_SYM(fh2m_hal_hw_thermal_type);
INNO_EXT_SYM(fh2m_hal_trigger_mcu_intr);
INNO_EXT_SYM(fh2m_hal_get_axi_latency_info);
INNO_EXT_SYM(fh2m_hal_get_axi_bandwidth_info);
INNO_EXT_SYM(fh2m_hal_get_ddr_bandwidth_info);
INNO_EXT_SYM(fh2m_hal_is_support_4k);
INNO_EXT_SYM(fh2m_hal_get_reg_addr);
INNO_EXT_SYM(fh2m_hal_reg_map_size_get);
INNO_EXT_SYM(fh2m_hal_reg_read8);
INNO_EXT_SYM(fh2m_hal_reg_read16);
INNO_EXT_SYM(fh2m_hal_reg_read32);
INNO_EXT_SYM(fh2m_hal_reg_write8);
INNO_EXT_SYM(fh2m_hal_reg_write16);
INNO_EXT_SYM(fh2m_hal_reg_write32);
INNO_EXT_SYM(fh2m_hal_reg_read32_offset);
INNO_EXT_SYM(fh2m_hal_reg_write32_offset);
INNO_EXT_SYM(fh2m_hal_reg_entity_addr);
INNO_EXT_SYM(fh2m_hal_get_powerinfo);
INNO_EXT_SYM(fh2m_hal_power_ops_init);
INNO_EXT_SYM(fh2m_hal_set_voltage);
INNO_EXT_SYM(fh2m_hal_get_gpufreq_info);
INNO_EXT_SYM(fh2m_hal_get_voltage);
INNO_EXT_SYM(fh2m_hal_get_voltage_from_bmc);
INNO_EXT_SYM(fh2m_get_pwr_debug_lvl);
INNO_EXT_SYM(fh2m_hal_is_enable_dyn_freq);
INNO_EXT_SYM(fh2m_hal_power_get_opptbl);
INNO_EXT_SYM(fh2m_hal_power_get_opptbl_size);
INNO_EXT_SYM(fh2m_hal_power_get_freq_offset);
INNO_EXT_SYM(fh2m_hal_gettype_hdmi);
INNO_EXT_SYM(fh2m_hal_gettype_dp);
INNO_EXT_SYM(fh2m_hal_gettype_lvds_type);
INNO_EXT_SYM(fh2m_hal_select_4k);
INNO_EXT_SYM(fh2m_hal_hdmi_edid_mode);
INNO_EXT_SYM(fh2m_hal_dp_edid_mode);
INNO_EXT_SYM(fh2m_hal_lvds_edid_mode);
INNO_EXT_SYM(fh2m_hal_hdmi_edid_data);
INNO_EXT_SYM(fh2m_hal_dp_edid_data);
INNO_EXT_SYM(fh2m_hal_lvds_edid_data);
INNO_EXT_SYM(fh2m_hal_has_gtt_mem);
INNO_EXT_SYM(fh2m_hal_has_limited_gtt_mem);
INNO_EXT_SYM(fh2m_hal_is_sharing_gpu_heap);
INNO_EXT_SYM(fh2m_hal_get_vga_hard_reset);
INNO_EXT_SYM(fh2m_hal_get_vga_reset_pin);
INNO_EXT_SYM(fh2m_hal_get_dp2vga_i2c_addr);
INNO_EXT_SYM(fh2m_hal_get_dp2vga_i2c_id);
INNO_EXT_SYM(fh2m_hal_get_dp2vga_chip_type);
INNO_EXT_SYM(fh2m_hal_get_hdmi_dp_en_status);
INNO_EXT_SYM(fh2m_hal_get_lvds_nums);
INNO_EXT_SYM(fh2m_hal_get_dev_id);
INNO_EXT_SYM(fh2m_hal_gpuchip_is_ovheat);
INNO_EXT_SYM(fh2m_hal_fanctrl_direction);
INNO_EXT_SYM(fh2m_hal_fanctrl_enable);
INNO_EXT_SYM(fh2m_hal_get_dynfreq_algo);
INNO_EXT_SYM(fh2m_hal_support_idle_feature);
INNO_EXT_SYM(fh2m_hal_get_tempctl_chip_params);
INNO_EXT_SYM(fh2m_hal_get_gpu_drop_freq);
INNO_EXT_SYM(fh2m_hal_get_gpu_recover_freq);
INNO_EXT_SYM(fh2m_hal_get_dbus_drop_freq);
INNO_EXT_SYM(fh2m_hal_get_dbus_recover_freq);
INNO_EXT_SYM(fh2m_hal_get_dyn_lpc_gpu_utils);
INNO_EXT_SYM(fh2m_hal_set_dyn_lpc_pcie_speed);
INNO_EXT_SYM(fh2m_hal_set_dyn_lpc_dbus_freq);
INNO_EXT_SYM(fh2m_hal_get_dvi_nums);
INNO_EXT_SYM(fh2m_hal_get_vga_nums);
INNO_EXT_SYM(fh2m_hal_get_dp_nums);
INNO_EXT_SYM(fh2m_hal_get_hdmi_nums);
INNO_EXT_SYM(fh2m_hal_show_hw_info);
INNO_EXT_SYM(fh2m_hal_get_mcufw_version);
INNO_EXT_SYM(fh2m_hal_get_gpu_stat);
INNO_EXT_SYM(fh2m_hal_get_gtt_dev_base);
INNO_EXT_SYM(fh2m_hal_get_lvds_vga_misc_en);
INNO_EXT_SYM(fh2m_hal_get_dual_link);
INNO_EXT_SYM(fh2m_hal_hwinfo_version);
INNO_EXT_SYM(fh2m_hal_hwinfo_get_item);
INNO_EXT_SYM(fh2m_hal_get_hwinfo_finished_status);
INNO_EXT_SYM(fh2m_hal_get_custominfo_finished_status);
INNO_EXT_SYM(fh2m_hal_sync_raw_intr);
INNO_EXT_SYM(fh2m_hal_set_gpu_raw_base);
INNO_EXT_SYM(fh2m_hal_get_gpu_feature);
INNO_EXT_SYM(fh2m_hal_dev_enable_irq);
INNO_EXT_SYM(fh2m_hal_dev_disable_irq);
INNO_EXT_SYM(fh2m_hal_set_irq_handler);
INNO_EXT_SYM(fh2m_cpu_paddr_to_dev_paddr);
INNO_EXT_SYM(fh2m_dev_paddr_to_cpu_paddr);
INNO_EXT_SYM(fh2m_hal_get_sys_reg_base);
INNO_EXT_SYM(fh2m_hal_get_ddrbase);
INNO_EXT_SYM(fh2m_hal_get_ddr_bar_len);
INNO_EXT_SYM(fh2m_hal_get_vram_dev_base);
INNO_EXT_SYM(fh2m_hal_vram_alloc);
INNO_EXT_SYM(fh2m_hal_vram_free);
INNO_EXT_SYM(fh2m_hal_get_vpu_vram_zone_base);
INNO_EXT_SYM(fh2m_hal_has_inv_mem);
INNO_EXT_SYM(fh2m_hal_get_gpu_info_hwinfo);
INNO_EXT_SYM(fh2m_hal_get_chip_name);
INNO_EXT_SYM(fh2m_hal_bmc_read32);
INNO_EXT_SYM(fh2m_hal_bmc_write32);

INNO_EXT_SYM(fh2m_hal_clean_irq_status);
INNO_EXT_SYM(fh2m_hal_zoom_is_enable);
INNO_EXT_SYM(fh2m_hal_get_dpu_match);
INNO_EXT_SYM(fh2m_hal_is_gtt_need_falling_back);
INNO_EXT_SYM(fh2m_hal_gettype_vga);
INNO_EXT_SYM(fh2m_hal_vga_edid_mode);
INNO_EXT_SYM(fh2m_hal_vga_edid_data);
INNO_EXT_SYM(fh2m_hal_get_odm_vendor);
INNO_EXT_SYM(fh2m_hal_get_pcb_version);
INNO_EXT_SYM(fh2m_hal_get_output_en);
INNO_EXT_SYM(fh2m_hal_get_output_mode);
INNO_EXT_SYM(fh2m_hal_get_backlight_mode);
INNO_EXT_SYM(fh2m_hal_set_dev_node);

INNO_EXT_SYM(fh2m_hal_detect_registered_interface);
INNO_EXT_SYM(fh2m_hal_set_interface_nums);
INNO_EXT_SYM(fh2m_hal_set_interface_info);
INNO_EXT_SYM(fh2m_hal_append_interface_info);
INNO_EXT_SYM(fh2m_hal_get_display_interface_info);

#if defined(CONFIG_NUMA) && defined(__INNO_CONTAINER__)
INNO_EXT_SYM(fh2m_bind_cpu_is_enable);
INNO_EXT_SYM(fh2m_hal_bind_numa_cpu_config_store);
INNO_EXT_SYM(fh2m_hal_bind_numa_cpu_config_show);
#endif //END CONFIG_NUMA __INNO_CONTAINER__

INNO_EXT_SYM(fh2m_hal_gtt_sort_enable);

/**************** HAL_VRAM ****************/
INNO_EXT_SYM(fh2m_pcie_paddr_to_cpu_paddr);
INNO_EXT_SYM(fh2m_cpu_paddr_to_pcie_paddr);
INNO_EXT_SYM(fh2m_cpu_paddr_to_gtt_paddr);
INNO_EXT_SYM(fh2m_gtt_paddr_to_cpu_paddr);

INNO_EXT_SYM(fh2m_hal_is_gtt_mem);
INNO_EXT_SYM(fh2m_hal_is_left_vram);
INNO_EXT_SYM(fh2m_hal_is_visible_vram);
INNO_EXT_SYM(fh2m_hal_is_invisible_vram);

/**************** HAL_DMA ****************/
INNO_EXT_SYM(fh2m_hal_is_support_dmar);
INNO_EXT_SYM(fh2m_hal_dma_map);
INNO_EXT_SYM(fh2m_hal_dma_unmap);
INNO_EXT_SYM(fh2m_hal_dma_transfer);
INNO_EXT_SYM(fh2m_hal_dma_init);
INNO_EXT_SYM(fh2m_hal_dma_deinit);
INNO_EXT_SYM(fh2m_hal_dma_suspend);
INNO_EXT_SYM(fh2m_hal_dma_resume);

#if defined(SRIOV_VF_MODE)
INNO_EXT_SYM(fh2m_hal_set_vf_extern_vram_size);
#endif

/*****************GTT ALLOC/FREE ******************/
INNO_EXT_SYM(fh2m_hal_gtt_alloc);
INNO_EXT_SYM(fh2m_hal_gtt_free);


/**************** HAL PHYSMEM ****************/
INNO_EXT_SYM(fh2m_hal_physmem_alloc);
INNO_EXT_SYM(fh2m_hal_physmem_free);
INNO_EXT_SYM(fh2m_hal_physmem_kernel_mapping);
INNO_EXT_SYM(fh2m_hal_physmem_kernel_unmapping);

/******************DMA VRAM POOL******************/
INNO_EXT_SYM(fh2m_hal_vram_dma_mem_pool_init);
INNO_EXT_SYM(fh2m_hal_vram_dma_mem_pool_deinit);
INNO_EXT_SYM(fh2m_hal_vram_dma_mem_pool_alloc);
INNO_EXT_SYM(fh2m_hal_vram_dma_mem_pool_free);
INNO_EXT_SYM(fh2m_hal_vram_dma_mem_pool_dev_paddr_to_virt_addr);

INNO_EXT_SYM(fh2m_hal_get_gtt_support);
INNO_EXT_SYM(fh2m_hal_get_smmu_support);

INNO_EXT_SYM(fh2m_hal_module_loadtime_register);
INNO_EXT_SYM(fh2m_hal_module_loadtime_get);


/****************** g3 add ******************/
INNO_EXT_SYM(fh2m_hal_set_gtt_write_mask_statistics);
INNO_EXT_SYM(fh2m_hal_get_gtt_write_mask_statistics);
INNO_EXT_SYM(fh2m_hal_get_gpu_raw_enabled);
INNO_EXT_SYM(fh2m_hal_raw_sync_init);
INNO_EXT_SYM(fh2m_hal_set_module_power_on);
INNO_EXT_SYM(fh2m_hal_set_module_power_off);
INNO_EXT_SYM(fh2m_hal_set_module_power_reset);
INNO_EXT_SYM(fh2m_hal_set_module_clk_off);
INNO_EXT_SYM(fh2m_hal_set_module_flr_reset);
INNO_EXT_SYM(fh2m_hal_set_smmu_ops);
INNO_EXT_SYM(fh2m_hal_get_smmu_ops);
INNO_EXT_SYM(fh2m_hal_get_innolink_chip_id);
INNO_EXT_SYM(fh2m_is_mod_update_voltage_enable);
/**************** INNO ML ****************/
INNO_EXT_SYM(fh2m_hal_get_and_deal_bmc_info);
INNO_EXT_SYM(fh2m_hal_bmc_get_val);
INNO_EXT_SYM(fh2m_hal_get_driver_version);
