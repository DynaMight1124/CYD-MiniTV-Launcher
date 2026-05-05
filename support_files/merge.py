from pathlib import Path
import os
import subprocess
from SCons.Script import Import

Import("env")

# Determine paths
build_dir = Path(env.subst("$BUILD_DIR"))
proj_dir = Path(env.subst("$PROJECT_DIR"))
pioenv = env.subst("${PIOENV}")

boot_bin = build_dir / "bootloader.bin"
part_bin = build_dir / "partitions.bin"
app_bin  = build_dir / "firmware.bin"

# Output combined binary
out_bin = proj_dir / f"MiniTV-Launcher-{pioenv}.bin"

def merge_bins_callback(target, source, env):
    # Esptool path and Python executable
    esptool_pkg = env.PioPlatform().get_package_dir("tool-esptoolpy")
    python_exe = env.get("PYTHONEXE", "python")
    
    missing = [p for p in [boot_bin, part_bin, app_bin] if not p.exists()]
    if missing:
        print("Missing files, merge aborted:")
        for p in missing:
            print(f" - {p}")
        return

    cmd = [
        python_exe, "-m", "esptool",
        "--chip", "esp32",
        "merge_bin",
        "--output", str(out_bin),
        "0x1000", str(boot_bin),
        "0x8000", str(part_bin),
        "0x10000", str(app_bin),
    ]

    print(f"\nMerging binaries into: {out_bin.name}")
    
    merge_env = os.environ.copy()
    merge_env["PYTHONPATH"] = str(esptool_pkg)
    
    result = subprocess.run(cmd, env=merge_env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Merge failed with exit code {result.returncode}")
        print(result.stderr.decode(errors="replace"))
    else:
        size = out_bin.stat().st_size
        print(f"Success! Combined binary created: {out_bin} ({size} bytes)")
        print("You can flash this single file to address 0x0 using ESP-Flasher.")

# This ensures it runs after firmware.bin is created
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bins_callback)
