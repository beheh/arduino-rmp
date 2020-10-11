/*
   Example for 6-digit TM1637 based segment Display
   The number of milliseconds after start will be displayed on the 6-digit screen

   The MIT License (MIT)

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

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
int lastThreeBytes[4] = {0, 0, 0, 0};
int8_t loading[6] = {11 , 11, 11, 11, 11, 11};
int8_t ListDispPoint[6] = {POINT_OFF, POINT_OFF, POINT_OFF, POINT_ON, POINT_OFF, POINT_OFF};

void setup()
{
  Serial.begin(9600);
  Serial.setTimeout(10);
  pinMode(SWAP_PIN, OUTPUT);
  digitalWrite(13, LOW);
  
  // init display
  active.init();
  active.set(1);
  active.display(loading, ListDispPoint);
  standby.init();
  standby.set(2);
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
    lastThreeBytes[0] = lastThreeBytes[1];
    lastThreeBytes[1] = lastThreeBytes[2];
    lastThreeBytes[2] = lastThreeBytes[3];
    lastThreeBytes[3] = Serial.read();
  }

  if (!handled) {
    if(lastThreeBytes[0] == 255) {
      // We've found the magic byte - let's assume this is a valid frequency

      bool isActive = lastThreeBytes[1] & B1 > 0;
      int offsetPrefix = lastThreeBytes[2] >> 2;
      int prefix = offsetPrefix + 100;
      int extra = lastThreeBytes[2] & B11;
      int suffix = (extra << 8) | lastThreeBytes[3];
      
      int8_t frequency[6] = {0, 0, 0, 0, 0, 0};
      frequency[5] = prefix / 100;
      frequency[4] = prefix % 100 / 10;
      frequency[3] = prefix % 10;
      frequency[2] = suffix / 100;
      frequency[1] = suffix % 100 / 10;
      frequency[0] = suffix % 10;

      if (isActive) {
        active.display(frequency, ListDispPoint);
      }
      else {
        standby.display(frequency, ListDispPoint);
      }

      handled = true;
    }
  }
}
