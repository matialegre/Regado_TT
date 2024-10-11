#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266mDNS.h>

// Pines de los relés
#define RELAY1 D1
#define RELAY2 D2

// Servidor web
ESP8266WebServer server(80);

// Variables de riego
int wateringDuration[7] = {0, 0, 0, 0, 0, 0, 0};  // Duración por día de la semana
int wateringInterval[7] = {0, 0, 0, 0, 0, 0, 0};  // Intervalo por día de la semana
int startHour[7] = {0, 0, 0, 0, 0, 0, 0};         // Hora de inicio por día
int startMinute[7] = {0, 0, 0, 0, 0, 0, 0};       // Minuto de inicio por día
int endHour[7] = {0, 0, 0, 0, 0, 0, 0};           // Hora de fin por día
int endMinute[7] = {0, 0, 0, 0, 0, 0, 0};         // Minuto de fin por día
bool daysOfWeek[7] = {false, false, false, false, false, false, false};  // Activo por día
bool manualMode = false;  // Modo manual
bool valve1State = false;  // Estado de la válvula 1
bool valve2State = false;  // Estado de la válvula 2
unsigned long lastSwitchTime = 0;  // Última vez que se cambió el estado de la válvula
bool isWatering = false;  // Estado actual: riego o pausa
int remainingTime = 0;  // Tiempo restante para el riego o pausa actua
// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);  // UTC-3 para Argentina

// Función para manejar la edición de un día
void handleEditDay() {
  int day = server.arg("day").toInt();
  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>TWISTED TRANSISTOR - Editar Día</title><link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css'><style>";
  html += "body { font-family: Arial, sans-serif; background-color: #f0f4f7; color: #333; }";
  html += ".container { max-width: 800px; margin: 20px auto; padding: 20px; background-color: #fff; border-radius: 10px; box-shadow: 0px 0px 15px rgba(0, 0, 0, 0.1); }";
  html += ".config-item { padding: 10px; margin: 10px 0; background-color: #e0f7fa; text-align: center; border-radius: 5px; }";
  html += "button { margin: 10px; padding: 10px; background-color: #00796b; color: white; border: none; border-radius: 5px; cursor: pointer; transition: background-color 0.3s ease; }";
  html += "button:hover { background-color: #004d40; }";
  html += "</style></head><body><div class='container'><h1>Editar Día: " + String(day) + "</h1>";

  // Formulario para editar los parámetros del día seleccionado
  html += "<form action='/saveday' method='post'>";
  html += "<input type='hidden' name='day' value='" + String(day) + "'>";
  html += "<div class='config-item'>Duración de riego (minutos): <input type='number' name='duration' value='" + String(wateringDuration[day]) + "'></div>";
  html += "<div class='config-item'>Intervalo de riego (minutos): <input type='number' name='interval' value='" + String(wateringInterval[day]) + "'></div>";
  html += "<div class='config-item'>Hora de inicio: <input type='number' name='startHour' value='" + String(startHour[day]) + "' min='0' max='23'> : <input type='number' name='startMinute' value='" + String(startMinute[day]) + "' min='0' max='59'></div>";
  html += "<div class='config-item'>Hora de fin: <input type='number' name='endHour' value='" + String(endHour[day]) + "' min='0' max='23'> : <input type='number' name='endMinute' value='" + String(endMinute[day]) + "' min='0' max='59'></div>";
  html += "<div class='config-item'><label><input type='checkbox' name='active'" + String(daysOfWeek[day] ? " checked" : "") + "> Activar este día</label></div>";
  html += "<div><button type='submit'>Guardar</button></div>";
  html += "</form>";
  html += "<button onclick=\"location.href='/'\">Volver</button>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}
// Función para guardar los parámetros del día
 void showConfig() {
  String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";

  // Título y estilos
  html += "<title>TWISTED TRANSISTOR - Sistema de Riego</title>";
  html += "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css'>";
  html += "<style>";
  
  // Imagen de fondo y estilos generales
  html += "body { font-family: Arial, sans-serif; background-image: url('https://www.fundacionaquae.org/wp-content/uploads/2020/03/regadio2.jpg'); background-size: cover; background-position: center; color: #333; }";
  html += ".container { max-width: 800px; margin: 20px auto; padding: 20px; background-color: rgba(255, 255, 255, 0.8); border-radius: 10px; box-shadow: 0px 0px 15px rgba(0, 0, 0, 0.1); }";
  html += "button { margin: 10px; padding: 10px; background-color: #00796b; color: white; border: none; border-radius: 5px; cursor: pointer; transition: background-color 0.3s ease; }";
  html += "button:hover { background-color: #004d40; }";
  html += ".day-button { margin: 5px; padding: 10px; border-radius: 5px; color: white; cursor: pointer; transition: background-color 0.3s ease; }";
  html += ".day-active { background-color: green; } .day-inactive { background-color: red; }";
  html += ".day-button:hover { background-color: #555; }";
  html += ".config-item { padding: 10px; margin: 10px 0; background-color: #e0f7fa; text-align: center; border-radius: 5px; }";
  html += "</style>";

  // Script para actualizar dinámicamente la hora, el estado de las válvulas y el tiempo restante
  html += "<script>";
  html += "function updateTimeAndStates() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      var response = JSON.parse(xhr.responseText);";
  html += "      document.getElementById('currentTime').innerHTML = response.time;";
  html += "      document.getElementById('valveStates').innerHTML = response.valves;";
  html += "      document.getElementById('dayStates').innerHTML = response.days;";
  html += "    }";
  html += "  };";
  html += "  xhr.open('GET', '/getTimeAndStates', true);";
  html += "  xhr.send();";
  html += "}";
  html += "setInterval(updateTimeAndStates, 1000);";  // Actualiza cada segundo

  // Script para actualizar el tiempo restante de riego/pausa dinámicamente
  html += "function updateRemainingTime() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.onreadystatechange = function() {";
  html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
  html += "      document.getElementById('remainingTime').innerHTML = xhr.responseText;";
  html += "    }";
  html += "  };";
  html += "  xhr.open('GET', '/remainingTime', true);";
  html += "  xhr.send();";
  html += "}";
  html += "setInterval(updateRemainingTime, 1000);";  // Actualiza cada segundo
  html += "</script>";

  // Inicio del cuerpo de la página
  html += "</head><body><div class='container'><h1>TWISTED TRANSISTOR - Sistema de Riego</h1>";

  // Mostrar la hora actual y el día
  html += "<h2>Sistema de Riego</h2><div id='currentTime'>Hora actual: " + timeClient.getFormattedTime() + "<br>";

  // Mostrar el día de la semana
  String daysOfWeekString[7] = { "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo" };
  int currentDay = timeClient.getDay();
  currentDay = (currentDay == 0) ? 6 : currentDay - 1;  // Ajustar el día (Lunes = 0, Domingo = 6)
  html += "Día actual: " + daysOfWeekString[currentDay] + "<br></div>";

  // Mostrar el estado de las válvulas (se actualizará automáticamente)
  html += "<div id='valveStates'>" + getValveStateHtml() + "</div>";

  // Días de la semana con sus colores y botones para editar la configuración
  html += "<div id='dayStates'>";
  String days[7] = { "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo" };
  for (int i = 0; i < 7; i++) {
    String dayClass = daysOfWeek[i] ? "day-button day-active" : "day-button day-inactive";
    html += "<button class='" + dayClass + "' onclick=\"location.href='/editday?day=" + String(i) + "'\">" + days[i] + "</button>";
  }
  html += "</div>";

  // Mostrar el tiempo restante de riego o pausa (se actualizará automáticamente)
  html += "<div><strong>Tiempo restante: </strong><span id='remainingTime'>Calculando...</span></div>";

  // Botón para activar/desactivar modo manual
  html += "<h2>Modo Manual</h2>";
  html += "<div><button onclick=\"location.href='/manual/" + String(manualMode ? "off" : "on") + "'\">" + (manualMode ? "Desactivar Manual" : "Activar Manual") + "</button></div>";

  // Botones para abrir/cerrar válvulas en modo manual
  if (manualMode) {
    html += "<h2>Control de Válvulas (Modo Manual)</h2>";
    html += "<button onclick=\"location.href='/valve1/" + String(valve1State ? "off" : "on") + "'\">" + (valve1State ? "<i class='fas fa-tint-slash'></i> Cerrar Válvula 1" : "<i class='fas fa-tint'></i> Abrir Válvula 1") + "</button>";
    html += "<button onclick=\"location.href='/valve2/" + String(valve2State ? "off" : "on") + "'\">" + (valve2State ? "<i class='fas fa-tint-slash'></i> Cerrar Válvula 2" : "<i class='fas fa-tint'></i> Abrir Válvula 2") + "</button>";
  }

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleSaveDay() {
  int day = server.arg("day").toInt();

  // Validar que las horas y minutos sean válidos
  int startH = server.arg("startHour").toInt();
  int startM = server.arg("startMinute").toInt();
  int endH = server.arg("endHour").toInt();
  int endM = server.arg("endMinute").toInt();

  if (startM >= 60 || endM >= 60 || startH >= 24 || endH >= 24) {
    server.send(400, "text/html", "<html><body><h1>Error: Formato incorrecto de hora o minutos</h1></body></html>");
    return;
  }

  // Guardar los valores
  wateringDuration[day] = server.arg("duration").toInt();
  wateringInterval[day] = server.arg("interval").toInt();
  startHour[day] = startH;
  startMinute[day] = startM;
  endHour[day] = endH;
  endMinute[day] = endM;
  daysOfWeek[day] = server.hasArg("active");
  showConfig();
}

// Función para manejar el modo manual
void handleManualMode(String action) {
  manualMode = (action == "on");
  showConfig();
}

// Función para controlar las válvulas
void handleValveControl(String valve, String action) {
  if (valve == "valve1") {
    valve1State = (action == "on");
    digitalWrite(RELAY1, valve1State ? LOW : HIGH);  // Controlar el relé
  } else if (valve == "valve2") {
    valve2State = (action == "on");
    digitalWrite(RELAY2, valve2State ? LOW : HIGH);  // Controlar el relé
  }
  showConfig();
}

String getValveStateHtml() {
  // Verificar el estado actual de las válvulas en tiempo real
  valve1State = (digitalRead(RELAY1) == LOW);
  valve2State = (digitalRead(RELAY2) == LOW);

  // Generar el HTML para los estados de las válvulas
  String valve1Status = valve1State ? "<span style='color:blue;'><i class='fas fa-tint'></i> Abierta</span>" : "<span style='color:red;'><i class='fas fa-tint-slash'></i> Cerrada</span>";
  String valve2Status = valve2State ? "<span style='color:blue;'><i class='fas fa-tint'></i> Abierta</span>" : "<span style='color:red;'><i class='fas fa-tint-slash'></i> Cerrada</span>";
  
  return "<div><strong>Válvula 1:</strong> " + valve1Status + " | <strong>Válvula 2:</strong> " + valve2Status + "</div>";
}
void setup() {
  Serial.begin(115200);

  // Configuración de pines
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, HIGH);  // Cerrar válvula 1 por defecto
  digitalWrite(RELAY2, HIGH);  // Cerrar válvula 2 por defecto

  // Configuración de WiFi y mDNS
  WiFiManager wifiManager;
  wifiManager.autoConnect("TWISTED_TRANSISTOR_AP");  // Nombre del AP
  if (!MDNS.begin("esp8266")) {  // Nombre local (esp8266.local)
    Serial.println("Error al iniciar mDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  // Iniciar cliente NTP para obtener la hora
  timeClient.begin();
  timeClient.update();

  // Ruta para obtener la hora actual y los estados
  server.on("/getTimeAndStates", []() {
    String response;

    // Crear JSON para actualizar múltiples elementos
    response += "{";

    // Hora actual
    response += "\"time\":\"Hora actual: " + timeClient.getFormattedTime() + "<br>Día actual: ";
    String daysOfWeekString[7] = { "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo" };
    int currentDay = timeClient.getDay();
    currentDay = (currentDay == 0) ? 6 : currentDay - 1;  // Ajustar el día (Lunes = 0, Domingo = 6)
    response += daysOfWeekString[currentDay] + "<br>\",";

    // Estado de las válvulas
    response += "\"valves\":\"" + getValveStateHtml() + "\",";

    // Días de la semana con sus colores
    response += "\"days\":\"";
    String days[7] = { "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo" };
    for (int i = 0; i < 7; i++) {
      String dayClass = daysOfWeek[i] ? "day-button day-active" : "day-button day-inactive";
      response += "<button class='" + dayClass + "' onclick=\\\"location.href='/editday?day=" + String(i) + "'\\\">" + days[i] + "</button>";
    }
    response += "\"}";

    // Enviar la respuesta en formato JSON
    server.send(200, "application/json", response);
  });

  // Ruta para obtener la hora actual
  server.on("/getTime", []() {
    String timeString = timeClient.getFormattedTime();
    server.send(200, "text/plain", timeString);
  });

  // Ruta para obtener el estado de las válvulas
  server.on("/getValveStates", []() {
    server.send(200, "text/html", getValveStateHtml());
  });

  // Configuración de las rutas del servidor web
  server.on("/", showConfig);
  server.on("/editday", handleEditDay);
  server.on("/saveday", HTTP_POST, handleSaveDay);
  server.on("/manual/on", []() {
    handleManualMode("on");
  });
  server.on("/manual/off", []() {
    handleManualMode("off");
  });
  server.on("/valve1/on", []() {
    handleValveControl("valve1", "on");
  });
  server.on("/valve1/off", []() {
    handleValveControl("valve1", "off");
  });
  server.on("/valve2/on", []() {
    handleValveControl("valve2", "on");
  });
  server.on("/valve2/off", []() {
    handleValveControl("valve2", "off");
  });

  // Ruta para mostrar el tiempo restante
  server.on("/remainingTime", []() {
    String remainingTimeStr = String(remainingTime / 60) + " min " + String(remainingTime % 60) + " seg";
    server.send(200, "text/plain", remainingTimeStr);
  });

  // Iniciar el servidor web
  server.begin();
  Serial.println("Servidor TCP iniciado");

  // Agregar servicio HTTP a mDNS-SD para acceder con 'esp8266.local'
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  MDNS.update(); // Asegúrate de que mDNS se actualice en el loop
  server.handleClient();
  timeClient.update();  // Actualizar la hora desde el servidor NTP

  // Si el modo manual está activo, no ejecutar el control automático
  if (manualMode) {
    return;
  }

  unsigned long currentMillis = millis();  // Obtener el tiempo actual en milisegundos
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int currentDay = timeClient.getDay();  // 0 es Domingo, 6 es Sábado
  currentDay = (currentDay == 0) ? 6 : currentDay - 1;  // Ajustar el día (Lunes = 0, Domingo = 6)

  // Actualizar el tiempo restante si es mayor a 0
  if (remainingTime > 0) {
    unsigned long elapsedTime = (currentMillis - lastSwitchTime) / 1000;  // Calcular el tiempo transcurrido en segundos
    if (elapsedTime > 0) {
      remainingTime -= elapsedTime;
      lastSwitchTime = currentMillis;  // Actualizar el tiempo de referencia
    }
  }

  // Si remainingTime llegó a 0, asegúrate de que no se vuelva negativo
  if (remainingTime <= 0) {
    remainingTime = 0;
  }

  // Control de riego según el día y horario configurado
  if (daysOfWeek[currentDay]) {
    if ((currentHour > startHour[currentDay] || (currentHour == startHour[currentDay] && currentMinute >= startMinute[currentDay])) &&
        (currentHour < endHour[currentDay] || (currentHour == endHour[currentDay] && currentMinute <= endMinute[currentDay]))) {

      // Verificar si es momento de cambiar el estado de riego
      if (remainingTime == 0) {
        if (isWatering) {
          // Pausar riego (cerrar válvulas)
          digitalWrite(RELAY1, HIGH);  // Cerrar válvula 1
          digitalWrite(RELAY2, HIGH);  // Cerrar válvula 2
          Serial.println("Pausando riego");
          isWatering = false;
          remainingTime = wateringInterval[currentDay] * 60;  // Establecer tiempo de pausa
        } else {
          // Iniciar riego (abrir válvulas)
          digitalWrite(RELAY1, LOW);  // Abrir válvula 1
          digitalWrite(RELAY2, LOW);  // Abrir válvula 2
          Serial.println("Iniciando riego");
          isWatering = true;
          remainingTime = wateringDuration[currentDay] * 60;  // Establecer tiempo de riego
        }
        lastSwitchTime = millis();  // Reiniciar el contador de tiempo
      }
    } else {
      // Fuera del horario de riego: cerrar válvulas
      if (isWatering || digitalRead(RELAY1) == LOW || digitalRead(RELAY2) == LOW) {
        digitalWrite(RELAY1, HIGH);  // Cerrar válvula 1
        digitalWrite(RELAY2, HIGH);  // Cerrar válvula 2
        Serial.println("Fuera de horario de riego, cerrando válvulas");
        isWatering = false;
      }
    }
  } else {
    // Si el día no está activo, asegúrate de cerrar las válvulas
    if (digitalRead(RELAY1) == LOW || digitalRead(RELAY2) == LOW) {
      digitalWrite(RELAY1, HIGH);  // Cerrar válvula 1
      digitalWrite(RELAY2, HIGH);  // Cerrar válvula 2
      Serial.println("Día no activo, cerrando válvulas");
    }
  }
}
