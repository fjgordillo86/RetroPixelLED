#!/bin/bash
# Dirección IP de tu ESP32
IP_ESP32="192.168.1.109"

# Enviamos una petición especial para volver al modo deseado (ej: modo 1 - GIFs)
curl -G "http://$IP_ESP32/batocera" \
    --data-urlencode "s=OFF" \
    --data-urlencode "g=OFF" > /dev/null 2>&1 &