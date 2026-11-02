#ifndef __KV260_H
#define __KV260_H

#include "stm32f10x.h"
#include "HandData.h"

/* USART1: PA9 = TX, PA10 = RX */

void KV260_Init(void);
KV260_Packet_t KV260_Receive(void);

#endif
