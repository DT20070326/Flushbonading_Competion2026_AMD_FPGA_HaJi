#include "stm32f10x.h"                  // Device header
#include "PWM.h"
#include "ASRpro.h"

Finger_Angle_t Finger_Angle={0,0,0,0,0};

int main(void)
{
	uint8_t Command;
	uint8_t VoiceAngle;

	Finger_Init();
	ASRpro_Init();

	Finger_ApplyAngle(Finger_Angle);		//上电先到中位 90 度

	while(1)
	{
		if (ASRpro_Receive(&Command)==1 && ASRpro_CommandToAngle(Command,&VoiceAngle)==1)
		{
			Finger_Angle.Damuzhi=VoiceAngle;		//语音指令：五指同时转到该角度
			Finger_Angle.Shizhi=VoiceAngle;
			Finger_Angle.Zhongzhi=VoiceAngle;
			Finger_Angle.Wumingzhi=VoiceAngle;
			Finger_Angle.Xiaomuzhi=VoiceAngle;
			Finger_ApplyAngle(Finger_Angle);
		}
	}
}
