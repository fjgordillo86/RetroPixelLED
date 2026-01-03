#!/bin/bash

# Configuración: Pon aquí la IP del ESP32
IP_ESP32="192.168.xxx.xxx"

# Enviamos el comando "DEFAULT" al salir del juego
curl -s "http://$IP_ESP32/game?name=DEFAULT" &
