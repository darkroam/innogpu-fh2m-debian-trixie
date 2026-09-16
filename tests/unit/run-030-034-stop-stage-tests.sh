#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/030-034-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT
cd "$ROOT"
tar --use-compress-program=zstd -xf docs/planning/evidence/o-stage/5.0.0-i5/o-stage-snapshot.tar.zst -C "$TMP"
patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$TMP/o-stage" -p1 < patches/030-034.patch
python3 - "$TMP/o-stage" "$TMP" <<'PY'
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
import sys

root, tmp = map(Path, sys.argv[1:])
pci = (root / 'fantgpu/fantgpu_pci_drv.c').read_text()
dma = (root / 'fantgpu/hal_dma.c').read_text()
passed = 0

def check(name, condition):
    global passed
    assert condition, name
    passed += 1
    print(f'stop-stage-t{passed:02d}_{name}=PASS', flush=True)

def body(name, text=pci):
    return re.search(r'^(?:static )?(?:int|bool|ssize_t) ' + name + r'\(.*?^}', text, re.M | re.S).group()

check('exact_scope', set(re.findall(r'^\+\+\+ b/(.+)$', Path('patches/030-034.patch').read_text(), re.M)) == {
    'fantgpu/fantgpu_pci_drv.c', 'fantgpu/hal_dma.c', 'fantgpu/hal_interface.h'})
check('shared_hal_unchanged', (root / 'fantgpu/hal.h').read_bytes() ==
      subprocess.check_output(['tar', '--use-compress-program=zstd', '-xOf',
          'docs/planning/evidence/o-stage/5.0.0-i5/o-stage-snapshot.tar.zst', 'o-stage/fantgpu/hal.h']))
write = body('fantgpu_pm_probe_write')
check('exact_commands_and_root', all(x in write for x in [
    'capable(CAP_SYS_ADMIN)', 'memchr(command,',
    '"arm pci-entry confirm=R5_I6_STOP_STAGE"',
    '"arm pre-power-sleep confirm=R5_I6_STOP_STAGE"', 'return -EINVAL;']))
check('arm_serialization', write.index('mutex_lock_interruptible') <
      write.index('probe->used = true') < write.index('probe->stop_stage = stop_stage') and
      all(x in write for x in ['probe->pm_transition', 'READ_ONCE(probe->removing)', 'probe->used']))
check('dma_variant_fail_closed', '!fh2m_hal_dma_idle_release_only()' in write and '-EOPNOTSUPP' in write)
check('rearm_only_successful_stop', all(x in write for x in [
    'stop_rearm = stop_stage != FANTGPU_PM_STOP_NONE && probe->used',
    'probe->stop_consumed && !probe->running', '!strcmp(probe->state, "stopped")',
    '(probe->used && !stop_rearm)', 'probe->stop_consumed = false;']))
suspend = body('fantgpu_device_suspend')
prefix = suspend[:suspend.index('/*cancle')]
check('pre_sleep_call_prefix', prefix.index('fh2m_hal_dma_suspend') <
      prefix.index('FANTGPU_PM_STOP_PRE_POWER_SLEEP') < prefix.index('return -ECANCELED') and
      not any(x in prefix for x in ['hal_power_sleep(', 'disable_irq(', 'atomic_set(', 'pci_save_state(']))
cb = body('fantgpu_pmops_suspend')
check('entry_before_device_suspend', cb.index('FANTGPU_PM_STOP_PCI_ENTRY') <
      cb.index('ret = -ECANCELED') < cb.index('fantgpu_device_suspend(dev, true, probe->stop_stage)'))
check('counter_before_child_recovery', cb.index('atomic_set(&probe->pdev_rsrc->pvr_resume_count, 0)') <
      cb.rindex('fantgpu_pm_transition_end(probe)'))
check('no_forbidden_checkpoints', set(re.findall(r'^\s*(FANTGPU_PM_STOP_\w+),', pci, re.M)) == {
    'FANTGPU_PM_STOP_NONE', 'FANTGPU_PM_STOP_PCI_ENTRY', 'FANTGPU_PM_STOP_PRE_POWER_SLEEP'})
release = body('hal_dma_chan_release_callback', dma) if False else re.search(
    r'static void hal_dma_chan_release_callback\(.*?^}', dma, re.M | re.S).group()
request = re.search(r'static struct hal_dma_phys_chan\* hal_dma_request_channel\(.*?^}', dma, re.M | re.S).group()
check('dma_idle_and_reacquire_proof', all(x in release for x in [
    'pool->dma_total_load > 0', 'task_cnt == 0', 'fh2m_fant_mutex_lock', 'chan = NULL']) and
    request.index('fh2m_fant_mutex_lock(min_load_chan->chan_lock)') <
    request.index('if (min_load_chan->chan == NULL)') < request.index('fh2m_fant_dma_request_channel'))

# Execute the actual PCI callback against stubs, not a rewritten state-machine model.
enum = re.search(r'enum fantgpu_pm_stop_stage \{.*?\};', pci, re.S).group()
state = re.search(r'struct fantgpu_pm_probe_state \{.*?\};', pci, re.S).group()
harness = '''
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
struct device { int dummy; };
struct mutex { int dummy; };
struct dentry { int dummy; };
struct dev_rsrc { int pvr_resume_count; struct device *dev; };
#define FANTGPU_PM_PROBE_TEXT_LEN 24
''' + enum + '\n' + state + '''
static struct fantgpu_pm_probe_state p;
static struct dev_rsrc r;
static int calls, unlocked, device_rc = -ECANCELED;
static int fantgpu_pm_transition_begin(struct device *d, struct fantgpu_pm_probe_state **out) { *out=&p; return 0; }
static void fantgpu_pm_transition_end(struct fantgpu_pm_probe_state *p) { unlocked++; }
static int fantgpu_pm_callback_result(struct device *d, const char *m, int rc) { return rc; }
#define pcie_info(...) ((void)0)
#define fantgpu_pm_marker(...) ((void)0)
#define fantgpu_pm_probe_marker(...) ((void)0)
#define atomic_set(ptr, value) (*(ptr)=(value))
static void fantgpu_pm_probe_set_text(char *d, const char *s) { strcpy(d,s); }
static const char *fantgpu_pm_stop_name(enum fantgpu_pm_stop_stage s) { return s==FANTGPU_PM_STOP_PCI_ENTRY?"pci-entry":"pre-power-sleep"; }
static int fantgpu_pm_propagate(int rc) { return rc<0?rc:rc?-EIO:0; }
static int fantgpu_device_suspend(struct device *d, bool s, enum fantgpu_pm_stop_stage stage) { calls++; return stage==FANTGPU_PM_STOP_NONE?0:device_rc; }
struct file { struct { struct dentry *dentry; } f_path; };
#define __user
#define CAP_SYS_ADMIN 1
#define SYSTEM_RUNNING 1
#define FANTGPU_PM_PROBE_CONFIRM "confirm=R5_I4_DIAGNOSTIC"
#define READ_ONCE(x) (x)
static int root_user=1, system_state=SYSTEM_RUNNING, idle_only=1, hal_calls;
static bool capable(int cap) { return root_user; }
static int copy_from_user(void *dst, const void *src, size_t n) { memcpy(dst,src,n); return 0; }
static int fantgpu_pm_probe_file_get(struct file *f, struct fantgpu_pm_probe_state **out) { *out=&p; return 0; }
static int mutex_lock_interruptible(struct mutex *m) { return 0; }
static void mutex_unlock(struct mutex *m) {}
static void debugfs_file_put(struct dentry *d) {}
static struct dev_rsrc *fh2m_fant_rsrc_devres_find(struct device *d) { return &r; }
static bool fh2m_hal_dma_idle_release_only(void) { return idle_only; }
static void hal_check_reg_accessiable(struct dev_rsrc *r) { hal_calls++; }
static int hal_power_sleep(struct dev_rsrc *r) { hal_calls++; return 0; }
static int hal_power_wakeup(struct dev_rsrc *r) { hal_calls++; return 0; }
''' + cb + '\n' + write + '''
static ssize_t command(const char *s) {
    struct file f={0}; loff_t pos=0;
    return fantgpu_pm_probe_write(&f,s,strlen(s),&pos);
}
int main(void) {
    struct device d;
    for (int stage=1; stage<=2; stage++) {
        memset(&p,0,sizeof(p)); p.pdev_rsrc=&r; p.stop_stage=stage;
        r.pvr_resume_count=1; calls=unlocked=0;
        assert(fantgpu_pmops_suspend(&d)==-ECANCELED);
        assert(calls==(stage==2) && unlocked==1 && r.pvr_resume_count==0);
        assert(p.stop_consumed && !p.running && !strcmp(p.state,"stopped"));
        assert(p.raw_rc==-ECANCELED && p.normalized_rc==-ECANCELED);
        assert(fantgpu_pmops_suspend(&d)==-EPERM);
        assert(calls==(stage==2) && unlocked==2);
    }
    for (int rc=-1; rc<=1; rc+=2) {
        memset(&p,0,sizeof(p)); p.pdev_rsrc=&r; p.stop_stage=2; device_rc=rc;
        assert(fantgpu_pmops_suspend(&d)==rc);
        assert(!strcmp(p.state,"failed") && p.normalized_rc==(rc<0?rc:-EIO));
    }
    memset(&p,0,sizeof(p)); p.pdev_rsrc=&r;
    assert(fantgpu_pmops_suspend(&d)==0);
    device_rc=-ECANCELED;
    memset(&p,0,sizeof(p)); p.pdev_rsrc=&r;
    root_user=0;
    assert(command("arm pci-entry confirm=R5_I6_STOP_STAGE")==-EPERM);
    root_user=1;
    assert(command("arm pci-entry confirm=R5_I6_STOP_STAGE junk")==-EINVAL);
    assert(command("arm pci-entry confirm=R5_I6_STOP_STAGE")>0);
    assert(command("arm pre-power-sleep confirm=R5_I6_STOP_STAGE")==-EBUSY);
    assert(fantgpu_pmops_suspend(&d)==-ECANCELED);
    assert(command("run reg-read confirm=R5_I4_DIAGNOSTIC")==-EBUSY);
    p.pm_transition=true;
    assert(command("arm pre-power-sleep confirm=R5_I6_STOP_STAGE")==-EBUSY);
    p.pm_transition=false;
    assert(command("arm pre-power-sleep confirm=R5_I6_STOP_STAGE")>0);
    assert(!p.stop_consumed && !strcmp(p.state,"armed"));
    assert(fantgpu_pmops_suspend(&d)==-ECANCELED);
    p.removing=true;
    assert(command("arm pci-entry confirm=R5_I6_STOP_STAGE")==-EBUSY);
    p.removing=false;
    strcpy(p.state,"failed");
    assert(command("arm pci-entry confirm=R5_I6_STOP_STAGE")==-EBUSY);
    memset(&p,0,sizeof(p)); p.pdev_rsrc=&r; idle_only=0;
    assert(command("arm pre-power-sleep confirm=R5_I6_STOP_STAGE")==-EOPNOTSUPP);
    assert(!strcmp(p.state,"failed") && !hal_calls);
}
'''
(tmp / 'callback.c').write_text(harness)
subprocess.run(['cc', '-std=gnu11', '-o', str(tmp / 'callback'), str(tmp / 'callback.c')], check=True)
subprocess.run([str(tmp / 'callback')], check=True)
check('compiled_callback_write_rearm_lifecycle_errno', True)
query = body('fh2m_hal_dma_idle_release_only', dma)
for active in (False, True):
    (tmp / 'query.c').write_text('#include <stdbool.h>\n#include <assert.h>\n' +
        ('#define ENABLE_DMA_INTERNAL_MANAGER_CHAN\n' if active else '') + query +
        f'\nint main(void) {{ assert(fh2m_hal_dma_idle_release_only()=={int(not active)}); }}\n')
    subprocess.run(['cc', '-o', str(tmp / 'query'), str(tmp / 'query.c')], check=True)
    subprocess.run([str(tmp / 'query')], check=True)
check('compiled_dma_variants', True)
spec = importlib.util.spec_from_file_location('o4', 'tools/o4-f0-lock-gen.py')
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
check('locked_after_tree', hashlib.sha256(o4.manifest_text(list(o4.walk_rows(root))).encode()).hexdigest() ==
      '5f6a5347c7e217ba3f7c5b71fdcad520148e0bc71231ab95fb023655865d11da')
check('no_patch_artifacts', not list(root.rglob('*.orig')) and not list(root.rglob('*.rej')))
for patch in ('030-034', '030-033', '030-032'):
    subprocess.run(['patch', '--batch', '--reverse', '--fuzz=0', '--no-backup-if-mismatch',
                    '-s', '-d', str(root), '-p1', '-i', str(Path('patches', patch + '.patch').resolve())],
                   check=True)
check('diagnostic_reverse_chain_returns_i3', hashlib.sha256(
      o4.manifest_text(list(o4.walk_rows(root))).encode()).hexdigest() ==
      'acfe80d1cff9437f8d4a77ee71640d1a4c0698614656f8312cdf662d8366361c')
check('diagnostic_reverse_removes_all_source_tokens', subprocess.run([
    'rg', '-a', '-q', 'fantgpu_pm_probe|fantgpu_pm_stop|FANTGPU_PM_STOP|R5_I4_DIAGNOSTIC|'
    'R5_I6_STOP_STAGE|fh2m_hal_dma_idle_release_only', str(root)]).returncode == 1 and
    not list(root.rglob('*.orig')) and not list(root.rglob('*.rej')))
print(f'PASS={passed} FAIL=0')
PY
