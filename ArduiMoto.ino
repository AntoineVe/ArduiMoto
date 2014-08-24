#include <DHT.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <stdlib.h>

// Définie le capteur DHT
#define DHTPIN 30
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int CoolingTemp = A0;	// Capteur TMP36

// Définie les pin pour l'écran LCD
const int pinLCD_RS = 5; 
const int pinLCD_Enable = 6;
const int pinLCD_D4 = 11;
const int pinLCD_D5 = 10;
const int pinLCD_D6 = 9;
const int pinLCD_D7 = 8;

char ssid[] = "WiFi**********";	//  SSID
char pass[] = "************";	// Mot de passe (WPA)
int status = WL_IDLE_STATUS;	// the Wifi radio's status

//IPAddress ip(192, 168, 1, 200);  // FIXME : En manuel,
//IPAddress gw(192, 168, 1, 1);    // impossible de faire
//IPAddress ns(192, 168, 1, 3);    // du broadcast.
//IPAddress mask(255, 255, 255, 0);
IPAddress server(255, 255, 255, 255);

int portudp = 3865;	// Port UDP pour le transfer des données. (xPL = 3865)

WiFiUDP Udp;	// Initialise l'UDP

unsigned long timer = 0;	// Le timer (compteur de temps depuis la mise en marche, 
							// unsigned long permet plus d'un mois de fonctionnement.

int count = 0;	// compteur pour la boucle
							
int TMP36(int capteur) {					// Cette fonction permet le calcul de la
	int mesure = analogRead(capteur);		// température depuis un capteur TMP36,
	float tension = (mesure/1024.0) * 5.0;	// prenant comme argument le pin du capteur
	int temperature = round((tension - 0.5) * 100);
	return temperature;
}

void SendUdPMessage(char *buffer) {		// fonction d'envoie à travers le datagram
	Udp.beginPacket(server, portudp);
	Udp.write(buffer);
	Udp.endPacket();
}

void xPL_hbeat() {
	String msg;
	char udp_msg[64];
	msg = "hbeat.basic\n{\ninterval=10\nversion=0.1\n}";
	msg.toCharArray(udp_msg, 64);
	SendUdPMessage(udp_msg);
}

void xPL_Timer() { // timer.basic
	String msg;
	char udp_msg[256];
	msg = "timer.basic\n{\ndevice=Arduino_Kawasaki\ntype=generic\ncurrent=started\nelapsed=";
	msg += millis()/1000;
	msg += "\n}";
	msg.toCharArray(udp_msg, 256);
	SendUdPMessage(udp_msg);
}

void xPL_Cooling(int cooling) {	// Envoie la température du circuit de refroidissement
	String msg;
	char udp_msg[256];
	msg = "sensor.basic\n{\ndevice=cooling\ntype=temp\nunits=c\ncurrent=";
	msg += cooling;
	msg += "\n}";
	msg.toCharArray(udp_msg, 256);
	SendUdPMessage(udp_msg);
}

void xPL_Humidity(int humidity) {	// Envoie le pourcentage d'humidité
	String msg;
	char udp_msg[256];
	msg = "sensor.basic\n{\ndevice=humidity\ntype=humidity\ncurrent=";
	msg += humidity;
	msg += "\n}";
	msg.toCharArray(udp_msg, 256);
	SendUdPMessage(udp_msg);
}

// Fonction de l'écran LCD
LiquidCrystal lcd(pinLCD_RS, pinLCD_Enable, pinLCD_D4, pinLCD_D5, pinLCD_D6, pinLCD_D7);

// Initialise certaine variable
int Cooling_last = -128;		// Place le dernier enregistrement à -128 car il n'y en a jamais eu. (valeur arbitraire)
int Humidity_last = -128;
int Cooling_now;
int humidity_now;
int t;

void setup()
{
//	Serial.begin(9600);	// Serial pour le debogage, c'est pratique
//	Mode des PIN des capteurs
	pinMode(CoolingTemp, INPUT);
	if (WiFi.status() == WL_NO_SHIELD) {
//		Serial.println("WiFi shield not present");
		while(true);
	}
//	WiFi.config(ip, ns, gw);
	if ( status != WL_CONNECTED) { // Essaie de se connecter une première fois au démarrage
		status = WiFi.begin(ssid, pass);
		delay(5000);
	}
	if ( status == WL_CONNECTED) {
		Udp.begin(portudp);
		xPL_hbeat(); // Envoie le premier heartbeat xPL
	}
	
	lcd.begin(16, 2); // Initialise un écran LCD 16 lignes / 2 colonnes
	
}

void loop() {
	if (count % 2 == 0) { // Mesure toutes les secondes la temperature
		Cooling_now = TMP36(CoolingTemp);
	}
	if (count % 60 ==0) { // Mesure l'humidité toutes les 30 secondes 
		humidity_now = dht.readHumidity();
	}
//	int packetSize = Udp.parsePacket();  // TODO : action xPL
//	if(packetSize) {
//		PacketAction(); // Exécute l'action si demandée
//	}
	if ( status == WL_CONNECTED) {		// Uniquement si connecté
		if (count % 20 == 0) {			// Envoie le HBEAT toutes les 10 secondes
			xPL_hbeat();
			count = 0;
//		}
//		if (Cooling_last != Cooling_now) {		// Envoie le TRIG uniquement si modification
			xPL_Cooling(Cooling_now);			// comme le prévoie le protocole xPL
//			Cooling_last = Cooling_now;			// FIXME : Le test ne fonctionne pas ?
//		}										// WORKAROUND : envoie en même temps que le heartbeat
//		if (Humidity_last != humidity_now); {
			xPL_Humidity(humidity_now);
//			Humidity_last = humidity_now;
		}
	}
	if ((millis()-timer) >= 500) {	// Mise à jour de l'affichage LCD toutes les 1/2 secondes
		lcd.setCursor(0, 0);		
		lcd.print(Cooling_now);
		lcd.setCursor(0, 1);
		lcd.print(humidity_now);
		timer = millis();
		count = count + 1;	// et mise en place du compteur par unité de 500 ms
	}
	if (count % 10 == 0) {				// retente la connexion toutes les 5 secondes
		if ( status != WL_CONNECTED) {	// si non connecté
			status = WiFi.begin(ssid, pass);
		}
	}
	if (count == 120) {	// replace le compteur "count" à 0 aprés 120, soit 1 minute.
		count = 0;
	}
}