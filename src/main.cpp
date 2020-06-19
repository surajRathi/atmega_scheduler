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
// volatile byte started[MAX_TASKS] = {}; // Replaced wth stack_pointer == 0
volatile uint16_t stack_pointer[MAX_TASKS] = {};
volatile uint16_t max_stack[MAX_TASKS] = {100, 100};
volatile uint16_t stacks_head = RAMEND;

void __attribute__ ((used)) setup() {
    LED_SET;
    LED_OFF;

    Serial.begin(9600);
    Serial.println("\n\nZzZ\n");
    delay(1000);

    cur_task = 1;
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
    "lds r24, (cur_task)\n"
    "ldi r23, 0x1\n"
    "eor r24, r23\n"
    "sts (cur_task), r24\n"

    // Load current task
    "lds r24, (cur_task)\n"
    "ldi r25, 0x0\n" // "lds r29, (cur_task + 1);"
    // ### Double cur_task too access two byte arrays###
    "add r24, r24\n"
    "adc r25, r25\n"

    // Calculate stack pointer[cur] address
    "ldi r26, lo8(stack_pointer)\n"
    "ldi r27, hi8(stack_pointer)\n"
    "add r26, r24\n"
    "adc r27, r25\n"

    "ld r30, X+\n"
    "ld r31, X\n"
    "or r31, r30\n" // Check if both are zero:
    "ld r31, X\n" // I just messed up r31;
    "brne resume\n" // Branch if zero flag is cleared

    // Start new task::

    // ### STACK POINTER ###
    // Get stacks head
    "lds r22, (stacks_head)\n"
    "lds r23, (stacks_head + 1)\n"

    // Set SP and stack_pointer[cur_task]
    // Calculate stack pointer[cur] address
    // TODO: Dont double load
    "ldi r26, lo8(stack_pointer)\n"
    "ldi r27, hi8(stack_pointer)\n"
    "add r26, r24\n"
    "adc r27, r25\n"
    "st X+, r22\n"
    "st X, r23\n"
    "out __SP_L__, r22\n"
    "out __SP_H__, r23\n"

    // Update stacks head: stacks_head = stacks_head - max_stack[cur_task]
    "ldi r26, lo8(max_stack)\n"
    "ldi r27, hi8(max_stack)\n"
    "add r26, r24\n"
    "adc r27, r25\n"
    "ld r30, X+\n"
    "ld r31, X\n"
    "sub r22, r30\n"
    "sbc r23, r31\n"
    // TODO: Check for underflow!!! i.e. stack overflow.
    "sts (stacks_head), r22\n"
    "sts (stacks_head + 1), r23\n"


    // ### START TASK ###
    "ldi r26, lo8(tasks)\n"
    "ldi r27, hi8(tasks)\n"
    "add r26, r24\n"
    "adc r27, r25\n"
    "ld r30, X+\n"
    "ld r31, X\n"
    "sei\n"
    "ijmp\n"


    // Restore:
    "resume:\n"
    "out __SP_L__, r30\n"
    "out __SP_H__, r31\n"
    // Restore state


    "reti\n"
    : : : "memory"



    // ELSE Set
    );
}

void __attribute__ ((unused)) loop() {}

void __attribute__ ((used, noinline, noreturn)) func_a() {
    Serial.println("A");
    Serial.println(stacks_head, HEX);
    Serial.println(stack_pointer[cur_task], HEX);
    while (true) {
        LED_ON;
        //Serial.println("aaaa");
        delay(1000);
    }
}

void __attribute__ ((used, noinline, noreturn)) func_b() {
    Serial.println("hellow beeeee");
    Serial.println(stacks_head, HEX);
    Serial.println(stack_pointer[cur_task], HEX);
    while (true)
        LED_OFF;
}
