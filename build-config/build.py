import os
import subprocess
import glob
import argparse
import shlex
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

class Colors:
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    GRAY = '\033[37m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BOLD = '\033[1m'
    ENDC = '\033[0m'

def run_command(command, description, cwd=None, quiet=False):
    cmd_str = shlex.join(command)
    # print(f"Executing: {cmd_str}")
    try:
        result = subprocess.run(
            command,
            check=True,
            shell=False,
            capture_output=True,
            text=True,
            cwd=cwd
        )
        if result.stdout and not quiet:
            print(f"{Colors.GRAY}STDOUT for '{description}':\n{result.stdout.strip()}{Colors.ENDC}")
        if result.stderr:
            print(f"{Colors.YELLOW}STDERR for '{description}':\n{result.stderr.strip()}{Colors.ENDC}")
    except subprocess.CalledProcessError as e:
        print(f"{Colors.RED}ERROR: Command failed with exit code {e.returncode}: {shlex.join(e.cmd)}{Colors.ENDC}")
        print(f"{Colors.RED}Output: {e.stdout.strip()}{Colors.ENDC}")
        print(f"{Colors.RED}Error Output: {e.stderr.strip()}{Colors.ENDC}")
        print(f"{Colors.RED}Build process aborted due to compilation error.{Colors.ENDC}")
        exit(1)
    except FileNotFoundError:
        print(f"{Colors.RED}ERROR: Command not found. Make sure '{command[0]}' is in your PATH.{Colors.ENDC}")
        exit(1)

def main():
    # --- Command-line Argument Parsing ---
    parser = argparse.ArgumentParser(description="proj-mmt debug build Script.")
    parser.add_argument(
        '-j', '--jobs',
        type=int,
        default=os.cpu_count() or 4,
        help='Number of parallel compilation jobs (threads). Defaults to CPU count.'
    )
    parser.add_argument(
        '--release',
        action='store_true',
        help='Enable release build'
    )
    args = parser.parse_args()
    MAX_WORKER_THREADS = args.jobs
    print(f"Using {MAX_WORKER_THREADS} threads for parallel compilation.")

    base_dir = ".."
    compiler = "g++"
    if args.release:
        print(f"{Colors.CYAN}{Colors.BOLD}Building in RELEASE mode.{Colors.ENDC}")
        compiler_flags = "-Wall -Wextra -pedantic -Os -march=nehalem".split()
        release_flags = "-s -static -static-libgcc -static-libstdc++".split()
    else:
        print(f"{Colors.CYAN}{Colors.BOLD}Building in DEBUG mode.{Colors.ENDC}")
        compiler_flags = "-Wall -Wextra -pedantic -g -march=native".split()
        release_flags = "".split()
    include_flags = [f"-I{os.path.join(base_dir, 'include')}"]
    linking_flags = "-lsodium -lws2_32 -lmf -lmfplat -lmfreadwrite -lmfuuid -lshlwapi -lole32 -loleaut32 -lrpcrt4 -lgdi32 -lgdiplus".split()
    imgui_flags = [f"-I{os.path.join(base_dir, 'imgui-win32-dx9', 'include')}", f"-L{os.path.join(base_dir, 'imgui-win32-dx9', 'lib')}"]
    imgui_linking_flags = "-limgui-win32-dx9 -ld3d9 -ldwmapi -luser32 -lwinmm".split()

    # --- Create Build Directories ---
    bin_dir = os.path.join(base_dir, "bin")
    engine_bin_dir = os.path.join(bin_dir, "engine")
    component_bin_dir = os.path.join(bin_dir, "component")
    os.makedirs(engine_bin_dir, exist_ok=True)
    os.makedirs(component_bin_dir, exist_ok=True)

    # Separate lists for different object file types to ensure correct linking
    engine_object_files = []
    component_object_files = []
    client_object_file = None
    server_object_file = None

    start_time = time.monotonic()
    with ThreadPoolExecutor(max_workers=MAX_WORKER_THREADS) as executor:
        futures = {}

        # BUild Engine files
        engine_src_path = os.path.join(base_dir, "src", "engine", "*.cpp")
        for src_file in glob.glob(engine_src_path):
            object_name = os.path.splitext(os.path.basename(src_file))[0] + ".o"
            output_path = os.path.join(engine_bin_dir, object_name)
            command = [compiler, *compiler_flags, *include_flags, "-c", src_file, "-o", output_path]
            print(f"Submitting engine file: {os.path.basename(src_file)}")
            future = executor.submit(run_command, command, object_name, quiet=True)
            futures[future] = ("engine", output_path)

        # BUild Component files
        component_src_path = os.path.join(base_dir, "src", "component", "*.cpp")
        for src_file in glob.glob(component_src_path):
            object_name = os.path.splitext(os.path.basename(src_file))[0] + ".o"
            output_path = os.path.join(component_bin_dir, object_name)
            command = [compiler, *compiler_flags, *include_flags, *imgui_flags, "-c", src_file, "-o", output_path]
            print(f"Submitting component file: {os.path.basename(src_file)}")
            future = executor.submit(run_command, command, object_name, quiet=True)
            futures[future] = ("component", output_path)

        # BUild Client
        client_src_path = os.path.join(base_dir, "src", "client.cpp")
        client_obj_path = os.path.join(bin_dir, "client.o")
        command = [compiler, *compiler_flags, *include_flags, *imgui_flags, "-c", client_src_path, "-o", client_obj_path]
        print("Submitting client")
        future = executor.submit(run_command, command, "client.o", quiet=True)
        futures[future] = ("client", client_obj_path)

        # BUild Server
        server_src_path = os.path.join(base_dir, "src", "server.cpp")
        server_obj_path = os.path.join(bin_dir, "server.o")
        command = [compiler, *compiler_flags, *include_flags, *imgui_flags, "-c", server_src_path, "-o", server_obj_path]
        print("Submitting server")
        future = executor.submit(run_command, command, "server.o", quiet=True)
        futures[future] = ("server", server_obj_path)

        # Wait for all compilation tasks to complete and collect results
        total_tasks = len(futures)
        for i, future in enumerate(as_completed(futures)):
            obj_type, obj_path = futures[future]
            try:
                future.result()  # Re-raises exceptions from run_command
                print(f"{Colors.CYAN}Compiled {os.path.basename(obj_path)} ({i+1}/{total_tasks} tasks completed) {Colors.ENDC}")
                if obj_type == "engine":
                    engine_object_files.append(obj_path)
                elif obj_type == "component":
                    component_object_files.append(obj_path)
                elif obj_type == "client":
                    client_object_file = obj_path
                elif obj_type == "server":
                    server_object_file = obj_path
            except Exception as exc:
                print(f'{Colors.RED}A compilation task for {os.path.basename(obj_path)} generated an exception: {exc}')
                executor.shutdown(wait=False, cancel_futures=True)
                exit(1)

    print("All source files compiled.")

    # --- Linking Client ---
    print("Linking client...")
    if client_object_file:
        client_exe_path = os.path.join(bin_dir, "client.exe")
        client_component_objects = [obj for obj in component_object_files if os.path.basename(obj) != "GmailLib.o"]
        client_link_objects = engine_object_files + client_component_objects + [client_object_file]
        command = [
            compiler, "-mwindows", "-municode", *compiler_flags, *release_flags,
            *client_link_objects,
            "-o", client_exe_path,
            *imgui_flags, *imgui_linking_flags, *linking_flags,
        ]
        run_command(command, f"{Colors.CYAN}Linking client{Colors.ENDC}")
    else:
        print("Skipping client linking due to compilation failure.")

    # --- Linking Server ---
    print("Linking server...")
    if server_object_file:
        server_exe_path = os.path.join(bin_dir, "server.exe")
        server_link_objects = engine_object_files + component_object_files + [server_object_file]
        command = [
            compiler, "-mwindows", "-municode", *compiler_flags, *release_flags,
            *server_link_objects,
            "-o", server_exe_path,
            *imgui_flags, *imgui_linking_flags, *linking_flags,
            "-Wl,-Bdynamic", "-lcurl"            # server also link with curl
        ]
        run_command(command, f"{Colors.CYAN}Linking server{Colors.ENDC}")
    else:
        print("Skipping server linking due to compilation failure.")

    duration = time.monotonic() - start_time
    print(f"{Colors.BOLD}{Colors.GREEN}Build process completed successfully in {duration:.3f} seconds{Colors.ENDC}")

if __name__ == "__main__":
    main()