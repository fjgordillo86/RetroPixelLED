# ✨ Retro Pixel LED v2.0.9

## 💡 Descripción del Proyecto

**Retro Pixel LED** es un firmware avanzado para dispositivos ESP32 diseñado para controlar matrices de LEDs (como las matrices HUB75 PxP o similares) a través de una interfaz web sencilla y potente.

Este sistema permite transformar una matriz LED en un centro de información y arte retro, permitiendo cambiar entre **GIFs animados**, **Texto Deslizante** o un **Reloj sincronizado por NTP**. La versión 2.0.9 introduce un sistema de archivos optimizado para eliminar los tiempos de espera al leer la tarjeta SD.

---

## 🛒 Lista de Materiales

Para garantizar la compatibilidad, se recomienda el uso de los componentes probados durante el desarrollo:

* **Microcontrolador:** [ESP32 DevKit V1 (38 pines) - AliExpress](https://es.aliexpress.com/item/1005005704190069.html)
* **Panel LED Matrix (HUB75):** [P2.5 / P3 / P4 RGB Matrix Panel - AliExpress](https://es.aliexpress.com/item/1005007439017560.html)
* **Lector de Tarjetas:** [Módulo Adaptador Micro SD (SPI) - AliExpress](https://es.aliexpress.com/item/1005005591145849.html)
* **Alimentación:** Fuente de alimentación de 5V (Mínimo 4A recomendado para paneles de 64x32).

---

## 📚 Librerías Necesarias

Para compilar este proyecto correctamente, debes instalar las siguientes librerías. Puedes buscarlas en el Gestor de Librerías de Arduino o descargarlas desde sus repositorios oficiales:

* **[ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA)**: Control de alto rendimiento para el panel LED mediante DMA.
* **[AnimatedGIF](https://github.com/bitbank2/AnimatedGIF)**: Decodificador eficiente para la reproducción de archivos GIF desde la SD.
* **[WiFiManager](https://github.com/tzapu/WiFiManager)**: Gestión de la conexión Wi-Fi mediante un portal cautivo.
* **[Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)**: Librería base para dibujar texto y formas geométricas.
* **[ArduinoJson](https://github.com/bblanchon/ArduinoJson)**: Para la gestión de archivos de configuración y comunicación web.

> **Nota:** Las librerías **SD** y **FS** ya vienen integradas por defecto en el paquete de placas (core) de ESP32 para Arduino.

---
  
## 🚀 Características Principales (v2.0.9)

| Característica | Descripción | Estado |
| :--- | :--- | :--- |
| **Múltiples Modos** | GIFs animados, Texto Deslizante (Marquesina) y Reloj NTP. | Estándar |
| **Gestión SD Web** | Interfaz para subir, borrar y organizar archivos directamente desde el navegador. | **Nuevo (v2.x)** |
| **Indexación de Caché** | Carga instantánea de GIFs mediante archivos `.txt` y `.sig`, evitando escaneos lentos. | **Mejorado (v2.0.9)** |
| **Filtro de Carpetas** | La UI solo muestra subcarpetas dentro del directorio `/gifs` para mayor orden. | **Nuevo (v2.0.9)** |
| **Actualización OTA** | Carga de nuevo firmware de forma inalámbrica sin conectar el cable USB. | Estándar |

---

## ⚙️ Instalación y Configuración

### 1. Conexiones (Pinout para ESP32 38-pin)

#### 📂 Lector de Tarjeta Micro SD (Interfaz SPI)
| Pin SD | Pin ESP32 | Función |
| :--- | :--- | :--- |
| **CS** | GPIO 5 | Chip Select |
| **SCK** | GPIO 18 | Clock |
| **MOSI** | GPIO 23 | Master Out Slave In |
| **MISO** | GPIO 19 | Master In Slave Out |
| **VCC** | 3.3V | Alimentación |
| **GND** | GND | GND |

#### 🖼️ Panel LED RGB (Interfaz HUB75)
| Pin Panel | Pin ESP32 | Función |
| :--- | :--- | :--- |
| **R1** | GPIO 25 | Datos Rojo (Superior) |
| **G1** | GPIO 26 | Datos Verde (Superior) |
| **B1** | GPIO 27 | Datos Azul (Superior) |
| **R2** | GPIO 14 | Datos Rojo (Inferior) |
| **G2** | GPIO 12 | Datos Verde (Inferior) |
| **B2** | GPIO 13 | Datos Azul (Inferior) |
| **A** | GPIO 2 | Selección de Fila A |
| **B** | GPIO 15 | Selección de Fila B |
| **C** | GPIO 4 | Selección de Fila C |
| **D** | GPIO 16 | Selección de Fila D |
| **CLK** | GPIO 22 | Clock |
| **LAT** | GPIO 17 | Latch |
| **OE** | GPIO 33 | Output Enable (Brillo) |
| **GND** | GND | GND |
  
### 2. Preparación de la Tarjeta SD

Es fundamental formatear la tarjeta en **FAT32** y mantener la siguiente estructura:

```text
/ (Raíz de la SD)
├── gifs/ 
│   ├── Arcade/      <-- GIFs organizados por categorías
│   └── Consolas/    <-- GIFs organizados por categorías
├── gif_cache.txt    <-- Generado automáticamente (Índice de rutas)
└── gif_cache.sig    <-- Generado automáticamente (Firma de validación)
```

[!IMPORTANTE] Límite de archivos: Se recomienda no superar los 100-150 GIFs por subcarpeta. Superar este límite puede agotar la memoria RAM del ESP32 durante la generación de la interfaz web de selección.

### 3. Gestión mediante Servidor Web (Web UI)

El firmware levanta un servidor web interno en el ESP32 que permite gestionar el dispositivo desde cualquier navegador (móvil o PC) conectado a la misma red:

* **Control en Tiempo Real:** Cambia entre los modos GIF, Reloj o Texto de forma instantánea.
* **Ajuste de Brillo:** Deslizador para controlar la intensidad lumínica del panel.
* **Personalización:** Configura los mensajes de la marquesina, colores y velocidad de desplazamiento.
* **Explorador de Archivos:** Sube nuevos GIFs a la SD, crea carpetas o borra archivos sin necesidad de extraer la tarjeta Micro SD.
* **Configuración de Red:** Acceso al panel de gestión de Wi-Fi para cambiar de red si es necesario.

> **Nota:** Para acceder, simplemente introduce la dirección IP que el ESP32 muestra en el monitor serie (o la que verás en el panel en futuras versiones) en la barra de direcciones de tu navegador.

## 🌐 Optimización de Rendimiento (Caché)
Para evitar que el ESP32 escanee toda la tarjeta SD en cada inicio (lo cual es lento), el sistema utiliza un mecanismo de Firma de Validación:

El usuario selecciona las carpetas activas en la interfaz web.
El sistema crea una firma única en gif_cache.sig.
Si al reiniciar las carpetas seleccionadas no han cambiado, el ESP32 lee directamente las rutas desde gif_cache.txt de forma instantánea.

## 🛠️ Próximas Mejoras (Roadmap)

* **🌐 Notificación de Conexión:** Mostrar la dirección IP asignada en el panel LED automáticamente al conectarse a la red Wi-Fi por primera vez.
* **🎮 Control por Infrarrojos (IR):** Soporte para mandos a distancia para encendido/apagado, cambio de modo y ajuste de brillo.
* **🏠 Integración Domótica:** Implementación de API REST o MQTT para control desde Home Assistant.
* **💤 Modo de Reposo:** Implementación de ahorro de energía (Light Sleep) para reducir el consumo cuando el panel no esté en uso.
* **🔌 Integración con Frontends:** Implementación de API HTTP/REST para permitir que programas externos (RetroPie, LaunchBox) cambien el GIF automáticamente al iniciar un juego.

## ⚖️ Licencia y Agradecimientos
Este proyecto se publica bajo la Licencia MIT. Consulta el archivo `LICENSE` para conocer los términos completos.

Agradecimientos especiales a los desarrolladores de:
* ESP32-HUB75-MatrixPanel-I2S-DMA
* AnimatedGIF
* WiFiManager
