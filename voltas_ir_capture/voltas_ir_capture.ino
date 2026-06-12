/*
 * Voltas AC IR Code Capture
 * 
 * Hardware: Arduino Uno R4 WiFi + HW-477 (VS1838B) IR receiver
 * Wiring:  HW-477 S -> Pin 2, + -> 5V, - -> GND
 * 
 * Uses IRremote library v4.x
 * Based on the official ReceiveDump example.
 * 
 * Instructions:
 * 1. Upload this sketch
 * 2. Open Serial Monitor at 115200 baud
 * 3. Point the Xiaomi phone IR blaster (or original Voltas remote) at the VS1838B
 * 4. Press buttons - the raw timing data will appear in Serial Monitor
 * 5. Copy the output and save it
 */

// CRITICAL: Must be defined BEFORE including IRremote.hpp
// AC protocols send long signals (655+ values), default buffer is too small
#define RAW_BUFFER_LENGTH 750

#include <IRremote.hpp>

#define IR_RECEIVE_PIN 2

void setup() {
    Serial.begin(115200);
    while (!Serial)
        ;  // Wait for serial on R4 WiFi (USB CDC)
    
    Serial.println(F("=== Voltas AC IR Code Capture ==="));
    Serial.println(F("Hardware: Arduino Uno R4 WiFi + VS1838B on pin 2"));
    Serial.print(F("IR buffer size: "));
    Serial.println(RAW_BUFFER_LENGTH);
    Serial.println();
    Serial.println(F("Point your Xiaomi phone or Voltas remote at the receiver."));
    Serial.println(F("Waiting for IR signal..."));
    Serial.println();
    
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
}

void loop() {
    if (IrReceiver.decode()) {
        Serial.println(F("============================================"));
        Serial.println(F("========== IR SIGNAL RECEIVED =============="));
        Serial.println(F("============================================"));
        Serial.println();
        
        // Check for buffer overflow
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW) {
            Serial.println(F("WARNING: IR buffer overflow! Signal too long."));
            Serial.println(F("Increase RAW_BUFFER_LENGTH if needed."));
            Serial.println();
        }
        
        // --- Section 1: Protocol identification ---
        IrReceiver.printIRResultShort(&Serial);
        Serial.println();
        
        // --- Section 2: Raw timing data (formatted) ---
        Serial.println(F("--- RAW TIMING DATA (formatted) ---"));
        IrReceiver.printIRResultRawFormatted(&Serial, true);
        Serial.println();
        
        // --- Section 3: C array format (microseconds) -- always printed ---
        Serial.println(F("--- RAW AS C ARRAY (microseconds) ---"));
        IrReceiver.compensateAndPrintIRResultAsCArray(&Serial, true);
        Serial.println();
        
        // --- Section 4: Send code (for replaying) ---
        if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
            Serial.println(F("--- SEND CODE (for replaying) ---"));
            IrReceiver.printIRSendUsage(&Serial);
            Serial.println();
        }
        
        // --- Section 5: Pronto hex (universal format) ---
        Serial.println(F("--- PRONTO HEX ---"));
        IrReceiver.compensateAndPrintIRResultAsPronto(&Serial);
        Serial.println();
        
        // --- Section 6: Source code variables ---
        if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
            Serial.println(F("--- SOURCE CODE VARIABLES ---"));
            IrReceiver.printIRResultAsCVariables(&Serial);
            Serial.println();
        }
        
        Serial.println(F("============================================"));
        Serial.println(F("========== END SIGNAL ======================"));
        Serial.println(F("============================================"));
        Serial.println();
        Serial.println();
        
        IrReceiver.resume();
    }
}
