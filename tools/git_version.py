Import("env")
import subprocess


def get_git_hash():
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=env.subst("$PROJECT_DIR"),
            stderr=subprocess.DEVNULL,
        )
        return out.decode("utf-8").strip()
    except Exception:
        return None


git_hash = get_git_hash()
if git_hash:
    env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(git_hash))])
    print(f"[VERSION] Injected FIRMWARE_VERSION={git_hash}")
