//test récupération du train en JSON 

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "Alarm.h" //fichier .h
#include "RTClib.h"
#include <LittleFS.h>

//instance de la class RTC_DS3231 

RTC_DS3231 rtc;

//instance de la class Alarm 

Alarm myAlarm;

//constantes pour le chemin d'accès vers le bon répertoire pour aller écrire/lire les infos du wifi

#define SSID_PATH "/wifi_ssid.txt"
#define PASS_PATH "/wifi_pass.txt"

int hour;
int minutes;
int alarmedHour;
int alarmMinutes;
String myTime; 
String Date;

//variables qui indiquent depuis quand le module RTC et le check de l'alarm à été mise a jour
unsigned long lastRTCUpdate = 0;
unsigned long lastAlarmCheck = 0;
bool colonBlink = false;

//variable qui contiennent les infos du train et quand elles doivent se mettre à jour

unsigned long lastTrainInfoUpdate = 0;
String arrivalTime;
String departurTime;
String arrivalStation;
String departureStation;
String trainNumber;
String duration;
String detailsDelayDeparture;
String detailsDelayArrival;

//variable qui stocke le temps du dernier événement
unsigned long lastDelayTime = 0;
unsigned long previousMillis = 0;

//définission des pins pour l'écran 
#define TFT_CS     10
#define TFT_RST    8
#define TFT_DC     9


Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

//déclaration des pin des boutons
int upButton = 3;
int downButton = 2;
int selectButton = 7;
int buzzerButton = 6;

//déclaration du pin pour le speaker
int speaker = 4;
bool toneState = false; // Variable pour alterner l'état du ton

//initialisation de la variable qui gère l'affichage du pressedButton et pressedButton principale
int pressedButton = 0;
int menu = 0;

//initialisation des variables qui vérifiront l'états des boutons (pressé ou non)
bool upButtonPressed = false;
bool downButtonPressed = false;
bool selectButtonPressed = false;
bool buzzerButtonPressed = false;

//booleen true si dans un écran

bool isIntoScreen = false;

//booleen si je suis dans l'écran SNCBAlarmClock

bool sncbAlarmCLock = false;

//booleen si je suis dans l'écran SNCB

bool SNCBScreen = false;

//booléen si je dois refresh le temps (si j'ai un écran qui affiche l'heure)

bool needToRefreshClock = false;

//booléen si une alarm à été set
bool alarmIsOn = false;

//booléen qui contient le status de l'alarm (Off ou On)

bool alarmStatus = false;

//booléen pour savoir si je suis dans l'écran setAlarm (si oui => true)

bool setAlarmClock = false;

//variable qui contiendront le nombre de secondes depuis lequel un bouton à été appuyé (éviter le rebond)
unsigned long lastUpButtonPress = 0;
unsigned long lastDownButtonPress = 0;
unsigned long lastSelectButtonPress = 0;
unsigned long lastBuzzerButtonPress = 0;


//déclaration des fonctions
void getRequestJson();
void displayPressed();
void startAlarmSpeaker();
void setupDisplay();
void SncbAlarmClockScreen();
void setupRTCModule();
void getCurrrentTimeDate();
void displayTime();
void blinkColon(bool blink);
void displayMenu();
void executeMenuOption(int option);
void moveRectangleMenu();
void SetAlarmScreen();
void manageSetAlarm();
void updateProgrammedTimeLCD(int digits[2]);
void checkAlarm();
void displayValueAlarm();
void handleTrainInfo(DynamicJsonDocument doc, DeserializationError error);
void handleDisplayTrainInfo();
void SncbScreen();
void handleDisplaySncbScreen();
String readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const String &message);
void displayWifiSetup();
void enterConfigMode();
void changeHourMinutesRtc(int hour, int minutes);

void setup() {
  Serial.begin(115200);

   if (!LittleFS.begin()) {
    Serial.println("An error occurred while mounting LittleFS");
    return;
  }

  pinMode(upButton, INPUT_PULLUP);
  pinMode(downButton, INPUT_PULLUP);
  pinMode(selectButton, INPUT_PULLUP);
  pinMode(buzzerButton, INPUT_PULLUP);
  myAlarm.begin(speaker); //Je set le pin 4 comme output et lié a l'objet alarm
  setupRTCModule();
  setupDisplay();

  Serial.println();

  // Lecture de la valeur du SSID et du mdp dans le fichier txt stocké sur la flash de l'arduino
  String ssid = readFile(LittleFS, SSID_PATH);
  String pass = readFile(LittleFS, PASS_PATH);


  //connexion au WIFI: si le fichier est vide -> je rentre dans le mode de configuration
  //                   si le fichier contient le mauvais mdp et/ou ssid, après 10 tentatives de connexions -> configuration mode 
  //                   si le fichier contient le bon mdp/ssid, connexion établie -> j'affiche le menu et le réveil démarre normalement
  if (ssid.length() > 0 && pass.length() > 0) {
    Serial.print("Connecting to WiFi ");
    displayWifiSetup();
    Serial.println(ssid);
    WiFi.begin(ssid.c_str(), pass.c_str());
    display.setCursor(94, 80);
    int attempts = 10; 
    while (WiFi.status() != WL_CONNECTED && attempts-- > 0) {
      delay(500);
      Serial.print(".");
      display.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected");
      display.setCursor(50, 135);
      display.print("Connexion etablie");
      displayMenu();
      return; //Sort du setup, va dans loop le réveil démarre
    } else {
      display.setCursor(50, 135);
      display.print("Connexion echouee");
      display.setCursor(11, 176);
      display.print("Mode de configuration: ON");
      Serial.println("\nFailed to connect to WiFi");
    }
  }
  enterConfigMode();
}

void loop() {
  alarmStatus = myAlarm.getStatus();
  if(millis()-lastRTCUpdate >= 10000){
    lastRTCUpdate = millis();
    displayTime();
  }

  if(millis()-lastAlarmCheck >=59000){
    lastAlarmCheck = millis();
    checkAlarm();
  }

  //je fetch les infos d'un train toute les 60 sec
  if(millis()-lastTrainInfoUpdate >= 60000){
    lastTrainInfoUpdate = millis();
    //fetch json
    getRequestJson();
    if(sncbAlarmCLock){
      handleDisplayTrainInfo();
    }
    if(SNCBScreen){
      handleDisplaySncbScreen();
    }
  }

  if (!digitalRead(upButton)) {
    if (!upButtonPressed) {
      upButtonPressed = true;
      lastUpButtonPress = millis();
    }
  } else {
    if (upButtonPressed && (millis() - lastUpButtonPress > 100)) {
      upButtonPressed = false;
      pressedButton = 1;
      displayPressed();
    }
  }

  if (!digitalRead(downButton)) {
    if (!downButtonPressed) {
      downButtonPressed = true;
      lastDownButtonPress = millis();
    }
  } else {
    if (downButtonPressed && (millis() - lastDownButtonPress > 100)) {
      downButtonPressed = false;
      pressedButton = 2;
      displayPressed();
    }
  }

  if (!digitalRead(selectButton)) {
    if (!selectButtonPressed) {
      selectButtonPressed = true;
      lastSelectButtonPress = millis();
    }
  } else {
    if (selectButtonPressed && (millis() - lastSelectButtonPress > 100)) {
      selectButtonPressed = false;
      pressedButton = 3;
      displayPressed();
    }
  }

  if (!digitalRead(buzzerButton)) {
    if (!buzzerButtonPressed) {
      buzzerButtonPressed = true;
      lastBuzzerButtonPress = millis();
    }
  } else {
    if (buzzerButtonPressed && (millis() - lastBuzzerButtonPress > 100)) {
      buzzerButtonPressed = false;
      pressedButton = 4;
      displayPressed();
    }
  }
  myAlarm.update(); //mise a jour de l'alarme (permet d'avoir le son saccadé sans rentrer dans une boucle bloquante)
}


//fonction qui récupère l'objet JSON depuis l'api

void getRequestJson(){

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    Serial.print("Requête HTTP GET...");
    http.begin("https://reveillotrain.bralion.be/getTrainForArduino/");
    int httpCode = http.GET(); 
      if (httpCode > 0) { // Vérifie si la requête HTTP a réussi
        Serial.printf("Code de réponse HTTP : %d\n", httpCode);
          if (httpCode == HTTP_CODE_OK) { // Vérifie si la réponse est OK
            DynamicJsonDocument doc(2048); // Crée un objet JSON dynamique
            DeserializationError error = deserializeJson(doc, http.getStream()); // Analyse la réponse JSON    
            if (!error) { // Vérifie si l'analyse JSON a réussi
              handleTrainInfo(doc, error);
              Serial.print("Nom du train : ");
              Serial.println(doc["name"].as<String>());
            } else {
              Serial.print("Erreur lors de l'analyse JSON : ");
              Serial.println(error.c_str());
            }
          }
        } else {
        Serial.printf("Échec de la requête HTTP : %s\n", http.errorToString(httpCode).c_str());
      }
    http.end(); // Libère les ressources du client HTTP
  }
}


void handleTrainInfo(DynamicJsonDocument doc, DeserializationError error){
  if(!error){
    arrivalTime = doc["arrivalTime"].as<String>();
    departurTime = doc["departureTime"].as<String>();
    departureStation = doc["stationDeparture"].as<String>();
    arrivalStation = doc["stationArrival"].as<String>();
    trainNumber = doc["name"].as<String>();
    duration = doc["duration"].as<String>();
    detailsDelayDeparture = doc["delayDeparture"].as<String>();
    detailsDelayArrival = doc["delayArrival"].as<String>();
  }else{
    arrivalTime = "ERROR";
    departurTime = "ERROR";
    departureStation = "ERROR";
    arrivalStation = "ERROr";
    trainNumber = "ERROR";
    duration = "ERROR";
    detailsDelayDeparture = "ERROR";
    detailsDelayArrival = "ERROR";
  }
}


void displayPressed(){
  Serial.println("menu = "+menu);
  switch (pressedButton){
    case 0:
      pressedButton = 1;
      break;
    case 1:
      Serial.println("Bouton up !");
      if(!isIntoScreen){
        menu++;
        //je change l'affichage du menu (je bouge le rectangle)
        displayMenu();
      }
      break;
    case 2:
      Serial.println("Bouton down");
      if(!isIntoScreen){
        menu--;
        //je change l'affichage du menu (je bouge le rectangle)
        displayMenu();
      }
      //startAlarmSpeaker();
      break;
    case 3:
      Serial.println("Bouton select");
      if(isIntoScreen){
        //je suis dans un écran donc j'affiche le menu et isIntoScreen = false 
        displayMenu();
        SNCBScreen = false;
      } else {
        //si je suis dans le menu et que j'appuie sur select, je dois afficher l'écran correspondant a la selection
        executeMenuOption(menu);
        isIntoScreen = true;
      }
      break;
    case 4:
      Serial.println("Bouton Buzzer");
      //si je ne suis pas en mode alarm ou dans le menu, je peux activer/désactiver l'alarm
      //j'active ou désactive l'alarm
      if(isIntoScreen && alarmStatus == false){
        alarmIsOn = !alarmIsOn;
        needToRefreshClock = false;
        sncbAlarmCLock = false;
        displayValueAlarm();
        delay(3000);
        //je retourne dans l'écran ou j'étais avant d'afficher la valeur de l'alarme 
        executeMenuOption(menu);
      }

      //si l'alarm est en cours de fonctionnement, je la désactive

      if(alarmStatus){
        myAlarm.stop();
      }
      break; 
  }
}


//fonction qui setup l'écran 
void setupDisplay(){
  display.begin();
  display.fillScreen(ILI9341_BLACK);
  display.setRotation(3);
  display.setTextSize(3);
  display.setTextColor(ILI9341_WHITE);
}

//display message progression connexion wifi

void displayWifiSetup(){
  display.setTextColor(0xFFFF);
  display.setTextSize(2);
  display.setCursor(57, 29);
  display.print("Connexion au WIFI ");
}

//displayMenu 

void displayMenu(){
  //setupDisplay();
  //test pour éviter le clignotement
  display.fillScreen(ILI9341_BLACK);
  needToRefreshClock = false;
  sncbAlarmCLock = false;
  isIntoScreen = false;
  //affichage pressedButton 
  static const unsigned char PROGMEM image_arrow_down_bits[] = {0x20,0x20,0x20,0x20,0xa8,0x70,0x20};
  static const unsigned char PROGMEM image_arrow_up_bits[] = {0x20,0x70,0xa8,0x20,0x20,0x20,0x20};
  static const unsigned char PROGMEM image_arrow_curved_down_right_bits[] = {0x78,0x38,0x78,0xe8,0xc0,0x40,0x40,0x20};
  static const unsigned char PROGMEM image_arrow_curved_down_left_bits[] = {0x20,0x40,0x40,0xc0,0xe8,0x78,0x38,0x78};
  display.drawBitmap(174, 3, image_arrow_down_bits, 5, 7, 0xFFFF);
  display.drawBitmap(214, 3, image_arrow_up_bits, 5, 7, 0xFFFF);
  display.setTextColor(0xFFFF);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(255, 3);
  display.print("Enter");
  display.setTextSize(4);
  display.setCursor(28, 77);
  display.print("Set Time");
  display.setCursor(30, 28);
  display.print("Set Alarm");
  display.setCursor(27, 132);
  display.print("Sncb/Alarm");
  display.setCursor(27, 186);
  display.print("Train Info");
  moveRectangleMenu();
}

//fonction qui va bouger le rectangle pour afficher le menu sélectionné
void moveRectangleMenu(){
  if (menu < 0) {
    menu = 3; // Si menu est inférieur à 0, revenir au dernier menu
  } else if (menu > 3) {
    menu = 0; // Si menu est supérieur à 3, revenir au premier menu
  }
  switch(menu) {
    case 0:
      display.drawRect(22, 19, 279, 49, 0xFFFF);
      //display.drawBitmap(12, 29, image_arrow_curved_down_right_bits, 5, 8, 0xFFFF);
      //display.drawBitmap(12, 48, image_arrow_curved_down_left_bits, 5, 8, 0xFFFF); 
      break;
    case 1:
      display.drawRect(22, 69, 279, 49, 0xFFFF);
      //display.drawBitmap(12, 78, image_arrow_curved_down_right_bits, 5, 8, 0xFFFF);
      //display.drawBitmap(12, 97, image_arrow_curved_down_left_bits, 5, 8, 0xFFFF);
      break;
    case 2:
      display.drawRect(22, 123, 279, 49, 0xFFFF);
      //display.drawBitmap(12, 132, image_arrow_curved_down_right_bits, 5, 8, 0xFFFF);
      //display.drawBitmap(12, 151, image_arrow_curved_down_left_bits, 5, 8, 0xFFFF);
      break;
    case 3:
      display.drawRect(22, 176, 279, 49, 0xFFFF);
      //display.drawBitmap(12, 185, image_arrow_curved_down_right_bits, 5, 8, 0xFFFF);
      //display.drawBitmap(12, 204, image_arrow_curved_down_left_bits, 5, 8, 0xFFFF);
      break;
  }

}

void executeMenuOption(int option) {
  switch(option) {
    case 0:
      // Action pour "Set Alarm"
      isIntoScreen = true;
      SetAlarmScreen();
      break;
    case 1:
      // Action pour changer l'heure
      isIntoScreen = true;
      ChangeHourScreen();
      break;
    case 2:
      isIntoScreen = true;
      sncbAlarmCLock = true;
      SncbAlarmClockScreen();
      break;
    case 3:
      isIntoScreen = true;
      needToRefreshClock = false;
      // Action pour "Train Info"
      SncbScreen();
      break;
  }
}


//Ecran SNCB

void SncbScreen(){
  setupDisplay();
  isIntoScreen = true;
  SNCBScreen = true;
  getRequestJson();
  handleDisplaySncbScreen();
  static const unsigned char PROGMEM image_SNCB_logoRedimensionne_svg_bits[] = {0x00,0x00,0x00,0x00,0x07,0xff,0xff,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xc0,0x00,0x00,0x00,0x00,0x00,0x01,0xff,0xff,0xe0,0x00,0x7f,0xff,0xf8,0x00,0x00,0x00,0x00,0x00,0x07,0xff,0xfc,0x00,0x00,0x03,0xff,0xfe,0x00,0x00,0x00,0x00,0x00,0x1f,0xff,0xc0,0x00,0x00,0x00,0x3f,0xff,0x80,0x00,0x00,0x00,0x00,0x7f,0xfe,0x00,0x00,0x00,0x00,0x07,0xff,0xe0,0x00,0x00,0x00,0x01,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0xff,0xf8,0x00,0x00,0x00,0x07,0xff,0xc0,0x00,0x00,0x00,0x00,0x00,0x3f,0xfc,0x00,0x00,0x00,0x0f,0xff,0x00,0x00,0x7f,0xff,0xc0,0x00,0x0f,0xff,0x00,0x00,0x00,0x1f,0xfe,0x00,0x03,0xff,0xff,0xf8,0x00,0x07,0xff,0x80,0x00,0x00,0x3f,0xf8,0x00,0x1f,0xff,0xff,0xfe,0x00,0x01,0xff,0xc0,0x00,0x00,0xff,0xf0,0x00,0x7f,0xff,0xff,0xff,0x80,0x00,0xff,0xe0,0x00,0x01,0xff,0xe0,0x01,0xff,0xff,0xff,0xff,0xe0,0x00,0x7f,0xf8,0x00,0x03,0xff,0x80,0x03,0xff,0xff,0xff,0xff,0xf0,0x00,0x3f,0xf8,0x00,0x03,0xff,0x00,0x0f,0xff,0xf8,0x07,0xff,0xf8,0x00,0x0f,0xfc,0x00,0x07,0xfe,0x00,0x1f,0xff,0xf8,0x03,0xff,0xfc,0x00,0x07,0xfe,0x00,0x0f,0xfe,0x00,0x1f,0xff,0xf8,0x01,0xff,0xfc,0x00,0x07,0xff,0x00,0x1f,0xfc,0x00,0x0f,0xff,0xf8,0x00,0xff,0xfe,0x00,0x03,0xff,0x80,0x1f,0xf8,0x00,0x07,0xff,0xf8,0x00,0xff,0xfe,0x00,0x01,0xff,0x80,0x3f,0xf0,0x00,0x03,0xff,0xf8,0x00,0x7f,0xfe,0x00,0x00,0xff,0xc0,0x3f,0xf0,0x00,0x00,0xff,0xf8,0x00,0x7f,0xfe,0x00,0x00,0xff,0xc0,0x7f,0xe0,0x00,0x00,0xff,0xf8,0x00,0x7f,0xfe,0x00,0x00,0x7f,0xe0,0x7f,0xe0,0x00,0x00,0xff,0xf8,0x00,0xff,0xfe,0x00,0x00,0x7f,0xe0,0x7f,0xc0,0x00,0x00,0xff,0xf8,0x00,0xff,0xfe,0x00,0x00,0x3f,0xe0,0xff,0xc0,0x00,0x00,0xff,0xf8,0x01,0xff,0xfc,0x00,0x00,0x3f,0xf0,0xff,0xc0,0x00,0x00,0xff,0xf8,0x03,0xff,0xfc,0x00,0x00,0x3f,0xf0,0xff,0xc0,0x00,0x00,0xff,0xf8,0x1f,0xff,0xf8,0x00,0x00,0x3f,0xf0,0xff,0x80,0x00,0x00,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x1f,0xf0,0xff,0x80,0x00,0x00,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x1f,0xf0,0xff,0x80,0x00,0x00,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x1f,0xf0,0xff,0x80,0x00,0x00,0xff,0xff,0xff,0xff,0xf8,0x00,0x00,0x1f,0xf0,0xff,0x80,0x00,0x00,0xff,0xff,0xff,0xff,0xfe,0x00,0x00,0x1f,0xf0,0xff,0x80,0x00,0x00,0xff,0xf8,0x07,0xff,0xff,0x00,0x00,0x1f,0xf0,0xff,0x80,0x00,0x00,0xff,0xf8,0x00,0xff,0xff,0x00,0x00,0x1f,0xf0,0xff,0x80,0x00,0x00,0xff,0xf8,0x00,0x7f,0xff,0x80,0x00,0x1f,0xf0,0xff,0xc0,0x00,0x00,0xff,0xf8,0x00,0x3f,0xff,0x80,0x00,0x3f,0xf0,0xff,0xc0,0x00,0x00,0xff,0xf8,0x00,0x1f,0xff,0xc0,0x00,0x3f,0xf0,0xff,0xc0,0x00,0x00,0xff,0xf8,0x00,0x1f,0xff,0xc0,0x00,0x3f,0xf0,0x7f,0xe0,0x00,0x00,0xff,0xf8,0x00,0x0f,0xff,0xc0,0x00,0x7f,0xe0,0x7f,0xe0,0x00,0x00,0xff,0xf8,0x00,0x0f,0xff,0xc0,0x00,0x7f,0xe0,0x3f,0xf0,0x00,0x00,0xff,0xf8,0x00,0x0f,0xff,0xc0,0x00,0xff,0xc0,0x3f,0xf0,0x00,0x02,0xff,0xf8,0x00,0x0f,0xff,0xc0,0x00,0xff,0xc0,0x1f,0xf8,0x00,0x07,0xff,0xf8,0x00,0x1f,0xff,0x80,0x01,0xff,0x80,0x1f,0xfc,0x00,0x0f,0xff,0xf8,0x00,0x1f,0xff,0x80,0x03,0xff,0x80,0x0f,0xfc,0x00,0x1f,0xff,0xf8,0x00,0x3f,0xff,0x00,0x03,0xff,0x00,0x07,0xfe,0x00,0x1f,0xff,0xf8,0x00,0x7f,0xff,0x00,0x07,0xfe,0x00,0x07,0xff,0x00,0x0f,0xff,0xf8,0x01,0xff,0xfe,0x00,0x0f,0xfe,0x00,0x03,0xff,0x80,0x07,0xff,0xff,0xff,0xff,0xfc,0x00,0x1f,0xfc,0x00,0x01,0xff,0xc0,0x01,0xff,0xff,0xff,0xff,0xf8,0x00,0x3f,0xf8,0x00,0x00,0xff,0xf0,0x00,0x7f,0xff,0xff,0xff,0xe0,0x00,0xff,0xf0,0x00,0x00,0x7f,0xf8,0x00,0x1f,0xff,0xff,0xff,0xc0,0x01,0xff,0xe0,0x00,0x00,0x1f,0xfc,0x00,0x07,0xff,0xff,0xfe,0x00,0x03,0xff,0x80,0x00,0x00,0x0f,0xff,0x00,0x00,0x7f,0xff,0xf0,0x00,0x0f,0xff,0x00,0x00,0x00,0x07,0xff,0xc0,0x00,0x00,0xf0,0x00,0x00,0x3f,0xfe,0x00,0x00,0x00,0x01,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0xff,0xf8,0x00,0x00,0x00,0x00,0xff,0xfc,0x00,0x00,0x00,0x00,0x03,0xff,0xf0,0x00,0x00,0x00,0x00,0x3f,0xff,0x80,0x00,0x00,0x00,0x1f,0xff,0xc0,0x00,0x00,0x00,0x00,0x0f,0xff,0xf8,0x00,0x00,0x01,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x03,0xff,0xff,0xc0,0x00,0x3f,0xff,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0xff,0xff,0xff,0xff,0xff,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xff,0xff,0xff,0xff,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00};
  static const unsigned char PROGMEM image_menu_information_sign_bits[] = {0x07,0xc0,0x18,0x30,0x20,0x08,0x43,0x04,0x43,0x04,0x80,0x02,0x83,0x82,0x83,0x02,0x83,0x02,0x83,0x02,0x43,0x04,0x43,0x84,0x20,0x08,0x18,0x30,0x07,0xc0,0x00,0x00};
  static const unsigned char PROGMEM image_clock_quarters_bits[] = {0x07,0xc0,0x19,0x30,0x21,0x08,0x40,0x04,0x41,0x04,0x81,0x02,0x81,0x02,0xe1,0x0e,0x80,0x82,0x80,0x42,0x40,0x04,0x40,0x04,0x21,0x08,0x19,0x30,0x07,0xc0,0x00,0x00};
  display.drawBitmap(105, 1, image_SNCB_logoRedimensionne_svg_bits, 100, 65, 0xFFFF);
  display.setTextColor(0xFFFF);
  display.setTextSize(2);
  display.setTextWrap(false);
  display.drawCircle(38, 120, 9, 0xFFFF);
  display.drawCircle(38, 155, 9, 0xFFFF);
  display.setTextSize(1);
  display.setCursor(36, 117);
  display.print("D");
  display.setCursor(36, 152);
  display.print("A");
  display.drawBitmap(53, 75, image_menu_information_sign_bits, 15, 16, 0xFFFF);
  display.setTextSize(2);
  display.drawBitmap(31, 183, image_clock_quarters_bits, 15, 16, 0xFFFF);
  display.drawRect(2, 69, 317, 166, 0xFFFF);
}

void handleDisplaySncbScreen(){
  Serial.println("refresh ecran SNCB");
  display.fillRect(73, 72, 163, 24, ILI9341_BLACK);
  display.fillRect(50, 100, 264, 125, ILI9341_BLACK);
  display.setTextSize(2);
  display.setCursor(78, 76);
  display.print(trainNumber);
  display.setCursor(55, 113);
  display.print(departureStation);
  display.setCursor(55, 148);
  display.print(arrivalStation);

  if(detailsDelayDeparture != "0"){
    display.setCursor(210, 114);
    display.print("+"+detailsDelayDeparture+"min");
  }
  if(detailsDelayArrival != "0"){
    display.setCursor(222, 148);
    display.print("+"+detailsDelayArrival+"min");
  }
  display.setCursor(54, 183);
  display.print(duration+"min "+"("+departurTime+"-"+arrivalTime+")");
}

//Ecran changement d'heure
void ChangeHourScreen(){
  setupDisplay();
  isIntoScreen = true;
  setAlarmClock = false;
  static const unsigned char PROGMEM image_arrow_up_bits[] = {0x20,0x70,0xa8,0x20,0x20,0x20,0x20};
  static const unsigned char PROGMEM image_menu_tools_bits[] = {0x80,0xe0,0xc1,0x60,0x42,0x80,0x22,0x8c,0x13,0x0c,0x0a,0xb4,0x06,0x48,0x05,0xf0,0x0b,0x00,0x14,0xe0,0x29,0xb0,0x50,0xd8,0xa0,0x6c,0xc0,0x34,0x00,0x1c,0x00,0x00};
  static const unsigned char PROGMEM image_arrow_down_bits[] = {0x20,0x20,0x20,0x20,0xa8,0x70,0x20};
  display.setTextColor(0xFFFF);
  display.setTextSize(2);
  display.setCursor(64, 34);
  display.print("Reglage de l'heure");
  display.setTextSize(1);
  display.setCursor(74, 151);
  display.print("");
  display.setTextSize(6);
  display.setCursor(74, 102);
  display.print("00:00");
  display.drawBitmap(104, 53, image_arrow_up_bits, 5, 7, 0xFFFF);
  display.drawBitmap(43, 33, image_menu_tools_bits, 14, 16, 0xFFFF);
  display.drawBitmap(104, 109, image_arrow_down_bits, 5, 7, 0xFFFF);
  manageSetAlarm();
}




//Ecran SNCB/AlarmClock

void SncbAlarmClockScreen(){
  setupDisplay();
  isIntoScreen = true;
  needToRefreshClock = true;
  displayTime(); 
  getRequestJson();
  handleDisplayTrainInfo();
  static const unsigned char PROGMEM image_clock_alarm_bits[] = {0x79,0x3c,0xb3,0x9a,0xed,0x6e,0xd0,0x16,0xa0,0x0a,0x41,0x04,0x41,0x04,0x81,0x02,0xc1,0x06,0x82,0x02,0x44,0x04,0x48,0x04,0x20,0x08,0x10,0x10,0x2d,0x68,0x43,0x84};
  static const unsigned char PROGMEM image_checked_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x0c,0x00,0x1c,0x80,0x38,0xc0,0x70,0xe0,0xe0,0x71,0xc0,0x3b,0x80,0x1f,0x00,0x0e,0x00,0x04,0x00,0x00,0x00,0x00,0x00};
  static const unsigned char PROGMEM image_pressedButton_bits[] = {0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00};
  static const unsigned char PROGMEM image_clock_quarters_bits[] = {0x07,0xc0,0x19,0x30,0x21,0x08,0x40,0x04,0x41,0x04,0x81,0x02,0x81,0x02,0xe1,0x0e,0x80,0x82,0x80,0x42,0x40,0x04,0x40,0x04,0x21,0x08,0x19,0x30,0x07,0xc0,0x00,0x00};
  static const unsigned char PROGMEM image_pressedButton_information_sign_white_bits[] = {0x07,0xc0,0x18,0x30,0x23,0x08,0x42,0x84,0x43,0x04,0x80,0x02,0x83,0x82,0x82,0x82,0x82,0x82,0x82,0x82,0x42,0x84,0x43,0x84,0x20,0x08,0x18,0x30,0x07,0xc0,0x00,0x00};
  static const unsigned char PROGMEM image_location_bits[] = {0x0f,0x80,0x30,0x60,0x40,0x10,0x47,0x10,0x88,0x88,0x90,0x48,0x90,0x48,0x50,0x50,0x48,0x90,0x27,0x20,0x20,0x20,0x10,0x40,0x08,0x80,0x05,0x00,0x07,0x00,0x02,0x00};
  static const unsigned char PROGMEM image_date_day_bits[] = {0x00,0x00,0x09,0x20,0x7f,0xfc,0xc9,0x26,0x80,0x02,0x8c,0x22,0x92,0x62,0x92,0xa2,0x82,0x22,0x84,0x22,0x88,0x22,0x90,0x22,0x9e,0xf2,0xc0,0x06,0x7f,0xfc,0x00,0x00};
  display.drawLine(-3, 127, 320, 127, 0xFFFF);
  if(alarmIsOn){
    display.drawBitmap(6, 5, image_clock_alarm_bits, 15, 16, 0xFFFF);
  }
  display.setTextColor(0xFFFF);
  display.setTextSize(6);
  display.setTextWrap(false);
  //display.setCursor(75, 40);
  //display.print("00:00");
  display.drawBitmap(29, 5, image_checked_bits, 14, 16, 0xFFFF);
  display.drawBitmap(299, 5, image_pressedButton_bits, 16, 16, 0xFFFF);
  display.drawLine(0, 0, 0, 0, 0xFFFF);
  display.setTextSize(3);
  display.setCursor(128, 136);
  display.print("SNCB");
  display.drawBitmap(9, 163, image_clock_quarters_bits, 15, 16, 0xFFFF);
  //display.setTextSize(2);
  //display.setCursor(32, 164);
  //display.print("On time");
  display.drawBitmap(9, 216, image_pressedButton_information_sign_white_bits, 15, 16, 0xFFFF);
  //display.setCursor(32, 217);
  //display.print("IC 3225");
  display.drawBitmap(10, 189, image_location_bits, 13, 16, 0xFFFF);
  //display.setCursor(32, 190);
  //display.print("Ath-Tournai");
  display.drawBitmap(248, 108, image_date_day_bits, 15, 16, 0xFFFF);
}

//fonction qui va refresh les infos du train/les afficher pour l'écran SNCB/AlarmClock
void handleDisplayTrainInfo(){
  Serial.println("refresh ecran info train");
  display.fillRect(32, 164, 248, 74, ILI9341_BLACK);
  display.setTextSize(2);
  display.setCursor(32, 164);
  if(detailsDelayDeparture == "0"){
    display.print("On time");
  } else{
    display.print("Delay: "+ detailsDelayDeparture + " min");
  }
  display.setCursor(32, 217);
  display.print(trainNumber);
  display.setCursor(32, 190);
  display.print(departureStation+"-"+arrivalStation);
}


//écran Set Alarm

void SetAlarmScreen(){
  setupDisplay();
  setAlarmClock = true;
  static const unsigned char PROGMEM image_file_download_bits[] = {0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x09,0x20,0x07,0xc0,0xe3,0x8e,0xa1,0x0a,0xbf,0xfa,0x80,0x02,0x80,0x02,0xff,0xfe,0x00,0x00,0x00,0x00};
  display.setTextColor(0xFFFF);
  display.setTextSize(6);
  display.setTextWrap(false);
  display.setCursor(74, 102);
  display.print("00:00");
  display.setTextSize(3);
  display.setCursor(46, 43);
  display.print("Set the alarm");
  display.setTextSize(1);
  display.setCursor(189, 9);
  display.print("Hour");
  display.drawBitmap(284, 5, image_file_download_bits, 15, 16, 0xFFFF);
  display.setCursor(227, 9);
  display.print("Minute");
  manageSetAlarm();
}

//gestion du set Alarm

void manageSetAlarm(){

  unsigned long currentMillis = millis();
  int programmedHour = 0;
  int programmedMinute = 0;

  int digitIndex = 0; // Indice du chiffre en cours de modification (0 pour l'heure, 1 pour les minutes)
  int digits[2] = { programmedHour, programmedMinute }; // Tableau pour stocker les chiffres programmés
  bool enterPressed = false;
  while (enterPressed == false) {
    if (!digitalRead(downButton)) {
      if (!downButtonPressed) {
        downButtonPressed = true;
        lastDownButtonPress = millis();
      }
    } else {
      if (downButtonPressed && (millis() - lastDownButtonPress > 50)) {
        downButtonPressed = false;
        digits[digitIndex]--;
        if (digits[digitIndex] < 0){
          if(digitIndex == 0){
            digits[digitIndex] = 23; // Remettre à 23 si on descend en dessous de 0 pour l'heure
          }
          if(digitIndex == 1){
            digits[digitIndex] = 59; // Remettre à 59 si on descend en dessous de 0 pour les minutes
          }
        }
        updateProgrammedTimeOLED(digits);
      }
    }

    if (!digitalRead(upButton)) {
      if (!upButtonPressed) {
        upButtonPressed = true;
        lastUpButtonPress = millis();
      }
    } else {
      if (upButtonPressed && (millis() - lastUpButtonPress > 50)) {
        upButtonPressed = false;
        digits[digitIndex]++;
        if (digits[digitIndex] > 23 && digitIndex == 0) {
          digits[digitIndex] = 0; // Remettre à 0 si on dépasse 23 (heures)
        }
        if (digits[digitIndex] > 59 && digitIndex == 1) {
          digits[digitIndex] = 0; // Remettre à 0 si on dépasse 59 (minutes et secondes)
        }
        updateProgrammedTimeOLED(digits);
      }
    }

    if (!digitalRead(selectButton)) {
      if (!selectButtonPressed) {
        selectButtonPressed = true;
        lastSelectButtonPress = millis();
      }
    } else {
      if (selectButtonPressed && (millis() - lastSelectButtonPress > 50)) {
        selectButtonPressed = false;
        digitIndex++;
        if (digitIndex > 2) {
          enterPressed = true; //sortie de la boucle
        }
        updateProgrammedTimeOLED(digits);
      }
    }
    
  }
  // Programmation terminée, mettre à jour les variables d'alarme si dans l'écran set alarm
  if(setAlarmClock){
    alarmIsOn = true; 
    alarmedHour = digits[0];
    alarmMinutes = digits[1];
    //informe l'utilisateur l'alarm qui est set
    if (currentMillis - previousMillis >= 3000) {
      displayValueAlarm();
    }
    return;
  } else {
    //Mettre à jour les variable du RTC si dans l'écran set Time
    changeHourMinutesRtc(digits[0], digits[1]);
    return;
  }

}

//ecran qui affiche la valeur de l'alarme et l'active ou la désactive quand on clique sur le buzzer 
void displayValueAlarm(){
    display.fillRect(0, 0, 320, 240, ILI9341_BLACK);
    display.setRotation(3);
    display.setTextSize(3);
    display.setCursor(84, 42);
    display.print("Alarm Set");
    if(alarmIsOn){
      display.print(" On");
    } else {
      display.print(" Off");
    }
    display.setTextSize(6);
    display.setCursor(74, 102);
    display.print((alarmedHour < 10 ? "0" : "") + String(alarmedHour) + ":" + (alarmMinutes < 10 ? "0" : "") + String(alarmMinutes));
}

//effacer l'ancien chiffre et afficher le resultat
void updateProgrammedTimeOLED(int digits[2]){
  display.setRotation(3);
  display.fillRect(64, 91, 200, 61, ILI9341_BLACK); //j'efface l'ancienne heure
  display.setTextSize(6);
  display.setCursor(74, 102);
  display.print(digits[0] < 10 ? "0" + String(digits[0]) : String(digits[0])); //conditions ternaires (condition ? valeur_si_vrai : valeur_si_faux) pour formater les chiffres avec un zéro devant s'ils sont inférieurs à 10. 
  display.print(":");
  display.print(digits[1] < 10 ? "0" + String(digits[1]) : String(digits[1]));
}

//fonction qui check si le temps introduit dans l'alarm est égale au temps actuel. Oui=>déclenchement de l'alarme
void checkAlarm(){
  Serial.println("Check de l'alarm");
  if(alarmIsOn && alarmedHour == hour && alarmMinutes == minutes){
    Serial.println("Alarm déclenchée, dans checkAlarm");
    myAlarm.play();
  }
}



//----------------------------------------------------------------------------------------------

//constructeur Alarm
Alarm::Alarm()
: playing(false)
, buzzerState(false)
, lastTonTime(0) {}

void Alarm::begin(int pin) {
  Pin = pin;
  pinMode(Pin, OUTPUT);
}

//fonction qui va déclencher le "bip bip" de l'alarm. Elle sera appellée dans la boucle loop qui est non bloquante
void Alarm::update() {
  if (playing) {
    unsigned long currentMillis = millis();

    if (buzzerState) {
      // Si le buzzer est allumé, vérifie si le temps de ton est écoulé
      if (currentMillis - lastTonTime >= tonDuration) {
        noTone(Pin);
        buzzerState = false;
        lastTonTime = currentMillis;
      }
    } else {
      // Si le buzzer est éteint, vérifie si le temps de pause est écoulé
      if (currentMillis - lastTonTime >= pauseDuration) {
        tone(Pin, 500); // fréquence de 500 Hz
        buzzerState = true;
        lastTonTime = currentMillis;
      }
    }
  }
}

void Alarm::play() {
  playing = true;
  buzzerState = false;
  lastTonTime = millis();
}

void Alarm::stop() {
  noTone(Pin);
  playing = false;
  buzzerState = false;
}

bool Alarm::getStatus() {
  return playing;
}



//----------------------------------------------------------------------------------------------


//fonction qui setup le module RTC
void setupRTCModule(){

  if (! rtc.begin()) {
    while (1) delay(10);
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

//fonction pour changer l'heure (et uniquement l'heure) si il y a un décalage
void changeHourMinutesRtc(int hour, int minutes){
  DateTime now = rtc.now();
  rtc.adjust(DateTime(now.year(), now.month(), now.day(), hour, minutes, 0));
}

//fonction qui récupère l'heure sur le rtc

void getCurrrentTimeDate(){
  DateTime now = rtc.now();
  Date = String(now.day()) + "-" + String(now.month()) + "-" + String(now.year());
  Serial.println("Date: "+Date);
  hour = now.hour(), DEC; 
  minutes = now.minute(), DEC; 
  //permet d'afficher un 0 si les minutes sont inférieur à 10
  myTime = (hour < 10 ? "0" : "") + String(hour) + ":" + (minutes < 10 ? "0" : "") + String(minutes);
  Serial.println(myTime);
}

//fonction qui affiche l'heure et gère la mise a jour de l'heure sur l'ecran SNCB/AlarmClock

void displayTime(){
  getCurrrentTimeDate();

  if(needToRefreshClock){
    // Efface l'heure précédente en dessinant un rectangle noir sur la zone de l'heure précédente
    display.setRotation(3);
    display.fillRect(70, 30, 190, 54, ILI9341_BLACK);
    display.fillRect(264, 109, 70, 18, ILI9341_BLACK);


    // Affiche la nouvelle heure
    display.setTextSize(6);
    display.setTextColor(ILI9341_WHITE);
    display.setCursor(75, 40);
    display.print(myTime);

    //affiche la nouvelle date 
    display.setTextSize(1);
    display.setCursor(267, 114);
    display.print(Date);
  }
}

//méthode de lecture et d'écriture sur la FLASH de l'arduino

String readFile(fs::FS &fs, const char *path) {
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("Failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const String &message) {
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written successfully");
  } else {
    Serial.println("File write failed");
  }
  file.close();
}

//menu de configuration: il écris et recoit le ssid et le mdp envoyé par l'application JAVA via le port COM

void enterConfigMode() {
  display.fillScreen(ILI9341_BLACK);
  display.setTextColor(0xFFFF);
  display.setTextSize(1);
  display.setCursor(72, 98);
  display.print("En attente du SSID et du MDP");
  display.setTextSize(2);
  display.setCursor(49, 31);
  display.print("Configuration Mode");

  while (true) {
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      if (input.startsWith("SSID:")) {
        String ssid = input.substring(5);
        writeFile(LittleFS, SSID_PATH, ssid);
        Serial.println("SSID saved.");
        Serial.println("Enter Password:");
      } else if (input.startsWith("PASS:")) {
        String pass = input.substring(5);
        writeFile(LittleFS, PASS_PATH, pass);
        Serial.println("Password saved.");
        Serial.println("Restarting to apply new settings...");
        display.setCursor(72, 147);
        display.setTextSize(1);
        display.print("MDP et SSID recu, redemarrage");
        delay(2000);
        ESP.restart();
      }
    }
  }
}

