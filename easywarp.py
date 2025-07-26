import csv
import os
import subprocess
import sys
import time
from pathlib import Path

# --- Helper Class for Flushing Log Output ---
class Logger:
    """Redirects print to a file and ensures every line is flushed."""
    def __init__(self, log_path):
        self.log_file = open(log_path, 'a', encoding='utf-8')

    def write(self, message):
        self.log_file.write(message)
        self.log_file.flush() # <-- The crucial fix

    def flush(self):
        # This flush method is needed for compatibility with sys.stdout.
        self.log_file.flush()

    def __del__(self):
        self.log_file.close()

# --- Core Functions ---

def is_admin():
    """Check if the script is r
    running with administrative privileges."""
    try:
        import ctypes
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

def run_as_admin():
    """Re-run the script with administrative privileges."""
    import ctypes
    ctypes.windll.shell32.ShellExecuteW(
        None, "runas", sys.executable, " ".join(sys.argv), None, 1
    )

def run_ip_scan(script_dir, config_dir):
    """
    Ensures necessary files are in the script directory and runs the IP scan script.
    """
    print("\n--- Preparing for IP address scan... ---")
    
    warp_exe_path = script_dir / 'warp.exe'
    ip_bat_path = script_dir / 'ip.bat'
    
    if not warp_exe_path.exists():
        print(f"Error: 'warp.exe' not found in {script_dir}. Cannot perform IP scan.", file=sys.stderr)
        sys.exit(1)

    if not ip_bat_path.exists():
        print(f"Error: Required file 'ip.bat' not found in {script_dir}.", file=sys.stderr)
        sys.exit(1)

    print("Starting IP scan via ip.bat. This may take a while...")
    
    try:
        # Using 'gbk' encoding to match the batch file's output on Chinese Windows
        # and 'errors=ignore' to prevent crashes on minor decoding issues.
        process = subprocess.run(
            [str(ip_bat_path)],
            input="1\n",
            text=True,
            capture_output=True,
            check=True,
            cwd=str(script_dir),
            timeout=600,
            encoding='gbk',
            errors='ignore'
        )
        print("✅ IP scan completed successfully.")

    except FileNotFoundError:
        print(f"Error: Could not find '{ip_bat_path}'.", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"Error during IP scan execution (return code {e.returncode}):", file=sys.stderr)
        print(f"--> STDOUT:\n{e.stdout}", file=sys.stderr)
        print(f"--> STDERR:\n{e.stderr}", file=sys.stderr)
        sys.exit(1)
    except subprocess.TimeoutExpired:
        print("IP scan timed out after 10 minutes.", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred during IP scan: {e}", file=sys.stderr)
        sys.exit(1)

def get_endpoints(csv_path):
    """
    Reads the result.csv file, which has a header, filters out endpoints 
    with loss > 10%, and returns a list of viable endpoints.
    """
    if not csv_path.exists():
        return []
    
    endpoints = []
    try:
        with open(csv_path, 'r', newline='', encoding='utf-8') as f:
            reader = csv.reader(f)
            try:
                header = [h.strip().lower() for h in next(reader)]
            except StopIteration:
                return []

            try:
                ip_index = header.index('ip')
                loss_index = header.index('loss')
            except ValueError:
                ip_index = 0
                loss_index = 1

            for row in reader:
                if not row or len(row) <= max(ip_index, loss_index):
                    continue

                try:
                    loss_str = row[loss_index].strip().replace('%', '')
                    loss_value = float(loss_str)
                    
                    if loss_value <= 10:
                        endpoint_str = row[ip_index].strip()
                        endpoints.append(endpoint_str)
                except (ValueError, IndexError):
                    continue
    
    except Exception as e:
        print(f"Error processing {csv_path.name}: {e}", file=sys.stderr)
        
    return endpoints

def create_config_content(endpoint):
    return f"""[Interface]
PrivateKey = cNNNs3WU2VdEOY4EYF9L+VK6/Unne0oVo39VK70k1mU=
Address = 172.16.0.2/32, 2606:4700:110:812f:c724:2973:cac1:dbcb/128
DNS = 1.1.1.1
MTU = 1280

[Peer]
PublicKey = bmXOC+F1FxEMF9dyiK2H5/1SUtzH0JuVo51h2wPfgyo=
AllowedIPs = 0.0.0.0/1, 128.0.0.0/1, ::/1, 8000::/1
Endpoint = {endpoint}
"""

def remove_endpoint_from_csv(csv_path, endpoint_to_remove):
    try:
        with open(csv_path, 'r', newline='', encoding='utf-8') as f:
            lines = list(csv.reader(f))
        
        updated_lines = [line for line in lines if not (line and line[0].strip() == endpoint_to_remove)]

        with open(csv_path, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerows(updated_lines)
        print(f"Removed failed endpoint {endpoint_to_remove} from {csv_path.name}")

    except Exception as e:
        print(f"Error updating {csv_path.name}: {e}", file=sys.stderr)

def check_connection_status(tunnel_name):
    try:
        log_result = subprocess.run(
            ["wireguard", "/dumplog", tunnel_name],
            check=True, capture_output=True, text=True, encoding='utf-8', timeout=10
        )
        log_text = log_result.stdout
        if not log_text.strip():
            return "failed"

        lines = log_text.strip().splitlines()
        
        for line in reversed(lines):
            if "Handshake for peer 1" in line and "completed" in line:
                return "connected"
            if "Keypair 1 created for peer 1" in line:
                return "connected"
            if "Handshake for peer 1" in line and "did not complete" in line:
                return "failed"

        return "failed"

    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return "failed"
    except FileNotFoundError:
        print("-> Error: 'wireguard' command not found.", file=sys.stderr)
        sys.exit(1)

# --- Background Service Functions ---

def background_worker():
    """The main background process for connecting, monitoring, and reconnecting."""
    config_dir = Path.home() / '.easywarp'
    log_file = config_dir / 'easywarp.log'
    pid_file = config_dir / 'easywarp.pid'

    with open(pid_file, 'w') as f:
        f.write(str(os.getpid()))

    # Redirect all output to the Logger instance
    sys.stdout = Logger(log_file)
    sys.stderr = sys.stdout
    
    print(f"--- Background worker started (PID: {os.getpid()}) at {time.strftime('%Y-%m-%d %H:%M:%S')} ---")

    csv_path = config_dir / 'result.csv'
    config_path = config_dir / 'config.conf'
    tunnel_name = config_path.stem

    if not csv_path.exists():
        print("'result.csv' not found. Running IP scan...")
        run_ip_scan(config_dir, config_dir)

    while True:
        endpoints = get_endpoints(csv_path)
        if not endpoints:
            print("\n--- No viable endpoints found. Triggering a new IP scan. ---")
            run_ip_scan(config_dir, config_dir)
            time.sleep(13)
            continue

        endpoint = endpoints[0]
        print(f"\n--- ({time.strftime('%H:%M:%S')}) Attempting endpoint: {endpoint} ---")

        subprocess.run(["wireguard", "/uninstalltunnelservice", tunnel_name], capture_output=True, check=False)
        time.sleep(0.5)

        config_path.write_text(create_config_content(endpoint), encoding='utf-8')
        
        try:
            subprocess.run(
                ["wireguard", "/installtunnelservice", str(config_path)],
                check=True, capture_output=True, text=True, encoding='utf-8', timeout=15
            )
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as e:
            print(f"Error starting WireGuard service for {endpoint}: {e.stderr if isinstance(e, subprocess.CalledProcessError) else 'Timeout'}", file=sys.stderr)
            remove_endpoint_from_csv(csv_path, endpoint)
            continue

        time.sleep(3)

        if check_connection_status(tunnel_name) == "connected":
            print(f"✅ ({time.strftime('%H:%M:%S')}) Connection successful with: {endpoint}")
            while True:
                time.sleep(13)
                if check_connection_status(tunnel_name) == "failed":
                    print(f"❌ ({time.strftime('%H:%M:%S')}) Connection lost with: {endpoint}.")
                    remove_endpoint_from_csv(csv_path, endpoint)
                    break
                else:
                    print(f"✅ {endpoint} is connecting correctly")
        else:
            print(f"❌ ({time.strftime('%H:%M:%S')}) Connection failed for {endpoint}.")
            remove_endpoint_from_csv(csv_path, endpoint)

def start_easywarp():
    """Launches the background worker process if not already running."""

    config_dir = Path.home() / '.easywarp'
    config_dir.mkdir(exist_ok=True)
    pid_file = config_dir / 'easywarp.pid'
    config_path = config_dir / 'config.conf'

    tunnel_name = config_path.stem


    log_file = config_dir / 'easywarp.log'

    if not log_file.exists():
        return
    try:
        file = open(log_file, "w")
        file.truncate()
        file.close()


    except Exception as e:
        print(f"Error reading log file: {e}", file=sys.stderr)

   
    if pid_file.exists():
        try:
            with open(pid_file, 'r') as f:
                pid = f.read().strip()
            subprocess.run(["wireguard", "/uninstalltunnelservice", tunnel_name], capture_output=True, check=False)
            result = subprocess.run(["tasklist", "/FI", f"PID eq {pid}"], capture_output=True, text=True)
            if str(pid) in result.stdout:
                sys.exit(0)
            else:
                pid_file.unlink()
        except Exception:
            pass

    command = [sys.executable, __file__, "start-background"]
    subprocess.Popen(command, creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP, close_fds=True)

def stop_easywarp():
    """Stops the WireGuard service and terminates the background worker."""
    config_dir = Path.home() / '.easywarp'
    tunnel_name = (config_dir / 'config.conf').stem
    pid_file = config_dir / 'easywarp.pid'
    log_file = config_dir / 'easywarp.log'

    sys.stdout = Logger(log_file)


    subprocess.run(["wireguard", "/uninstalltunnelservice", tunnel_name], capture_output=True, check=False)
    print("Easywarp quit successfully")



    if pid_file.exists():
        try:
            with open(pid_file, 'r') as f:
                pid = int(f.read().strip())
            subprocess.run(["taskkill", "/F", "/PID", str(pid)], capture_output=True, check=False)
        except (ValueError, FileNotFoundError):
            pass
        finally:
            if pid_file.exists():
                pid_file.unlink()

def show_log():
    """Displays the EasyWarp activity log."""
    config_dir = Path.home() / '.easywarp'
    log_file = config_dir / 'easywarp.log'
    if not log_file.exists():
        print("(Log file does not exist)")
        return
    try:
        with open(log_file, 'r', encoding='utf-8') as f:
            content = f.read()
            if not content.strip():
                print("(Log is empty)")
            else:
                print(content.strip())
    except Exception as e:
        print(f"Error reading log file: {e}", file=sys.stderr)

def main():
    """Main entry point for the easywarp CLI."""
    if len(sys.argv) < 2:
        print("Usage: easywarp [start|stop|log]")
        sys.exit(1)

    command = sys.argv[1].lower()
    
    if command in ['start', 'stop', 'start-background']:
        if not is_admin():
            run_as_admin()
            sys.exit(0)

    if command == 'start':
        start_easywarp()
    elif command == 'start-background':
        background_worker()
    elif command == 'stop':
        stop_easywarp()
    elif command == 'log':
        show_log()
    else:
        print(f"Unknown command: {command}", file=sys.stderr)
        print("Usage: easywarp [start|stop|log]", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()