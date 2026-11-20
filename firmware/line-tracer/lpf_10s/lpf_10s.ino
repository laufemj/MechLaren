// --- Pin Definitions ---
const int IRPins[8] = {22, 24, 26, 28, 30, 32, 34, 36};
const int Dir1Pin_A = 23; // Left Motor Direction 1
const int Dir2Pin_A = 25; // Left Motor Direction 2
const int SpeedPin_A = 2;   // Left Motor Speed (PWM)
const int Dir1Pin_B = 27; // Right Motor Direction 1
const int Dir2Pin_B = 29; // Right Motor Direction 2
const int SpeedPin_B = 3;   // Right Motor Speed (PWM)
const int EchoPin = 8;
const int TrigPin = 9;

// --- PID Constants (튜닝이 필요한 값들) ---
float Kp = 135.0;
float Ki = 0.0;
float Kd = 235.0;

// --- Global Variables ---
float error = 0;
float lastError = 0; 
float integral = 0;
const float midPsn = 3.5; // 센서 중앙 위치 (0~7 기준)
int baseSpeed = 250;    // 목표 최고 속도

float filtered_psn = 3.5; 
float alpha = 0.3; 
float targetPsn = 3.5; // ⭐️ 전역 변수: 최종 PID 목표점

// --- 소프트 스타트(가속) 변수 ---
int currentSpeed = 0; 
int accelerationRate = 100; 

// --- 🚨 인코스 최적화 및 곡선 판단 변수 ---
float accumulatedError = 0.0; 
const float errorThreshold = 6.0; 
const float offset = 1.0; 
// -----------------------------------------------------

void setup() {
  Serial.begin(9600); // ⭐️ 수정: Serial 통신 속도를 9600에서 99600으로 높이면 출력이 더 원활할 수 있습니다.
  for (int i = 0; i < 8; i++) {
    pinMode(IRPins[i], INPUT);
  }
  pinMode(Dir1Pin_A, OUTPUT);
  pinMode(Dir2Pin_A, OUTPUT);
  pinMode(SpeedPin_A, OUTPUT);
  pinMode(Dir1Pin_B, OUTPUT);
  pinMode(Dir2Pin_B, OUTPUT);
  pinMode(SpeedPin_B, OUTPUT);

  pinMode(TrigPin, OUTPUT);
  pinMode(EchoPin, INPUT);
}

void stopMotors() {
  analogWrite(SpeedPin_A, 0);
  analogWrite(SpeedPin_B, 0);
  digitalWrite(Dir1Pin_A, LOW);
  digitalWrite(Dir2Pin_A, LOW);
  digitalWrite(Dir1Pin_B, LOW);
  digitalWrite(Dir2Pin_B, LOW);
  
  currentSpeed = 0; 
  
  accumulatedError = 0.0;
  targetPsn = midPsn; // 정지 시 목표점을 중앙으로 리셋
}

void loop() {
  long duration;
  float distance;
  digitalWrite(TrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(TrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(TrigPin, LOW);
  duration = pulseIn(EchoPin, HIGH);
  distance = duration * 0.034 / 2;

  if (distance < 17 && distance > 0) {
    stopMotors(); 
    Serial.println("Obstacle Detected! Stopping.");
  } 
  else {
    // --- (추가) 부드러운 가속 로직 ---
    if (currentSpeed < baseSpeed) {
      currentSpeed += accelerationRate;
      currentSpeed = min(currentSpeed, baseSpeed); 
    }
    
    int vleft = 0;
    int vright = 0;
    
    // --- 1. Read Sensor Values ---
    int IRValues[8];
    int min = 8, max = -1;
    int count = 0;
    for (int i = 0; i < 8; i++) {
      IRValues[i] = digitalRead(IRPins[i]);
      if (IRValues[i] == LOW) { // 라인 감지 (LOW 활성화 기준)
        if (min == 8) min = i;
        max = i;
        count++;
      }
    }

    // --- 2. Process Sensor Data and Control Motors ---
    if (count > 0) {
      float psn = (min + max) / 2.0; 
      
      filtered_psn = (alpha * psn) + ((1.0 - alpha) * filtered_psn);
      
      // 1. 현재 오차 계산 (라인 중앙 기준)
      float currentError = midPsn - filtered_psn; 
      
      // ⭐️ 핵심 수정: targetPsn의 리셋을 코너 판단 로직 안으로 이동 ⭐️
      // 현재 targetPsn 값을 유지한 채로 코너 판단 로직을 실행합니다.

      // 오차가 작은 흔들림(1.5 이하)은 무시하고, 지속적인 오차만 누적합니다.
      if (abs(currentError) > 1.5) {
          accumulatedError += currentError;
      }
      
      // 누적 오차를 이용한 코너 판단 및 목표점 이동
      if (abs(accumulatedError) > errorThreshold) {
          // 누적된 오차가 양수(> 0) -> 좌회전 코너로 판단
          if (accumulatedError > 0) { 
              targetPsn = 1.5; // 목표를 좌측 (인코스)으로 이동
          } 
          // 누적된 오차가 음수(< 0) -> 우회전 코너로 판단
          else { 
              targetPsn = 5.5; // 목표를 우측 (인코스)으로 이동
          }
          
          // 목표점 이동 후, 누적값 초기화
          accumulatedError = 0; 
      }
      // ⭐️ 수정된 리셋 로직: 누적 오차가 임계값 이하일 때만 중앙으로 복귀합니다. ⭐️
      else {
          targetPsn = midPsn; // 코너가 아닐 때만 중앙(3.5)으로 설정
      }
      // -----------------------------------------------------------
      
      error = targetPsn - filtered_psn; // 최종 목표점(targetPsn) 사용
      
      integral += error;
      integral = constrain(integral, -400, 400); 

      float derivative = error - lastError;
      float correction = Kp * error + Ki * integral + Kd * derivative;
      lastError = error; 

      vleft = currentSpeed - correction;
      vright = currentSpeed + correction;
      
      vleft = constrain(vleft, -255, 255);
      vright = constrain(vright, -255, 255);

      // --- Control Left Motor ---
      if (vleft >= 0) {
        digitalWrite(Dir1Pin_A, LOW); digitalWrite(Dir2Pin_A, HIGH);
        analogWrite(SpeedPin_A, vleft);
      } else {
        digitalWrite(Dir1Pin_A, HIGH); digitalWrite(Dir2Pin_A, LOW);
        analogWrite(SpeedPin_A, -vleft);
      }

      // --- Control Right Motor ---
      if (vright >= 0) {
        digitalWrite(Dir1Pin_B, LOW); digitalWrite(Dir2Pin_B, HIGH);
        analogWrite(SpeedPin_B, vright);
      } else {
        digitalWrite(Dir1Pin_B, HIGH); digitalWrite(Dir2Pin_B, LOW);
        analogWrite(SpeedPin_B, -vright);
      }
    } 
    // If the line is lost (라인 이탈)
    else {
      targetPsn = midPsn; // 라인 이탈 시 목표점은 중앙으로 초기화
      
      // (라인 이탈 시 로직은 그대로 유지)
      if (lastError > 0) { 
        digitalWrite(Dir1Pin_A, LOW);  digitalWrite(Dir2Pin_A, HIGH); 
        digitalWrite(Dir1Pin_B, LOW); digitalWrite(Dir2Pin_B, HIGH);  
        analogWrite(SpeedPin_A, 100);
        analogWrite(SpeedPin_B, 250); 
      } 
      else { 
        digitalWrite(Dir1Pin_A, LOW); digitalWrite(Dir2Pin_A, HIGH); 
        digitalWrite(Dir1Pin_B, LOW);  digitalWrite(Dir2Pin_B, HIGH); 
        analogWrite(SpeedPin_A, 250);
        analogWrite(SpeedPin_B, 100); 
      }
      accumulatedError = 0.0;
    }

    //--- 3. Debugging Output ---
    Serial.print("Err: ");
    Serial.print(error);
    Serial.print(" | TarPsn: "); // 현재 PID 목표 위치
    Serial.print(targetPsn);
    Serial.print(" | AccErr: "); // 누적 오차 값
    Serial.print(accumulatedError);
    Serial.print(" | L: ");
    Serial.print(vleft);
    Serial.print(" | R: ");
    Serial.println(vright);
  }

  delay(10); // 루프 주기를 10ms로 설정
}