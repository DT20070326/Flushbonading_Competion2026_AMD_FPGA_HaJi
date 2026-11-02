#ifndef __ASRPRO_H
#define __ASRPRO_H

#include "stm32f10x.h"

/* USART3: PB10 = TX, PB11 = RX */

/* ASRpro 语音模块通过 USART3 发来的指令帧，固定 3 字节：0xFF 指令 0xFE
   指令与角度的对应：
   FF 01 FE -> 五指转到 100 度
   FF 02 FE -> 五指转到 140 度
   FF 03 FE -> 五指转到 140 度
   FF 04 FE -> 苹果，五指转到 110 度                                    */
#define ASR_FRAME_HEADER	0xFF
#define ASR_FRAME_TAIL		0xFE

#define ASR_CMD_100DEG		0x01
#define ASR_CMD_140DEG_A	0x02
#define ASR_CMD_140DEG_B	0x03
#define ASR_CMD_APPLE		0x04

void ASRpro_Init(void);

/* 主循环反复调用。收到一条完整且校验通过的指令帧返回 1，并写出指令字节；
   没有新指令返回 0。缓存里有多条时只返回最后一条 */
uint8_t ASRpro_Receive(uint8_t *Command);

/* 指令字节换算成五指角度：认识的指令返回 1 并写出 0~180 的角度，
   不认识的指令返回 0 */
uint8_t ASRpro_CommandToAngle(uint8_t Command,uint8_t *Angle);

#endif
