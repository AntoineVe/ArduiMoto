//#include <Servo.h>
#include <DHT.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <stdlib.h>
#include <max6675.h>

// Définie le capteur DHT
		// Attention, pin < 32 !
#define DHTTYPE DHT11
#define DHTPIN 2
DHT dht(DHTPIN, DHTTYPE); // DHT11 (Temperature et humidité)

//Servo ctrlralenti;

// Définie les pins
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
const int CoolingTemp = A0;	// Capteur TMP36
const int pinVbat = A1;		// Pont diviseur de tension 15V -> 5V
const int pinVclign = A2;	// Pont diviseur de tension 15V -> 5V aux bornes des clignotants
const int ClignoRelais = 23;// (Le relais d'origine à cramé, valeur = 42 eur. utilisition d'un relais standars G5V2 et un pont diviseur de tension)
const int pinTC_D0 = 42;
const int pinTC_CLK = 44;	// Amplificateur pour thermocouple
const int pinTC_CS = 46;
const int TMP36_Arduino_vcc = A13;
const int TMP36_Arduino_data = A14;
const int TMP36_Arduino_gnd = A15;

MAX6675 thermocouple(pinTC_CLK, pinTC_CS, pinTC_D0);	// Déclaration du thermocouple


// Les LED d'état
const int WifiLED = 3;


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
unsigned long timerHBEAT = 0;			// Les différents timer de fonctions
unsigned long timerTMP36 = 0;
unsigned long timerDHT11 = 0;
unsigned long timerLCD = 0;
unsigned long timerWIFI = 0;
unsigned long timerCligno = 0;
unsigned long timerThermoC = 0;

int TMP36(int capteur) {					// Cette fonction permet le calcul de la
	int mesure = analogRead(capteur);		// température depuis un capteur TMP36,
	float tension = ((mesure * 5.0) / 1024);	// prenant comme argument le pin du capteur
	int temperature = round((tension - 0.5) * 100);
	return temperature;
}

int Vbat(int pin) {
	int tension = ((analogRead(pin)/1024.0)*15.0)*10;
	return tension;
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
int temperature_tc_now;
int temperature_tc_last;
int Arduino_tmp_now;
float Vbatterie_now;

//volatile float rpm_time = 0;				//  TODO + FIXME : capteur pour RPM (effet Hall bougie ?)
//volatile float rpm_time_last = 0;	
//int rpm;						
//volatile int rpm_array[5] = {0,0,0,0,0};
//
//void rpm_calc() {
//	rpm_time = (micros() - rpm_time_last); 
//	rpm_time_last = micros();
//}

int affcount1 = 0; // Compteur pour l'alternance de l'affichage
int ClignoState = 0; // Etat du clignotant
int Vclign = 0; // Tension aux diviseur de tension pour le relais du clignotant


void setup()
{
	Serial.begin(9600);	// Serial pour le debogage, c'est pratique
//	Mode des PIN des capteurs
	pinMode(CoolingTemp, INPUT);
	pinMode(pinVbat, INPUT);
	pinMode(WifiLED, OUTPUT);
	pinMode(pinVclign, INPUT);
	pinMode(ClignoRelais, OUTPUT);
	pinMode(TMP36_Arduino_vcc, OUTPUT);
	pinMode(TMP36_Arduino_gnd, OUTPUT);
	pinMode(TMP36_Arduino_vcc, INPUT);
	digitalWrite(ClignoRelais, LOW);
	digitalWrite(TMP36_Arduino_vcc, HIGH);
	digitalWrite(TMP36_Arduino_gnd, LOW);
//	Wifi.status();
//	if (WiFi.status() == WL_NO_SHIELD) {
//		Serial.println("WiFi shield not present");
//		while(true);
//	}
//	WiFi.config(ip, ns, gw);
//	if ( status != WL_CONNECTED) { // Essaie de se connecter une première fois au démarrage
//		status = WiFi.begin(ssid, pass);
//		delay(5000);
//	}
//	if ( status == WL_CONNECTED) {
//		Udp.begin(portudp);
//		xPL_hbeat(); // Envoie le premier heartbeat xPL
//	}
	
	lcd.begin(20, 4); // Initialise un écran LCD 16 lignes / 2 colonnes
	
//	attachInterrupt(5, rpm_calc, RISING);	// Interrupt pour calcul des RPM
	
//	ctrlralenti.attach(2); // TODO : Servomoteur de contrôle du ralenti (variable selon la temperature, fait office de "starter")
}

void loop() {
	if (millis() - timerCligno > 500 ) { // D'origine, le clignotant est donné pour 85 cycles par minute (60/85 = 0.706), mais je prefere 1/2 seconde
		timerCligno = millis();
		if (ClignoState == 0) {
			Vclign = analogRead(pinVclign);
		}
		if (Vclign > 300 ) {
			if (ClignoState == 0) {
				digitalWrite(ClignoRelais, HIGH);
				ClignoState = 1;
			}
			else {
				digitalWrite(ClignoRelais, LOW);
				ClignoState = 0;
			}
		}
	}
	if (millis() - timerTMP36 > 2000) { // Mesure toutes les 2 secondes la temperature et la tension de la batterie
		timerTMP36 = millis();
		Cooling_now = TMP36(CoolingTemp);
		Arduino_tmp_now = TMP36(TMP36_Arduino_data);
		Vbatterie_now = ((Vbat(pinVbat)/10.0) + 0.1);	// +0.1 car offset de mesure constaté
	}
	if (millis() - timerDHT11 > 10000) { // Interroge le DHT11 toutes les 10 secondes
		timerDHT11 = millis();
		humidity_now = dht.readHumidity();
		temperature_dht_now = dht.readTemperature();
		
	}
	if (millis() - timerThermoC > 2000) {
		temperature_tc_now = round(thermocouple.readCelsius());
		if (temperature_tc_now == 0) {
			temperature_tc_now = temperature_tc_last;
		}
		temperature_tc_last = temperature_tc_now;
	}
//	int packetSize = Udp.parsePacket();  // TODO : action xPL
//	if(packetSize) {
//		PacketAction(); // Exécute l'action si demandée
//	}
	if ( status == WL_CONNECTED) {		// Uniquement si connecté
		if (millis() - timerHBEAT > 10000) {			// Envoie le HBEAT toutes les 10 secondes
			timerHBEAT = millis();
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
	if (millis() - timerLCD > 2000) {	// Alterne l'affichage toutes les 2 secondes
		timerLCD = millis();
		lcd.clear();
		if (Vbatterie_now < 12.0) {			// Les valeurs de 12V, 11.5V et 15V sont dans la revue technique de la moto
			lcd.setCursor(0, 3);
			lcd.print("Batterie faible");
		} else if (Vbatterie_now < 11.5) {
			lcd.setCursor(0, 3);
			lcd.print("Batterie critique !");
		} else if (Vbatterie_now > 15) {
			lcd.setCursor(0, 3);
			lcd.print("Surcharge batterie !");
		}
		if (affcount1 == 0) {		
			lcd.setCursor(0, 0);
			lcd.print("Cooling : ");
			lcd.print(Cooling_now);
			lcd.print((char)223);
			lcd.print("C");
			lcd.setCursor(0, 1);
			lcd.print("Echapp. : ");
			lcd.print(temperature_tc_now + 12); // Offset thermocouple
			lcd.print((char)223);
			lcd.print("C");
			lcd.setCursor(0, 2);
			lcd.print("Batt. : ");
			lcd.print(Vbatterie_now);
			lcd.print(" V");
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
			lcd.setCursor(0, 2);
			lcd.print("Arduino : ");
			lcd.print(Arduino_tmp_now);
			lcd.print((char)223);
			lcd.print("C");
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
	}
//	if (millis() - timerWIFI > 5000) {				// retente la connexion toutes les 5 secondes
//		timerWIFI = millis();
//		if ( status != WL_CONNECTED) {	// si non connecté
//			digitalWrite(WifiLED, LOW);
//			status = WiFi.begin(ssid, pass);
//		}
//	}
//	if (status == WL_CONNECTED) { 
//		digitalWrite(WifiLED, HIGH);
//		Udp.begin(portudp);
//	}	// LED d'état WiFi	
}
