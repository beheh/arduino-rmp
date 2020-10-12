#include "TM1637_6D.h"
#include "EnableInterrupt.h"

class RotaryEncoder {
  private:
    byte pinA;
    byte pinB;
    bool aSet = false;
    bool bSet = false;
    bool stable = false;
    bool lastClickHigh = false;
    bool rotating = false;
    void (*increment)();
    void (*decrement)();

  public:
    RotaryEncoder(byte pinA, byte pinB, void(*increment)(), void(*decrement)()) {
      this->increment = increment;
      this->decrement = decrement;
      this->pinA = pinA;
      this->pinB = pinB;
      this->aSet = digitalRead(pinA);
      this->bSet = digitalRead(pinB);
      this->stable = aSet == bSet;
      this->lastClickHigh = aSet || bSet;
    }

    void interruptA() {
      delay(5);
      aSet = digitalRead(pinA);
      bSet = digitalRead(pinB);
      if (aSet != bSet) {
        stable = false;
      }
      if(!stable) {
        if(aSet == bSet) {
          // we're the second interrupt - so A lead
          decrement();
          stable = true;
        }
      }
    }

    void interruptB() {
      delay(5);
      aSet = digitalRead(pinA);
      bSet = digitalRead(pinB);
      if (aSet != bSet) {
        stable = false;
      }
      if(!stable) {
        if(aSet == bSet) {
          // we're the second interrupt - so A lead
          increment();
          stable = true;
        }
      }
    }

    void init() {
      pinMode(this->pinA, INPUT);
      pinMode(this->pinB, INPUT);
    }
};

#define SWAP_PIN 2
#define INNER_B_PIN 3
#define INNER_A_PIN 4
#define OUTER_A_PIN 5
#define OUTER_B_PIN 6
#define STANDBY_CLK_PIN 9
#define STANDBY_DIO_PIN 10
#define ACTIVE_CLK_PIN 11
#define ACTIVE_DIO_PIN 12

int prefix = 122;
int postfix = 800;
int lastPrefix = prefix;
int lastPostfix = postfix;

void decrementOuter() {
  /*if (--prefix < 118) {
    prefix += 136 - 118 + 1;
  }*/
  Serial.write("cmd=OuterDown\n");
}

void incrementOuter() {
  /*if (++prefix > 136) {
    prefix -= 136 - 118 + 1;
  }*/
  Serial.write("cmd=OuterUp\n");
}

void decrementInner() {
  /*postfix -= 100;
  if (postfix < 0) {
    postfix = 900;
  }*/
  Serial.write("cmd=InnerDown\n");
}

void incrementInner() {
  /*postfix += 100;
  if (postfix >= 1000) {
    postfix = 0;
  }*/
  Serial.write("cmd=InnerUp\n");
}

RotaryEncoder outer(OUTER_A_PIN, OUTER_B_PIN, incrementOuter, decrementOuter);
RotaryEncoder inner(INNER_A_PIN, INNER_B_PIN, incrementInner, decrementInner);
TM1637_6D active(ACTIVE_CLK_PIN, ACTIVE_DIO_PIN);
TM1637_6D standby(STANDBY_CLK_PIN, STANDBY_DIO_PIN);

void interruptOuterA() {
  outer.interruptA();
}

void interruptOuterB() {
  outer.interruptB();
}

void interruptInnerA() {
  inner.interruptA();
}

void interruptInnerB() {
  inner.interruptB();
}

void interruptSwap() {
  delay(1);
  if(digitalRead(SWAP_PIN)) {
    Serial.write("cmd=Swap\n");
  }
}

bool handled = true;
int messageBuffer[4] = {0, 0, 0, 0};
int8_t activeFreq[6];
int8_t standbyFreq[6];
int8_t loading[6] = {11 , 11, 11, 11, 11, 11};
int8_t ListDispPoint[6] = {POINT_OFF, POINT_OFF, POINT_OFF, POINT_ON, POINT_OFF, POINT_OFF};
bool powered = false;

void setup()
{
  Serial.begin(9600);
  Serial.setTimeout(10);
  Serial.write("reset\n");

  pinMode(SWAP_PIN, OUTPUT);
  digitalWrite(13, LOW);

  // init display
  active.init();
  active.set(7);
  active.display(loading, ListDispPoint);
  standby.init();
  standby.set(7);
  standby.display(loading, ListDispPoint);

  // init rotary
  inner.init();
  outer.init();
  enableInterrupt(OUTER_A_PIN, interruptOuterA, CHANGE);
  enableInterrupt(OUTER_B_PIN, interruptOuterB, CHANGE);
  enableInterrupt(INNER_A_PIN, interruptInnerA, CHANGE);
  enableInterrupt(INNER_B_PIN, interruptInnerB, CHANGE);

  // init button
  pinMode(SWAP_PIN, INPUT);
  digitalWrite(SWAP_PIN, HIGH);
  enableInterrupt(SWAP_PIN, interruptSwap, CHANGE);
}

void loop()
{
  int numBytes = Serial.available();
  for (int n = 0; n < numBytes; n++) {
    handled = false;
    messageBuffer[0] = messageBuffer[1];
    messageBuffer[1] = messageBuffer[2];
    messageBuffer[2] = messageBuffer[3];
    messageBuffer[3] = Serial.read();

    if(messageBuffer[0] == 255) {
      break;
    }
  }

  if (!handled) {
    if(messageBuffer[0] == 255) {
      // We've found the magic byte - let's assume this is a valid frequency
      int cmd = messageBuffer[1];
      switch(cmd) {
        case 0:
        case 1:
        {
          int offsetPrefix = messageBuffer[2] >> 2;
          int prefix;
          if(offsetPrefix == 0) {
            // test mode
            prefix = 888;
          }
          else {
            prefix = offsetPrefix + 100;
          }
          int extra = messageBuffer[2] & B11;
          int suffix = (extra << 8) | messageBuffer[3];

          int8_t frequency[6] = {0, 0, 0, 0, 0, 0};
          frequency[5] = prefix / 100;
          frequency[4] = prefix % 100 / 10;
          frequency[3] = prefix % 10;
          frequency[2] = suffix / 100;
          frequency[1] = suffix % 100 / 10;
          frequency[0] = suffix % 10;

          if (cmd == 1) {
            memcpy(activeFreq, frequency, sizeof(frequency[0])*6);
            if(powered) {
              active.display(activeFreq, ListDispPoint);
            }
          }
          else {
            memcpy(standbyFreq, frequency, sizeof(frequency[0])*6);
            if(powered) {
              standby.display(standbyFreq, ListDispPoint);
            }
          }
          break;
        }
        case 2:
        {
          int brightness = messageBuffer[2];
          if (brightness >= 0 && brightness <= 7) {
            active.set(brightness);
            standby.set(brightness);
            if (powered) {
              active.display(activeFreq, ListDispPoint);
              standby.display(standbyFreq, ListDispPoint);
            }
          }
          break;
        }
        case 3:
        {
          int received_power = messageBuffer[2];
          powered = received_power == 1;
          if (powered) {
            active.display(activeFreq, ListDispPoint);
            standby.display(standbyFreq, ListDispPoint);
          }
          else {
            active.clearDisplay();
            standby.clearDisplay();
          }
          break;
        }
      }

      handled = true;
    }
  }
}
