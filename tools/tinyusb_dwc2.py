from pathlib import Path
import subprocess

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
dependency = project / "vendor" / "tinyusb"
expected_commit = "1eb6ce784ca9b8acbbe43dba9f1d9c26c2e80eb0"
actual_commit = subprocess.check_output(
    ["git", "-C", str(dependency), "rev-parse", "HEAD"], text=True
).strip()
assert actual_commit == expected_commit, "TinyUSB 依赖版本不匹配"
assert not subprocess.check_output(
    ["git", "-C", str(dependency), "status", "--porcelain"], text=True
).strip(), "TinyUSB 官方依赖存在本地修改"

# 本地驱动移植官方 ISO 恢复处理，其余头文件使用固定版本依赖。
driver = project / "vendor" / "tinyusb-dwc2"
env.Prepend(CPPPATH=[str(driver), str(dependency / "src" / "portable" / "synopsys" / "dwc2")])
env.BuildSources(
    "$BUILD_DIR/tinyusb-dwc2",
    str(driver),
    src_filter=["+<dcd_dwc2.c>"],
)
