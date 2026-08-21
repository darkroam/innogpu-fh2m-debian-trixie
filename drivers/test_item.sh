TEST_ITEM:{
	CODE="#include <asm/io.h>
	void inno_testconf_ioremap_nocache(void) {
		ioremap_nocache();
	}"
	DEFINE="INNOGPU_IOREMAP_NOCACHE_PRESENT"
	VAL=""
	TYPE="fail_define"
	DEBUG="false"
}

TEST_ITEM:{
	CODE="#include <asm/io.h>
	void inno_testconf_ioremap_cache(void) {
		ioremap_cache();
	}"
	DEFINE="INNOGPU_IOREMAP_CACHE_PRESENT"
	VAL=""
	TYPE="fail_define"
	DEBUG="false"
}

TEST_ITEM:{
	CODE="#include <asm/io.h>
	void inno_testconf_ioremap_wc(void) {
		ioremap_wc();
	}"
	DEFINE="INNOGPU_IOREMAP_WC_PRESENT"
	VAL=""
	TYPE="fail_define"
	DEBUG="false"
}

TEST_ITEM:{
	CODE="#include <drm/drm_probe_helper.h>
	void inno_testconf_drm_probe_helper(void) {

	}"
	DEFINE="INNOGPU_DRM_PROBE_HELPER_PRESENT"
	VAL=""
	TYPE="success_define"
	DEBUG="false"
}

TEST_ITEM:{
	CODE="#include <drm/drmP.h>
	void inno_testconf_drmP(void) {

	}"
	DEFINE="INNOGPU_DRMP_PRESENT"
	VAL=""
	TYPE="success_define"
	DEBUG="false"
}

TEST_ITEM:{
	CODE="#include <linux/dma-buf.h>
	void inno_testconf_dma_buf_ops(void) {
		struct dma_buf_ops ops;
		ops.map = NULL;
	}"
	DEFINE="INNOGPU_DMA_BUF_OPS_MAP_PRESENT"
	VAL=""
	TYPE="success_define"
	DEBUG="false"
}

TEST_ITEM:{
	CODE="#include <linux/kernel.h>
	void inno_testconf_do_exit(void) {
			do_exit(0);
	}"
	DEFINE="INNOGPU_DO_EXIT_PRESENT"
	VAL=""
	TYPE="success_define"
	DEBUG="false"
	NEED_LINK="true"
}

TEST_ITEM:{
    CODE="#include <linux/compiler.h>
    #include <linux/uaccess.h>
    void inno_testaccess_ok(void) {
        access_ok(0,(char*)0,0);
    }"
    DEFINE="INNOGPU_ACCESS_OK_2PARAM_PRESENT"
    VAL=""
    TYPE="fail_define"
    DEBUG="false"
}

TEST_ITEM:{
    CODE="#include <linux/mm.h>
    void inno_testvmf_insert_mixed(void) {
        vmf_insert_mixed();
    }"
    DEFINE="INNOGPU_VMF_INSERT_MIXED_PRESENT"
    VAL=""
    TYPE="fail_define"
    DEBUG="false"
}

TEST_ITEM:{
    CODE="#include <linux/vmalloc.h>
    void inno_testunmap_kernel_range(void) {
        unmap_kernel_range(0,0);
    }"
    DEFINE="INNOGPU_UNMAP_KERNEL_RANGE_NOPRESENT"
    VAL=""
    TYPE="fail_define"
    DEBUG="false"
    NEED_LINK="true"
}

TEST_ITEM:{
    CODE="
    void inno_testion_client_create(void) {
        ion_client_create();
    }"
    DEFINE="INNOGPU_ION_CLIENT_PRESENT"
    VAL=""
    TYPE="fail_define"
    DEBUG="false"
}

TEST_ITEM:{
    CODE="#include <linux/workqueue.h>
    void inno_testconf_atrribute_warning(void) {
        flush_workqueue(system_wq);
    }"
    DEFINE="INNOGPU_ATRRIBUTE_WARNING_PRESENT"
    VAL=""
    TYPE="fail_define"
    DEBUG="false"
}

TEST_ITEM:{
    CODE="#include <linux/mmzone.h>
    void inno_testconf_get_order(void) {
        if(MAX_ORDER);
    }"
    DEFINE="INNOGPU_MAX_ORDER_PRESENT"
    VAL=""
    TYPE="success_define"
    DEBUG="false"
}

TEST_ITEM:{
    CODE="#include <linux/atomic.h>
    #include <linux/types.h>
    /* include time.h for CentOS Linux 4.18.0-80.el8.x86_64,
     * that indirectly include rh_kabi.h which define RH_KABI_FILE_HOLE used by shrinker.h.
     */
    #include <linux/time.h>
    #include <linux/shrinker.h>
    void inno_testconf_get_shrinker(void) {
        shrinker_register();
    }"
    DEFINE="INNOGPU_SHRINKER_REGISTER_PRESENT"
    VAL=""
    TYPE="fail_define"
    DEBUG="false"
}

TEST_ITEM:{
    CODE="#include <drm/drm_file.h>
    void inno_testconf_get_debugfs(void) {
        struct drm_minor minor;
		minor.debugfs_lock = NULL;
    }"
    DEFINE="INNOGPU_DEBUGFS_PRESENT"
    VAL=""
    TYPE="success_define"
    DEBUG="false"
}
