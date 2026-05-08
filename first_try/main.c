#include "platform.h"
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include <string.h>
#include "queue.h"
#include "gpio.h"
#include "timer.h"


#define BUFF_SIZE 64 //read buffer length

Queue rx_queue; // Queue for storing received character
volatile int uart_time = 0;
volatile int blink_time = 0;
volatile int new_digit_time = 0;
volatile int start_uart_timer = 0;
volatile int reset_buffer = 0;
volatile int blink = 0; 
int loop_on = 0; 
volatile int HALT = 0; //Hasnt been used outside main still
volatile int delay = 0; 
volatile int toggle = 0;
volatile int new_digit;


//------------------ UART_ISR ---------------------------
void uart_rx_isr(uint8_t rx) {
	// Check if the received character is a printable ASCII character
	if (rx >= 0x0 && rx <= 0x7F && rx != '\r'){ //Exept the ENTER
		// Store the received character
		queue_enqueue(&rx_queue, rx);
		start_uart_timer = 1;
	}
	if (rx == '\r') {
		queue_enqueue(&rx_queue, rx); //Dont start the uart timer when enter or backspace
		uart_time = 4001; // Freeze the uart till we start pressing buttons again
	}
}

//------------------- TIMER_ISR-------------------------
void timer_isr(){

	//When uart sends a char we receive it set the timer flag back to zero 
	//And initialize timer
  if(start_uart_timer){
		  uart_time = 0;
		  start_uart_timer = 0;
	}
	//Increment while < 4000
	if(uart_time < 4000)
     uart_time++;
	//send the reset buffer signal and increment to not send continiusly the reset
  if(uart_time == 4000){
		reset_buffer = 1;
		uart_time ++;
	}	
	
  //Toggle
	if(blink){
      if(blink_time < delay)
				 blink_time ++;
			else{
				 blink_time = 0;
				 toggle = 1 ;
			}
  }
	
  //New digit
  if(blink) {
		 if(new_digit_time < 2000)
			  new_digit_time++;
		 else{
			 new_digit_time = 0;
			 new_digit = 1;
		 }
	}		
}

//-------------------- MAIN -------------------------

int main() {
	
	// Variables to help with UART read
	uint8_t rx_char = 0;
	char buff[2][BUFF_SIZE]; // The UART read string will be stored here
	uint32_t i; //cntr for blinking digits
	uint32_t write_buff, exec_buff, write_index, exec_index;
	//Initialize the indexes 
	write_buff = 0;
	exec_buff  = 1;
	write_index = 0;
	exec_index = 0;
	
	//Create the delay array
	int delay_array[] = {2001, 500, 250, 167, 125, 100, 83, 71, 63, 56};
	
	
	// Initialize the receive queue and UART
	queue_init(&rx_queue, 128);
	uart_init(115200);
	uart_set_rx_callback(uart_rx_isr); // Set the UART receive callback function
	uart_enable(); // Enable UART module
	
	//Initialize the board LED
	gpio_set_mode(P_LED_R, Output);
	gpio_set(P_LED_R, 0);
	
	// Initialize Timer and its ISR
	timer_init(1000); //1 need intrpt per 1 ms wich is 1000 us                
  timer_set_callback(timer_isr); //Set the ISR
	
	//Setting the priorities 
	//Pending the E-stop wich will be set to 0 AFTER we write the isr :< 
	NVIC_SetPriority(SysTick_IRQn, 1); //Timer 1  
  NVIC_SetPriority(USART2_IRQn, 2); //Uart 2
	
	__enable_irq(); // Enable interrupts
		timer_enable();
	
	uart_print("\r\n");// Print newline
  uart_print("Enter the numbers:");
	
	while(1) {
		
		
		if(!HALT) {
		
		
		//----------- WRITE BUFFER RESET IF NEEDED -----------------
		
		if(reset_buffer == 1) {
			write_index = 0;
			reset_buffer = 0;
			uart_print("\r\nEnter the numbers:");
		}
		
		//------------------- ASCII HANDLE ---------------------
		
		if(queue_dequeue(&rx_queue, &rx_char)) {
			
			if (rx_char == 0x7F) { // Handle backspace character
				if (write_index > 0) {
					write_index--; // Move write buffer  index back
					uart_tx(rx_char); // Send backspace character to erase on terminal
				}
		  }
		  //If ENTER has been pressed 
			else if (rx_char == '\r') { 
				 if (write_index == 0) continue; //In case we just press enter 
         loop_on = 0; //No loop by default
				 if(buff[write_buff][write_index - 1] == '-'){
					   loop_on = 1; //Set the loop by default
					   write_index --;// Go one position back because there is no blink for '-'
				 }
				 blink = 1; // Set blink signal
				 //Change the buffers
				 write_buff = !write_buff;
				 exec_buff = !exec_buff;
				 exec_index = write_index - 1;
				 write_index = 0; // To precent ovf
         delay = delay_array[buff[exec_buff][0] - '0']; //initalize the delay	
         i = 0; //Initialize the digits cntr
				 blink_time = 0;
				 new_digit_time = 0;
				 if (buff[exec_buff][0] == '0') gpio_set(P_LED_R, 0);
         else gpio_set(P_LED_R, 1);
         uart_print("\r\nEnter the numbers:");				 
         				 
			}
			else if ((rx_char >= '0' && rx_char <= '9') || rx_char == '-') {
				// Store and echo the received character back
				buff[write_buff][write_index++] = (char)rx_char; // Store character in buffer
				uart_tx(rx_char); // Echo character back to terminal
			}
	  }
		
		//--------------------------- BLINK -----------------------------
		if(blink == 1) {
			
			//TOGGLE
			if(toggle) {
				gpio_toggle(P_LED_R);
				toggle = 0;
			}
			if(new_digit) {
				i++;
				blink_time = 0;
				
				//In case we have multiple --
				while(i <= exec_index && buff[exec_buff][i] == '-') i++; 
				
				 //In case we have loop on go back to zero
				if(i > exec_index && loop_on) {
					 i = 0;
					 delay = delay_array[buff[exec_buff][0] - '0'];
					 new_digit = 0;
				}
				else if (i > exec_index && !loop_on){
					 blink = 0;
					 new_digit = 0;				
					 gpio_set(P_LED_R, 0); //Stop blinking
				}
				else{
					 delay = delay_array[buff[exec_buff][i] - '0'];
					 new_digit = 0;
				}
				
				//Since we still blinking
				if (blink == 1) { 
					if (buff[exec_buff][i] == '0') gpio_set(P_LED_R, 0);
					else gpio_set(P_LED_R, 1);
				}
			}
		}
		
		
	  }//!HALT if
		__WFI();
  } //While(1)
}	// Main