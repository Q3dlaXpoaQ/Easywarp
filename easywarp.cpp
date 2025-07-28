#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <random>
#include <set>

namespace fs = std::filesystem;

// --- Helper Functions ---

bool is_admin() {
    BOOL is_admin = FALSE;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID administrators_group;
    if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators_group)) {
        CheckTokenMembership(NULL, administrators_group, &is_admin);
        FreeSid(administrators_group);
    }
    return is_admin;
}

void run_as_admin(int argc, char* argv[]) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstringstream args;
    for (int i = 1; i < argc; ++i) {
        args << argv[i] << " ";
    }
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.lpParameters = args.str().c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
}

std::string run_command(const std::string& command, const std::string& input = "", const std::string& cwd = ".") {
    HANDLE g_hChildStd_IN_Rd = NULL;
    HANDLE g_hChildStd_IN_Wr = NULL;
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0);
    SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0);
    SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    CreateProcessA(NULL, (LPSTR)command.c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, cwd.c_str(), &siStartInfo, &piProcInfo);

    if (!input.empty()) {
        DWORD dwWritten;
        WriteFile(g_hChildStd_IN_Wr, input.c_str(), input.length(), &dwWritten, NULL);
    }
    CloseHandle(g_hChildStd_IN_Wr);
    CloseHandle(g_hChildStd_OUT_Wr);

    DWORD dwRead;
    CHAR chBuf[4096];
    std::string output;
    while (ReadFile(g_hChildStd_OUT_Rd, chBuf, 4096, &dwRead, NULL) && dwRead != 0) {
        output.append(chBuf, dwRead);
    }

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(g_hChildStd_OUT_Rd);
    CloseHandle(g_hChildStd_IN_Rd);

    return output;
}



bool is_process_running(const std::string& pid) {
    std::string command = "tasklist /FI \"PID eq " + pid + "\"";
    std::string output = run_command(command);
    return output.find(pid) != std::string::npos;
}

std::vector<std::string> generate_random_ipv4() {
    std::vector<std::string> ip_ranges = {
        "162.159.192.", "162.159.193.", "162.159.195.",
        "188.114.96.", "188.114.97.", "188.114.98.", "188.114.99."
    };
    std::set<std::string> unique_ips;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    while (unique_ips.size() < 100) {
        std::string base_ip = ip_ranges[distrib(gen) % ip_ranges.size()];
        std::string full_ip = base_ip + std::to_string(distrib(gen));
        unique_ips.insert(full_ip);
    }

    return std::vector<std::string>(unique_ips.begin(), unique_ips.end());
}

// --- Core Functions (to be implemented) ---


void run_ip_scan(const fs::path& script_dir, const fs::path& config_dir) {
    std::cout << "\n--- Preparing for IP address scan... ---" << std::endl;

    fs::path warp_exe_path = script_dir / "warp.exe";

    if (!fs::exists(warp_exe_path)) {
        std::cerr << "Error: 'warp.exe' not found in " << script_dir << ". Cannot perform IP scan." << std::endl;
        exit(1);
    }

    std::cout << "Generating random IPv4 addresses and running warp. This may take a while..." << std::endl;

    auto ips = generate_random_ipv4();
    fs::path ip_txt_path_in_script_dir = script_dir / "ip.txt";
    std::ofstream ip_file(ip_txt_path_in_script_dir);
    for (const auto& ip : ips) {
        ip_file << ip << std::endl;
    }
    ip_file.close();

    // Ensure result.csv exists before running warp.exe
    fs::path result_csv_path_in_script_dir = script_dir / "result.csv";
    if (!fs::exists(result_csv_path_in_script_dir)) {
        std::ofstream(result_csv_path_in_script_dir).close(); // Create an empty file
    }

    run_command("warp", "", script_dir.string());

    fs::path result_csv_path_in_config_dir = config_dir / "result.csv";

    if (fs::exists(result_csv_path_in_script_dir)) {
        if (fs::exists(result_csv_path_in_config_dir)) {
            fs::remove(result_csv_path_in_config_dir);
        }
        fs::rename(result_csv_path_in_script_dir, result_csv_path_in_config_dir);
    } else {
        std::cerr << "Warning: 'result.csv' was not generated by warp.exe in " << script_dir << std::endl;
    }

    fs::remove(ip_txt_path_in_script_dir);
}

std::vector<std::string> get_endpoints(const fs::path& csv_path) {
    std::vector<std::string> endpoints;
    if (!fs::exists(csv_path)) {
        return endpoints;
    }

    std::ifstream file(csv_path);
    std::string line;
    std::getline(file, line); // Skip header

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string ip, loss_str;
        std::getline(ss, ip, ',');
        std::getline(ss, loss_str, ',');

        try {
            float loss = std::stof(loss_str.substr(0, loss_str.find('%')));
            if (loss <= 25) {
                endpoints.push_back(ip);
            }
        } catch (const std::invalid_argument& e) {
            // Ignore lines with invalid loss values
        }
    }

    return endpoints;
}

std::string create_config_content(const std::string& endpoint) {
    return "[Interface]\n"
           "PrivateKey = cNNNs3WU2VdEOY4EYF9L+VK6/Unne0oVo39VK70k1mU=\n"
           "Address = 172.16.0.2/32, 2606:4700:110:812f:c724:2973:cac1:dbcb/128\n"
           "DNS = 1.1.1.1\n"
           "MTU = 1280\n\n"
           "[Peer]\n"
           "PublicKey = bmXOC+F1FxEMF9dyiK2H5/1SUtzH0JuVo51h2wPfgyo=\n"
           "AllowedIPs = 0.0.0.0/1, 128.0.0.0/1, ::/1, 8000::/1\n"
           "Endpoint = " + endpoint + "\n";
}

void remove_endpoint_from_csv(const fs::path& csv_path, const std::string& endpoint_to_remove) {
    std::vector<std::string> lines;
    std::ifstream file_in(csv_path);
    std::string line;
    while (std::getline(file_in, line)) {
        if (line.find(endpoint_to_remove) == std::string::npos) {
            lines.push_back(line);
        }
    }
    file_in.close();

    std::ofstream file_out(csv_path, std::ios::trunc);
    for (const auto& l : lines) {
        file_out << l << std::endl;
    }
}

std::string check_connection_status(const std::string& tunnel_name) {
    std::string command = "wireguard /dumplog " + tunnel_name;
    std::string output = run_command(command);

    std::stringstream ss(output);
    std::string line;
    std::vector<std::string> lines;

    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    // Iterate through lines in reverse order (from newest to oldest)
    for (int i = lines.size() - 1; i >= 0; --i) {
        const std::string& current_line = lines[i];

        if (current_line.find("Handshake for peer 1") != std::string::npos && current_line.find("completed") != std::string::npos) {
            return "connected";
        }
        if (current_line.find("Keypair 1 created for peer 1") != std::string::npos) {
            return "connected";
        }
        if (current_line.find("Handshake for peer 1") != std::string::npos && current_line.find("did not complete") != std::string::npos) {
            return "failed";
        }
    }

    return "failed";
}

void background_worker() {
    fs::path config_dir = fs::path(getenv("USERPROFILE")) / ".easywarp";
    fs::create_directories(config_dir);
    fs::path log_file = config_dir / "easywarp.log";
    fs::path pid_file = config_dir / "easywarp.pid";

    std::ofstream pid_out(pid_file);
    pid_out << GetCurrentProcessId();
    pid_out.close();

    // Redirect cout and cerr to log file
    std::ofstream log_stream(log_file, std::ios::trunc);
    std::streambuf* cout_buf = std::cout.rdbuf();
    std::cout.rdbuf(log_stream.rdbuf());
    std::streambuf* cerr_buf = std::cerr.rdbuf();
    std::cerr.rdbuf(log_stream.rdbuf());

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << "--- Background worker started (PID: " << GetCurrentProcessId() << ") at " << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << " ---" << std::endl;

    fs::path csv_path = config_dir / "result.csv";
    fs::path config_path = config_dir / "config.conf";
    std::string tunnel_name = config_path.stem().string();

    if (!fs::exists(csv_path)) {
        std::cout << "'result.csv' not found. Running IP scan..." << std::endl;
        run_ip_scan(fs::current_path(), config_dir);
    }

    while (true) {
        auto endpoints = get_endpoints(csv_path);
        if (endpoints.empty()) {
            std::cout << "\n--- No viable endpoints found. Triggering a new IP scan. ---" << std::endl;
            run_ip_scan(fs::current_path(), config_dir);
            std::cout << "✅ IP scan completed successfully." << std::endl;
            Sleep(17000);
            continue;
        }

        std::string endpoint = endpoints[0];
        auto now = std::chrono::system_clock::now();        
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);        
        std::cout << "\n--- (" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << ") Attempting endpoint: " << endpoint << " ---" << std::endl;

        run_command("wireguard /uninstalltunnelservice " + tunnel_name);
        Sleep(500);

        std::ofstream(config_path) << create_config_content(endpoint);

        run_command("wireguard /installtunnelservice " + config_path.string());
        Sleep(3800);

        if (check_connection_status(tunnel_name) == "connected") {
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::cout << "✅ (" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << ") Connection successful with: " << endpoint << std::endl;
            while (true) {
                Sleep(13000);
                if (check_connection_status(tunnel_name) == "failed") {
                    auto now = std::chrono::system_clock::now();
                    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
                    std::cout << "❌ (" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << ") Connection lost with: " << endpoint << "." << std::endl;
                    remove_endpoint_from_csv(csv_path, endpoint);
                    break;
                } else {
                    std::cout << "✅ " << endpoint << " is connecting correctly" << std::endl;
                }
            }
        } else {
            auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::cout << "❌ (" << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << ") Connection failed for " << endpoint << "." << std::endl;
            remove_endpoint_from_csv(csv_path, endpoint);
        }
    }
}

void start_easywarp() {
    fs::path config_dir = fs::path(getenv("USERPROFILE")) / ".easywarp";
    fs::create_directories(config_dir);
    fs::path pid_file = config_dir / "easywarp.pid";
    fs::path config_path = config_dir / "config.conf";
    std::string tunnel_name = config_path.stem().string();

    if (fs::exists(pid_file)) {
        std::ifstream pid_in(pid_file);
        std::string pid_str;
        pid_in >> pid_str;
        run_command("wireguard /uninstalltunnelservice " + tunnel_name);
        Sleep(500); // Give WireGuard some time to uninstall
        run_command("taskkill /F /PID " + pid_str);
        Sleep(1000); // Give OS time to release file handle
    }

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring command = L"\"" + std::wstring(path) + L"\" start-background";

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    CreateProcessW(NULL, &command[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void stop_easywarp() {
    fs::path config_dir = fs::path(getenv("USERPROFILE")) / ".easywarp";
    fs::path tunnel_name = (config_dir / "config.conf").stem();
    fs::path pid_file = config_dir / "easywarp.pid";
    fs::path log_file = config_dir / "easywarp.log";

    // Redirect cout and cerr to log file
    std::ofstream log_stream(log_file, std::ios::app); // Use app mode to append
    std::streambuf* cout_buf = std::cout.rdbuf();
    std::cout.rdbuf(log_stream.rdbuf());
    std::streambuf* cerr_buf = std::cerr.rdbuf();
    std::cerr.rdbuf(log_stream.rdbuf());

    run_command("wireguard /uninstalltunnelservice " + tunnel_name.string());
    Sleep(1500); // Give WireGuard more time to uninstall

    if (fs::exists(pid_file)) {
        std::ifstream pid_in(pid_file);
        std::string pid_str;
        pid_in >> pid_str;
        run_command("taskkill /F /PID " + pid_str);
        Sleep(1000); // Give OS time to release file handle
    }
    std::cout << "Easywarp quit successfully" << std::endl;

    // Close the log stream explicitly before restoring original buffers
    log_stream.close();

    // Restore original cout and cerr buffers
    std::cout.rdbuf(cout_buf);
    std::cerr.rdbuf(cerr_buf);
}

void show_log() {
    fs::path config_dir = fs::path(getenv("USERPROFILE")) / ".easywarp";
    fs::path log_file = config_dir / "easywarp.log";
    if (!fs::exists(log_file)) {
        std::cout << "(Log file does not exist)" << std::endl;
        return;
    }
    std::ifstream log_in(log_file);
    std::cout << log_in.rdbuf();
}


int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) {
        std::cerr << "Usage: easywarp [start|stop|log]" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    for (auto& c : command) {
        c = tolower(c);
    }

    if (command == "start" || command == "stop" || command == "start-background") {
        if (!is_admin()) {
            run_as_admin(argc, argv);
            return 0;
        }
    }

    if (command == "start") {
        start_easywarp();
    } else if (command == "start-background") {
        background_worker();
    } else if (command == "stop") {
        stop_easywarp();
    } else if (command == "log") {
        show_log();
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        std::cerr << "Usage: easywarp [start|stop|log]" << std::endl;
        return 1;
    }

    return 0;
}
