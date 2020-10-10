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

#define OUTER_A_PIN 2
#define OUTER_B_PIN 3
#define INNER_A_PIN 4
#define INNER_B_PIN 5
#define ACTIVE_CLK_PIN 6
#define ACTIVE_DIO_PIN 7

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
TM1637_6D primary(ACTIVE_CLK_PIN, ACTIVE_DIO_PIN);

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

void setup()
{
  Serial.begin(9600);
  
  // init display
  primary.init();
  primary.set(3);
  updatePrimary();

  // init rotary
  inner.init();
  outer.init();  
  enableInterrupt(OUTER_A_PIN, interruptOuterA, CHANGE);
  enableInterrupt(OUTER_B_PIN, interruptOuterB, CHANGE);
  enableInterrupt(INNER_A_PIN, interruptInnerA, CHANGE);
  enableInterrupt(INNER_B_PIN, interruptInnerB, CHANGE);
}

//int8_t frequency[6] = {0, 0, 8, 2, 2, 1};
int8_t ListDispPoint[6] = {POINT_OFF, POINT_OFF, POINT_OFF, POINT_ON, POINT_OFF, POINT_OFF};

void updatePrimary() {
  int8_t final_freq[6];
  final_freq[5] = prefix / 100;
  final_freq[4] = prefix % 100 / 10;
  final_freq[3] = prefix % 10;
  final_freq[2] = postfix / 100;
  final_freq[1] = postfix % 100 / 10;
  final_freq[0] = postfix % 10;
  primary.display(final_freq, ListDispPoint);
  lastPrefix = prefix;
  lastPostfix = postfix;
}

void loop()
{
  delay(10);

  if(prefix != lastPrefix || postfix != lastPostfix) {
    updatePrimary();
  }
}
