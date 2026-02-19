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
#define FIRMWARE_VERSION "3.0.4" // MODIFICADO: Se implementa en el Modo Galería de GIFs la opción de mostrar el Reloj durante 10seg cada x número GIFs reproducidos.
#define PREF_NAMESPACE "pixel_config"
#define DEVICE_NAME_DEFAULT "RetroPixel-Default"
#define TZ_STRING_SPAIN "CET-1CEST,M3.5.0,M10.5.0/3" // Cadena TZ por defecto segura
#define GIFS_BASE_PATH "/gifs" // Directorio base para los GIFs
#define GIF_CACHE_FILE "/gif_cache.txt" // Archivo para guardar el índice de GIFs
#define GIF_CACHE_SIG "/gif_cache.sig" // Archivo de firma
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
MatrixPanel_I2S_DMA *display = nullptr; 
TaskHandle_t displayTaskHandle = NULL; // Manejador para poder matar la tarea en OTA


// Variables de estado y reproducción
bool sdMontada = false;
bool enModoGestion = false;
bool interrumpirReproduccion = false;
unsigned long gifCachePosition = 0; // Guardamos la posición del cursor en el archivo txt
bool hayGifsEnCache = false;        // Bandera simple para saber si hay contenido
int x_offset = 0; // Offset para centrado GIF
int y_offset = 0;
int contadorGifsReproducidos = 0;
unsigned long tiempoInicioRelojForzado = 0;
bool modoRelojTemporalActivo = false;
const long DURACION_RELOJ_MS = 10000;  // Mostrar reloj durante 10 segundos

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

// Datos dinámicos desde MQTT
String mqtt_temp = "--";    // Temperatura (ej: "22°")
int mqtt_weather_icon = 0;  // 0:Sol, 1:Lluvia, 2:Nubes, 3:Nieve...
String mqtt_custom_msg = ""; // Notificación

// Clientes
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Topics para Auto-Discovery
const char* discovery_topic_bright = "homeassistant/number/retropixel/brightness/config";
const char* discovery_topic_mode = "homeassistant/select/retropixel/mode/config";
const char* discovery_topic_text = "homeassistant/text/retropixel/message/config";

// Topics (Temas)
const char* topic_cmd_mode = "retropixel/cmd/mode";   // Cambiar modo
const char* topic_cmd_bright = "retropixel/cmd/bright"; // Cambiar brillo
const char* topic_state = "retropixel/state";         // Reportar estado a HA
const char* topic_cmd_power = "retropixel/cmd/power";  // Encender / Apagar el Switch
const char* topic_state_power = "retropixel/state/power";  // Reportar estado del switch
const char* topic_cmd_text = "retropixel/cmd/text";   // Recibir nuevo mensaje
const char* topic_state_text = "retropixel/state/text"; // Reportar mensaje actual a HA
const char* topic_cmd_clock_style = "retropixel/cmd/clock_style"; // Recibir estilo del reloj
const char* topic_state_clock_style = "retropixel/state/clock_style"; //Reportar estilo del reloj
const char* topic_cmd_clock_color = "retropixel/cmd/clock_color"; // Recibir color del reloj
const char* topic_state_clock_color = "retropixel/state/clock_color"; //Reportar color del reloj
const char* topic_cmd_text_color = "retropixel/cmd/text_color"; // Recibir color del texto
const char* topic_state_text_color = "retropixel/state/text_color"; //Reportar color del texto

// Iconos de clima 8x8 (1 bit por píxel)
const unsigned char icon_sun[]    = {0x00, 0x3c, 0x7e, 0x7e, 0x7e, 0x7e, 0x3c, 0x00};
const unsigned char icon_cloud[]  = {0x00, 0x00, 0x1c, 0x3f, 0x7f, 0x7f, 0x00, 0x00};
const unsigned char icon_rain[]   = {0x1c, 0x3f, 0x7f, 0x7f, 0x22, 0x44, 0x22, 0x00};
const unsigned char icon_snow[]   = {0x24, 0x00, 0xbd, 0x3c, 0x3c, 0xbd, 0x00, 0x24};
const unsigned char icon_storm[]  = {0x1c, 0x3f, 0x7f, 0x1c, 0x1c, 0x08, 0x10, 0x00};
const unsigned char icon_moon[] = {0x1c, 0x38, 0x70, 0x70, 0x70, 0x38, 0x1c, 0x00};
const unsigned char icon_fog[] = {0x00, 0x3e, 0x00, 0x7f, 0x00, 0x1c, 0x3e, 0x00};
const unsigned char icon_lightning_rainy[] = {0x3c, 0x7e, 0x18, 0x3c, 0x0c, 0x1e, 0x21, 0x00};

// --- CONFIGURACIÓN RELOJ ---
// Fuente compacta 5x8 para el Reloj Avanzado
const uint8_t font5x8[11][5] = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
  {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
  {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
  {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
  {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
  {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
  {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
  {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
  {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
  {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
  {0x00, 0x36, 0x36, 0x00, 0x00}  // :
};


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
    std::vector<String> activeFolders; 
    String activeFolders_str = "/GIFS";
    // 2. Configuración de Hora/Fecha
    String timeZone = TZ_STRING_SPAIN; 
    bool format24h = true;
    // 3. Configuración del Modo Reloj
    int clockEffect;
    uint32_t clockColor = 0x00FF00;
    bool autoClock;      // Activa/Desactiva el reloj automático
    int clockInterval;   // Cada cuántos GIFs se muestra el reloj
    // 4. Configuración del Modo Texto Deslizante
    uint32_t slidingTextColor = 0x00FF00;
    // 5. Configuración de Hardware/Sistema
    bool WifiOffMode; // Modo sin Wifi
    int panelChain = 2; // "Número de Paneles LED (en Cadena)"
    char device_name[40] = {0};
    // 6. Configuración MQTT Home Assistant
    bool mqtt_enabled = false;
    char mqtt_name[40] = "Retro Pixel LED"; // Nombre amigable para HA
    char mqtt_host[40] = "192.168.1.100";
    int mqtt_port = 1883;
    char mqtt_user[40] = "";
    char mqtt_pass[40] = "";
    // 7. Configuración Avanzada de Hardware
    int minRefreshRate;  // Tasa de refresco
    int latchBlanking;   // Para el ghosting
    int i2sSpeed;        // 0=8Mhz 1=10Mhz, 2=16Mhz, 3=20Mhz
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
    config.brightness = preferences.getInt("brightness", 40); // Aplicamos un 15% de brillo por defecto
    config.playMode = preferences.getInt("playMode", 1);
    config.slidingText = preferences.getString("slidingText", config.slidingText);
    config.textSpeed = preferences.getInt("textSpeed", 50);
    config.gifRepeats = preferences.getInt("gifRepeats", 1);
    config.randomMode = preferences.getBool("randomMode", false);
    config.mqtt_enabled = preferences.getBool("mqtt_en", false);
    String mName = preferences.getString("m_name", "Retro Pixel LED");
    config.timeZone = preferences.getString("timeZone", TZ_STRING_SPAIN);
    config.clockEffect = preferences.getInt("clockEffect", 0);  
    config.clockColor = preferences.getULong("clockColor", 0x00FF00);
    config.slidingTextColor = preferences.getULong("slideColor", 0x00FF00);
    config.WifiOffMode = preferences.getBool("WifiOffMode", false);
    config.panelChain = preferences.getInt("panelChain", 2);
    config.autoClock = preferences.getBool("autoClock", false); // Por defecto desactivado
    config.clockInterval = preferences.getInt("clockInterval", 5); // Por defecto cada 5
    // Carga de ajustes avanzados ---
    // Por defecto 90Hz, Blanking 1, Velocidad 1 (10MHz)
    config.minRefreshRate = preferences.getInt("minRefresh", 90);
    config.latchBlanking = preferences.getInt("latchBlank", 1);
    config.i2sSpeed = preferences.getInt("i2sSpeed", 1);
    // Configuración MQTT
    strncpy(config.mqtt_name, mName.c_str(), sizeof(config.mqtt_name));
    String mHost = preferences.getString("m_host", "192.168.1.100");
    strncpy(config.mqtt_host, mHost.c_str(), sizeof(config.mqtt_host));
    config.mqtt_port = preferences.getInt("m_port", 1883);
    String mUser = preferences.getString("m_user", "");
    strncpy(config.mqtt_user, mUser.c_str(), sizeof(config.mqtt_user));
    String mPass = preferences.getString("m_pass", "");
    strncpy(config.mqtt_pass, mPass.c_str(), sizeof(config.mqtt_pass));
    
    // device_name: Lectura especial para char array
    String nameStr = preferences.getString("deviceName", DEVICE_NAME_DEFAULT);
    strncpy(config.device_name, nameStr.c_str(), sizeof(config.device_name) - 1);
    config.device_name[sizeof(config.device_name) - 1] = '\0';
    
    config.activeFolders_str = preferences.getString("activeFolders", "/GIFS");
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
    
    preferences.end();
}

void savePlaybackConfig() { 
    preferences.begin(PREF_NAMESPACE, false);// modo escritura

    preferences.putBool("powerState", config.powerState);
    preferences.putInt("brightness", config.brightness);
    preferences.putInt("playMode", config.playMode);
    preferences.putString("slidingText", config.slidingText);
    preferences.putInt("textSpeed", config.textSpeed);
    preferences.putInt("gifRepeats", config.gifRepeats);
    preferences.putBool("randomMode", config.randomMode);
    preferences.putBool("autoClock", config.autoClock);
    preferences.putInt("clockInterval", config.clockInterval);
    
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
    
    preferences.putBool("powerState", config.powerState);
    preferences.putString("timeZone", config.timeZone);
    preferences.putBool("format24h", config.format24h);
    preferences.putInt("clockEffect", config.clockEffect);
    preferences.putULong("clockColor", config.clockColor);;
    const char* newKey = "slideColor";
    preferences.putULong(newKey, config.slidingTextColor);
    preferences.putInt("panelChain", config.panelChain);
    preferences.putBool("WifiOffMode", config.WifiOffMode);
    preferences.putString("deviceName", config.device_name);
    // Ajustes avanzados
    preferences.putInt("minRefresh", config.minRefreshRate);
    preferences.putInt("latchBlank", config.latchBlanking);
    preferences.putInt("i2sSpeed", config.i2sSpeed);
    // Configuración MQTT
    preferences.putBool("mqtt_en", config.mqtt_enabled);
    preferences.putString("m_name", config.mqtt_name);
    preferences.putString("m_host", config.mqtt_host);
    preferences.putInt("m_port", config.mqtt_port);
    preferences.putString("m_user", config.mqtt_user);
    preferences.putString("m_pass", config.mqtt_pass);
    
    preferences.end();

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

// ====================================================================
//                  INTERFAZ WEB PRINCIPAL
// ====================================================================
    
    //enModoGestion = false; // Al entrar al inicio, liberamos el panel para que muestre GIFs
    //server.send(200, "text/html", webPage());

    // 1. Aseguramos rendimiento máximo al cargar la web
    if (getCpuFrequencyMhz() < 240) setCpuFrequencyMhz(240);

    // 2. Avisamos al navegador que enviaremos por trozos (Chunked)
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", ""); // Envía solo las cabeceras

    // --- VARIABLES ---
    char hexColor[8]; sprintf(hexColor, "#%06X", config.clockColor);
    char hexTextColor[8]; sprintf(hexTextColor, "#%06X", config.slidingTextColor); 
    int brightnessPercent = (int)(((float)config.brightness / 255.0) * 100.0);

    // --- TROZO 1: CABECERA Y ESTILOS ---
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>";
    html += "<title>Retro Pixel LED</title>" + getStyle() + "</head><body><div class='c'>";
    html += "<h1>Retro Pixel <span style='font-weight:200'>LED</span></h1>";
    server.sendContent(html); // Enviamos el primer trozo y liberamos memoria

    // --- TROZO 2: ESTADO DEL PANEL ---
    html = "<div class='card'><h3>Estado del Panel</h3>";
    if (config.powerState) 
        html += "<a href='/power' class='btn' style='background:rgba(211,47,47,0.1); border:1px solid #ff2e63; color:#ff2e63; box-shadow: 0 0 15px rgba(255,46,99,0.2);'>APAGAR MATRIZ LED</a>";
    else 
        html += "<a href='/power' class='btn' style='background:rgba(56,142,60,0.1); border:1px solid #08d9d6; color:#08d9d6; box-shadow: 0 0 15px rgba(8,217,214,0.2);'>ENCENDER MATRIZ LED</a>";
    html += "</div><form action='/save' method='POST'>";
    server.sendContent(html);

    // --- TROZO 3: BRILLO Y MODO ---
    html = "<div class='card'><h3>Brillo <span id='brightnessValue' style='color:#00f2ff; float:right;'>" + String(brightnessPercent) + "%</span></h3>";
    html += "<input type='range' name='b' min='0' max='255' value='" + String(config.brightness) + "' oninput='updateBrightness(this.value)'></div>";
    
    html += "<div class='card'><h3>Modo de Reproducción</h3>";
    html += "<select name='pm' id='playModeSelect'>";
    html += String("<option value='0'") + (config.playMode == 0 ? " selected" : "") + ">📁 Galería de GIFs</option>";
    html += String("<option value='1'") + (config.playMode == 1 ? " selected" : "") + ">📝 Texto Deslizante</option>";
    html += String("<option value='2'") + (config.playMode == 2 ? " selected" : "") + ">🕒 Reloj Digital</option>";
    html += String("<option value='3'") + (config.playMode == 3 ? " selected" : "") + ">🕹️ Arcade</option></select></div>";
    server.sendContent(html);

    // --- TROZO 4: CONFIGURACIÓN GIF ---
    html = "<div id='gifConfig' style='display:" + String(config.playMode == 0 ? "block" : "none") + ";'>";
    html += "<div class='card'><h3>Ajustes de Galería</h3>";
    
    // Reloj Automático
    html += "<div style='display:flex; align-items:center; justify-content:space-between; background:rgba(0,242,255,0.05); padding:10px; border-radius:8px; margin-bottom:15px; border:1px solid rgba(0,242,255,0.1);'>";
    html += "  <label style='display:flex; align-items:center; cursor:pointer; font-size:13px; margin:0;'>";
    html += "    <input type='checkbox' name='ac' onchange='toggleAutoClock(this.checked)' " + String(config.autoClock ? "checked" : "") + " style='margin-right:8px;'>";
    html += "   🕒 Mostar Reloj";
    html += "  </label>";
    html += "  <div id='autoClockSettings' style='display:" + String(config.autoClock ? "block" : "none") + ";'>";
    html += "    <span style='font-size:12px; color:#888;'>Cada: </span>";
    html += "    <input type='number' name='ci' min='1' max='99' value='" + String(config.clockInterval) + "' style='width:45px; background:#111; border:1px solid #333; color:#00f2ff; text-align:center; border-radius:4px; padding:2px;'>";
    html += "    <span style='font-size:12px; color:#888;'> GIFs</span>";
    html += "  </div>";
    html += "</div>";

    // Repeticiones y Orden
    html += "<div class='grid-2'>";
    html += " <div><label>Repeticiones</label><input type='number' name='r' min='1' max='50' value='" + String(config.gifRepeats) + "'></div>";
    html += " <div><label>Orden</label><select name='m'><option value='0'" + String(config.randomMode ? "" : " selected") + ">Secuencial</option><option value='1'" + String(config.randomMode ? " selected" : "") + ">Aleatorio</option></select></div>";
    html += "</div>";
    
    html += "<label style='margin-top:20px;'>Carpetas en SD</label><div class='cb'>";
    if (sdMontada) {
        if (allFolders.empty()) {
            html += "<p style='color:#888; font-size:11px;'>No hay carpetas en /gifs</p>";
        } else {
            for (const String& f : allFolders) {
                bool isChecked = (std::find(config.activeFolders.begin(), config.activeFolders.end(), f) != config.activeFolders.end());
                html += "<label><input type='checkbox' name='f' value='" + f + "'" + (isChecked ? " checked" : "") + "> " + f + "</label>";
            }
        }
    } else {
        html += "<p style='color:#ff2e63; font-size:11px;'>⚠️ SD NO DETECTADA</p>";
    }
    html += "</div></div></div>";
    server.sendContent(html);

    // --- TROZO 5: TEXTO, RELOJ Y FOOTER (Limpiado) ---
    html = "<div id='textConfig' style='display:" + String(config.playMode == 1 ? "block" : "none") + ";'>";
    html += "<div class='card'><h3>Configuración Texto</h3>";
    html += "<label>Mensaje Personalizado</label><input type='text' name='st' value='" + config.slidingText + "' maxlength='100'>";
    html += "<div class='grid-2' style='align-items: center;'>";
    html += " <div><label>Velocidad (ms)</label><input type='number' name='ts' min='10' max='1000' value='" + String(config.textSpeed) + "'></div>";
    html += " <div style='text-align:center;'><label>Color</label><input type='color' name='stc' value='" + String(hexTextColor) + "'></div>";
    html += "</div></div></div>"; 

    html += "<div id='clockConfig' style='display:" + String(config.playMode == 2 ? "block" : "none") + ";'>";
    html += "<div class='card'><h3>Configuración Reloj</h3>";
    html += "<div class='grid-2' style='align-items: center;'>";
    html += " <div><label>Efecto Visual</label><select name='clockEffect'>";
    const char* effects[] = {"Rainbow Flow", "Static Rainbow", "Solid Neon", "Night Fire", "Pulse Breath", "Matrix Digital", "Color Gradient 50%", "Color Gradient 80%"};
    for(int i = 0; i < 8; i++) {
        html += "<option value='" + String(i) + "'" + (config.clockEffect == i ? " selected" : "") + ">" + effects[i] + "</option>";
    }
    html += " </select></div>";
    html += " <div style='text-align:center;'><label>Color Base</label><input type='color' name='cc' value='" + String(hexColor) + "'></div>";
    html += "</div></div></div>"; 
    server.sendContent(html);

    // Botones y Footer
    html = "<button type='submit' class='btn save-btn'>GUARDAR CAMBIOS</button>";
    html += "<div class='grid-3'>";
    html += " <a href='/config' class='btn btn-ajustes'>AJUSTES</a>";
    html += " <a href='/file_manager' class='btn btn-files' onclick='return confirm(\"Se detendrá el panel. ¿Continuar?\")'>FILES</a>";
    html += " <a href='/ota' class='btn btn-ota'>OTA</a>";
    html += "</div></form>";
    html += "<div class='footer'><span>v" + String(FIRMWARE_VERSION) + "</span><span>IP: " + WiFi.localIP().toString() + "</span></div>";

    // --- TROZO 6: JAVASCRIPT (Simplificado) ---
    html += "<script>";
    html += "function updateBrightness(v){ document.getElementById('brightnessValue').innerHTML=Math.round((v/255)*100)+'%'; }";
    html += "function toggleAutoClock(e){document.getElementById('autoClockSettings').style.display=e?'block':'none';}";
    html += "var sel = document.getElementById('playModeSelect');";
    html += "if(sel){ sel.onchange=function(){"; 
    html += " var m = this.value;";
    html += " document.getElementById('gifConfig').style.display = (m=='0')?'block':'none';";
    html += " document.getElementById('textConfig').style.display = (m=='1')?'block':'none';";
    html += " document.getElementById('clockConfig').style.display = (m=='2')?'block':'none';";
    html += "};}"; 
    html += "</script></div></body></html>";
    
    server.sendContent(html);
    
    // 3. Finalizamos el envío
    server.sendContent(""); 

}

//void handleConfig() { server.send(200, "text/html", configPage()); }
void handleConfig() {

// ====================================================================
//                  INTERFAZ WEB CONFIGURACIÓN
// ====================================================================
    // 1. Rendimiento máximo y cabeceras Chunked
    if (getCpuFrequencyMhz() < 240) setCpuFrequencyMhz(240);
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // --- TROZO 1: CABECERA Y WIFI ---
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'>";
    html += "<title>Configuración</title>" + getStyle() + "</head><body><div class='c'>";
    html += "<h1>Configuración</h1><form action='/save_config' method='POST'>";

    html += "<div class='card'><h2>1. WiFi y Reloj</h2>";
    html += "<label>Modo de Funcionamiento</label><select name='wifiOffMode'>";
    html += "<option value='0'" + String(!config.WifiOffMode ? " selected" : "") + ">Online (Necesita red WiFi)</option>";
    html += "<option value='1'" + String(config.WifiOffMode ? " selected" : "") + ">Offline (No necesita red WiFi)</option>";
    html += "</select>";
    html += "<div class='info-box' style='background: #332200; border-left: 4px solid #ffaa00; padding: 10px; margin-top: 10px;'>";
    html += "⚠️ <b>Nota:</b> En modo Offline el panel genera su propia red, conéctate a ella para usar Retro Pixel LED. En este modo el reloj no se sincronizará. ";
    html += "Al cambiar de modo, debes <b>Guardar y Reiniciar</b>.";
    html += "<i>* Si no se configura una red WiFi en 3 min, se activa el Modo Offline automáticamente.</i></div><br>";

    html += "<label>Zona Horaria (TZ String)</label><input type='text' name='tz' value='" + config.timeZone + "'>";
    html += "<div class='info-box'>Valor por defecto para España: <code>" + String(TZ_STRING_SPAIN) + "</code>. <br>Puedes buscar otras zonas horarias aquí: <a href='https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv' target='_blank' style='color:#00fbff;'>Lista de TZ Strings</a>. La gestión del DST es automática.</div></div>";
    server.sendContent(html);

    // --- TROZO 2: HARDWARE ---
    html = "<div class='card'><h2>2. Hardware & Panel</h2>"; 
    html += "<label>Número de Paneles (Chain)</label><input type='number' name='pc' min='1' max='8' value='" + String(config.panelChain) + "'>";
    html += "<div class='grid-2'>"; 
    html += "<div><label>Velocidad I2S</label><select name='i2s'>";
    const char* speeds[] = {"8 MHz (Seguro)", "10 MHz (Estable)", "16 MHz (Normal)", "20 MHz (Turbo)"};
    for(int i=0; i<4; i++) {
        html += "<option value='" + String(i) + "'" + (config.i2sSpeed == i ? " selected" : "") + ">" + speeds[i] + "</option>";
    }
    html += "</select></div>";
    html += "<div><label>Refresco Mín. (Hz)</label><input type='number' name='mrr' min='30' max='120' value='" + String(config.minRefreshRate) + "'></div></div>";

    html += "<label style='margin-top:10px;'>Latch Blanking (Anti-Ghosting)</label>";
    html += "<div style='display:flex; gap:10px; align-items:center;'>";
    html += "<input type='range' name='lb' min='1' max='4' step='1' value='" + String(config.latchBlanking) + "' oninput='document.getElementById(\"lbVal\").innerText=this.value'>";
    html += "<span id='lbVal' style='color:#00f2ff; font-weight:bold; font-size:14px; width:20px;'>" + String(config.latchBlanking) + "</span></div>";
    html += "<div class='info-box'>⚠️ <b>Nota:</b> Subir el 'Latch Blanking' reduce el brillo fantasma. Un refesco muy alto puede desiquilibar la conexión WiFi. <br>Cambiar estos valores requiere <b>Guardar y Reiniciar</b>.</div></div>";
    server.sendContent(html);

    // --- TROZO 3: MQTT ---
    html = "<div class='card'><h2>3. Home Assistant (MQTT)</h2>";
    html += "<div class='cb'><label><input type='checkbox' id='mqtt_en' name='mqtt_en' value='1' onchange='toggleMQTT(this.checked)'" + String(config.mqtt_enabled ? " checked" : "") + "> Activar Integración</label></div>";
    html += "<div id='mqtt_fields' style='display:" + String(config.mqtt_enabled ? "block" : "none") + "; margin-top:10px;'>";
    html += "<label>Nombre Dispositivo</label><input type='text' name='m_name' value='" + String(config.mqtt_name) + "'>";
    html += "<div class='grid-2'><div><label>Broker IP</label><input type='text' name='m_host' value='" + String(config.mqtt_host) + "'></div>";
    html += "<div><label>Puerto</label><input type='number' name='m_port' value='" + String(config.mqtt_port) + "'></div></div>";
    html += "<label>Usuario</label><input type='text' name='m_user' value='" + String(config.mqtt_user) + "'>";
    html += "<label>Contraseña</label><input type='password' name='m_pass' value='" + String(config.mqtt_pass) + "'></div></div>";
    server.sendContent(html);

    // --- TROZO 4: BOTONES Y FOOTER ---
    html = "<div class='dual-grid' style='grid-template-columns: repeat(3, 1fr); margin-bottom: 20px;'>";
    html += "<button type='submit' class='btn save-btn'>GUARDAR</button>";
    html += "<button type='button' class='btn restart-btn' onclick=\"if(confirm('¿Reiniciar?')) location.href='/restart';\">RESET</button>";
    html += "<a href='/' class='btn back-btn'>VOLVER</a>";
    html += "</div></form>";

    html += "<button type='button' class='btn reset-btn' onclick=\"if(confirm('¿BORRAR TODO?')) location.href='/factory_reset';\">RESTABLECER FÁBRICA</button>";
    html += "<div class='footer'><span>v" + String(FIRMWARE_VERSION) + " - fjgordillo86</span><span>IP: " + WiFi.localIP().toString() + "</span></div>";
    html += "<script>function toggleMQTT(s){ document.getElementById('mqtt_fields').style.display=s?'block':'none'; }</script></div></body></html>";
    
    server.sendContent(html);
    server.sendContent(""); // Finalizar envío
}


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
    // 1. Recogida de argumentos existentes
    int tempBrightness = server.hasArg("b") ? server.arg("b").toInt() : config.brightness;
    int tempPlayMode = server.hasArg("pm") ? server.arg("pm").toInt() : config.playMode;
    String tempSlidingText = server.hasArg("st") ? server.arg("st") : config.slidingText;
    int tempTextSpeed = server.hasArg("ts") ? server.arg("ts").toInt() : config.textSpeed;
    int tempGifRepeats = server.hasArg("r") ? server.arg("r").toInt() : config.gifRepeats;
    bool tempRandomMode = server.hasArg("m") ? (server.arg("m").toInt() == 1) : config.randomMode;

    if (server.hasArg("clockEffect")) config.clockEffect = server.arg("clockEffect").toInt();
    if (server.hasArg("cc")) config.clockColor = parseHexColor(server.arg("cc"));
    if (server.hasArg("stc")) config.slidingTextColor = parseHexColor(server.arg("stc"));

    // --- Recogida de Auto Reloj ---
    // Si el checkbox "ac" está presente, es que se ha marcado (true)
    config.autoClock = server.hasArg("ac"); 
    
    // Si viene el intervalo "ci", lo guardamos
    if (server.hasArg("ci")) {
        config.clockInterval = server.arg("ci").toInt();
        if (config.clockInterval < 1) config.clockInterval = 1; // Seguridad
    }

    // 2. Gestión de Carpetas Temporal
    std::vector<String> tempFolders;
    for(size_t i = 0; i < server.args(); ++i) {
        if (server.argName(i) == "f") {
            tempFolders.push_back(server.arg(i));
        }
    }

    if (tempFolders.empty() && sdMontada) {
         tempFolders.push_back("/");
    }

    // 3. Actualizar configuración en RAM
    config.brightness = tempBrightness;
    config.playMode = tempPlayMode;
    config.slidingText = tempSlidingText;
    config.textSpeed = tempTextSpeed;
    config.gifRepeats = tempGifRepeats;
    config.randomMode = tempRandomMode;
    config.activeFolders = tempFolders;

    if (display) display->setBrightness8(config.brightness);

    // 4. Gestión del escaneo
    if (config.playMode == 0) {
        recargarGifsPendiente = true; 
        // Resetear contador al guardar para empezar un ciclo limpio
        contadorGifsReproducidos = 0;
        modoRelojTemporalActivo = false;
        Serial.println(">> Escaneo de carpetas programado...");
    }

    // 5. Guardar en Flash
    savePlaybackConfig(); // Asegúrate de que esta función guarde autoClock y clockInterval en NVS

    // 6. Respuesta Web
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Guardado");

    // 7. Sincronizar con Home Assistant
    syncMQTTState();
}

void handleSaveConfig() { 
    // 1. Reseteamos el timeout del portal para darnos margen
    wm.setConfigPortalTimeout(180); 

    // 2. Configuración de Sistema y Hardware
    if (server.hasArg("deviceName")) { 
        String nameStr = server.arg("deviceName");
        strncpy(config.device_name, nameStr.c_str(), sizeof(config.device_name) - 1);
        config.device_name[sizeof(config.device_name) - 1] = '\0';
    }
    if (server.hasArg("pc")) config.panelChain = server.arg("pc").toInt();

    // Ajustes avanzados ---
    if (server.hasArg("i2s")) config.i2sSpeed = server.arg("i2s").toInt();
    if (server.hasArg("mrr")) config.minRefreshRate = server.arg("mrr").toInt();
    if (server.hasArg("lb"))  config.latchBlanking = server.arg("lb").toInt();

    // 3. Configuración de Reloj y Wifi (Modo Online/Offline)
    if (server.hasArg("wifiOffMode")) {
        config.WifiOffMode = (server.arg("wifiOffMode") == "1");
    }
    if (server.hasArg("tz")) config.timeZone = server.arg("tz");

    if (server.hasArg("clockEffect")) {
        config.clockEffect = server.arg("clockEffect").toInt();
    }

    // 4. Colores y Texto
    if (server.hasArg("cc")) config.clockColor = parseHexColor(server.arg("cc"));
    if (server.hasArg("stc")) config.slidingTextColor = parseHexColor(server.arg("stc"));
    if (server.hasArg("st")) { 
        config.slidingText = server.arg("st"); 
        xPosMarquesina = display->width();
    }

    // 5. Configuración MQTT
    config.mqtt_enabled = server.hasArg("mqtt_en");
    if (server.hasArg("m_name")) strncpy(config.mqtt_name, server.arg("m_name").c_str(), sizeof(config.mqtt_name) - 1);
    if (server.hasArg("m_host")) strncpy(config.mqtt_host, server.arg("m_host").c_str(), sizeof(config.mqtt_host) - 1);
    if (server.hasArg("m_port")) config.mqtt_port = server.arg("m_port").toInt();
    if (server.hasArg("m_user")) strncpy(config.mqtt_user, server.arg("m_user").c_str(), sizeof(config.mqtt_user) - 1);
    if (server.hasArg("m_pass")) strncpy(config.mqtt_pass, server.arg("m_pass").c_str(), sizeof(config.mqtt_pass) - 1);

    // 6. Guardar en FLASH
    saveSystemConfig();
    
    // 7. Sincronización MQTT 
    // Solo si NO estamos en Offline, si MQTT está habilitado y si hay conexión real
    if (!config.WifiOffMode && config.mqtt_enabled && mqttClient.connected()) {
        String technicalID = "retropixel_" + chipID;
        mqttClient.publish(("retropixel/" + technicalID + "/state/text").c_str(), config.slidingText.c_str(), true);
        syncMQTTState();
        // Nota: No desconectamos para que el panel siga reportando mientras no reiniciemos
    }

    // 8. Respuesta al navegador (Redirección 302)
    server.sendHeader("Location", "/config");
    server.send(302, "text/plain", "Configuracion Guardada");

    Serial.println(">> Configuración guardada y MQTT condicionado aplicado.");
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
    
    "input[type='color'] { -webkit-appearance: none; border: 2px solid rgba(255,255,255,0.1); width: 90px; height: 45px; border-radius: 12px; background: none; cursor: pointer; transition: transform 0.2s; }"
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
        // 1. DETENER TAREA DE DISPLAY (Evita crash por conflicto de núcleos)
        if (displayTaskHandle != NULL) {
            vTaskDelete(displayTaskHandle);
            displayTaskHandle = NULL;
        }
        
        // 2. APAGAR PANTALLA Y DETENER GIFS
        if (display) display->fillScreen(0);
        gif.close(); // Cerrar acceso SD
        
        Serial.printf("OTA: Iniciando actualización: %s\n", upload.filename.c_str());
        
        // 3. INICIAR UPDATE
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
        
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.println("OTA: Éxito. Reiniciando...");
            String s = "<!DOCTYPE html><html><head>" + getStyle() + "</head><body><div class='c'><div class='card'><h2>¡ÉXITO!</h2><p>Actualización completada.<br>Reiniciando sistema...</p></div><script>setTimeout(function(){location.href='/';},10000);</script></div></body></html>";
            server.send(200, "text/html", s);
            delay(1000); 
            ESP.restart();
        } else {
            Update.printError(Serial);
            server.send(500, "text/plain", "Error Finalizando OTA");
        }
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
                 //listarArchivosGif(); Ya no listamos hemos cambiado a escribir directamente en SD tardaria mucho si hay demasiados GIFs
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
    html += "<form action='/create_dir' method='POST' style='margin-top:10px;'>";
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
                html += "<a href='#' onclick='postDelete(\"" + fileName + "\", \"dir\"); return false;' style='color:#ff2e63; text-decoration:none; font-size:20px;'>&times;</a>";
            } else {
                html += "<div style='display:flex; flex-direction:column;'><span style='color:#eee; font-size:13px;'>📄 " + fileName + "</span><span style='color:#555; font-size:9px;'>" + String(file.size() / 1024) + " KB</span></div>";
                html += "<a href='#' onclick='postDelete(\"" + fileName + "\", \"file\"); return false;' style='color:#ff2e63; text-decoration:none; font-size:20px;'>&times;</a>";
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

    // SCRIPT PARA GESTIONAR EL BORRADO VIA POST
    html += "<script>"
            "function postDelete(name, type) {"
            "  if(!confirm('¿Estás seguro de borrar ' + name + '?')) return;"
            "  var form = document.createElement('form');"
            "  form.method = 'POST';"
            "  form.action = '/delete';"
            "  var inputName = document.createElement('input');"
            "  inputName.type = 'hidden'; inputName.name = 'name'; inputName.value = name;"
            "  var inputType = document.createElement('input');"
            "  inputType.type = 'hidden'; inputType.name = 'type'; inputType.value = type;"
            "  form.appendChild(inputName);"
            "  form.appendChild(inputType);"
            "  document.body.appendChild(form);"
            "  form.submit();"
            "}"
            "</script>";

    return html;
}

// ====================================================================
//                   FUNCIONES CORE DE VISUALIZACIÓN
// ====================================================================

// --- 1. Funciones Callback para AnimatedGIF ---

// Función de dibujo de la librería GIF
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

// --- 2. Funciones de color y dibujo ---
// Convierte colores HSV a formato RGB565 (Efecto Arcoíris)
uint16_t hsvTo565(uint16_t h, uint8_t s, uint8_t v) {
    float fH = h / 60.0;
    float fS = s / 255.0;
    float fV = v / 255.0;
    float c = fV * fS;
    float x = c * (1 - fabs(fmod(fH, 2.0) - 1));
    float m = fV - c;
    float r, g, b;
    if (fH < 1) { r = c; g = x; b = 0; }
    else if (fH < 2) { r = x; g = c; b = 0; }
    else if (fH < 3) { r = 0; g = c; b = x; }
    else if (fH < 4) { r = 0; g = x; b = c; }
    else if (fH < 5) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    return ((uint16_t)((r + m) * 31) << 11) | ((uint16_t)((g + m) * 63) << 5) | (uint16_t)((b + m) * 31);
}

// Dibuja un carácter usando la fuente 5x8
void drawCustomChar(int x, int y, int index, uint16_t color, int scale) {
    for (int i = 0; i < 5; i++) {
        uint8_t line = font5x8[index][i];
        for (int j = 0; j < 8; j++) {
            if (line & (1 << j)) {
                display->fillRect(x + (i * scale), y + (j * scale), scale, scale, color);
            }
        }
    }
}


// --- 3. Funciones de Utilidad de Visualización ---

void mostrarMensaje(const char* mensaje, uint16_t color = 0xF800 ) {
    if (!display) return;
    display->fillScreen(0);
    display->setTextSize(1);
    display->setTextWrap(false);
    display->setTextColor(color);
    display->setCursor(0, MATRIX_HEIGHT / 2 - 4);
    display->print(mensaje);
    display->flipDMABuffer();
}

// --- 4. Funcion para área de Notificaciones ---

void dibujarBarraNotificaciones() {

    // Calculamos el punto de inicio igual que en tu reloj
    int offset = (display->width() - 0) / 2; 

    // 1. Configuración de fuente
    display->setFont(NULL);      // Fuente por defecto (5x7)
    display->setTextSize(1);     // Tamaño original

    // 2. Borramos la franja superior en toda la pantalla
        display->fillRect(128, 0, 128, 8, 0); 
     
    // 3. Dibujamos Icono según MQTT
    uint16_t colorClima;
    const unsigned char* iconToDraw;

    switch(mqtt_weather_icon) {
        case 0: iconToDraw = icon_sun;   colorClima = display->color565(255, 255, 0); break;
        case 1: iconToDraw = icon_cloud; colorClima = display->color565(180, 180, 180); break;
        case 2: iconToDraw = icon_rain;  colorClima = display->color565(0, 100, 255); break;
        case 3: iconToDraw = icon_snow;  colorClima = display->color565(255, 255, 255); break;
        case 4: iconToDraw = icon_storm; colorClima = display->color565(200, 0, 200); break;
        case 5: iconToDraw = icon_moon; colorClima = display->color565(200, 200, 255); break;
        case 6: iconToDraw = icon_lightning_rainy; colorClima = display->color565(200, 0, 255); break;
        case 7: iconToDraw = icon_fog; colorClima = display->color565(180, 180, 200); break;
        default: iconToDraw = icon_sun;  colorClima = display->color565(255, 255, 0); break;
    }
    display->drawBitmap(offset + 97, 0, iconToDraw, 8, 8, colorClima);

    // 4. Dibujar Temperatura
    display->setTextColor(display->color565(200, 200, 200));
    display->setCursor(offset + 107, 0); 
    display->print(mqtt_temp);
    if(mqtt_temp != "--") {
        // Dibujamos un pequeño cuadrado de 2x2 para el grado
        display->drawRect(offset + 119, 0, 2, 2, display->color565(200, 200, 200));
        display->setCursor(offset + 122, 0);
        display->print("C");
    }

    // 5. Notificación
    if (mqtt_custom_msg != "") {
        display->setCursor(offset + 2, 0);
        display->setTextColor(display->color565(200, 200, 200));
        display->print(mqtt_custom_msg);
    }
    
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
// Función modificada para escribir directamente en el archivo de caché
void scanGifDirectory(File &cacheFile, String path) {
    File root = SD.open(path);
    if (!root) {
        Serial.printf("Error al abrir directorio: %s\n", path.c_str());
        return;
    }
    
    File entry = root.openNextFile();
    while(entry){
        yield(); // Permitir que el sistema atienda procesos en segundo plano
        if(!entry.isDirectory()){
            String fileName = entry.name();
            if (fileName.endsWith(".gif") || fileName.endsWith(".GIF")) {
                String fullPath = path + "/" + fileName;
                // Limpiamos el doble slash si la ruta es la raíz (/)
                if (path == "/") fullPath = fileName;
                
                // ESCRITURA DIRECTA A LA SD
                cacheFile.println(fullPath);
                hayGifsEnCache = true; // Marcamos que hemos encontrado al menos uno
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
    // 1. Mostrar mensaje informativo en el panel LED 
    if (display) {
        display->fillScreen(0);
        display->setTextColor(display->color565(0, 242, 255)); // Cian Cyberpunk
        display->setTextSize(1);
        display->setCursor(168, 7);
        display->print("LISTANDO");
        display->setCursor(168, 17);
        display->print("GIFS...");
        // Opcional: una línea de progreso estética
        display->drawFastHLine(160, 27, 64, display->color565(255, 0, 100)); 
    }

    // 2. Generar firma basada en las carpetas activas
    String currentSignature = generateCacheSignature();
    if (currentSignature.length() == 0) { 
        hayGifsEnCache = false;
        Serial.println("No hay carpetas seleccionadas.");
        return;
    }

    // 3. Comprobar si la firma ha cambiado para evitar escaneos innecesarios
    bool cacheIsValid = false;
    if (SD.exists(GIF_CACHE_SIG)) {
        File sigFile = SD.open(GIF_CACHE_SIG, FILE_READ);
        if (sigFile) {
            String savedSignature = sigFile.readStringUntil('\n');
            savedSignature.trim();
            // Si la firma coincide y el archivo existe, la caché es válida
            if (savedSignature == currentSignature && SD.exists(GIF_CACHE_FILE)) {
                cacheIsValid = true;
            }
            sigFile.close();
        }
    }

    // 4. Si la caché es válida, no escaneamos la SD, solo reseteamos el puntero
    if (cacheIsValid) {
        Serial.println("Caché válida detectada. Usando lista existente.");
        gifCachePosition = 0; 
        hayGifsEnCache = true;
        return;
    }

    // 5. Si NO es válida (cambiaron carpetas o primer inicio), regeneramos
    Serial.println("Generando nuevo índice de GIFs en SD...");
    
    // Borramos el caché anterior para empezar limpio
    if (SD.exists(GIF_CACHE_FILE)) SD.remove(GIF_CACHE_FILE);

    File cacheFile = SD.open(GIF_CACHE_FILE, FILE_WRITE);
    if (!cacheFile) {
        Serial.println("ERROR CRÍTICO: No se puede escribir en SD.");
        return;
    }

    hayGifsEnCache = false;
    // Escaneamos cada carpeta activa
    for (const String& path : config.activeFolders) { 
        scanGifDirectory(cacheFile, path);
    }
    
    cacheFile.close();

    // 6. Guardar la nueva firma
    File newSigFile = SD.open(GIF_CACHE_SIG, FILE_WRITE);
    if (newSigFile) {
        newSigFile.print(currentSignature);
        newSigFile.close();
    }
    
    // Reseteamos la posición de lectura al inicio del nuevo archivo
    gifCachePosition = 0;
    Serial.println("Indice generado correctamente en SD.");
}

// --- Funciones de Modos de Reproducción ---
// Variables globales necesarias para el loop
extern unsigned long lastFrameTime; // Declarar esta variable en el ámbito global si aún no existe
// extern WebServer server; // Asumiendo que el objeto server es global

String obtenerSiguienteGifSD() {
    if (!SD.exists(GIF_CACHE_FILE)) return "";

    File cacheFile = SD.open(GIF_CACHE_FILE, FILE_READ);
    if (!cacheFile) return "";

    // --- LÓGICA MODO ALEATORIO (HARDWARE RNG) ---
    if (config.randomMode) {
        uint32_t fileSize = cacheFile.size();
        
        if (fileSize > 15) { 
            // Usamos esp_random()
            // El operador '%' ajusta ese número gigante al tamaño del archivo.
            uint32_t randomPos = esp_random() % (fileSize - 10);
            
            cacheFile.seek(randomPos);

            // Si no estamos al principio, avanzamos hasta el siguiente salto de línea
            if (randomPos != 0) {
                while (cacheFile.available()) {
                    char c = cacheFile.read();
                    if (c == '\n') break; 
                }
            }
        }
    } 
    // --- LÓGICA MODO SECUENCIAL ---
    else {
        if (gifCachePosition > 0) {
            cacheFile.seek(gifCachePosition);
        }
    }

    // --- LECTURA COMÚN ---
    if (!cacheFile.available()) {
        cacheFile.seek(0);
        if (!config.randomMode) gifCachePosition = 0;
    }

    String gifPath = cacheFile.readStringUntil('\n');
    gifPath.trim();

    if (!config.randomMode) {
        gifCachePosition = cacheFile.position();
    }
    
    cacheFile.close();
    return gifPath;
}

void ejecutarModoGif() {
    if (!display) return; 
    
    // 1. CAMBIO: Verificación usando la nueva bandera 'hayGifsEnCache' en lugar del vector
    if (!sdMontada || !hayGifsEnCache) {
        mostrarMensaje(!sdMontada ? "SD ERROR" : "NO GIFS");
        delay(200);
        // Si la SD está montada pero no hay GIFs detectados, intentamos regenerar la lista
        if (sdMontada) listarArchivosGif();
        return;
    }

    // 2. CAMBIO: ELIMINADO el bloque de rotación de índices del vector.
    // En su lugar, pedimos la siguiente ruta directamente al archivo de la SD.
    String gifPath = obtenerSiguienteGifSD(); 
    
    // Si la función devuelve vacío (fin de archivo o error), salimos y reintentamos en el siguiente ciclo
    if (gifPath == "") {
        return;
    }
    
    // Bucle de repetición del GIF (config.gifRepeats)
    for (int rep = 0; rep < config.gifRepeats; ++rep) { 
        // SALIDA SI CAMBIA EL MODO O SE APAGA
        if (config.playMode != 0 || !config.powerState || enModoGestion) return;
        
        if (gif.open(gifPath.c_str(), GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
            
            x_offset = (128 - gif.getCanvasWidth()) / 2; 
            y_offset = (32 - gif.getCanvasHeight()) / 2; 

            display->clearScreen(); 

            int delayMs;
            
            // Bucle principal de reproducción de frames
            while (gif.playFrame(true, &delayMs)) {
                
                // Verificación de Apagado / Modo Gestión
                if (!config.powerState || enModoGestion || config.playMode != 0 || recargarGifsPendiente) {
                    gif.close();
                    return; // Sale inmediatamente de la reproducción
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
            // Si falla un archivo específico, no detenemos todo, solo mostramos error brevemente
            mostrarMensaje("Error GIF", display->color565(255, 255, 0));
            
            unsigned long start = millis();
            while (millis() - start < 1000) {
                 if (!config.powerState || enModoGestion) return;
                 server.handleClient();
                 yield();
            }
        }
    }

    // 3. CAMBIO: ELIMINADO currentGifIndex++; 
    // La función obtenerSiguienteGifSD() ya avanza el cursor automáticamente.
}

void ejecutarModoTexto() {
    if (!display) return; 

    // 1. Configuración de color
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

        // Calculamos el ancho real en píxeles 
        int16_t x1, y1;
        uint16_t w, h;
        display->getTextBounds(config.slidingText, 0, 0, &x1, &y1, &w, &h);

        // Si el texto terminó de pasar
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

// 2. Ejecutar Modo Reloj Completo HH:MM:SS
void ejecutarModoReloj() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    static int minAnterior = -1;
    static int modoAnterior = -1;
    static int estiloAnterior = -1;

    // 1. GESTIÓN DE POSICIONAMIENTO DINÁMICO
    // Si MQTT activo: startY = 9 (reloj abajo). Si MQTT apagado: startY = 6 (reloj arriba).
    int startY = config.mqtt_enabled ? 9 : 6;
    
    // Si cambia el minuto o el modo, limpiamos TODO para evitar residuos
    if (modoAnterior != config.playMode || timeinfo.tm_min != minAnterior || estiloAnterior != config.clockEffect) {
        display->fillScreen(0); 
        minAnterior = timeinfo.tm_min;
        modoAnterior = config.playMode;
        estiloAnterior = config.clockEffect; 
        Serial.println("Reloj: Refresco por cambio de estilo o tiempo.");
    }

    // 2. PREPARACIÓN DEL RELOJ
    char fullTimeStr[9]; 
    strftime(fullTimeStr, sizeof(fullTimeStr), "%H:%M:%S", &timeinfo);
     
    int startX = 128; 
    uint32_t ms = millis();

    // 3. DIBUJAR LOS DÍGITOS DEL RELOJ
    for (int i = 0; i < 8; i++) {
        int xPos = startX + (i * 16);
        
        // Limpiamos solo el rectángulo donde va este dígito (evita parpadeo global)
        display->fillRect(xPos, startY, 15, 24, 0); 

        uint16_t color;
        switch (config.clockEffect) {
            case 0: color = hsvTo565((ms / 25 + xPos) % 360, 255, 255); break;
            case 1: color = hsvTo565((ms / 50 + (i * 40)) % 360, 255, 255); break;
            case 2: // Solid Neon
                {
                    uint8_t r = (config.clockColor >> 16) & 0xFF;
                    uint8_t g = (config.clockColor >> 8) & 0xFF;
                    uint8_t b = config.clockColor & 0xFF;
                    color = display->color565(r, g, b); 
                }
                break;
            case 3: color = hsvTo565((int)(5 + sin(ms / 500.0) * 10) % 360, 255, 200); break;
            case 4: // Pulse Breath
                {
                    float factor = 0.85 + (0.15 * sin(ms / 159.0)); 
                    uint8_t r = (config.clockColor >> 16) & 0xFF;
                    uint8_t g = (config.clockColor >> 8) & 0xFF;
                    uint8_t b = config.clockColor & 0xFF;
                    color = display->color565(r * factor, g * factor, b * factor);
                }
                break;
            case 5: // Matrix Digital
                {
                    int brilliance = 195 + (sin((ms / 200.0) + i) * 60);
                    if (brilliance < 80) brilliance = 80; 
                    color = display->color565(0, brilliance, 0);
                }
                break;
            case 6: // Gradient 50%
                {
                    uint8_t r1 = (config.clockColor >> 16) & 0xFF;
                    uint8_t g1 = (config.clockColor >> 8) & 0xFF;
                    uint8_t b1 = config.clockColor & 0xFF;
                    float ratio = i / 14.0;
                    color = display->color565(r1 + (255 - r1) * ratio, g1 + (255 - g1) * ratio, b1 + (255 - b1) * ratio);
                }
                break;
            case 7: // Gradient 80%
                {
                    uint8_t r1 = (config.clockColor >> 16) & 0xFF;
                    uint8_t g1 = (config.clockColor >> 8) & 0xFF;
                    uint8_t b1 = config.clockColor & 0xFF;
                    float ratio = i / 8.75;
                    color = display->color565(r1 + (255 - r1) * ratio, g1 + (255 - g1) * ratio, b1 + (255 - b1) * ratio);
                }
                break;    
            default: color = 0xFFFF; break;
        }

        // Dibujamos el dígito
        if (fullTimeStr[i] >= '0' && fullTimeStr[i] <= '9') {
            drawCustomChar(xPos, startY, fullTimeStr[i] - '0', color, 3);
        } else if (fullTimeStr[i] == ':') {
            if (timeinfo.tm_sec % 2 == 0) {
                display->fillRect(xPos + 6, startY + 6, 3, 3, color);
                display->fillRect(xPos + 6, startY + 15, 3, 3, color);
            }
        }
    }

    // 4. DIBUJAR LA BARRA DE NOTIFICACIONES (Solo si MQTT está activo)
    if (config.mqtt_enabled) {
        dibujarBarraNotificaciones();
    }
}

void ejecutarModoArcade() {
    // Si entramos aquí pero ya estamos en modo gestión, salimos antes de abrir nada
    if (enModoGestion) return;

    char pathChar[128];
    rutaGifArcade.toCharArray(pathChar, sizeof(pathChar));
    
    if (gif.open(pathChar, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
        // El bucle principal del GIF
        while (gif.playFrame(true, NULL)) {
            
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
    
    // --- SELECT DE MODOS ---
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

    // --- SELECTOR DE ESTILO DE RELOJ --- 
    String clockStyleConfig = "{\"name\":\"Estilo Reloj\",\"stat_t\":\"retropixel/" + technicalID + "/state/clock_style\",\"cmd_t\":\"retropixel/" + technicalID + "/cmd/clock_style\",\"options\":[\"Rainbow Flow\",\"Static Rainbow\",\"Solid Neon\",\"Night Fire\",\"Pulse Breath\",\"Matrix Digital\",\"Gradient 50%\",\"Gradient 80%\"],\"uniq_id\":\"" + technicalID + "_clock_style\"" + deviceJSON + "}";
    mqttClient.publish(("homeassistant/select/" + technicalID + "/clock_style/config").c_str(), clockStyleConfig.c_str(), true);

    // --- RUEDA DE COLOR PARA EL RELOJ ---
    String clockLightConfig = "{\"name\":\"Color Reloj\",\"stat_t\":\"retropixel/" + technicalID + "/state/clock_color\",\"cmd_t\":\"retropixel/" + technicalID + "/cmd/clock_color\",\"rgb_cmd_t\":\"retropixel/" + technicalID + "/cmd/clock_color/set\",\"rgb_stat_t\":\"retropixel/" + technicalID + "/state/clock_color/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"uniq_id\":\"" + technicalID + "_clock_rgb\"" + deviceJSON + "}";  
    mqttClient.publish(("homeassistant/light/" + technicalID + "/clock_color/config").c_str(), clockLightConfig.c_str(), true);

    // --- RUEDA DE COLOR PARA EL TEXTO ---
    String textLightConfig = "{\"name\":\"Color Texto\",\"stat_t\":\"retropixel/" + technicalID + "/state/text_color\",\"cmd_t\":\"retropixel/" + technicalID + "/cmd/text_color\",\"rgb_cmd_t\":\"retropixel/" + technicalID + "/cmd/text_color/set\",\"rgb_stat_t\":\"retropixel/" + technicalID + "/state/text_color/set\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"uniq_id\":\"" + technicalID + "_text_rgb\"" + deviceJSON + "}";    
    mqttClient.publish(("homeassistant/light/" + technicalID + "/text_color/config").c_str(), textLightConfig.c_str(), true);

    Serial.print("Discovery enviado para: ");
    Serial.println(friendlyName);
}

void reconnectMQTT() {
    // Si el MQTT no está activado en la web, salimos inmediatamente
    if (!config.mqtt_enabled) return;

    int reintentos = 0;
    const int maxReintentos = 5;
    String technicalID = "retropixel_" + chipID;

    while (!mqttClient.connected() && reintentos < maxReintentos) {
        Serial.printf("Intentando conexión MQTT (Intento %d/%d)...\n", reintentos + 1, maxReintentos);
        
        // Configuramos el servidor con los datos actuales de la web
        mqttClient.setServer(config.mqtt_host, config.mqtt_port);

        // Intentamos conectar usando el technicalID como ClientID único
        if (mqttClient.connect(technicalID.c_str(), config.mqtt_user, config.mqtt_pass)) {
            Serial.println("¡Conectado a MQTT con éxito!");
            
            // 1. SUSCRIPCIÓN DINÁMICA CON COMODÍN
            // Escuchamos todo lo que venga de HA hacia nuestro ID
            mqttClient.subscribe(("retropixel/" + technicalID + "/cmd/#").c_str());
            Serial.println("Suscrito a: retropixel/" + technicalID + "/cmd/#");

            // 2. Enviamos el Discovery para que aparezca/se actualice en HA
            sendMQTTDiscovery();

             // 3. Sincronizar el estado actual
            syncMQTTState();

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
    // Convertir el mensaje recibido a String
    String message = "";
    for (int i = 0; i < length; i++) message += (char)payload[i];

    // Creamos el prefijo dinámico para responder el estado al tópico correcto
    String technicalID = "retropixel_" + chipID;
    String stateTopicPrefix = "retropixel/" + technicalID + "/state/";
    String strTopic = String(topic);

    Serial.println("MQTT Recibido [" + strTopic + "]: " + message);

    // ----------------------------------------------------
    // 1. CONTROL DE MODO (GIFs, Texto, Reloj, Arcade)
    // ----------------------------------------------------
    if (strTopic.endsWith("/cmd/mode")) {
        int modoAnterior = config.playMode; // Guardamos el modo que había

        if (message == "GIFs")      config.playMode = 0;
        else if (message == "Texto") config.playMode = 1;
        else if (message == "Reloj") config.playMode = 2;
        else if (message == "Arcade") config.playMode = 3;

        if (config.playMode == 0) {
            gif.close(); 
            
            // CRÍTICO: Si la lista de GIFs está vacía, hay que llenarla
            // o el modo GIF entrará y saldrá sin dibujar nada (pantalla negra).
            if (!hayGifsEnCache) {
                Serial.println("MQTT: Lista vacía, escaneando SD...");
                listarArchivosGif(); 
            }

            // Aseguramos que el índice sea válido tras el escaneo
            gifCachePosition = 0;        
            Serial.println("MQTT: Modo GIF activado.");
        }

        display->fillScreen(0);
        savePlaybackConfig();
        mqttClient.publish((stateTopicPrefix + "mode").c_str(), message.c_str(), true);
        Serial.println("MQTT: Modo cambiado y reseteado a: " + message);
       
    }
    // ----------------------------------------------------
    // 2. CONTROL DE ENCENDIDO / APAGADO
    // ----------------------------------------------------
    else if (strTopic.endsWith("/cmd/power")) {
        config.powerState = (message == "ON");
        if (!config.powerState && display) display->fillScreen(0);
        mqttClient.publish((stateTopicPrefix + "power").c_str(), message.c_str(), true);
        savePlaybackConfig();
    }
    // ----------------------------------------------------
    // 3. CONTROL DE BRILLO (0 - 255)
    // ----------------------------------------------------
    else if (strTopic.endsWith("/cmd/bright")) {
        config.brightness = constrain(message.toInt(), 0, 255);
        if (display) display->setBrightness8(config.brightness);
        mqttClient.publish((stateTopicPrefix + "bright").c_str(), String(config.brightness).c_str(), true);
        savePlaybackConfig();
    }
    // ----------------------------------------------------
    // 4. CONTROL DE TEXTO PERSONALIZADO
    // ----------------------------------------------------
    else if (strTopic.endsWith("/cmd/text")) {
        config.slidingText = message;
        xPosMarquesina = display->width();
        savePlaybackConfig();
        mqttClient.publish((stateTopicPrefix + "text").c_str(), message.c_str(), true);
        Serial.println("Nuevo texto MQTT (slidingText): " + message);
    }
    // ----------------------------------------------------
    // 5. NOTIFICACIONES Y TIEMPO
    // ---------------------------------------------------- 
    else if (strTopic.endsWith("/cmd/temp")) {
         mqtt_temp = message; 
         Serial.println("MQTT Temp recibida: " + mqtt_temp);
    }
    else if (strTopic.endsWith("/cmd/weather")) { 
        mqtt_weather_icon = message.toInt(); 
        Serial.println("MQTT Clima (Icon ID): " + String(mqtt_weather_icon));
    }
    else if (strTopic.endsWith("/cmd/notify")) { 
        mqtt_custom_msg = message; 
        Serial.println("MQTT Notificación: " + mqtt_custom_msg);
    }
    /// ----------------------------------------------------
    // 6. CONTROL DE ESTILO DEL RELOJ
    // ----------------------------------------------------
    if (strTopic.endsWith("/cmd/clock_style")) {
        message.trim();
        int seleccionado = -1;
        if (message.equalsIgnoreCase("Rainbow Flow"))      seleccionado = 0;
        else if (message.equalsIgnoreCase("Static Rainbow")) seleccionado = 1;
        else if (message.equalsIgnoreCase("Solid Neon"))     seleccionado = 2;
        else if (message.equalsIgnoreCase("Night Fire"))     seleccionado = 3;
        else if (message.equalsIgnoreCase("Pulse Breath"))   seleccionado = 4;
        else if (message.equalsIgnoreCase("Matrix Digital")) seleccionado = 5;
        else if (message.equalsIgnoreCase("Gradient 50%"))   seleccionado = 6;
        else if (message.equalsIgnoreCase("Gradient 80%"))   seleccionado = 7;

        if (seleccionado != -1) {
            config.clockEffect = seleccionado;
            saveSystemConfig();
            mqttClient.publish((stateTopicPrefix + "clock_style").c_str(), message.c_str(), true);
            if(config.playMode == 2) display->fillScreen(0);
            Serial.printf("MQTT Estilo ID %d aplicado.\n", seleccionado);
        }
    }
    // ----------------------------------------------------
    // 7. CONTROL COLOR RELOJ (RUEDA RGB)
    // ----------------------------------------------------
    if (strTopic.endsWith("/cmd/clock_color/set")) {
        int r, g, b;
        if (sscanf(message.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
            config.clockColor = (uint32_t)((r << 16) | (g << 8) | b);
            mqttClient.publish((stateTopicPrefix + "clock_color/set").c_str(), message.c_str(), true);
            mqttClient.publish((stateTopicPrefix + "clock_color").c_str(), "ON", true);
            saveSystemConfig();
            if(config.playMode == 2) display->fillScreen(0);
        }
    }
    // ----------------------------------------------------
    // 8. CONTROL COLOR TEXTO (RUEDA RGB)
    // ----------------------------------------------------
    if (strTopic.endsWith("/cmd/text_color/set")) {
        int r, g, b;
        if (sscanf(message.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
            config.slidingTextColor = (uint32_t)((r << 16) | (g << 8) | b);
            mqttClient.publish((stateTopicPrefix + "text_color/set").c_str(), message.c_str(), true);
            saveSystemConfig();
        }
    }

    // ----------------------------------------------------
    // 9. BARRA DE NOTIFICACIONES
    // ----------------------------------------------------
    if (config.playMode == 2 && (strTopic.indexOf("temp") != -1 || strTopic.indexOf("weather") != -1 || strTopic.indexOf("notify") != -1)) {
        dibujarBarraNotificaciones();
    }
}

void syncMQTTState() {
    // 1. Verificación inicial: Si no hay MQTT o no está conectado, salimos
    if (!config.mqtt_enabled || !mqttClient.connected()) return;

    // 2. Definimos las variables necesarias para construir los tópicos
    String technicalID = "retropixel_" + chipID;
    String stateTopicPrefix = "retropixel/" + technicalID + "/state/";

    // 3. Mapeo del modo actual
    String modoTexto;
    switch (config.playMode) {
        case 0:  modoTexto = "GIFs";   break;
        case 1:  modoTexto = "Texto";  break;
        case 2:  modoTexto = "Reloj";  break;
        case 3:  modoTexto = "Arcade"; break;
        default: modoTexto = "GIFs";   break;
    }


    // 4. Preparar nombres de efectos del reloj y brillo
    String brillo = String(config.brightness);
    String encendido = config.powerState ? "ON" : "OFF";

    const char* effectNames[] = {"Rainbow Flow", "Static Rainbow", "Solid Neon", "Night Fire", "Pulse Breath", "Matrix Digital", "Gradient 50%", "Gradient 80%"};
    String currentEffectName = "Rainbow Flow";
    if(config.clockEffect >= 0 && config.clockEffect <= 7) {
        currentEffectName = String(effectNames[config.clockEffect]);
    }
    
    // 5. Función auxiliar para convertir color a formato R,G,B para Home Assistant
    auto toRGBStr = [](uint32_t c) {
        return String((c >> 16) & 0xFF) + "," + String((c >> 8) & 0xFF) + "," + String(c & 0xFF);
    };

    // 6. Enviar estados básicos
    mqttClient.publish((stateTopicPrefix + "mode").c_str(), modoTexto.c_str(), true);
    mqttClient.publish((stateTopicPrefix + "bright").c_str(), String(config.brightness).c_str(), true);
    mqttClient.publish((stateTopicPrefix + "power").c_str(), (config.powerState ? "ON" : "OFF"), true);
    mqttClient.publish((stateTopicPrefix + "text").c_str(), config.slidingText.c_str(), true);
    mqttClient.publish((stateTopicPrefix + "clock_style").c_str(), currentEffectName.c_str(), true);

    // 7. Sincronizar Ruedas de Color (Luces en HA)
    mqttClient.publish((stateTopicPrefix + "clock_color").c_str(), "ON", true);
    mqttClient.publish((stateTopicPrefix + "clock_color/set").c_str(), toRGBStr(config.clockColor).c_str(), true);
    mqttClient.publish((stateTopicPrefix + "text_color").c_str(), "ON", true);
    mqttClient.publish((stateTopicPrefix + "text_color/set").c_str(), toRGBStr(config.slidingTextColor).c_str(), true);
    
    Serial.println("MQTT: Sincronización de estado enviada a HA (Modo: " + modoTexto + ")");
}

// --- TAREAS DUAL CORE ---
void TaskDisplay(void * pvParameters);

// ====================================================================
//                             SETUP Y LOOP
// ====================================================================

void setup() {
    Serial.begin(115200);

    // --- 1. OBTENER ID ÚNICO (VERSIÓN UNIVERSAL CORE 3.X) ---
    // Leemos la MAC directamente de los eFuses del hardware (64 bits)
    uint64_t chipid_raw = ESP.getEfuseMac(); 
    
    // Extraemos los últimos 3 bytes (los últimos 6 caracteres hexadecimales)
    // Desplazamos 24 bits para obtener la parte final de la MAC
    uint32_t chipid_num = (uint32_t)(chipid_raw >> 24); 

    char mac_str[7];
    sprintf(mac_str, "%06X", chipid_num); // Convertimos a texto Hexadecimal de 6 cifras
    chipID = String(mac_str);
    chipID.toUpperCase();
    
    Serial.print(">> ID Único del Dispositivo calculado al inicio: ");
    Serial.println(chipID);

    // --- 2. SEMÁFORO (MUTEX) ---
    sdMutex = xSemaphoreCreateMutex();
    if (sdMutex == NULL) {
        Serial.println("Error al crear el Semáforo");
    }
       
    if (!SPIFFS.begin(true)) {
        Serial.println("Error al montar SPIFFS.");
    }

    loadConfig();

    // --- 3. INICIALIZACIÓN DE LA SD --- 
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

    // --- 4. CONEXIÓN WIFI --- 
    if (config.device_name[0] == '\0') {
        strncpy(config.device_name, DEVICE_NAME_DEFAULT, sizeof(config.device_name) - 1);
        config.device_name[sizeof(config.device_name) - 1] = '\0'; 
    }

    wm.setHostname(config.device_name);

    // CONFIGURACIÓN WM: Hacer que el portal no bloquee el resto del código
    wm.setConfigPortalBlocking(false); 
    //wm.setConfigPortalTimeout(180); // 3 minutos de espera, luego sigue

    if (config.WifiOffMode) {
        // ESCENARIO A: Modo Offline configurado
        Serial.println("Modo Offline activado. Generando red propia...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Retro Pixel LED");
        config.slidingText = "Modo Offline Activo - Conectate a la IP: 192.168.4.1 para usar Retro Pixel LED";
    } else {
        // ESCENARIO B: Intento Online
        WiFi.mode(WIFI_AP_STA); 
        config.slidingText = "MODO ONLINE - Buscando WiFi...";

        // Configuramos el timeout a 3 minutos (180 seg)
        wm.setConfigPortalTimeout(180);
        wm.setConfigPortalBlocking(false);

        // Intentamos conectar
        if (!wm.autoConnect("Retro Pixel LED")) {
            Serial.println("Portal activo. Esperando configuracion o timeout...");
            config.slidingText = "Conectate a la red WiFi Retro Pixel LED para configurarlo - IP:192.168.4.1 - Si a los 3 minutos no se ha conectado a una red WiFi se desactivara el AP. Reinicia Retro Pixel LED para que vuelva activar el AP de configuracion WiFi";
        } else {
            // Si conecta a la primera:
            Serial.println("Conectado con éxito.");
            config.slidingText = "Retro Pixel LED v" + String(FIRMWARE_VERSION) + " - IP: " + WiFi.localIP().toString();
        }
    }

    // --- 5. INICIALIZACIÓN DE LA MATRIZ LED --- 
    const int FINAL_MATRIX_WIDTH = PANEL_RES_X * config.panelChain;

    HUB75_I2S_CFG::i2s_pins pin_config = {
        R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
        A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
        LAT_PIN, OE_PIN, CLK_PIN
    };

    HUB75_I2S_CFG matrix_config(
        FINAL_MATRIX_WIDTH, 
        MATRIX_HEIGHT,      
        config.panelChain,  
        pin_config          
    );

    // --- 6. APLICACIÓN DE AJUSTES AVANZADOS  ---
    
    // 6.1. Velocidad I2S (Mapeo del índice 0-3 a las constantes de la librería)
    if (config.i2sSpeed == 0)      matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_8M;
    else if (config.i2sSpeed == 1) matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_10M;
    else if (config.i2sSpeed == 2) matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_16M;
    else if (config.i2sSpeed == 3) matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_20M;
    else matrix_config.i2sspeed = HUB75_I2S_CFG::HZ_10M;

    // 6.2. Latch Blanking (Anti-Ghosting)
    // El rango válido 1-4.
    matrix_config.latch_blanking = config.latchBlanking;

    // 6.3. Tasa de Refresco Mínima
    // Si por error viene un 0, forzamos 60Hz.
    if (config.minRefreshRate < 30) config.minRefreshRate = 60;
    matrix_config.min_refresh_rate = config.minRefreshRate;

    // 6.4. Doble Buffer (Siempre activo para GIFs)
    //matrix_config.double_buff = true;

    // 6.5. Sincronización
    matrix_config.clkphase = false;

    // Crear el objeto Display con la nueva configuración
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
            // Activamos la bandera para que el loop() lo haga en segundo plano.
            recargarGifsPendiente = true; 
            Serial.println("Carga de GIFs programada...");
        }
        delay(1000);
    } else {
        Serial.println("ERROR: No se pudo asignar memoria para la matriz LED.");
    }
    
    initTime();
    
    // --- 7. CONFIGURACIÓN DE RUTAS DEL SERVIDOR WEB ---
    server.on("/", HTTP_GET, handleRoot);
    server.on("/power", HTTP_GET, handlePower);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/config", HTTP_GET, handleConfig); 
    server.on("/save_config", HTTP_GET, handleSaveConfig); 
    server.on("/restart", HTTP_GET, handleRestart); 
    server.on("/factory_reset", HTTP_GET, handleFactoryReset);
    
    // RUTAS GESTIÓN DE ARCHIVOS
    server.on("/file_manager", HTTP_GET, handleFileManager);
    server.on("/delete", HTTP_POST, handleFileDelete);
    server.on("/create_dir", HTTP_POST, handleCreateDir); 

    // RUTA PARA BATOCERA ---
    server.on("/batocera", HTTP_GET, []() {
    if (server.hasArg("s") && server.hasArg("g")) {
        // A. BLOQUEO: Pausamos para liberar la SD
        interrumpirReproduccion = true; 
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
        interrumpirReproduccion = false; 
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

    // --- 7. MQTT ---
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
        &displayTaskHandle,          
        1              
    );
    
    Serial.println("Servidor HTTP iniciado.");
}


bool modoTemporalActivado = false;

void loop() {
    // 1. Si el portal de WiFiManager está funcionando
    if (wm.getConfigPortalActive()) {
        wm.process();
        modoTemporalActivado = false; // Resetear bandera si el portal está activo
    } 
    else {
        // 2. Gestión de desconexión inteligente (Cuando falla el WiFi o el AP expira)
        if (WiFi.status() != WL_CONNECTED && !config.WifiOffMode && !modoTemporalActivado) {
            
            Serial.println(">> WiFi no detectado. Esperando 3s por seguridad...");
            delay(3000); 

            if (!config.WifiOffMode) { 
                Serial.println(">> Entrando en Modo Offline temporal.");
                config.WifiOffMode = true; 
                modoTemporalActivado = true;
				config.slidingText = "Se ha desactivado el AP de configuracion por no haber configurado la red WiFi. Si quieres configurar la red WiFi REINICIAME!";

                // Forzamos el modo AP_STA para que aunque el portal cierre, el servidor web siga escuchando en la IP del AP o de la última red.
                WiFi.mode(WIFI_AP_STA);
            }
        }

        // 3. Procesar servidor web
        server.handleClient();

        // 3.1 Reintento de reconexión automática (Background)
        // Si estamos en modo temporal (porque el router estaba apagado), probamos cada 60s
        static unsigned long ultimaPruebaWiFi = 0;
        if (modoTemporalActivado && (millis() - ultimaPruebaWiFi > 60000)) {
            ultimaPruebaWiFi = millis();
            Serial.println(">> Reintentando conexión WiFi...");
            WiFi.begin(); // Intenta conectar con las credenciales que ya conoce
        }


        // 3.2 Gestión de éxito en la conexión
        if (WiFi.status() == WL_CONNECTED) {
            // Si recuperamos el WiFi, desactivamos el modo temporal y el bloqueo
            if (modoTemporalActivado) {
                Serial.println(">> WiFi recuperado con éxito.");
                modoTemporalActivado = false;
            }
        
            if (config.mqtt_enabled) {
                // 1. Si NO estamos conectados, intentamos conectar
                if (!mqttClient.connected()) {
                    reconnectMQTT(); 
                }
                // 2. Mantenemos el bucle de escucha
                mqttClient.loop();
            }
            
        }
    }

    // 4. Gestión de GIFs
    if (recargarGifsPendiente) {
        delay(200); 
        listarArchivosGif();
        recargarGifsPendiente = false; 
    }
}

// --- TAREA PARA EL NÚCLEO 1 (PANEL LED) ---
void TaskDisplay(void * pvParameters) {
    for (;;) {
        // 1. Comprobamos si Hay que listar GIFs
        // Ponemos esto lo primero para que interrumpa cualquier cambio de configuración
        if (recargarGifsPendiente) {
            if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500))) {
                listarArchivosGif(); // Esta función ahora limpia pantalla y pone "LISTANDO..."
                recargarGifsPendiente = false;
                xSemaphoreGive(sdMutex);
            }
            // No hacemos 'continue' aquí para que pueda entrar en el modo GIF inmediatamente después
        }

        // 2. Si el panel está apagado (Ahorro de Energía Dinámico)
        if (config.powerState == false) {
            display->fillScreen(0);

            // Si la frecuencia es distinta de 80MHz, la bajamos
            if (getCpuFrequencyMhz() != 80) {
                setCpuFrequencyMhz(80);
                Serial.println(F("[POWER] Modo ahorro: CPU a 80MHz (WiFi Activo)"));
            }

            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        } else {
            // Si encendemos, restauramos los 240MHz
            if (getCpuFrequencyMhz() != 240) {
                setCpuFrequencyMhz(240);
                Serial.println(F("[POWER] Modo Performance: CPU a 240MHz"));
            }

        }

        // 3. Bloque de Seguridad: Gestor de Archivos o Interrupción Silenciosa
        // Detenemos los GIFs y mostramos mensaje de "FILES MODE"
        if (enModoGestion) {
            display->fillScreen(0);
            display->setTextColor(display->color565(0, 242, 255)); // Cian Cyberpunk
            display->setCursor(177, 7);
            display->print("FILES");
            display->setCursor(180, 17);
            display->print("MODE");
            
            // Dibujamos una pequeña línea de progreso o adorno Cyberpunk
            display->drawFastHLine(160, 27, 64, display->color565(255, 0, 100)); 
            
            vTaskDelay(pdMS_TO_TICKS(500)); // Esperamos medio segundo antes de volver a chequear
            continue; // Saltamos el resto del código (no se ejecutan GIFs)
        }

        // Interrupción silenciosa para cambios rápidos (Arcade)
        if (interrumpirReproduccion) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 4. Ejecución Normal con Semáforo
        if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100))) {
            if (display) { 
        
                // A) LÓGICA DE RELOJ TEMPORAL (Si está activo)
                if (modoRelojTemporalActivo) {
                    // Si el tiempo ha expirado, volvemos a modo normal
                    if (millis() - tiempoInicioRelojForzado >= DURACION_RELOJ_MS) {
                        modoRelojTemporalActivo = false;
                        contadorGifsReproducidos = 0; // Reiniciamos contador
                    } else {
                        ejecutarModoReloj(); // Muestra el reloj
                    }
                } 

                // B) LÓGICA DE MODOS NORMALES
                else {
                    switch (config.playMode) {
                        case 0: // Modo Galería
                            ejecutarModoGif(); 
                    
                            // Si el usuario activó la función, sumamos al contador
                            if (config.autoClock) {
                                contadorGifsReproducidos++;
                                if (contadorGifsReproducidos >= config.clockInterval) {
                                    modoRelojTemporalActivo = true;
                                    tiempoInicioRelojForzado = millis();
                                }
                            }
                            break;
                    
                        case 1: ejecutarModoTexto(); break;
                        case 2: ejecutarModoReloj(); break;
                        case 3: ejecutarModoArcade(); break;
                    }
                }
            }
            xSemaphoreGive(sdMutex);
        }

        // Un pequeño respiro de 1ms para que el Watchdog del sistema esté feliz
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}