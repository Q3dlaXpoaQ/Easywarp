@echo off
chcp 65001
REM 获取当前用户名
for /f "tokens=2 delims=\" %%a in ('whoami') do set "username=%%a"

REM 定义目标文件夹路径
set "target_folder=c:\users\%username%\.easywarp"

REM 检查目标文件夹是否存在，如果不存在则创建
if not exist "%target_folder%" mkdir "%target_folder%"

REM 移动 ip.bat 文件
if exist "ip.bat" (
  copy "ip.bat" "%target_folder%"
  echo ip.bat 已移动到 %target_folder%
) else (
  echo 找不到 ip.bat 文件
)

REM 移动 warp.exe 文件
if exist "warp.exe" (
  copy "warp.exe" "%target_folder%"
  echo warp.exe 已移动到 %target_folder%
) else (
  echo 找不到 warp.exe 文件
)

pause
