# Easywarp

An automatic Warp node switching tool (自动切换warp节点的工具) that allows you to connect to Warp with a single command (实现一行指令连接warp).

## Usage (使用方法)

1.  **Install the Wireguard Client (安装Wireguard客户端):** Make sure you have the Wireguard client installed on your system.

2.  **Verify Wireguard Installation (验证Wireguard安装):** Open your command prompt or terminal and type `wireguard`. If the Wireguard client interface appears, you're good to go. (在控制台运行```wireguard```，若弹出wireguard客户端即可)

3.  **Installation Script (安装脚本):** Run the `install.bat` script. This will create a `.easywarp` folder in your user directory (`%username%`). This folder will store the Warp nodes and the filtering tool `warp.exe`. (运行*install.bat*，将会在%username%文件夹下创建一个 *.easywarp* 文件夹存储节点和筛选工具warp.exe)

4.  **Start Easywarp (开始Easywarp):** Open your command prompt or terminal and navigate to the directory where you have `easywarp.py`. Then, run the following command:

    ```bash
    python easywarp.py start
    ```

    *   **First Run (第一次运行):** The first time you run this, the script might need to filter and select suitable Warp nodes. Please be patient, as this process can take some time. (第一次运行可能要筛选脚本，请耐心等待)

5.  **Check the Logs (查看运行情况):** To view the running status and any potential errors, use the following command:

    ```bash
    python easywarp.py log
    ```
    (可输入```python easywarp.py log```查看运行情况)

6.  **Stop Easywarp (停止Easywarp):** If you encounter errors or want to stop the script, use the following command:

    ```bash
    python easywarp.py stop
    ```
    (若有报错，运行```python easywarp.py stop```可自动暂停)

### Tips (提示)

*   **Empty Log Output (log指令后输出为empty):** If the `python easywarp.py log` command returns "empty", try running the command prompt or terminal as an administrator. This can resolve permission issues that might prevent the script from accessing the necessary logs. (若运行log指令后输出为empty，可尝试以管理员身份运行)
