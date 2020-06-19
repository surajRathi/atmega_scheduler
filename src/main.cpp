#include <Arduino.h>

#define GUARD asm volatile ("nop" ::: "memory")

#define LED_SET DDRB |= (1u << 7u)
#define LED_OFF PORTB &= ~(1u << 7u)
#define LED_ON PORTB |= (1u << 7u)


extern "C" {
void __attribute__ ((used, noinline, noreturn)) func_a();
void __attribute__ ((used, noinline, noreturn)) func_b();
}

const uint8_t MAX_TASKS = 2;

volatile uint8_t cur_task asm ("cur_task") = 0xff;

typedef void (*volatile func_ptr)();

func_ptr tasks[MAX_TASKS] asm ("tasks") = {&func_a, &func_b}; //  [2 ... MAX_TASKS] = nullptr};
volatile byte started[MAX_TASKS] = {};
volatile uint16_t stack_pointer[MAX_TASKS] = {};
volatile uint16_t max_stack[MAX_TASKS] = {};
volatile uint16_t stacks_head = RAMEND;

void __attribute__ ((used)) setup() {
    LED_SET;
    LED_OFF;
    cur_task = 0;
    noInterrupts();
    TCCR1A = 0u; // Prescaler = 256
    TCCR1B = (1u << CS12); // Prescaler = 256
    OCR1A = 62499; // Overflow
    TIMSK1 = (1u << TOIE1); // Enable overflow interrupt
    TCNT1 = OCR1A; // Initializes count to run IS
    interrupts();
}

ISR(TIMER1_OVF_vect, ISR_NAKED) {
    asm volatile (
    // Save state

    // Update cur_task

    // Load current task
    "lds r24, (cur_task)\n"
    "ldi r25, 0x0\n" // "lds r29, (cur_task + 1);"

    // IF !STARTED[CUR_TASK]
    // Set started
    "ldi r26, lo8(started)\n"
    "ldi r27, hi8(started)\n"
    "add r26, r24\n"
    "adc r27, r25\n"
    "ldi r23, 0xff\n" // Or 0x1 // true
    "st X, r23\n"

    // Double cur_task
    "add r24, r24\n"
    "adc r25, r25\n"



    // Start function
    "ldi r26, lo8(tasks)\n"
    "ldi r27, hi8(tasks)\n"
    "add r26, r24\n"
    "adc r27, r25\n"
    "ld r30, X+\n"
    "ld r31, X\n"
    "sei\n"
    "ijmp\n" : : : "memory"

    // ELSE Set
    );
}

void __attribute__ ((unused)) loop() {}

void __attribute__ ((used, noinline, noreturn)) func_a() {
    TIMSK1 = 0; // Enable overflow interrupt
    Serial.begin(9600);
    Serial.println("hellow world");
    Serial.println(cur_task);
    Serial.println(RAMEND, HEX);
    while (true) {
        LED_ON;
        Serial.println("aaaa");
        delay(1000);
    }
}

void __attribute__ ((used, noinline, noreturn)) func_b() {
    TIMSK1 = 0; // Enable overflow interrupt
    Serial.begin(9600);
    Serial.println("hellow beeeee");
    while (true)
        LED_ON;
}
