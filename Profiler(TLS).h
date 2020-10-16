#pragma once
#include "stdafx.h"

enum e_DEF_PROFILER
{
	e_MAX_NAME_LEN = 64,
	e_MAX_NODE_COUNT = 100,
	e_MAX_THREAD_COUNT = 50
};


typedef struct st_NODE
{
	BOOL			bIsUse;
	WCHAR			szName[e_MAX_NAME_LEN];			// 프로파일 샘플 이름.

	LARGE_INTEGER	lStartTime;			// 프로파일 샘플 실행 시간.

	__int64			iTotalTime;			// 전체 사용시간 카운터 Time.	(출력시 호출회수로 나누어 평균 구함)
	__int64			iMin[2];			// 최소 사용시간 카운터 Time.	(초단위로 계산하여 저장 / [0] 가장최소 [1] 다음 최소 [2])
	__int64			iMax[2];			// 최대 사용시간 카운터 Time.	(초단위로 계산하여 저장 / [0] 가장최대 [1] 다음 최대 [2])
	
	unsigned long			iCall;				// 누적 호출 횟수
}NODE;

// st_NODE를 관리할 구조체
typedef struct st_THREAD_NODE
{
	DWORD dwThreadID;
	NODE* pNode;
}THREAD_NODE;

// 초기화 함수
void InitialProfiler();

// 프로파일링 시작 함수
bool ProfilingBegin(WCHAR* szName);

// 프로파일링 노드를 구하는 함수
bool GetNode(WCHAR* szName, NODE** outNode);

bool ProfilingEnd(WCHAR* szName);

bool ProfilePrint(void);

#define PRO_BEGIN(X) ProfilingBegin(X)
#define PRO_END(X) ProfilingEnd(X)