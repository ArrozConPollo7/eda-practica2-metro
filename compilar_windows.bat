@echo off
rem Compila el navegador del Metro en Windows (Visual Studio o MinGW).
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] No se encontro CMake en el PATH.
  echo Instalelo desde https://cmake.org/download/ y vuelva a intentar.
  echo Si usa Visual Studio, instale la carga "Desarrollo para el escritorio con C++".
  pause
  exit /b 1
)

echo Compilando...
cmake -S . -B build
if errorlevel 1 goto error
cmake --build build --config Release
if errorlevel 1 goto error

echo.
if exist "build\Release\navegador.exe" (
  echo Listo: build\Release\navegador.exe
) else if exist "build\navegador.exe" (
  echo Listo: build\navegador.exe
) else (
  echo [ERROR] La compilacion no dejo un ejecutable.
  pause
  exit /b 1
)
echo.
echo Para la interfaz web:   ejecutar_web.bat
echo Para el menu de consola: ejecutar_consola.bat
pause
exit /b 0

:error
echo.
echo [ERROR] Fallo la compilacion. Revise los mensajes de arriba.
pause
exit /b 1
