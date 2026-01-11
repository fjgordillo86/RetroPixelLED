@echo off
setlocal enabledelayedexpansion
title Retro Pixel LED - Indexador Universal v1.5.4

:: --- CONFIGURACIÓN DE RUTAS LOCALES ---
set "cache_name=batocera_cache.txt"
set "listado_nombres=%~dp0nombres_roms_batocera.txt"
set "tmp_file=%~dp0cache_temp.tmp"

:menu
cls
echo ======================================================
echo       RETRO PIXEL LED - GESTOR DE INDICE v1.5.4
echo ======================================================
echo  1. Actualizar UN sistema (ej: snes)
echo  2. Actualizar TODOS los sistemas (Escaneo total)
echo  3. Borrar indice y empezar de cero
echo  4. Salir
echo ======================================================
set /p opt="Selecciona una opcion [1-4]: "

if "%opt%"=="3" (
    if exist "%listado_nombres%" del /f /q "%listado_nombres%"
    echo Archivos locales borrados.
    pause
    goto :menu
)
if "%opt%"=="4" exit

:config_rutas
set /p sd_drive="Letra de unidad SD (ej: E): "
set /p roms_base="Ruta base de ROMs (ej: \\192.168.1.112\share\roms): "

if not exist "%roms_base%" (
    echo [ERROR] No se puede acceder a la ruta de red.
    pause
    goto :menu
)

set "sd_path=%sd_drive%:\batocera"
set "final_cache=%sd_drive%:\%cache_name%"

if not exist "%sd_path%" mkdir "%sd_path%"

:: CREACIÓN/RESETEO DEL ARCHIVO DE REFERENCIA
echo LISTADO DE REFERENCIA PARA GIFS > "%listado_nombres%"
echo Generado el: %date% %time% >> "%listado_nombres%"
echo ========================================== >> "%listado_nombres%"

if "%opt%"=="1" (
    set /p sistema="Introduce el nombre del sistema (ej: snes): "
    call :procesar_sistema
    goto :finalizar
)

if "%opt%"=="2" (
    for /d %%d in ("%roms_base%\*") do (
        set "sistema=%%~nxd"
        call :procesar_sistema
    )
    goto :finalizar
)
goto :menu

:procesar_sistema
echo.
echo --- Procesando sistema: [!sistema!] ---
echo [!sistema!] >> "%listado_nombres%"

if not exist "%sd_path%\!sistema!" mkdir "%sd_path%\!sistema!"

if exist "%final_cache%" (
    findstr /v /c:"|!sistema!|" "%final_cache%" > "%tmp_file%"
    move /y "%tmp_file%" "%final_cache%" >nul
)

:: 1. Logo de Fallback
if exist "%sd_path%\!sistema!\_logo.gif" (
    (echo 01^|!sistema!^|default^|/batocera/!sistema!/_logo.gif)>>"%final_cache%"
)

:: 2. Búsqueda de juegos
set "encontrado=0"
for %%f in ("%roms_base%\!sistema!\*.*") do (
    set "rom_name=%%~nf"
    
    :: ESCRIBIR SIEMPRE EN EL LISTADO DE NOMBRES (para que sepas qué GIFs crear)
    echo   !rom_name! >> "%listado_nombres%"
    
    if exist "%sd_path%\!sistema!\!rom_name!.gif" (
        (echo 00^|!sistema!^|!rom_name!^|/batocera/!sistema!/!rom_name!.gif)>>"%final_cache%"
        echo [OK] Juego: !rom_name!
        set "encontrado=1"
    )
)

if "!encontrado!"=="0" (
    echo [i] No hay GIFs para !sistema!. Revisa: %listado_nombres%
)
echo. >> "%listado_nombres%"
goto :eof

:finalizar
echo.
echo ======================================================
echo  PROCESO COMPLETADO
echo  Listado: %listado_nombres%
echo ======================================================
pause
goto :menu
