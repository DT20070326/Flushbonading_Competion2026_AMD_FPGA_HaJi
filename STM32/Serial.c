#include "stm32f10x.h"                  // Device header
#include "Serial.h"

/* 底层字节发送。串口初始化见 KV260.c（USART1）与 ASRpro.c（USART3） */

void Serial1_SendByte(uint8_t Byte)
{
	USART_SendData(USART1,Byte);
	while(USART_GetFlagStatus(USART1,USART_FLAG_TXE)==RESET);
}

void Serial3_SendByte(uint8_t Byte)
{
	USART_SendData(USART3,Byte);
	while(USART_GetFlagStatus(USART3,USART_FLAG_TXE)==RESET);
}
