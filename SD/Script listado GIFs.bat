@echo off
SETLOCAL EnableDelayedExpansion

REM ====================================================================
REM CONFIGURACIÓN AUTOMÁTICA
REM ====================================================================

REM La ruta donde se ejecuta el script (la raíz de la SD)
SET "SD_ROOT_PATH=%CD%"
SET "GIFS_PATH=%SD_ROOT_PATH%\gifs"

SET "CACHE_FILE=%SD_ROOT_PATH%\gif_cache.txt"
SET "SIG_FILE=%SD_ROOT_PATH%\gif_cache.sig"

REM ====================================================================
REM PREPARACIÓN Y VERIFICACIÓN
REM ====================================================================

ECHO --- Generador de Cache GIF Automático (Root) ---
ECHO Carpeta de la SD: %SD_ROOT_PATH%
ECHO Buscando subcarpetas en: %GIFS_PATH%

IF NOT EXIST "%GIFS_PATH%" (
    ECHO ERROR: La carpeta 'gifs' no fue encontrada en la raíz.
    ECHO Asegurese de que la carpeta 'gifs' exista en %SD_ROOT_PATH%
    pause
    EXIT /B 1
)

ECHO. > %CACHE_FILE%
ECHO. > %SIG_FILE%

SET "GIF_COUNT=0"
SET "SIGNATURE="

REM ====================================================================
REM LÓGICA DE ESCANEO Y GENERACIÓN DE CACHÉ
REM ====================================================================

REM Iterar sobre todos los subdirectorios dentro de la carpeta /gifs
FOR /D %%D IN ("%GIFS_PATH%\*") DO (
    
    REM %%~nD: Obtiene solo el nombre de la carpeta (ej. abstracto)
    SET "SUBFOLDER_NAME=%%~nD"

    REM 1. Construir el path de la SD para la firma: /gifs/nombre_carpeta:
    SET "SD_SIGNATURE_PATH=/gifs/!SUBFOLDER_NAME!"
    SET "SIGNATURE=!SIGNATURE!!SD_SIGNATURE_PATH!:"
    
    ECHO Escaneando subcarpeta: !SUBFOLDER_NAME!
    
    REM Búsqueda de archivos .gif dentro de la subcarpeta
    FOR %%G IN ("%%D\*.gif") DO (
        
        REM Obtiene el nombre completo del archivo (con su extensión)
        SET "FILE_NAME=%%~nxG"

        REM 2. Construir la ruta final de la SD para el archivo: /gifs/subfolder/file.gif
        SET "SD_FILE_PATH=/gifs/!SUBFOLDER_NAME!/!FILE_NAME!"
        
        REM Escribir la ruta al archivo de caché
        ECHO !SD_FILE_PATH!>> %CACHE_FILE%
        SET /A GIF_COUNT+=1
    )
)

REM ====================================================================
REM GENERACION DE FIRMA (.sig)
REM ====================================================================

REM Escribir la firma completa al archivo .sig
ECHO !SIGNATURE!> %SIG_FILE%

ECHO.
ECHO =================================================================
ECHO ✅ PROCESO FINALIZADO
ECHO %GIF_COUNT% rutas de GIF escritas en %CACHE_FILE%
ECHO Se incluyeron todas las subcarpetas encontradas en la firma.
ECHO Los archivos han sido creados en la RAIZ de la SD.
ECHO =================================================================
pause
