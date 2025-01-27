import os
import subprocess

# Define the subdirectories where CMake should be run
subdirs = [
    ".",
    "example/sample-projects/bare-metal",
    "example/sample-projects/system-architecture/client",
    "example/sample-projects/system-architecture/server",
    "example/sample-projects/zeromq-msgpack-cpp",
    "example/sample-projects/zeromq-serializer"
]

# Define the CMake generator and architecture options
cmake_options = [
    "-G", "Visual Studio 17 2022",   # Specify Visual Studio version
    "-A", "x64",                     # Set the architecture to 64-bit
    "-B", "build",                   # Set the build directory
    "-S", "."                        # Set the source directory (current directory)
]

# Iterate through each subdirectory and run cmake
for subdir in subdirs:
    print(f"Running CMake in: {subdir}")
    
    # Check if the directory exists
    if os.path.isdir(subdir):
        # Run cmake in the subdirectory, passing the cmake options
        result = subprocess.run(
            ["cmake"] + cmake_options + [subdir], 
            cwd=subdir, capture_output=True, text=True
        )
        
        # Check if there was any error during the cmake run
        if result.returncode == 0:
            print(f"CMake run successful in {subdir}")
        else:
            print(f"CMake run failed in {subdir}. Error:\n{result.stderr}")
    else:
        print(f"Directory not found: {subdir}")
