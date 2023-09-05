// ---------------- LIBRERIAS -------
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <DHT_U.h>
#include <PID_v1_bc.h>

#include <ArduinoJson.h>

// ---------------- PINES -----------
//Boton de seguridad
#define BUTTON_PIN 3

//Controladores
#define RESISTENCIA 13
#define HUMIDIFICADOR 2

//Sensores
#define SENSORTEMP 11
#define DHTPIN 12    // Digital pin connected to the DHT sensor 
#define PIEL A4
#define CALIDAD A1
#define LED_on A5

// LCD
const int pin_RS = 8; 
const int pin_EN = 9; 
const int pin_d4 = 4; 
const int pin_d5 = 5; 
const int pin_d6 = 6; 
const int pin_d7 = 7; 
const int pin_BL = 10;

//LEDs ALARMAs
const int LED_alarma1= A2;
const int LED_alarma2 = A3;
 



//-------------------- ALARMAS
const int tmin_piel = 33; 
const int tmax_piel = 39;
const int tmin_aire = 19; 
const int tmax_aire = 40; //Esas prenden el LED grande

const int rango_temp = 1; //El delta aceptable entre control y t medida

const int hum_min = 20; 
const int hum_max = 91;

const int rango_hum = 10;
int estadoLED;


// ---------------- VARIABLES Y ESTADOS INICIALES
//Lecturas
float TemperaturaAmbiente=25;
float Temp_controlar;
float TemperaturaPiel=15;

double Humedad=80;

int calidadAire;

//Estados controladores
byte Estado_rele=0; //0 es off - 1 es on
byte Estado_humidificador=0; //0 es off - 1 es on

//SetPoints
float SetPointTemp = 34;
double SetPointHum = 40;

//Modos
//byte Cambiomodo=0; 
byte ModoTemp=0;// 1 es piel 0 es aire

//Estado sensores
int estadoAire = 1;
int estadoPiel = 1;
int estadoHumedad = 1;
int estadoCO2= 1;
float PielAnterior = 30;
int calidadAnterior = 200;

//Estados alarmas
int estadoAlarmaMediaHumedad = 0;
int estadoAlarmaAltaHumedad = 0;

int estadoAlarmaMediaTempAire = 0;
int estadoAlarmaAltaTempAire = 0;

int estadoAlarmaMediaTempPiel = 0;
int estadoAlarmaAltaTempPiel = 0;

//Estado SIN alarmas
int inicializando = 1;


//Variaciones Hum y Temp
float listaTemp[]={0, 100};
float variacionTemp;
float listaHumedad[]={0, 100};
float variacionHum;

// ---------------- INICIOS 
//Pantalla
LiquidCrystal lcd( pin_RS,  pin_EN,  pin_d4,  pin_d5,  pin_d6,  pin_d7);

//Sensor Temp
OneWire oneWire(SENSORTEMP);  // KY-001 Signal pin is connected to Arduino pin 8
DallasTemperature sensors(&oneWire);    // create DallasTemperature object and pass
                                        //  oneWire object as parameter
// Sensor Hum
#define DHTTYPE    DHT11 // type of sensor
DHT_Unified dht(DHTPIN, DHTTYPE);

//PID
//Specify the links and initial tuning parameters
double Kp=136.3, Ki=50.27, Kd=92.2951;
double Output;
PID myPID(&Humedad, &Output, &SetPointHum, Kp, Ki, Kd, DIRECT);

// ---------------- TIEMPOS 
//Cambio de pantalla
int PeriodoPantalla =2500;
unsigned long windowStartTimePantalla;
int Pantalla=0;
int limPantalla = 1;

// DHT
uint32_t windowStartTimeDHT;
uint32_t PeriodoDHT;
unsigned long windowStartTimeDHT_CONTROL;

// Botones Display
int PeriodoBotones = 150;
unsigned long windowStartTimeButtons;

//Parpadeo Alarmas
unsigned long MillisAnteriores = 0;
const long intervaloParpadeo = 60;

//Inicializacion (Estado SIN alarmas)
unsigned long windowStartTimeInicio;
unsigned long intervaloInicio = 5*60000;

//Medición variaciones
const long periodoVariacionHum = 100000;
unsigned long windowStartTimeVariacionHum=700000;

const long periodoVariacionTemp = 60000;
unsigned long windowStartTimeVariacionTemp=700000;

void setup() {  
  //Inicio Comunicación Serial
  Serial.begin(115200);
  Serial2.begin(9600);
  
  //Inicio Ventanas de tiempo
  windowStartTimeButtons = millis();
  windowStartTimeDHT=millis();
  windowStartTimePantalla=millis();
  windowStartTimeDHT_CONTROL = millis(); 
  windowStartTimeInicio = millis();
  
  //--------- Entradas
  // Boton
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // Sensor DHT
  sensor_t sensor;
  dht.begin();
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  PeriodoDHT= sensor.min_delay / 1000;
  // Sensor Temp
  sensors.begin();
  
  //--------- Salidas
  // Resistencia
  pinMode(RESISTENCIA, OUTPUT);
  digitalWrite(RESISTENCIA,HIGH); //lo dejamos apagado por ahora
  //LED ALARMAS
  pinMode(LED_on,OUTPUT);
  digitalWrite(LED_on, HIGH); //El de prendido todo el tiempo
  pinMode(LED_alarma1,OUTPUT);
  pinMode(LED_alarma2,OUTPUT);
  //Humidificador
  pinMode(HUMIDIFICADOR, OUTPUT);

  //PID
  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, PeriodoDHT);   
  //turn the PID on
  myPID.SetMode(AUTOMATIC); //ESTA

  // Display  
  lcd.begin(16, 2);
  lcd.setCursor(4,0);
  lcd.print("INCUBALA");
  delay(5000);
  for (int positionCounter = 0; positionCounter < 16; positionCounter++) {
   lcd.scrollDisplayLeft();
   delay(50);
  }
  lcd.clear();
  
  StaticJsonDocument<200> jsonDocInicial;
  jsonDocInicial["SPH"] = SetPointHum;
  jsonDocInicial["SPT"] = SetPointTemp;
  String jsonStringInicial;
  serializeJson(jsonDocInicial, jsonStringInicial);
  Serial2.println(jsonStringInicial);

  StaticJsonDocument<400> jsonDocInicial2;
  jsonDocInicial2["Conexion Sensor Humedad"] = estadoHumedad;
  jsonDocInicial2["Conexion Sensor TempAire"] = estadoAire;
  jsonDocInicial2["Conexion Sensor TempPiel"] = estadoPiel;
  jsonDocInicial2["Modo"] = ModoTemp;
  jsonDocInicial2["Rele"] = Estado_rele;
  jsonDocInicial2["Inicializando"] = 1;
  String jsonStringInicial2;
  serializeJson(jsonDocInicial2, jsonStringInicial2);
  Serial2.println(jsonStringInicial2);

  StaticJsonDocument<500> jsonDocInicial3;
  jsonDocInicial3["Estado Alarma Media Humedad"] = 0;
  jsonDocInicial3["Estado Alarma Alta Humedad"] = 0;
  jsonDocInicial3["Estado Alarma Media TempAire"] = 0;
  jsonDocInicial3["Estado Alarma Alta TempAire"] = 0;
  jsonDocInicial3["Estado Alarma Media TempPiel"] = 0;
  jsonDocInicial3["Estado Alarma Alta TempPiel"] = 0;
  jsonDocInicial3["Hola"] = 100000;
  String jsonStringInicial3;
  serializeJson(jsonDocInicial3, jsonStringInicial3);
  Serial2.println(jsonStringInicial3);
  
}

void loop() {
  // -------------------------------------- TEMPERATURA ---------------------------------------
  
  // ---- Medición Aire
  sensors.requestTemperatures();                                // Command to get temperatures
  TemperaturaAmbiente=sensors.getTempCByIndex(0);
  TemperaturaAmbiente=(TemperaturaAmbiente+0.19)*0.9944083+0.01; //Calibración
  //Serial.print(TemperaturaAmbiente);
  listaMinMax(TemperaturaAmbiente, listaTemp);

  
  //Variación Aire
  if (millis()-windowStartTimeVariacionTemp > periodoVariacionTemp && inicializando==0){
    variacionTemp = (listaTemp[0]-listaTemp[1])/2;
    Serial.println("variacionTemp");
    Serial.print(variacionTemp);
    listaTemp[0] = TemperaturaAmbiente;
    listaTemp[1] = TemperaturaAmbiente;

    windowStartTimeVariacionTemp=millis();
    StaticJsonDocument<200> jsonDocVariacionTemp;
    jsonDocVariacionTemp["variacionTemp"] = variacionTemp;
    String jsonStringvariacionTemp;
    serializeJson(jsonDocVariacionTemp, jsonStringvariacionTemp);
    Serial2.println(jsonStringvariacionTemp);
}
  
  // ---- Medición Piel
  PielAnterior = TemperaturaPiel;
  
  TemperaturaPiel = analogRead(PIEL)*5/1024.0;
  TemperaturaPiel = TemperaturaPiel - 0.5;
  TemperaturaPiel = TemperaturaPiel / 0.01;
  


  // ---- Control
  if (ModoTemp ==0){
    Temp_controlar=TemperaturaAmbiente;
   }
  else{
    Temp_controlar=TemperaturaPiel;
   }
  
  if ((Temp_controlar>=SetPointTemp or estadoAire==false) and Estado_rele==1){                             //Apago resistencia
    digitalWrite(RESISTENCIA,HIGH);
    Estado_rele=0;
    //Serial.println("Estado rele:");
    //Serial.println(Estado_rele);
    StaticJsonDocument<200> jsonDocRele;
    jsonDocRele["Rele"] = Estado_rele;
    String jsonStringRele;
    serializeJson(jsonDocRele, jsonStringRele);
    Serial2.println(jsonStringRele);

  }
  else if(Estado_rele==0 && Temp_controlar<=SetPointTemp ){
    digitalWrite(RESISTENCIA,LOW);                              //Prendo resistencia
    Estado_rele=1;
    //Serial.println("Estado rele:");
    //Serial.println(Estado_rele);
    StaticJsonDocument<200> jsonDocRele;
    jsonDocRele["Rele"] = Estado_rele;
    String jsonStringRele;
    serializeJson(jsonDocRele, jsonStringRele);
    Serial2.println(jsonStringRele);    
  }
  
  
  // -------------------------------------- HUMEDAD --------------------------------------------
  // ---- Medición
  if(millis() - windowStartTimeDHT>PeriodoDHT){
    windowStartTimeDHT=millis();
    sensors_event_t event;
    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    Humedad=event.relative_humidity;
    listaMinMax(Humedad, listaHumedad);
  }
  //Variación Hum
  if (millis()-windowStartTimeVariacionHum > periodoVariacionHum && inicializando==0){
    
    variacionHum = (listaHumedad[0]-listaHumedad[1])/2;
    Serial.println("variacionHum");
    Serial.print(variacionHum);
    listaHumedad[0] = Humedad;
    listaHumedad[1] = Humedad;
    windowStartTimeVariacionHum=millis();

    StaticJsonDocument<200> jsonDocVariacionHum;
    jsonDocVariacionHum["variacionHum"] = variacionHum;
    String jsonStringvariacionHum;
    serializeJson(jsonDocVariacionHum, jsonStringvariacionHum);
    Serial2.println(jsonStringvariacionHum);
  }

  //Serial.print(Humedad);
  //Serial.print(",");
  //Serial.println(SetPointHum);
  
  // ---- Control
  myPID.Compute();
  int dutyCycle = 0;
  if (millis() - windowStartTimeDHT_CONTROL > PeriodoDHT)
  { //time to shift the Relay Window
    windowStartTimeDHT_CONTROL += PeriodoDHT;
  }
  if (Output < millis() - windowStartTimeDHT_CONTROL){
    dutyCycle = map(Output, 0, PeriodoDHT, 0, 100);
    //dutyCycle = constrain(dutyCycle, 0, 100);
    //erial.println("DC");
    //Serial.println(dutyCycle);
    StaticJsonDocument<200> jsonDocHumidificador;
    jsonDocHumidificador["DC"] = dutyCycle;
    String jsonStringHumidificador;
    serializeJson(jsonDocHumidificador, jsonStringHumidificador);
    Serial2.println(jsonStringHumidificador); 

    if (Estado_humidificador == 1){
      pulso();
      //Serial.print("off");
      Estado_humidificador = 0; 
      /*
      StaticJsonDocument<200> jsonDocHumidificador;
      jsonDocHumidificador["Humidificador"] = Estado_humidificador;
      String jsonStringHumidificador;
      serializeJson(jsonDocHumidificador, jsonStringHumidificador);
      Serial2.println(jsonStringHumidificador); 
      */
    }

  }
  else{
    if (Estado_humidificador ==0){
      apagar();
      //Serial.print("on");
      Estado_humidificador = 1;
      /*
      StaticJsonDocument<200> jsonDocHumidificador;
      jsonDocHumidificador["Humidificador"] = Estado_humidificador;
      String jsonStringHumidificador;
      serializeJson(jsonDocHumidificador, jsonStringHumidificador);
      Serial2.println(jsonStringHumidificador); 
      */
    }
  }
  
  // -------------------------------------- CALIDAD AIRE --------------------------------------------
  calidadAnterior = calidadAire;
  calidadAire = analogRead(CALIDAD);
  // -------------------------------------- DISPLAY --------------------------------------------
  checkConexion();
  show_Pantalla();
  if (inicializando == 0){
    checkAlarmas();
  }
  if (digitalRead(BUTTON_PIN) == LOW) {
    cambiarSetPoint(); 
  }
  /*
  Serial.print(TemperaturaAmbiente);
  Serial.print(",");
  Serial.print(SetPointTemp);
  Serial.print(",");
  Serial.print(Estado_rele);
  Serial.print(",");
  Serial.print(Humedad);
  Serial.print(",");
  Serial.println(SetPointHum);
  */
  //------------------------------------- INICIALIZANDO -----------------------------------------
  
  if ((((millis()-windowStartTimeInicio) > intervaloInicio) or (Temp_controlar>=SetPointTemp))  && inicializando == 1){
    windowStartTimeVariacionHum = millis();
    windowStartTimeVariacionTemp = millis();
    Serial.println("fin de inic");

    inicializando = 0;
    StaticJsonDocument<200> jsonDocInicial;
    jsonDocInicial["Inicializando"] = inicializando;
    String jsonStringInicial;
    serializeJson(jsonDocInicial, jsonStringInicial);
    Serial2.println(jsonStringInicial);
  }
}

void show_Pantalla(){
  if(millis() - windowStartTimePantalla >PeriodoPantalla){
    if (ModoTemp ==0){
      if (estadoAire == false or estadoHumedad == false or estadoCO2 == false){
        limPantalla = 2;
      }
      else{
        limPantalla = 1;
      }
    }
    else{
      if (estadoPiel == false or estadoHumedad == false or estadoCO2 == false){
        limPantalla = 2;
      }
      else{
        limPantalla = 1;
      }
    }
    
    if (Pantalla==2){
      if (ModoTemp ==0){
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("ErrorDesconexion");
          if (estadoAire==false){
            lcd.setCursor(0,1);
            lcd.print("TA");
          }
          if (estadoHumedad==false){
            lcd.setCursor(6,1);
            lcd.print("H");
          }
          if (estadoCO2==false){
            lcd.setCursor(8,1);
            lcd.print("CO2");
          }
          Pantalla = 0;
        }
      else if (ModoTemp ==1){
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("ErrorDesconexion");
          if (estadoAire == false){
            lcd.setCursor(0,1);
            lcd.print("TA");
          }
          if (estadoPiel==false){
            lcd.setCursor(3,1);
            lcd.print("TP");
          }
          if (estadoHumedad==false){
            lcd.setCursor(6,1);
            lcd.print("H");
          }
          if (estadoCO2==false){
            lcd.setCursor(8,1);
            lcd.print("CO2");
          }
          Pantalla = 0; 
       }    
      }

    else if (Pantalla ==0){
    //Primer pantalla (Temp del modo y Humedad)
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Humedad:"); 
      lcd.print(Humedad);
      lcd.print("%");
      lcd.setCursor(0,1);

      StaticJsonDocument<200> jsonDoc;
      jsonDoc["Humedad Valor"] = Humedad;

      if (ModoTemp ==0){
        lcd.print("TempAire:");
        lcd.print(TemperaturaAmbiente);
        jsonDoc["Temp Aire Valor"] = TemperaturaAmbiente;
        }
      else{
        lcd.print("TempPiel:");
        lcd.print(TemperaturaPiel);
        jsonDoc["Temp Piel Valor"] = TemperaturaPiel;
      } 
      lcd.print((char)223);   //Grados 
      lcd.print("C"); 
      // Convertir el objeto JSON en una cadena
      String jsonString;
      serializeJson(jsonDoc, jsonString);
      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonString);
      Pantalla +=1;
    }
    else if(Pantalla ==1){
    //Segunda Pantalla (Modo, calidad del aire y TA si estoy en Modo Piel)
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Modo:");
      StaticJsonDocument<200> jsonDocPantalla2;
      if (ModoTemp ==0){
        lcd.print("A");
        
        lcd.print(" TP:");
        if (estadoPiel==true){
          lcd.print(TemperaturaPiel);
          lcd.setCursor(14,0);
          lcd.print((char)223);   //Grados 
          lcd.print("C");
          jsonDocPantalla2["Temp Piel Valor"] = TemperaturaPiel;
        }
        else{
          lcd.print("--");
        }
      }
      else{
        lcd.print("P");
        lcd.print(" TA:");
        lcd.print(TemperaturaAmbiente);
        lcd.setCursor(14,0);
        lcd.print((char)223);   //Grados 
        lcd.print("C"); 
        jsonDocPantalla2["Temp Aire Valor"] = TemperaturaAmbiente;
      }
      lcd.setCursor(0,1);
      lcd.print("CO2:");
      lcd.print(calidadAire);
      lcd.print("PPM ");
      jsonDocPantalla2["calidadAire"] = calidadAire;

      // Convertir el objeto JSON en una cadena
      String jsonStringPantalla2;
      serializeJson(jsonDocPantalla2, jsonStringPantalla2);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringPantalla2);

      if (limPantalla == 2){
        Pantalla=2;
      }
      else{
        Pantalla=0;
      }
    }
  windowStartTimePantalla=millis();
  return;
  }
}

void checkConexion(){
  if (TemperaturaAmbiente < -120){
    if (estadoAire ==true){
      StaticJsonDocument<200> jsonDocestadoAire;
      jsonDocestadoAire["Conexion Sensor TempAire"] = 0;

      // Convertir el objeto JSON en una cadena
      String jsonStringestadoAire;
      serializeJson(jsonDocestadoAire, jsonStringestadoAire);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringestadoAire);
    }
    estadoAire = false;
  }
  else if(TemperaturaAmbiente > -120){
    if (estadoAire ==false){
      StaticJsonDocument<200> jsonDocestadoAire;
      jsonDocestadoAire["Conexion Sensor TempAire"] = 1;

      // Convertir el objeto JSON en una cadena
      String jsonStringestadoAire;
      serializeJson(jsonDocestadoAire, jsonStringestadoAire);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringestadoAire);
    }
    estadoAire = true;
  }
  if ((abs(TemperaturaPiel - PielAnterior) > 20) || TemperaturaPiel >50){
    if (estadoPiel == true){
      StaticJsonDocument<200> jsonDocestadoPiel;
      jsonDocestadoPiel["Conexion Sensor TempPiel"] = 0;

      // Convertir el objeto JSON en una cadena
      String jsonStringestadoPiel;
      serializeJson(jsonDocestadoPiel, jsonStringestadoPiel);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringestadoPiel);
    }
    estadoPiel = false;
    if (ModoTemp == 1){
      ModoTemp = 0;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("DESCONEXION MP");
      lcd.setCursor(0,1);
      lcd.print("Cambio a MA");
      StaticJsonDocument<200> jsonDocModoTemp;
      jsonDocModoTemp["Modo"] = ModoTemp;

      // Convertir el objeto JSON en una cadena
      String jsonStringModoTemp;
      serializeJson(jsonDocModoTemp, jsonStringModoTemp);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringModoTemp);
      delay(1500);
    }
  }
  else if (abs(TemperaturaPiel - PielAnterior) < 20){
    if (estadoPiel == false){
      StaticJsonDocument<200> jsonDocestadoPiel;
      jsonDocestadoPiel["Conexion Sensor TempPiel"] = 1;

      // Convertir el objeto JSON en una cadena
      String jsonStringestadoPiel;
      serializeJson(jsonDocestadoPiel, jsonStringestadoPiel);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringestadoPiel);
    }
    estadoPiel = true;
  }
  
  if (isnan(Humedad)){
    if (estadoHumedad ==true){
      StaticJsonDocument<200> jsonDocestadoHumedad;
      jsonDocestadoHumedad["Conexion Sensor Humedad"] = 0;

      // Convertir el objeto JSON en una cadena
      String jsonStringestadoHumedad;
      serializeJson(jsonDocestadoHumedad, jsonStringestadoHumedad);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringestadoHumedad);
    }
    estadoHumedad = false;
  }
  else if (not isnan(Humedad)){
    if (estadoHumedad == false){
      StaticJsonDocument<200> jsonDocestadoHumedad;
      jsonDocestadoHumedad["Conexion Sensor Humedad"] = 1;

      // Convertir el objeto JSON en una cadena
      String jsonStringestadoHumedad;
      serializeJson(jsonDocestadoHumedad, jsonStringestadoHumedad);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringestadoHumedad);
    }
    estadoHumedad = true;
  }
  if (abs(calidadAire - calidadAnterior) > 30){
    if (estadoCO2 ==true){
      StaticJsonDocument<200> jsonDocestadoCO2;
      jsonDocestadoCO2["Conexion Sensor Calidad"] = 0;

      // Convertir el objeto JSON en una cadena
      String jsonStringestadoCO2;
      serializeJson(jsonDocestadoCO2, jsonStringestadoCO2);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringestadoCO2);
    }
    estadoCO2 = false;
  }
  else if (abs(calidadAire - calidadAnterior) < 30){
    if (estadoCO2 ==false){
      StaticJsonDocument<200> jsonDocestadoCO2;
      jsonDocestadoCO2["Conexion Sensor Calidad"] = 1;

      // Convertir el objeto JSON en una cadena
      String jsonStringestadoCO2;
      serializeJson(jsonDocestadoCO2, jsonStringestadoCO2);

      // Enviar la cadena JSON a través de Serial2
      Serial2.println(jsonStringestadoCO2);
    }
    estadoCO2 = true;
  }
  return;
}

void checkAlarmas(){
  //------------------------------------- Humedad y Temperatura Ambiente ----------------------------------
  // --------------- ALARMAS ALTAS --------------------------------------
  //Activar LED 
  if (TemperaturaAmbiente > tmax_aire or TemperaturaAmbiente < tmin_aire or Humedad > hum_max or Humedad < hum_min){
      parpadeo(LED_alarma2);
    }
   else{
      digitalWrite(LED_alarma2, LOW);
   }

  //Publicar cambio de estado Humedad
  if (Humedad > hum_max or Humedad < hum_min){
    if (estadoAlarmaAltaHumedad==0){
      StaticJsonDocument<200> jsonDocestadoAHumedad;
      jsonDocestadoAHumedad["Estado Alarma Alta Humedad"] = 1;
      String jsonStringestadoAHumedad;
      serializeJson(jsonDocestadoAHumedad, jsonStringestadoAHumedad);
      Serial2.println(jsonStringestadoAHumedad);
    }
    estadoAlarmaAltaHumedad = 1;
  }
  else{
    if (estadoAlarmaAltaHumedad==1){
      StaticJsonDocument<200> jsonDocestadoAHumedad;
      jsonDocestadoAHumedad["Estado Alarma Alta Humedad"] = 0;
      String jsonStringestadoAHumedad;
      serializeJson(jsonDocestadoAHumedad, jsonStringestadoAHumedad);
      Serial2.println(jsonStringestadoAHumedad);
    }
    estadoAlarmaAltaHumedad = 0;
  }
  //  Publicar cambio de estado Temp Aire
  if (TemperaturaAmbiente > tmax_aire or TemperaturaAmbiente < tmin_aire){
    if (estadoAlarmaAltaTempAire==0){
      StaticJsonDocument<200> jsonDocestadoATemp;
      jsonDocestadoATemp["Estado Alarma Alta TempAire"] = 1;
      String jsonStringestadoATemp;
      serializeJson(jsonDocestadoATemp, jsonStringestadoATemp);
      Serial2.println(jsonStringestadoATemp);
    }
    estadoAlarmaAltaTempAire = 1;
  }
  else{
    if (estadoAlarmaAltaTempAire==1){
      StaticJsonDocument<200> jsonDocestadoATemp;
      jsonDocestadoATemp["Estado Alarma Alta TempAire"] = 0;
      String jsonStringestadoATemp;
      serializeJson(jsonDocestadoATemp, jsonStringestadoATemp);
      Serial2.println(jsonStringestadoATemp);
    }
    estadoAlarmaAltaTempAire = 0;
  }
  // --------------- ALARMAS MEDIAS  --------------------------------------
      //Publicar cambio de estado Media Humedad
  if ((Humedad > SetPointHum+rango_hum or Humedad < SetPointHum-rango_hum) and estadoAlarmaAltaHumedad==0){
    if (estadoAlarmaMediaHumedad==0){
      StaticJsonDocument<200> jsonDocestadoMHumedad;
      jsonDocestadoMHumedad["Estado Alarma Media Humedad"] = 1;
      String jsonStringestadoMHumedad;
      serializeJson(jsonDocestadoMHumedad, jsonStringestadoMHumedad);
      Serial2.println(jsonStringestadoMHumedad);
    }
    estadoAlarmaMediaHumedad = 1;
  }
  else{
    if (estadoAlarmaMediaHumedad==1){
      StaticJsonDocument<200> jsonDocestadoMHumedad;
      jsonDocestadoMHumedad["Estado Alarma Media Humedad"] = 0;
      String jsonStringestadoMHumedad;
      serializeJson(jsonDocestadoMHumedad, jsonStringestadoMHumedad);
      Serial2.println(jsonStringestadoMHumedad);
    }
    estadoAlarmaMediaHumedad = 0;
  }
  if (ModoTemp == 0){
    if (TemperaturaAmbiente > SetPointTemp + rango_temp or TemperaturaAmbiente < SetPointTemp - rango_temp or Humedad > SetPointHum+rango_hum or Humedad < SetPointHum-rango_hum){
      parpadeo(LED_alarma1);
    }
    else{
      digitalWrite(LED_alarma1, LOW);
    }
  //  Publicar cambio de estado Temp Aire 
    if ((TemperaturaAmbiente > SetPointTemp + rango_temp or TemperaturaAmbiente < SetPointTemp - rango_temp) and estadoAlarmaAltaTempAire==0){
      if (estadoAlarmaMediaTempAire==0){
        StaticJsonDocument<200> jsonDocestadoMTemp;
        jsonDocestadoMTemp["Estado Alarma Media TempAire"] = 1;
        String jsonStringestadoMTemp;
        serializeJson(jsonDocestadoMTemp, jsonStringestadoMTemp);
        Serial2.println(jsonStringestadoMTemp);
      }
      estadoAlarmaMediaTempAire = 1;
    }
    else{
      if (estadoAlarmaMediaTempAire==1){
        StaticJsonDocument<200> jsonDocestadoMTemp;
        jsonDocestadoMTemp["Estado Alarma Media TempAire"] = 0;
        String jsonStringestadoMTemp;
        serializeJson(jsonDocestadoMTemp, jsonStringestadoMTemp);
        Serial2.println(jsonStringestadoMTemp);
      }
      estadoAlarmaMediaTempAire = 0;
    }
  }
  //


  // ------------------------------------------- TEMP PIEL ------------------------------
  // ALTA
  if (estadoPiel == 1){
    if (TemperaturaPiel > tmax_piel or TemperaturaPiel < tmin_piel){
      parpadeo(LED_alarma2);
    }
    else{
      digitalWrite(LED_alarma2, LOW);
    }
    if (TemperaturaPiel > tmax_piel or TemperaturaPiel < tmin_piel){
      if (estadoAlarmaAltaTempPiel==0){
        StaticJsonDocument<200> jsonDocestadoATempPiel;
        jsonDocestadoATempPiel["Estado Alarma Alta TempPiel"] = 1;
        String jsonStringestadoATempPiel;
        serializeJson(jsonDocestadoATempPiel, jsonStringestadoATempPiel);
        Serial2.println(jsonStringestadoATempPiel);
      }
      estadoAlarmaAltaTempPiel = 1;
    }
    else{
      if (estadoAlarmaAltaTempPiel==1){
        StaticJsonDocument<200> jsonDocestadoATempPiel;
        jsonDocestadoATempPiel["Estado Alarma Alta TempPiel"] = 0;
        String jsonStringestadoATempPiel;
        serializeJson(jsonDocestadoATempPiel, jsonStringestadoATempPiel);
        Serial2.println(jsonStringestadoATempPiel);
      }
      estadoAlarmaAltaTempPiel = 0;
    }
  }

  //MEDIA
  if (ModoTemp == 1){ 
    if (TemperaturaPiel > SetPointTemp + rango_temp or TemperaturaPiel < SetPointTemp - rango_temp){
      parpadeo(LED_alarma1);
    }
    else{
      digitalWrite(LED_alarma1, LOW);
    }
    if ((TemperaturaPiel > SetPointTemp + rango_temp or TemperaturaPiel < SetPointTemp - rango_temp) and estadoAlarmaAltaTempPiel==0){
      if (estadoAlarmaMediaTempPiel==0){
        StaticJsonDocument<200> jsonDocestadoMTempPiel;
        jsonDocestadoMTempPiel["Estado Alarma Media TempPiel"] = 1;
        String jsonStringestadoMTempPiel;
        serializeJson(jsonDocestadoMTempPiel, jsonStringestadoMTempPiel);
        Serial2.println(jsonStringestadoMTempPiel);
      }
      estadoAlarmaMediaTempPiel = 1;
    }
    else{
      if (estadoAlarmaMediaTempPiel==1){
        StaticJsonDocument<200> jsonDocestadoMTempPiel;
        jsonDocestadoMTempPiel["Estado Alarma Media TempPiel"] = 0;
        String jsonStringestadoMTempPiel;
        serializeJson(jsonDocestadoMTempPiel, jsonStringestadoMTempPiel);
        Serial2.println(jsonStringestadoMTempPiel);
      }
      estadoAlarmaMediaTempPiel = 0;
    }
  }
 return;
}

void cambiarSetPoint(){ //Falta para modo Aire-Piel
  lcd.clear();
  //delay(800); 
  int boton;
  while(1){
    lcd.setCursor(0,0);
    lcd.print("SPH:"); 
    lcd.print(SetPointHum);
    lcd.setCursor(0,1);
    lcd.print("SPT:"); 
    lcd.print(SetPointTemp);
    lcd.setCursor(11,1);
    lcd.print("M:");    
    lcd.setCursor(13,1);
    if (ModoTemp == 0){
      lcd.print('A');
    }
    else{
      lcd.print('P');
    }
    if (digitalRead(BUTTON_PIN) == HIGH){
        break;
      }
    if(millis() - windowStartTimeButtons>PeriodoBotones){
      boton = analogRead (0);
      // Para temp (Up-Down)
      //windowStartTimeButtons += PeriodoBotones;
      windowStartTimeButtons=millis();
      //Serial.print(digitalRead(BUTTON_PIN));
      if (boton <60){
        if (ModoTemp ==0 and estadoPiel == false){ //Sensor de piel desconectado, se mantiene en aire
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("ERROR CONEXION");
          lcd.setCursor(0,1);
          lcd.print("MP no disponible");
          delay(2000);
          lcd.clear();
        }
        else{
          ModoTemp = !ModoTemp;
          StaticJsonDocument<200> jsonDocModoTemp;
          jsonDocModoTemp["Modo"] = ModoTemp;
          String jsonStringModoTemp;
          serializeJson(jsonDocModoTemp, jsonStringModoTemp);
          Serial2.println(jsonStringModoTemp);
          
        }
        if (ModoTemp == 1){
          if (SetPointTemp > 38){
            SetPointTemp = 38;
          StaticJsonDocument<200> jsonDocSPT;
          jsonDocSPT["SPT"] = SetPointTemp;
          String jsonStringSPT;
          serializeJson(jsonDocSPT, jsonStringSPT);
          Serial2.println(jsonStringSPT);
          }
          if (SetPointTemp < 34){
            SetPointTemp = 34;
            StaticJsonDocument<200> jsonDocSPT;
            jsonDocSPT["SPT"] = SetPointTemp;
            String jsonStringSPT;
            serializeJson(jsonDocSPT, jsonStringSPT);
            Serial2.println(jsonStringSPT);
          }            
        }
      } 
      else if (boton < 200){
        if (ModoTemp == 0 and SetPointTemp < 39){
          SetPointTemp = SetPointTemp+0.5;
          StaticJsonDocument<200> jsonDocSPT;
          jsonDocSPT["SPT"] = SetPointTemp;
          String jsonStringSPT;
          serializeJson(jsonDocSPT, jsonStringSPT);
          Serial2.println(jsonStringSPT);
        }
        else if (ModoTemp == 1 and SetPointTemp < 38){
          SetPointTemp = SetPointTemp+0.5;
          StaticJsonDocument<200> jsonDocSPT;
          jsonDocSPT["SPT"] = SetPointTemp;
          String jsonStringSPT;
          serializeJson(jsonDocSPT, jsonStringSPT);
          Serial2.println(jsonStringSPT);
        }
      }
      else if (boton < 400){
        if (ModoTemp == 0 and SetPointTemp > 20){
          SetPointTemp = SetPointTemp-0.5;
          StaticJsonDocument<200> jsonDocSPT;
          jsonDocSPT["SPT"] = SetPointTemp;
          String jsonStringSPT;
          serializeJson(jsonDocSPT, jsonStringSPT);
          Serial2.println(jsonStringSPT);
        }
        else if (ModoTemp == 1 and SetPointTemp > 34){
          SetPointTemp = SetPointTemp-0.5;
          StaticJsonDocument<200> jsonDocSPT;
          jsonDocSPT["SPT"] = SetPointTemp;
          String jsonStringSPT;
          serializeJson(jsonDocSPT, jsonStringSPT);
          Serial2.println(jsonStringSPT);
        }

      }
    // Para humedad (Left y select)
      else if (boton < 600 and SetPointHum < 80){
        SetPointHum = SetPointHum+1;
        StaticJsonDocument<200> jsonDocSPH;
        jsonDocSPH["SPH"] = SetPointHum;

        // Convertir el objeto JSON en una cadena
        String jsonStringSPH;
        serializeJson(jsonDocSPH, jsonStringSPH);

        // Enviar la cadena JSON a través de Serial2
        Serial2.println(jsonStringSPH);
      }
      else if (boton < 800 and SetPointHum > 40){
        SetPointHum = SetPointHum-1;
        StaticJsonDocument<200> jsonDocSPH;
        jsonDocSPH["SPH"] = SetPointHum;

        // Convertir el objeto JSON en una cadena
        String jsonStringSPH;
        serializeJson(jsonDocSPH, jsonStringSPH);

        // Enviar la cadena JSON a través de Serial2
        Serial2.println(jsonStringSPH);
      }
    }
  }
}

void parpadeo(int NRO_LED){ 
  unsigned long MillisActual = millis();
  if(MillisActual - MillisAnteriores >= intervaloParpadeo){
    if (estadoLED == LOW){
        estadoLED = HIGH;
    }
    else{
        estadoLED = LOW;
    }
    MillisAnteriores = MillisActual;
  }
  digitalWrite(NRO_LED, estadoLED);
  return;
}

void pulso(){
  digitalWrite(HUMIDIFICADOR, LOW);
  delay(300);
  digitalWrite(HUMIDIFICADOR, HIGH);
  delay(50);
}

void apagar(){
  pulso();
  delay(50);
  pulso();
}

void listaMinMax(float valor, float* lista){
  if (valor>lista[0]){
    lista[0]=valor;
  }
  if (valor<lista[1]){
    lista[1]=valor;
  }
}