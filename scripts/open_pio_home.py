"""Open the running PlatformIO Home session through VS Code Remote - SSH."""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request


def find_home_url():
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    processes = sorted(
        (process for process in Path("/proc").iterdir() if process.name.isdigit()),
        key=lambda process: int(process.name),
        reverse=True,
    )
    for process in processes:
        try:
            if process.stat().st_uid != os.getuid():
                continue
            args = (process / "cmdline").read_bytes().decode().split("\0")
        except (OSError, UnicodeError):
            continue
        if "home" not in args or not any(
            arg == "platformio" or arg.endswith("/platformio") for arg in args
        ):
            continue
        try:
            port = args[args.index("--port") + 1]
            session = args[args.index("--session-id") + 1]
        except (ValueError, IndexError):
            continue
        if not port.isdigit() or not 1 <= int(port) <= 65535:
            continue
        if not re.fullmatch(r"[A-Za-z0-9_-]+", session):
            continue
        base = f"http://127.0.0.1:{port}/session/{session}/"
        try:
            with opener.open(base + "package.json", timeout=5) as response:
                package = json.load(response)
                if response.status == 200 and package.get("version"):
                    return base + "?theme=dark"
        except (OSError, urllib.error.URLError, ValueError):
            continue
    raise RuntimeError(
        "ไม่พบ Home เซสชันที่ทำงานอยู่\n"
        "กด Ctrl+Shift+P → PlatformIO Home รอให้เริ่มทำงาน แล้วรันงานนี้อีกครั้ง"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--url-only", action="store_true", help="แสดง URL ปัจจุบันโดยไม่เปิดเบราว์เซอร์"
    )
    options = parser.parse_args()
    if not sys.platform.startswith("linux"):
        print("รันสคริปต์นี้บน Ubuntu ผ่าน Terminal ของ VS Code ที่เชื่อม SSH", file=sys.stderr)
        return 1
    try:
        url = find_home_url()
    except (OSError, RuntimeError) as error:
        print(error, file=sys.stderr)
        return 1

    if options.url_only:
        print("URL ปัจจุบันของ PlatformIO Home:", flush=True)
        print(url, flush=True)
        return 0

    code = shutil.which("code")
    if not code or not os.environ.get("VSCODE_IPC_HOOK_CLI"):
        print(url, flush=True)
        print("คัดลอก URL ด้านบนไปเปิดใน Firefox / Brave บน Windows")
        print("หากต้องการเปิดอัตโนมัติ ให้รันจาก Terminal ของ VS Code ในหน้าต่าง SSH")
        return 1
    try:
        result = subprocess.run(
            [code, "--openExternal", url],
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        print(url, flush=True)
        print("ส่งคำสั่งเปิดเบราว์เซอร์ไม่ได้: คัดลอก URL ด้านบนไปเปิดบน Windows")
        return 1
    if result.returncode:
        print(url, flush=True)
        print("VS Code ส่งคำสั่งเปิด URL ไม่สำเร็จ: คัดลอก URL ด้านบนไปเปิดบน Windows")
        return 1

    print("ส่งคำสั่งเปิด Home ไปยังเบราว์เซอร์เริ่มต้นบน Windows แล้ว")
    print("ครั้งต่อไปใช้ Tasks: Run Task → PlatformIO: Open Home in Browser (SSH) ได้เลย")
    print("คงการเชื่อมต่อ SSH และการส่งต่อพอร์ต Home ไว้ระหว่างใช้งาน")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
