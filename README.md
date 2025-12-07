# ✨ Retro Pixel LED v1.4.1

## 💡 Descripción del Proyecto

**Retro Pixel LED** es un firmware avanzado para dispositivos ESP32 diseñado para controlar matrices de LEDs (como las matrices HUB75 PxP o similares) a través de una interfaz web sencilla.

Permite a los usuarios cambiar el **modo de reproducción** (GIFs, Texto Deslizante o Reloj), ajustar el brillo, y modificar la configuración del sistema de manera inalámbrica (OTA). Es ideal para crear pantallas decorativas de estilo retro, relojes digitales, y visualizadores de información personalizables.

## 🚀 Características Principales (v1.4.1)

*   **Múltiples Modos:** Reproducción de GIFs, Texto Deslizante (Marquesina) y Reloj NTP.
*   **Interfaz Web:** Control total de brillo, modos y personalización (colores, velocidad, mensajes).
*   **Configuración Wi-Fi (WiFiManager):** Manejo automático de la conexión y portal cautivo si falla la red.
*   **Actualización Remota (OTA):** Permite cargar nuevo firmware y sistema de archivos (SPIFFS/LittleFS) de forma inalámbrica.

---

## ⚙️ Instalación y Configuración

### 1. Requisitos de Hardware

*   **Microcontrolador:** ESP32.
*   **Pantalla LED:** Matriz LED compatible con HUB75 (o el hardware de control que utilices).

### 2. Librerías de Arduino Necesarias

Este proyecto requiere las siguientes librerías de terceros (disponibles en el Gestor de Librerías de Arduino o GitHub):

| Librería | Autor/Fuente | Licencia (General) |
| :--- | :--- | :--- |
| **WiFiManager** | T. J. T. T. / Tzapu | MIT |
| **ESP32_HUB75_LED_MATRIX_PANEL_DMA_Display** | mrcodetastic | MIT |
| *Otras específicas* | *...* | *...* |

**Nota:** Las librerías estándar del framework ESP32 (`WiFi.h`, `WebServer.h`, `SPIFFS.h`, `Preferences.h`, etc.) ya están incluidas con el soporte de placa.

### 3. Carga Inicial

1.  Abre el proyecto en tu IDE de Arduino/VSCode + PlatformIO.
2.  Asegúrate de configurar correctamente los pines del ESP32 para la matriz LED en el código.
3.  Carga el código y luego utiliza la herramienta "ESP32 Sketch Data Upload" para subir los archivos estáticos (GIFs, fuentes, HTML de la interfaz) a la partición **SPIFFS** o **LittleFS**.

---

## 🌐 Conexión

Al iniciar, el ESP32 intentará conectarse. Si no encuentra las credenciales o la red falla, creará un **Punto de Acceso (AP)** llamado **`Retro Pixel LED`** donde podrás configurar tu Wi-Fi.

## ⚖️ Licencia y Agradecimientos

Este proyecto de firmware se publica bajo la **Licencia MIT**.

Agradecemos a los desarrolladores de las librerías mencionadas anteriormente, cuyo trabajo bajo licencias permisivas (principalmente **MIT**) hace posible este proyecto. Consulta el archivo `LICENSE` para conocer los términos completos.
