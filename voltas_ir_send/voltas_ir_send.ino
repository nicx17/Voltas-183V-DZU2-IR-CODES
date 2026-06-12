/*
 * Voltas 183V DZU2 AC - IR Transmitter
 * 
 * Hardware: Arduino Uno R4 WiFi + IR LED
 * 
 * WIRING (simple direct-drive for testing):
 * 
 *   Arduino Pin 3 ---[100 ohm resistor]---[IR LED +]---[IR LED -]--- GND
 * 
 *   - Pin 3 = IR send (PWM timer pin)
 *   - 100 ohm resistor (use 68-150 ohm, lower = brighter but more current)
 *   - IR LED: longer leg = anode (+), shorter leg = cathode (-)
 *   - Range: ~1-2 meters with direct drive
 *
 * WIRING (transistor-boosted for better range):
 *
 *   Arduino Pin 3 ---[1K resistor]--- NPN Base
 *                                     NPN Emitter --- GND
 *                                     NPN Collector --- IR LED cathode (-)
 *                                                      IR LED anode (+) ---[47 ohm]--- 5V
 *
 *   - NPN transistor: 2N2222, BC547, or similar
 *   - Range: 3-5+ meters
 *
 * Instructions:
 * 1. Upload this sketch
 * 2. Open Serial Monitor at 115200 baud
 * 3. Send 'P' to toggle power (ON/OFF alternates)
 * 4. Point the IR LED at the Voltas AC indoor unit receiver
 */

#define IR_SEND_PIN 3

#include <IRremote.hpp>

// ============================================================
// CAPTURED IR DATA from Voltas 183V DZU2 via VS1838B receiver
// Protocol: Unknown (48-bit, 2 frames, header + data)
// ============================================================

// Signal 1 (Power OFF / State 1)
const uint16_t signal1[] = {
    // Frame 1: header + 48 bits
    4430,4470,
    530,1620, 530,520, 530,1670, 530,1620,  // 1011
    530,520, 530,520, 530,1620, 580,470,     // 0010
    530,520, 530,1670, 530,520, 530,520,     // 0100
    530,1620, 530,1620, 530,520, 530,1670,   // 1101
    530,520, 530,520, 530,1620, 530,1620,    // 0011
    530,1670, 480,1670, 530,1620, 530,1620,  // 1111
    530,1670, 480,1670, 530,520, 530,520,    // 1100
    530,520, 530,520, 530,520, 530,520,      // 0000
    530,1670, 480,1670, 530,520, 530,520,    // 1100
    530,520, 530,520, 530,520, 530,520,      // 0000
    530,520, 530,570, 480,1670, 530,1620,    // 0011
    530,1620, 530,1670, 480,1670, 530,1620,  // 1111
    // Gap between frames
    530,5220,
    // Frame 2: repeat of frame 1
    4480,4470,
    530,1620, 530,520, 530,1620, 530,1670,
    480,570, 480,570, 530,1620, 530,520,
    530,520, 530,1620, 530,520, 530,570,
    480,1670, 530,1620, 530,520, 530,1620,
    530,520, 530,570, 480,1670, 530,1620,
    530,1620, 530,1670, 480,1670, 530,1620,
    530,1620, 530,1620, 530,570, 480,570,
    480,570, 530,520, 530,520, 530,520,
    530,1620, 530,1620, 530,570, 480,570,
    480,570, 530,520, 530,520, 530,520,
    530,520, 530,520, 530,1620, 530,1670,
    530,1620, 530,1620, 530,1620, 530,1670,
    480
};

// Signal 2 (Power ON / State 2)
const uint16_t signal2[] = {
    // Frame 1: header + 48 bits
    4430,4470,
    530,1620, 530,570, 480,1670, 530,1620,  // 1011
    530,520, 530,520, 530,1620, 530,520,     // 0010
    530,570, 480,1670, 530,520, 530,520,     // 0100
    530,1620, 530,1620, 530,570, 480,1670,   // 1101
    530,520, 530,1620, 530,1620, 530,1670,   // 0111
    480,1670, 530,520, 530,1620, 530,1620,   // 1011
    530,1670, 480,570, 480,570, 530,520,     // 1000
    530,520, 530,1620, 530,520, 530,520,     // 0100
    530,1670, 530,1620, 530,1620, 530,520,   // 1110
    530,520, 530,520, 530,570, 480,570,      // 0000
    480,570, 530,520, 530,520, 530,1620,     // 0001
    530,1620, 530,1670, 480,1670, 530,1620,  // 1111
    // Gap between frames
    530,5270,
    // Frame 2: repeat of frame 1
    4430,4470,
    530,1620, 530,520, 530,1620, 530,1670,
    480,570, 480,570, 530,1620, 530,520,
    530,520, 530,1620, 530,570, 480,570,
    480,1670, 530,1620, 530,520, 530,1620,
    530,570, 480,1670, 530,1620, 530,1620,
    530,1670, 480,570, 480,1670, 530,1620,
    530,1620, 530,520, 530,520, 530,570,
    480,570, 480,1670, 530,520, 530,520,
    530,1620, 530,1670, 480,1670, 530,520,
    530,520, 530,520, 530,520, 530,520,
    530,520, 530,520, 530,570, 480,1670,
    530,1620, 530,1620, 530,1620, 530,1670,
    480
};

const uint16_t signal1_len = sizeof(signal1) / sizeof(signal1[0]);
const uint16_t signal2_len = sizeof(signal2) / sizeof(signal2[0]);

bool acIsOn = false;

void setup() {
    Serial.begin(115200);
    while (!Serial);
    
    IrSender.begin(IR_SEND_PIN);
    
    Serial.println(F("=== Voltas 183V DZU2 AC IR Transmitter ==="));
    Serial.println(F("IR LED on pin 3"));
    Serial.println();
    Serial.println(F("Commands (send via Serial Monitor):"));
    Serial.println(F("  P = Toggle Power ON/OFF"));
    Serial.println(F("  1 = Send Signal 1 (first captured state)"));
    Serial.println(F("  2 = Send Signal 2 (second captured state)"));
    Serial.println();
    Serial.println(F("Point IR LED at AC and send a command."));
    Serial.println();
}

void sendSignal(const uint16_t *data, uint16_t len, const char *name) {
    Serial.print(F("Sending: "));
    Serial.print(name);
    Serial.print(F(" ("));
    Serial.print(len);
    Serial.println(F(" values)..."));
    
    IrSender.sendRaw(data, len, 38);  // 38 kHz carrier
    
    Serial.println(F("Sent!"));
    Serial.println();
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        
        switch (cmd) {
            case 'P':
            case 'p':
                if (acIsOn) {
                    sendSignal(signal1, signal1_len, "Power OFF (Signal 1)");
                    acIsOn = false;
                } else {
                    sendSignal(signal2, signal2_len, "Power ON (Signal 2)");
                    acIsOn = true;
                }
                Serial.print(F("AC state: "));
                Serial.println(acIsOn ? F("ON") : F("OFF"));
                break;
                
            case '1':
                sendSignal(signal1, signal1_len, "Signal 1");
                break;
                
            case '2':
                sendSignal(signal2, signal2_len, "Signal 2");
                break;
                
            default:
                break;
        }
    }
}
