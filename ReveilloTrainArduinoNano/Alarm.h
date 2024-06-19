#ifndef ALARM_H
#define ALARM_H

class Alarm {
  public:
    Alarm(); // constructeur 
    void begin(int pin); // méthode qui affecte un pin pour l'alarme
    void update(); // méthode qui gère l'état de l'alarme
    void play(); // méthode qui déclenche l'alarme
    void stop(); // méthode qui stoppe l'alarme
    bool getStatus(); // méthode qui indique si l'alarme est en cours de fonctionnement ou pas

  private: // attributs privés
    int Pin;
    bool playing;
    bool buzzerState; // nouvel attribut pour suivre l'état du buzzer
    unsigned long lastTonTime;
    static const unsigned long tonDuration = 500; // durée du ton en ms
    static const unsigned long pauseDuration = 100; // durée de la pause entre les "bips" en ms
};

#endif
