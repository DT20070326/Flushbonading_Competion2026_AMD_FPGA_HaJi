#include "stm32f10x.h"                  // Device header
#include "ASRpro.h"

/* USART3: PB10 = TX, PB11 = RX */

/*接收缓存参数*/
#define ASR_RX_BUFFER_SIZE		32		//接收缓存的大小，单位字节

/*接收缓存由中断写入、由主循环读出，使用环形结构，读写位置相同即表示缓存为空*/
static uint8_t ASR_RxBuffer[ASR_RX_BUFFER_SIZE];
static volatile uint16_t ASR_RxWrite;
static volatile uint16_t ASR_RxRead;

/**
  * 函    数：ASRpro 语音串口初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用USART3，引脚为PB10（TX）和PB11（RX），波特率9600，开启接收中断
  */
void ASRpro_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);

	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_11;
	GPIO_Init(GPIOB,&GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate=115200;
	USART_InitStructure.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode=USART_Mode_Tx|USART_Mode_Rx;
	USART_InitStructure.USART_Parity=USART_Parity_No;
	USART_InitStructure.USART_StopBits=USART_StopBits_1;
	USART_InitStructure.USART_WordLength=USART_WordLength_8b;

	USART_Init(USART3,&USART_InitStructure);

	USART_ITConfig(USART3,USART_IT_RXNE,ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitStructure.NVIC_IRQChannel=USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=2;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART3,ENABLE);
}

/**
  * 函    数：USART3中断函数
  * 参    数：无
  * 返 回 值：无
  * 说    明：此函数无需在主程序中调用，每接收到一个字节，硬件自动执行一次
  *           只把字节存入接收缓存，不做解析
  */
void USART3_IRQHandler(void)
{
	if (USART_GetITStatus(USART3,USART_IT_RXNE)==SET)
	{
		uint16_t Next=(ASR_RxWrite+1)%ASR_RX_BUFFER_SIZE;		//计算写位置的下一个位置

		if (Next!=ASR_RxRead)									//缓存未满才写入，满了则丢弃这个字节
		{
			ASR_RxBuffer[ASR_RxWrite]=(uint8_t)USART_ReceiveData(USART3);
			ASR_RxWrite=Next;
		}

		USART_ClearITPendingBit(USART3,USART_IT_RXNE);
	}
}

/**
  * 函    数：从接收缓存中读取一个字节
  * 参    数：Byte 读取得到的一个字节数据，输出参数
  * 返 回 值：1表示成功读取到一个字节，0表示接收缓存中没有数据
  * 说    明：此函数需在主循环中反复调用，读取中断接收到的数据
  */
static uint8_t ASR_ReadByte(uint8_t *Byte)
{
	if (ASR_RxRead==ASR_RxWrite)								//读位置和写位置相同，说明接收缓存为空
	{
		return 0;
	}

	*Byte=ASR_RxBuffer[ASR_RxRead];
	ASR_RxRead=(ASR_RxRead+1)%ASR_RX_BUFFER_SIZE;

	return 1;
}

/**
  * 函    数：接收一条语音指令
  * 参    数：Command 收到的指令字节，输出参数
  * 返 回 值：1表示收到一条完整且校验通过的指令帧，0表示没有新指令
  * 说    明：此函数需在主循环中反复调用。指令帧固定 3 字节：0xFF 指令 0xFE，
  *           帧头或帧尾不对则丢弃；等待帧尾时又收到 0xFF 则按新帧头重新同步
  */
uint8_t ASRpro_Receive(uint8_t *Command)
{
	static uint8_t State;							//0=等帧头，1=等指令，2=等帧尾
	static uint8_t Buffer;							//暂存指令字节
	uint8_t Byte;
	uint8_t Got=0;

	while (ASR_ReadByte(&Byte)==1)
	{
		if (State==0)								//状态0：等待帧头 0xFF
		{
			if (Byte==ASR_FRAME_HEADER)
			{
				State=1;
			}
		}
		else if (State==1)							//状态1：收到指令字节
		{
			if (Byte==ASR_FRAME_HEADER)				//连续两个帧头，保持等待指令
			{
				State=1;
			}
			else
			{
				Buffer=Byte;
				State=2;
			}
		}
		else										//状态2：等待帧尾 0xFE
		{
			if (Byte==ASR_FRAME_TAIL)
			{
				*Command=Buffer;
				Got=1;
			}
			State=0;

			if (Byte==ASR_FRAME_HEADER)				//这一字节同时也可能是下一帧的帧头
			{
				State=1;
			}
		}
	}

	return Got;
}

/**
  * 函    数：语音指令换算成五指角度
  * 参    数：Command 指令字节，输入参数
  *            Angle 五指目标角度(0~180)，输出参数
  * 返 回 值：1表示是指令表里的指令，0表示不认识的指令
  * 说    明：FF 01 FE -> 100 度；FF 02 FE、FF 03 FE -> 140 度；FF 04 FE(苹果) -> 110 度
  */
uint8_t ASRpro_CommandToAngle(uint8_t Command,uint8_t *Angle)
{
	switch (Command)
	{
		case ASR_CMD_100DEG:
			*Angle=100;
			return 1;
		case ASR_CMD_140DEG_A:
		case ASR_CMD_140DEG_B:
			*Angle=140;
			return 1;
		case ASR_CMD_APPLE:
			*Angle=110;
			return 1;
		default:
			return 0;
	}
}
