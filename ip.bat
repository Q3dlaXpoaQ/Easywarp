@echo off & setlocal enabledelayedexpansion
goto start

:start
if not exist "warp.exe" (
    echo warp.exe not found.
    pause
    exit
)
goto main

:main
title CF WARP Endpoint Selector
set /a menu=1
echo 1. Select WARP IPv4 Endpoint
echo 2. Select WARP IPv6 Endpoint
echo 0. Exit
echo.
set /p menu=Select an option (default %menu%):
if "%menu%"=="" set menu=1
if "%menu%"=="0" exit
if "%menu%"=="1" (
    title WARP IPv4 Endpoint Selection
    goto getv4
)
if "%menu%"=="2" (
    title WARP IPv6 Endpoint Selection
    goto getv6
)
cls
goto main

:getv4
echo.
echo Loading IPv4 addresses from built-in list...
(   echo 162.159.192.0/24
    echo 162.159.193.0/24
    echo 162.159.195.0/24
    echo 188.114.96.0/24
    echo 188.114.97.0/24
    echo 188.114.98.0/24
    echo 188.114.99.0/24
) > ips-v4.txt
for /f "delims=" %%i in (ips-v4.txt) do (
    set !random!_%%i=randomsort
)
del ips-v4.txt
echo IP list loaded. Randomizing...
for /f "tokens=2,3,4 delims=_.=" %%i in ('set ^| findstr =randomsort ^| sort /m 10240') do (
    call :randomcidrv4
    if not defined %%i.%%j.%%k.!cidr! set %%i.%%j.%%k.!cidr!=anycastip&set /a n+=1
    if !n! EQU 100 goto getip
)
goto getv4

:randomcidrv4
set /a cidr=%random%%%256
goto :eof

:getv6
echo.
echo Loading IPv6 addresses from built-in list...
(   echo 2606:4700:d0::/48
    echo 2606:4700:d1::/48
) > ips-v6.txt
for /f "delims=" %%i in (ips-v6.txt) do (
    set !random!_%%i=randomsort
)
del ips-v6.txt
echo IP list loaded. Randomizing...
for /f "tokens=2,3,4 delims=_:=" %%i in ('set ^| findstr =randomsort ^| sort /m 10240') do (
    call :randomcidrv6
    if not defined [%%i:%%j:%%k::!cidr!] set [%%i:%%j:%%k::!cidr!]=anycastip&set /a n+=1
    if !n! EQU 100 goto getip
)
goto getv6

:randomcidrv6
set str=0123456789abcdef
set /a r=%random%%%16
set cidr=!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!:!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!:!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!:!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
set /a r=%random%%%16
set cidr=!cidr!!str:~%r%,1!
goto :eof

:getip
del ip.txt > nul 2>&1
for /f "tokens=1 delims==" %%i in ('set ^| findstr =randomsort') do (
    set %%i=
)
for /f "tokens=1 delims==" %%i in ('set ^| findstr =anycastip') do (
    echo %%i>>ip.txt
)
for /f "tokens=1 delims==" %%i in ('set ^| findstr =anycastip') do (
    set %%i=
)
warp
del ip.txt > nul 2>&1
echo.
echo Endpoint selection complete. Press any key to exit.
pause>nul
exit