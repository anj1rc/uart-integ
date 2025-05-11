#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define DHTPIN 6
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);


void setup() {
  Serial.begin(9600);
  dht.begin();
  lcd.init();
  lcd.backlight();
  
  pinMode(5, OUTPUT); 
  pinMode(7, OUTPUT); 
  digitalWrite(7, LOW);
}

void loop() {
  int tempValue = analogRead(A0);
  float tempC = (tempValue * 500.0) / 1023.0;
  
  float humidity = dht.readHumidity();
  
  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.print(tempC);
  lcd.write(0xDF);
  lcd.print("C");
  
  lcd.setCursor(0, 1);
  lcd.print("Hum:");
  lcd.print(humidity);
  lcd.print("%");
  
  int ldrValue = analogRead(A1);
  String ledStatus = (ldrValue < 5) ? "ON" : "OFF";
  digitalWrite(5, (ldrValue < 5) ? HIGH : LOW);
  
  int waterLevel = analogRead(A2);
  String relayStatus = (waterLevel > 300) ? "ON" : "OFF";
  digitalWrite(7, (waterLevel > 300) ? HIGH : LOW);
  
  String data = "Temp:" + String(tempC) + ",Hum:" + String(humidity) + 
                ",LDR:" + String(ldrValue) + ",Water:" + String(waterLevel) +
                ",LED:" + ledStatus + ",Relay:" + relayStatus;
  Serial.println(data);
  
  delay(2000); 
}
