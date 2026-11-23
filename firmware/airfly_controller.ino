</> C++

/*
 #define F_CPU 1000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#define BTN_PIN  PB0   // Physical button to GND (internal pull-up enabled)
#define PWR_PIN  PB1   // Open-drain output to AirFly Power pad
#define RST_PIN  PB2   // Open-drain output to AirFly Reset pad

static inline void release_out(uint8_t pin) {
    DDRB &= ~(1 << pin);
    PORTB &= ~(1 << pin);
}

static inline void press_out(uint8_t pin) {
    PORTB &= ~(1 << pin);
    DDRB |= (1 << pin);
}

static inline uint8_t btn_is_pressed(void) {
    return !(PINB & (1 << BTN_PIN)); // Active LOW
}

ISR(PCINT0_vect) {
    // Wake from deep sleep on button press
}

int main(void) {
    // Outputs High-Z, internal pull-up enabled on PB0 (BTN_PIN)
    DDRB = 0x00;
    PORTB = (1 << BTN_PIN);

    // Enable Pin Change Interrupt on PB0
    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT0);
    sei();

    while (1) {
        // Sleep until button is pressed
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_enable();
        sleep_cpu();
        sleep_disable();

        _delay_ms(25); // Debounce initial press
        if (!btn_is_pressed()) continue;

        // Measure how long the 1st press is held (up to 350ms)
        uint16_t hold_ms = 0;
        while (btn_is_pressed() && hold_ms < 350) {
            _delay_ms(10);
            hold_ms += 10;
        }

        if (btn_is_pressed()) {
            // CASE 1: Single Press & Hold (>350ms) -> Pass straight to AirFly POWER
            press_out(PWR_PIN);
            while (btn_is_pressed()) {
                _delay_ms(10);
            }
            _delay_ms(25); // Debounce release
            release_out(PWR_PIN);
        } else {
            // CASE 2: 1st click was released quickly (<350ms) -> Bypass Power!
            // Wait up to 500ms for the 2nd press
            uint16_t gap_ms = 0;
            while (!btn_is_pressed() && gap_ms < 500) {
                _delay_ms(10);
                gap_ms += 10;
            }

            if (btn_is_pressed()) {
                _delay_ms(25); // Debounce 2nd press
                if (btn_is_pressed()) {
                    // 2nd click pressed within 500ms -> Hold AirFly RESET while held
                    press_out(RST_PIN);
                    while (btn_is_pressed()) {
                        _delay_ms(10);
                    }
                    _delay_ms(25);
                    release_out(RST_PIN);
                }
            }
            // If no 2nd press within 500ms, the single short tap is safely ignored
        }
    }
}
