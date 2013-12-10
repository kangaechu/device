#include "MorseListener.h"
#include "Global.h"
#include "pgmStrToRAM.h"

// #define DEBUG

// 0-1023
#define ON_MIN_THRESHOLD 570

// # standard morse tree
// T _    M _ _    O _ _ _    CH _ _ _ _
//                            Ö _ _ _ .
//                 G _ _ .    Q _ _ . _
//                            Z _ _ . .
//        N _ .    K _ . _    Y _ . _ _
//                            C _ . _ .
//                 D _ . .    X _ . . _
//                            B _ . . .
// E .    A . _    W . _ _    J . _ _ _
//                            P . _ _ .
//                 R . _ .    Ä . _ . _
//                            L . _ . .
//        I . .    U . . _    Ü . . _ _
//                            F . . _ .
//                 S . . .    V . . . _
//                            H . . . .

// # hex+/ morse tree
// # 4,5 should be used more frequently than others because they're used in ascii alphabet
// 4 _    3 _ _    0 _ _ _    ? _ _ _ _
//                            ? _ _ _ .
//                 1 _ _ .    ? _ _ . _
//                            ? _ _ . .
//        6 _ .    8 _ . _    ? _ . _ _
//                            ? _ . _ .
//                 9 _ . .    ? _ . . _
//                            A _ . . .
// 5 .    2 . _    B . _ _    ? . _ _ _
//                            ? . _ _ .
//                 C . _ .    ? . _ . _
//                            ? . _ . .
//        7 . .    D . . _    ? . . _ _
//                            ? . . _ .
//                 E . . .    F . . . _
//                            / . . . .

// prog_char morseTable[] PROGMEM = "ETIANMSURWDKGOHVF*L*PJBXCYZQ**54*3***2**+****16=/*****7***8*90*";
// morse table specialized to hex+/ [0-9A-F/]
prog_char morseTable[] PROGMEM = "547263EDCB9810/F******A*******";

MorseListener::MorseListener(int pin, uint16_t wpm) :
    pin_(pin)
{
    setWPM(wpm);

    enabled_ = false;

    clear();
}

void MorseListener::clear() {
    index_                    = -1; // next index = (index + 1) * 2 + (isDah ? 1 : 0)
    is_on_                    = false;
    word_started_             = false;
    did_call_letter_callback_ = false;
    last_changed_             = 0;
    last_on_                  = 0;
}

void MorseListener::setWPM(uint16_t wpm) {
    wpm_ = wpm;

    uint16_t t = 1200 / wpm_;
    debounce_period_  = t / 2;
    min_letter_space_ = t * 2;  // TODO: is this too short?
    min_word_space_   = t * 4;
}

void MorseListener::setup() {
    // when 13:
    //  min_letter_space_ 184
    //  min_word_space_   369
#ifdef DEBUG
    Serial.print(P("t/2 debouncePeriod:")); Serial.println(debounce_period_);
    Serial.print(P("tx2 minLetterSpace:")); Serial.println(min_letter_space_);
    Serial.print(P("tx4 minWordSpace:"));   Serial.println(min_word_space_);
    float letter = 1200. / (float)wpm_;
    Serial.print(P("tx1 dit interval:")); Serial.println(letter);
    Serial.print(P("tx3 dah interval:")); Serial.println(letter * 3);
#endif
}

void MorseListener::loop() {
    if (! enabled_) {
        return;
    }

    int  raw   = analogRead(pin_);
    static bool input = false;
#ifdef DEBUG
    Serial.print("raw: "); Serial.println(raw); // add delay when enabling this
    delay(1);
#endif

    unsigned long interval = 0;

    // analogRead input is 1kHz audio
    // we smooth it here

    if ( raw > ON_MIN_THRESHOLD ) {
        input    = true;
        last_on_ = global.now;
    }
    else if ( global.now - last_on_ > debounce_period_ ) {
        input    = false;
    }
    else if ( global.now < last_on_ ) {
        last_on_ = 0; // just in case, millis() passed unsigned long limit
    }

    // check ON/OFF state change

    if ( input ) {
        // ON
        if ( ! is_on_ ) {
            // OFF -> ON
            if (word_started_) {
                // interval: duration of OFF time
                interval = global.now - last_changed_;
            }
            is_on_        = true;
            last_changed_ = global.now;
            word_started_ = true;

#ifdef DEBUG
            Serial.print(P("off->on: ")); Serial.println(interval);
            Serial.print(P(" raw: ")); Serial.println(raw);
#endif
        }
    }
    else {
        // OFF
        interval = global.now - last_changed_;
        if ( is_on_ && word_started_ ) {
            // ON -> OFF
            // interval: duration of ON time
            is_on_                  = false;
            last_changed_           = global.now;
            did_call_letter_callback_ = false; // can call again after 1st letter

#ifdef DEBUG
            Serial.print(P("on->off: ")); Serial.println(interval);
            Serial.print(P(" raw: ")); Serial.println(raw);
#endif
        }
        else {
            // OFF continously
            // interval: duration of OFF time
        }
    }

    // decode

    if ( (interval > 0) && (! is_on_) && (last_changed_ == global.now) ) {
        // ON -> OFF
        // interval: duration of ON time

        index_ = (index_ + 1) * 2;
        // dah length == letter space length
        if (interval > min_letter_space_) {
            // dah detected
            index_ ++;
        }
        else {
            // dit detected
        }
    }
    else if ( interval > 0 ) {
        // OFF -> ON
        // or OFF continuously

        // interval: duration of OFF time

        if ( ! word_started_ ) {
            // OFF continously
        }
        else if ( (! did_call_letter_callback_) && (interval > min_letter_space_) ) {
            // detected letter space
            did_call_letter_callback_ = true;

#ifdef DEBUG
            Serial.print(P("index: ")); Serial.println(index_);
#endif

            char letter = pgm_read_byte_near(morseTable + index_);

            letterCallback( letter );

            // after letter detected
            index_ = -1;
        }
        else if ( interval > min_word_space_ ) {
            // detected word space

            wordCallback();

            // after word detected
            clear();
        }
    }
}

void MorseListener::enable(bool enabled) {
    enabled_ = enabled;
}
