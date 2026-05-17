#include "platform.h"
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include <string.h>
#include "queue.h"
#include "timer.h"
#include "leds.h"
#include "gpio.h"

#define BUFF_SIZE 128

// --- Global Variables ---
Queue rx_queue;

// Timers and Flags
volatile uint32_t ms_cnt = 0;
//uart 4s timer
volatile int uart_time_out_flag = 0;
volatile uint32_t uart_timer = 0;
volatile int uart_timer_enable = 0;
volatile int uart_timer_rst = 0;

// led blink timer
volatile uint32_t blink_timer = 0;
volatile int blink_timer_enable = 0;
volatile int blink_timer_rst = 0;
volatile int new_digit_time_rst = 0 ;

// Sequence State
volatile uint32_t numbers[BUFF_SIZE]; // how much time does it has to stay on
volatile int times[BUFF_SIZE]; // how many trantitions for example for 1 it means 500ms on 500 off and thats 4 trantitions
volatile int total_digits = 0;
volatile int i = 0; // index for the sequence helper arrays
volatile int led_state = 0; // 0 = OFF, 1 = ON
volatile int is_continuous = 0; // if "-" on the end than its 1
volatile int sequence_done_flag = 0; // Flag for sequence completion
volatile uint32_t new_digit_time = 0; //Timer for changing every 2 seconds the blinking rate based on digit

// HALT & Unlock State
volatile int halt_mode = 0;              // 1 = System is halted
volatile int unlock_window = 0;          // 1 = Waiting for password
volatile uint32_t unlock_timer = 0;      // Counts to 5000ms
volatile int unlock_timeout_flag = 0;    // Flags when 5s is up
volatile int button_pressed_flag = 0;    // Set by external button interrupt
volatile int unlock_timer_rst = 0;       //reset the 5s timer


// Timer ISR : it counts 4s 5s and handles the time for each blinking
void timer_isr() {
     ms_cnt ++;  
}

// UART ISR
void uart_rx_isr(uint8_t rx) {
    if ( (rx >= '0' && rx <= '9') ||
				 (rx >= 'a' && rx <= 'z')	||
         (rx >= 'A' && rx <= 'Z') ||  
         (rx == '-') ||               
         (rx == '\r') ||              
         (rx == '\n') ||              
         (rx == '\b') ||              
         (rx == 0x7F) ) {             
        
        queue_enqueue(&rx_queue, rx);
    }
}

// Button ISR
void my_button_callback(int status) {
    button_pressed_flag = 1; 
}

int main() {
		
	  //Create the delay array
	  uint32_t delay_array[] = {2000, 500, 250, 167, 125, 100, 83, 71, 63, 56};
		
		// buffer and uart needed signals
    uint8_t rx_char = 0;
    char buff[BUFF_SIZE];
    uint32_t buff_index;
    
    // Initialize LED
    leds_init();
    
		// Initialize Button
    gpio_set_mode(PC_13, PullDown); 
    gpio_set_trigger(PC_13, Rising); 
    gpio_set_callback(PC_13, my_button_callback);
    
		//Initialize Timer
    timer_init(1000); // 1000 microseconds = 1 millisecond tick
    timer_set_callback(timer_isr);
    timer_enable();
    
		//Initialize Uart
    queue_init(&rx_queue, BUFF_SIZE);
    uart_init(115200);
    uart_set_rx_callback(uart_rx_isr);
    uart_enable();
    
		//Set Priorities
    NVIC_SetPriority(EXTI15_10_IRQn, 0); 
    NVIC_SetPriority(SysTick_IRQn, 1);      
    NVIC_SetPriority(USART2_IRQn, 2);
		
		//Enable Interrupts
    __enable_irq(); 
    
    //Start
    uart_print("\r\nSystem Initialized.\r\n");
    uart_timer_enable = 1; 
    uart_print("Enter sequence: ");
    buff_index = 0;

    // while loop wfi
    while(1) {
        
				// Halt and Unlock Mode
        if (button_pressed_flag) {
            button_pressed_flag = 0; // if flag was raised drop it 
            blink_timer_enable = 0;
					
            if (!halt_mode) {
                //its !halt_mode so when its first pressed because it starts with 0 it always will come here
                halt_mode = 1; // raise the flag so it wont come again on each on
                timer_disable(); // Disable timer during HALT
                leds_set(1, 0, 0); //set the led high
                uart_print("\r\n\n[HALT MODE] Sequence paused. Timer disabled.\r\n"); // info the user that its in halt mode
            } else {
							// if halt_mode = 1 then do nothing else
                if (!unlock_window) {
                    // same logic second press halt mode is 1 so it goes into else and the unlockwindow is by default init 0 so it comes here
                    unlock_window = 1; // raise the flag that starts the 5s timer, if after the 5s it hasnt given the pass then it stays in halt mode because the second if its going to be false
                    timer_enable(); // Re-enable timer to count the 5s window
                    unlock_timer_rst = 1; // mhdenise the unlock timer so it counts properly 5s 
                    buff_index = 0; //reset buffer 
                    uart_print("\r\nOverride requested. Awaiting password...\r\n"); // info the user
                }
            }
        }

				// if timeoutr on unlock window
        if (unlock_timeout_flag) {
            unlock_timeout_flag = 0; // reset the flag
            buff_index = 0; // reset buffer
            timer_disable(); // Failed to unlock in time, disable timer again
            uart_print("\r\n[TIMEOUT] Window closed. Back to HALT mode.\r\n");
        }
				
				//if 5s timer for unlocking the window
				if (unlock_window) {
            
					 if(unlock_timer_rst){
						 unlock_timer = ms_cnt; //Capture the current ms value 
						 unlock_timer_rst = 0; //Drop the flag to prevent continuous capture
					 }
					
          if (ms_cnt - unlock_timer >= 5000U) {
               unlock_window = 0;         // Close the window
               unlock_timeout_flag = 1;   // Tell main() we timed out
          }
       }
    

				//if uart time_out_flag 
        if (uart_time_out_flag) {
            uart_time_out_flag = 0; //reset flag 
					//this could be just uart_timer_enable = 0 and uart_time =0 when in unlock_window but its the same factionality
            if (buff_index > 0 && !unlock_window) { 
                buff_index = 0; 
                uart_print("\r\n[4s Timeout - Input Cleared]\r\nEnter sequence: ");
            }
        }
				
				//if uart 4 second timer
        if (uart_timer_enable) {
            
            if(uart_timer_rst){
							uart_timer = ms_cnt;
							uart_timer_rst = 0 ;//Drop the flag to stop continuous recapturing
            }							
           					
            if (ms_cnt - uart_timer >= 4000U) {
                uart_time_out_flag = 1;
							  uart_timer_enable = 0;
            }
        }
    
        // Sequence completion update
        if (sequence_done_flag) {
            sequence_done_flag = 0; // Clear the flag
            uart_print("\r\n[INFO] Sequence execution ended.\r\nEnter sequence: ");
        }


        // Etoimo uart handling
        if (queue_dequeue(&rx_queue, &rx_char)) {
            
            __disable_irq();
            uart_timer_rst = 1;      // start 4s window when a character comes   
            uart_time_out_flag = 0;
					  uart_timer_enable = 1 ;
            __enable_irq();
						
					// handle backspace 
            if (rx_char == 0x7F || rx_char == '\b') {
                if (buff_index > 0) {
                    buff_index--; 
                    uart_print("\b \b"); 
                }
            } 
						//if enter comes
            else if (rx_char == '\r') {
                buff[buff_index] = '\0'; 
                
                // halt mode handling when enter comes
                if (halt_mode) {
									// when in unlock window
                    if (unlock_window) {
											//eimaste large ama grapseis UnLock unlock UNLOCK h oti allo se syndiasmo kefalaia mikra to dexomaste
                        for (int k = 0; k < buff_index; k++) {
                            if (buff[k] >= 'a' && buff[k] <= 'z') {
                                buff[k] = buff[k] - 32;
                            }
                        }
												//check the pass
                        if (strcmp(buff, "UNLOCK") == 0) {
                            halt_mode = 0; //exit halt mode
                            unlock_window = 0; // exit unlock window mode
                            leds_set(0, 0, 0); // turn off the led
                            // Timer remains enabled, normal operation resumes
                            uart_print("\r\n[SUCCESS] Resuming normal operation.\r\n\nEnter sequence: ");
                        } else {
                            uart_print("\r\n[INCORRECT PASSWORD]\r\n");
                            unlock_window = 0; //exit unlock window
                            timer_disable(); // Wrong password, disable timer again
                        }
                    }
										//when in halt mode
										else {
                        uart_print("\r\n[ERROR] SYSTEM LOCKED\r\n");
                    }
										//reset the buffer
                    buff_index = 0; 
                } 
                //Normal
                else {
                    uart_print("\r\nExecuting: ");
                    uart_print(buff);
                    uart_print("\r\nEnter sequence: ");
                    
									//check for "-" at the end
                    int is_temp_continuous = 0;
                    if (buff_index > 0 && buff[buff_index - 1] == '-') {
                        is_temp_continuous = 1;
                        buff_index--; //discard "-" from the squence
                    }
										//Disable irq to set the numbers array
										__disable_irq(); 
                    
                    total_digits = 0; // Number of digits we will pass for blinking

                    for (int j = 0; j < buff_index; j++) {
                        char c = buff[j];
                        //Only passing the digits
                        if (c >= '0' && c <= '9') {
                             //Numbers matrix will contain the ms for each toggle
                            numbers[total_digits] = delay_array[c - '0'];      
                            total_digits++;
                        } 
                    }

                    
                    if (total_digits > 0) { //If we actually have digits to blink
                        is_continuous = is_temp_continuous; 
                        //Copy the is_continuous to now if we will have repetitions
                        // Reset blinking state machine
                        i = 0;
                        blink_timer_rst = 1;
                        new_digit_time_rst = 1; //Timer for 2 seconds change. Initialize at 0
											  if(buff[0] == '0') led_state = 0; //Check if first digit is zero to not blink
											  else led_state = 1;
												leds_set(led_state, 0, 0);
                        blink_timer_enable = 1; //Start the blink timer
                    } 
                    else {
                        //We don have any numbers
                        blink_timer_enable = 0; //Nothing to blink
                        leds_set(0, 0, 0);  //Set led as zero
                    }
                    __enable_irq();  //enabling interrupts
                     // reset the buffer for the next sequence
                    buff_index = 0; 
                  }
									
                }
            
            // Handle Normal Typing
            else {
                if (buff_index < BUFF_SIZE - 1) {
                    buff[buff_index++] = (char)rx_char; 
                    uart_tx(rx_char); 
                } 
                // Buffer Overflow Warning
                else {
                    uart_print("\r\n[WARNING] Buffer full! Maximum characters reached.\r\n");
                    uart_print("Enter sequence: ");
                }
            }
				}
        
         //------------------- HANDLE BLINKING --------------------
          // blink timer
          if (blink_timer_enable) {
						
						//Blink time reset
						if(blink_timer_rst){
							blink_timer_rst = 0; //Drop the flag
							blink_timer = ms_cnt; //Capture blink_timer
						}
						//New digit time reset 
            if(new_digit_time_rst){
							new_digit_time_rst = 0; //Drop the flag
							new_digit_time = ms_cnt; //Capture new digit time
							
            }							
              		
        
			       // it counts based on time 
               if (ms_cnt - blink_timer >= numbers[i]) {
                  blink_timer_rst = 1; //reset the blink timer 
            
                  // for 0 turn it off
                  if (numbers[i] == 2000) {
							      leds_set(0,0,0);
						      }
						      // Toggle led
						      else{
							      led_state = !led_state;
							      leds_set(led_state, 0, 0);
                  }
            
            
                  if (ms_cnt - new_digit_time >= 2000U) {
                      new_digit_time_rst = 1; //Reset the new digit time
                      i++; 
							        //again check if the number is not 0 initialize led at 1 else zero
                      if(numbers[i] == 2000) led_state = 0;
							        else led_state = 1;
							        leds_set(led_state,0,0);
                
                      if (i >= total_digits) {
                          if (is_continuous) {
                              i = 0; // Restart the sequence from the beginning
											        new_digit_time_rst = 1; //Reset new digit timer
                              blink_timer_rst = 1;		//Reset blink timer												
											       if(numbers[i] == 2000) led_state = 0;  //Do the same check for the led
							               else led_state = 1;
							               leds_set(led_state,0,0);
                          } else {
                              blink_timer_enable = 0; // Turn off sequence
                              leds_set(0, 0, 0);      // Force LED off
                              sequence_done_flag = 1; // Set end-of-sequence flag
                          }
                      }
                 }
              }
          }				
					//After all the cheks go to sleep
            __WFI(); 
        
	}		
}
