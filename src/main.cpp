#include <Arduino.h>
#include <ESP32Servo.h>

int servoPin = GPIO_NUM_25;
int buttonPin = GPIO_NUM_14;
Servo myServo;

const int upPosition = 180;
const int waveUp = 120;
const int waveDown = 20;
const int downPosition = 0;

int currentPosition = waveDown;

enum class State {
  Waiting,
  Off,
  NeedsWater,
};

State currentState = State::Waiting;
int64_t lastDrinkTimeSeconds = 0;
int64_t drinkIntervalSeconds = 20;
int64_t maxWaveSeconds = 30;

static int64_t getSecondsSinceStart() { return esp_timer_get_time() / 1000000; }

void setup() {
  Serial.begin(9600);

  pinMode(buttonPin, INPUT_PULLUP);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 1000, 2000);

  myServo.write(currentPosition);

  lastDrinkTimeSeconds = getSecondsSinceStart();
}

static bool buttonIsDown() { return digitalRead(buttonPin) == LOW; }

static void moveToPosition(const int target, const int delayPerDegree = 1,
                           bool allowIncomplete = false) {
  const int step = target > currentPosition ? 1 : -1;
  while (currentPosition != target) {
    if (allowIncomplete && buttonIsDown()) {
      return;
    }
    currentPosition += step;
    myServo.write(currentPosition);
    delay(delayPerDegree);
  }
}

void loop() {
  switch (currentState) {
  case State::NeedsWater: {
    Serial.println("Needs Water");
    const int64_t waveStartTime = getSecondsSinceStart();
    while (true) {
      const int64_t secondsSinceWaveStart =
          getSecondsSinceStart() - waveStartTime;
      int delayPerDegree = max<int64_t>(3, 10 - secondsSinceWaveStart / 4);
      moveToPosition(waveUp, delayPerDegree, true);
      moveToPosition(waveDown, delayPerDegree, true);
      if (buttonIsDown()) {
        currentState = State::Waiting;
        break;
      }
      if (secondsSinceWaveStart > maxWaveSeconds) {
        moveToPosition(downPosition, 10, true);
        while (true) {
          if (buttonIsDown()) {
            currentState = State::Waiting;
            break;
          }
        }
        break;
      }
    }
    break;
  }
  case State::Waiting: {
    Serial.println("Waiting");
    moveToPosition(waveDown, 10);
    delay(1000);
    lastDrinkTimeSeconds = getSecondsSinceStart();
    while (true) {
      if (lastDrinkTimeSeconds + drinkIntervalSeconds <
          getSecondsSinceStart()) {
        currentState = State::NeedsWater;
        break;
      }
      if (buttonIsDown()) {
        currentState = State::Off;
        break;
      }
    }
    break;
  }
  case State::Off: {
    Serial.println("Off");
    moveToPosition(upPosition, 10);
    delay(1000);
    while (true) {
      if (buttonIsDown()) {
        currentState = State::Waiting;
        break;
      }
    }
    break;
  }
  }
}
