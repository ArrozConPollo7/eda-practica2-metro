@echo off
rem Opcion B: solo consola (el main con el menu)
setlocal
cd /d "%~dp0"

if exist "build\Release\navegador.exe" (
  set "EXE=build\Release\navegador.exe"
) else if exist "build\navegador.exe" (
  set "EXE=build\navegador.exe"
) else (
  echo [ERROR] No hay ejecutable. Corra primero compilar_windows.bat
  pause
  exit /b 1
)

echo Menu de consola del navegador de rutas.
"%EXE%"
echo.
pause
