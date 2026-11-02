#ifndef __SERIAL_H
#define __SERIAL_H

#include "stm32f10x.h"

/* 底层字节发送。串口初始化见 KV260.c（USART1）与 ASRpro.c（USART3） */

void Serial1_SendByte(uint8_t Byte);
void Serial3_SendByte(uint8_t Byte);

#endif
