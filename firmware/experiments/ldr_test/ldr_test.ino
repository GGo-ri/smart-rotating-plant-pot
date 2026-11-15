// 조도센서를 연결한 아날로그 핀 번호
const int ldrPin = A0;  

void setup() {
  Serial.begin(9600);  // 시리얼 통신 시작 (속도: 9600bps)
}

void loop() {
  int ldrValue = analogRead(ldrPin);  // 조도값 읽기 (0~1023 범위)
  Serial.print("LDR Value: ");
  Serial.println(ldrValue);           // 시리얼 모니터에 출력

  delay(500);  // 0.5초마다 측정
}
