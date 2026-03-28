"""
Script: 04_build_samples.py
Description:
    Build orchestrator for all DelegateMQ sample projects.
    Assumes '03_generate_samples.py' has already been run to configure
    (generate) the build directories. This script compiles every project
    and reports pass/fail.

    See Examples Setup within DETAILS.md before running script.

Key Features:
    1. Platform Awareness: Skips Windows-only projects on Linux and vice versa.
    2. Client/Server Support: Builds client and server sub-projects independently.
    3. Build Config: Builds Release configuration on both Windows (MSVC
       multi-config) and Linux (single-config generators).
    4. Summary Report: Prints a final pass/fail/skipped table.

Skip List (always excluded):
    - bare-metal-arm          Embedded ARM target, requires cross-compiler
    - stm32-freertos          Embedded STM32 target, requires cross-compiler
    - mqtt-rapidjson          Requires external MQTT broker (excluded from run)
    - serialport-serializer   Requires physical serial port hardware
    - system-architecture-python  Python-only client, no C++ build

Usage:
    Run this script FOURTH, after generating project files with 03_generate_samples.py.
    > python 04_build_samples.py
"""

import os
import sys
import platform
import subprocess

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

SKIP_ALWAYS = {
    "bare-metal-arm",
    "stm32-freertos",
    "mqtt-rapidjson",
    "serialport-serializer",
    "system-architecture-python",
}

WINDOWS_ONLY = {
    "freertos-bare-metal",
    "win32-pipe-serializer",
    "win32-tcp-serializer",
    "win32-udp-serializer",
}

LINUX_ONLY = {
    "linux-tcp-serializer",
    "linux-udp-serializer",
}

# Projects that contain dotnet sub-projects requiring 'dotnet build' in addition
# to any cmake targets.  Keys are project names; values are subdirectory names.
DOTNET_BUILDS = {
    "databus-interop": ["csharp-client"],
}

IS_WINDOWS = platform.system() == "Windows"
BUILD_CONFIG = "Release"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def build_project(build_dir, label):
    """
    Run cmake --build on an already-configured build directory.
    Returns (success, error_output).
    """
    if not os.path.isdir(build_dir):
        return False, f"Build directory not found: {build_dir}"

    cmd = ["cmake", "--build", build_dir, "--config", BUILD_CONFIG]
    try:
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if result.returncode != 0:
            return False, result.stdout
        return True, ""
    except Exception as exc:
        return False, str(exc)


def build_dotnet(project_subdir, label):
    """
    Run 'dotnet build' in a directory containing a .csproj file.
    Returns (success, error_output).
    """
    cmd = ["dotnet", "build", "--configuration", BUILD_CONFIG]
    try:
        result = subprocess.run(
            cmd,
            cwd=project_subdir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if result.returncode != 0:
            return False, result.stdout
        return True, ""
    except Exception as exc:
        return False, str(exc)


def collect_build_dirs(project_dir):
    """
    Return a list of (label, build_dir) tuples for a project.
    Client/server projects yield two entries; standalone yields one.
    """
    client_build = os.path.join(project_dir, "client", "build")
    server_build = os.path.join(project_dir, "server", "build")

    if os.path.isdir(client_build) or os.path.isdir(server_build):
        targets = []
        if os.path.isdir(client_build):
            targets.append(("client", client_build))
        if os.path.isdir(server_build):
            targets.append(("server", server_build))
        return targets

    top_build = os.path.join(project_dir, "build")
    if os.path.isdir(top_build):
        return [("", top_build)]

    return []


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def build_samples():
    repo_root   = os.path.dirname(os.path.abspath(__file__))
    samples_dir = os.path.join(repo_root, "example", "sample-projects")

    if not os.path.isdir(samples_dir):
        print(f"Error: sample-projects directory not found: {samples_dir}")
        return False

    os_label = "Windows" if IS_WINDOWS else "Linux"

    print("=" * 60)
    print("  DelegateMQ Sample Builder")
    print("=" * 60)
    print(f"  Platform : {os_label}")
    print(f"  Config   : {BUILD_CONFIG}")
    print()
    print("  WARNING: This will build all sample projects.")
    print("  Compilation may take several minutes depending on")
    print("  the number of projects and machine speed.")
    print("=" * 60)
    answer = input("\nContinue? [y/N]: ").strip().lower()
    if answer != "y":
        print("Aborted.")
        return True
    print()

    passed  = []
    failed  = []
    skipped = []

    for project_name in sorted(os.listdir(samples_dir)):
        project_dir = os.path.join(samples_dir, project_name)
        if not os.path.isdir(project_dir):
            continue

        # --- Skip checks ---
        if project_name in SKIP_ALWAYS:
            skipped.append((project_name, "excluded"))
            continue
        if project_name in WINDOWS_ONLY and not IS_WINDOWS:
            skipped.append((project_name, "Windows only"))
            continue
        if project_name in LINUX_ONLY and IS_WINDOWS:
            skipped.append((project_name, "Linux only"))
            continue

        targets = collect_build_dirs(project_dir)
        if not targets:
            skipped.append((project_name, "not configured — run 03_generate_samples.py first"))
            continue

        project_failed = False
        for label, build_dir in targets:
            tag = f"{project_name}/{label}" if label else project_name
            print(f"[BUILDING] {tag}")
            success, err = build_project(build_dir, label)
            if success:
                print(f"   ok")
            else:
                print(f"   FAILED")
                # Print the last 15 lines of compiler output for context
                lines = err.strip().splitlines()
                for line in lines[-15:]:
                    print(f"   {line}")
                project_failed = True

        # Build any dotnet sub-projects (e.g. C# interop clients) — Windows only
        for subdir_name in (DOTNET_BUILDS.get(project_name, []) if IS_WINDOWS else []):
            subdir = os.path.join(project_dir, subdir_name)
            tag = f"{project_name}/{subdir_name}"
            print(f"[BUILDING] {tag}  (dotnet)")
            success, err = build_dotnet(subdir, subdir_name)
            if success:
                print(f"   ok")
            else:
                print(f"   FAILED")
                lines = err.strip().splitlines()
                for line in lines[-15:]:
                    print(f"   {line}")
                project_failed = True

        if project_failed:
            failed.append(project_name)
        else:
            passed.append(project_name)

    # --- Summary ---
    total = len(passed) + len(failed)
    print("\n" + "=" * 60)
    print(f"RESULTS  {len(passed)}/{total} passed   {len(failed)} failed   {len(skipped)} skipped")
    print("=" * 60)

    if failed:
        print("\nFAILED:")
        for name in failed:
            print(f"   {name}")

    if skipped:
        print("\nSKIPPED:")
        for name, reason in skipped:
            print(f"   {name:<40} ({reason})")

    print()
    return len(failed) == 0


if __name__ == "__main__":
    success = build_samples()
    sys.exit(0 if success else 1)
