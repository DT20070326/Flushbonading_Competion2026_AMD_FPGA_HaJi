#ifndef __TRAIN_H
#define __TRAIN_H

#include "stm32f10x.h"
#include "HandData.h"                /* Finger_Angle_t，用于上报目标值 */

/* 康复训练：按预设的两套操驱动舵机，非阻塞执行；支持录入操
   角度约定：伸直=0°，并拢=180°，90° 为中位 */

/* 工作模式：默认正常模式(舵机不跟 KV260，保持中位)
   训练模式做操并与手部动作比较；录入模式跟手录操 */
#define TRAIN_MODE_NORMAL   0
#define TRAIN_MODE_TRAIN    1
#define TRAIN_MODE_RECORD   2

void    Train_Init(void);            /* 建立 1ms 时基、初始化套表，上电调用一次 */
uint8_t Train_GetSetCount(void);     /* 当前有几套操(内置 2 + 已录) */
uint8_t Train_GetMode(void);         /* 当前工作模式 TRAIN_MODE_xxx */
void    Train_EnterTrainMode(void);  /* 进入训练模式(收到「开始康复训练」)；录入/提示中不改 */
uint8_t Train_Start(uint8_t SetNo);  /* 启动第 SetNo 套(1 基)，越界/录入中返回 0 */
void    Train_Update(void);          /* 主循环反复调用：非阻塞推进做操、录入采样、评估手部动作 */
uint8_t Train_IsBusy(void);          /* 1=做操或准备提示进行中(此时舵机不跟 KV260) */
uint8_t Train_TakeFinished(void);    /* 刚做完一套返回 1 并清标志，供上报效果 */
uint8_t Train_GetEffect(void);       /* 最近一次做完一套的效果 0~3(Train_TakeFinished 后读) */
uint8_t Train_GetError(void);        /* 最近一次做完一套的误差(每指平均绝对误差,度)；0xFF=还没有 */
void    Train_Stop(void);            /* 中止当前做操，回正常模式 */

uint8_t Train_IsSampling(void);                 /* 1=录入正在真正采样(准备提示已走完) */
uint8_t Train_GetTarget(Finger_Angle_t *Angle); /* 做操进行中返回 1 并写出本步目标角度；否则 0 */
uint32_t Train_GetTick(void);                   /* 1ms 时基，供上层定时上报用 */

void    Train_RecordStart(void);     /* 开始录入(收到 0x03)：先准备提示(张开->合上->再张开)再采样；做操中/已录满则忽略 */
void    Train_RecordStop(void);      /* 请求结束录入(收到 0x04)；只在录入进行中有效，提示阶段忽略 */

#endif
