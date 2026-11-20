// --- Pin Definitions ---
const int IRPins[8] = {22, 24, 26, 28, 30, 32, 34, 36};
const int Dir1Pin_B = 25; // Left Motor Direction 1
const int Dir2Pin_B = 23; // Left Motor Direction 2
const int SpeedPin_B = 2;   // Left Motor Speed (PWM)
const int Dir1Pin_A = 27; // Right Motor Direction 1
const int Dir2Pin_A = 29; // Right Motor Direction 2
const int SpeedPin_A = 3;   // Right Motor Speed (PWM)
const int EchoPin = 8;
const int TrigPin = 9;

// --- PID Constants (튜닝이 필요한 값들) ---
float Kp = 105.0;
float Ki = 0.0;
float Kd = 200.0;

// --- Global Variables ---
float error = 0;
float lastError = 0; 
float integral = 0;
const float midPsn = 3.5; 
int baseSpeed = 200;    // (수정) 이제 '목표 최고 속도'가 됩니다.

float filtered_psn = 3.5; 
float alpha = 0.977; 

// --- (추가) 소프트 스타트(가속) 변수 ---
int currentSpeed = 0; // 현재 적용되는 기본 속도
int accelerationRate = 30; // 1 루프(10ms)당 증가할 속도 값 (튜닝 대상)


void setup() {
  Serial.begin(9600);
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
  
  // (추가) 장애물로 정지 시, 현재 속도를 0으로 리셋
  currentSpeed = 0; 
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

  if (distance < 24 && distance > 0) {
    stopMotors(); // (수정) 이 함수는 이제 currentSpeed를 0으로 리셋합니다.
    Serial.println("Obstacle Detected! Stopping.");
  } 
  else {
    // --- (추가) 부드러운 가속 로직 ---
    // 장애물이 없으면, 목표 속도(baseSpeed)에 도달할 때까지 현재 속도를 천천히 올립니다.
    if (currentSpeed < baseSpeed) {
      currentSpeed += accelerationRate;
      // 목표 속도(baseSpeed)를 넘지 않도록 제한합니다.
      currentSpeed = min(currentSpeed, baseSpeed); 
    }
    // ---------------------------------

    int vleft = 0;
    int vright = 0;
    
    // --- 1. Read Sensor Values ---
    int IRValues[8];
    int min = 8, max = -1;
    int count = 0;
    for (int i = 0; i < 8; i++) {
      IRValues[i] = digitalRead(IRPins[i]);
      if (IRValues[i] == LOW) { 
        if (min == 8) min = i;
        max = i;
        count++;
      }
    }

    // --- 2. Process Sensor Data and Control Motors ---
    if (count > 0) {
      float psn = (min + max) / 2.0; 
      
      filtered_psn = (alpha * psn) + ((1.0 - alpha) * filtered_psn);
      
      error = midPsn - filtered_psn; 
      
      integral += error;
      integral = constrain(integral, -400, 400); 

      float derivative = error - lastError;
      float correction = Kp * error + Ki * integral + Kd * derivative;
      lastError = error; 

      // --- (수정) 모터 속도 계산 시 'baseSpeed' 대신 'currentSpeed' 사용 ---
      vleft = currentSpeed - correction;
      vright = currentSpeed + correction;
      // ----------------------------------------------------------------
      
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
    // If the line is lost
    else {
      // (라인 이탈 시 로직은 그대로 유지)
      if (lastError > 0) { 
        digitalWrite(Dir1Pin_A, LOW);  digitalWrite(Dir2Pin_A, HIGH); 
        digitalWrite(Dir1Pin_B, LOW); digitalWrite(Dir2Pin_B, HIGH);  
        analogWrite(SpeedPin_A, 40);
        analogWrite(SpeedPin_B, 200);
      } 
      else { 
        digitalWrite(Dir1Pin_A, LOW); digitalWrite(Dir2Pin_A, HIGH); 
        digitalWrite(Dir1Pin_B, LOW);  digitalWrite(Dir2Pin_B, HIGH); 
        analogWrite(SpeedPin_A, 200);
        analogWrite(SpeedPin_B, 40);
      }
    }

    //--- 3. Debugging Output ---
    // Serial.print("Err: ");
    // Serial.print(error);
    // Serial.print(" | CurSpd: "); // (추가) 현재 가속도 확인용
    // Serial.print(currentSpeed);
    // Serial.print(" | L: ");
    // Serial.print(vleft);
    // Serial.print(" | R: ");
    // Serial.println(vright);
    // for (int i = 0; i <8; i++){
    //   Serial.print(IRValues[i]);
    // }
    // Serial.println();
  }

  delay(10); // A small delay to stabilize the loop
}