
#include <WiFi.h>
#include "FS.h"
#include "SPIFFS.h"
#include "TimeLib.h" //Use Arduino time library ver1.5
#include "ESP32_WebGet.h" //mgo-tec library ver 1.12
#include <MQTTClient.h>

//#define SIGPIN 12
#define LEDPIN 23

const int pwmPin = A14;

const char *ap_ssid = "RHIap"; //ESP32 softAP SSID
const char *ap_pass = "pass"; //ESP32 softAP password

const char* AP_config_file = "/config.txt"; //ssid, password save file

IPAddress LIP; //Local IP address
WiFiServer server(80);
WiFiClient wificlient;
ESP32_WebGet EWG;
 
uint8_t ssid_num;
String ssid_rssi_str[30];
String ssid_str[30];
String Selected_SSID_str = " ";
String Sel_SSID_PASS_str = " ";

String RHI_ID = " ";
String msg = " ";
long randNum = 0;
int vol_value = 0;

uint32_t scanLastTime = 0;
boolean First_Scan_Set = true;

boolean onlineFlag = false;
boolean mqttsetupFlag = false;

MQTTClient mqttclient;
unsigned long lastMillis = 0;


void connect() {
  /*
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  // shiftrのkeyとSecretを入力
  Serial.print("\nconnecting...");
  */
  randNum = random(4096, 65536); // 4096~65535の乱数(0埋め用)
  RHI_ID = String("RHI_"+String(randNum, HEX));
  while (!mqttclient.connect(RHI_ID.c_str(), "AAA", "BBB")) { // AAA: pub_token, BBB:sub_token
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  mqttclient.subscribe("/pressure");
  // wificlient.unsubscribe("/hello");
}


void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload); // RHI_FFFF.512
  String payloadID = payload.substring(0, 8);
  Serial.println("payloadID:" + payloadID);
  String payloadValue = payload.substring(9);
  Serial.println("payloadValue:" + payloadValue);
  if(topic == "/pressure" && payloadValue.toInt() < 1024 && payloadID != RHI_ID) {
    //digitalWrite(SIGPIN, HIGH);
    ledcWrite(0, 255-(payload.toInt()/4));
    digitalWrite(LEDPIN, HIGH);
    delay(50);
    //digitalWrite(SIGPIN, LOW);
    digitalWrite(LEDPIN, LOW);
    ledcWrite(0, 0);
  } 
  else {
    //digitalWrite(SIGPIN, LOW);
    ledcWrite(0, 0);
  }
}

void setup() {
  //pinMode(SIGPIN, OUTPUT);
  ledcSetup(0, 12800, 8); // 使用するタイマーのチャネルと周波数を設定
  ledcAttachPin(pwmPin, 0);
  pinMode(LEDPIN, OUTPUT);
  Serial.begin(115200);
  
  // softAP
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS failed, or not present");
    return;
  }
  WiFi.mode(WIFI_AP_STA); //AP mode and STA mode
  WiFi.softAP(ap_ssid, ap_pass);
  delay(100);
  Serial.println("Setup done");
  server.begin();
  Serial.println(F("Server started"));
  scanLastTime = millis();
  
}


void loop() {
  if(onlineFlag == false) {
    wificlient_connect();
    wifi_scan(30000);
  }
  
  if(onlineFlag == true) {
   if(mqttsetupFlag == false) { 
      // MQTT
      mqttclient.begin("broker.shiftr.io", wificlient);
      mqttclient.onMessage(messageReceived);
      connect();
      mqttsetupFlag = true;
   }
    mqttclient.loop();
    delay(10);
    if (!mqttclient.connected()) {
      connect();
    }
  }
/*
  // publish a message roughly every second.
  if (millis() - lastMillis > 3000) {
    lastMillis = millis();
    wificlient.publish("/hello", "world");
  }
*/
  // 送信
  vol_value = analogRead(33)/4;
  //Serial.println(vol_value);
  if(vol_value < 1023) {
    msg = String(RHI_ID + "." + vol_value);    //TODO msgに固有ID付与する．送信時に振動させないため
    mqttclient.publish("/pressure", msg);
  }
  // 受信
  mqttclient.onMessage(messageReceived);
  //topic = " ";
  //payload = " ";
}


void wificlient_connect() {
  String html_res_head = "HTTP/1.1 200 OK\r\n";
         html_res_head += "Content-type:text/html\r\n";
         html_res_head += "Connection:close\r\n\r\n";
  String html_tag1 = "<!DOCTYPE html>\r\n<html>\r\n<head>\r\n";
         html_tag1 += "<meta name='viewport' content='initial-scale=1.5'>\r\n";
         html_tag1 += "</head>\r\n\r\n";
         html_tag1 += "<body style='background:#000; color:#fff; font-size:100%;'>\r\n";
         html_tag1 += "ESP32 (ESP-WROOM-32)<br>\r\n";
         html_tag1 += "Access Point Selector<br>\r\n";
  String html_tag2 = "\r\n</body>\r\n</html>\r\n\r\n";
 
  wificlient = server.available();
 
  if (wificlient) {
    Serial.println("new wificlient");
    String req_str = "";
    while (wificlient.connected()){
      while(wificlient.available()){
        req_str =wificlient.readStringUntil('\n');
        if(req_str.indexOf("\r") == 0) break;
        Serial.println(req_str);
        if(req_str.indexOf("GET / ") >= 0){
          Serial.println("--------------- GET Request Receive from Clinet");
          while(wificlient.available()){
            char c = wificlient.read();
            Serial.write(c);
          }
          Serial.println("--------------- GET Request Receive Finish");
          html_send(false, "ESP32 STA connection close", "ESP32 STA connection close", "#FFF", html_res_head, html_tag1, html_tag2);
 
          delay(10);
          wificlient.stop();
          Serial.println("wificlient disonnected");
          delay(10);
          req_str = "";
        }else if(req_str.indexOf("GET /?") >= 0){
          Serial.println("--------------- SUBMIT Receive from Clinet");
          int16_t getTXT_pass = req_str.indexOf("pass1=");
          int16_t getTXT_select = req_str.indexOf("ssid_select=");
          int16_t getTXT_close = req_str.indexOf("connection_close=");
 
          if(getTXT_pass > 0){
            Sel_SSID_PASS_str = req_str.substring(getTXT_pass + 6, req_str.indexOf("&ssid_sel_submit"));
          }
          if(getTXT_select > 0){
            Selected_SSID_str = req_str.substring(getTXT_select + 12, req_str.indexOf("&pass1"));
          }
          if(getTXT_close < 0){
            Serial.printf("Selected_SSID_str = %s\r\n", Selected_SSID_str.c_str());
            Serial.printf("Sel_SSID_PASS_str = %s\r\n", Sel_SSID_PASS_str.c_str());
 
            while(wificlient.available()){
              char c = wificlient.read();
              Serial.write(c);
            }
 
            Serial.println("-------------- SUBMIT Request Receive Finish");
            String str_w = Selected_SSID_str + "\r\n" + Sel_SSID_PASS_str;
 
            SPIFFS_writeFile(AP_config_file, str_w.c_str());
            delay(500);
 
            char spiffs_read_ssid[64] = {};
            char spiffs_read_pass[64] = {};
 
            SPIFFS_readFile(AP_config_file, spiffs_read_ssid, spiffs_read_pass);
            Serial.printf("SPIFFS read ssid = %s\r\n", (const char *)spiffs_read_ssid);
            Serial.printf("SPIFFS read pass = %s\r\n", (const char *)spiffs_read_pass);
            Serial.printf("\r\n%s Connecting ...\r\n", Selected_SSID_str.c_str());
 
            delay(10); // Important! This delay is necessary to connect to the Access Point.
            WiFi.begin(spiffs_read_ssid, spiffs_read_pass);
            uint32_t timeout = millis();
            while(1){
              if(WiFi.status() != WL_CONNECTED){
                delay(500);
                Serial.print(".");
                if(millis()-timeout > 20000){
                  html_send(false, "disconnected", "STA disconnected Time OUT", "#F00", html_res_head, html_tag1, html_tag2);
                  //return 0;
                  break;
                }
              }else{
                // インターネット接続完了
                LIP = WiFi.localIP();
                Serial.println("\r\nWiFi connected");
                Serial.print("Local IP address: "); Serial.println(LIP);
                Serial.printf("\r\n-----------%s Connected!\r\n", spiffs_read_ssid);
 
                EWG.EWG_NTP_TimeLib_init(9, "time.windows.com"); //NTPサーバー取得初期化
                String NowTime = String(year()) + "/" + String(month()) + "/" + String(day()) + "/ " + String(hour()) + ":" + String(minute());
                html_send(true, Selected_SSID_str, "ESP32 STA connect OK!<br>"+ NowTime, "#0F0", html_res_head, html_tag1, html_tag2);
                Serial.printf("Now Time %s\r\n", NowTime.c_str());
                //return 1;
                onlineFlag = true;
                break;
              }
            }
          }else{
            html_send(false, "---", "ESP32 STA Closed!", "#F00", html_res_head, html_tag1, html_tag2);
            WiFi.disconnect(false); //false=WiFi_ON , true=WiFi_OFF
          }
 
          delay(10);
          wificlient.stop();
          Serial.println("wificlient disonnected");
          delay(10);
          req_str = "";
        }else if(req_str.indexOf("GET /favicon") >= 0){
          favicon_response();
          req_str = "";
        }
      }
    }
  }
}


void html_send(boolean sta_connected, String message1, String message2, String color, String html_res_head, String html_tag1, String html_tag2) {
  wificlient.print(html_res_head);
  wificlient.print(html_tag1);
  wificlient.print(HTML_Select_Box_str("!xxxx", message1));
  wificlient.printf("<p style='color:%s; font-size:80%%'>%s</p>\r\n", color.c_str(), message2.c_str());
  if(sta_connected == true){
    wificlient.print("<span style='font-size:80%'>ESP32 STA IP = ");
    wificlient.print(LIP);
    wificlient.print("</span>\r\n");
  }
  wificlient.print(html_tag2);
 
  Serial.print(html_res_head);
  Serial.print(html_tag1);
  Serial.print(HTML_Select_Box_str("!xxxx", message1));
  Serial.printf("<p style='color:%s; font-size:80%%'>%s</p>\r\n", color.c_str(), message2.c_str());
  if(sta_connected == true){
    Serial.print("<span style='font-size:80%%'>ESP32 STA IP = ");
    Serial.print(LIP);
    Serial.print("</span>\r\n");
  }
  Serial.print(html_tag2);
}


String HTML_Select_Box_str(String button_id, String Sel_Ssid) {
  String str = "";
  String selected_str = "";
  str += "<form name='F_ssid_select'>\r\n";
  str += "  <select name='ssid_select'>\r\n";
  for(int i=0; i<ssid_num; i++){
    if(Selected_SSID_str == ssid_str[i]){
      selected_str = " selected";
    }else{
      selected_str = "";
    }
    str += "    <option value=" + ssid_str[i] +  selected_str + ">" + ssid_rssi_str[i] + "</option>\r\n";
  }
  str += "</select><br>\r\n";
  str += "Password<br><input type='password' name='pass1'>\r\n";
  str += "<br><button type='submit' name='ssid_sel_submit' value='send' style='background-color:#AFA;' onclick='document.getElementById(\"ssid_sel_txt\").innerHTML=document.F_ssid_select.ssid_select.value;'>STA Connection GO!</button>\r\n";
  str += "</form><br>\r\n";
  str += "<form name='F_connection_close'>\r\n";
  str += "  <button type='submit' name='connection_close' value='send' style='background-color:#FAA;' onclick='document.getElementById(\"ssid_sel_txt\").innerHTML=\"ESP32 STA connection close\";'>STA Connection Close</button>\r\n";
  str += "</form>\r\n";
  str += "<br>  (Selected SSID)<br><span id='ssid_sel_txt'  style='font-size:80%;'>";
  str += Sel_Ssid;
  str += "</span>\r\n";
  return str;
}


void wifi_scan(uint32_t scan_interval) {
  if( (First_Scan_Set == true) || ((millis() - scanLastTime) > scan_interval) ){
    Serial.println("scan start");
 
    // WiFi.scanNetworks will return the number of networks found
    ssid_num = WiFi.scanNetworks();
    Serial.println("scan done\r\n");
    if (ssid_num == 0) {
      Serial.println("no networks found\r\n");
    } else {
      Serial.printf("%d networks found\r\n\r\n", ssid_num);
      for (int i = 0; i < ssid_num; ++i) {
        ssid_str[i] = WiFi.SSID(i);
        String wifi_auth_open = ((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
        ssid_rssi_str[i] = ssid_str[i] + " (" + WiFi.RSSI(i) + "dBm)" + wifi_auth_open;
        Serial.printf("%d: %s\r\n", i, ssid_rssi_str[i].c_str());
        delay(10);
      }
    }
    Serial.println("");
    scanLastTime = millis();
    First_Scan_Set = false;
  }
}


void SPIFFS_writeFile(const char * path, const char *c_ssid_pass) {
    Serial.printf("SPIFFS writing file: %s\n", path);
    File file = SPIFFS.open(path, FILE_WRITE);
    if(!file){
      Serial.println("Failed to open file for writing");
      return;
    }
    file.seek(0);
    if(file.print(c_ssid_pass)){
      Serial.println(c_ssid_pass);
      Serial.println("SPIFFS file written\r\n");
    } else {
      Serial.println("SPIFFS write failed\r\n");
    }
    file.close();
}


void SPIFFS_readFile(const char * path, char ssid_c[], char pass_c[]) {
    Serial.printf("SPIFFS reading file: %s\n", path);
 
    File file = SPIFFS.open(path);
    if(!file || file.isDirectory()){
      Serial.println("SPIFFS Failed to open file for reading");
      return;
    }
 
    int i=0, j=0;
    while(file.available()){
      ssid_c[i] = file.read();
      if(ssid_c[i] == '\r'){
        ssid_c[i] = '\0';
        char c = file.read();
        if(c == '\n'){
          while(file.available()){
            pass_c[j++] = file.read();
          }
          break;
        }
      }
      i++;
    }
    if(i < 1) Serial.println("cannot read ssid");
    if(j < 1) Serial.println("cannot read password");
    file.close();
}


void favicon_response() {
  Serial.println(F("-----------------------Favicon GET Request Received"));
  while(wificlient.available()){
    Serial.write(wificlient.read());
  }
 
  wificlient.print(F("HTTP/1.1 404 Not Found\r\n"));
  wificlient.print(F("Connection:close\r\n\r\n"));
 
  delay(10);
  wificlient.stop();
  delay(10);
 
  Serial.println(F("-----------------Client.stop (by Favicon Request)"));
}
