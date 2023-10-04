const char* VERSION = "1.11";   // 코드 버전


#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

SoftwareSerial bluetoothSerial(A2, A3); // RX, TX pins
#define LED_PIN    4         // 네오픽셀 데이터 핀
#define LED_COUNT  34        // 네오픽셀의 개수
#define YELLOW     0xFF5500  // 노란색 (RGB 값)
#define LYELLOW    0xFF3300  // 밝은 노란색 (RGB 값)
#define WHITE      0xFFBFAF  // 흰색 (RGB 값)

#define MIN_HANDLE 132       // 조향 각도 최소 값
#define MAX_HANDLE 585        // 조향 각도 최대 값

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// 모터 및 핀 관련 상수 정의
const int motor1DirectionPin = 7;  // 모터 1 방향 핀
const int motor1SpeedPin = 9;      // 모터 1 속도 제어 핀
const int motor2DirectionPin = 8;  // 모터 2 방향 핀
const int motor2SpeedPin = 10;     // 모터 2 속도 제어 핀
const int motor3DirectionPin = 12; // 모터 3 방향 핀
const int motor3SpeedPin = 11;     // 모터 3 속도 제어 핀
const int handlePin = A0;          // 가변저항 핀
const int frontLedPin = 13;        // 전조등 핀
const int backLedPin = 5;          // 후진등 핀
const int handleRelay = 3;         // 헨들 전원 차단 핀
const int batRelay = 2;            // 배터리 전원 차단 핀
const int batVoltage = A1;         // 배터리 전압 측정 핀
const int batChange = A4;         // 배터리 전환 핀

// 변수 선언
String inputString = "";
boolean stringComplete = false;

// 조향 관련 변수
int Setpoint = 0; // 목표 핸들 값
int Input = 0;    // 현재 핸들 값
int Output = 0;   // 모터 3 출력
int erobe = 3;

String Frontled;
String Backled;
String Neopixel;
bool Handlerelay;
bool Batrelay;

const unsigned long PERIOD1 = 1000;

Timer t;

void setup() {
  Serial.begin(9600);
  bluetoothSerial.begin(9600);
  // 핀 모드 설정
  pinMode(motor1DirectionPin, OUTPUT);
  pinMode(motor1SpeedPin, OUTPUT);
  pinMode(motor2DirectionPin, OUTPUT);
  pinMode(motor2SpeedPin, OUTPUT);
  pinMode(motor3DirectionPin, OUTPUT);
  pinMode(motor3SpeedPin, OUTPUT);
  pinMode(handlePin, INPUT);
  pinMode(frontLedPin, OUTPUT);
  pinMode(backLedPin, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(handleRelay, OUTPUT);
  pinMode(batRelay, OUTPUT);
  pinMode(batVoltage, INPUT);
  pinMode(batChange, OUTPUT);

  strip.clear();
  strip.show();
  controlMotor(motor1DirectionPin, motor1SpeedPin, 0);
  controlMotor(motor2DirectionPin, motor2SpeedPin, 0);
  digitalWrite(backLedPin, HIGH);
  digitalWrite(batChange, HIGH);
  t.every(1,neoledsignal);
}

void loop() {
  int potValue = analogRead(handlePin);
  int batValue = analogRead(batVoltage);

  while (Serial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
  while (bluetoothSerial.available()) {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
  if (stringComplete) {
    if (inputString == "$info\n") {
      Serial.println("#tslite " + String(VERSION) + " " + String(potValue) + " " + String(batValue));
    } else if (inputString == "$stop\n") {
      while (true) {
        Serial.println("아두이노를 시작하려면 '$start'를 입력하세요:");
        while (Serial.available() == 0) {}

        String input = Serial.readStringUntil('\n');
        if (input == "$start") {
          Serial.println("시작합니다.");
          break;
        } else {
          Serial.println("올바른 명령이 아닙니다.");
        }
      }
    } else {
      parseCommand(inputString);
    }

    inputString = "";
    stringComplete = false;
  }
  
  Input = map(potValue, 120, 579, -45, 45);
  Serial.print(potValue);
  Serial.print(" ");
  Serial.print(Input);
  Serial.print(" ");
  Serial.println(Setpoint);

  if (Input < Setpoint - 5) {
    controlMotor(motor3DirectionPin, motor3SpeedPin, -255);
  } else if (Input > Setpoint + 5) {
    controlMotor(motor3DirectionPin, motor3SpeedPin, 255);
  } else {
    if (Input < Setpoint - 3) {
      int speed = Setpoint-Input;
      int realspeed = map(speed, -10, 0, 255, 0);
      controlMotor(motor3DirectionPin, motor3SpeedPin, -100);
    } else if (Input > Setpoint + 3) {
      int speed = Setpoint-Input;
      int realspeed = map(speed, 10, 0, 255, 0);
      controlMotor(motor3DirectionPin, motor3SpeedPin, 100);
    } else {
      controlMotor(motor3DirectionPin, motor3SpeedPin, 0);
    }
  }

  neoledsignal(Neopixel.c_str());
  fledsignal(Frontled.c_str());
  bledsignal(Backled.c_str());
  digitalWrite(handleRelay, Handlerelay);
  digitalWrite(batRelay, Batrelay);
}

bool stringToBool(const String& str) {
    return (str == "true");
}

void parseCommand(String command) {
  int leftWheelSpeed, rightWheelSpeed;
  int setpoint;
  char frontled[10];
  char backled[10];
  char neopixel[10];
  char handlerelay[10];
  char batrelay[10];

  int parsed = sscanf(command.c_str(), "#%d %d %i %s %s %s %s %s %s", &leftWheelSpeed, &rightWheelSpeed, &setpoint, frontled, backled, neopixel, handlerelay, batrelay);

  if (parsed == 8) {
    Serial.print("OK: ");
    Serial.println(command.c_str());

    controlMotor(motor1DirectionPin, motor1SpeedPin, leftWheelSpeed);
    controlMotor(motor2DirectionPin, motor2SpeedPin, rightWheelSpeed);

    Setpoint = setpoint;
    Frontled = String(frontled);
    Backled = String(backled);
    Neopixel = String(neopixel);
    Handlerelay = stringToBool(String(handlerelay));
    Batrelay = stringToBool(String(batrelay));
  } else {
    Serial.println("Invalid command format");
  }
}

void controlMotor(int directionPin, int speedPin, int speed) {
  if (speed > 0) {
    digitalWrite(directionPin, HIGH);
    analogWrite(speedPin, speed);
  } else {
    digitalWrite(directionPin, LOW);
    analogWrite(speedPin, abs(speed));
  }
}

void fledsignal(const char* direction) {
  if (strcmp(direction, "true") == 0) {
    digitalWrite(frontLedPin, HIGH);
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, WHITE);
    }
    strip.show();
  } else if (strcmp(direction, "false") == 0) {
    digitalWrite(frontLedPin, LOW);
    strip.clear();
    strip.show();
  }
}

void bledsignal(const char* direction) {
  if (strcmp(direction, "true") == 0) {
    digitalWrite(backLedPin, LOW);
  } else if (strcmp(direction, "false") == 0) {
    digitalWrite(backLedPin, HIGH);
  }
}

void neoledsignal(const char* direction) {
  if (strcmp(direction, "left") == 0) {
    for (int i = 16; i > 0; i--) {
      strip.setPixelColor(i, YELLOW);
      strip.show();
      delay(6);
    }
    delay(1000);

  } else if (strcmp(direction, "right") == 0) {
    for (int i = 16; i >= 0; i--) {
      strip.setPixelColor(i + 17, YELLOW);
      strip.show();
      delay(6);
    }
    delay(1000);

  } else if (strcmp(direction, "all") == 0) {
    for (int i = 0; i < 17; i++) {
      strip.setPixelColor(i, YELLOW);
      strip.setPixelColor(i + 17, YELLOW);
    }
    strip.show();
  } else if (strcmp(direction, "off") == 0) {
    strip.clear();
    strip.show();
  }
}
