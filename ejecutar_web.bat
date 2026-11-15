@echo off
rem Opcion A: interfaz web en http://localhost:8080
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

echo Levantando el servidor en http://localhost:8080
echo (deje esta ventana abierta; Ctrl+C para detenerlo)
start "Metro de Medellin - servidor" "%EXE%" --web
timeout /t 2 >nul
start "" http://localhost:8080
exit /b 0
