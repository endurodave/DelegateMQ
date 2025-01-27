import os
import subprocess

def build_project(project_file):
    """
    Function to build a Visual Studio project using MSBuild and show output on screen.
    """
    # Print status before starting the build
    print(f"\nStarting build for: {project_file}")

    # Define the path to MSBuild (update it to your installation path)
    msbuild_path = r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"  # Adjust this path

    try:
        # Run the MSBuild command to build the project with Debug configuration
        process = subprocess.Popen(
            [msbuild_path, project_file, '/p:Configuration=Debug'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Stream the output of the MSBuild process to the screen
        for stdout_line in process.stdout:
            print(stdout_line, end='')  # Print standard output
        for stderr_line in process.stderr:
            print(stderr_line, end='')  # Print standard error

        # Wait for the process to finish and get the return code
        process.wait()

        # Check if the build succeeded
        if process.returncode == 0:
            print(f"Successfully built: {project_file}")
        else:
            print(f"Build failed for: {project_file} with return code {process.returncode}")

    except Exception as e:
        print(f"An error occurred while building {project_file}: {e}")

def find_projects(directory):
    """
    Recursively find all Visual Studio project files (.csproj, .vcxproj, .sln) in the directory.
    """
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith(('.sln')):
                project_file = os.path.join(root, file)
                print(f"Found project: {project_file}")  # Debug print
                yield project_file

def build_all_projects(start_directory):
    """
    Recursively build all Visual Studio projects starting from the given directory.
    """
    for project_file in find_projects(start_directory):
        build_project(project_file)

if __name__ == "__main__":
    # Specify the directory to start the recursive search
    start_directory = input("Enter the directory to start building projects: ")

    # Start building projects
    build_all_projects(start_directory)
