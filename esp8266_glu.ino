/*Эта  веб-сервер*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include<SPI.h>
#include<EEPROM.h>
#define EEPROM_SIZE 512

/*
 * Собсна, коды команд (HTML-запросы):
 * http://192.168.1.1:80 - вход (оно же localhost). 
 * http://192.168.1.1:80/move?dir=result - результат измерений 
 * http://192.168.1.1:80/move?dir=test - Начать тест, проверить проводимость ИК-спектра (излучатель ИК),
 * проверить проводимость видимого света (синий СИД), соотнести. Переходит дальше не автомате.
 * http://192.168.1.1:80/move?dir=calibration_d - данные о КАЛибровке
 * http://192.168.1.1:80/move?dir=reset_confirm - сброс до заводских настроек
 * Ячейки EEPROM:
 * 0 - isCalibrated,
 * 1 - gluMin, 
 * 2 - glu5, 
 * 3 - gluMax, 
 * 4 - gluLevelMin, 
 * 5 - gluLevel5, 
 * 6 - gluLevelMax, 
 * 7 - oxyMin, 
 * 8 - oxy5, 
 * 9 - oxyMax;
 * 10 - logscale (размерр таблицы измерений)
 * 11 и далее - логи.
 * */
/* Установите здесь свои SSID и пароль */
const char* ssid = "GLUcose";       // SSID
const char* password = "termpaper";  // пароль

/* Настройки IP адреса */
IPAddress local_ip(192,168,1,1);//Не использую доменное имя
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
uint8_t LED_Pin = 5;//Синий СИД 
uint8_t phr_IN = A0;//Вход аналогового сигнала, он же A0
uint8_t relay = 0;//Вход реле,
uint8_t tst = 1;//Контакт для запуска тестового режима; подаем высокий уровень
uint8_t tstm = 3;//Контакт для выбора датчика и излучателя в тестовом режимы; подаем высокий уровень
uint8_t irDaOut = 2;//Выход ИК-излучателя
uint8_t gluMin, glu5, gluMax, gluLevelMin, gluLevel5, gluLevelMax, oxyMin, oxy5, oxyMax;
int isCalibrated = false;
bool isTesting = false;
int latest_visible;
int latest_irda;
int resultator;
String curMod = "Visible";//при нуле

ESP8266WebServer server(80);//порт 80


char buff[]="Hello \n";
String main_page = "<!DOCTYPE html>\
<html>\
<body>\
<h1>Infrared glucometer on microcontroller - main</h1>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=test\'\">\
    Do the test\
</button>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=latest\'\">\
    Glucose logs\
</button>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=calibration_d\'\">\
   Calibration data\
</button>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=reset_confirm\'\">\
    Factory reset\
</button>\
</body>\
</html>";

String rst_query = "<!DOCTYPE html>\
<html>\
<body>\
<h1>Are you sure you want to reset the device and delete all the logs?</h1>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80\'\">\
    No\
</button>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80\'\">\
    No\
</button>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=do_reset\'\">\
   Yes\
</button>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80\'\">\
    No\
</button>\
</body>\
</html>";

String success = "<!DOCTYPE html>\
<html>\
<body>\
<h1>Operation successful!</h1>\
<button class=\"favorite styled\"\
        type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80\'\">\
    Proceed\
</button>\
</body>\
</html>";

String displayLogs (){

String strng = "<!DOCTYPE html>\n";
strng += "<html>\n";
strng += "<body>\n";
strng += "<h1>Log of latest operations:</h1>\n";

String table = "";
int logscale = EEPROM.read(10);
if(logscale !=0){
for(int i = 0; i < logscale;i++){
	int ef = i + 1;
 String ef2 = ""+ef;
  String hz = "<h0>Entry " + ef2 + ", result: ";
  if(EEPROM.read(11+i) < EEPROM.read(4)){
    hz += "Glucose too low";
  }else if(EEPROM.read(11+i) > EEPROM.read(4)){
    hz += "Glucose too high";
  }else {
    hz += "Regular";
  }
  
  
  hz += " </h0>\n";
  strng += hz;
}
}else{
	String hz = "<h0>No logs available.</h0>\n";
  strng += hz;
}
  String closeup = "<button class=\"favorite styled\"\n";
  closeup += "      type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80\'\">\n";
 closeup += "      Back to menu\n";
closeup += "  </button>\n";
closeup += "   </body>\n";
closeup += "   </html>";
  return strng + table + closeup;
};

String calib_data (){

String strng = "<!DOCTYPE html>\n";
strng += "<html>\n";
strng += "<body>\n";
strng += "<h1>Calibration data:</h1>\n";

if(EEPROM.read(0) == 3){
	strng += "<h0>Calibration data has been set.</h0>\n";
}
if(EEPROM.read(0) != 3){
	strng += "<h0>Calibration required.</h0>\n";
}

  String closeup = "<button class=\"favorite styled\"\n";
  closeup += "      type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80\'\">\n";
 closeup += "      Back to menu\n";
closeup += "  </button>\n";
 closeup = "<button class=\"favorite styled\"\n";
  closeup += "      type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=start_calib\'\">\n";
 closeup += "      Begin calibration\n";
closeup += "  </button>\n";
closeup += "   </body>\n";
closeup += "   </html>";
  return strng + closeup;
};

String result_nq (){

String strng = "<!DOCTYPE html>\n";
strng += "<html>\n";
strng += "<body>\n";
strng += "<h1>Result:</h1>\n";

String table = "";
  String hz = "<h0>Result: ";
  if(resultator < EEPROM.read(4)){
    hz += "Glucose too low";
  }else if(resultator > EEPROM.read(4)){
    hz += "Glucose too high";
  }else {
    hz += "Regular";
  }
  
  
  hz += " </h0>\n";
  strng += hz;

  String closeup = "<button class=\"favorite styled\"\n";
  closeup += "      type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80\'\">\n";
 closeup += "      Back to menu\n";
closeup += "  </button>\n";
closeup += "   </body>\n";
closeup += "   </html>";
  return strng + table + closeup;
};

String result_query (){

String strng = "<!DOCTYPE html>\n";
strng += "<html>\n";
strng += "<body>\n";
strng += "<h1>Calibration result set: what it supposed to be</h1>\n";

  String closeup = "<button class=\"favorite styled\"\n";
  closeup += "      type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=calib_was_low\'\">\n";
 closeup += "      Low glucose\n";
closeup += "  </button>\n";
closeup +="<button class=\"favorite styled\"\n";
  closeup += "      type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=calib_was_reg\'\">\n";
 closeup += "      Regular glucose\n";
closeup += "  </button>\n";
closeup +="<button class=\"favorite styled\"\n";
  closeup += "      type=\"button\" onclick=\"location.href=\'http://192.168.1.1:80/move?dir=calib_was_high\'\">\n";
 closeup += "      High glucose\n";
closeup += "  </button>\n";
closeup += "   </body>\n";
closeup += "   </html>";
  return strng + closeup;
};



void setup() {
 Serial.begin(9600); /* begin serial with 9600 baud */
 SPI.begin();  /* begin SPI */
 pinMode(LED_Pin, OUTPUT);
 pinMode(phr_IN, INPUT);
 pinMode(relay,OUTPUT);
 pinMode(tst,INPUT);
 pinMode(tstm,INPUT);
  pinMode(irDaOut,OUTPUT);
 WiFi.softAP(ssid, password);
 EEPROM.begin(EEPROM_SIZE);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  server.onNotFound(handle_NotFound);
  server.begin();
   delay(100);
  Serial.println("HTTP server started");
  //server.handleClient();
  server.on("/move", HTTP_GET, handleMoveRequest);
   Serial.println("Connection established!");//пусть будет для отладки
  
  
}

void loop() {
  server.handleClient();
  /* 
 for(int i=0; i<sizeof buff; i++){  //Вся эта несчастная часть позволяет образовать мост между ESP-12E и ATxmega328 с передачей словесных команд
  SPI.transfer(buff[i]);
  delay(100);
  }
  */ if(digitalRead(tst)){
	  isTesting = true;
	  if(!digitalRead(tstm)){
		  curMod="Visible";
	  }
	  if(!digitalRead(tstm)){
		  curMod="IrDA";
	  }
  Serial.print("Analog ADC: ");
  Serial.print("Is testing ");
  Serial.print(isTesting);
  Serial.print(" currentMode: ");
  Serial.print(curMod);
  Serial.print(" Value ");
  }
}

void handleMoveRequest() {
  if (!server.hasArg("dir")) {
    server.send(404, "text / plain", "Consider using ?dir= ");
    return;
  }
  String direction = server.arg("dir");
  if (direction.equals("test_visible")) {
    visible_blink();
    server.send(200, "text / plain", "Testing visible light level");
  }
   if (direction.equals("test_irda")) {
    irda_blink();
    server.send(200, "text / plain", "Testing irda level");
  }
   if (direction.equals("reset_confirm")) {
    //data_reset();
    server.send(200, "text / html", rst_query);
  }
  if (direction.equals("start_calib")) {
    //data_reset();
    doTheTest(0);
    server.send(200, "text / html", result_query());
  }
  if (direction.equals("test")) {
    //data_reset();
    doTheTest(1);
    server.send(200, "text / html", result_nq());
  }
  if (direction.equals("calib_was_reg")) {
    //data_reset();
    EEPROM.write(5, resultator);
    int setstats = EEPROM.read(0)+1;
    EEPROM.write(0, setstats);
    server.send(200, "text / html", calib_data());
  }
  if (direction.equals("calib_was_low")) {
    //data_reset();
    EEPROM.write(4, resultator);
    int setstats = EEPROM.read(0)+1;
    EEPROM.write(0, setstats);
    server.send(200, "text / html", calib_data());
  }
  if (direction.equals("calib_was_high")) {
    //data_reset();
    EEPROM.write(6, resultator);
    int setstats = EEPROM.read(0)+1;
    EEPROM.write(0, setstats);
    server.send(200, "text / html", calib_data());
  }
  if (direction.equals("calibration_d")) {
    //data_reset();
    server.send(200, "text / html", calib_data());
  }
  if (direction.equals("latest")) {
    //data_reset();
    server.send(200, "text / html", displayLogs());
  }
  if (direction.equals("do_reset")) {
    data_reset();
    server.send(200, "text / html", success);
  }
  else {
    server.send(404, "text / html",  main_page);
  }
}

void handle_NotFound() {
  server.send(404, "text / plain", "404: Not found");
}
void visible_blink() {
  Serial.println("Blinkon");
  digitalWrite(LED_Pin, HIGH);
  analogRead(A0);
  delay(2500);
  digitalWrite(LED_Pin, LOW);
  Serial.println("Blinkoff");
}
void data_reset(){
  EEPROM.write(0, 0);
  EEPROM.write(10, 0);
  EEPROM.commit();
}

void doTheTest(int writeToLog){
	//int visDataRaw[10];
	//int irDataRaw[10];
  //Если коротко, то чем больше синего и меньше ИК, тем сахар выше. Важно, чтобы все было нормально с капиллярами.
	latest_visible=0;
	latest_irda=0;
	digitalWrite(relay,0);
  digitalWrite(LED_Pin,1);
  for(int j = 0; j<10;j++){
	  latest_visible += analogRead(phr_IN);
	  delay(10);
  }//Почему делим на 40? Потому что пишем значения в 1 байт (2^8=256)
  latest_visible /= 40;
  digitalWrite(LED_Pin,0);
  digitalWrite(irDaOut,1);
  digitalWrite(relay,1);
  for(int k = 0; k<10;k++){
	  latest_irda += analogRead(phr_IN);
	  delay(10);
  }
  digitalWrite(irDaOut,0);
  digitalWrite(relay,0);
  
  latest_irda /= 40;

  resultator = latest_visible - latest_irda; //Чем больше разница, тем выше уровень глюкозы. При ОТРИЦАТЕЛЬНОМ значении все очень плохо.
  if(resultator <= 0){resultator=0;}
  if(writeToLog==1){
  int logscale = EEPROM.read(10);
  EEPROM.write(logscale+1, resultator);
  EEPROM.write(10, logscale+1);
  EEPROM.commit();
  }
}


void irda_blink() {
  Serial.println("Blinkon");
  digitalWrite(LED_Pin, HIGH);
  delay(2500);
  digitalWrite(LED_Pin, LOW);
  Serial.println("Blinkoff");
}
