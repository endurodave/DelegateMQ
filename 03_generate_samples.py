"""
Script: 03_generate_samples.py
Description:
    Configuration runner for all DelegateMQ sample projects.
    This script recursively finds every example project and generates the 
    Visual Studio solutions (or Makefiles) required to browse or compile them.
    
    IMPORTANT: It does NOT compile the code. It only prepares the build directory.

    See Examples Setup within DETAILS.md before running script.

Key Features:
    1. Smart Discovery: Recursively scans for 'CMakeLists.txt'.
    2. Exclusion Logic: Automatically skips shared library folders ('common', 'include', 'src')
       and specific platforms like 'bare-metal-arm'.
    3. Clean Generation: Removes old 'build' directories to ensure a fresh configuration.
    4. Project Generation: Runs 'cmake -B build' to create .sln files ready for IDE usage.
    5. Special Handling:
       - FreeRTOS projects are automatically configured for 'Win32' architecture.
       - Standard projects use the default generator (x64 on modern Windows).

Usage:
    Run this script THIRD to generate project files for your IDE.
    > python 03_generate_samples.py
"""

import os
import shutil
import subprocess

def build_samples():
    repo_root = os.path.dirname(os.path.abspath(__file__))
    target_dir = os.path.join(repo_root, "example", "sample-projects")

    if not os.path.exists(target_dir):
        print(f"Error: Could not find directory: {target_dir}")
        return

    print(f"Cleaning and Building ALL projects in: {target_dir}\n")

    for dirpath, dirnames, files in os.walk(target_dir):
        # 1. CLEANUP: Avoid recursing into build/install folders
        if "build" in dirnames: dirnames.remove("build")
        if "install" in dirnames: dirnames.remove("install")
        
        # SKIP: Explicitly skip bare-metal-arm directory to prevent processing
        if "bare-metal-arm" in dirnames: dirnames.remove("bare-metal-arm")

        if "CMakeLists.txt" in files:
            project_name = os.path.basename(dirpath)
            
            # --- SKIP COMMON/SHARED FOLDERS ---
            if project_name in ["common", "include", "src", "bare-metal-arm"]:
                # These are sub-libraries or skipped platforms, not standalone apps
                continue

            # 2. CLEAN: Delete existing build folder
            build_path = os.path.join(dirpath, "build")
            if os.path.exists(build_path):
                try: shutil.rmtree(build_path)
                except: pass 

            # 3. CONFIGURE
            if "freertos" in project_name.lower():
                print(f"[CONFIGURING] {project_name} (Win32)")
                cmd = ["cmake", "-B", "build", "-A", "Win32", "."]
            else:
                print(f"[CONFIGURING] {project_name}")
                cmd = ["cmake", "-B", "build", "."]

            try:
                subprocess.run(
                    cmd, cwd=dirpath, check=True,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
                )
                print("   Success!")
                
            except subprocess.CalledProcessError as e:
                print(f"   FAILED: {project_name}")

                # --- SPECIAL HANDLING FOR SERIALPORT ---
                if "serialport-serializer" in project_name:
                    print(f"   >> NOTICE: This project requires manual dependency setup.")
                    print(f"   >> Please read: {os.path.join(dirpath, 'README.md')}")
                    print(f"   >> You likely need to build 'libserialport' manually first.\n")

                if e.stderr:
                    lines = e.stderr.split('\n')
                    for i, line in enumerate(lines):
                        if "CMake Error" in line or "Could not find" in line or "FATAL_ERROR" in line:
                            print(f"   Reason: {line.strip()}")
                            # Print a bit of context
                            if i+1 < len(lines): print(f"          {lines[i+1].strip()}")
                            break

if __name__ == "__main__":
    build_samples()