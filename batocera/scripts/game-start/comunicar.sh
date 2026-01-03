#!/bin/bash

# Configuración: Pon aquí la IP del ESP32
IP_ESP32="192.168.xxx.xxx"

# Capturar datos de Batocera
# $1 es el nombre del sistema (ej: nes)
# $2 es la ruta del juego
SISTEMA="$1"
NOMBRE_JUEGO=$(basename -- "$2")
NOMBRE_JUEGO="${NOMBRE_JUEGO%.*}"

# Limpieza para URL
NOMBRE_URL="${NOMBRE_JUEGO// /%20}"

# Enviamos ambos datos al ESP32
curl -s "http://$IP_ESP32/game?name=$NOMBRE_URL&sys=$SISTEMA" &
