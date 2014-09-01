#include <Servo.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <stdlib.h>

// Définie le capteur DHT
#define DHTPIN 2		// Attention, pin < 32 !
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

Servo ctrlralenti;

const int CoolingTemp = A0;	// Capteur TMP36

// Définie les pin pour l'écran LCD
const int pinLCD_RS = 40; 
const int pinLCD_Enable = 38;
const int pinLCD_D0 = 36;
const int pinLCD_D1 = 34;
const int pinLCD_D2 = 32;
const int pinLCD_D3 = 30;
const int pinLCD_D4 = 28;
const int pinLCD_D5 = 26;
const int pinLCD_D6 = 24;
const int pinLCD_D7 = 22;

char ssid[] = "*****";	//  SSID
char pass[] = "*****";	// Mot de passe (WPA)
int status = WL_IDLE_STATUS;	// the Wifi radio's status

//IPAddress ip(192, 168, 1, 200);  // WONTFIX : En manuel,
//IPAddress gw(192, 168, 1, 1);    // impossible de faire
//IPAddress ns(192, 168, 1, 3);    // du broadcast. Le DHCP c'est bien.
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

void xPL_dhtTemp(int temp) {
	String msg;
	char udp_msg[256];
	msg = "sensor.basic\n{\ndevice=outside\ntype=temp\nunits=c\ncurrent=";
	msg += temp;
	msg += "\n}";
	msg.toCharArray(udp_msg, 256);
	SendUdPMessage(udp_msg);
}

// Fonction de l'écran LCD
LiquidCrystal lcd(pinLCD_RS, pinLCD_Enable, pinLCD_D0, pinLCD_D1, pinLCD_D2, pinLCD_D3, pinLCD_D4, pinLCD_D5, pinLCD_D6, pinLCD_D7);

// Initialise certaine variable
//int Cooling_last = -128;		// Place le dernier enregistrement à -128 car il n'y en a jamais eu. (valeur arbitraire)
//int Humidity_last = -128;
//int temperature_dht_last = -128;
int Cooling_now;
int humidity_now;
int temperature_dht_now;

//volatile float rpm_time = 0;				//  TODO + FIXME : capteur pour RPM (effet Hall bougie ?)
//volatile float rpm_time_last = 0;	
//int rpm;						
//volatile int rpm_array[5] = {0,0,0,0,0};
//
//void rpm_calc() {
//	rpm_time = (micros() - rpm_time_last); 
//	rpm_time_last = micros();
//}

int affcount1 = 0;

// Les LED d'état
int WifiLED = 40;

void setup()
{
	Serial.begin(9600);	// Serial pour le debogage, c'est pratique
//	Mode des PIN des capteurs
	pinMode(CoolingTemp, INPUT);
	pinMode(WifiLED, OUTPUT);
//	Wifi.status();
//	if (WiFi.status() == WL_NO_SHIELD) {
//		Serial.println("WiFi shield not present");
//		while(true);
//	}
//	WiFi.config(ip, ns, gw);
	if ( status != WL_CONNECTED) { // Essaie de se connecter une première fois au démarrage
		status = WiFi.begin(ssid, pass);
		delay(5000);
	}
	if ( status == WL_CONNECTED) {
		Udp.begin(portudp);
		xPL_hbeat(); // Envoie le premier heartbeat xPL
	}
	
	lcd.begin(20, 4); // Initialise un écran LCD 16 lignes / 2 colonnes
	
//	attachInterrupt(5, rpm_calc, RISING);	// Interrupt pour calcul des RPM
	
//	ctrlralenti.attach(2); // TODO : Servomoteur de contrôle du ralenti (variable selon la temperature, fait office de "starter")
}

void loop() {
	if (count % 10 == 0) { // Mesure toutes les secondes la temperature
		Cooling_now = TMP36(CoolingTemp);
	}
	if (count % 200 == 0) { // Mesure l'humidité toutes les 30 secondes 
		humidity_now = dht.readHumidity();
		temperature_dht_now = dht.readTemperature();
	}
//	int packetSize = Udp.parsePacket();  // TODO : action xPL
//	if(packetSize) {
//		PacketAction(); // Exécute l'action si demandée
//	}
	if ( status == WL_CONNECTED) {		// Uniquement si connecté
		if (count % 100 == 0) {			// Envoie le HBEAT toutes les 10 secondes
			xPL_hbeat();
//		}
//		if (Cooling_last != Cooling_now) {		// Envoie le TRIG uniquement si modification
			xPL_Cooling(Cooling_now);			// comme le prévoie le protocole xPL
//			Cooling_last = Cooling_now;			// FIXME : Le test ne fonctionne pas ?
//		}										// WORKAROUND : envoie en même temps que le heartbeat
//		if (Humidity_last != humidity_now) {
			xPL_Humidity(humidity_now);
//			Humidity_last = humidity_now;
//		}
//		if (temperature_dht_now != temperature_dht_last) {
			xPL_dhtTemp(temperature_dht_now);
//			temperature_dht_last = temperature_dht_now;
		}
	}
	if (count % 10 == 0) {
		lcd.clear();
		if (affcount1 == 0) {		// Alterne l'affichage toutes les 2 secondes
			lcd.setCursor(0, 0);
			lcd.print("Moteur : ");
			lcd.print(Cooling_now);
			lcd.print((char)223);
			lcd.print("C");;
			affcount1 = 1;
		} else {
			lcd.setCursor(0, 0);
			lcd.print("Ext. : ");
			lcd.print(temperature_dht_now);
			lcd.print((char)223);
			lcd.print("C");
			lcd.setCursor(0, 1);
			lcd.print("Hum. : ");
			lcd.print(humidity_now);
			lcd.print("%");
			affcount1 = 0;
		}
//		lcd.print("RPM : ");
//		if (rpm_time > 0) {
//			rpm_array[0] = rpm_array[1];
//			rpm_array[1] = rpm_array[2];
//			rpm_array[2] = rpm_array[3];
//			rpm_array[3] = rpm_array[4];
//			rpm_array[4] = 60*(1000000/rpm_time);
//			rpm = (rpm_array[0] + rpm_array[1] + rpm_array[2] + rpm_array[3] + rpm_array[4]) / 5;
//		}
//		rpm = 60*(1000000/rpm_time);
//		lcd.print(rpm_time);
		timer = millis();
	}
	if (count % 50 == 0) {				// retente la connexion toutes les 5 secondes
		if ( status != WL_CONNECTED) {	// si non connecté
			digitalWrite(WifiLED, LOW);
			status = WiFi.begin(ssid, pass);
		}
	}
	if (status == WL_CONNECTED) { digitalWrite(WifiLED, HIGH); }	// LED d'état WiFi
	if (count == 240) {	// replace le compteur "count" à 1 minute.
		count = 0;
	} else {
		count = count + 1;	// et continue le compteur par unité de 250 ms
	}
}
