# 💡 LED Sequence Controller

> **Status:** Logic verified, pending hardware testing.
> **Architecture:** Non-blocking Interrupt-driven State Machine.

---

## 🏗️ System Architecture
### The "Ping-Pong" Buffer Strategy
We use a two-dimensional array `buff[2][64]` to manage incoming and outgoing data efficiently:

* **`write_buff`**: The active buffer receiving characters from UART.
* **`exec_buff`**: The buffer currently "driving" the LED sequence.
* **The Switch**: Upon pressing **ENTER**, the buffers swap roles. This allows the user to type a new sequence while the LED is still executing the previous one without any lag or interruption.

---

## ⚙️ Interrupts (ISR) Logic
The system uses interrupts to handle timing without using `delay()`, keeping the CPU free for other tasks.

### 📡 UART Reception (`uart_rx_isr`)
* Triggered on every keystroke.
* Stores characters in a circular `queue`.
* Resets the **4-second inactivity timeout**.

### ⏱️ System Heartbeat (`timer_isr` - 1ms Tick)
This ISR manages three critical software timers:
1.  **`uart_time`**: Tracks the 4-second timeout for buffer resets.
2.  **`blink_time`**: Handles the toggle frequency (ON/OFF) of the LED.
3.  **`new_digit_time`**: Triggers the transition to the next digit every 2 seconds.

---

## 🔴 Blinking Logic & Patterns
The execution is handled in the main loop under the `if(blink == 1)` state:

* **Digits '1'-'9'**: LED starts **ON** immediately for instant feedback, then toggles based on the `delay_array`.
* **Digit '0'**: Represents a 2-second pause (LED stays **OFF**).
* **Dash '-'**: Skipped automatically if found within a sequence.
* **Looping**: If a sequence ends with a dash (e.g., `123-`), the `loop_on` flag is set and the sequence repeats indefinitely.

---

## 🚦 Priority Hierarchy
To ensure timing accuracy, we override the driver defaults in `main()`:
1.  **Timer (Priority 1)**: High priority for precise blinking.
2.  **UART (Priority 2)**: Medium priority for data handling.
3.  *Emergency Stop (Priority 0 - Future)*.

---

## 📝 TODO List
- [ ] **E-STOP Implementation**: Develop `EXTI` interrupt for the blue push button (Priority 0).
- [ ] **UNLOCK Sequence**: Create logic to resume operation only when "UNLOCK" is typed.
- [ ] **Hardware Debugging**: Calibrate `delay_array` and verify UART echoes on the physical board.

---
*Developed for STM32 - Non-blocking firmware approach.*
