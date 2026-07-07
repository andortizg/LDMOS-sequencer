#include <avr/wdt.h>

// TX/RX Sequencer for 1KW HF LDMOS Power Amplifier - ATtiny85
//
// Pin mapping:
// PB0 (pin5) - OVERDRIVE   : HIGH_PWR_IN from SP6GK TX-RX board, active HIGH
// PB1 (pin6) - RF_RELAY_IN : Input relay (TRX side) via BC547→PNP SP6GK (HIGH=active)
// PB2 (pin7) - PTT_OUT     : PTT signal to S21RC controller (HIGH=TX, 12V)
// PB3 (pin2) - PTT_IN      : Operator PTT input, active LOW (internal pull-up)
//                            Wired in series with STBY/OPER switch (NC=OPER)
// PB4 (pin3) - RF_RELAY_OUT: Output relay (ANT side) via BC547→PNP SP6GK (HIGH=active)
// PB5        - RESET, do not use
//
// Notes:
// - RF_RELAY_IN and RF_RELAY_OUT drive BC547 NPN transistors in common-emitter config
//   which pull the base of PNP drivers on the SP6GK board to GND, activating the RM85 relays
// - PTT_OUT drives the S21RC SSPA controller bias line (12V = TX active)
//   The S21RC controller activates the LDMOS bias and keys the transceiver
// - In STBY position, the STBY/OPER switch opens the PTT_IN line → micro never sees TX
//   and relay coil power is cut → RM85 relays return to NC (bypass) position
// - Overdrive protection (from SP6GK overpower detector) blocks PTT_OUT to S21RC
//   which interprets it as end of TX and cuts the LDMOS bias
//   Bias is NOT restored until operator releases PTT (manual reset required)
// - Hardware watchdog (250ms) resets the micro if firmware hangs,
//   returning all outputs to safe RX state via setup() initialization

#define OVERDRIVE    0
#define RF_RELAY_IN  1
#define PTT_OUT      2
#define PTT_IN       3
#define RF_RELAY_OUT 4

// Sequence timing in ms
#define T_RELAY     10   // RM85 mechanical relay switching time
#define T_BIAS      15   // margin for S21RC bias stabilization
#define T_RF_DECAY   8   // RF decay time after transceiver stops transmitting

bool inTX       = false;
bool biasKilled = false;  // true if PTT blocked due to overdrive protection

void setup() {
  wdt_disable();

  pinMode(OVERDRIVE,    INPUT);
  pinMode(RF_RELAY_IN,  OUTPUT);
  pinMode(PTT_OUT,      OUTPUT);
  pinMode(PTT_IN,       INPUT_PULLUP);
  pinMode(RF_RELAY_OUT, OUTPUT);

  // Safe initial state: RX
  // SP6GK relays in NC (bypass) position when BC547 is OFF
  digitalWrite(RF_RELAY_OUT, LOW);
  digitalWrite(RF_RELAY_IN,  LOW);
  digitalWrite(PTT_OUT,      LOW);  // LOW = S21RC in RX mode, bias OFF

  wdt_enable(WDTO_250MS);
}

// TX sequence:
//   1. ANT output relay switches to TX (no RF load, bias off)
//   2. TRX input relay switches to TX
//   3. PTT sent to S21RC (activates bias and keys transceiver)
void goTX() {
  digitalWrite(RF_RELAY_OUT, HIGH);
  delay(T_RELAY);
  wdt_reset();

  digitalWrite(RF_RELAY_IN, HIGH);
  delay(T_RELAY);
  wdt_reset();

  digitalWrite(PTT_OUT, HIGH);   // S21RC activates bias and transmits
  delay(T_BIAS);
  wdt_reset();

  inTX       = true;
  biasKilled = false;
}

// RX sequence:
//   1. PTT to S21RC released (S21RC cuts bias and stops transceiver)
//   2. Wait for RF decay
//   3. TRX input relay returns to RX
//   4. ANT output relay returns to RX
void goRX() {
  digitalWrite(PTT_OUT, LOW);    // S21RC cuts bias and stops transceiver
  delay(T_RF_DECAY);
  wdt_reset();

  digitalWrite(RF_RELAY_IN, LOW);
  delay(T_RELAY);
  wdt_reset();

  digitalWrite(RF_RELAY_OUT, LOW);

  inTX       = false;
  biasKilled = false;
}

// Overdrive protection: blocks PTT signal to S21RC
// S21RC interprets this as end of TX and cuts LDMOS bias
// Operator must release PTT to reset this condition
void killBias() {
  digitalWrite(PTT_OUT, LOW);
  biasKilled = true;
}

void loop() {
  wdt_reset();

  bool pttActive       = (digitalRead(PTT_IN)   == LOW);
  bool overdriveActive = (digitalRead(OVERDRIVE) == HIGH);

  // Overdrive protection: requires PTT release to reset
  if (inTX && overdriveActive && !biasKilled) {
    killBias();
  }

  // PTT management
  if (pttActive && !inTX) {
    goTX();
  } else if (!pttActive && inTX) {
    goRX();
  }
}
