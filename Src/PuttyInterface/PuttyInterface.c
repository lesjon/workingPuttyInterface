/*
 * PuttyInterface.c
 *
 *  Created on: Sep 29, 2017
 *      Author: Leon
 */

#include "PuttyInterface.h"

#if __has_include("stm32f0xx_hal.h")
#  include "stm32f0xx_hal.h"
#elif  __has_include("stm32f3xx_hal.h")
#  include "stm32f3xx_hal.h"
#elif  __has_include("stm32f4xx_hal.h")
#  include "stm32f4xx_hal.h"
#endif

#include <string.h>
#include <stdio.h>

char smallStrBuffer[1024];

#ifdef PUTTY_USB
bool usb_comm = false;
#endif

// clears the current line, so new text can be put in
static void ClearLine(){
	TextOut("\r                                                                                        \r");
}

// modulo keeping the value within the real range
// val is the start value,
// dif is the difference that will be added
// modulus is the value at which it wraps
static uint8_t wrap(uint8_t val, int8_t dif, uint8_t modulus)
{
	dif %= modulus;
	if(dif < 0)
		dif += modulus;
	dif += (val);
	if(dif >= modulus)
		dif -= modulus;
	return (uint8_t) dif;
}
// This function deals with input by storing values and calling functoni func with a string of the input when its done
// also handles up and down keys
// input is a pointer to the characters to handle,
// n_chars is the amount of chars to handle
// func is the function to call when a command is complete
static void HandlePcInput(char * input, size_t n_chars, HandleLine func){
	static char PC_Input[COMMANDS_TO_REMEMBER][MAX_COMMAND_LENGTH];
	static uint8_t PC_Input_counter = 0;
	static int8_t commands_counter = 0;
	static int8_t kb_arrow_counter = 0;
	static uint8_t commands_overflow = 0;
	if(input[0] == 0x0d){//newline, is end of command
		kb_arrow_counter = 0;// reset the arrow input counter
		PC_Input[commands_counter][PC_Input_counter++] = '\0';
		TextOut("\r");
		TextOut(PC_Input[commands_counter]);
		TextOut("\n\r");
		PC_Input_counter = 0;
		func(PC_Input[commands_counter++]);// Callback func
        commands_overflow = !(commands_counter = commands_counter % COMMANDS_TO_REMEMBER) || commands_overflow;// if there are more than the maximum amount of stored values, this needs to be known
        PC_Input[commands_counter][0] = '\0';
	}else if(input[0] == 0x08){//backspace
		if(PC_Input_counter != 0)
			PC_Input_counter--;
	}else if(input[0] == 0x1b){//escape, also used for special keys in escape sequences
		if(n_chars > 1){// an escape sequence
			uprintf("more than one char\n\r");
			switch(input[1]){
				case 0x5b:// an arrow key
					switch(input[2]){
						case 'A'://arrow ^
							kb_arrow_counter--;
							break;
						case 'B'://arrow \/;
							kb_arrow_counter++;
							break;
						case 'C'://arrow ->
							break;
						case 'D'://arrow <-
							break;
					}
					uint8_t cur_pos = commands_overflow ? wrap(commands_counter, kb_arrow_counter, COMMANDS_TO_REMEMBER) : wrap(commands_counter, kb_arrow_counter, commands_counter+1);
					PC_Input_counter = strlen(PC_Input[cur_pos])-1;
					PC_Input[cur_pos][PC_Input_counter] = '\r';
					strcpy(PC_Input[commands_counter], PC_Input[cur_pos]);
					ClearLine();
					TextOut(PC_Input[commands_counter]);
					break;
			}
		}
	}else{// If it is not a special character, the value is put in the current string
		uprintf("%c", input[0]);
		PC_Input[commands_counter][PC_Input_counter++] = (char)input[0];
	}
}
void TextOut(char *str){
	uint8_t length = strlen(str);

#ifdef PUTTY_USART
	HAL_UART_Transmit(&huartx, (uint8_t*)str, length, 0xFFFF);
#endif
#ifdef PUTTY_USB
	if(usb_comm){
		CDC_Transmit_FS((uint8_t*)str, length);
		HAL_Delay(1);
	}
#endif
}

void HexOut(uint8_t data[], uint8_t length){
#ifdef PUTTY_USART
	HAL_UART_Transmit_IT(&huartx, data, length);
#endif
#ifdef PUTTY_USB
	if(usb_comm){
		CDC_Transmit_FS(data, length);
		HAL_Delay(1);
	}
#endif
}



void PuttyInterface_Init(PuttyInterfaceTypeDef* pitd){
	pitd->huart2_Rx_len = 0;
	char * startmessage = "----------PuttyInterface_Init-----------\n\r";
	uprintf(startmessage);
#ifdef PUTTY_USART
	HAL_UART_Receive_IT(&huart2, pitd->rec_buf, 1);
#endif
}

void PuttyInterface_Update(PuttyInterfaceTypeDef* pitd){
	if(pitd->huart2_Rx_len){
#ifdef PUTTY_USB
		usb_comm = true;
#endif
		HandlePcInput((char*)&pitd->small_buf, pitd->huart2_Rx_len, pitd->handle);
		pitd->huart2_Rx_len = 0;
#ifdef PUTTY_USART
		HAL_UART_Receive_IT(&huart2, pitd->rec_buf, 1);
#endif
	}
}
