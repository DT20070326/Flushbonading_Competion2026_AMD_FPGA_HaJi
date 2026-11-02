#include "stm32f10x.h"                  // Device header
#include "PWM.h"

/* 舵机脉宽范围(us)，对应 0 度 ~ 180 度 */
#define SERVO_MIN_US    500
#define SERVO_MAX_US    2500
/* 1 个定时器计数 = 10us（72MHz / 720 = 100kHz，周期 2000 计数 = 20ms = 50Hz） */
#define SERVO_TICK_US   10

void Finger_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TimeBaseInitStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);

	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;
	TimeBaseInitStructure.TIM_Period=2000-1;
	TimeBaseInitStructure.TIM_Prescaler=720-1;
	TimeBaseInitStructure.TIM_RepetitionCounter=0;

	TIM_InternalClockConfig(TIM2);
	TIM_TimeBaseInit(TIM2,&TimeBaseInitStructure);

	TIM_InternalClockConfig(TIM3);
	TIM_TimeBaseInit(TIM3,&TimeBaseInitStructure);

	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode=TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OCPolarity=TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState=TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse=0;

	TIM_OC1Init(TIM2,&TIM_OCInitStructure);
	TIM_OC2Init(TIM2,&TIM_OCInitStructure);
	TIM_OC3Init(TIM2,&TIM_OCInitStructure);
	TIM_OC4Init(TIM2,&TIM_OCInitStructure);

	TIM_OC1Init(TIM3,&TIM_OCInitStructure);

	TIM_Cmd(TIM2,ENABLE);
	TIM_Cmd(TIM3,ENABLE);
}

void PWM_SetCompare_PA0(uint16_t Compare)
{
	TIM_SetCompare1(TIM2,Compare);
}

void PWM_SetCompare_PA1(uint16_t Compare)
{
	TIM_SetCompare2(TIM2,Compare);
}

void PWM_SetCompare_PA2(uint16_t Compare)
{
	TIM_SetCompare3(TIM2,Compare);
}

void PWM_SetCompare_PA3(uint16_t Compare)
{
	TIM_SetCompare4(TIM2,Compare);
}

void PWM_SetCompare_PA6(uint16_t Compare)
{
	TIM_SetCompare1(TIM3,Compare);
}

/* 角度(0~180) 换算成比较值：0 度 -> 500us，180 度 -> 2500us */
static uint16_t AngleToCompare(uint16_t Angle)
{
	if (Angle > 180)
	{
		Angle = 180;
	}
	return (uint16_t)((SERVO_MIN_US + (uint32_t)Angle * (SERVO_MAX_US - SERVO_MIN_US) / 180) / SERVO_TICK_US);
}

/* 食指、中指、无名指的舵机与大拇指、小指的安装方向相反，
   同样的手指角度要输出 180-angle 才能让手指朝同一个方向动 */

static void Servo_Xiaomuzhi(uint16_t angle)
{
	PWM_SetCompare_PA0(AngleToCompare(angle));
}

static void Servo_Wumingzhi(uint16_t angle)
{
	PWM_SetCompare_PA1(AngleToCompare(180-angle));
}

static void Servo_Zhongzhi(uint16_t angle)
{
	PWM_SetCompare_PA2(AngleToCompare(180-angle));
}

static void Servo_Shizhi(uint16_t angle)
{
	PWM_SetCompare_PA3(AngleToCompare(180-angle));
}

static void Servo_Damuzhi(uint16_t angle)
{
	PWM_SetCompare_PA6(AngleToCompare(angle));
}

const Servo_TypeDef SetAngle =
{
	Servo_Xiaomuzhi,
	Servo_Wumingzhi,
	Servo_Zhongzhi,
	Servo_Shizhi,
	Servo_Damuzhi
};

/**
  * 函    数：数据包转手指角度
  * 参    数：Packet KV260 数据包，输入参数
  * 返 回 值：五个手指的目标角度，范围：0~180
  * 说    明：数据包中超出 0~180 的字段视为无效，该手指取中位 90；
  *           还没收到过有效帧（帧头不是 0xFF）时，五个手指都取中位 90
  */
Finger_Angle_t Finger_Convert(KV260_Packet_t Packet)
{
	Finger_Angle_t Angle;

	Angle.Damuzhi=90;
	Angle.Shizhi=90;
	Angle.Zhongzhi=90;
	Angle.Wumingzhi=90;
	Angle.Xiaomuzhi=90;

	if (Packet.Header!=0xFF)						//还没收到过有效帧
	{
		return Angle;
	}

	if (Packet.D<=180) { Angle.Damuzhi   = Packet.D; }
	if (Packet.S<=180) { Angle.Shizhi    = Packet.S; }
	if (Packet.Z<=180) { Angle.Zhongzhi  = Packet.Z; }
	if (Packet.W<=180) { Angle.Wumingzhi = Packet.W; }
	if (Packet.X<=180) { Angle.Xiaomuzhi = Packet.X; }

	return Angle;
}

/**
  * 函    数：把手指角度输出到五个舵机
  * 参    数：Angle 五个手指的角度，范围：0~180
  * 返 回 值：无
  */
void Finger_ApplyAngle(Finger_Angle_t Angle)
{
	SetAngle.Damuzhi(Angle.Damuzhi);
	SetAngle.Shizhi(Angle.Shizhi);
	SetAngle.Zhongzhi(Angle.Zhongzhi);
	SetAngle.Wumingzhi(Angle.Wumingzhi);
	SetAngle.Xiaomuzhi(Angle.Xiaomuzhi);
}
