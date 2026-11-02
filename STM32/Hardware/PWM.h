#ifndef __PWM_H
#define __PWM_H

#include "stm32f10x.h"
#include "HandData.h"

void Finger_Init(void);
void PWM_SetCompare_PA0(uint16_t Compare);
void PWM_SetCompare_PA1(uint16_t Compare);
void PWM_SetCompare_PA2(uint16_t Compare);
void PWM_SetCompare_PA3(uint16_t Compare);
void PWM_SetCompare_PA6(uint16_t Compare);

/* 舵机角度接口，用法： SetAngle.Shizhi(90);  表示食指转到 90 度 */
typedef struct
{
	void (*Xiaomuzhi)(uint16_t angle);      /* PA0 */
	void (*Wumingzhi)(uint16_t angle);      /* PA1 */
	void (*Zhongzhi)(uint16_t angle);       /* PA2 */
	void (*Shizhi)(uint16_t angle);         /* PA3 */
	void (*Damuzhi)(uint16_t angle);        /* PA6 */
} Servo_TypeDef;

extern const Servo_TypeDef SetAngle;

/* 五个手指的目标角度，上电默认为中位 90 度 */
extern Finger_Angle_t Finger_Angle;

Finger_Angle_t Finger_Convert(KV260_Packet_t Packet);
void Finger_ApplyAngle(Finger_Angle_t Angle);

#endif
