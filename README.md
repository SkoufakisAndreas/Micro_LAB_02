LED Sequence Controller - Project Overview
This project implements a Non-Blocking State Machine to control an LED blinking sequence via UART. Unlike simple projects, this code does not use delay(), allowing it to handle multiple tasks (receiving data, timing timeouts, and blinking) simultaneously.

1. System Architecture: The "Ping-Pong" Buffers
We use a two-dimensional array buff[2][64] to handle data.

write_buff: The buffer currently receiving characters from UART.

exec_buff: The buffer currently "running" the LED sequence.

The Switch: When the user presses ENTER, the buffers swap roles. This allows the user to type a new sequence while the LED is still executing the previous one.

2. Interrupts (ISR) Logic
The system relies on two main interrupts to handle timing without blocking:

uart_rx_isr: Triggered every time a key is pressed. It places characters in a queue and resets the 4-second timeout timer.

timer_isr (1ms Tick): This is the "heartbeat" of the system. It increments three different counters:

uart_time: Tracks the 4-second inactivity timeout.

blink_time: Controls the frequency of the LED toggle (ON/OFF).

new_digit_time: Triggers the move to the next digit every 2 seconds.

3. The Blink State Machine (Main Loop)
The blinking logic is located inside while(1) under the if(blink == 1) condition:

Digits '1'-'9': The LED starts ON immediately for better responsiveness and then toggles based on the delay_array.

Digit '0': Represents a 2-second pause. The LED stays OFF. (Logic: The toggle delay is set to 2001ms, so it never triggers before the 2000ms digit-switch).

Dash '-': If a dash is found in the middle of a sequence, the code skips it and moves to the next digit.

Looping: If the sequence ends with a dash (e.g., 123-), the loop_on flag is set, and the sequence restarts from the beginning indefinitely.

4. Interrupt Priorities
We have manually overridden the driver defaults in main() to establish a hierarchy:

Timer (Priority 1): High priority to ensure blinking and timeouts are accurate.

UART (Priority 2): Lower priority. It’s okay if a character is delayed by a few microseconds.
(Note: The E-STOP will be set to Priority 0 later).

5. Current Status & TODOs
Compilation: The code is clean and compiles without errors.

Hardware Test: NOT TESTED YET. Physical verification on the board is required to calibrate the delay_array and verify UART echoes.

TODO: E-STOP: We need to implement the EXTI interrupt for the blue push button. This should set the HALT flag to 1 and kill the LED immediately.

TODO: UNLOCK: We need a mechanism to check for the string "UNLOCK" when the system is in HALT mode to resume operation.

Note for Collaborators: Look for __WFI() in the main loop. This command puts the CPU to sleep between interrupts to save power while waiting for the next 1ms tick or UART character.
