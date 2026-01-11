#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include <Update.h>       // Librería para la funcionalidad OTA
#include <DNSServer.h>    // Requerido por WiFiManager
#include <WiFiManager.h>  // Librería para gestión WiFi
#include <PubSubClient.h> // Librería para MQTT Integración en Home Assistant

// --- LIBRERÍAS DE HARDWARE ---
#include "SD.h"           // Gestión de la Micro SD
#include "AnimatedGIF.h"  // Decodificador de GIFs
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // Gestión del panel LED

// ====================================================================
//                          CONSTANTES & FIRMWARE
// ====================================================================
#define FIRMWARE_VERSION "2.1.9" // MODIFICADO: Nueva versión del firmware se implementa Integración con Batocera
#define PREF_NAMESPACE "pixel_config"
#define DEVICE_NAME_DEFAULT "RetroPixel-Default"
#define TZ_STRING_SPAIN "CET-1CEST,M3.5.0,M10.5.0/3" // Cadena TZ por defecto segura
#define GIFS_BASE_PATH "/gifs" // Directorio base para los GIFs
#define GIF_CACHE_FILE "/gif_cache.txt" // Archivo para guardar el índice de GIFs
#define GIF_CACHE_SIG "/gif_cache.sig" // Nuevo archivo de firma
#define M5STACK_SD SD

// --- NUEVA DEFINICIÓN DE PINES HUB75 ---
// Estos pines usan una combinación segura fuera del bus SD/Flash.
#define CLK_PIN       16
#define OE_PIN        15
#define LAT_PIN       4
#define A_PIN         33
#define B_PIN         32
#define C_PIN         22
#define D_PIN         17
#define E_PIN         -1 // Desactivado para paneles 64x32 (escaneo 1/16)
#define R1_PIN        25
#define G1_PIN        26
#define B1_PIN        27
#define R2_PIN        14
#define G2_PIN        12
#define B2_PIN        13

// --- NUEVA DEFINICIÓN DE PINES SPI SD (Nativos VSPI) ---
// Estos pines usan el bus VSPI de hardware para mayor velocidad.
#define SD_CS_PIN     5   
#define VSPI_MISO     19
#define VSPI_MOSI     23
#define VSPI_SCLK     18

// Definiciones de panel
const int PANEL_RES_X = 64; 
const int PANEL_RES_Y = 32;
#define MATRIX_HEIGHT PANEL_RES_Y 

// Variables globales para el sistema
WebServer server(80);
Preferences preferences;
WiFiManager wm;
AnimatedGIF gif;
// Puntero a la Matriz 
MatrixPanel_I2S_DMA *display = nullptr; 


// Variables de estado y reproducción
bool sdMontada = false;
bool enModoGestion = false;
std::vector<String> archivosGIF; 
int currentGifIndex = 0; 
int x_offset = 0; // Offset para centrado GIF
int y_offset = 0;

// Variables de modos
int xPosMarquesina = 0;
unsigned long lastScrollTime = 0;
const char* ntpServer = "pool.ntp.org";

// Variable global para la ruta del GIF de Batocera
String juegoActual = "default";
String sistemaActual = "default";
String rutaGifArcade = "/batocera/default/_default.gif";

// --- GESTIÓN DE RUTA SD ---
String currentPath = "/"; // Ruta actual para el administrador de archivos
File fsUploadFile; // Variable global para la subida de archivos
File FSGifFile;    // Variable global para el manejo de archivos GIF
File currentFile; 
bool recargarGifsPendiente = false; // Indica si hay un escaneo de SD pendiente
SemaphoreHandle_t sdMutex; // Semáforo para proteger el acceso a la SD y el Bus SPI


// --- CONFIGURACIÓN MQTT ---
String chipID; // Aquí guardaremos el ID basado en la MAC

// Clientes
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Topics para Auto-Discovery
const char* discovery_topic_bright = "homeassistant/number/retropixel/brightness/config";
const char* discovery_topic_mode = "homeassistant/select/retropixel/mode/config";
const char* discovery_topic_text = "homeassistant/text/retropixel/message/config";

// Topics (Temas)
const char* topic_cmd_mode = "retropixel/cmd/mode";   // Para cambiar modo
const char* topic_cmd_bright = "retropixel/cmd/bright"; // Para el brillo
const char* topic_state = "retropixel/state";         // Para reportar estado a HA
const char* topic_cmd_power = "retropixel/cmd/power";  // Encender / Apagar el Switch
const char* topic_state_power = "retropixel/state/power";  // Para reportar estado del switch
const char* topic_cmd_text = "retropixel/cmd/text";   // Para recibir el nuevo mensaje
const char* topic_state_text = "retropixel/state/text"; // Para reportar el mensaje actual a HA


// ====================================================================
//                          ESTRUCTURA DE DATOS
// ====================================================================

struct Config {
    // 1. Controles de Reproducción
    bool powerState;
    int brightness = 150;
    int playMode = 0; // 0: GIFs, 1: Texto, 2: Reloj, 3: Arcade
    String slidingText = "Retro Pixel LED v" + String(FIRMWARE_VERSION) + " - IP: " + WiFi.localIP().toString();
    int textSpeed = 50;
    int gifRepeats = 1;
    bool randomMode = false;
    // activeFolders (vector) para uso en runtime, activeFolders_str para Preferences
    std::vector<String> activeFolders; 
    String activeFolders_str = "/GIFS";
    // 2. Configuración de Hora/Fecha
    String timeZone = TZ_STRING_SPAIN; 
    bool format24h = true;
    // 3. Configuración del Modo Reloj
    uint32_t clockColor = 0xFF0000;
    // Rojo (por defecto)
    bool showSeconds = true;
    bool showDate = false;
    // 4. Configuración del Modo Texto Deslizante
    uint32_t slidingTextColor = 0x00FF00;
    // Verde (por defecto)
    // 5. Configuración de Hardware/Sistema
    int panelChain = 2; 
    // "Número de Paneles LED (en Cadena)"
    char device_name[40] = {0};
    // 6. Configuración MQTT Home Assistant
    bool mqtt_enabled = false;
    char mqtt_name[40] = "Retro Pixel LED"; // Nombre amigable para HA
    char mqtt_host[40] = "192.168.1.100";
    int mqtt_port = 1883;
    char mqtt_user[40] = "";
    char mqtt_pass[40] = "";
};
Config config;

// Lista de todas las carpetas
std::vector<String> allFolders;


// ====================================================================
//            BUSCADOR DE RUTAS EN EL INDICE DE BATOCERA
// ====================================================================
String buscarEnCache(String sistema, String juego) {
    sistema.trim(); sistema.toLowerCase();
    juego.trim(); juego.toLowerCase();

    // --- NIVEL 1: BUSCAR EL JUEGO (00) ---
    Serial.printf(">> Nivel 1: Buscando JUEGO [%s] -> [%s]\n", sistema.c_str(), juego.c_str());
    String rutaEncontrada = ejecutarBusqueda(sistema, juego, "00");
    
    if (rutaEncontrada != "") {
        Serial.print(">> MATCH JUEGO: "); Serial.println(rutaEncontrada);
        // Si hay juego, buscamos si tiene variantes (_1, _2...) y devolvemos una
        return seleccionarVarianteAleatoria(rutaEncontrada);
    }

    // --- NIVEL 2: SI NO HAY JUEGO, BUSCAR LOGO DEL SISTEMA (01) ---
    Serial.printf(">> Nivel 2: Juego no encontrado. Buscando LOGO de [%s]\n", sistema.c_str());
    rutaEncontrada = ejecutarBusqueda(sistema, "default", "01"); // Reutilizamos la variable
    
    if (rutaEncontrada != "") {
        Serial.print(">> MATCH LOGO: "); Serial.println(rutaEncontrada);
        return rutaEncontrada;
    }

    // --- NIVEL 3: GIF POR DEFECTO TOTAL ---
    Serial.println(">> Nivel 3: Sin match en caché. Cargando default absoluto.");
    return "/batocera/default/_default.gif";
}

// Función interna para no repetir código de lectura de archivos
String ejecutarBusqueda(String sistema, String juego, String prefijo) {
    String resultado = "";
    if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(2000))) {
        File cache = SD.open("/batocera_cache.txt");
        if (cache) {
            while (cache.available()) {
                String linea = cache.readStringUntil('\n');
                linea.trim();
                
                // Optimizamos: si no empieza por el prefijo (00 o 01), saltamos
                if (!linea.startsWith(prefijo)) continue;

                int p1 = linea.indexOf('|');
                int p2 = linea.indexOf('|', p1 + 1);
                int p3 = linea.lastIndexOf('|');

                if (p1 != -1 && p2 != -1 && p3 != -1) {
                    String sistCache = linea.substring(p1 + 1, p2);
                    String juegoCache = linea.substring(p2 + 1, p3);
                    
                    sistCache.trim(); sistCache.toLowerCase();
                    juegoCache.trim(); juegoCache.toLowerCase();

                    if (sistCache == sistema && juegoCache == juego) {
                        resultado = linea.substring(p3 + 1);
                        resultado.trim();
                        break; 
                    }
                }
            }
            cache.close();
        }
        xSemaphoreGive(sdMutex);
    }
    return resultado;
}

String seleccionarVarianteAleatoria(String rutaOriginal) {
    // 1. Separamos la extensión .gif
    int punto = rutaOriginal.lastIndexOf('.');
    String base = rutaOriginal.substring(0, punto); // Ej: /batocera/neogeo/mslug
    String ext = rutaOriginal.substring(punto);     // Ej: .gif

    // 2. Contamos cuántas variantes existen
    int totalVariantes = 0;
    
    // Comprobamos mslug_1.gif, mslug_2.gif... hasta un máximo de 5
    for (int i = 1; i <= 5; i++) {
        String testPath = base + "_" + String(i) + ext;
        if (SD.exists(testPath)) {
            totalVariantes = i;
        } else {
            break; // Si no existe el _2, dejamos de buscar
        }
    }

    // 3. Si no hay variantes, devolvemos la original
    if (totalVariantes == 0) return rutaOriginal;

    // 4. Si hay variantes, elegimos una al azar (incluyendo la original como opción 0)
    int eleccion = random(0, totalVariantes + 1); // Random entre 0 y totalVariantes
    
    if (eleccion == 0) return rutaOriginal;
    
    String rutaElegida = base + "_" + String(eleccion) + ext;
    Serial.printf(">> Variante detectada! Elegida la numero %d: %s\n", eleccion, rutaElegida.c_str());
    return rutaElegida;
}

// Declaraciones de funciones
void handleRoot();
void handleSave();
void handleConfig();
void handleSaveConfig();
void handleFactoryReset();
void handleRestart();
void handleOTA();
void handleOTAUpload();
void notFound();
void syncMQTTState();

// NUEVAS DECLARACIONES PARA GESTIÓN DE ARCHIVOS
void handleFileManager();
void handleFileUpload();
void handleFileDelete();
String fileManagerPage();

String webPage();
String configPage();
void loadConfig();
void savePlaybackConfig();
void saveSystemConfig();
void initTime();

// Funciones de visualización y control de modos
void mostrarMensaje(const char* mensaje, uint16_t color);
void listarArchivosGif();
void ejecutarModoGif();
void ejecutarModoTexto();
void ejecutarModoReloj();
void scanFolders();
// ====================================================================
//                      MANEJO DE CONFIGURACIÓN (PREFERENCES)
// ====================================================================

void loadConfig() { 
    preferences.begin(PREF_NAMESPACE, true);
    config.powerState = preferences.getBool("powerState", true);
    config.brightness = preferences.getInt("brightness", 150);
    config.playMode = preferences.getInt("playMode", 1);
    config.slidingText = preferences.getString("slidingText", config.slidingText);
    config.textSpeed = preferences.getInt("textSpeed", 50);
    config.gifRepeats = preferences.getInt("gifRepeats", 1);
    config.randomMode = preferences.getBool("randomMode", false);
    config.mqtt_enabled = preferences.getBool("mqtt_en", false);
    String mName = preferences.getString("m_name", "Retro Pixel LED");
    strncpy(config.mqtt_name, mName.c_str(), sizeof(config.mqtt_name));
    String mHost = preferences.getString("m_host", "192.168.1.100");
    strncpy(config.mqtt_host, mHost.c_str(), sizeof(config.mqtt_host));
    config.mqtt_port = preferences.getInt("m_port", 1883);
    String mUser = preferences.getString("m_user", "");
    strncpy(config.mqtt_user, mUser.c_str(), sizeof(config.mqtt_user));
    String mPass = preferences.getString("m_pass", "");
    strncpy(config.mqtt_pass, mPass.c_str(), sizeof(config.mqtt_pass));
// 1. Cargar string serializado (compatibilidad)
    config.activeFolders_str = preferences.getString("activeFolders", "/GIFS");
// 2. Deserializar el string en el vector config.activeFolders
    config.activeFolders.clear();
    int start = 0;
    int end = config.activeFolders_str.indexOf(',');
    while (end != -1) {
        config.activeFolders.push_back(config.activeFolders_str.substring(start, end));
        start = end + 1;
        end = config.activeFolders_str.indexOf(',', start);
    }
    // Añadir el último elemento (o el único si no hay comas)
    if (start < config.activeFolders_str.length()) {
        config.activeFolders.push_back(config.activeFolders_str.substring(start));
    }
    
    // Carga de configuración de Sistema/Reloj
    config.timeZone = preferences.getString("timeZone", TZ_STRING_SPAIN);
    config.format24h = preferences.getBool("format24h", true);
    // Carga del color del reloj
    config.clockColor = preferences.getULong("clockColor", 0xFF0000);
    config.showSeconds = preferences.getBool("showSeconds", true);
    config.showDate = preferences.getBool("showDate", false);
    
    // Carga del color del texto (usando la clave corta 'slideColor' que resolvió la persistencia)
    config.slidingTextColor = preferences.getULong("slideColor", 0x00FF00);
    config.panelChain = preferences.getInt("panelChain", 2); // Usa el nuevo valor por defecto
    
    // device_name: Lectura especial para char array
    String nameStr = preferences.getString("deviceName", DEVICE_NAME_DEFAULT);
    strncpy(config.device_name, nameStr.c_str(), sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';

    preferences.end();
}

void savePlaybackConfig() { 
    preferences.begin(PREF_NAMESPACE, false);// modo escritura
    
    preferences.putInt("brightness", config.brightness);
    preferences.putInt("playMode", config.playMode);
    preferences.putString("slidingText", config.slidingText);
    preferences.putInt("textSpeed", config.textSpeed);
    preferences.putInt("gifRepeats", config.gifRepeats);
    preferences.putBool("randomMode", config.randomMode);
    
    // Guardar carpetas (serializando de vector a String)
    String foldersToSave;
    for (size_t i = 0; i < config.activeFolders.size(); ++i) {
        foldersToSave += config.activeFolders[i];
        if (i < config.activeFolders.size() - 1) {               
            foldersToSave += ",";
        }
    }
    config.activeFolders_str = foldersToSave;
    preferences.putString("activeFolders", config.activeFolders_str);
    
    preferences.end();
}

void saveSystemConfig() { 
    preferences.begin(PREF_NAMESPACE, false); // 'false' es para modo escritura
    
    preferences.putString("timeZone", config.timeZone);
    preferences.putBool("format24h", config.format24h);
    
    // Configuración del Modo Reloj
    preferences.putULong("clockColor", config.clockColor);
    preferences.putBool("showSeconds", config.showSeconds);
    preferences.putBool("showDate", config.showDate);
    // Color del texto deslizante (clave corta "slideColor")
    const char* newKey = "slideColor";
    preferences.putULong(newKey, config.slidingTextColor);
    // Configuración de Hardware/Sistema
    preferences.putInt("panelChain", config.panelChain);
    preferences.putString("deviceName", config.device_name);
    // Configuración MQTT
    preferences.putBool("mqtt_en", config.mqtt_enabled);
    preferences.putString("m_name", config.mqtt_name);
    preferences.putString("m_host", config.mqtt_host);
    preferences.putInt("m_port", config.mqtt_port);
    preferences.putString("m_user", config.mqtt_user);
    preferences.putString("m_pass", config.mqtt_pass);
    
    preferences.end();
// Confirma la escritura en la NVS
}

// ====================================================================
//                      FUNCIÓN CRÍTICA DE REINICIO
// ====================================================================

void handleFactoryReset() {
    preferences.begin(PREF_NAMESPACE, false);
    preferences.clear(); // Borra todas las configuraciones guardadas
    preferences.end();
    
    wm.resetSettings();
// Borra la configuración WiFi
    
    server.send(200, "text/html", "<h2>Restablecimiento Completo</h2><p>Todos los ajustes (incluida la conexión WiFi) han sido borrados. El dispositivo se reiniciará en 3 segundos e iniciará el Portal Cautivo.</p>");
    delay(3000);
    ESP.restart();
}


// ====================================================================
//                      FUNCIÓN CRÍTICA DE TIEMPO
// ====================================================================
void initTime() {
    const char* tz_to_use = TZ_STRING_SPAIN;
    if (!config.timeZone.isEmpty() && config.timeZone.length() >= 4) {
        tz_to_use = config.timeZone.c_str();
    } else {
        Serial.println("ADVERTENCIA: Zona horaria vacía/inválida. Usando valor por defecto seguro.");
    }
    
    configTzTime(tz_to_use, ntpServer); 
    
    Serial.printf("Configurando NTP con servidor: %s, Zona Horaria: %s\n", ntpServer, tz_to_use);
    time_t now = time(nullptr);
    int attempts = 0;
    while (now < 10000 && attempts < 10) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        attempts++;
    }
    Serial.println("");
}

// ====================================================================
//                      MANEJADORES HTTP Y WEB
// ====================================================================

// --- Utilidad para conversión de color HEX a uint32_t ---
uint32_t parseHexColor(String hex) {
    if (hex.startsWith("#")) hex.remove(0, 1);
// Usar 0x00FF00 (Verde) como valor por defecto si la longitud es incorrecta
    if (hex.length() != 6) return 0x00FF00;
    return strtoul(hex.c_str(), NULL, 16);
}

// --- Rutas del Servidor ---
void handleRoot() {
    enModoGestion = false; // Al entrar al inicio, liberamos el panel para que muestre GIFs
    scanFolders(GIFS_BASE_PATH); // Escaneamos las carpetas para asegurar que la UI tenga qué mostrar
    server.send(200, "text/html", webPage());
}

void handleConfig() { server.send(200, "text/html", configPage()); }

void handlePower() {
    // 1. Invertimos el estado actual
    config.powerState = !config.powerState;
    
    // 2. Guardamos en la memoria Flash (powerState se guarda en saveSystemConfig)
    saveSystemConfig();
    
    // 3. Avisamos a Home Assistant del nuevo estado
    syncMQTTState();
    
    // 4. Redirigimos de vuelta a la página principal
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "OK");
}

void handleSave() { 
    // 1. Recogida de argumentos en variables temporales para no corromper la config actual si hay crash
    int tempBrightness = server.hasArg("b") ? server.arg("b").toInt() : config.brightness;
    int tempPlayMode = server.hasArg("pm") ? server.arg("pm").toInt() : config.playMode;
    String tempSlidingText = server.hasArg("st") ? server.arg("st") : config.slidingText;
    int tempTextSpeed = server.hasArg("ts") ? server.arg("ts").toInt() : config.textSpeed;
    int tempGifRepeats = server.hasArg("r") ? server.arg("r").toInt() : config.gifRepeats;
    bool tempRandomMode = server.hasArg("m") ? (server.arg("m").toInt() == 1) : config.randomMode;

    // 2. Gestión de Carpetas Temporal
    std::vector<String> tempFolders;
    for(size_t i = 0; i < server.args(); ++i) {
        if (server.argName(i) == "f") {
            tempFolders.push_back(server.arg(i));
        }
    }

    // Protección contra lista vacía
    if (tempFolders.empty() && sdMontada) {
         tempFolders.push_back("/");
    }

    // 3. ACTUALIZAR CONFIGURACIÓN EN RAM (Sin guardar en Flash todavía)
    config.brightness = tempBrightness;
    config.playMode = tempPlayMode;
    config.slidingText = tempSlidingText;
    config.textSpeed = tempTextSpeed;
    config.gifRepeats = tempGifRepeats;
    config.randomMode = tempRandomMode;
    config.activeFolders = tempFolders;

    // Aplicar brillo visualmente de inmediato
    if (display) display->setBrightness8(config.brightness);

    // 4. GESTIÓN DEL ESCANEO (BANDERA)
    // No ejecutamos listarArchivosGif() aquí para evitar el timeout de la web.
    // La bandera se activará y el loop() hará el trabajo pesado.
    if (config.playMode == 0) {
        recargarGifsPendiente = true; 
        Serial.println(">> Escaneo de carpetas programado para el loop...");
    }

    // 5. GUARDAR EN FLASH
    // Ahora que hemos procesado todo, guardamos la configuración. 
    // Si el escaneo posterior en el loop() falla, al menos estos valores ya están en flash.
    savePlaybackConfig();

    // 6. RESPUESTA WEB
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Guardado");

    // 7. Sincronizar con Home Assistant si aplica
    syncMQTTState();
}

void handleSaveConfig() { 
    bool timeChanged = false;

    // 1. Configuración de Sistema
    if (server.hasArg("deviceName")) { 
        String nameStr = server.arg("deviceName");
        strncpy(config.device_name, nameStr.c_str(), sizeof(config.device_name) - 1);
        config.device_name[sizeof(config.device_name) - 1] = '\0';
    }
    
    if (server.hasArg("pc")) config.panelChain = server.arg("pc").toInt();

    // 2. Configuración de Tiempo
    if (server.hasArg("tz") && server.arg("tz") != config.timeZone) {
        config.timeZone = server.arg("tz");
        timeChanged = true;
    }
    config.format24h = (server.arg("f24h") == "1"); 
    config.showSeconds = (server.arg("ss") == "1");
    config.showDate = (server.arg("sd") == "1");

    // 3. Colores
    if (server.hasArg("cc")) config.clockColor = parseHexColor(server.arg("cc"));
    if (server.hasArg("stc")) config.slidingTextColor = parseHexColor(server.arg("stc"));

    // 4. Configuración MQTT
    config.mqtt_enabled = (server.arg("mqtt_en") == "1");
    if (server.hasArg("m_name")) strncpy(config.mqtt_name, server.arg("m_name").c_str(), sizeof(config.mqtt_name) - 1);
    if (server.hasArg("m_host")) strncpy(config.mqtt_host, server.arg("m_host").c_str(), sizeof(config.mqtt_host) - 1);
    if (server.hasArg("m_port")) config.mqtt_port = server.arg("m_port").toInt();
    if (server.hasArg("m_user")) strncpy(config.mqtt_user, server.arg("m_user").c_str(), sizeof(config.mqtt_user) - 1);
    if (server.hasArg("m_pass")) strncpy(config.mqtt_pass, server.arg("m_pass").c_str(), sizeof(config.mqtt_pass) - 1);
    if (server.hasArg("st")) { 
        config.slidingText = server.arg("st"); 
        xPosMarquesina = display->width();             // Reiniciamos la animación
    }

    // 5. Guardar todo en la Flash
    saveSystemConfig();
    
    // 6. Aplicar cambios inmediatos y sincronizar MQTT
    if (timeChanged) {
        initTime();
    }

    // Si MQTT está activo y conectado, enviamos el nuevo texto a HA antes de reiniciar/desconectar
    if (config.mqtt_enabled && mqttClient.connected()) {
        String technicalID = "retropixel_" + chipID;
        // Publicamos el texto específico para que el cuadro de HA se actualice
        mqttClient.publish(("retropixel/" + technicalID + "/state/text").c_str(), config.slidingText.c_str(), true);
        
        // Sincronizamos el resto de estados (brillo, modo, etc.)
        syncMQTTState();
        delay(100); 

        // Solo desconectamos si han cambiado parámetros críticos de conexión (Host, Port, User)
        // Si solo ha cambiado el texto, no hace falta desconectar.
        // Pero si prefieres resetear la conexión por seguridad, dejamos tu código:
        mqttClient.disconnect(); 
    }

    // 7. Respuesta al navegador
    server.sendHeader("Location", "/config");
    server.send(302, "text/plain", "Configuración Guardada...");
}


// ====================================================================
//              ESTILO CSS CYBERPUNK UNIFICADO
// ====================================================================
String getStyle() {
    return "<style>"
    "* { box-sizing: border-box; }" 
    "body { font-family: 'Inter', -apple-system, sans-serif; background: radial-gradient(circle at top, #1a1c2c 0%, #0d0e14 100%); color: #fff; margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; min-height: 100vh; }"
    ".c { max-width: 500px; width: 100%; }"
    "h1 { text-align: center; font-size: 26px; font-weight: 800; background: linear-gradient(to right, #00f2ff, #0062ff); -webkit-background-clip: text; -webkit-text-fill-color: transparent; letter-spacing: -1px; margin-bottom: 30px; text-transform: uppercase; }"
    "h2 { font-size: 15px; color: #00f2ff; border-bottom: 1px solid rgba(0, 242, 255, 0.3); padding-bottom: 8px; margin-top: 10px; margin-bottom: 20px; text-transform: uppercase; letter-spacing: 1px; }"
    ".card { background: rgba(255, 255, 255, 0.03); backdrop-filter: blur(12px); border: 1px solid rgba(255, 255, 255, 0.1); padding: 20px; border-radius: 20px; margin-bottom: 20px; box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.5); }"
    
    /* --- SLIDER BRILLO AZUL NEÓN --- */
    "input[type=range] { width: 100%; -webkit-appearance: none; background: transparent; margin: 15px 0; }"
    "input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 6px; background: rgba(255,255,255,0.1); border-radius: 3px; }"
    "input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; height: 22px; width: 22px; border-radius: 50%; background: #00f2ff; margin-top: -8px; box-shadow: 0 0 15px #00f2ff, 0 0 5px #fff; border: 2px solid #fff; cursor: pointer; }"

    /* INPUTS Y SELECTS */
    "input:not([type='checkbox']):not([type='color']), select { width: 100%; padding: 12px; border-radius: 10px; background: rgba(0,0,0,0.4); border: 1px solid rgba(255,255,255,0.1); color: #fff; font-size: 15px; margin-top: 5px; }"
    "label { display: block; margin-top: 15px; font-size: 11px; color: #00f2ff; font-weight: bold; text-transform: uppercase; letter-spacing: 0.5px; }"
    
    /* --- ESTILO CHECKBOXES --- */
    ".cb label { font-weight: normal; text-transform: none; color: #ccc; display: flex; align-items: center; gap: 10px; margin: 10px 0; font-size: 13px; cursor: pointer; }"
    ".cb input[type='checkbox'] { width: 18px; height: 18px; accent-color: #00f2ff; cursor: pointer; }"

    /* --- DISEÑO DE COLORES --- */
    ".dual-neon { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-top: 15px; }"
    ".color-card { background: rgba(255,255,255,0.02); border: 1px solid rgba(255,255,255,0.05); padding: 15px; border-radius: 15px; text-align: center; transition: all 0.3s; }"
    ".color-card:hover { border-color: rgba(0, 242, 255, 0.5); background: rgba(0, 242, 255, 0.05); }"
    
    "input[type='color'] { -webkit-appearance: none; border: 2px solid rgba(255,255,255,0.1); width: 60px; height: 60px; border-radius: 12px; background: none; cursor: pointer; transition: transform 0.2s; }"
    "input[type='color']::-webkit-color-swatch-wrapper { padding: 4px; }"
    "input[type='color']::-webkit-color-swatch { border-radius: 8px; border: none; }"

    ".info-box { background: rgba(0, 242, 255, 0.05); border-left: 3px solid #00f2ff; padding: 12px; margin-top: 15px; font-size: 11px; color: #aaa; line-height: 1.4; border-radius: 0 8px 8px 0; }"
    ".grid-2, .dual-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 10px; }"
    ".grid-3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; margin-top: 10px; }"
    
    /* BOTONES NEÓN */
    ".btn { display: flex; align-items: center; justify-content: center; width: 100%; padding: 15px; border-radius: 12px; font-weight: bold; text-align: center; border: none; font-size: 10px; cursor: pointer; text-transform: uppercase; transition: 0.3s; text-decoration: none; min-height: 45px; }"
    ".save-btn { background: linear-gradient(45deg, #00b09b, #96c93d); color: #fff; box-shadow: 0 4px 15px rgba(0, 176, 155, 0.4); font-size: 13px; }"
    ".btn-ajustes, .btn-volver, .back-btn { background: linear-gradient(45deg, #f39c12, #ff512f); color: #fff; box-shadow: 0 4px 15px rgba(243, 156, 18, 0.4); }"
    ".btn-files { background: linear-gradient(45deg, #9b59b6, #da22ff); color: #fff; box-shadow: 0 4px 15px rgba(155, 89, 182, 0.4); }"
    ".btn-ota, .btn-reset, .restart-btn { background: linear-gradient(45deg, #3498db, #0575E6); color: #fff; box-shadow: 0 4px 15px rgba(52, 152, 219, 0.4); }"
    ".reset-btn { background: linear-gradient(45deg, #e74c3c, #8e0e00); color: #fff; box-shadow: 0 4px 15px rgba(231, 76, 60, 0.4); margin-top: 20px; }"

    ".footer { display: flex; justify-content: space-between; font-size: 10px; color: #444; margin-top: 20px; border-top: 1px solid #222; padding-top: 10px; width:100%; }"
    "</style>";
}

// ====================================================================
//                  INTERFAZ WEB PRINCIPAL
// ====================================================================
String webPage() {
    int brightnessPercent = (int)(((float)config.brightness / 255.0) * 100.0); 
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Retro Pixel LED</title>" + getStyle() + "</head><body><div class='c'>";
    html += "<h1>Retro Pixel <span style='font-weight:200'>LED</span></h1>";

    // ESTADO DEL PANEL
    html += "<div class='card'><h3>Estado del Panel</h3>";
    if (config.powerState) 
        html += "<a href='/power' class='btn' style='background:rgba(211,47,47,0.1); border:1px solid #ff2e63; color:#ff2e63; box-shadow: 0 0 15px rgba(255,46,99,0.2);'>APAGAR MATRIZ LED</a>";
    else 
        html += "<a href='/power' class='btn' style='background:rgba(56,142,60,0.1); border:1px solid #08d9d6; color:#08d9d6; box-shadow: 0 0 15px rgba(8,217,214,0.2);'>ENCENDER MATRIZ LED</a>";
    html += "</div><form action='/save' method='POST'>";
    
    // BRILLO
    html += "<div class='card'><h3>Brillo <span id='brightnessValue' style='color:#00f2ff; float:right;'>" + String(brightnessPercent) + "%</span></h3>";
    html += "<input type='range' name='b' min='0' max='255' value='" + String(config.brightness) + "' oninput='updateBrightness(this.value)'></div>";

    // MODO
    html += "<div class='card'><h3>Modo de Reproducción</h3>";
    html += "<select name='pm' id='playModeSelect'>";
    html += String("<option value='0'") + (config.playMode == 0 ? " selected" : "") + ">📁 Galería de GIFs</option>";
    html += String("<option value='1'") + (config.playMode == 1 ? " selected" : "") + ">📝 Texto Deslizante</option>";
    html += String("<option value='2'") + (config.playMode == 2 ? " selected" : "") + ">🕒 Reloj Digital</option>";
    html += String("<option value='3'") + (config.playMode == 3 ? " selected" : "") + ">🕹️ Arcade</option></select></div>";

    // CONFIGURACIÓN TEXTO
    html += "<div id='textConfig' class='card' style='display:" + String(config.playMode == 1 ? "block" : "none") + ";'>";
    html += "<h3>Configuración Texto</h3><label>Mensaje</label><input type='text' name='st' value='" + config.slidingText + "' maxlength='100'>";
    html += "<label>Velocidad (ms)</label><input type='number' name='ts' min='10' max='1000' value='" + String(config.textSpeed) + "'></div>";

    // CONFIGURACIÓN GIF
    html += "<div id='gifConfig' style='display:" + String(config.playMode == 0 ? "block" : "none") + ";'>";
    html += "<div class='card'><h3>Ajustes de Galería</h3>";
    html += "<label>Repeticiones</label><input type='number' name='r' min='1' max='50' value='" + String(config.gifRepeats) + "'>";
    html += "<label>Orden</label><select name='m'><option value='0'" + String(config.randomMode ? "" : " selected") + ">Secuencial</option><option value='1'" + String(config.randomMode ? " selected" : "") + ">Aleatorio</option></select>";
    
    // SECCIÓN DE CARPETAS
    html += "<label>Carpetas Activas</label><div class='cb'>";
    if (sdMontada) {
        if (allFolders.empty()) {
            html += "<p style='color:#888; font-size:12px;'>No hay carpetas en /gifs</p>";
        } else {
            for (const String& f : allFolders) {
                bool isChecked = (std::find(config.activeFolders.begin(), config.activeFolders.end(), f) != config.activeFolders.end());
                html += "<label><input type='checkbox' name='f' value='" + f + "'" + (isChecked ? " checked" : "") + "> " + f + "</label>";
            }
        }
    } else {
        html += "<p style='color:#ff2e63; font-size:12px;'>SD NO MONTADA</p>";
    }
    html += "</div></div></div>"; // Cierra el div 'cb' y las cards de GIF

    // BOTONES DE ACCIÓN
    html += "<button type='submit' class='btn save-btn'>GUARDAR CAMBIOS</button>";
    html += "<div class='grid-3'>";
    html += "<a href='/config' class='btn btn-ajustes'>AJUSTES</a>";
    //html += "<a href='/file_manager' class='btn btn-files'>FILES</a>";
    html += "<a href='/file_manager' class='btn btn-files' onclick='return confirm(\"Se detendrá la reproducción de GIFs para gestionar la SD de forma segura. ¿Continuar?\")'>FILES</a>";
    html += "<a href='/ota' class='btn btn-ota'>OTA</a>";
    html += "</div></form>";

    html += "<div class='footer'><span>v" + String(FIRMWARE_VERSION) + " - fjgordillo86</span><span>IP: " + WiFi.localIP().toString() + "</span></div>";

    html += "<script>"
            "function updateBrightness(v){ document.getElementById('brightnessValue').innerHTML=Math.round((v/255)*100)+'%'; }"
            "document.getElementById('playModeSelect').onchange=function(){"
            "  var m = this.value;"
            "  document.getElementById('gifConfig').style.display = (m=='0')?'block':'none';"
            "  document.getElementById('textConfig').style.display = (m=='1')?'block':'none';"
            "};"
            "</script></div></body></html>";
    return html;
}

// ====================================================================
//                  INTERFAZ WEB CONFIGURACIÓN
// ====================================================================
String configPage() {
    char hexColor[8]; sprintf(hexColor, "#%06X", config.clockColor);
    char hexTextColor[8]; sprintf(hexTextColor, "#%06X", config.slidingTextColor); 

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Config</title>" + getStyle() + "</head><body><div class='c'>";
    html += "<h1>Configuración</h1><form action='/save_config'>";

    // 1. HORA Y FECHA
    html += "<div class='card'><h2>1. Hora y Fecha</h2>";
    html += "<label>Zona Horaria (TZ String)</label><input type='text' name='tz' value='" + config.timeZone + "'>";
    html += "<div class='info-box'>Valor por defecto para España: <code>" + String(TZ_STRING_SPAIN) + "</code>. La gestión del horario de verano/invierno (DST) se hace automáticamente.</div>";
    
    html += "<label>Formato de Hora</label><select name='f24h'><option value='1'" + String(config.format24h ? " selected" : "") + ">24 Horas (HH:MM)</option><option value='0'" + String(!config.format24h ? " selected" : "") + ">12 Horas (AM/PM)</option></select>";
    
    // Checkboxes
    html += "<div class='cb' style='margin-top:20px;'>";
    html += "<label><input type='checkbox' name='ss' value='1'" + String(config.showSeconds ? " checked" : "") + "> SEGUNDOS</label>";
    html += "<label><input type='checkbox' name='sd' value='1'" + String(config.showDate ? " checked" : "") + "> MOSTRAR FECHA</label>";
    html += "</div></div>";

    // 2. ESTÉTICA 
    html += "<div class='card'><h2>2. Colores</h2>";
    html += "<div class='dual-neon'>";
    html += "  <div class='color-card'><label style='margin-top:0;'>RELOJ</label><input type='color' name='cc' value='" + String(hexColor) + "'></div>";
    html += "  <div class='color-card'><label style='margin-top:0;'>TEXTO</label><input type='color' name='stc' value='" + String(hexTextColor) + "'></div>";
    html += "</div></div>";

    // 3. HARDWARE
    html += "<div class='card'><h2>3. Hardware</h2><label>Número de Paneles LED</label><input type='number' name='pc' min='1' max='8' value='" + String(config.panelChain) + "'></div>";

    // 4. MQTT
    html += "<div class='card'><h2>4. Home Assistant (MQTT)</h2>";
    html += "<div class='cb'><label><input type='checkbox' id='mqtt_en' name='mqtt_en' value='1' onchange='toggleMQTT(this.checked)'" + String(config.mqtt_enabled ? " checked" : "") + "> Activar Integración</label></div>";
    
    html += "<div id='mqtt_fields' style='display:" + String(config.mqtt_enabled ? "block" : "none") + "; margin-top:10px;'>";
    html += "<label>Nombre Dispositivo</label><input type='text' name='m_name' value='" + String(config.mqtt_name) + "'>";
    html += "<div class='dual-grid'><div><label>Broker IP</label><input type='text' name='m_host' value='" + String(config.mqtt_host) + "'></div><div><label>Puerto</label><input type='number' name='m_port' value='" + String(config.mqtt_port) + "'></div></div>";
    html += "<label>Usuario</label><input type='text' name='m_user' value='" + String(config.mqtt_user) + "'>";
    html += "<label>Contraseña</label><input type='password' name='m_pass' value='" + String(config.mqtt_pass) + "'>";
    
    html += "<div class='info-box'>Nota: Al guardar se reiniciará la conexión MQTT.</div></div></div>";

    // FILA DE 3 BOTONES
    html += "<div class='dual-grid' style='grid-template-columns: repeat(3, 1fr); margin-bottom: 20px;'>";
    html += "<button type='submit' class='btn save-btn'>GUARDAR</button>";
    html += "<button type='button' class='btn restart-btn' onclick=\"if(confirm('¿Reiniciar?')) location.href='/restart';\">RESET</button>";
    html += "<a href='/' class='btn back-btn'>VOLVER</a>";
    html += "</div></form>";

    // BOTÓN FÁBRICA
    html += "<button type='button' class='btn reset-btn' onclick=\"if(confirm('¿BORRAR TODO?')) location.href='/factory_reset';\">RESTABLECER FÁBRICA</button>";

    html += "<div class='footer'><span>v" + String(FIRMWARE_VERSION) + " - fjgordillo86</span><span>IP: " + WiFi.localIP().toString() + "</span></div>";

    html += "<script>function toggleMQTT(s){ document.getElementById('mqtt_fields').style.display=s?'block':'none'; }</script></div></body></html>";
    
    return html;
}

// ====================================================================
//                  INTERFAZ WEB OTA
// ====================================================================
void handleOTA() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += getStyle();
    html += "</head><body><div class='c'>";
    html += "<h1>ACTUALIZACIÓN <span style='color:#3498db'>OTA</span></h1>";
  
    html += "<div class='card'>";
    html += "<h3>Firmware</h3>";
    html += "<form method='POST' action='/update' enctype='multipart/form-data' id='upload_form'>";
  
    // Input de archivo con estilo mejorado
    html += "<div style='border: 2px dashed rgba(52, 152, 219, 0.5); padding: 20px; border-radius: 12px; margin-bottom: 20px; text-align: center;'>";
    html += "<input type='file' name='update' style='font-size: 12px; color: #ccc;'>";
    html += "</div>";
  
    // Botón SUBIR
    html += "<button type='submit' class='btn btn-ota'>SUBIR Y ACTUALIZAR</button>";
    html += "</form></div>";

    // Botón VOLVER 
    html += "<a href='/' class='btn btn-ajustes' style='margin-top: 10px;'>VOLVER AL PANEL</a>";

    html += "<div class='footer'>";
    html += "<span>v" + String(FIRMWARE_VERSION) + " - fjgordillo86</span>"; 
    html += "<span>IP: " + WiFi.localIP().toString() + "</span>";
    html += "</div>";

   html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleOTAUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            String s = "<!DOCTYPE html><html><head>" + getStyle() + "</head><body><div class='c'><div class='card'><h2>¡ÉXITO!</h2><p>Reiniciando...</p></div><script>setTimeout(function(){location.href='/';},10000);</script></div></body></html>";
            server.send(200, "text/html", s);
            delay(500); ESP.restart();
        } else server.send(500, "text/plain", "Error OTA");
    }
}

void notFound() { server.send(404, "text/plain", "Not Found"); }

void handleRestart() { 
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    // La etiqueta meta es un respaldo físico por si el JS falla
    html += "<meta http-equiv='refresh' content='10;url=/'>"; 
    html += getStyle(); 
    html += "</head><body><div class='c'>";
    
    html += "<div class='card' style='text-align:center;'>";
    html += "<h2 style='color:#00f2ff;'>REINICIANDO...</h2>";
    html += "<p style='color:#eee;'>El sistema se está reiniciando para aplicar los cambios.</p>";
    html += "<div class='info-box' style='border-color:#00f2ff;'>Espere unos segundos. Será redirigido al panel automáticamente.</div>";
    
    // Animación de carga
    html += "<div style='margin: 20px auto; width: 40px; height: 40px; border: 4px solid rgba(0, 242, 255, 0.1); border-top: 4px solid #00f2ff; border-radius: 50%; animation: spin 1s linear infinite;'></div>";
    html += "<style>@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }</style>";
    
    html += "</div>";
    
    // Aumentamos a 10 segundos para dar tiempo real al ESP32 a reconectar al WiFi
    html += "<script>setTimeout(function(){ window.location.href='/'; }, 10000);</script>";
    
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
    
    // CRUCIAL: Aumentamos el delay a 2 segundos. 
    // 500ms a veces es poco para que el buffer del chip envíe todo el HTML antes de morir.
    Serial.println("Reiniciando ESP32...");
    delay(2000); 
    ESP.restart();
}

// ====================================================================
//                    MANEJADORES DE GESTIÓN DE ARCHIVOS
// ====================================================================

// --- MANEJO DE SUBIDA DE ARCHIVOS (Optimizado para Multi-Upload) --- 

void handleFileUpload(){ 
    if (!sdMontada) { 
        server.send(500, "text/plain", "Error: SD no montada.");
        return; 
    } 
    
    HTTPUpload& upload = server.upload(); 
    
    // 1. INICIO de la subida del archivo (UPLOAD_FILE_START)
    if (upload.status == UPLOAD_FILE_START) { 
        // La ruta de destino es la ruta actual (currentPath) + el nombre del archivo
        String uploadPath = currentPath + upload.filename;
        
        Serial.printf("Inicio de la subida a: %s\n", uploadPath.c_str()); 
        
        // Abrir el archivo de la SD para escribir (sobrescribe si existe)
        // La variable global fsUploadFile se usará para escribir el contenido.
        fsUploadFile = SD.open(uploadPath.c_str(), FILE_WRITE);

        if (!fsUploadFile) {
            Serial.printf("ERROR: No se pudo abrir el archivo %s para escribir.\n", uploadPath.c_str());
        }
        
    // 2. ESCRITURA de datos (UPLOAD_FILE_WRITE)
    } else if (upload.status == UPLOAD_FILE_WRITE) { 
        if(fsUploadFile) { 
            fsUploadFile.write(upload.buf, upload.currentSize);
        } 
        
    // 3. FIN de la subida del archivo (UPLOAD_FILE_END)
    } else if (upload.status == UPLOAD_FILE_END) { 
        if (fsUploadFile) { 
            fsUploadFile.close(); 
            Serial.printf("Subida finalizada. Nombre: %s | Tamaño: %d bytes\n", upload.filename.c_str(), upload.totalSize);
            
            // Solo refrescar la lista de GIFs si el archivo subido es un GIF.
            String filename = upload.filename;
            if (filename.endsWith(".gif") || filename.endsWith(".GIF")) {
                 listarArchivosGif(); 
            }
        } else {
            Serial.printf("Error en la subida del archivo %s: Falló la escritura o la apertura.\n", upload.filename.c_str());
        }
    } 
}

// --- MANEJO DE BORRADO DE ARCHIVOS Y CARPETAS ---

void handleFileDelete() {
    if (!sdMontada) {
        server.send(500, "text/plain", "Error: SD no montada.");
        return;
    }

    // 1. BLOQUEO: Detenemos el panel LED para tener acceso exclusivo a la SD
    enModoGestion = true; 

    if (server.hasArg("name") && server.hasArg("type")) {
        String filename = server.arg("name");
        String filetype = server.arg("type");
        
        // Aseguramos que currentPath termine en '/' antes de sumar el nombre
        String tempPath = currentPath;
        if (!tempPath.endsWith("/")) tempPath += "/";
        String fullPath = tempPath + filename;
        
        if (filetype == "dir") {
            // Nota: rmdir solo funciona si la carpeta está vacía
            if (SD.rmdir(fullPath.c_str())) { 
                Serial.printf("Directorio borrado: %s\n", fullPath.c_str());
            } else {
                Serial.printf("ERROR: No se pudo borrar el directorio (¿Está vacío?): %s\n", fullPath.c_str());
            }
        } else {
            if (SD.remove(fullPath.c_str())) {
                Serial.printf("Archivo borrado: %s\n", fullPath.c_str());
            } else {
                Serial.printf("ERROR: No se pudo borrar el archivo: %s\n", fullPath.c_str());
            }
        }
    }

    // 2. PAUSA: Pequeño respiro para que la SD actualice su tabla de archivos
    delay(100);

    // 3. REDIRECCIÓN: Forzamos al navegador a recargar la lista de archivos limpia
    server.sendHeader("Location", "/file_manager?path=" + currentPath);
    server.send(303); 
}

// --- MANEJO DE CREACIÓN DE CARPETAS ---

void handleCreateDir() {
    enModoGestion = true; // <--- 1. BLOQUEO DE SEGURIDAD: Detener el panel LED inmediatamente

    if (server.hasArg("name")) {
        String newDirName = server.arg("name");
        
        // Limpiar el nombre de la carpeta (quitar espacios o barras raras)
        newDirName.trim(); 
        
        // Construir la ruta usando currentPath (que ya debería venir limpia)
        String fullPath = currentPath;
        if (!fullPath.endsWith("/")) fullPath += "/";
        fullPath += newDirName;

        if (SD.mkdir(fullPath.c_str())) {
            Serial.printf("Carpeta creada con éxito: %s\n", fullPath.c_str());
        } else {
            Serial.printf("ERROR al crear: %s (¿SD llena o protegida?)\n", fullPath.c_str());
        }
    }

    // 2. PAUSA TÉCNICA: Damos 100ms para que la tabla de archivos de la SD se asiente
    delay(100); 

    // 3. REDIRECCIÓN LIMPIA: Usamos 303 (See Other) en lugar de 302 para forzar un GET fresco
    server.sendHeader("Location", "/file_manager?path=" + currentPath);
    server.send(303); 
}

// --- MANEJO DE ARCHIVOS (VISUALIZACIÓN Y NAVEGACIÓN) ---

void handleFileManager() {
    enModoGestion = true; // El panel a "FILES MODE"

    String requestedPath = server.hasArg("path") ? server.arg("path") : "/";
    
    // Limpieza de ruta
    if (!requestedPath.startsWith("/")) requestedPath = "/" + requestedPath;
    
    // Al acceder aquí, limpiamos la caché de archivos de la SD
    // para que la siguiente lectura sea real.
    currentPath = requestedPath;
    
    // Enviamos respuesta con cabeceras que prohíben la caché del navegador
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    
    server.send(200, "text/html", fileManagerPage(currentPath));
}

String fileManagerPage(String path) {
    // 1. Verificación de seguridad inicial: ¿Está la SD montada? 
    if (!sdMontada) {
        return "<!DOCTYPE html><html><head><meta charset='UTF-8'>" + getStyle() + "</head><body>"
               "<div class='c'><div class='card'><h2>⚠️ SD No Detectada</h2>"
               "<div class='info-box'>Asegúrate de que la tarjeta SD esté insertada y reinicia el dispositivo.</div>"
               "<a href='/' class='btn back-btn'>VOLVER AL INICIO</a></div></div></body></html>";
    }

    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Gestor de Archivos</title>" + getStyle() + "</head><body><div class='c'>";
    html += "<h1>Gestor de <span style='font-weight:200'>Archivos</span></h1>";

    // --- CARD 1: SUBIDA MÚLTIPLE ---
    html += "<div class='card'><h2>Subir Archivos</h2>";
    html += "<div class='info-box'>Subiendo a: <code>" + path + "</code></div>";
    html += "<form method='POST' action='/upload' enctype='multipart/form-data' style='margin-top:15px;'>";
    html += "<input type='file' name='upload' id='file-input' multiple style='display:none;' onchange='document.getElementById(\"file-name\").innerHTML = this.files.length + \" archivo(s) seleccionados\"'>";
    html += "<label for='file-input' class='btn' style='background:rgba(0,242,255,0.05); border:1px dashed #00f2ff; color:#00f2ff; margin-bottom:10px;'>📂 SELECCIONAR GIFS</label>";
    html += "<div id='file-name' style='font-size:10px; text-align:center; margin-bottom:15px; color:#666; font-family:monospace;'>Ningún archivo seleccionado</div>";
    html += "<input type='hidden' name='dir' value='" + path + "'>";
    html += "<button type='submit' class='btn save-btn'>INICIAR SUBIDA</button>";
    html += "</form></div>";

    // --- CARD 2: CREAR CARPETA ---
    html += "<div class='card'><h2>Nueva Carpeta</h2>";
    html += "<form action='/create_dir' method='GET' style='margin-top:10px;'>";
    html += "<input type='text' name='name' placeholder='Nombre de la carpeta' required style='margin-bottom:10px;'>";
    html += "<button type='submit' class='btn btn-ota'>CREAR DIRECTORIO</button>";
    html += "</form></div>";

    // --- CARD 3: EXPLORADOR ---
    html += "<div class='card'><h2>Explorador de SD</h2>";
    html += "<div style='background:rgba(0,0,0,0.3); border-radius:15px; overflow:hidden; border:1px solid rgba(255,255,255,0.05);'>";

    // Contenedor Flexbox para alinear botones en la misma línea
    html += "<div style='display:flex; justify-content:space-between; align-items:center; padding:10px 15px; border-bottom:1px solid rgba(255,255,255,0.1); background:rgba(255,255,255,0.02);'>";

    // 1. Botón de retroceso
    if (path != "/") {
    String parentPath = path.substring(0, path.lastIndexOf('/', path.length() - 2) + 1);
    if (parentPath == "") parentPath = "/";
    html += "<a href='/file_manager?path=" + parentPath + "' style='color:#00f2ff; text-decoration:none; font-size:11px; font-weight:bold; display:flex; align-items:center;'>⬅️ SUBIR NIVEL</a>";
    } else {
    // Espaciador vacío para mantener el botón de refrescar a la derecha si estamos en la raíz
    html += "<span></span>"; 
    }
    // 2. Botón de Refrescar
    // Usamos el mismo estilo de color pero con un toque verde neón para diferenciarlo
    html += "<a href='/file_manager?path=" + path + "' style='color:#2ecc71; text-decoration:none; font-size:11px; font-weight:bold; display:flex; align-items:center; gap:5px;'>REFRESCAR 🔄</a>";
    html += "</div>"; // Cierra el contenedor Flexbox

    // Apertura de directorio con pequeño reintento de seguridad
    File root = SD.open(path.c_str());
    if (!root && path != "/") { 
        vTaskDelay(pdMS_TO_TICKS(50));
        root = SD.open(path.c_str()); 
    }

    if (!root || !root.isDirectory()) {
        html += "<p style='padding:20px; text-align:center; color:#ff2e63;'>Error al leer directorio. <a href='/file_manager?path=" + path + "' style='color:#00f2ff;'>Reintentar</a></p>";
    } else {
        File file = root.openNextFile();
        int count = 0;
        while (file) {
            count++;
            String fileName = String(file.name());
            int lastSlash = fileName.lastIndexOf('/');
            if (lastSlash != -1) fileName = fileName.substring(lastSlash + 1);
            
            bool isDir = file.isDirectory();
            html += "<div style='display:flex; justify-content:space-between; align-items:center; padding:12px 15px; border-bottom:1px solid rgba(255,255,255,0.03);'>";
            
            if (isDir) {
                html += "<a href='/file_manager?path=" + path + fileName + "/' style='color:#f39c12; text-decoration:none; font-size:13px; display:flex; align-items:center; gap:8px;'>📁 " + fileName + "</a>";
                html += "<a href='/delete?name=" + fileName + "&type=dir' onclick='return confirm(\"¿Borrar carpeta?\")' style='color:#ff2e63; text-decoration:none; font-size:20px;'>&times;</a>";
            } else {
                html += "<div style='display:flex; flex-direction:column;'><span style='color:#eee; font-size:13px;'>📄 " + fileName + "</span><span style='color:#555; font-size:9px;'>" + String(file.size() / 1024) + " KB</span></div>";
                html += "<a href='/delete?name=" + fileName + "&type=file' onclick='return confirm(\"¿Eliminar permanentemente?\")' style='color:#ff2e63; text-decoration:none; font-size:20px;'>&times;</a>";
            }
            html += "</div>";
            
            file.close(); 
            file = root.openNextFile();
        }
        root.close();
        if (count == 0) html += "<p style='padding:30px; text-align:center; color:#444; font-size:12px;'>Esta carpeta está vacía</p>";
    }
    html += "</div></div>";

    // Botón para volver al inicio
    html += "<div style='text-align:center; margin-top:20px; margin-bottom:20px;'>";
    html += "<a href='/' class='btn back-btn' style='display:inline-block; width:auto; min-width:200px; max-width:90%;'>VOLVER AL PANEL PRINCIPAL</a>";
    html += "</div>";

    uint64_t totalBytes = SD.totalBytes();
    uint64_t freeBytes = totalBytes - SD.usedBytes();
    uint32_t totalMB = (uint32_t)(totalBytes / (1024 * 1024));
    uint32_t libreMB = (uint32_t)(freeBytes / (1024 * 1024));

    html += "<div class='footer'>";
    html += "<span>v" + String(FIRMWARE_VERSION) + " - fjgordillo86</span>"; 
    html += "<span style='color:#888;'>SD: <strong style='color:#00f2ff;'>" + String(libreMB) + " MB</strong> Libres / " + String(totalMB) + " MB</span>";
    html += "<span>IP: " + WiFi.localIP().toString() + "</span>";
    html += "</div>";

    html += "</div></body></html>";
    return html;
}

// ====================================================================
//                   FUNCIONES CORE DE VISUALIZACIÓN
// ====================================================================

// --- 1. Funciones Callback para AnimatedGIF ---

// Función de dibujo de la librería GIF (necesita estar definida)
void GIFDraw(GIFDRAW *pDraw)
{
    // Las variables deben estar declaradas en el ámbito global o como 'extern'
    extern int x_offset; 
    extern int y_offset; 
    extern MatrixPanel_I2S_DMA *display; 

    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;
    int iCount; // Variable para el conteo de píxeles opacos

    if (!display) return; 

    // BaseX: Punto de inicio del frame, incluyendo el offset de centrado
    int baseX = pDraw->iX + x_offset; 
    
    // Altura y ancho del frame
    iWidth = pDraw->iWidth;
    if (iWidth > 128) 
        iWidth = 128; 
        
    usPalette = pDraw->pPalette;
    
    // Y: Coordenada Y de inicio del dibujo, incluyendo el offset de centrado
    y = pDraw->iY + pDraw->y + y_offset; 

    s = pDraw->pPixels;

    // Lógica para frames con transparencia o método de descarte (Disposal)
    if (pDraw->ucHasTransparency) { 
        
        iCount = 0;
        
        for (x = 0; x < iWidth; x++) {
            if (s[x] == pDraw->ucTransparent) {
                if (iCount) { 
                    for(int xOffset_ = 0; xOffset_ < iCount; xOffset_++ ){
                        // 🛑 CORRECCIÓN: SUMAMOS 128 (Primera línea)
                        display->drawPixel(baseX + x - iCount + xOffset_ + 128, y, usTemp[xOffset_]); 
                    }
                    iCount = 0;
                }
            } else {
                usTemp[iCount++] = usPalette[s[x]];
            }
        }
        
        if (iCount) {
            for(int xOffset_ = 0; xOffset_ < iCount; xOffset_++ ){
                // 🛑 CORRECCIÓN: SUMAMOS 128 (Segunda línea)
                display->drawPixel(baseX + x - iCount + xOffset_ + 128, y, usTemp[xOffset_]); 
            }
        }

    } else { // No hay transparencia (dibujo simple de línea completa)
        s = pDraw->pPixels;
        for (x=0; x<iWidth; x++)
            // 🛑 CORRECCIÓN: SUMAMOS 128 (Tercera línea)
            display->drawPixel(baseX + x + 128, y, usPalette[*s++]); 
    }
} /* GIFDraw() */

// Funciones de gestión de archivo para SD
static void * GIFOpenFile(const char *fname, int32_t *pSize)
{
    // Usamos la variable global FSGifFile
    extern File FSGifFile; 
    FSGifFile = M5STACK_SD.open(fname);
    if (FSGifFile) {
        *pSize = FSGifFile.size();
        return (void *)&FSGifFile; // Devolver puntero a la variable global
    }
    return NULL;
}

static void GIFCloseFile(void *pHandle)
{
    File *f = static_cast<File *>(pHandle);
    if (f != NULL)
        f->close();
}

static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // El 'ugly work-around' de su ejemplo
    if ((pFile->iSize - pFile->iPos) < iLen)
        iBytesRead = pFile->iSize - pFile->iPos - 1; 
    if (iBytesRead <= 0)
        return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
}


static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    return pFile->iPos;
}


// --- 2. Funciones de Utilidad de Visualización ---

void mostrarMensaje(const char* mensaje, uint16_t color = 0xF800 /* Rojo */) {
    if (!display) return;
    display->fillScreen(0); // Negro
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(color);
    display->setCursor(0, MATRIX_HEIGHT / 2 - 4);
    display->print(mensaje);
    display->flipDMABuffer();
}

// ====================================================================
//                  FUNCIÓN DE ESCANEO DE CARPETAS PARA LA UI
// ====================================================================

// Función para escanear y listar solo las CARPETAS dentro de un path base
void scanFolders(String basePath) {
    allFolders.clear(); // Limpiamos la lista global de carpetas
    
    // 1. Aseguramos que el directorio base exista.
    if (!SD.exists(basePath)) {
        SD.mkdir(basePath); // Creamos la carpeta /gifs si no existe
        Serial.printf("Directorio base creado: %s\n", basePath.c_str());
        return; 
    }

    File root = SD.open(basePath);
    if (!root || !root.isDirectory()) {
        Serial.printf("Error: %s no es un directorio válido.\n", basePath.c_str());
        return;
    }
    
    Serial.printf("Escaneando subcarpetas dentro de: %s\n", basePath.c_str());
    
    File entry = root.openNextFile();
    while(entry){
        if(entry.isDirectory()){
            String dirName = entry.name();
            // Construimos la ruta completa (Ejemplo: /gifs/animals)
            String fullPath = basePath + "/" + dirName; 
            
            // Añadir a la lista que se muestra en la interfaz web
            allFolders.push_back(fullPath); 
        }
        entry = root.openNextFile();
    }
    root.close();
    
    // Ordenar la lista para mostrarla alfabéticamente en la web
    if (allFolders.size() > 0) {
        std::sort(allFolders.begin(), allFolders.end());
    }
}

// ====================================================================
//                 GESTIÓN DE ARCHIVOS GIF & CACHÉ
// ====================================================================

// Nueva función para leer la lista de GIFs desde el archivo de caché
bool loadGifCache() {
    archivosGIF.clear(); // Limpiamos la lista actual
    
    // Abrir el archivo de caché para lectura
    File cacheFile = SD.open(GIF_CACHE_FILE, FILE_READ);
    if (!cacheFile) {
        Serial.println("Cache de GIFs no encontrada. Forzando escaneo de SD.");
        return false; // El caché no existe o falló la lectura
    }

    Serial.println("Cargando lista de GIFs desde el caché...");
    
    // Leer cada línea del archivo (cada línea es una ruta de GIF)
    while (cacheFile.available()) {
        String line = cacheFile.readStringUntil('\n');
        line.trim(); // Eliminar espacios en blanco o retornos de carro
        if (line.length() > 0) {
            archivosGIF.push_back(line);
        }
    }
    
    cacheFile.close();
    
    if (archivosGIF.empty()) {
        Serial.println("Cache vacía. Forzando escaneo de SD.");
        return false;
    }
    
    Serial.printf("Cache cargada. %d GIFs encontrados.\n", archivosGIF.size());
    return true; // Cache cargada con éxito
}

// Nueva función para escribir la lista de GIFs al archivo de caché
void saveGifCache() {
    File cacheFile = SD.open(GIF_CACHE_FILE, FILE_WRITE);
    if (!cacheFile) {
        Serial.println("Error: No se pudo crear/abrir el archivo de caché.");
        return;
    }

    Serial.println("Guardando lista actual de GIFs en caché...");
    for (const String& path : archivosGIF) {
        cacheFile.println(path); // Escribimos cada ruta en una nueva línea
    }
    
    cacheFile.close();
    Serial.println("Caché de GIFs guardada correctamente.");
}

// Función recursiva que escanea una ruta en busca de GIFs
void scanGifDirectory(String path) {
    File root = SD.open(path);
    if (!root) {
        Serial.printf("Error al abrir directorio: %s\n", path.c_str());
        return;
    }
    
    File entry = root.openNextFile();
    while(entry){
        if(entry.isDirectory()){
            // Si es un directorio, lo ignoramos para la lista plana de archivos
            // Si en el futuro quiere escanear recursivamente, esta es la línea a cambiar.
        } else {
            // Es un archivo, verificar si es un GIF
            String fileName = entry.name();
            if (fileName.endsWith(".gif") || fileName.endsWith(".GIF")) {
                String fullPath = path + "/" + fileName; // Construimos la ruta completa
                // Limpiamos el doble slash si la ruta es la raíz (/)
                if (path == "/") { 
                    fullPath = fileName;
                }
                
                archivosGIF.push_back(fullPath); // Añadimos la ruta a la lista global
            }
        }
        entry = root.openNextFile();
    }
    root.close();
}


// Función auxiliar para generar la firma de la configuración actual
String generateCacheSignature() {
    String signature = "";
    // MODIFICADO: Usamos config.activeFolders que es el vector donde se guardan las carpetas seleccionadas
    for (const String& folder : config.activeFolders) { 
        signature += folder + ":"; // Concatenamos las rutas separadas por :
    }
    return signature;
}

// Función principal de listado de archivos GIF (usa la lógica de validación de firma)
void listarArchivosGif() {
    // 1. Generar la firma de la configuración actual de carpetas
    String currentSignature = generateCacheSignature();
    
    // Si no hay carpetas seleccionadas, la lista debe estar vacía
    if (currentSignature.length() == 0) { 
        archivosGIF.clear();
        Serial.println("No hay carpetas de GIF seleccionadas. Lista vacía.");
        return;
    }

    // 2. Intentar validar la caché leyendo el archivo de firma
    bool cacheIsValid = false; 
    File sigFile = SD.open(GIF_CACHE_SIG, FILE_READ);
    
    if (sigFile) {
        String savedSignature = sigFile.readStringUntil('\n');
        sigFile.close();
        savedSignature.trim();

        if (savedSignature == currentSignature) {
            cacheIsValid = true;
        } else {
            Serial.println("Firmas de caché no coinciden. La configuración ha cambiado.");
        }
    } else {
        Serial.println("Archivo de firma de caché no encontrado. Forzando escaneo.");
    }
    
    // 3. Cargar o Reconstruir la lista
    if (cacheIsValid && loadGifCache()) { 
        return; // ¡Caché válida y cargada! Salimos sin escanear.
    }

    // 4. Reconstrucción: Escaneo de SD y regeneración de archivos de caché
    Serial.println("Reconstruyendo lista de GIFs, escaneando SD...");
    archivosGIF.clear();

    // 4a. Escanear las carpetas seleccionadas
    for (const String& path : config.activeFolders) { 
        scanGifDirectory(path);
    }

    // 4b. Guardar el índice de GIFs
    saveGifCache(); 

    // 4c. Guardar la nueva firma para la próxima vez
    File newSigFile = SD.open(GIF_CACHE_SIG, FILE_WRITE);
    if (newSigFile) {
        newSigFile.print(currentSignature);
        newSigFile.close();
        Serial.println("Nueva firma de caché guardada.");
    } else {
         Serial.println("ERROR: No se pudo escribir el archivo de firma.");
    }
}

// --- 3. Funciones de Modos de Reproducción ---
// Variables globales necesarias para el loop
extern unsigned long lastFrameTime; // Declarar esta variable en el ámbito global si aún no existe
// extern WebServer server; // Asumiendo que el objeto server es global

void ejecutarModoGif() {
    if (!display) return; 
    
    // Verificación de la SD y lista de archivos
    if (!sdMontada || archivosGIF.empty()) {
        mostrarMensaje(!sdMontada ? "SD ERROR" : "NO GIFS");
        delay(200);
        return;
    }

    // Rotación de la lista de GIFs
    if (currentGifIndex >= archivosGIF.size()) {
        listarArchivosGif();
        currentGifIndex = 0;
        if (archivosGIF.empty()) return;
    }
    
    String gifPath = archivosGIF[currentGifIndex]; 
    
    // Bucle de repetición del GIF (config.gifRepeats)
    for (int rep = 0; rep < config.gifRepeats; ++rep) { 
        // SALIDA SI CAMBIA EL MODO O SE APAGA
        if (config.playMode != 0 || !config.powerState || enModoGestion) return; // <--- NUEVO
        
        if (gif.open(gifPath.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
            
            x_offset = (128 - gif.getCanvasWidth()) / 2; 
            y_offset = (32 - gif.getCanvasHeight()) / 2; 

            display->clearScreen(); 

            int delayMs;
            
            // Bucle principal de reproducción de frames
            while (gif.playFrame(true, &delayMs)) {
                
                // --- VERIFICACIÓN DE APAGADO / GESTIÓN ---
                if (!config.powerState || enModoGestion || config.playMode != 0) {
                    gif.close();
                    return; 
                }

                // Manejo de WebServer y espera no bloqueante
                server.handleClient(); 
                
                unsigned long targetTime = millis() + delayMs;
                while (millis() < targetTime) {
                    // Volvemos a chequear dentro de la espera del frame
                    if (!config.powerState || enModoGestion) {
                        gif.close();
                        return;
                    }
                    server.handleClient(); 
                    yield(); 
                }
            }
            gif.close();
            
        } else {
            Serial.printf("Error abriendo GIF: %s\n", gifPath.c_str());
            mostrarMensaje("Error GIF", display->color565(255, 255, 0));
            
            unsigned long start = millis();
            while (millis() - start < 1000) {
                 if (!config.powerState || enModoGestion) return;
                 server.handleClient();
                 yield();
            }
        }
    }

    currentGifIndex++; 
}

void ejecutarModoTexto() {
    if (!display) return; 

    // 1. Configuración de color (usando tu lógica de bits)
    uint16_t colorTexto = display->color565(
        (config.slidingTextColor >> 16) & 0xFF,
        (config.slidingTextColor >> 8) & 0xFF,
        config.slidingTextColor & 0xFF
    ); 

    display->setTextSize(1); 
    display->setTextWrap(false); 
    display->setTextColor(colorTexto); 

    // 2. Control de tiempo para el scroll
    if (millis() - lastScrollTime > config.textSpeed) {
        lastScrollTime = millis(); 
        xPosMarquesina--;

        // Calculamos el ancho real en píxeles (más preciso que multiplicar por 6)
        int16_t x1, y1;
        uint16_t w, h;
        display->getTextBounds(config.slidingText, 0, 0, &x1, &y1, &w, &h);

        // Si el texto terminó de pasar (su posición es menor que su ancho negativo)
        if (xPosMarquesina < -((int)w)) {
            xPosMarquesina = display->width(); 
        }

        // 3. Dibujado
        display->fillScreen(0); // Limpiar buffer
        // MATRIX_HEIGHT / 2 - 4 suele centrar bien la fuente estándar de 7-8px
        display->setCursor(xPosMarquesina, (MATRIX_HEIGHT / 2) - (h / 2));
        display->print(config.slidingText);
        display->flipDMABuffer(); 
    }
}


void ejecutarModoReloj() {
    if (!display) return; 
    struct tm timeinfo; 
    if(!getLocalTime(&timeinfo)){
        mostrarMensaje("NTP ERROR", display->color565(0, 0, 255));
        delay(100);
        return; 
    }
    
    uint16_t colorReloj = display->color565(
        (config.clockColor >> 16) & 0xFF,
        (config.clockColor >> 8) & 0xFF,
        config.clockColor & 0xFF
    ); 
    char timeFormat[10]; 
    if (config.format24h) {
        strcpy(timeFormat, config.showSeconds ? "%H:%M:%S" : "%H:%M"); 
    } else {
        // NOTA: EL FORMATO ORIGINAL DE SU CÓDIGO INCLUÍA %p (AM/PM), 
        // lo que ocupa más espacio. Si sigue habiendo cortes, elimine "%p"
        strcpy(timeFormat, config.showSeconds ? "%I:%M:%S %p" : "%I:%M %p"); 
    }

    char timeString[20]; 
    strftime(timeString, sizeof(timeString), timeFormat, &timeinfo); 
    
    char dateString[11]; 
    strftime(dateString, sizeof(dateString), "%d/%m/%Y", &timeinfo); 

    display->fillScreen(display->color565(0, 0, 0)); 
    display->setTextColor(colorReloj); 
    
    // --- CÁLCULO Y DIBUJO DE LA HORA (TAMAÑO 2) ---
    display->setTextSize(2); 
    
    // 1. Cálculo del centrado horizontal (X)
    int xHora = (display->width() - (strlen(timeString) * 12)) / 2;
    
    // 2. APLICACIÓN DE LA CORRECCIÓN: EMPUJAR 65px A LA IZQUIERDA
    xHora += 65; 
    
    // 3. Dibujo de la hora
    // Aseguramos que la posición X no sea negativa y la posición Y sea 8
    display->setCursor(xHora > 0 ? xHora : 0, 8); 
    display->print(timeString); 
    
    // --- DIBUJO DE LA FECHA (MARQUESINA - TAMAÑO 1) ---
    display->setTextSize(1); 
    
    // La variable lastScrollTime debe ser global
    extern unsigned long lastScrollTime; 
    
    if (config.showDate) {
        // xPosFecha debe ser una variable global para que la marquesina funcione
        extern int xPosMarquesina; 
        int anchoFecha = strlen(dateString) * 6;

        // Lógica de desplazamiento (usa config.textSpeed de la configuración)
        if (millis() - lastScrollTime > config.textSpeed) { 
            lastScrollTime = millis(); 
            xPosMarquesina--;
            if (xPosMarquesina < -anchoFecha) {
                xPosMarquesina = display->width(); 
            }
        }
        // El cursor de la fecha se ajusta a la posición de la marquesina
        display->setCursor(xPosMarquesina, MATRIX_HEIGHT - 8); 
        display->print(dateString);
    }
    
    display->flipDMABuffer(); 
    delay(50);
}

void ejecutarModoArcade() {
    // Si entramos aquí pero ya estamos en modo gestión, salimos antes de abrir nada
    if (enModoGestion) return;

    char pathChar[128];
    rutaGifArcade.toCharArray(pathChar, sizeof(pathChar));
    
    if (gif.open(pathChar, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
        // El bucle principal del GIF
        while (gif.playFrame(true, NULL)) {
            
            // --- ESTA ES LA CLAVE ---
            // Si el servidor activa enModoGestion para leer la caché,
            // cerramos el archivo y salimos de la función inmediatamente.
            if (enModoGestion || config.playMode != 3) { 
                gif.close(); 
                return; 
            }
            
            yield(); // Mantiene vivo el sistema
        }
        gif.close();
    } else {
        // Si falla, volvemos al default
        rutaGifArcade = "/batocera/default/_default.gif";
    }
}


// ====================================================================
//                                MQTT
// ====================================================================

void sendMQTTDiscovery() {
    mqttClient.setBufferSize(1024);

    // 1. DEFINIR LOS IDs (Esto es lo que te faltaba)
    // Usamos chipID (la MAC) para que sea único e invariable
    String technicalID = "retropixel_" + chipID; 
    
    // El nombre amigable que el usuario puso en la web
    String friendlyName = (String(config.mqtt_name).length() > 0) ? String(config.mqtt_name) : "Retro Pixel " + chipID;

    // 2. CREAR EL JSON DEL DISPOSITIVO
    String deviceJSON = ",\"dev\":{";
    deviceJSON += "\"ids\":[\"" + technicalID + "\"],";
    deviceJSON += "\"name\":\"" + friendlyName + "\",";
    deviceJSON += "\"sw\":\"" + String(FIRMWARE_VERSION) + "\",";
    deviceJSON += "\"mdl\":\"Retro Pixel LED\",";
    deviceJSON += "\"mf\":\"fjgordillo86\",";
    deviceJSON += "\"cu\":\"http://" + WiFi.localIP().toString() + "/\"";
    deviceJSON += "}";

    // 3. ENTIDADES (Usamos technicalID para que los tópicos sean únicos por panel)
    
    // --- SELECT DE MODOS (Actualizado con Arcade) ---
    String modeConfig = "{\"name\":\"Modo\",\"stat_t\":\"retropixel/" + technicalID + "/state/mode\",\"cmd_t\":\"retropixel/" + technicalID + "/cmd/mode\",\"options\":[\"GIFs\",\"Reloj\",\"Texto\",\"Arcade\"],\"uniq_id\":\"" + technicalID + "_mode\"" + deviceJSON + "}";
    mqttClient.publish(("homeassistant/select/" + technicalID + "/mode/config").c_str(), modeConfig.c_str(), true);

    // --- SWITCH POWER ---
    String powerConfig = "{\"name\":\"Estado\",\"stat_t\":\"retropixel/" + technicalID + "/state/power\",\"cmd_t\":\"retropixel/" + technicalID + "/cmd/power\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"uniq_id\":\"" + technicalID + "_power\"" + deviceJSON + "}";
    mqttClient.publish(("homeassistant/switch/" + technicalID + "/power/config").c_str(), powerConfig.c_str(), true);

    // --- BRILLO ---
    String brightConfig = "{\"name\":\"Brillo\",\"stat_t\":\"retropixel/" + technicalID + "/state/bright\",\"cmd_t\":\"retropixel/" + technicalID + "/cmd/bright\",\"min\":0,\"max\":255,\"uniq_id\":\"" + technicalID + "_bright\"" + deviceJSON + "}";
    mqttClient.publish(("homeassistant/number/" + technicalID + "/bright/config").c_str(), brightConfig.c_str(), true);

    // --- TEXTBOX PARA EL MODO TEXTO ---
    String textConfig = "{\"name\":\"Texto Pantalla\",\"stat_t\":\"retropixel/" + technicalID + "/state/text\",\"cmd_t\":\"retropixel/" + technicalID + "/cmd/text\",\"mode\":\"text\",\"min\":1,\"max\":100,\"uniq_id\":\"" + technicalID + "_text\"" + deviceJSON + "}";
    mqttClient.publish(("homeassistant/text/" + technicalID + "/text/config").c_str(), textConfig.c_str(), true);
    // Forzamos la actualización del estado con el valor que ya tiene el ESP32
    mqttClient.publish(("retropixel/" + technicalID + "/state/text").c_str(), config.slidingText.c_str(), true);

    Serial.print("Discovery enviado para: ");
    Serial.println(friendlyName);
}

void reconnectMQTT() {
    // Si el MQTT no está activado en la web, salimos inmediatamente
    if (!config.mqtt_enabled) return;

    int reintentos = 0;
    const int maxReintentos = 5;
    // IMPORTANTE: Asegúrate de que technicalID se construye igual que en el Discovery
    String technicalID = "retropixel_" + chipID;

    while (!mqttClient.connected() && reintentos < maxReintentos) {
        Serial.printf("Intentando conexión MQTT (Intento %d/%d)...\n", reintentos + 1, maxReintentos);
        
        // Configuramos el servidor con los datos actuales de la web
        mqttClient.setServer(config.mqtt_host, config.mqtt_port);

        // Intentamos conectar usando el technicalID como ClientID único
        if (mqttClient.connect(technicalID.c_str(), config.mqtt_user, config.mqtt_pass)) {
            Serial.println("¡Conectado a MQTT con éxito!");
            
            // 1. SUSCRIPCIÓN DINÁMICA CON COMODÍN (Solución al problema de recepción)
            // Escuchamos todo lo que venga de HA hacia nuestro ID
            mqttClient.subscribe(("retropixel/" + technicalID + "/cmd/#").c_str());
            Serial.println("Suscrito a: retropixel/" + technicalID + "/cmd/#");

            // 2. Enviamos el Discovery para que aparezca/se actualice en HA
            sendMQTTDiscovery();

            // 3. Publicar estados actuales para sincronizar la interfaz de HA
            String modoTexto = "GIFs";
            if (config.playMode == 1) modoTexto = "Texto";
            else if (config.playMode == 2) modoTexto = "Reloj";

            mqttClient.publish(("retropixel/" + technicalID + "/state/mode").c_str(), modoTexto.c_str(), true);
            mqttClient.publish(("retropixel/" + technicalID + "/state/bright").c_str(), String(config.brightness).c_str(), true);
            mqttClient.publish(("retropixel/" + technicalID + "/state/power").c_str(), config.powerState ? "ON" : "OFF", true);
            mqttClient.publish(("retropixel/" + technicalID + "/state/text").c_str(), config.slidingText.c_str(), true);

            reintentos = 0; // Reseteamos contador al conectar
        } else {
            Serial.print("Fallo conexión, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" reintentando en 5 segundos...");
            
            reintentos++;
            
            // Espera de 5 segundos sin bloquear el núcleo (Core 0)
            vTaskDelay(pdMS_TO_TICKS(5000)); 

            // Si durante la espera desactivas MQTT en la web, salimos
            if (!config.mqtt_enabled) return;
        }
    }

    if (reintentos >= maxReintentos) {
        Serial.println("MQTT: Máximo de reintentos alcanzado. Se reintentará en el próximo ciclo del loop.");
    }
}


void callback(char* topic, byte* payload, unsigned int length) {
    // 1. Convertir el mensaje recibido a String
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    String strTopic = String(topic);
    Serial.print("Mensaje recibido en [");
    Serial.print(strTopic);
    Serial.print("]: ");
    Serial.println(message);

    // Creamos el prefijo dinámico para responder el estado al tópico correcto
    String technicalID = "retropixel_" + chipID;
    String stateTopicPrefix = "retropixel/" + technicalID + "/state/";

    // ----------------------------------------------------
    // 1. CONTROL DE MODO (GIFs, Texto, Reloj, Arcade)
    // ----------------------------------------------------
    if (strTopic.endsWith("/cmd/mode")) {
        if (message == "GIF") config.playMode = 0;
        else if (message == "Texto") config.playMode = 1;
        else if (message == "Reloj") config.playMode = 2;
        else if (message == "Arcade") config.playMode = 3;
        
        
        mqttClient.publish((stateTopicPrefix + "mode").c_str(), message.c_str(), true);
        saveSystemConfig(); 
        Serial.println("Modo cambiado a: " + message);
    }

    // ----------------------------------------------------
    // 2. CONTROL DE ENCENDIDO / APAGADO
    // ----------------------------------------------------
    else if (strTopic.endsWith("/cmd/power")) {
        if (message == "ON") {
            config.powerState = true;
        } else {
            config.powerState = false;
            if (display) display->fillScreen(0);
        }
        
        mqttClient.publish((stateTopicPrefix + "power").c_str(), message.c_str(), true);
        saveSystemConfig(); 
        Serial.println("Energía: " + message);
    }

    // ----------------------------------------------------
    // 3. CONTROL DE BRILLO (0 - 255)
    // ----------------------------------------------------
    else if (strTopic.endsWith("/cmd/bright")) {
        int val = message.toInt();
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        
        config.brightness = val;
        if (display) {
            display->setBrightness8(config.brightness);
        }
        
        mqttClient.publish((stateTopicPrefix + "bright").c_str(), String(val).c_str(), true);
        saveSystemConfig(); 
        Serial.printf("Brillo ajustado a: %d\n", val);
    }

    // ----------------------------------------------------
    // 4. CONTROL DE TEXTO PERSONALIZADO
    // ----------------------------------------------------
    else if (strTopic.endsWith("/cmd/text")) {
        // 1. Actualizamos la variable slidingText que usa la marquesina
        config.slidingText = message; 

        // 2. Reiniciamos la posición para que el nuevo texto entre por la derecha
        xPosMarquesina = display->width(); 

        // 3. Guardar en la Flash para que persista tras reiniciar
        saveSystemConfig(); 

        // 4. Confirmar el estado a Home Assistant (mismo tópico de estado que en Discovery)
        mqttClient.publish((stateTopicPrefix + "text").c_str(), message.c_str(), true);
        
        Serial.println("Nuevo texto MQTT (slidingText): " + message);
    }
}

void syncMQTTState() {
    // Si no hay MQTT, no hacemos nada
    if (!config.mqtt_enabled || !mqttClient.connected()) return;

    String technicalID = "retropixel_" + chipID;
    
    // Preparamos los valores
    // Mapeo de los 4 modos: 0=GIFs, 1=Texto, 2=Reloj, 3=Arcade
    String modoTexto;
    switch (config.playMode) {
        case 0:  modoTexto = "GIFs";   break;
        case 1:  modoTexto = "Texto";  break;
        case 2:  modoTexto = "Reloj";  break;
        case 3:  modoTexto = "Arcade"; break;
        default: modoTexto = "GIFs";   break;
    }

    String brillo = String(config.brightness);
    String encendido = config.powerState ? "ON" : "OFF";

    // Enviamos los 4 estados a sus tópicos correspondientes
    mqttClient.publish(("retropixel/" + technicalID + "/state/mode").c_str(), modoTexto.c_str(), true);
    mqttClient.publish(("retropixel/" + technicalID + "/state/bright").c_str(), brillo.c_str(), true);
    mqttClient.publish(("retropixel/" + technicalID + "/state/power").c_str(), encendido.c_str(), true);
    mqttClient.publish(("retropixel/" + technicalID + "/state/text").c_str(), config.slidingText.c_str(), true);
    
    Serial.println("MQTT: Sincronización de estado enviada a HA (Modo: " + modoTexto + ")");
}

// --- TAREAS DUAL CORE ---
void TaskDisplay(void * pvParameters);

// ====================================================================
//                             SETUP Y LOOP
// ====================================================================

void setup() {
    Serial.begin(115200);

    // --- 1. SEMÁFORO (MUTEX) ---
    sdMutex = xSemaphoreCreateMutex();
    if (sdMutex == NULL) {
        Serial.println("Error al crear el Semáforo");
    }
       
    if (!SPIFFS.begin(true)) {
        Serial.println("Error al montar SPIFFS.");
    }

    loadConfig();

    // --- 2. INICIALIZACIÓN DE LA SD --- 
    SPI.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Error al montar la tarjeta SD!");
        sdMontada = false;
        delay(100);
    } else {
        Serial.println("Tarjeta SD montada correctamente.");
        sdMontada = true;
        scanFolders(GIFS_BASE_PATH);
    }   

    gif.begin(LITTLE_ENDIAN_PIXELS);

    // --- 3. CONEXIÓN WIFI --- 
    if (config.device_name[0] == '\0') {
        strncpy(config.device_name, DEVICE_NAME_DEFAULT, sizeof(config.device_name) - 1);
        config.device_name[sizeof(config.device_name) - 1] = '\0'; 
    }

    wm.setHostname(config.device_name);
    WiFi.mode(WIFI_AP_STA); 
    wm.setSaveConfigCallback(nullptr); 

    Serial.println("Intentando conectar o iniciando portal cautivo...");
    delay(500);
    if (!wm.autoConnect("Retro Pixel LED")) { 
        Serial.println("Fallo de conexión y timeout del portal. Reiniciando...");
        delay(3000);
        ESP.restart();
    } 

    Serial.println("\nConectado a WiFi.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    config.slidingText = "Retro Pixel LED v" + String(FIRMWARE_VERSION) + " - IP: " + WiFi.localIP().toString();
    
    // --- GENERAR ID ÚNICO ---
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    chipID = mac.substring(mac.length() - 6);
    chipID.toUpperCase(); 
    
    Serial.print("ID Único del Dispositivo: ");
    Serial.println(chipID);

    // --- 4. INICIALIZACIÓN DE LA MATRIZ LED --- 
    const int FINAL_MATRIX_WIDTH = PANEL_RES_X * config.panelChain;
    HUB75_I2S_CFG::i2s_pins pin_config = {
        R1_PIN, G1_PIN, B1_PIN,
        R2_PIN, G2_PIN, B2_PIN,
        A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
        LAT_PIN, OE_PIN, CLK_PIN
    };
    HUB75_I2S_CFG matrix_config(
        FINAL_MATRIX_WIDTH, 
        MATRIX_HEIGHT,      
        config.panelChain,  
        pin_config          
    );

    display = new MatrixPanel_I2S_DMA(matrix_config);
    if (display) { 
        display->begin();
        display->setBrightness8(config.brightness);
        if (!config.powerState) {
            display->fillScreen(0);
        } else {
            display->fillScreen(display->color565(0, 0, 0));
        } 

        if (!sdMontada) {
            mostrarMensaje("SD Error!", display->color565(255, 0, 0));
        } else if (WiFi.status() == WL_CONNECTED) {
            mostrarMensaje("WiFi OK!", display->color565(0, 255, 0));
        } else {
             mostrarMensaje("AP Mode", display->color565(255, 255, 0));
        }
        
        if (config.playMode == 0) {
            listarArchivosGif();
        }
        delay(1000);
    } else {
        Serial.println("ERROR: No se pudo asignar memoria para la matriz LED.");
    }
    
    initTime();
    
    // --- 5. CONFIGURACIÓN DE RUTAS DEL SERVIDOR WEB ---
    server.on("/", HTTP_GET, handleRoot);
    server.on("/power", HTTP_GET, handlePower);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/config", HTTP_GET, handleConfig); 
    server.on("/save_config", HTTP_GET, handleSaveConfig); 
    server.on("/restart", HTTP_GET, handleRestart); 
    server.on("/factory_reset", HTTP_GET, handleFactoryReset);
    
    // RUTAS GESTIÓN DE ARCHIVOS
    server.on("/file_manager", HTTP_GET, handleFileManager);
    server.on("/delete", HTTP_GET, handleFileDelete);
    server.on("/create_dir", HTTP_GET, handleCreateDir); 

    // RUTA PARA BATOCERA ---
    server.on("/batocera", HTTP_GET, []() {
    if (server.hasArg("s") && server.hasArg("g")) {
        // A. BLOQUEO: Pausamos para liberar la SD
        enModoGestion = true; 
        delay(150); 

        String s = server.arg("s");
        String g = server.arg("g");
        s.trim(); g.trim();

        if (g == "OFF") {
            // --- EVENTO DE APAGADO ---
            Serial.println(">>> BATOCERA OFF: Volviendo a Modo GIFs");
            config.playMode = 0; // Cambiamos al modo (GIFs)
        } 
        else if (g == "STOP" || g == "") {
            // --- EVENTO DE SALIDA DE JUEGO ---
            Serial.println(">>> GAME-STOP: Cargando Logo por defecto");
            rutaGifArcade = "/batocera/default/_default.gif";
            config.playMode = 3; // Mantenemos Modo Arcade para ver el logo
        } 
        else {
            // --- EVENTO DE INICIO DE JUEGO ---
            Serial.printf(">>> JUEGO -> Sistema: %s | Juego: %s\n", s.c_str(), g.c_str());
            rutaGifArcade = buscarEnCache(s, g);
            config.playMode = 3; // Mantenemos Modo Arcade para ver el logo
        }

        // B. REANUDACIÓN
        enModoGestion = false; 
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Faltan argumentos");
    }
});

    // REDIRECCIÓN TRAS SUBIDA
    server.on("/upload", HTTP_POST, [](){ 
        // Al terminar de subir, redirige al gestor para que el panel siga en "FILES MODE"
        server.sendHeader("Location", "/file_manager?path=" + currentPath);
        server.send(303); 
    }, handleFileUpload);
    
    // RUTAS OTA
    server.on("/ota", HTTP_GET, handleOTA);
    server.on("/ota_upload", HTTP_POST, [](){ 
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart(); 
    }, handleOTAUpload);
    
    server.onNotFound(notFound);

    server.begin();

    // --- 6. MQTT ---
    if (config.mqtt_enabled) {
        mqttClient.setServer(config.mqtt_host, config.mqtt_port);
        mqttClient.setBufferSize(1024);
        mqttClient.setCallback(callback);
        Serial.println("MQTT Configurado.");
    }

    // Lanzar la tarea del panel en el Core 1
    xTaskCreatePinnedToCore(
        TaskDisplay,   
        "TaskDisplay", 
        10000,          
        NULL,          
        2,             
        NULL,          
        1              
    );
    
    Serial.println("Servidor HTTP iniciado.");
}


void loop() {  // El Core 0 solo se encarga de la web, el WiFi y MQTT  

    // Recarga diferida de GIFs para evitar colapso de la web
    if (recargarGifsPendiente) {
        Serial.println(">> Posponiendo el escaneo de la SD para liberar recursos web...");
        delay(200);             // Pequeña pausa para asegurar que el cliente recibió la web
        listarArchivosGif();
        recargarGifsPendiente = false; 
        Serial.println(">> Escaneo de SD finalizado con éxito.");
    }

    // Gestión Web
    server.handleClient();
    // Solo si el MQTT está activado en la configuración
    if (config.mqtt_enabled) {
        if (!mqttClient.connected()) {
            reconnectMQTT();
        }
        mqttClient.loop();
    }
}

// --- TAREA PARA EL NÚCLEO 1 (PANEL LED) ---
void TaskDisplay(void * pvParameters) {
    for (;;) {
        // 1. Si el panel está apagado por software
        if (config.powerState == false) {
            display->fillScreen(0);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Si estamos en el Gestor de Archivos (enModoGestion == true)
        // Detenemos los GIFs y mostramos un mensaje de "FILES MODE"
        if (enModoGestion) {
            display->fillScreen(0);
            display->setTextColor(display->color565(255, 100, 0)); // Color Naranja
            display->setCursor(2, 7);
            display->print("FILES");
            display->setCursor(2, 17);
            display->print("MODE");
            
            // Dibujamos una pequeña línea de progreso o adorno Cyberpunk
            display->drawFastHLine(0, 27, 64, display->color565(0, 242, 255)); 
            
            vTaskDelay(pdMS_TO_TICKS(500)); // Esperamos medio segundo antes de volver a chequear
            continue; // Saltamos el resto del código (no se ejecutan GIFs)
        }

        // 3. Ejecución Normal con Semáforo (Si NO estamos en gestión)
        if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100))) {
            if (display) { 
                switch (config.playMode) {
                    case 0: ejecutarModoGif();   break;
                    case 1: ejecutarModoTexto(); break;
                    case 2: ejecutarModoReloj(); break;
                    case 3: ejecutarModoArcade(); break;
                }
            }
            // Soltamos el semáforo para que el núcleo 0 pueda usar la SD si lo necesita
            xSemaphoreGive(sdMutex);
        }

        // Un pequeño respiro de 1ms para que el Watchdog del sistema esté feliz
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}