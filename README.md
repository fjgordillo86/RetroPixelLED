# ✨ Retro Pixel LED v2.1.0

## 💡 Descripción del Proyecto

**Retro Pixel LED** es un firmware avanzado para dispositivos ESP32 diseñado para controlar matrices de LEDs (como las matrices HUB75 PxP o similares) a través de una interfaz web sencilla y potente.
Este sistema permite transformar una matriz LED en un centro de información y arte retro, permitiendo cambiar entre **GIFs animados**, **Texto Deslizante** o un **Reloj sincronizado por NTP**

---
## 🚀 Características Principales (v2.1.9)

| Característica | Descripción | Estado |
| :--- | :--- | :--- |
| **🕹️ Modo Arcade** | Integración nativa para Frontends (Batocera/RetroPie). Cambio dinámico de GIFs vía API/MQTT. | **Nuevo** |
| **🧠 Dual Core Engine** | **Núcleo 0:** Gestiona la Web, WiFi, gestor de Archivos, protocolo MQTT y actualizaciones OTA. **Núcleo 1:** Dedicado exclusivamente al renderizado de la matriz LED y la decodificación de GIFs.| **Optimizado** |
| **🛡️ Sistema Mutex** | Uso de semáforos para evitar conflictos de lectura en la tarjeta SD. | **Estable** |
| **🏠 Home Assistant** | Integración total mediante **MQTT Discovery**. Autodetectable. | **Mejorado** |
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
├── gifs/                <-- Uso exclusivo para el Modo Galeria de GIFs.
│   ├── Arcade/          <-- GIFs organizados por categorías.
│   └── Consolas/        <-- GIFs organizados por categorías.
├── Batocera/            <-- Uso exclusivo para el Modo Arcade.
│   ├── default/         <-- Generada automáticamente con el Script. (Aquí ira el gif por defecto "_default.gif")
│   ├── mame/            <-- Generada automáticamente con el Script. GIFs organizados por sistemas
│   └── neogeo/          <-- Generada automáticamente con el Script. GIFs organizados por sistemas
├── batocera_cache.txt   <-- Generado automáticamente con el Script. (Índice de rutas - Modo Arcade)
├── gif_cache.txt        <-- Generado automáticamente (Índice de rutas - Modo Galeria de GIFs)
└── gif_cache.sig        <-- Generado automáticamente (Firma de validación - Modo Galeria de GIFs)
```

> [!CAUTION]
>**[IMPORTANTE]** Límite de archivos para el "Modo Galeria de GIFs": Se recomienda no superar los 100-150 GIFs por subcarpeta. Superar este límite puede agotar la memoria RAM del ESP32 durante la generación de la interfaz web de selección.


### 4. 🌐 Configuración Inicial y Conexión Wi-Fi

Si es la primera vez que usas el dispositivo o si has cambiado de red, el **Retro Pixel LED** entrará en modo de configuración automática:

1.  **Conexión al Punto de Acceso:** Busca en tu smartphone o PC una red Wi-Fi llamada `Retro Pixel LED`. (No requiere contraseña).
2.  **Portal Cautivo:** Una vez conectado, el navegador debería abrirse automáticamente. Si no lo hace, accede a la dirección `192.168.4.1`.
3.  **Configurar Wi-Fi:** Pulsa en "Configure WiFi", selecciona tu red doméstica, introduce la contraseña y guarda. El ESP32 se reiniciará y se conectará a tu red local.


### 5. 🖥️ Gestión mediante Servidor Web (Web UI)

Una vez que el dispositivo esté en tu red local, puedes acceder a su panel de control introduciendo su dirección IP en el navegador.

> **💡 Cómo encontrar la IP:** > * Se muestra en el **Monitor Serie** al arrancar y en el propio **Panel LED** tras la primera conexión

### Funcionalidades Disponibles:
* **🕹️ Control en Tiempo Real:** Cambia de modo entre **Galería de GIF** - **Reloj** - **Texto Deslizante** - **Arcade** al instante.
* **☀️ Brillo Inteligente:** Ajusta la intensidad de los LEDs (0-100%).
* **📁 Explorador de Archivos SD:** Sube, borra o crea carpetas para tus GIFs sin sacar la tarjeta Micro SD.
* **✍️ Editor de Texto:** Cambia el mensaje de la marquesina, colores y velocidad de desplazamiento.
* **🏠 Home Assistant:** Manejo de todas las funciones disponibles desde Home Assistant.
* **🛠️ Actualización OTA:** Instala nuevas versiones del firmware de forma inalámbrica.


### 6. 🕹️ Integración con Batocera (Modo Arcade)

El **Modo Arcade** es una de las funciones más potentes de esta versión, permitiendo que la matriz LED actúe como una "Marquesina Arcade" dinámica que reacciona en tiempo real al juego que selecciones en tu sistema **Batocera**.

### 1. El Concepto de Doble Lógica
A diferencia del Modo Galería de GIFs (reproducción aleatoria), el Modo Arcade es específico. El sistema no busca un GIF al azar, sino que busca el archivo exacto que corresponde al juego que acabas de lanzar.

### 2. Flujo de Trabajo y Script de Sincronización ROMS-GIFs
Para facilitar la gestión, el sistema utiliza el Script **Listar Gifs - Modo Arcade** (disponible en la carpeta `/tools`) que automatiza todo el proceso:

1.  **Generación de Estructura:** El script escanea tus ROMs y crea automáticamente las carpetas por sistema dentro de `/Batocera/` (ej: `/Batocera/mame/`, etc.).
2.  **Creación del Índice Maestro:** Genera el archivo `batocera_cache.txt` en la raíz de la SD. Este archivo contiene la ruta exacta de cada juego, permitiendo que el ESP32 no tenga que "navegar" por las carpetas, sino que vaya directo al archivo.
3.  **Diccionario de Nombres:** Genera el archivo `nombres_roms_batocera.txt`en la misma ubicacion que tengas el script. Este archivo  contiene el listado de todos los nombres de ROMs detectados. 
    * **¿Para qué sirve?** Sirve de guía para que sepas exactamente qué nombre ponerle a tus archivos GIF. Si tu ROM se llama `mslug2.zip`, el script te indicará que el GIF debe llamarse `mslug2.gif`.

### 2.1 🛠️ Cómo usar el Script Listar Gifs - Modo Arcade (Windows .bat)

El script se encuentra en la carpeta `/tools` del repositorio. Es una herramienta automatizada para Windows que prepara y sincroniza tu tarjeta SD en tres fases:

#### Fase 1: Preparación y Nomenclatura
1. **Conecta la SD** de tu Retro Pixel LED a tu ordenador.
2. **Ejecuta el archivo:** Haz doble clic sobre Script `Listar Gifs - Modo Arcade.bat`.
3. **Configuración de rutas:** El script te preguntará la letra de unidad SD (ej: E) y la ubicación de tu carpeta de ROMs de Batocera. Puedes usar una ruta local o una ruta de red:
   * **Ruta local:** `ej-> D:\share\roms` (Si tienes el disco de Batocera conectado al PC).
   * **Ruta de red:** `ej-> \\192.168.1.112\share\roms` (Si accedes vía WiFi/Ethernet).
4. **Resultado:** El script creará las carpetas por sistema en tu SD y generará el archivo **`nombres_roms_batocera.txt`** en la misma ubicacion que tengas el script. Este archivo es tu guía para saber qué nombre exacto debe tener cada GIF.
   
#### Fase 2: Personalización de Assets
Antes de volver a pasar el script, debes organizar tus archivos:
* **GIFs de Juegos:** Copia tus GIFs en sus carpetas correspondientes usando los nombres que viste en el `.txt`.
> [!CAUTION]
> Si la ROM es `mslug.zip`, el GIF debe ser `mslug.gif`.Cuidado con añadir espacios en blanco al principio o final del nombre ej: `mslug .gif` puede causar que se encuentre el gif.
  
* **Logos de Sistema:** Dentro de cada carpeta (ej: `/Batocera/mame/`), añade un GIF llamado **`_logo.gif`**. Este se mostrará en caso de que no tengas el GIF del juego.
* **GIF por Defecto:** En la carpeta `/Batocera/default/`, si esta no esta creala manuealmente y añade un GIF llamado **`_default.gif`**. Este es el recurso maestro y se mostrará en dos casos:
    * Si falta tanto el logo del sistema como el GIF específico del juego.
    * Cuando sales de un juego y vuelves al menú.
       
#### Fase 3: Generación del Índice (Caché)
1.  **Ejecuta el script de nuevo:** Una vez hayas copiado tus GIFs, ejecuta el Script `Listar Gifs - Modo Arcade.bat` otra vez.
2.  **Sincronización:** El script detectará las coincidencias reales entre tus ROMs y tus GIFs, y generará el archivo maestro **`batocera_cache.txt`**.
3.  **¡Listo!:** Expulsa la SD y colócala en tu panel LED.

> [!CAUTION]
> **Acceso por Red (Samba):**
> Si al intentar acceder a la ruta `ej-> \\192.168.1.112\share\roms` Windows te solicita credenciales, utiliza las que trae Batocera por defecto:
> * **Usuario:** `root`
> * **Contraseña:** `linux`

> [!TIP]
> **Sincronización rápida:** Cada vez que añadas nuevos GIFs a las carpetas de `/Batocera/`, vuelve a ejecutar el `.bat` para que el ESP32 reconozca los nuevos archivos en el índice de caché.

### 3 🛰️ Configuración de Scripts en Batocera (Comunicación)

Para que el panel LED cambie automáticamente, debemos instalar tres scripts en tu sistema Batocera. Esto permite que Batocera notifique al ESP32 cada vez que inicias o cierras un juego y apagas el sistema. Estos se encuentran en la carpeta `batocera/scripts`

#### A. Cómo editar los scripts (Configurar la IP)
> [!CAUTION]
>  No utilices el Bloc de Notas básico de Windows, ya que cambiaría el formato de fin de línea a Windows (CRLF) y el script dejará de funcionar en Batocera. 

1. **Usa un editor avanzado:** Abre los archivos `pixel_start.sh`, `pixel_stop.sh` y `pixel_off.sh` con **Notepad++**, **VS Code** o **Sublime Text**.
2. **Cambia la IP:** Busca la línea del comando y sustituye la IP de ejemplo por la IP de tu ESP32.
3. **Verifica el formato Unix:** En Notepad++, asegúrate de que en la esquina inferior derecha indique **Unix (LF)**. Si dice Windows (CRLF), ve a *Editar > Conversión de fin de línea > Convertir a Formato Unix (LF)*.
4. **Guarda los cambios.**
   
#### B. Ubicación de los Scripts
Debes colocar los archivos en la carpeta de configuración de EmulationStation. Puedes acceder vía red (Samba) a la siguiente ruta:
[cite_start]`\\192.168.1.xxx\share\system\configs\emulationstation\scripts` 

Organiza los archivos en estas subcarpetas:
* `/game-start/pixel_start.sh` (Se activa al lanzar un juego).
* `/game-end/pixel_stop.sh` (Se activa al salir al menú).
* `/quit/pixel_off.sh` (Se activa al apagar Batocera).

#### C. Asignación de Permisos de Ejecución
  Es **obligatorio** otorgar permisos de ejecución a los archivos mediante una consola SSH (como PuTTY). Ejecuta los siguientes comandos:

  1. **Conéctate por SSH:** Abre PuTTY, introduce la IP de tu Batocera en "Host name" y pincha en "Open".
     <img width="453" height="444" alt="Putty Configuración" src="https://github.com/user-attachments/assets/2eec17d6-36b0-4ef5-bea6-b4d30aa8ee01" />

  2. **Identificate:** Usa el usuario `root` y contraseña `linux`.
     <img width="661" height="519" alt="Putty login" src="https://github.com/user-attachments/assets/46308f5f-d01a-4495-97f7-e8c07bc3915f" />

  3. **Otorgar permisos a los script:** Copia y pega (clic derecho en PuTTY para pegar) es romendable enviarlos de un en uno:
     <img width="862" height="516" alt="Putty permisos" src="https://github.com/user-attachments/assets/9d38f2d1-4065-40ad-a100-7651da94c1af" />
      ```bash
      # Comandos para otorgar permisos de ejecución
      chmod +x /userdata/system/configs/emulationstation/scripts/game-start/pixel_start.sh 
      chmod +x /userdata/system/configs/emulationstation/scripts/game-end/pixel_stop.sh
      chmod +x /userdata/system/configs/emulationstation/scripts/quit/pixel_off.sh
      ```
  4. **Verificar permisos de los script:** Copia y pega (clic derecho en PuTTY para pegar) es romendable enviarlos de un en uno:
     <img width="871" height="516" alt="Putty verificar permisos" src="https://github.com/user-attachments/assets/863665be-9eb3-42c8-a1a4-06f39dfbb7ba" />
      ```bash
      # Comandos para verificar permisos de ejecución:
      ls -l /userdata/system/configs/emulationstation/scripts/game-start/pixel_start.sh
      ls -l /userdata/system/configs/emulationstation/scripts/game-end/pixel_stop.sh
      ls -l /userdata/system/configs/emulationstation/scripts/quit/pixel_off.sh
      ```
      **Ejemplo de permisos de ejecución correctos:**-rwxr-xr-x 1 root root 320 ene 10 13:18 /userdata/system/configs/emulationstation/scripts/game-end/pixel_stop.sh


### 4. Funcionamiento en Tiempo Real
* **Inicio de Juego:** El script `pixel_start.sh` envía el sistema y la ROM al ESP32
* **Cierre de Juego:** El script `pixel_stop.sh` indica al ESP32 que vuelva a mostrar el logo del sistema o el archivo `_default.gif`
* **Apagado del Sistema:** Al apagar Batocera, se envía un comando final para que el panel LED pase a modo **Galería de GIF**.
* **Gestión de Errores:** Si no existe el logo o el GIF del juego, o mientras navegas por los menús generales, el panel mostrará siempre el `_default.gif`


### 5. Configuración Crítica: IP Fija para el ESP32

Para que el modo **🕹️ Arcade** de Batocera funcione siempre correctamente, es fundamental que el ESP32 mantenga siempre la misma dirección IP.

> [!TIP]
> **Asignar IP fija al ESP32:** > Los scripts de Batocera envían las órdenes (como cambiar el GIF al lanzar un juego) a una dirección IP específica que tú configuras manualmente. Si el router reinicia y le asigna una IP distinta al ESP32, la comunicación se cortará y el panel dejará de actualizarse.
>
> **¿Cómo hacerlo?**
> 1. Accede a la configuración de tu router.
> 2. Busca la sección de **DHCP Estático** o **Asignación de IP por MAC**.
> 3. Vincula la dirección MAC de tu ESP32 con la IP que hayas escrito en tus scripts (ej: `192.168.1.107`).
> 4. Dado que cada router es diferente, si tienes dudas busca en Google: *"Cómo asignar IP fija [modelo de tu router]"*.
---

  
## 🌐 Optimización de Rendimiento (Caché)
Para evitar que el ESP32 escane toda la tarjeta SD en cada inicio (lo cual es lento), el sistema utiliza un mecanismo de Firma de Validación:

El usuario selecciona las carpetas activas en la interfaz web.
El sistema crea una firma única en gif_cache.sig.
Si al reiniciar las carpetas seleccionadas no han cambiado, el ESP32 lee directamente las rutas desde gif_cache.txt de forma instantánea.


## 🛠️ Hoja de Ruta (Roadmap de Optimización)

Para las próximas versiones, el proyecto se centrará en dos niveles de mejora:

### ⚡ Nivel de Optimización (Rendimiento)
* **Búsqueda Binaria:** Implementación de algoritmo de búsqueda binaria sobre `batocera_cache.txt` y `gif_cache.txt`. Esto permitirá lanzamientos instantáneos incluso en colecciones con más de 10.000 juegos.
* **Streaming de SD a Web:** Refactorización del *FileManager* para listar archivos directamente desde la SD al navegador, eliminando por completo el uso de búfer de RAM intermedio.
* **💤 Gestión de Energía:** Implementación de modo *Sleep* y apagado programado para prolongar la vida útil de los paneles LED.

### 🎨 Nivel Estético (Visual)
* **Playlist Rotativa:** Cambiar la lógica de "un solo GIF" por una "lista de reproducción" que cambie de GIF cada cierto tiempo mientras el juego está activo.
* **Variantes Aleatorias:** Soporte para múltiples GIFs por juego (ej: `sonic_1.gif`, `sonic_2.gif`) para añadir dinamismo visual al panel.
* **🕔 Mejoras en la función Reloj:** Distintos diseños de reloj a elegir desde la WEB.
* **📡 Mejoras en la función Text:** Distintos tamaños de letra...
* **✍️ Control por Infrarrojos (IR):** Soporte para mandos a distancia para control físico (Encendido/Brillo/Modos).


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
* Grupo Telgram DMDos
