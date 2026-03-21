# 📓 Historial de Cambios (Changelog) - Retro Pixel LED


## [4.0.0] - 2026-03-21
### ✨ Añadido (The Playlist & Instant Response Update)
- **📜 Sistema de Playlists Dinámicas:** Implementación de soporte para múltiples listas de reproducción personalizadas mediante archivos `.txt` en la carpeta `/playlists`. Permite al usuario alternar entre colecciones temáticas (ej. "MisFavoritos", "Metal Slug", "Arcade") desde la interfaz web sin necesidad de re-escanear toda la tarjeta SD.
- **⚡ Interrupción Atómica de Renderizado:** Uso de la bandera global `interrumpirReproduccion` sincronizada entre núcleos. Ahora, al cambiar de Playlist, Modo o recibir un comando de Batocera, el GIF actual se detiene de forma **instantánea** (milisegundos), eliminando la espera de finalización del GIF anterior.
- **🖥️ Script Generador de Playlists (v1.0.1):** Nueva herramienta interactiva para Windows que automatiza la creación de listas. Incluye detección inteligente de subcarpetas, normalización de rutas para ESP32 (`/gifs/...`) y creación automática de la estructura de directorios en la SD.
- **💾 Persistencia NVS de Playlist:** El sistema ahora recuerda y carga automáticamente la última playlist seleccionada tras un reinicio, guardando el estado en la memoria no volátil del ESP32.

### 🛠️ Optimizado (Core & Performance)
- **🚀 Carga Instantánea:** Al usar el modo Playlist, el sistema omite el proceso de "LISTANDO GIFs...", permitiendo una reproducción inmediata incluso en tarjetas SD con miles de archivos.
- **🧠 Memory Safety:** Optimización del uso de `std::vector` para el listado dinámico de archivos de playlist en la web, evitando fragmentación de memoria en procesos de selección largos.

### 🐛 Corregido
- **📏 Corrección de Rutas en Archivos:** Solucionado el error que duplicaba la letra de la unidad (ej. `I:\`) en las rutas de los archivos de caché cuando se generaban desde Windows.
- **🔄 Reset de Índice:** Corregido el fallo donde el puntero de lectura (`gifCachePosition`) quedaba fuera de rango al cambiar de una lista larga a una más corta, evitando bloqueos en la lectura de la SD.
---

## [3.0.5] - 2026-02-20
### 🛡️ Corregido (Critical Stability Hotfix)
- **🔒 SD Mutex Integration (Modo Gestión):** Se ha implementado el uso de semáforos (`sdMutex`) en las funciones de listado de archivos y gestor de archivos. Esto elimina el error `Guru Meditation Error: LoadProhibited` que ocurría al intentar navegar por la web mientras el panel reproducía un GIF desde la SD.
- **💾 POST Config Method:** Migración de toda la lógica de guardado de parámetros desde el método `GET` al método `POST`. Esto garantiza que las configuraciones de WiFi, Hardware y MQTT se guarden de forma íntegra, evitando errores de URLs truncadas o fallos de escritura en la memoria Flash.
- **🎨 External CSS:** Migración del estilo visual a un archivo `/style.css` independiente. **Web más rápida.** Libera memoria RAM crítica y permite el uso de caché del navegador.
  
---
## [3.0.4] - 2026-02-19
### ✨ Añadido (Smart Feature & Web Stability Update)
- **🕒 Intercalado de Reloj Automático:** Nueva función que permite mostrar el reloj digital durante 10 segundos tras la reproducción de un número configurable de GIFs. Ideal para mantener el panel informativo sin salir del modo galería.
- **📦 Chunked Transfer Encoding:** Implementación de envío de datos por fragmentos en el servidor Web. Esto permite cargar listas de carpetas y archivos masivos sin agotar la memoria RAM del ESP32, garantizando la carga completa de la interfaz en dispositivos móviles.

### 🛠️ Optimizado (UI/UX Refinement)
- **🎨 Interfaz Unificada:** Reubicación de los controles de "Auto Reloj" dentro de la tarjeta de Ajustes de Galería. La configuración de intervalos y activación ahora es más intuitiva y centralizada.
- **🧼 Limpieza de Buffers HTML:** Refactorización de las funciones `handleRoot` y `handleConfig` para evitar duplicidad de tarjetas y asegurar un renderizado limpio de la configuración MQTT y Hardware.

### ⚙️ Mejoras de Sistema
- **📡 Robustez MQTT:** Sincronización automática de estados tras el guardado de configuración, asegurando que Home Assistant refleje los cambios de modo o texto inmediatamente.
- **🔒 Estabilidad de Memoria:** Reducción drástica del "Heap Peak" durante el uso de la interfaz web gracias al sistema de streaming de contenidos por trozos.

---
## [3.0.3] - 2026-02-10
### ✨ Añadido (Performance & Efficiency Update)
- **🌱 Eco-Energy Mode:** Implementación de escalado dinámico de frecuencia de CPU. El sistema detecta cuando el panel está en estado `Power OFF` y reduce la velocidad del chip de **240MHz a 80MHz**. Esto disminuye drásticamente el consumo y la temperatura, manteniendo el WiFi y Home Assistant totalmente operativos.
- **🚀 Ultra-Fast Wake:** Al encender el panel desde la Web o HA, la CPU restaura instantáneamente los 240MHz para garantizar la decodificación fluida de GIFs sin latencia.

### 🛠️ Optimizado (Modo Arcade Clean UI)
- **🕹️ Arcade Visual Fix:** Eliminada la etiqueta intrusiva "FILES MODE" que aparecía al recibir comandos remotos (Batocera). Ahora, el cambio de GIF es directo y limpio, mejorando la estética de la integración arcade.
- **🖥️ Task Handling:** Refinada la prioridad del `TaskDisplay` para evitar micro-tirones durante el cambio dinámico de frecuencias.

### ⚙️ Compatibilidad
- **🔧 Core Stability:** Optimización validada para el **ESP32 Arduino Core 3.3.5**, asegurando la estabilidad del bus I2S y las peticiones Web bajo el nuevo SDK.

---
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
