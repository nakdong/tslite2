const char* VERSION = "1.13b";   // 코드 버전

#include <avr/wdt.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

SoftwareSerial bluetoothSerial(A2, A3); // RX, TX pins

#define LED_PIN    4         // 네오픽셀 데이터 핀
#define LED_COUNT  34        // 네오픽셀의 개수
#define YELLOW     0xFF5500  // 노란색 (RGB 값)
#define LYELLOW    0xFF3300  // 밝은 노란색 (RGB 값)
#define WHITE      0xFFBFAF  // 흰색 (RGB 값)

#define MIN_HANDLE 132       // 조향 각도 최소 값
#define MAX_HANDLE 585       // 조향 각도 최대 값

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
const int batChange = A4;          // 배터리 전환 핀

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

// 딜레이 관련 변수
unsigned long previousMillis = 0;
const long neopixelInterval = 500; // 네오픽셀 딜레이 시간 (500ms 예제)
const long ledInterval = 1000;     // LED 딜레이 시간 (1000ms 예제)

void softwareReset() {
  wdt_enable(WDTO_15MS); // Enable the watchdog timer with a timeout of 15ms
  while (1) {} // Wait for the watchdog timer to reset the microcontroller
}

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

  // 초기화
  strip.clear();
  strip.show();
  controlMotor(motor1DirectionPin, motor1SpeedPin, 0);
  controlMotor(motor2DirectionPin, motor2SpeedPin, 0);
}

void loop() {
  unsigned long currentMillis = millis();

  // 네오픽셀 딜레이 처리
  if (currentMillis - previousMillis >= neopixelInterval) {
    previousMillis = currentMillis;

    // 네오픽셀 딜레이 시간이 지났으므로 원하는 작업 실행
    neoledsignal(Neopixel.c_str());
  }

  // LED 딜레이 처리
  if (currentMillis - previousMillis >= ledInterval) {
    previousMillis = currentMillis;

    // LED 딜레이 시간이 지났으므로 원하는 작업 실행
    fledsignal(Frontled.c_str());
    bledsignal(Backled.c_str());
  }

  // 나머지 루프 코드를 계속 실행
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
    } else if (inputString == "$reset\n") {
      // softwareReset();
    } else if (inputString == "$stop\n") {
      // "start" 입력을 받을 때까지 대기합니다.
      controlMotor(motor1DirectionPin, motor1SpeedPin, 0);
      controlMotor(motor2DirectionPin, motor2SpeedPin, 0);
      controlMotor(motor3DirectionPin, motor3SpeedPin, 0);
      while (true) {
        Serial.println("아두이노를 시작하려면 '$start'를 입력하세요:");
        while (Serial.available() == 0) {
          // 입력이 없으면 대기합니다.
        }

        // 입력을 읽고 비교합니다.
        String input = Serial.readStringUntil('\n');
        if (input == "$start") {
          Serial.println("시작합니다.");
          break; // "start"를 입력받으면 루프를 종료합니다.
        } else {
          Serial.println("올바른 명령이 아닙니다.");
        }
      }
    } else {
      // parseCommand 함수 호출
      parseCommand(inputString);
    }

    // inputString 초기화
    inputString = "";
    stringComplete = false;
  }

  // 핸들 값 매핑
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
      int speed = Setpoint - Input;
      int realspeed = map(speed, -10, 0, 255, 0);
      controlMotor(motor3DirectionPin, motor3SpeedPin, -100);
    } else if (Input > Setpoint + 3) {
      int speed = Setpoint - Input;
      int realspeed = map(speed, 10, 0, 255, 0);
      controlMotor(motor3DirectionPin, motor3SpeedPin, 100);
    } else {
      controlMotor(motor3DirectionPin, motor3SpeedPin, 0);
    }
  }

  digitalWrite(handleRelay, Handlerelay);
  digitalWrite(batRelay, Batrelay);
}

bool stringToBool(const String& str) {
    return (str == "true");
}

// 시리얼로부터 받은 명령을 처리하는 함수
void parseCommand(String command) {
  int leftWheelSpeed, rightWheelSpeed;
  int setpoint;
  char frontled[10];
  char backled[10];
  char neopixel[10];
  char handlerelay[10];
  char batrelay[10];

  // 시리얼로 받은 명령을 분석
  int parsed = sscanf(command.c_str(), "#%d %d %i %s %s %s %s %s %s", &leftWheelSpeed, &rightWheelSpeed, &setpoint, frontled, backled, neopixel, handlerelay, batrelay);

  if (parsed == 8) {
    Serial.print("Received Command: ");
    Serial.println(command.c_str());

    // 모터 1과 모터 2 제어
    controlMotor(motor1DirectionPin, motor1SpeedPin, leftWheelSpeed);
    controlMotor(motor2DirectionPin, motor2SpeedPin, rightWheelSpeed);

    // 변수 업데이트
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

// 모터 제어 함수
void controlMotor(int directionPin, int speedPin, int speed) {
  if (speed > 0) {
    // 이론상 전진 코드
    digitalWrite(directionPin, HIGH);
    analogWrite(speedPin, speed);
  } else {
    // 이론상 후진 코드
    digitalWrite(directionPin, LOW);
    analogWrite(speedPin, abs(speed));
  }
}

// 전조등 제어 함수
void fledsignal(const char* direction) {
  if (strcmp(direction, "true") == 0) {
    digitalWrite(frontLedPin, HIGH);
  } else if (strcmp(direction, "false") == 0) {
    // 전조등 끄기
    digitalWrite(frontLedPin, LOW);
  }
}

// 후진등 제어 함수
void bledsignal(const char* direction) {
  if (strcmp(direction, "true") == 0) {
    // 후진등 켜기
    digitalWrite(backLedPin, HIGH);
  } else if (strcmp(direction, "false") == 0) {
    // 후진등 끄기
    digitalWrite(backLedPin, LOW);
  }
}

// 네오픽셀 제어 함수
void neoledsignal(const char* direction) {
  if (strcmp(direction, "left") == 0) {
    // 왼쪽
    for (int i = 16; i > 0; i--) {
      strip.setPixelColor(i, YELLOW);
    }
    strip.show();
  } else if (strcmp(direction, "right") == 0) {
    // 오른쪽
    for (int i = 16; i >= 0; i--) {
      strip.setPixelColor(i + 17, YELLOW);
    }
    strip.show();
  } else if (strcmp(direction, "all") == 0) {
    // 왼쪽 15줄과 오른쪽 15줄을 동시에 노란색으로 켜기
    for (int i = 0; i < 17; i++) {
      strip.setPixelColor(i, YELLOW);
      strip.setPixelColor(i + 17, YELLOW);
    }
    strip.show();
  } else if (strcmp(direction, "off") == 0) {
    // 모든 네오픽셀 끄기
    strip.clear();
    strip.show();
  }
}
