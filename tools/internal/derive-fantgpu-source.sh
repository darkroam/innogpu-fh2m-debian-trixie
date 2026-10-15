# Sourced by the Debian builder and the Gentoo module build. Not a command.
# Caller must set ROOT before calling derive_fantgpu_source.
# Same transformation for compile staging and packaged DKMS source. The parent
# trace stays historical; the final derivative has its own exact whole-tree gate.
# Body hashes are slice-dependent. Do not mix them.
# 2cbebaa71abca1bdb924db8831573a48ef357d6acd2904227447d507a3684cf4
#   = `git show e562053:tools/build-innogpu-driver.sh`, from the line
#   "# Same transformation for compile staging" through the newline before
#   "APPLIED_SOURCE_FIXES=".
# a83b02988364081cfb4eb65abfb791e4fbf2e5bceb5968181a53942631fe8fe4
#   = this file from the derive_fantgpu_source() line through its closing
#   brace, heredoc-aware, including both lines. Header comments are outside.
derive_fantgpu_source() {
    python3 - "$ROOT" "$1" <<'PY'
import hashlib, importlib.util, pathlib, sys
repo, tree = map(pathlib.Path, sys.argv[1:])
spec = importlib.util.spec_from_file_location("o4", repo / "tools/internal/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
def digest():
    return hashlib.sha256(o4.manifest_text(list(o4.walk_rows(str(tree)))).encode()).hexdigest()
if digest() != "5f6a5347c7e217ba3f7c5b71fdcad520148e0bc71231ab95fb023655865d11da":
    sys.exit("builder_derived_source=FAIL parent tree drift")
path = tree / "cfg_detect.sh"
before = path.read_bytes()
if hashlib.sha256(before).hexdigest() != "fb44433928f749cc973de19673327aaae701e93298d5eba31012b674936bb18d":
    sys.exit("builder_derived_source=FAIL cfg_detect identity")
path.write_bytes(before + b'LC_ALL=C sort -o "$CFG_FILE_DIR/$CFG_FILE" "$CFG_FILE_DIR/$CFG_FILE"\n')
# Feature detection handles stable-tree backports without guessing versions.
path = tree / "test_item.sh"
path.write_text(path.read_text() + '\nTEST_ITEM:{\n    CODE="#include <drm/drm_fb_helper.h>\n    void fant_testconf_fb_helper_alloc_info(struct drm_fb_helper *helper) {\n        drm_fb_helper_alloc_info(helper);\n    }"\n    DEFINE="FANTGPU_DRM_FB_HELPER_ALLOC_INFO_PRESENT"\n    VAL=""\n    TYPE="success_define"\n    DEBUG="false"\n}\n')
path = tree / "fantsrvkm/fantdpu_drm_fb.c"
text = path.read_text()
old = "\treturn drm_fb_helper_alloc_info(helper);"
new = '#ifdef FANTGPU_DRM_FB_HELPER_ALLOC_INFO_PRESENT\n\treturn drm_fb_helper_alloc_info(helper);\n#else\n\t/* New DRM allocates this before fb_probe and frees it in helper_fini. */\n\treturn helper->info;\n#endif'
assert text.count(old) == 1
text = text.replace(old, new)
old = "\tif (!info) {\n\t\terr = -ENOMEM;"
assert text.count(old) == 1
text = text.replace(old, "\tif (IS_ERR_OR_NULL(info)) {\n\t\terr = info ? PTR_ERR(info) : -ENOMEM;")
text = text.replace("info->flags = FBINFO_DEFAULT | FBINFO_HWACCEL_DISABLED;",
                    "info->flags |= FBINFO_DEFAULT | FBINFO_HWACCEL_DISABLED;")
path.write_text(text)
if digest() != "e4d043bff8ed79b92b52512aad8f8e05ac51113bef2dd4cb4505dd96c510ccb7":
    sys.exit("builder_derived_source=FAIL i9 intermediate tree drift")
# Each label unwinds only operations that succeeded. Bluetooth never registers
# a PM notifier; no extra state or shared dev_rsrc layout change is needed.
path = tree / "fantpower/fant_input_event.c"
text = path.read_text()
changes = [
    ("\tif (!input_handle_priv)\n\t\treturn -ENOMEM;",
     "\tif (!input_handle_priv) {\n\t\terror = -ENOMEM;\n\t\tgoto err_free_handle;\n\t}"),
    ("\t\tregister_pm_notifier(&input_handle_priv->pm_nb);",
     "\t\terror = register_pm_notifier(&input_handle_priv->pm_nb);\n\t\tif (error)\n\t\t\tgoto err_free_private;"),
    ("\terror = input_register_handle(handle);\n\tif (error)\n\t\tgoto err_free_handle;",
     "\terror = input_register_handle(handle);\n\tif (error)\n\t\tgoto err_unregister_notifier;"),
    (" err_free_handle:\n\tkfree(handle);",
     " err_unregister_notifier:\n\tif (BUS_BLUETOOTH != dev->id.bustype)\n\t\tunregister_pm_notifier(&input_handle_priv->pm_nb);\n err_free_private:\n\tkfree(input_handle_priv);\n err_free_handle:\n\tkfree(handle);"),
]
for before, after in changes:
    assert text.count(before) == 1, "input cleanup source anchor drift"
    text = text.replace(before, after)
path.write_text(text)
if digest() != "c08b2bd426bcceba2853eb0767994875b3080cfaf4d5a1afcff821f3092aa67e":
    sys.exit("builder_derived_source=FAIL i10 intermediate tree drift")
# D/F share this defect. This does not fix API-internal waits or R5.
path = tree / "fantvpu/fantvpu_drv.c"
text = path.read_text()
for before, after in [
    ("\t\t\t\tif (ret == 0) {\n\t\t\t\t\tbreak;\n\t\t\t\t}\n\t\t\t\tudelay(50);",
     "\t\t\t\tif (ret == 0 || ret == 1) {\n\t\t\t\t\tbreak;\n\t\t\t\t}\n\t\t\t\tudelay(50);"),
    ("\t\tif (ret == 0) {\n\t\t\tbreak;\n\t\t}\n\t\tmsleep(10);",
     "\t\tif (ret == 0 || ret == 1) {\n\t\t\tbreak;\n\t\t}\n\t\tmsleep(10);"),
    ("\t\tt->drv_info = drv_info;\n\t\tt->last_timestamp",
     "\t\tt->drv_info = drv_info;\n\t\tt->timer_suspend = false;\n\t\tt->last_timestamp"),
]:
    assert text.count(before) == 1, "VPU timer source anchor drift"
    text = text.replace(before, after)
path.write_text(text)
derived = digest()
if derived != "f0e5490ad2d14fb280ef0e3bf76018462143b9223a357e0699b1c0ed376980b5":
    sys.exit("builder_derived_source=FAIL derived tree drift: " + derived)
print("builder_derived_source=PASS version=5.0.0-i11 tree=" + derived)
PY
    patch --batch --forward --fuzz=0 --no-backup-if-mismatch -s -d "$1" -p1 \
        < "$ROOT/patches/m-035-private-accessor.patch"
    python3 - "$ROOT" "$1" <<'PY'
import hashlib, importlib.util, pathlib, sys
repo, tree = map(pathlib.Path, sys.argv[1:])
spec = importlib.util.spec_from_file_location("o4", repo / "tools/internal/o4-f0-lock-gen.py")
o4 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(o4)
digest = hashlib.sha256(o4.manifest_text(list(o4.walk_rows(str(tree)))).encode()).hexdigest()
expected = "9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee"
if digest != expected:
    sys.exit("builder_derived_source=FAIL i12 final tree drift: " + digest)
print("builder_derived_source=PASS version=5.0.0-i12 tree=" + digest)
PY
}
