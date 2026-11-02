#include "stm32f10x.h"                  // Device header
#include "KV260.h"

/* USART1: PA9 = TX, PA10 = RX */

/*接收缓存参数*/
#define KV260_RX_BUFFER_SIZE		64		//接收缓存的大小，单位字节

/*接收缓存由中断写入、由主循环读出，使用环形结构，读写位置相同即表示缓存为空*/
static uint8_t KV260_RxBuffer[KV260_RX_BUFFER_SIZE];
static volatile uint16_t KV260_RxWrite;
static volatile uint16_t KV260_RxRead;

/**
  * 函    数：KV260 串口初始化
  * 参    数：无
  * 返 回 值：无
  * 说    明：使用USART1，引脚为PA9（TX）和PA10（RX），波特率9600，开启接收中断
  */
void KV260_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);

	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_10;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate=9600;
	USART_InitStructure.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode=USART_Mode_Tx|USART_Mode_Rx;
	USART_InitStructure.USART_Parity=USART_Parity_No;
	USART_InitStructure.USART_StopBits=USART_StopBits_1;
	USART_InitStructure.USART_WordLength=USART_WordLength_8b;
	USART_Init(USART1,&USART_InitStructure);

	USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitStructure.NVIC_IRQChannel=USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=1;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART1,ENABLE);
}

/**
  * 函    数：USART1中断函数
  * 参    数：无
  * 返 回 值：无
  * 说    明：此函数无需在主程序中调用，每接收到一个字节，硬件自动执行一次
  *           只把字节存入接收缓存，不做解析
  */
void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1,USART_IT_RXNE)==SET)
	{
		uint16_t Next=(KV260_RxWrite+1)%KV260_RX_BUFFER_SIZE;		//计算写位置的下一个位置

		if (Next!=KV260_RxRead)									//缓存未满才写入，满了则丢弃这个字节
		{
			KV260_RxBuffer[KV260_RxWrite]=(uint8_t)USART_ReceiveData(USART1);
			KV260_RxWrite=Next;
		}

		USART_ClearITPendingBit(USART1,USART_IT_RXNE);
	}
}

/**
  * 函    数：从接收缓存中读取一个字节
  * 参    数：Byte 读取得到的一个字节数据，输出参数
  * 返 回 值：1表示成功读取到一个字节，0表示接收缓存中没有数据
  * 说    明：此函数需在主循环中反复调用，读取中断接收到的数据
  */
static uint8_t KV260_ReadByte(uint8_t *Byte)
{
	if (KV260_RxRead==KV260_RxWrite)								//读位置和写位置相同，说明接收缓存为空
	{
		return 0;
	}

	*Byte=KV260_RxBuffer[KV260_RxRead];
	KV260_RxRead=(KV260_RxRead+1)%KV260_RX_BUFFER_SIZE;

	return 1;
}

/**
  * 函    数：接收一个数据包
  * 参    数：无
  * 返 回 值：最近一次收到的完整数据帧
  * 说    明：此函数需在主循环中反复调用。完整数据帧为 sizeof(KV260_Packet_t) 字节，
  *           以 0xFF 开头、0x59 结尾，帧尾不对则整帧丢弃；
  *           还没收到过完整帧时，返回的帧头不是 0xFF
  */
KV260_Packet_t KV260_Receive(void)
{
	static uint8_t State;
	static uint8_t Index;
	static KV260_Packet_t Buffer;
	static KV260_Packet_t Packet;					//最近一次收到的完整帧

	uint8_t Byte;

	while (KV260_ReadByte(&Byte)==1)
	{
		if (State==0)								//状态0：等待帧头 0xFF
		{
			if (Byte==0xFF)
			{
				((uint8_t *)&Buffer)[0]=Byte;		//帧头存入字节0
				Index=1;
				State=1;
			}
		}
		else if (State==1)							//状态1：收取中间的字节
		{
			((uint8_t *)&Buffer)[Index]=Byte;
			Index++;
			if (Index>=(uint8_t)(sizeof(KV260_Packet_t)-1))
			{
				State=2;
			}
		}
		else										//状态2：收到帧尾字节，校验通过才收下
		{
			((uint8_t *)&Buffer)[Index]=Byte;
			State=0;
			if (Byte==0x59)
			{
				Packet=Buffer;
			}
		}
	}

	return Packet;
}
