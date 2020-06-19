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
volatile uint16_t stack_pointer[MAX_TASKS] = {0, 0};
volatile uint16_t max_stack[MAX_TASKS] = {200, 200};
volatile uint16_t stacks_head = RAMEND;

void __attribute__ ((used)) setup() {
    LED_SET;
    LED_OFF;

    Serial.begin(9600);
    Serial.println("\n\nZzZ\n");
    delay(1000);

    cur_task = 0;
    noInterrupts();
    TCCR1A = 0u; // Prescaler = 256
    TCCR1B = (1u << CS12); // Prescaler = 256
    OCR1A = 62499; // Overflow
    TIMSK1 = (1u << TOIE1); // Enable overflow interrupt
    TCNT1 = 0; // Initializes count to run IS
    //interrupts();
    asm volatile ("jmp spawn\n");
}


#define SAVE_CONTEXT "push r0\n" \
"in r0, __SREG__\n" \
"push r0\n" \
"push r1\n" \
"clr r1\n"  \
"push r2\n" \
"push r3\n" \
"push r4\n" \
"push r5\n" \
"push r6\n" \
"push r7\n" \
"push r8\n" \
"push r9\n" \
"push r10\n" \
"push r11\n" \
"push r12\n" \
"push r13\n" \
"push r14\n" \
"push r15\n" \
"push r16\n" \
"push r17\n" \
"push r18\n" \
"push r19\n" \
"push r20\n" \
"push r21\n" \
"push r22\n" \
"push r23\n" \
"push r24\n" \
"push r25\n" \
"push r26\n" \
"push r27\n" \
"push r28\n" \
"push r29\n" \
"push r30\n" \
"push r31\n"

#define RESTORE_CONTEXT "pop r31\n" \
"pop r30\n" \
"pop r29\n" \
"pop r28\n" \
"pop r27\n" \
"pop r26\n" \
"pop r25\n" \
"pop r24\n" \
"pop r23\n" \
"pop r22\n" \
"pop r21\n" \
"pop r20\n" \
"pop r19\n" \
"pop r18\n" \
"pop r17\n" \
"pop r16\n" \
"pop r15\n" \
"pop r14\n" \
"pop r13\n" \
"pop r12\n" \
"pop r11\n" \
"pop r10\n" \
"pop r9\n" \
"pop r8\n" \
"pop r7\n" \
"pop r6\n" \
"pop r5\n" \
"pop r4\n" \
"pop r3\n" \
"pop r2\n" \
"pop r1\n" \
"pop r0\n" \
"out __SREG__, r0\n" \
"pop r0\n"

ISR(TIMER1_OVF_vect, ISR_NAKED) {
    asm volatile (
    // Save state
    // oof 32 byte stack overhead
    SAVE_CONTEXT

    "in r22, __SP_L__\n"    "in r23, __SP_H__\n"

    // Load cur_task
    "lds r24, (cur_task)\n" "ldi r25, 0x0\n" // "lds r29, (cur_task + 1);"

    // Double cur_task to access two byte arrays
    "add r24, r24\n" "adc r25, r25\n"

    // Calculate stack pointer[cur] address
    "ldi r26, lo8(stack_pointer)\n" "ldi r27, hi8(stack_pointer)\n"
    "add r26, r24\n"    "adc r27, r25\n"
    "st X+, r22\n"      "st X, r23\n"


    // Update cur_task
    "lds r24, (cur_task)\n"
    "ldi r23, 0x1\n"
    "eor r24, r23\n"
    "sts (cur_task), r24\n"

    "spawn:\n"

    // Load cur_task
    "lds r24, (cur_task)\n" "ldi r25, 0x0\n" // "lds r29, (cur_task + 1);"

    // Double cur_task to access two byte arrays
    "add r24, r24\n" "adc r25, r25\n"

    // Calculate stack pointer[cur] address
    "ldi r26, lo8(stack_pointer)\n" "ldi r27, hi8(stack_pointer)\n"
    "add r26, r24\n"                "adc r27, r25\n"

    "ld r30, X+\n" "ld r31, X\n"
    "or r31, r30\n" // Check if both are zero:
    "ld r31, X\n" // I just messed up r31;
    "brne resume\n" // Branch if zero flag is cleared

    // Start new task::
    // ### STACK POINTER ###
    // Get stacks head
    "lds r22, (stacks_head)\n" "lds r23, (stacks_head + 1)\n"

    // Set SP and stack_pointer[cur_task]
    // Calculate stack pointer[cur] address
    // TODO: Dont double load
    "ldi r26, lo8(stack_pointer)\n" "ldi r27, hi8(stack_pointer)\n"
    "add r26, r24\n"                "adc r27, r25\n"

    "out __SP_L__, r22\n"   "out __SP_H__, r23\n"
    "st X+, r22\n"          "st X, r23\n"

    // Update stacks head: stacks_head = stacks_head - max_stack[cur_task]
    "ldi r26, lo8(max_stack)\n" "ldi r27, hi8(max_stack)\n"
    "add r26, r24\n"            "adc r27, r25\n"
    "ld r30, X+\n"              "ld r31, X\n"
    "sub r22, r30\n"            "sbc r23, r31\n"
    // TODO: Check for underflow i.e. stack overflow.
    "sts (stacks_head), r22\n"  "sts (stacks_head + 1), r23\n"


    // ### START TASK ###
    "ldi r26, lo8(tasks)\n" "ldi r27, hi8(tasks)\n"
    "add r26, r24\n"        "adc r27, r25\n"
    "ld r30, X+\n"          "ld r31, X\n"

    "sei\n" "ijmp\n"


    // Restore:
    "resume:\n"

    // Load cur_task
    "lds r24, (cur_task)\n" "ldi r25, 0x0\n" // "lds r29, (cur_task + 1);"
    // Double cur_task to access two byte arrays
    "add r24, r24\n"        "adc r25, r25\n"

    // Get stack_pointer[cur]
    "ldi r26, lo8(stack_pointer)\n" "ldi r27, hi8(stack_pointer)\n"
    "add r26, r24\n"                "adc r27, r25\n"
    "ld r30,  X+\n"                 "ld r31,  X\n"

    "out __SP_L__, r30\n"           "out __SP_H__, r31\n"

    RESTORE_CONTEXT

    "reti\n"

    : : : "memory"
    );
}

void __attribute__ ((unused)) loop() {}

void __attribute__ ((used, noinline, noreturn)) func_a() {
    Serial.println("Apple");
    while (true) {
        LED_ON;
        Serial.print("\ra");
        delay(200);
    }
}

void __attribute__ ((used, noinline, noreturn)) func_b() {
    Serial.println("Bee");
    while (true) {
        Serial.print("\rb");
        delay(200);
        LED_OFF;
    }
}
