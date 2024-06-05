#ifndef ALARM_H
#define ALARM_H

class Alarm{
  public:
    Alarm(); //constructeur 
    void begin(int pin); //méthode qui affecte un pin pour l'alarm
    void play(); //méthode qui déclenche l'alarme
    void stop(); //méthode qui stop l'alarme
    bool getStatus(); //méthode qui indique si l'alarme est en cours de fonctionnement ou pas


  private: //attributs privé
    int Pin;
    bool playing;
    unsigned long lastTonTime;
};
#endif