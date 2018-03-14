#include <Arduino.h>
#include <TimerOne.h>

/*
 * Notes resolution in microseconds
 */
#define NOTE_RESOLUTION     40

/*
 * Recognized MIDI status bytes
 */
#define NOTE_ON     0b1001
#define NOTE_OFF    0b1000
#define PITCH_BEND  0b1110

/*
 * Pins
 */
#define PIN_STEP1           2
#define PIN_STEP2           4
#define PIN_STEP3           6
#define PIN_STEP4           8
#define PIN_STEP5           10
#define PIN_STEP6           12
#define PIN_STEP7           14
#define PIN_STEP8           16

#define PIN_DIR1            3
#define PIN_DIR2            5
#define PIN_DIR3            7
#define PIN_DIR4            9
#define PIN_DIR5            11
#define PIN_DIR6            13
#define PIN_DIR7            15
#define PIN_DIR8            17

/* 
 * First and last pin being used for floppies, used for looping over all pins.
 */
#define MIN_PIN             PIN_STEP1
#define MAX_PIN             PIN_DIR8

/*
 * Notes frequencies in microseconds
 */
const int microPeriods[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        30578, 28861, 27242, 25713, 24270, 22909, 21622, 20409, 19263, 18182, 17161, 16198, // C1 - B1
        15289, 14436, 13621, 12856, 12135, 11454, 10811, 10205, 9632, 9091, 8581, 8099, // C2 - B2
        7645, 7218, 6811, 6428, 6068, 5727, 5406, 5103, 4816, 4546, 4291, 4050, // C3 - B3
        3823, 3609, 3406, 3214, 3034, 2864, 2703, 2552, 2408, 2273, 2146, 2025, // C4 - B4
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * NOTE:
 *  Many of the arrays below contain unused indexes.
 *  This is to prevent the Arduino from having to convert a pin input to an alternate array index and save as many cycles as possible.
 *  In other words information for pin 2 will be stored in index 2, and information for pin 4 will be stored in index 4.
 */

/*
 * An array of maximum track positions for each stepper pin.
 * Even pins are used for stepper pins, so only even numbers need a value here.
 * 3.5" Floppies have 80 tracks, 5.25" have 50.
 * These should be doubled, because each tick is now half a position (use 158 and 98).
 */
const byte MAX_POSITION[] = {
        0, 0, 158, 0, 158, 0, 158, 0, 158, 0, 158, 0, 158, 0, 158, 0, 158, 0
};

/*
 * Array to track the current position of each floppy head.
 * Even pins are used for stepper pins, so only even numbers need a value here.
 */
byte currentPosition[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Array to keep track of state of each pin.
 * Even indexes track the stepper-pins for toggle purposes.
 * Odd indexes track direction-pins.
 * LOW = forward, HIGH=reverse
 */
byte currentState[] = {
        0, 0, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW, LOW
};

/*
 * Current period assigned to each pin.
 * 0 = off.
 * Each period is of the length specified by the NOTE_RESOLUTION variable above.
 * i.e. A period of 10 is (NOTE_RESOLUTION x 10) microseconds long.
 */
unsigned int currentPeriod[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Current tick
 */
unsigned int currentTick[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Functions declarations
 */
void tick(void);

void applyTick(byte stepperPin, byte directionPin);

void togglePin(byte stepperPin, byte directionPin);

void resetAll(void);

/*
 * Entry point
 */
void setup() {
    Serial.begin(19200);                 // Start serial communication
    Serial.print("setup... ");

    pinMode(PIN_STEP1, OUTPUT);         // Stepper 1
    pinMode(PIN_DIR1, OUTPUT);          // Direction 1
    pinMode(PIN_STEP2, OUTPUT);         // Stepper 2
    pinMode(PIN_DIR2, OUTPUT);          // Direction 2
    pinMode(PIN_STEP3, OUTPUT);         // Stepper 3
    pinMode(PIN_DIR3, OUTPUT);          // Direction 3
    pinMode(PIN_STEP4, OUTPUT);         // Stepper 4
    pinMode(PIN_DIR4, OUTPUT);          // Direction 4
    pinMode(PIN_STEP5, OUTPUT);         // Stepper 5
    pinMode(PIN_DIR5, OUTPUT);          // Direction 5
    pinMode(PIN_STEP6, OUTPUT);         // Stepper 6
    pinMode(PIN_DIR6, OUTPUT);          // Direction 6
    pinMode(PIN_STEP7, OUTPUT);         // Stepper 7
    pinMode(PIN_DIR7, OUTPUT);          // Direction 7
    pinMode(PIN_STEP8, OUTPUT);         // Stepper 8
    pinMode(PIN_DIR8, OUTPUT);          // Direction 8

    resetAll();                         // With all pins setup, let's do a first run reset
    delay(1000);

    Timer1.initialize(NOTE_RESOLUTION); // Set up a timer at the defined resolution
    Timer1.attachInterrupt(tick);       // Attach the tick function

    Serial.println("done");
}

void loop() {
    static byte message[3] = {};

    // Only read if we have 3 bytes waiting
    if (Serial.available() > 2) {
        Serial.readBytes(message, sizeof(message));
        const byte status = message[0], data1 = message[1], data2 = message[2];

        const byte command = status >> 4; // 1000CCCC -> 1000
        const byte channel = status & 0b1111; // 1000CCCC -> CCCC
        const byte pin = (channel + 1) * 2;

        uint16_t period;
        switch (command) {
            case NOTE_ON: {
                period = (data2 > 0) ? (uint16_t) microPeriods[message[1]] / 80 : 0;
                break;
            }
            case NOTE_OFF: {
                period = 0;
                break;
            }
            case PITCH_BEND: {
                if (currentPeriod[pin] > 0) {
                    period = (uint16_t) (
                            currentPeriod[pin] / pow(2, 200 * (((data2 << 7) | data1) - 8192) / (1200 * 8192))
                    );
                } else {
                    period = 0;
                }
                break;
            }
            default: {
                return;
            }
        }
        currentPeriod[pin] = period;
    }

    Serial.print("Got: ");
    Serial.print(message[0], BIN);
    Serial.print(' ');
    Serial.print(message[1], BIN);
    Serial.print(' ');
    Serial.println(message[2], BIN);
}

/*
 * Functions implementations
 */
void tick(void) {
    // Called by the timer interrupt at the specified resolution.
    applyTick(PIN_STEP1, PIN_DIR1);
    applyTick(PIN_STEP2, PIN_DIR2);
    applyTick(PIN_STEP3, PIN_DIR3);
    applyTick(PIN_STEP4, PIN_DIR4);
    applyTick(PIN_STEP5, PIN_DIR5);
    applyTick(PIN_STEP6, PIN_DIR6);
    applyTick(PIN_STEP7, PIN_DIR7);
    applyTick(PIN_STEP8, PIN_DIR8);
}

void applyTick(const byte stepperPin, const byte directionPin) {
    // If there is a period set for the stepper pin, count the number of ticks that pass, and toggle the pin
    // if the current period is reached.
    if (currentPeriod[stepperPin] > 0) {
        currentTick[stepperPin]++;
        if (currentTick[stepperPin] >= currentPeriod[stepperPin]) {
            togglePin(stepperPin, directionPin);
            currentTick[stepperPin] = 0;
        }
    }
}

void togglePin(const byte stepperPin, byte directionPin) {
    // Switch directions if end has been reached
    if (currentPosition[stepperPin] >= MAX_POSITION[stepperPin]) {
        currentState[directionPin] = HIGH;
        digitalWrite(directionPin, HIGH);
    } else if (currentPosition[stepperPin] <= 0) {
        currentState[directionPin] = LOW;
        digitalWrite(directionPin, LOW);
    }

    // Update currentPosition
    if (currentState[directionPin] == HIGH) {
        currentPosition[stepperPin]--;
    } else {
        currentPosition[stepperPin]++;
    }

    // Pulse the control pin
    digitalWrite(stepperPin, currentState[stepperPin]);
    currentState[stepperPin] = ~currentState[stepperPin];
}

void resetAll(void) {
    // Stop all notes (don't want to be playing during/after reset)
    for (byte pin = MIN_PIN; pin <= MAX_PIN; pin += 2) {
        currentPeriod[pin] = 0;             // Stop playing notes
    }

    // New all-at-once reset
    for (byte s = 0; s < 80; s++) {         // For max drive's position
        for (byte pin = MIN_PIN; pin <= MAX_PIN; pin += 2) {
            digitalWrite(pin + 1, HIGH);    // Go in reverse
            digitalWrite(pin, HIGH);
            digitalWrite(pin, LOW);
        }
        delay(5);
    }

    for (byte pin = MIN_PIN; pin <= MAX_PIN; pin += 2) {
        currentPosition[pin] = 0;           // We're reset.
        digitalWrite(pin + 1, LOW);
        currentState[pin + 1] = 0;          // Ready to go forward.
    }
}
