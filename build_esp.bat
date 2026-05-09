@echo off
echo ========================================
echo   小智主机 - 一键编译+烧录脚本
echo ========================================
echo.

cd /d %~dp0
echo 当前目录: %cd%
echo.

set PATH=D:\Espressif\python_env\idf5.5_py3.11_env\Scripts;D:\Espressif\tools\idf-python\3.11.2;D:\Espressif\tools\cmake\3.30.2\bin;D:\Espressif\tools\ninja\1.12.1;%PATH%
set IDF_PYTHON_ENV_PATH=D:\Espressif\python_env\idf5.5_py3.11_env

call D:\Espressif\frameworks\esp-idf-v5.5.3\export.bat
if %ERRORLEVEL% neq 0 (
    echo ESP-IDF 环境加载失败！
    pause
    exit /b 1
)

echo.
echo ========================================
echo   步骤1: 编译小智主机...
echo ========================================
idf.py build
if %ERRORLEVEL% neq 0 (
    echo 编译失败，请检查错误信息！
    pause
    exit /b 1
)

echo.
echo ========================================
echo   步骤2: 烧录到 COM20...
echo ========================================
echo 请确保开发板已连接 USB，按任意键开始烧录...
echo.
pause

idf.py -p COM20 flash
if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================
    echo   烧录成功！正在打开监视器...
    echo   按 Ctrl + ] 退出监视器
    echo ========================================
    idf.py -p COM20 monitor
)

pause
