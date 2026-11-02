#ifndef __HAND_DATA_H
#define __HAND_DATA_H

#include <stdint.h>

/*接收数据包*********************/

/**
  * 说    明：KV260 通过 USART1 发来的数据包，固定 14 字节
  *           数据包格式： 0xFF 'D'+值 'S'+值 'Z'+值 'W'+值 'X'+值 'Y'+值 0x59
  *           标签与手指的对应：D 大拇指，S 食指，Z 中指，W 无名指，X 小指
  *           值均为 1 字节，范围 0~180，故用 uint8_t（int8_t 最大 127，装不下）
  *           注意：结构体成员顺序即为字节顺序，全部为 1 字节，
  *                 sizeof(KV260_Packet_t) 即为 14，不会产生填充字节
  */
typedef struct
{
	uint8_t Header;		//字节0，帧头，规定为 0xFF
	uint8_t TagD;		//字节1，标签D，规定为 'D'，标识后面的字节为大拇指角度
	uint8_t D;			//字节2，大拇指角度，0~180
	uint8_t TagS;		//字节3，标签S，规定为 'S'，标识后面的字节为食指角度
	uint8_t S;			//字节4，食指角度，0~180
	uint8_t TagZ;		//字节5，标签Z，规定为 'Z'，标识后面的字节为中指角度
	uint8_t Z;			//字节6，中指角度，0~180
	uint8_t TagW;		//字节7，标签W，规定为 'W'，标识后面的字节为无名指角度
	uint8_t W;			//字节8，无名指角度，0~180
	uint8_t TagX;		//字节9，标签X，规定为 'X'，标识后面的字节为小指角度
	uint8_t X;			//字节10，小指角度，0~180
	uint8_t TagY;		//字节11，标签Y，规定为 'Y'，标识后面的字节为预留值
	uint8_t Y;			//字节12，预留，暂无对应舵机
	uint8_t Tail;		//字节13，帧尾，规定为 0x59
} KV260_Packet_t;

/*********************接收数据包*/


/*手指角度*********************/

/**
  * 说    明：由 KV260 数据包校验后得到的目标角度，范围：0~180
  *           超出 0~180 的字段不写入，保留上一次的角度
  *           成员顺序与 Finger_ApplyAngle 中的输出顺序一致
  */
typedef struct
{
	uint8_t Damuzhi;		//大拇指，输出 PA6（TIM3_CH1）
	uint8_t Shizhi;			//食指，输出 PA3（TIM2_CH4）
	uint8_t Zhongzhi;		//中指，输出 PA2（TIM2_CH3）
	uint8_t Wumingzhi;		//无名指，输出 PA1（TIM2_CH2）
	uint8_t Xiaomuzhi;		//小指，输出 PA0（TIM2_CH1）
} Finger_Angle_t;

/*********************手指角度*/

#endif
