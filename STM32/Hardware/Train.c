#include "stm32f10x.h"                  // Device header
#include "Train.h"
#include "PWM.h"                        //Finger_ApplyAngle / Finger_Angle(经 HandData.h 引入)

/* 康复训练：按预设的两套操驱动舵机，非阻塞执行；支持录入操
   角度约定：伸直=0°，并拢=180°，90° 为中位 */

#define TRAIN_HOLD_MS			1000	//内置操每步保持 1 秒

#define TRAIN_BUILTIN_SETS		2		//内置套数
#define TRAIN_MAX_REC_SETS		3		//最多再录 3 套
#define TRAIN_MAX_SETS			(TRAIN_BUILTIN_SETS+TRAIN_MAX_REC_SETS)

#define TRAIN_REC_INTERVAL_MS	500		//录入采样间隔 0.5s
#define TRAIN_REC_DURATION_MS	20000	//录入最长 20s
#define TRAIN_MAX_REC_SAMPLES	(TRAIN_REC_DURATION_MS/TRAIN_REC_INTERVAL_MS)	//40

/*录入前的准备提示：张开 -> 合上 -> 再张开，提醒操作者即将开始，走完才真正采样*/
#define TRAIN_CUE_HOLD_MS	1000	//准备提示每步保持 1s
#define TRAIN_CUE_STEPS		3

/*训练评估：把手部(KV260)动作和操的目标角度做对比
  前 2s 按 0.1s 采 20 个点标定手部响应延迟，之后按该延迟对齐采样并累计误差*/
#define TRAIN_EVAL_INTERVAL_MS	100		//评估采样间隔 0.1s
#define TRAIN_EVAL_CALIB_MS		2000	//标定窗口 2s
#define TRAIN_EVAL_CALIB_N		(TRAIN_EVAL_CALIB_MS/TRAIN_EVAL_INTERVAL_MS)	//20 个标定点
#define TRAIN_EVAL_MAX_LAG		(TRAIN_EVAL_CALIB_N/2)	//延迟档数 10(0~9)，相邻两档正好相隔 1s

/*每指平均绝对误差(度)分档，越小越好 -> 效果 3/2/1/0*/
#define TRAIN_EVAL_L3			30		//≤30 最好
#define TRAIN_EVAL_L2			50		//≤50 好
#define TRAIN_EVAL_L1			70		//≤70 良，其余较差

/*一步：五指目标角度 + 保持时间(ms)
  角度成员顺序与 Finger_Angle_t 一致：大拇指,食指,中指,无名指,小指*/
typedef struct
{
	Finger_Angle_t Angle;
	uint16_t HoldMs;
} TrainStep_t;

/*一套操：步骤表 + 整表重复次数*/
typedef struct
{
	const TrainStep_t *Steps;
	uint8_t Count;
	uint8_t Repeat;
} TrainSet_t;

/*第一套(4步×1s，重复5次=20s)：完全伸直 -> 90° -> 完全伸直 -> 完全并拢*/
static const TrainStep_t Set1_Steps[] =
{
	{{0,   0,   0,   0,   0  }, TRAIN_HOLD_MS},		//完全伸直
	{{90,  90,  90,  90,  90 }, TRAIN_HOLD_MS},		//90°
	{{0,   0,   0,   0,   0  }, TRAIN_HOLD_MS},		//完全伸直
	{{180, 180, 180, 180, 180}, TRAIN_HOLD_MS},		//完全并拢
};

/*第二套(20步×1s，重复1次=20s)：逐指张开 -> 逐指握拢 -> 再逐指张开 -> 逆序逐指握拢，每根手指一步*/
static const TrainStep_t Set2_Steps[] =
{
	/*张开：拇指 -> 小指，每次只多伸一根*/
	{{0,   180, 180, 180, 180}, TRAIN_HOLD_MS},		//伸拇指
	{{0,   0,   180, 180, 180}, TRAIN_HOLD_MS},		//伸食指
	{{0,   0,   0,   180, 180}, TRAIN_HOLD_MS},		//伸中指
	{{0,   0,   0,   0,   180}, TRAIN_HOLD_MS},		//伸无名指
	{{0,   0,   0,   0,   0  }, TRAIN_HOLD_MS},		//伸小指，五指全伸
	/*握拢：拇指 -> 小指*/
	{{180, 0,   0,   0,   0  }, TRAIN_HOLD_MS},		//握拇指
	{{180, 180, 0,   0,   0  }, TRAIN_HOLD_MS},		//握食指
	{{180, 180, 180, 0,   0  }, TRAIN_HOLD_MS},		//握中指
	{{180, 180, 180, 180, 0  }, TRAIN_HOLD_MS},		//握无名指
	{{180, 180, 180, 180, 180}, TRAIN_HOLD_MS},		//握小指，回到攥拳
	/*再张开：拇指 -> 小指*/
	{{0,   180, 180, 180, 180}, TRAIN_HOLD_MS},		//伸拇指
	{{0,   0,   180, 180, 180}, TRAIN_HOLD_MS},		//伸食指
	{{0,   0,   0,   180, 180}, TRAIN_HOLD_MS},		//伸中指
	{{0,   0,   0,   0,   180}, TRAIN_HOLD_MS},		//伸无名指
	{{0,   0,   0,   0,   0  }, TRAIN_HOLD_MS},		//伸小指，五指全伸
	/*逆序握拢：小指 -> 拇指*/
	{{0,   0,   0,   0,   180}, TRAIN_HOLD_MS},		//握小指
	{{0,   0,   0,   180, 180}, TRAIN_HOLD_MS},		//握无名指
	{{0,   0,   180, 180, 180}, TRAIN_HOLD_MS},		//握中指
	{{0,   180, 180, 180, 180}, TRAIN_HOLD_MS},		//握食指
	{{180, 180, 180, 180, 180}, TRAIN_HOLD_MS},		//握拇指，回到攥拳
};

/*录入采样点：每套一行，最多 TRAIN_MAX_REC_SAMPLES 个点*/
static TrainStep_t s_RecSteps[TRAIN_MAX_REC_SETS][TRAIN_MAX_REC_SAMPLES];

/*录入前的准备提示姿态：张开(伸直0°) -> 合上(并拢180°) -> 再张开，提醒操作者要开始了*/
static const Finger_Angle_t s_PrepCue[TRAIN_CUE_STEPS] =
{
	{0,   0,   0,   0,   0  },		//张开
	{180, 180, 180, 180, 180},		//合上
	{0,   0,   0,   0,   0  },		//再张开
};

/*中位姿态：正常模式下舵机保持的中立姿势(不跟 KV260)*/
static const Finger_Angle_t s_Neutral = {90,90,90,90,90};

/*运行时套表：前 TRAIN_BUILTIN_SETS 个是内置，后面是录入的*/
static TrainSet_t s_Sets[TRAIN_MAX_SETS];
static uint8_t    s_SetCount;

/*做操运行时状态*/
static volatile uint32_t s_TickMs;		//1ms 递增时基
static uint8_t  s_Busy;					//1=做操进行中
static uint8_t  s_SetIndex;				//当前套(0 基)
static uint8_t  s_StepIndex;			//当前步
static uint8_t  s_RepeatIndex;			//当前第几轮
static uint32_t s_StepStartMs;			//当前步开始时刻
static uint8_t  s_Finished;				//跑完标志，供 Train_TakeFinished 读取

/*录入运行时状态*/
static uint8_t  s_RecActive;			//1=正在录入
static uint8_t  s_RecSlot;				//录到第几个槽(0 基，对应 s_RecSteps)
static uint8_t  s_RecSampleCount;		//已采点数
static uint32_t s_RecStartMs;			//开始时刻
static uint32_t s_RecLastSampleMs;		//上次采样时刻
static uint8_t  s_RecStopReq;			//收到 0x04 置位，下次 Update 收尾

/*录入前准备提示运行时状态*/
static uint8_t  s_CueActive;			//1=准备提示进行中(此时不采样)
static uint8_t  s_CueStep;				//当前提示步
static uint32_t s_CueStartMs;			//当前提示步开始时刻

/*工作模式：正常(默认) / 训练 / 录入*/
static uint8_t  s_Mode;					//取值 TRAIN_MODE_xxx

/*做操评估运行时状态*/
static uint8_t  s_EvalActive;			//1=本次做操正在评估手部动作
static uint32_t s_EvalLastSampleMs;		//上次评估/标定采样时刻
static uint8_t  s_EvalSampleCount;		//标定已采点数
static Finger_Angle_t s_EvalHand[TRAIN_EVAL_CALIB_N];	//标定期手部角度
static Finger_Angle_t s_EvalTarget[TRAIN_EVAL_CALIB_N];	//标定期操目标角度
static uint8_t  s_EvalLag;				//标定得到的响应延迟(0~9 档，每档 0.1s)
static uint8_t  s_EvalStepSampled;		//当前步是否已采样评分
static uint32_t s_EvalErrSum;			//累计绝对误差(五指之和)
static uint16_t s_EvalStepCount;		//已评分步数
static uint8_t  s_LastEffect;			//最近一次训练效果 0~3
static uint8_t  s_LastError;			//最近一次训练误差(每指平均绝对误差,度)，0xFF=还没有

/**
  * 函    数：训练模块初始化
  * 说    明：用 TIM4 建 1ms 时基(TIM2/TIM3 已被舵机 PWM 占用)，并装载内置两套操
  */
void Train_Init(void)
{
	TIM_TimeBaseInitTypeDef TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4,ENABLE);

	TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;
	TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;
	TimeBaseInitStructure.TIM_Period=10-1;			//10kHz 计 10 次 = 1ms
	TimeBaseInitStructure.TIM_Prescaler=7200-1;		//72MHz / 7200 = 10kHz
	TimeBaseInitStructure.TIM_RepetitionCounter=0;
	TIM_InternalClockConfig(TIM4);
	TIM_TimeBaseInit(TIM4,&TimeBaseInitStructure);

	TIM_ITConfig(TIM4,TIM_IT_Update,ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitStructure.NVIC_IRQChannel=TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=2;
	NVIC_Init(&NVIC_InitStructure);

	TIM_Cmd(TIM4,ENABLE);

	/*装载内置两套操，每套都跑满 20s(步长 1s，靠重复次数凑)*/
	s_Sets[0].Steps=Set1_Steps;
	s_Sets[0].Count=(uint8_t)(sizeof(Set1_Steps)/sizeof(Set1_Steps[0]));
	s_Sets[0].Repeat=5;									//4 步 × 1s × 5 = 20s

	s_Sets[1].Steps=Set2_Steps;
	s_Sets[1].Count=(uint8_t)(sizeof(Set2_Steps)/sizeof(Set2_Steps[0]));
	s_Sets[1].Repeat=1;									//20 步 × 1s × 1 = 20s

	s_SetCount=TRAIN_BUILTIN_SETS;
	s_Mode=TRAIN_MODE_NORMAL;							//上电默认正常模式
	s_LastError=0xFF;									//还没有训练结果
}

/*TIM4 更新中断：1ms 递增时基*/
void TIM4_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM4,TIM_IT_Update)==SET)
	{
		s_TickMs++;
		TIM_ClearITPendingBit(TIM4,TIM_IT_Update);
	}
}

uint8_t Train_GetSetCount(void)
{
	return s_SetCount;
}

uint8_t Train_IsBusy(void)
{
	return (uint8_t)(s_Busy||s_CueActive);	//做操/准备提示时都占用舵机，不让它跟 KV260
}

uint8_t Train_TakeFinished(void)
{
	uint8_t Finished=s_Finished;
	s_Finished=0;
	return Finished;
}

void Train_Stop(void)
{
	s_Busy=0;
	s_EvalActive=0;
	s_Mode=TRAIN_MODE_NORMAL;				//中止后回正常模式，舵机回中位
	Finger_ApplyAngle(s_Neutral);
}

uint8_t Train_GetEffect(void)
{
	return s_LastEffect;
}

uint8_t Train_GetError(void)
{
	return s_LastError;
}

uint8_t Train_IsSampling(void)
{
	return s_RecActive;					//准备提示阶段为 0，真正采样才为 1
}

uint8_t Train_GetTarget(Finger_Angle_t *Angle)
{
	if (s_Busy==0)						//只有做操进行中才有"该到的"目标角度
	{
		return 0;
	}
	*Angle=s_Sets[s_SetIndex].Steps[s_StepIndex].Angle;
	return 1;
}

uint32_t Train_GetTick(void)
{
	return s_TickMs;
}

uint8_t Train_GetMode(void)
{
	return s_Mode;
}

/**
  * 函    数：进入训练模式
  * 说    明：收到「开始康复训练」时调用；录入/准备提示中不改
  */
void Train_EnterTrainMode(void)
{
	if (s_RecActive || s_CueActive)
	{
		return;
	}
	s_Mode=TRAIN_MODE_TRAIN;
}

/**
  * 函    数：启动第 SetNo 套操(1 基)
  * 返 回 值：1=启动成功，0=套号越界或正在录入
  * 说    明：会覆盖正在执行的那一套
  */
uint8_t Train_Start(uint8_t SetNo)
{
	if (SetNo<1 || SetNo>s_SetCount)
	{
		return 0;
	}
	if (s_RecActive || s_CueActive)		//录入/准备提示中不许做操
	{
		return 0;
	}

	s_Mode=TRAIN_MODE_TRAIN;				//做操即处于训练模式
	s_SetIndex=(uint8_t)(SetNo-1);
	s_StepIndex=0;
	s_RepeatIndex=0;
	s_Finished=0;
	s_Busy=1;

	/*评估：标定与评分累计清零，延迟默认按步末采样(采满 2s 后覆盖)*/
	s_EvalActive=1;
	s_EvalLastSampleMs=s_TickMs;
	s_EvalSampleCount=0;
	s_EvalLag=(uint8_t)(TRAIN_EVAL_MAX_LAG-1);
	s_EvalStepSampled=0;
	s_EvalErrSum=0;
	s_EvalStepCount=0;

	Finger_ApplyAngle(s_Sets[s_SetIndex].Steps[0].Angle);
	s_StepStartMs=s_TickMs;
	return 1;
}

/**
  * 函    数：开始录入操
  * 说    明：做操中或已录满则忽略；先给准备提示(张开->合上->再张开)，
  *           三步走完才真正开始采样；录入期间舵机跟着手(KV260)走(main 里处理)
  */
void Train_RecordStart(void)
{
	if (s_Busy || s_RecActive || s_CueActive)							//做操中/已在录/提示中
	{
		return;
	}
	if ((uint8_t)(s_SetCount-TRAIN_BUILTIN_SETS)>=TRAIN_MAX_REC_SETS)	//录入套数已满
	{
		return;
	}

	s_Mode=TRAIN_MODE_RECORD;											//进入录入模式
	s_RecSlot=(uint8_t)(s_SetCount-TRAIN_BUILTIN_SETS);					//先占好槽位
	s_RecSampleCount=0;
	s_RecStopReq=0;

	s_CueStep=0;														//启动准备提示
	s_CueActive=1;
	s_CueStartMs=s_TickMs;
	Finger_ApplyAngle(s_PrepCue[0]);
}

/**
  * 函    数：请求结束录入
  * 说    明：只在录入进行中才响应；准备提示阶段收到则忽略，
  *           避免停止命令早到把整次录入打掉。实际收尾在 Train_RecordUpdate 里做
  */
void Train_RecordStop(void)
{
	if (s_RecActive)
	{
		s_RecStopReq=1;
	}
}

/*准备提示推进：每步保持 TRAIN_CUE_HOLD_MS，三步走完后正式开始录入*/
static void Train_CueUpdate(void)
{
	if (s_CueActive==0)
	{
		return;
	}

	if ((uint32_t)(s_TickMs-s_CueStartMs)<TRAIN_CUE_HOLD_MS)			//当前步还没到时间
	{
		return;
	}

	s_CueStep++;
	if (s_CueStep>=TRAIN_CUE_STEPS)										//提示走完
	{
		s_CueActive=0;
		s_RecStartMs=s_TickMs;											//从此开始计时录入
		s_RecLastSampleMs=s_TickMs;
		s_RecActive=1;
		return;															//舵机交回 main 跟 KV260
	}

	Finger_ApplyAngle(s_PrepCue[s_CueStep]);
	s_CueStartMs=s_TickMs;
}

/*录入收尾：把采到的点追加成新的一套，并回正常模式*/
static void Train_RecordFinish(void)
{
	s_RecActive=0;
	s_RecStopReq=0;
	s_Mode=TRAIN_MODE_NORMAL;
	Finger_ApplyAngle(s_Neutral);

	if (s_RecSampleCount>0)
	{
		s_Sets[s_SetCount].Steps=s_RecSteps[s_RecSlot];
		s_Sets[s_SetCount].Count=s_RecSampleCount;
		s_Sets[s_SetCount].Repeat=1;
		s_SetCount++;
	}
	s_RecSampleCount=0;
}

/*录入推进：到点采样；满 20s 或收到停止请求则收尾*/
static void Train_RecordUpdate(void)
{
	if (s_RecActive==0)
	{
		return;
	}

	if (s_RecStopReq || (uint32_t)(s_TickMs-s_RecStartMs)>=TRAIN_REC_DURATION_MS)
	{
		Train_RecordFinish();
		return;
	}

	if ((uint32_t)(s_TickMs-s_RecLastSampleMs)>=TRAIN_REC_INTERVAL_MS)
	{
		s_RecLastSampleMs=s_TickMs;
		if (s_RecSampleCount<TRAIN_MAX_REC_SAMPLES)
		{
			s_RecSteps[s_RecSlot][s_RecSampleCount].Angle=Finger_Angle;	//记录当前舵机角度
			s_RecSteps[s_RecSlot][s_RecSampleCount].HoldMs=TRAIN_REC_INTERVAL_MS;
			s_RecSampleCount++;
		}
	}
}

/*角度差的绝对值*/
static uint16_t AngleAbsDiff(uint8_t a,uint8_t b)
{
	return (a>b)?(uint16_t)(a-b):(uint16_t)(b-a);
}

/*两组手指角度的差异：五指绝对差之和，0~900*/
static uint16_t AngleDist(Finger_Angle_t a,Finger_Angle_t b)
{
	return (uint16_t)(AngleAbsDiff(a.Damuzhi,b.Damuzhi)
	                + AngleAbsDiff(a.Shizhi,b.Shizhi)
	                + AngleAbsDiff(a.Zhongzhi,b.Zhongzhi)
	                + AngleAbsDiff(a.Wumingzhi,b.Wumingzhi)
	                + AngleAbsDiff(a.Xiaomuzhi,b.Xiaomuzhi));
}

/**
  * 函    数：标定收尾，求手部响应延迟
  * 说    明：在 0~TRAIN_EVAL_MAX_LAG-1 档里找延迟：第 i 点最接近操当前目标，
  *           第 i+10 点最接近 1 秒后(相邻一步)的操目标，取两者误差和最小的一档
  */
static void Train_EvalComputeLag(void)
{
	uint8_t i;
	uint8_t Best=0;
	uint32_t BestErr=0xFFFFFFFFUL;

	for (i=0;i<TRAIN_EVAL_MAX_LAG;i++)
	{
		uint32_t Err=AngleDist(s_EvalHand[i],s_EvalTarget[i])
		           +AngleDist(s_EvalHand[i+TRAIN_EVAL_MAX_LAG],s_EvalTarget[i+TRAIN_EVAL_MAX_LAG]);
		if (Err<BestErr)
		{
			BestErr=Err;
			Best=i;
		}
	}
	s_EvalLag=Best;
}

/*标定采样：每 0.1s 记一个手部/目标点，采满 20 点后算延迟*/
static void Train_EvalCalibSample(const TrainStep_t *Step)
{
	if (s_EvalSampleCount>=TRAIN_EVAL_CALIB_N)
	{
		return;
	}

	s_EvalHand[s_EvalSampleCount]=Finger_Angle;
	s_EvalTarget[s_EvalSampleCount]=Step->Angle;
	s_EvalSampleCount++;

	if (s_EvalSampleCount>=TRAIN_EVAL_CALIB_N)
	{
		Train_EvalComputeLag();
	}
}

/**
  * 函    数：评分采样
  * 说    明：按延迟对齐取一次手部角度与当前步目标比，累计误差；
  *           Force=1 用于步末补采(步长不足延迟时)
  */
static void Train_EvalStepCapture(const TrainStep_t *Step,uint8_t Force)
{
	uint32_t Delay;

	if (s_EvalActive==0 || s_EvalStepSampled || s_EvalSampleCount<TRAIN_EVAL_CALIB_N)
	{
		return;
	}

	Delay=(uint32_t)s_EvalLag*TRAIN_EVAL_INTERVAL_MS;
	if (Delay>Step->HoldMs)					//步长不够时退到步末
	{
		Delay=Step->HoldMs;
	}
	if (Force==0 && (uint32_t)(s_TickMs-s_StepStartMs)<Delay)
	{
		return;
	}

	s_EvalErrSum+=AngleDist(Finger_Angle,Step->Angle);
	s_EvalStepCount++;
	s_EvalStepSampled=1;
}

/*评估推进：先做标定采样，再做当前步的评分采样*/
static void Train_EvalUpdate(const TrainStep_t *Step)
{
	if (s_EvalActive==0)
	{
		return;
	}

	if (s_EvalSampleCount<TRAIN_EVAL_CALIB_N
	    && (uint32_t)(s_TickMs-s_EvalLastSampleMs)>=TRAIN_EVAL_INTERVAL_MS)
	{
		s_EvalLastSampleMs=s_TickMs;
		Train_EvalCalibSample(Step);
	}

	Train_EvalStepCapture(Step,0);
}

/*评估收尾：按每指平均误差定效果等级 0~3*/
static void Train_EvalFinish(void)
{
	uint32_t AvgErr;

	if (s_EvalActive==0)
	{
		return;
	}
	s_EvalActive=0;

	if (s_EvalStepCount==0)					//没采到有效评分点
	{
		s_LastEffect=0;
		s_LastError=0xFF;
		return;
	}

	AvgErr=s_EvalErrSum/s_EvalStepCount/5;	//每指平均绝对误差(度)
	s_LastError=(AvgErr>180)?180:(uint8_t)AvgErr;
	if (AvgErr<=TRAIN_EVAL_L3)       { s_LastEffect=3; }
	else if (AvgErr<=TRAIN_EVAL_L2)  { s_LastEffect=2; }
	else if (AvgErr<=TRAIN_EVAL_L1)  { s_LastEffect=1; }
	else                             { s_LastEffect=0; }
}

/**
  * 函    数：非阻塞推进
  * 说    明：主循环反复调用。推进做操到点切下一步；处理录入采样与收尾
  */
void Train_Update(void)
{
	const TrainSet_t *Set;
	const TrainStep_t *Step;

	Train_CueUpdate();						//录入前准备提示推进
	Train_RecordUpdate();					//录入采样/收尾

	if (s_Busy==0)
	{
		return;
	}

	Set=&s_Sets[s_SetIndex];
	Step=&Set->Steps[s_StepIndex];

	Train_EvalUpdate(Step);									//标定/评分采样

	if ((uint32_t)(s_TickMs-s_StepStartMs)<Step->HoldMs)	//当前步还没到时间
	{
		return;
	}

	Train_EvalStepCapture(Step,1);							//步末补采一次，避免漏评

	s_StepIndex++;											//切下一步
	if (s_StepIndex>=Set->Count)							//一轮走完
	{
		s_StepIndex=0;
		s_RepeatIndex++;
		if (s_RepeatIndex>=Set->Repeat)						//整套做完
		{
			s_Busy=0;
			s_Finished=1;
			Train_EvalFinish();
			s_Mode=TRAIN_MODE_NORMAL;						//做完回正常模式
			Finger_ApplyAngle(s_Neutral);
			return;
		}
	}
	s_EvalStepSampled=0;									//新的一步重新评分采样

	Finger_ApplyAngle(Set->Steps[s_StepIndex].Angle);
	s_StepStartMs=s_TickMs;
}
