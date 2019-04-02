/******************************************************************************
    Copyright (C) 2019 by Alejandro Vargas <alejandro.anv@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

// docs at https://github.com/iotappstory/ESP-Library

#define MODE_BUTTON_SHORT_PRESS       500
#define MODE_BUTTON_LONG_PRESS        4000
#define MODE_BUTTON_VERY_LONG_PRESS   10000
#define CALLHOME_INTERVAL 60

#define PING_TIME 1000*60
#define MSG_TIME 1000*600
#define TOPIC_TIME 1000*2

#define DEBOUNCEDELAY 50


#define LEDR D0
#define LEDG D1
#define LEDB D2



#define COMPDATE __DATE__ __TIME__
#define MODEBUTTON 0                                        // Button pin on the esp for selecting modes. D3 for the Wemos!

#define CHANNEL_PREFIX "#anvEsp8266"


#include "Arduino.h"
#include "uCRC16Lib.h"
#include <Ticker.h>  //Ticker Library

#include <IOTAppStory.h>                                    // IotAppStory.com library
IOTAppStory IAS(COMPDATE, MODEBUTTON);                      // Initialize IOTAppStory

Ticker anvticker;

String grupo=""; 
String devicename=""; 


String channel;
String nick="Ep";

const byte interruptPin = 13;
#define LED1 D5



const char* host = "chat.freenode.net";
int estado_actual=0;
byte color=0;

unsigned long lastDebounceTime = 0;



String ultima_linea="";
String topic="";

bool hola=1;

WiFiClient client;

char* yourname="test device";
char* groupname="test";

/////////////////////////////////////////////////////////
void blinkled(){
  digitalWrite(LED1, !(digitalRead(LED1)));  //Invert Current State of LED  
}

// ================================================ SETUP ================================================
void setup() {
  pinMode(LED1, OUTPUT);
  anvticker.attach(0.1, blinkled);
   
  IAS.preSetDeviceName("syncrolights");                         // preset deviceName this is also your MDNS responder: http://iasblink.local

  memchr(groupname,0,sizeof(groupname));
  
//  IAS.addField(LEDpin, "ledpin", 2, 'P');                   // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
//  IAS.addField(blinkTime, "Blinktime(mS)", 5, 'N');         // reference to org variable | field label value | max char return

  IAS.addField(yourname, "YOUR NAME", 64, 'L');
  IAS.addField(groupname, "GROUP NAME", 64, 'L');


  // You can configure callback functions that can give feedback to the app user about the current state of the application.
  // In this example we use serial print to demonstrate the call backs. But you could use leds etc.
  IAS.onModeButtonShortPress([]() {
    Serial.println(F(" If mode button is released, I will enter in firmware update mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  });

  IAS.onModeButtonLongPress([]() {
    digitalWrite(LED1,HIGH);
    Serial.println(F(" If mode button is released, I will enter in configuration mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  });

  IAS.onConfigMode([]() {
    digitalWrite(LED1,HIGH);
    Serial.println(F(" Entering config mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  });

  IAS.onFirmwareUpdateDownload([]() {
    digitalWrite(LED1,HIGH);
    Serial.println(F("*-------------------------------------------------------------------------*"));
    Serial.println(F(" Downloading update."));
  });


  
   IAS.begin('P');                                           // Optional parameter: What to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact

  //IAS.setCallHome(true);                                    // Set to true to enable calling home frequently (disabled by default)
  //IAS.setCallHomeInterval(CALLHOME_INTERVAL);               // Call home interval in seconds, use 60s only for development. Please change it to at least 2 hours in production
	
  //-------- Your Setup starts from here ---------------

  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  digitalWrite(LEDR,LOW);
  digitalWrite(LEDG,LOW);
  digitalWrite(LEDB,LOW);

  anvticker.detach();
  digitalWrite(LED1,LOW);
  

  unsigned char mac[6];
  WiFi.macAddress(mac);
  
  for (int i = 3; i < 6; ++i) 
      nick += String(mac[i], 16);


  devicename=String(yourname);

  if (groupname=="") grupo="test";
  else grupo=String(groupname);
  
  char buf[5];
  uint16_t crc=calccrc(CHANNEL_PREFIX+grupo);
  sprintf(buf,"%04x",crc);
  
  channel=CHANNEL_PREFIX+String(buf);
 
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);

  digitalWrite(LED1,LOW);
   
  Serial.println("=====================================================================================================");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("nick: "+nick);
  Serial.println("channel: "+channel);
  Serial.println("=====================================================================================================");

}






byte errores=0;
///////////////////////////////////////////////////////////////////////////////
bool conectar_irc(){

anvticker.attach(0.3, blinkled);

Serial.println("iniciando conexion...");
client.stop();
delay(500);
if (!client.connect(host, 6667)) {
   Serial.println("No se pudo abir socket");
   errores++;
   //if (errores>20) 
   if (errores>20) {
       //ESP.restart();
       //ESP.reset();
       WiFi.forceSleepBegin(); wdt_reset(); ESP.restart(); while(1)wdt_reset();
       }
        
   return false;
   }  
errores=0;

    
Serial.println("Conectado, iniciando sesion.");

delay(1000);
while(client.available()) {
  char c = client.read();
  Serial.print(c);
  }



String extranick="";
int n=0;
byte r;
do {
   Serial.println("enviando nick "+nick+extranick);

   client.print("NICK "+nick+extranick+"\n");
   client.print("USER "+nick+extranick+" 8 * :"+nick+extranick+"\n");
   
   r=esperar_respuesta("Welcome","already in use",30);
   if (r==2) {
      n++;
      extranick=String(n,16);
      }
   } while (r==2);

if (r!=1) {
   Serial.println("Error al enviar nick");
   return false;
   }


Serial.println("enviando join a "+channel);
client.print("JOIN "+channel+"\r");
r=esperar_respuesta("JOIN "+channel,"",30);

Serial.println("Esperando '332 "+nick+extranick+" "+channel+" :' para obtener el topic");
r=esperar_respuesta("332 "+nick+extranick+" "+channel+" :","",10);
//if (r==1) 
procesar_topic(0);

hola=1;
digitalWrite(LED1,LOW);
anvticker.detach();  
}

unsigned long PingPreviousMillis = 0;
unsigned long MsgPreviousMillis = 0;
unsigned long TopicPreviousMillis = 0;

///////////////////////////////////////////////////////////////////////////////
bool verificar_conectado(){
if (WiFi.status() != WL_CONNECTED) {
   Serial.println("no hay conexion wifi");
   return false;
   }

if (!client.connected()) {
    return conectar_irc();
    }

unsigned long PingCurrentMillis = millis();
if (PingCurrentMillis - PingPreviousMillis >= PING_TIME) {  
    PingPreviousMillis = PingCurrentMillis;
    if (! verificar_ping()) return conectar_irc();
    }

return true;
}

//////////////////////////////////////////////////////////////////////////////
bool verificar_ping(){

String mensaje="ANV";
unsigned char mac[6];
WiFi.macAddress(mac);

for (int i = 0; i < 6; ++i) 
   mensaje += String(mac[i], 16);

Serial.println("enviando PING "+mensaje);

client.print("PING "+mensaje+"\n");

if (esperar_respuesta(mensaje,"",5)==1) {
  Serial.println("recibida respuesta a ping");
  return true;
  }

Serial.println("No se recibe respuesta al ping\n");    
return false;
}
/////////////////////////////////////////////////////////////////////////////
byte esperar_respuesta(String texto1,String texto2,unsigned int tiempo){
client.setTimeout(100);

for (int i=0;i<tiempo*10;i++){
    ultima_linea=client.readStringUntil('\r');
    if (ultima_linea.length()>0) Serial.println(ultima_linea);
    
    if (texto1.length()>0 && ultima_linea.indexOf(texto1)>0) return 1;
    if (texto2.length()>0 && ultima_linea.indexOf(texto2)>0) return 2;
    }
Serial.println("No se ha recibido ni '"+texto1+"' ni '"+texto2+"'");    
return 0;  
}

//////////////////////////////////////////////////////////////////////////////
bool cambiar_topic(String t){

uint16_t crc;
char buf[5];
crc=calccrc(t+grupo);
sprintf(buf,"%04x",crc);
  
client.println("TOPIC "+channel+" :"+String(buf)+t);

//return (esperar_respuesta("TOPIC "+channel+" :"+t,"",10)==1);

}




//////////////////////////////////////////////////////////////////////////////////////////


unsigned int hexToDec(String hexString) {
  
  unsigned int decValue = 0;
  int nextInt;
  
  for (int i = 0; i < hexString.length(); i++) {
    
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    
    decValue = (decValue * 16) + nextInt;
  }
  
  return decValue;
}
/////////////////////////////////////////////////////////////////////////////////////////

uint16_t calccrc(String s){
char buf[256];
uint16_t crc;

s.toCharArray(buf,sizeof(buf));

crc=uCRC16Lib::calculate(buf, strlen(buf));
return crc;
}

///////////////////////////////////////////////////////////////////////
void procesar_topic(byte b){    
uint16_t crc_recibido;
uint16_t crc_calculado;

String tmp;
int p;

String buscar;

if (b==1) 
  buscar="TOPIC "+channel+" :";
else 
  buscar=channel+" :";


p=ultima_linea.indexOf(buscar);

if (p<1) {
  if (b==0) {
     Serial.println("no se encuentra cadena de topic en >>>"+ultima_linea+"<<<<");
     Serial.print("Parece que el canal no tiene topic");
     cambiar_topic(String(estado_actual));
     Serial.println("Asignando "+String(estado_actual));
     }
  return;
  }

    
Serial.println("Cambio de topic del canal a '"+ultima_linea.substring(p+buscar.length())+"'");
//topic=ultima_linea.substring(p+buscar.length());



tmp=ultima_linea.substring(p+buscar.length()+4);

crc_recibido=hexToDec(ultima_linea.substring(p+buscar.length(),p+buscar.length()+4));
crc_calculado=calccrc(tmp+grupo);


Serial.print("ultima_linea="); Serial.println(ultima_linea);
Serial.print("buscar="); Serial.println(buscar);
Serial.print("p="); Serial.println(p);
Serial.print("cadena del crc="); Serial.println(ultima_linea.substring(p+buscar.length(),p+buscar.length()+4));
Serial.print("tmp="); Serial.println(tmp);
Serial.println(crc_recibido,HEX);
Serial.print("crc calculado=");
Serial.println(crc_calculado,HEX);




if (crc_recibido==crc_calculado){

   topic=tmp;
   estado_actual=topic.toInt();
   Serial.println("NUEVO TOPIC="+topic);
   color=estado_actual;
   showcolor();
   }
else {
   
   Serial.print("El topic no es valido: \ncrc recibido=");
   Serial.println(crc_recibido,HEX);
   Serial.print("crc calculado=");
   Serial.println(crc_calculado,HEX);
   cambiar_topic(String(estado_actual));
   Serial.println("Asignando "+String(estado_actual));
   }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void showcolor(){
digitalWrite(LEDR,(color & 1));
digitalWrite(LEDG,(color & 2)>>1);
digitalWrite(LEDB,(color & 4)>>2);
  
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleInterrupt() {

if ((millis() - lastDebounceTime)<DEBOUNCEDELAY) return;
  
lastDebounceTime = millis();
color++;
if (color>8) color=0;

showcolor();  
}

// ================================================ LOOP =================================================
void loop() {
  IAS.loop();                                // this routine handles the calling home on the configured itnerval as well as reaction of the Flash button. If short press: update of skethc, long press: Configuration


  //-------- Your Sketch starts from here ---------------

int p;

verificar_conectado();  
client.setTimeout(100);

if (client.available()){
  ultima_linea=client.readStringUntil('\r');

  Serial.println(ultima_linea);
  procesar_topic(1);    
  }

unsigned long TopicCurrentMillis = millis();
if (estado_actual!=color && TopicCurrentMillis - TopicPreviousMillis >= TOPIC_TIME) {  
    TopicPreviousMillis = TopicCurrentMillis;
    Serial.println("Color changed, sending topic change\n");
    cambiar_topic(String(color));
    }


unsigned long MsgCurrentMillis = millis();
if (hola || MsgCurrentMillis - MsgPreviousMillis >= MSG_TIME) {  
    MsgPreviousMillis = MsgCurrentMillis;
    client.print("PRIVMSG "+channel+" : "+devicename+" ");
    client.print(WiFi.localIP());
    client.print("\r");
    hola=0;
    }

   
//TODO:
// <-PING :asimov.freenode.net
// ->????????????????


}


