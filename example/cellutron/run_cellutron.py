import subprocess
import os
import sys
import time

def main():
    # Define paths to executables relative to this script
    base_path = os.path.dirname(os.path.abspath(__file__))
    apps = [
        {"name": "Safety",     "path": "build/safety/Debug/cellutron_safety.exe"},
        {"name": "Controller", "path": "build/controller/Debug/cellutron_controller.exe"},
        {"name": "GUI",        "path": "build/gui/Debug/cellutron_gui.exe"},
    ]

    processes = []

    print("--- Starting Cellutron Distributed System ---")

    for app in apps:
        exe_path = os.path.join(base_path, app["path"])
        
        if not os.path.exists(exe_path):
            print(f"ERROR: Could not find {app['name']} at {exe_path}")
            print("Did you build the project first?")
            sys.exit(1)

        print(f"Launching {app['name']}...")
        
        # Start each process in a new console window
        # creationflags=subprocess.CREATE_NEW_CONSOLE is Windows-specific
        p = subprocess.Popen(
            exe_path, 
            creationflags=subprocess.CREATE_NEW_CONSOLE,
            cwd=os.path.dirname(exe_path)
        )
        processes.append(p)
        
        # Small delay to ensure sockets are ready before next app starts
        time.sleep(0.5)

    print("\nAll applications started.")
    print("Close the console windows or terminate this script to stop.")

    try:
        # Keep the script alive while processes are running
        while any(p.poll() is None for p in processes):
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down...")
        for p in processes:
            p.terminate()

if __name__ == "__main__":
    main()
