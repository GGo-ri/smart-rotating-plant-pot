#include <DHT.h>
#include <SoftwareSerial.h>

// --- 핀 설정 ---
#define DHTPIN 10
#define DHTTYPE DHT11
#define SOILPIN A5

DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial BTSerial(6, 5); // HM-10 TX -> 6, RX <- 5

// --- STATE 핀 ---
const int statePin = 11; // HM-10의 STATE 핀

// --- 조도센서 핀 ---
const int eastPin = A0;
const int northPin  = A1;
const int southPin = A2;
const int westPin  = A3;

// --- 모터 드라이버 핀 ---
const int ENA = 9;
const int IN1 = 8;
const int IN2 = 7;

// --- 토양 습도 기준값 ---
const int dryValue = 1023;
const int wetValue = 300;

// --- 속도 및 상태 변수 ---
int currentSpeed = 100;
String btBuffer = "";
bool isRotating = false;

// --- 회전 모드 및 방향 ---
enum RotationMode { AUTO, USER };
RotationMode rotationMode = AUTO;

enum Direction { NORTH, EAST, SOUTH, WEST };
Direction currentDirection;
Direction brightestDirection;

// --- 타이머 ---
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000;

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 13000;

int sendCount = 0;

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  dht.begin();

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  pinMode(statePin, INPUT);

  brightestDirection = getBrightestDirection();
  currentDirection = brightestDirection;
  Serial.print("🧭 초기 방향: ");
  printDirection(currentDirection);
}

void loop() {
  if (isRotating) return;

  handleBluetooth();
  updateModeByBluetoothState();
  autoRotateIfNeeded();
  //runUserModeIfNeeded();
  sendSensorDataIfNeeded();
}

void handleBluetooth() {
  while (BTSerial.available()) {
    char c = BTSerial.read();
    if (c == '\n' || c == '\r') {
      if (btBuffer.length() > 0) {
        handleReceivedLine(btBuffer);
        btBuffer = "";
      }
    } else {
      btBuffer += c;
    }
  }
}

void handleReceivedLine(String line) {
  Serial.println();

  if (line.startsWith("SPD:")) {
    int speed = line.substring(4).toInt();
    if (speed >= 0 && speed <= 100) {
      currentSpeed = speed;
      Serial.println("✅ 속도 설정됨: " + String(currentSpeed));
    } else {
      Serial.println("⚠️ 잘못된 속도 값");
    }
  } else if (line == "MODE:MANUAL") {
    rotationMode = USER;
    Serial.println("🔧 사용자 모드 진입");
  } else if (line == "MODE:AUTO") {
    rotationMode = AUTO;
    stopMotor();
    Serial.println("🔁 자동 모드 복귀");
    rotateToDirection(brightestDirection);
    currentDirection = brightestDirection;
  } else if (line.startsWith("DIR:")) {
    char dirChar = line.charAt(4);
    Direction target;
    String dirName;

    switch (dirChar) {
      case 'N': target = NORTH; dirName = "북"; break;
      case 'E': target = EAST;  dirName = "동"; break;
      case 'S': target = SOUTH; dirName = "남"; break;
      case 'W': target = WEST;  dirName = "서"; break;
      default:
        Serial.println("⚠️ 잘못된 방향 코드");
        return;
    }

    Serial.println("블루투스 수신 : " + dirName);
    rotateToDirection(target);
    currentDirection = target;
  } else {
    Serial.println("⚠️ 알 수 없는 명령");
  }
}

void updateModeByBluetoothState() {
  bool isConnected = digitalRead(statePin);

  if (!isConnected && rotationMode != AUTO) {
    rotationMode = AUTO;
    stopMotor();
    Serial.println("📴 블루투스 연결 끊김 → 자동모드 전환");
    rotateToDirection(brightestDirection);
    currentDirection = brightestDirection;  // ✅ 회전 직후 현재 방향 업데이트
  }
}

void autoRotateIfNeeded() {
  if (rotationMode != AUTO) return;

  if (millis() - lastCheckTime >= checkInterval) {
    lastCheckTime = millis();
    Direction target = getBrightestDirection();
    brightestDirection = target;
    Serial.println("--------------------");
    Serial.print("💡 가장 밝은 방향: ");
    printDirection(target);

    if (target != currentDirection) {
      rotateToDirection(target);
      currentDirection = target;
    } else {
      Serial.println("✅ 현재 방향 유지");
    }
  }
}

// void runUserModeIfNeeded() {
//   if (rotationMode != USER) return;
//   int pwmVal = map(currentSpeed, 0, 100, 0, 255);
//   analogWrite(ENA, pwmVal);
//   digitalWrite(IN1, HIGH);
//   digitalWrite(IN2, LOW);
// }

void stopMotor() {
  analogWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

void sendSensorDataIfNeeded() {
  if (isRotating) return;

  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    float temp = dht.readTemperature();
    float humid = dht.readHumidity();
    int soilRaw = analogRead(SOILPIN);

    if (isnan(temp) || isnan(humid)) {
      Serial.println("❌ DHT 오류");
      return;
    }

    int soilPercent = map(soilRaw, wetValue, dryValue, 100, 0);
    soilPercent = constrain(soilPercent, 0, 100);

    String msg = "temp:" + String(temp, 1) + ",humid:" + String(humid, 1) + ",soil:" + String(soilPercent);
    BTSerial.println(msg);

    if(sendCount % 5 == 0)
    {
      Serial.println("--------------------");
      Serial.println("온도 : " + String(temp, 1) + "°C");
      Serial.println("습도 : " + String(humid, 1) + "%");
      Serial.println("토양습도 : " + String(soilPercent) +"%");
    }

    sendCount++;
  }
}

Direction getBrightestDirection() {
  int n = analogRead(northPin);
  int e = analogRead(eastPin);
  int s = analogRead(southPin);
  int w = analogRead(westPin);

  Serial.println("--------------------");
  Serial.println("☀️ 조도:");
  Serial.print("북: "); Serial.println(n);
  Serial.print("동: "); Serial.println(e);
  Serial.print("남: "); Serial.println(s);
  Serial.print("서: "); Serial.println(w);

  int minVal = n;
  Direction dir = NORTH;
  if (e < minVal) { minVal = e; dir = EAST; }
  if (s < minVal) { minVal = s; dir = SOUTH; }
  if (w < minVal) { minVal = w; dir = WEST; }

  return dir;
}

void rotateClockwise90() {
  isRotating = true;
  Serial.println("🔄 시계방향 90도 회전 중 ,,,");
  analogWrite(ENA, map(currentSpeed, 0, 100, 0, 255));
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  delay(24000);
  stopMotor();
  isRotating = false;
}

void rotateCounterClockwise90() {
  isRotating = true;
  Serial.println("🔄 반시계방향 90도 회전 중 ,,,");
  analogWrite(ENA, map(currentSpeed, 0, 100, 0, 255));
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  delay(24000);
  stopMotor();
  isRotating = false;
}

void rotate180() {
  isRotating = true;
  Serial.println("🔄 180도 회전 중 ,,,");
  analogWrite(ENA, map(currentSpeed, 0, 100, 0, 255));
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  delay(48000);
  stopMotor();
  isRotating = false;
}

void rotateToDirection(Direction target) {
  int diff = (target - currentDirection + 4) % 4;

  if (diff == 1) {
    rotateClockwise90();
  } else if (diff == 2) {
    rotate180();
  } else if (diff == 3) {
    rotateCounterClockwise90();
  }
}

void printDirection(Direction dir) {
  switch (dir) {
    case NORTH: Serial.println("북"); break;
    case EAST:  Serial.println("동"); break;
    case SOUTH: Serial.println("남"); break;
    case WEST:  Serial.println("서"); break;
  }
}
