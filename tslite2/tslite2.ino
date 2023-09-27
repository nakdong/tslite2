const char* VERSION = "1.5";   // 코드 버전

#include <avr/wdt.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN    4         // 네오픽셀 데이터 핀
#define LED_COUNT  30        // 네오픽셀의 개수
#define YELLOW     0xFFAA00  // 노란색 (RGB 값)
#define WHITE      0xFFFFFF  // 흰색 (RGB 값)

#define MIN_HANDLE 315       // 조향 각도 최소 값
#define MAX_HANDLE 95        // 조향 각도 최대 값

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// 모터 및 핀 관련 상수 정의
const int motor1DirectionPin = 7;  // 모터 1 방향 핀
const int motor1SpeedPin = 9;      // 모터 1 속도 제어 핀
const int motor2DirectionPin = 8;  // 모터 2 방향 핀
const int motor2SpeedPin = 10;     // 모터 2 속도 제어 핀
const int motor3DirectionPin = 12; // 모터 3 방향 핀
const int motor3SpeedPin = 11;     // 모터 3 속도 제어 핀
const int handlePin = A0;          // 가변저항 핀
const int frontLedPin = 13;        // 전조등 핀, 13번 핀이라 작동될 때 ㅁ
const int backLedPin = 5;          // 후진등 핀
const int handleRelay = 3;         // 헨들 전원 차단 핀
const int batRelay = 2;            // 배터리 전원 차단 핀
const int batVoltage = A1;         // 배터리 전압 측정 핀

// 변수 선언
String inputString = "";
boolean stringComplete = false;

// 조향 관련 변수
double Setpoint = 0; // 목표 핸들 값
double Input = 0;    // 현재 핸들 값
double Output = 0;   // 모터 3 출력
double erobe = 0;

String Frontled;
String Backled;
String Neopixel;
bool Handlerelay;
bool Batrelay;

void softwareReset() {
  wdt_enable(WDTO_15MS); // Enable the watchdog timer with a timeout of 15ms
  while (1) {} // Wait for the watchdog timer to reset the microcontroller
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // 시리얼 포트가 준비될 때까지 대기합니다.
  }
  Serial.println("OK: booting");
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
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, 0); // 모든 픽셀 끄기
  }
  strip.show();
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
  if (stringComplete) {
    if (inputString == "$info\n") {
      Serial.println("#tslite " + String(VERSION) + " " + String(potValue) + " " + String(batValue));
    } else if (inputString == "$reset\n") {
      softwareReset();
    } else if (inputString == "$stop\n") {
      // "$start" 입력을 받을 때까지 대기합니다.
      while (true) {
        Serial.println("아두이노를 시작하려면 '$start'혹은 '$reset'을 입력하세요:");
        while (Serial.available() == 0) {
          // 입력이 없으면 대기합니다.
        }

        // 입력을 읽고 비교합니다.
        String input = Serial.readStringUntil('\n');
        if (input == "$start") {
          Serial.println("시작합니다.");
          break; // "start"를 입력받으면 루프를 종료합니다.
        } else if (input == "$reset") {
          softwareReset();
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
  Input = map(potValue, MIN_HANDLE, MAX_HANDLE, -45, 45);

  // 핸들 제어 로직
  if (Input < Setpoint - erobe) {
    controlMotor(motor3DirectionPin, motor3SpeedPin, -255);
  } else if (Input > Setpoint + erobe) {
    controlMotor(motor3DirectionPin, motor3SpeedPin, 255);
  } else {
    if (Input < Setpoint) {
      int speed = Setpoint - Input;
      int realspeed = map(speed, -10, 0, 255, 0);
      controlMotor(motor3DirectionPin, motor3SpeedPin, realspeed);
    } else if (Input > Setpoint) {
      int speed = Setpoint - Input;
      int realspeed = map(speed, 10, 0, 255, 0);
      controlMotor(motor3DirectionPin, motor3SpeedPin, realspeed);
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
  // #(왼쪽 바퀴 속도,double) (오른쪽 바퀴 속도,double) (조향 각도,double) (앞쪽 전등,bool) (뒤쪽 전등,bool) (깜빡이,left|right|all|off) (핸들 릴레이,bool) (배터리 릴레이,bool)
  int parsed = sscanf(command.c_str(), "#%d %d %d %s %s %s %s %s %s", &leftWheelSpeed, &rightWheelSpeed, &setpoint, frontled, backled, neopixel, handlerelay, batrelay);

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
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, WHITE);
    }
    strip.show();  // 변경된 색상 표시
    delay(10);    // 대기
  } else if (strcmp(direction, "false") == 0) {
    // 전조등 끄기
    digitalWrite(frontLedPin, LOW);
    // 모든 네오픽셀 끄기
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, 0); // 모든 픽셀 끄기
    }
    strip.show();
    delay(500);      // 모두 끈 후 대기
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
    // 왼쪽 15줄을 노란색으로 켜기
    for (int i = 0; i < 15; i++) {
      strip.setPixelColor(i, YELLOW);
      strip.show();  // 한 줄마다 변경된 색상 표시
      delay(6);    // 대기
    }
    delay(400);      // 왼쪽 15줄 모두 켜진 후 0.5초 대기
    // 모든 네오픽셀 끄기
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, 0); // 모든 픽셀 끄기
    }
    strip.show();
    delay(100);      // 왼쪽 15줄 모두 켜진 후 0.5초 대기
  } else if (strcmp(direction, "right") == 0) {
    // 오른쪽 15줄을 노란색으로 켜기
    for (int i = 14; i >= 0; i--) {
      strip.setPixelColor(i + 15, YELLOW);
      strip.show();  // 한 줄마다 변경된 색상 표시
      delay(6);    // 대기
    }
    delay(540);      // 오른쪽 15줄 모두 켜진 후 0.5초 대기
    // 모든 네오픽셀 끄기
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, 0); // 모든 픽셀 끄기
    }
    strip.show();
    delay(100);      // 모두 끈 후 대기
  } else if (strcmp(direction, "all") == 0) {
    // 왼쪽 15줄과 오른쪽 15줄을 동시에 노란색으로 켜기
    for (int i = 0; i < 15; i++) {
      strip.setPixelColor(i, YELLOW);
      strip.setPixelColor(i + 15, YELLOW);
    }
    strip.show();  // 변경된 색상 표시
    delay(500);    // 0.5초 대기
  } else if (strcmp(direction, "off") == 0) {
    // 모든 네오픽셀 끄기
    strip.clear();
    strip.show();
  }
}
