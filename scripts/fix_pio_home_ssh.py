"""Apply or undo the local PlatformIO IDE 3.3.4 Home fix for Remote SSH."""

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--restore", action="store_true")
    options = parser.parse_args()
    extension = (
        Path.home()
        / ".vscode-server/extensions/platformio.platformio-ide-3.3.4-linux-x64"
        / "dist/extension.js"
    )
    backup = extension.with_name("extension.js.before-pio-home-ssh-fix")
    old = '${a.home.getFrontendUrl({start:e,theme:t,workspace:W.getEnterpriseSetting("defaultPIOHomeWorkspace")})}'
    new = '${await r().env.asExternalUri(r().Uri.parse(a.home.getFrontendUrl({start:e,theme:t,workspace:W.getEnterpriseSetting("defaultPIOHomeWorkspace")})))}'
    source = extension.read_text()
    if options.restore:
        if not backup.is_file():
            raise RuntimeError("No backup found")
        if source != backup.read_text().replace(old, new, 1):
            raise RuntimeError("Extension changed since patching; refusing to overwrite it")
        shutil.copy2(backup, extension)
        print("Restored original extension. Run Developer: Reload Window.")
        return
    if new in source:
        print("SSH Home URL fix is already installed.")
        return
    if source.count(old) != 1:
        raise RuntimeError("Unexpected extension source; no files changed")
    if backup.exists() and backup.read_text() != source:
        raise RuntimeError("An unrelated backup already exists; no files changed")
    if not backup.exists():
        shutil.copy2(extension, backup)
    patched = source.replace(old, new, 1)
    check_file = Path("/tmp/platformio-home-ssh-extension-check.js")
    check_file.write_text(patched)
    servers = Path.home() / ".vscode-server/cli/servers"
    nodes = list(servers.glob("Stable-*/server/node"))
    if not nodes:
        raise RuntimeError("VS Code Node runtime not found; no extension changes made")
    subprocess.run([str(nodes[0]), "--check", str(check_file)], check=True)
    extension.write_text(patched)
    check_file.unlink()
    print("Patched PIO Home to use VS Code's SSH tunnel inside its existing tab.")
    print("Backup:", backup)
    print("Run Developer: Reload Window, then PlatformIO: Home.")


if __name__ == "__main__":
    main()
