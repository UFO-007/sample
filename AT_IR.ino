#include <HardwareSerial.h>
#include <IRremote.hpp>


#define ESP_BAUDRATE 115200
#define DEBUG_BAUDRATE 115200
#define LED PB9
#define IR_Pin PB10  

IRrecv irrecv;
HardwareSerial& SerialESP = Serial2;  // USART2: PA2(TX),  PA3(RX)
HardwareSerial& IR_Serial = Serial3;  // USART3: PB10(TX), PB11(RX)
bool wifiConnected = false;
String RecvdData = "";
unsigned long IR;

// Объявление структуры ДО функций, которые её используют
struct HTTPRequest {
  String method;
  String endpoint;
  String body;
  String params;
};

void debugPrint(String message) {
  Serial.println("[DEBUG] " + message);
}

String sendATCommand(String command, uint32_t timeout = 100) {//2000
  debugPrint("SEND: " + command);
  SerialESP.println(command);
  
  String response = "";
  uint32_t start = millis();
  while (millis() - start < timeout) {
    while (SerialESP.available()) {
      char c = SerialESP.read();
      response += c;
      if (response.endsWith("OK") || 
          response.endsWith("ERROR") || 
          response.endsWith("FAIL")) break;
    }
  }
  debugPrint("RECV: " + response);
  return response;
} // sendATCommand

// Функция для парсинга HTTP-запроса (объявлена после структуры)
HTTPRequest parseHTTPRequest(String request) {
  HTTPRequest req;
  
  // Извлекаем метод (GET/POST)
  int methodEnd = request.indexOf(' ');
  if (methodEnd != -1) {
    req.method = request.substring(0, methodEnd);
  }

  // Извлекаем endpoint
  int startEndpoint = methodEnd + 1;
  int endEndpoint = request.indexOf(' ', startEndpoint);
  if (endEndpoint != -1) {
    String fullPath = request.substring(startEndpoint, endEndpoint);
    
    // Отделяем параметры запроса
    int paramStart = fullPath.indexOf('?');
    if (paramStart != -1) {
      req.endpoint = fullPath.substring(0, paramStart);
      req.params = fullPath.substring(paramStart + 1);
    } else {
      req.endpoint = fullPath;
    }
  }

  // Для POST запросов извлекаем тело
  if (req.method == "POST") {
    int BodyLen, Body_Len_pos = request.indexOf("Content-Length: ");
    debugPrint("Body_Len_pos= " + String(Body_Len_pos));// = 46
    if (Body_Len_pos != -1){
    Body_Len_pos += 16; // длинна "Content-Length: "
    String tmp = request.substring(Body_Len_pos, Body_Len_pos + 10);
    //int del = tmp.lastIndexOf("\r\n");
    BodyLen = tmp.lastIndexOf("\r\n");
    debugPrint("request.length= " + String(request.length()));// = 620
    tmp.remove(BodyLen);
    tmp.trim();// tmp = 8
    debugPrint("tmp= " + tmp);
    BodyLen = tmp.toInt();// del
    debugPrint("BodyLen= " + String(BodyLen));
    /*
    Body_Len_pos = response.indexOf("\r\n\r\n") + 4;
    debugPrint("Body_Len_pos= " + String(Body_Len_pos));
    Body = response.substring(Body_Len_pos, Body_Len_pos + del);
    debugPrint("Body= "+Body);
    */
    int bodyStart = request.indexOf("\r\n\r\n");
    if (bodyStart != -1) {
      bodyStart += 4;
      req.body = request.substring(bodyStart, bodyStart + BodyLen);
      //req.body = Body;
      req.body.trim();
    }
    }
  }
  debugPrint("parseHTTPRequest:");
  debugPrint("req.method= " + req.method);
  debugPrint("req.endpoint= " + req.endpoint);
  debugPrint("req.body= " + req.body);
  return req;
}

// Остальной код без изменений...
// [Здесь должен быть код setup(), getParamValue(), sendHTTPResponse(), freeMemory(), loop()]

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(IR_Pin, INPUT_PULLUP);
  SerialESP.begin(ESP_BAUDRATE);
  while (!SerialESP){;}
  IR_Serial.begin(115200);
  while (!IR_Serial){;}
  Serial.begin(DEBUG_BAUDRATE);
  while (!Serial){;}
  debugPrint("Initializing system...");

  sendATCommand("AT+RST", 3000);
  delay(500);//1000
  
  if (sendATCommand("AT").indexOf("OK") == -1) {
    debugPrint("ESP not responding!");
    while(1);
  }

  sendATCommand("AT+CWMODE=1");
  
  // ЗАМЕНИТЕ НА ВАШИ ДАННЫЕ WI-FI
  String cmd = "AT+CWJAP=\"";
  cmd += "Xiaomi_7C73_007";     // Ваш SSID
  cmd += "\",\"";
  cmd += "9627220499"; // Ваш пароль
  cmd += "\"";
  
  if (sendATCommand(cmd, 10000).indexOf("OK") != -1) {
    wifiConnected = true;
    debugPrint("WiFi connected!");
  } else {
    debugPrint("WiFi connection failed!");
  }

  sendATCommand("AT+CIPMUX=1");
  sendATCommand("AT+CIPSERVER=1,80");
  sendATCommand("AT+CIFSR");
  
  //digitalWrite(PC13, HIGH);
  irrecv.begin(IR_Pin, ENABLE_LED_FEEDBACK);
  Serial.println("IRin = Enabled");
}
/*
// Функция для парсинга параметров запроса
String getParamValue(String params, String key) {// state=on, 
  int keyStart = params.indexOf(key + "=");
  if (keyStart == -1) return "";
  // хуйня полная
  int valueStart = keyStart + key.length() + 1;
  int valueEnd = params.indexOf('&', valueStart);
  if (valueEnd == -1) {
    return params.substring(valueStart);
  }
  return params.substring(valueStart, valueEnd);
}
*/
void sendHTTPResponse(int channel, HTTPRequest req) {
  String html = "";
  String contentType = "text/html";
  int statusCode = 200;
  String statusMessage = "OK";
  // Обработка разных эндпоинтов
  if (req.endpoint == "/") {
    html = "<html><head><title>STM32 Server</title></head>";
    html += "<body><h1>STM32F1 + ESP-01</h1>";
    html += "<p>Uptime: " + String(millis() / 1000) + "s</p>";
    html += "<p>IR Code: " + String(IR, HEX) + "</p>";
    
    // Форма для POST-запроса
    html += "<form method='POST' action='/led'>";
    html += "LED Control: ";
    html += "<select name='state'>";
    html += "<option value='on'>ON</option>";
    html += "<option value='off'>OFF</option>";
    html += "</select>";
    html += "<input type='submit' value='Apply'>";
    html += "</form>";
    
    html += "<p>Endpoints:</p>";
    html += "<ul><li><a href='/'>Home</a></li>";
    html += "<li><a href='/status'>Status</a></li>";
    html += "<li><a href='/data'>Sensor Data</a></li></ul>";
    html += "</body></html>";
  }
  else if (req.endpoint == "/status") {
    html = "<html><head><title>System Status</title></head>";
    html += "<body><h1>System Status</h1>";
    html += "<p>WiFi: " + String(wifiConnected ? "Connected" : "Disconnected") + "</p>";
    html += "<p>Free Memory: " + String(freeMemory()) + " bytes</p>";
    html += "<p><a href='/'>Back to Home</a></p>";
    html += "</body></html>";
  }
  else if (req.endpoint == "/data") {
    int adcValue = analogRead(PA0);
    html = "<html><head><title>Sensor Data</title></head>";
    html += "<body><h1>Sensor Data</h1>";
    html += "<p>ADC Value: " + String(adcValue) + "</p>";
    html += "<p>Voltage: " + String(adcValue * 3.3 / 4095, 2) + "V</p>";
    html += "<p><a href='/'>Back to Home</a></p>";
    html += "</body></html>";
  }
  else if (req.endpoint == "/led") {
    if (req.method == "POST") {
      //String ledState = getParamValue(req.body, "state");//????
      if (req.body == "state=on") {
        digitalWrite(LED, LOW);  // Включить светодиод
        html = "LED turned ON";
        debugPrint("LED turned ON");
      } else if (req.body == "state=off") {
        digitalWrite(LED, HIGH); // Выключить светодиод
        html = "LED turned OFF";
        debugPrint("LED turned OFF");
      } else {
        html = "Invalid LED state";
      }
    } else {
      html = "Use POST method for LED control";
    }
  }
  else {
    // Неизвестный эндпоинт
    statusCode = 404;
    statusMessage = "Not Found";
    contentType = "text/plain";
    html = "404 Not Found\nEndpoint '" + req.endpoint + "' doesn't exist";
  }

  // Формируем HTTP-ответ
  String response = "HTTP/1.1 " + String(statusCode) + " " + statusMessage + "\r\n";
  response += "Content-Type: " + contentType + "\r\n";
  response += "Connection: close\r\n\r\n";
  response += html;

  // Отправка команды
  String cmd = "AT+CIPSEND=";
  cmd += channel;
  cmd += ",";
  cmd += response.length();
  sendATCommand(cmd, 500);

  // Отправка данных
  SerialESP.print(response);
  delay(100);

  // Закрытие соединения
  cmd = "AT+CIPCLOSE=";
  cmd += channel;
  sendATCommand(cmd);
  RecvdData = "";
}

// Функция для получения информации о свободной памяти
extern "C" char* sbrk(int incr);
int freeMemory() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}

void loop() {
  RecvdData = "";
  while (SerialESP.available()) {
    //RecvdData += SerialESP.readStringUntil('\n');
    RecvdData = SerialESP.readString();
    debugPrint("ESP.read: " + RecvdData);
  }
    
  if (RecvdData.indexOf("+IPD") != -1) {
    // Парсинг данных запроса
    //RecvdData.remove(0, 5);
     
    int contentStart = RecvdData.indexOf("+IPD,") + 5;
    debugPrint("contentStart= "+String(contentStart));
    int channel = RecvdData.charAt(contentStart) - 48;//'0';
    //debugPrint("char= "+String(contentStart));
    debugPrint("RecvdData.charAt(contentStart)= " + String(RecvdData.charAt(contentStart)));
    debugPrint("channel= " + String(channel));
    contentStart = RecvdData.indexOf(':') + 1;
    String request = RecvdData.substring(contentStart);
    
    // Парсим HTTP-запрос
    HTTPRequest req = parseHTTPRequest(request);
      
    debugPrint("Method: " + req.method);
    debugPrint("Endpoint: " + req.endpoint);
    if (req.params.length() > 0) debugPrint("Params: " + req.params);
    if (req.body.length() > 0) debugPrint("Body: " + req.body);
      
    sendHTTPResponse(channel, req);
  }
  while (IR_Serial.available() > 0){

  }

  
  debugPrint('')
  if (irrecv.decode()){
    IR = irrecv.decodedIRData.decodedRawData;
    if (IR != 0) Serial.println(IR, HEX); // Выводим в порт
    irrecv.resume(); // Receive the next value
  }
  /*
  // Мигание светодиодом
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    // Не мигаем если светодиод управляется через запрос
    if (digitalRead(LED) == HIGH) {
      digitalWrite(LED, LOW);
    } else {
      digitalWrite(LED, HIGH);
    }
  }
  */
}
