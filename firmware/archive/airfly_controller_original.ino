</> C++

/*
  iPod AirFly Pro Modification - Single-File Sketch
  MCU: ATtiny85

  INPUT:
    PB2 (pin 2) - Tactile button (active LOW with internal pull-up)

  OUTPUT:
    PB0 (pin 0) - AirFly Reset control (transistor driver, active HIGH for 4000ms)

  BEHAVIOR:
    - Two intentional short taps (each between MIN_PRESS_MS and MAX_PRESS_MS)
      within MULTI_TAP_MS trigger a 4000ms Reset pulse on PB0.
    - Holding a press longer than MAX_PRESS_MS cancels an in-progress tap sequence.
    - A COOLDOWN_MS pause after Reset prevents immediate re-triggering.
*/

#include <Arduino.h>

// ---------------- PIN CONFIGURATION ----------------

constexpr uint8_t BUTTON_PIN = 2;  // PB2
constexpr uint8_t RESET_PIN  = 0;  // PB0

// ---------------- TIMING (ms) ----------------

constexpr unsigned long DEBOUNCE_MS   = 35;    // Switch bounce filter
constexpr unsigned long MIN_PRESS_MS  = 50;    // Minimum duration to count as a real tap
constexpr unsigned long MAX_PRESS_MS  = 350;   // Maximum duration for a short tap (longer cancels)
constexpr unsigned long MULTI_TAP_MS  = 550;   // Max gap between taps for a 2-click trigger
constexpr unsigned long RESET_HOLD_MS = 4000;  // AirFly Pro reset pulse duration
constexpr unsigned long COOLDOWN_MS   = 1500;  // Ignore new taps briefly after reset finishes

// ---------------- SEQUENCE CONFIG ----------------

constexpr uint8_t REQUIRED_TAPS = 2;

enum State : uint8_t {
  IDLE,
  COUNTING_TAPS,
  RESET_ACTIVE
};

State state = IDLE;

// ---------------- STATE VARIABLES ----------------

bool rawButton = HIGH;
bool buttonStable = HIGH;
bool buttonDown = false;
bool pressExceededMax = false;

uint8_t tapCount = 0;

unsigned long rawChangedAt = 0;
unsigned long pressStartedAt = 0;
unsigned long lastTapAt = 0;
unsigned long resetStartedAt = 0;
unsigned long cooldownUntil = 0;
bool hasCooldown = false;

// ---------------- HELPERS ----------------

bool isInCooldown(unsigned long now) {
  if (!hasCooldown) {
    return false;
  }
  // Signed difference handles millis() 49.7-day wraparound cleanly
  if (static_cast<long>(now - cooldownUntil) >= 0) {
    hasCooldown = false;
    return false;
  }
  return true;
}

void stopSequence() {
  state = IDLE;
  tapCount = 0;
  lastTapAt = 0;
}

void startReset(unsigned long now) {
  digitalWrite(RESET_PIN, HIGH);
  resetStartedAt = now;
  tapCount = 0;
  lastTapAt = 0;
  state = RESET_ACTIVE;
}

void endReset(unsigned long now) {
  digitalWrite(RESET_PIN, LOW);
  resetStartedAt = 0;
  cooldownUntil = now + COOLDOWN_MS;
  hasCooldown = true;
  stopSequence();
}

void recordTap(unsigned long pressStart, unsigned long releaseTime) {
  if (state == RESET_ACTIVE || isInCooldown(releaseTime)) {
    return;
  }

  const unsigned long pressDuration = releaseTime - pressStart;
  if (pressDuration < MIN_PRESS_MS || pressDuration > MAX_PRESS_MS) {
    return;
  }

  if (state != COUNTING_TAPS || (pressStart - lastTapAt > MULTI_TAP_MS)) {
    tapCount = 1;
    lastTapAt = pressStart;
    state = COUNTING_TAPS;
  } else {
    tapCount++;
    lastTapAt = pressStart;
  }

  if (tapCount >= REQUIRED_TAPS) {
    startReset(releaseTime);
  }
}

void updateButton(unsigned long now) {
  const bool reading = digitalRead(BUTTON_PIN);

  if (reading != rawButton) {
    rawButton = reading;
    rawChangedAt = now;
  }

  if ((now - rawChangedAt) >= DEBOUNCE_MS && reading != buttonStable) {
    buttonStable = reading;

    if (buttonStable == LOW) {
      buttonDown = true;
      pressExceededMax = false;
      pressStartedAt = now;
    } else {
      buttonDown = false;
      if (!pressExceededMax) {
        recordTap(pressStartedAt, now);
      }
    }
  }

  // Cancel tap counting on a long press without interrupting an active reset
  if (buttonDown &&
      !pressExceededMax &&
      (now - pressStartedAt > MAX_PRESS_MS)) {
    pressExceededMax = true;
    if (state == COUNTING_TAPS) {
      stopSequence();
    }
  }
}

void updateTimers(unsigned long now) {
  if (state == COUNTING_TAPS && !buttonDown && (now - lastTapAt > MULTI_TAP_MS)) {
    stopSequence();
  }

  if (state == RESET_ACTIVE && (now - resetStartedAt >= RESET_HOLD_MS)) {
    endReset(now);
  }
}

// ---------------- SETUP & LOOP ----------------

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
}

void loop() {
  const unsigned long now = millis();
  updateButton(now);
  updateTimers(now);
}
