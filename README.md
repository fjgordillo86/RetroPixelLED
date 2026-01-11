# ✨ Retro Pixel LED v2.1.0

## 💡 Descripción del Proyecto

**Retro Pixel LED** es un firmware avanzado para dispositivos ESP32 diseñado para controlar matrices de LEDs (como las matrices HUB75 PxP o similares) a través de una interfaz web sencilla y potente.
Este sistema permite transformar una matriz LED en un centro de información y arte retro, permitiendo cambiar entre **GIFs animados**, **Texto Deslizante** o un **Reloj sincronizado por NTP**

---
## 🚀 Características Principales (v2.1.0)

| Característica | Descripción | Estado |
| :--- | :--- | :--- |
| **🧠 Dual Core Engine** | **Núcleo 1** dedicado a los LEDs y **Núcleo 0** a la red. Evitar parpadeos. | **Nuevo** |
| **🛡️ Sistema Mutex** | Uso de semáforos para evitar conflictos de lectura en la tarjeta SD. | **Nuevo** |
| **🏠 Home Assistant** | Integración total mediante **MQTT Discovery**. Autodetectable. | **Nuevo** |
| **📁 FileManager Pro** | Gestor de archivos web con soporte para carpetas y subida masiva. | **Mejorado** |
| **⚡ Activar/Desactivar Matriz** | Botón para encender y apagar el panel LED. | **Nuevo** |
| **🌐 Notificación de Conexión** | Mostrar la dirección IP asignada en el panel LED automáticamente al conectarse a la red Wi-Fi por primera vez. | **Nuevo** |

---

## 🛒 Lista de Materiales

Para garantizar la compatibilidad, se recomienda el uso de los componentes probados durante el desarrollo:

* **Microcontrolador:** [ESP32 DevKit V1 (38 pines) - AliExpress](https://es.aliexpress.com/item/1005005704190069.html)
* **Panel LED Matrix (HUB75):** [P2.5 / P3 / P4 RGB Matrix Panel - AliExpress](https://es.aliexpress.com/item/1005007439017560.html)
* **Lector de Tarjetas:** [Módulo Adaptador Micro SD (SPI) - AliExpress](https://es.aliexpress.com/item/1005005591145849.html)
* **Placa conexión ESP32-Panel LED:** [DMDos Board V3 - Mortaca ](https://www.mortaca.com/) (Opcional, no hay que soldar y tiene lector SD incroporado)
* **Alimentación:** Fuente de alimentación de 5V (Mínimo 4A recomendado para paneles de 64x32).

---

## ⚙️ Instalación y Configuración

### 1. 🔌 Conexiones 
Si utilizas DMDos Board V3 esta parte ya la tienes, salta al siguiente punto.

#### 📂 Lector de Tarjeta Micro SD (Interfaz SPI)
| Pin SD | Pin ESP32 | Función |
| :--- | :--- | :--- |
| **CS** | GPIO 5 | Chip Select |
| **CLK** | GPIO 18 | Clock |
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
| **A** | GPIO 33 | Selección de Fila A |
| **B** | GPIO 32 | Selección de Fila B |
| **C** | GPIO 22 | Selección de Fila C |
| **D** | GPIO 17 | Selección de Fila D |
| **E** | GND | GND |
| **CLK** | GPIO 16 | Clock |
| **LAT** | GPIO 4 | Latch |
| **OE** | GPIO 15 | Output Enable (Brillo) |


### 2. 🚀 Programar el ESP32
Ya no es necesario instalar Arduino IDE ni configurar librerías manualmente. Puedes programar tu ESP32 directamente desde el navegador.

### **[👉 Abrir instalador web RETRO PIXEL LED](https://fjgordillo86.github.io/RetroPixelLED/)**

**Pasos para la instalación:**
1. Utiliza un navegador compatible (**Google Chrome** o **Microsoft Edge**).
2. Conecta tu ESP32 al puerto USB del ordenador.
3. Haz clic en el botón **"Install"** de la web y selecciona el puerto COM correspondiente.
4. **IMPORTANTE:** Si es la primera vez que instalas la v2.1.0, asegúrate de marcar la casilla **"Erase device"** en el asistente para realizar una limpieza completa de la memoria y evitar errores de fragmentación.

> 💡 **¿No reconoce tu ESP32?**
> Si al pulsar "Install" no aparece ningún puerto COM, es probable que necesites instalar los drivers del chip USB de tu placa:
> * **Chip CP2102:** [Descargar Drivers Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
> * **Chip CH340/CH341:** [Descargar Drivers SparkFun](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all)
  
### 3. 📂 Preparación de la Tarjeta SD

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

### 4. 🌐 Configuración Inicial y Conexión Wi-Fi

Si es la primera vez que usas el dispositivo o si has cambiado de red, el **Retro Pixel LED** entrará en modo de configuración automática:

1.  **Conexión al Punto de Acceso:** Busca en tu smartphone o PC una red Wi-Fi llamada `Retro Pixel LED`. (No requiere contraseña).
2.  **Portal Cautivo:** Una vez conectado, el navegador debería abrirse automáticamente. Si no lo hace, accede a la dirección `192.168.4.1`.
3.  **Configurar Wi-Fi:** Pulsa en "Configure WiFi", selecciona tu red doméstica, introduce la contraseña y guarda. El ESP32 se reiniciará y se conectará a tu red local.

### 5. 🖥️ Gestión mediante Servidor Web (Web UI)

Una vez que el dispositivo esté en tu red local, puedes acceder a su panel de control introduciendo su dirección IP en el navegador.

> **💡 Cómo encontrar la IP:** > * Se muestra en el **Monitor Serie** al arrancar y en el propio **Panel LED** tras la primera conexión

### Funcionalidades Disponibles:
* **🕹️ Control en Tiempo Real:** Cambia de modo entre **GIF**, **Reloj** o **Marquesina** al instante.
* **☀️ Brillo Inteligente:** Ajusta la intensidad de los LEDs (0-255).
* **📁 Explorador de Archivos SD:** Sube, borra o crea carpetas para tus GIFs sin sacar la tarjeta Micro SD.
* **✍️ Editor de Texto:** Cambia el mensaje de la marquesina, colores y velocidad de desplazamiento.
* **🏠 Home Assistant:** Manejo de todas las funciones disponibles desde Home Assistant.
* **🛠️ Actualización OTA:** Instala nuevas versiones del firmware de forma inalámbrica.

## 🌐 Optimización de Rendimiento (Caché)
Para evitar que el ESP32 escanee toda la tarjeta SD en cada inicio (lo cual es lento), el sistema utiliza un mecanismo de Firma de Validación:

El usuario selecciona las carpetas activas en la interfaz web.
El sistema crea una firma única en gif_cache.sig.
Si al reiniciar las carpetas seleccionadas no han cambiado, el ESP32 lee directamente las rutas desde gif_cache.txt de forma instantánea.

## 🛠️ Próximas Mejoras (Roadmap)

### 🚀 En Desarrollo / Próximamente
* **🎮 Soporte para Batocera/RetroPie:** Integración mediante scripts *game-start* para cambiar el GIF del panel automáticamente según el juego seleccionado en el Frontend (vía API HTTP).
* **🕔 Mejoras en la función Reloj:** Distintos diseños de reloj a elegir desde la WEB.
* **📡 Mejoras en la función Text:** Distintos tamaños de letra...
* **✍️ Control por Infrarrojos (IR):** Soporte para mandos a distancia para control físico (Encendido/Brillo/Modos).
* **💤 Gestión de Energía:** Implementación de modo *Sleep* y apagado programado para prolongar la vida útil de los paneles LED.

## 📚 Librerías Necesarias

En el caso de querer compilar y programar el proyecto dede Arduino IDE correctamente, debes instalar las siguientes librerías. Puedes buscarlas en el Gestor de Librerías de Arduino o descargarlas desde sus repositorios oficiales:

* **[ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA)**: Control de alto rendimiento para el panel LED mediante DMA.
* **[AnimatedGIF](https://github.com/bitbank2/AnimatedGIF)**: Decodificador eficiente para la reproducción de archivos GIF desde la SD.
* **[WiFiManager](https://github.com/tzapu/WiFiManager)**: Gestión de la conexión Wi-Fi mediante un portal cautivo.
* **[Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)**: Librería base para dibujar texto y formas geométricas.
* **[ArduinoJson](https://github.com/bblanchon/ArduinoJson)**: Para la gestión de archivos de configuración y comunicación web.

> **Nota:** Las librerías **SD** y **FS** ya vienen integradas por defecto en el paquete de placas (core) de ESP32 para Arduino.

## ⚖️ Licencia y Agradecimientos
Este proyecto se publica bajo la Licencia MIT. Consulta el archivo `LICENSE` para conocer los términos completos.

Agradecimientos especiales a los desarrolladores de:
* ESP32-HUB75-MatrixPanel-I2S-DMA
* AnimatedGIF
* WiFiManager
