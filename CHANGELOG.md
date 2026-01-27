# 📓 Historial de Cambios (Changelog) - Retro Pixel LED

## [3.0.1] - 2026-01-27
### 🛠️ Corregido (Hotfix: True Random Engine)
- **🎲 Hardware RNG Integration:** Eliminado el uso de randomSeed() y random() por software. Ahora el sistema utiliza esp_random() directamente, que lee el ruido térmico del chip ESP32. Esto garantiza que la secuencia de GIFs sea 100% diferente en cada reinicio, corrigiendo el patrón repetitivo.
- **📁 Optimización de Punteros SD:** Refinada la lógica de alineación de líneas en el archivo de caché para asegurar que el modo aleatorio siempre lea rutas de archivos completas y válidas.

---
## [3.0.0] - 2026-01-25
### ✨ Añadido (Major Update: Infinite SD Engine)
- **♾️ SD Streaming Engine:** Implementación de lectura directa de archivos GIF desde la SD. Eliminada la limitación de memoria RAM para las listas de archivos.
- **📟 UI de Estado:** Nueva pantalla informativa "LISTANDO GIFs..." con coordenadas corregidas para feedback visual inmediato durante el escaneo de la SD.
- **⚡ Proceso No Bloqueante:** Inserción de `yield()` en los bucles de escaneo de archivos, manteniendo el servidor Web y el sistema activos durante procesos largos.
- **🎯 Interrupción Instantánea:** Mejora en el núcleo de reproducción que permite detener GIFs largos en milisegundos para procesar cambios de configuración o nuevas búsquedas.

### 🛠️ Optimizado
- **Zero RAM Footprint:** Sustitución de `std::vector` por punteros de posición (`seek`) en archivos de texto planos.
- **Sistema de Firmas (Signature):** El sistema ahora detecta si la configuración de carpetas ha cambiado para evitar re-escaneos innecesarios en cada reinicio.
- **Robustez OTA:** Refactorización total del proceso de actualización; ahora se detiene la tarea del segundo núcleo (`vTaskDelete`) para dedicar el 100% de la CPU a la escritura del firmware, eliminando los fallos de "Not Found".

---

## [2.2.9] - 2026-01-25
### ✨ Añadido
- **Control Dinámico de Reloj:** Implementación de `startY` variable (6px/9px) según el estado de las notificaciones MQTT.
- **8 Estilos de Reloj:** Añadidos modos Rainbow, Solid Neon, Pulse Breath, Matrix Digital y Gradients.
- **Hardware Tuning:** Ajustes configurables desde Web para velocidad I2S, Refresh Rate y Latch Blanking.
- **Modo WiFi Híbrido:** Capacidad de operar en modo Offline sin bloqueos de búsqueda de red.
- **Integración HA Pro:** Soporte para iconos de clima (Sol, Nubes, Lluvia, etc.), temperatura y envío de texto dinámico vía MQTT.

### 🛠️ Optimizado
- **Estabilidad del Panel:** Refactorización del motor de dibujado eliminando el glitching ("píxeles locos") al 100%.
- **Gestión de Memoria:** Cambio de esquema de particiones a `Minimal SPIFFS` para soportar el binario de 1.2MB.
- **Sincronización MQTT:** El cambio a modo GIF ahora es instantáneo gracias al reseteo forzado del índice y cierre del objeto `gif`.

### 🐛 Corregido
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
