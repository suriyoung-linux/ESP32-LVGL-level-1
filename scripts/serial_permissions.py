"""Use existing dialout membership when the IDE session has stale groups."""

import os
import shlex
import shutil
import sys

Import("env")


def configure_serial_upload():
    if not sys.platform.startswith("linux"):
        return
    if env.subst("$UPLOAD_PROTOCOL") != "esptool":
        return

    import grp
    import pwd

    try:
        serial_group = grp.getgrnam("dialout")
    except KeyError:
        return

    active_groups = set(os.getgroups()) | {os.getgid(), os.getegid()}
    if serial_group.gr_gid in active_groups:
        return

    user = pwd.getpwuid(os.getuid())
    if user.pw_name not in serial_group.gr_mem and user.pw_gid != serial_group.gr_gid:
        return

    sg = shutil.which("sg")
    if not sg:
        raise RuntimeError("Restart the Ubuntu session to activate dialout access; sg is unavailable.")

    # Keep PlatformIO's uploader, flags, image addresses and port detection.
    # sg only activates membership already granted to this user; no sudo needed.
    upload_command = env["UPLOADCMD"]
    env.Replace(UPLOADCMD="%s dialout -c %s" % (shlex.quote(sg), shlex.quote(upload_command)))
    print("Serial access: activating existing dialout membership for Upload.")


configure_serial_upload()
