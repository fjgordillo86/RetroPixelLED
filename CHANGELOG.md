# 📓 Historial de Cambios (Changelog) - Retro Pixel LED

## [2.2.9] - 2026-01-25
### ✨ Añadido
- **Control Dinámico de Reloj:** Implementación de `startY` variable (6px/9px) según el estado de las notificaciones MQTT.
- **8 Estilos de Reloj:** Añadidos modos Rainbow, Solid Neon, Pulse Breath, Matrix Digital y Gradients.
- **Hardware Tuning:** Ajustes configurables desde Web para velocidad I2S, Refresh Rate y Latch Blanking.
- **Modo WiFi Híbrido:** Capacidad de operar en modo Offline sin bloqueos de búsqueda de red.
- **Integración HA Pro:** Soporte para iconos de clima, temperatura y envío de texto dinámico vía MQTT.

### 🛠️ Optimizado
- **Estabilidad del Panel:** Refactorización del motor de dibujado eliminando el glitching ("píxeles locos") al 100%.
- **Gestión de Memoria:** Cambio de esquema de particiones a `Minimal SPIFFS` para soportar el binario de 1.2MB.
- **Sincronización MQTT:** El cambio a modo GIF ahora es instantáneo gracias al reseteo forzado del índice y cierre del objeto `gif`.

### 🐛 Corregido
- **Pantalla Negra en OTA:** Se detiene el segundo núcleo (`enModoGestion`) y se cierra la SD antes de iniciar la actualización.
- **Persistencia de Color:** Corregido error en la conversión de colores de 24 bits para el modo Texto y Neon.
- **Refresco de Pantalla:** Eliminados restos visuales al cambiar entre efectos de reloj o minutos.

---

## [2.1.9] - 2026-01-10
### ✨ Añadido
- **Modo Arcade:** Integración inicial con scripts de Batocera.
- **Dual Core Engine:** Primera implementación estable usando `vTaskCreatePinnedToCore`.
- **FileManager:** Subida de archivos vía Web.

### 🛠️ Optimizado
- **Sistema Mutex:** Protección básica de acceso a la tarjeta SD.
