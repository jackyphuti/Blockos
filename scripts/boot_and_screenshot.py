#!/usr/bin/env python3
import os
import sys
import time
import json
import socket
import subprocess
from PIL import Image

REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO_DIR, "build")
DISK_IMG = os.path.join(REPO_DIR, "disk.img")
DATA_IMG = os.path.join(REPO_DIR, "data.img")
FS_IMG = os.path.join(REPO_DIR, "fat32_test.img")
EFI_FILE = os.path.join(BUILD_DIR, "BOOTX64.EFI")
VARS_COPY = os.path.join(BUILD_DIR, "OVMF_VARS.fd")
SERIAL_LOG = os.path.join(BUILD_DIR, "serial.log")
QMP_SOCK = "/tmp/qemu_blockos_qmp.sock"
SCREENSHOT_PPM = "/tmp/blockos_screen.ppm"
SCREENSHOT_PNG = os.path.join(BUILD_DIR, "blockos_screenshot.png")

def find_ovmf():
    codes = [
        "/usr/share/OVMF/OVMF_CODE_4M.fd",
        "/usr/share/OVMF/OVMF_CODE.fd",
        "/usr/share/edk2/ovmf/OVMF_CODE.fd",
        "/usr/share/edk2/x64/OVMF_CODE.fd"
    ]
    varss = [
        "/usr/share/OVMF/OVMF_VARS_4M.fd",
        "/usr/share/OVMF/OVMF_VARS.fd",
        "/usr/share/edk2/ovmf/OVMF_VARS.fd",
        "/usr/share/edk2/x64/OVMF_VARS.fd"
    ]
    c_found = next((c for c in codes if os.path.exists(c)), None)
    v_found = next((v for v in varss if os.path.exists(v)), None)
    return c_found, v_found

OVMF_CODE, OVMF_VARS = find_ovmf()

def run_cmd(cmd, check=True):
    print(f"[RUN] {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    return subprocess.run(cmd, check=check, shell=isinstance(cmd, str), capture_output=True, text=True)

def prepare_disks():
    print("[1/5] Preparing disk images...")
    os.makedirs(BUILD_DIR, exist_ok=True)
    if os.path.exists(VARS_COPY):
        os.remove(VARS_COPY)
    subprocess.run(["cp", OVMF_VARS, VARS_COPY], check=True)

    if not os.path.exists(EFI_FILE):
        print(f"Error: {EFI_FILE} not found!")
        sys.exit(1)

    # Create 256MB disk.img with FAT32
    if os.path.exists(DISK_IMG):
        os.remove(DISK_IMG)
    with open(DISK_IMG, "wb") as f:
        f.truncate(256 * 1024 * 1024)

    run_cmd(["mkfs.fat", "-F", "32", "-n", "BLOCKOS", DISK_IMG])
    run_cmd(["mmd", "-i", DISK_IMG, "::/EFI"])
    run_cmd(["mmd", "-i", DISK_IMG, "::/EFI/BOOT"])
    run_cmd(["mcopy", "-i", DISK_IMG, EFI_FILE, "::/EFI/BOOT/BOOTX64.EFI"])

    # Create startup.nsh so UEFI shell immediately launches the kernel without waiting
    startup_nsh = os.path.join(BUILD_DIR, "startup.nsh")
    with open(startup_nsh, "w") as f:
        f.write("\\EFI\\BOOT\\BOOTX64.EFI\r\n")
    run_cmd(["mcopy", "-i", DISK_IMG, startup_nsh, "::/startup.nsh"])

    # Create dummy secondary images if needed
    if not os.path.exists(DATA_IMG):
        with open(DATA_IMG, "wb") as f:
            f.truncate(64 * 1024 * 1024)
    if not os.path.exists(FS_IMG):
        with open(FS_IMG, "wb") as f:
            f.truncate(64 * 1024 * 1024)
        run_cmd(["mkfs.fat", "-F", "32", FS_IMG])

def boot_qemu():
    print("[2/5] Launching QEMU...")
    if os.path.exists(QMP_SOCK):
        os.remove(QMP_SOCK)
    if os.path.exists(SERIAL_LOG):
        os.remove(SERIAL_LOG)
    if os.path.exists(SCREENSHOT_PPM):
        os.remove(SCREENSHOT_PPM)
    for p in ["/tmp/blockos_gui.ppm", "/tmp/blockos_gui_bt.ppm"]:
        if os.path.exists(p):
            os.remove(p)

    qemu_cmd = [
        "qemu-system-x86_64",
        "-machine", "q35",
        "-m", "2048",
        "-cpu", "max",
        "-drive", f"if=pflash,format=raw,unit=0,readonly=on,file={OVMF_CODE}",
        "-drive", f"if=pflash,format=raw,unit=1,file={VARS_COPY}",
        "-drive", f"file={DISK_IMG},format=raw,if=ide,index=0",
        "-drive", f"file={DATA_IMG},format=raw,if=ide,index=1",
        "-drive", f"file={FS_IMG},format=raw,if=ide,index=2",
        "-device", "virtio-keyboard-pci",
        "-device", "virtio-rng-pci",
        "-serial", f"file:{SERIAL_LOG}",
        "-display", "none",
        "-qmp", f"unix:{QMP_SOCK},server,nowait",
    ]

    proc = subprocess.Popen(qemu_cmd)
    return proc

def capture_screenshot(proc):
    print("[3/5] Waiting for QMP socket and OS boot...")
    for _ in range(20):
        if os.path.exists(QMP_SOCK):
            break
        time.sleep(0.5)

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(QMP_SOCK)

    # Initial QMP handshake
    data = s.recv(1024).decode('utf-8')
    s.sendall(json.dumps({"execute": "qmp_capabilities"}).encode('utf-8') + b'\n')
    data = s.recv(1024).decode('utf-8')

    print("[4/5] Letting BlockOS initialize graphics and boot to shell (waiting 12s)...")
    time.sleep(12)

    # Capture screendump
    print(f"[5/5] Requesting screendump to {SCREENSHOT_PPM}...")
    screendump_cmd = {
        "execute": "screendump",
        "arguments": {"filename": SCREENSHOT_PPM}
    }
    s.sendall(json.dumps(screendump_cmd).encode('utf-8') + b'\n')
    time.sleep(1)
    resp = s.recv(2048).decode('utf-8')
    print("QMP screendump response:", resp)

    # Save splash
    SPLASH_PNG = os.path.join(BUILD_DIR, "blockos_boot_splash.png")
    if os.path.exists(SCREENSHOT_PPM):
        img = Image.open(SCREENSHOT_PPM)
        img.save(SPLASH_PNG, "PNG")
        print(f"Boot splash saved: {SPLASH_PNG}")

    # Press Enter to dismiss boot splash and enter GUI desktop
    print("[6/6] Sending keypress (Enter) to dismiss boot splash and load 90s GUI desktop...")
    sendkey_cmd = {
        "execute": "send-key",
        "arguments": {"keys": [{"type": "qcode", "data": "ret"}]}
    }
    s.sendall(json.dumps(sendkey_cmd).encode('utf-8') + b'\n')
    time.sleep(3)
    s.recv(1024)

    # Capture GUI desktop screenshot
    GUI_PPM = "/tmp/blockos_gui.ppm"
    GUI_PNG = os.path.join(BUILD_DIR, "blockos_gui.png")
    s.sendall(json.dumps({"execute": "screendump", "arguments": {"filename": GUI_PPM}}).encode('utf-8') + b'\n')
    time.sleep(1)
    s.recv(1024)

    # Now type 'bt' and press Enter in the terminal to trigger the Bluetooth Manager window
    print("[7/7] Typing 'bt' + Enter to open Bluetooth Manager window...")
    for k in ["b", "t", "ret"]:
        s.sendall(json.dumps({"execute": "send-key", "arguments": {"keys": [{"type": "qcode", "data": k}]}}).encode('utf-8') + b'\n')
        time.sleep(0.5)
        s.recv(1024)

    time.sleep(4.0)
    BT_PPM = "/tmp/blockos_gui_bt.ppm"
    BT_PNG = os.path.join(BUILD_DIR, "blockos_gui_bluetooth.png")
    s.sendall(json.dumps({"execute": "screendump", "arguments": {"filename": BT_PPM}}).encode('utf-8') + b'\n')
    time.sleep(1)
    s.recv(1024)

    # Terminate QEMU
    s.sendall(json.dumps({"execute": "quit"}).encode('utf-8') + b'\n')
    time.sleep(0.5)
    s.close()
    proc.wait(timeout=5)

    artifact_dir = "/mnt/c/Users/jacky/.gemini/antigravity/brain/e98d047a-c3a1-4a2c-a964-68aa79553677"

    if os.path.exists(GUI_PPM):
        img2 = Image.open(GUI_PPM)
        img2.save(GUI_PNG, "PNG")
        print(f"GUI desktop screenshot saved: {GUI_PNG}")
        if os.path.exists(artifact_dir):
            import shutil
            shutil.copy2(GUI_PNG, os.path.join(artifact_dir, "blockos_gui.png"))
            shutil.copy2(SPLASH_PNG, os.path.join(artifact_dir, "blockos_boot_splash.png"))

    if os.path.exists(BT_PPM):
        img3 = Image.open(BT_PPM)
        img3.save(BT_PNG, "PNG")
        print(f"Bluetooth Manager screenshot saved: {BT_PNG}")
        if os.path.exists(artifact_dir):
            import shutil
            shutil.copy2(BT_PNG, os.path.join(artifact_dir, "blockos_gui_bluetooth.png"))
            print(f"Screenshots copied to artifact dir: {artifact_dir}")

    return True

def main():
    prepare_disks()
    proc = boot_qemu()
    try:
        success = capture_screenshot(proc)
        if not success:
            sys.exit(1)
    finally:
        if proc.poll() is None:
            proc.kill()

if __name__ == "__main__":
    main()
