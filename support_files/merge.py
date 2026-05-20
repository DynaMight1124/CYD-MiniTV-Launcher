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

def merge_bins_callback(target, source, env):
    # Esptool path and Python executable
    esptool_pkg = env.PioPlatform().get_package_dir("tool-esptoolpy")
    python_exe = env.get("PYTHONEXE", "python")
    
    # Mapping environment names to the friendly filenames requested
    friendly_name = pioenv
    if "minitv" in pioenv:
        friendly_name = pioenv.replace("minitv-", "MiniTV-Launcher-")
    elif "handheld" in pioenv:
        friendly_name = pioenv.replace("handheld-", "CYD-Handheld-")
        
    out_file = proj_dir / f"{friendly_name}.bin"
    
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
        "--output", str(out_file),
        "0x1000", str(boot_bin),
        "0x8000", str(part_bin),
        "0x10000", str(app_bin),
    ]

    print(f"\nMerging binaries into: {out_file.name}")
    
    merge_env = os.environ.copy()
    merge_env["PYTHONPATH"] = str(esptool_pkg)
    
    result = subprocess.run(cmd, env=merge_env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Merge failed with exit code {result.returncode}")
        print(result.stderr.decode(errors="replace"))
    else:
        size = out_file.stat().st_size
        print(f"Success! Combined binary created: {out_file.name} ({size} bytes)")
        print("You can flash this single file to address 0x0 using ESP-Flasher.")

# This ensures it runs after firmware.bin is created
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bins_callback)
