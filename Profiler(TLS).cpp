#include "stdafx.h"
#include "Profiler(TLS).h"

// TLS인덱스
DWORD _dwTLSIndex;

// 프로파일링 배열
THREAD_NODE ProfileArray[e_DEF_PROFILER::e_MAX_THREAD_COUNT];

// 해상도 변수
LARGE_INTEGER			_IFrequency;
<<<<<<< Updated upstream


void InitialProfiler()
{
	QueryPerformanceFrequency(&_IFrequency);
=======
__int64 _dMicFrequency;

void InitialProfiler()
{
	QueryPerformanceFrequency(&_IFrequency); 
	_dMicFrequency = _IFrequency.QuadPart / 1000000;
>>>>>>> Stashed changes

	_dwTLSIndex = TlsAlloc();
	if (_dwTLSIndex == TLS_OUT_OF_INDEXES)
		exit(-1);

	for (int i = 0; i < e_MAX_THREAD_COUNT; i++)
	{
		ProfileArray[i].dwThreadID = -1;
		ProfileArray[i].pNode = nullptr;
	}
}

bool ProfilingBegin(WCHAR* szName)
{
	LARGE_INTEGER	liStartTime;
	NODE* targetNode;

	GetNode(szName, &targetNode);
	if (targetNode == nullptr)
		return false;

	// 이 함수가 대상에 대해서 중복으로 호출되었을 때
	if (targetNode->lStartTime.QuadPart != 0)
		return false;

	QueryPerformanceCounter(&liStartTime);

	targetNode->lStartTime = liStartTime;

	return true;

}

bool GetNode(WCHAR* szName, NODE** outNode)
{
	NODE* targetNode;

	targetNode = (NODE*)TlsGetValue(_dwTLSIndex);
	if (targetNode == nullptr)
	{
		// 메모리 할당
		targetNode = new NODE[e_MAX_NODE_COUNT];

		// 메모리 초기화
		for (int i = 0; i < e_MAX_NODE_COUNT; i++)
		{
			memset(&targetNode[i], 0, sizeof(NODE));
		}

		// 현재 스레드 TLS에 값 세팅
		if (FALSE == TlsSetValue(_dwTLSIndex, targetNode))
			return false;

		// NODE 배열을 관리하는 ProfileArray에 세팅
		for (int i = 0; i < e_MAX_THREAD_COUNT; i++)
		{
			if ((ProfileArray[i].dwThreadID) == -1 && (ProfileArray[i].pNode == nullptr))
			{
				ProfileArray[i].dwThreadID = GetCurrentThreadId();
				ProfileArray[i].pNode = targetNode;
				break;
			}
		}
	}
	else
	{
		for (int i = 0; i < e_MAX_NODE_COUNT; i++)
		{
			// Node배열에 이미 존재한다면
			if ((targetNode[i].bIsUse == TRUE) && (wcscmp(targetNode[i].szName, szName) == 0))
			{
				*outNode = &targetNode[i];
				break;
			}

			// Node배열에 존재하지 않는다면 세팅
			if ((targetNode[i].bIsUse == FALSE) && (wcscmp(targetNode[i].szName, L"") == 0))
			{
				targetNode[i].bIsUse = TRUE;

				targetNode[i].iCall = 0;

				for (int j = 0; j < 2; j++)
				{
					targetNode[i].iMax[j] = 0;
				}

				for (int j = 0; j < 2; j++)
				{
					targetNode[i].iMin[j] = INT64_MAX;
				}

				targetNode[i].iTotalTime = 0;

				StringCchPrintf(targetNode[i].szName, wcslen(szName) + 1, L"%s", szName);

				*outNode = &targetNode[i];
				break;

			}
		}
	}

	return true;
}

bool ProfilingEnd(WCHAR* szName)
{
	LARGE_INTEGER liEndTime;
	DWORD dwIndex;
	NODE* targetNode;
<<<<<<< Updated upstream
=======
	LARGE_INTEGER lPlayTime;
	QueryPerformanceCounter(&liEndTime);
>>>>>>> Stashed changes

	GetNode(szName, &targetNode);
	if (targetNode == nullptr)
		return false;

<<<<<<< Updated upstream
	QueryPerformanceCounter(&liEndTime);

	__int64 PlayTime = (liEndTime.QuadPart - targetNode->lStartTime.QuadPart);

	if (targetNode->iMax[1] < PlayTime)
	{
		targetNode->iMax[0] = targetNode->iMax[1];
		targetNode->iMax[1] = PlayTime;
	}

	if (targetNode->iMin[1] > PlayTime)
	{
		targetNode->iMin[0] = targetNode->iMin[1];
		targetNode->iMin[1] = PlayTime;
	}

	targetNode->iCall++;
	targetNode->iTotalTime += PlayTime;
=======

	lPlayTime.QuadPart = (liEndTime.QuadPart - targetNode->lStartTime.QuadPart);

	__int64 Time = lPlayTime.QuadPart / _dMicFrequency;

	if (targetNode->iMax[1] < Time)
	{
		targetNode->iMax[0] = targetNode->iMax[1];
		targetNode->iMax[1] = Time;
	}

	if (targetNode->iMin[1] > Time)
	{
		targetNode->iMin[0] = targetNode->iMin[1];
		targetNode->iMin[1] = Time;
	}

	targetNode->iCall++;
	targetNode->iTotalTime += Time;
>>>>>>> Stashed changes
	targetNode->lStartTime.QuadPart = 0;

	return true;
}

bool ProfilePrint(void)
{
	SYSTEMTIME		stNowTime;
	WCHAR			fileName[256];
	DWORD			dwBytesWritten;
	WCHAR			wBuffer[200];
	HANDLE			hFile;

	GetLocalTime(&stNowTime);
	StringCchPrintf(fileName,
		sizeof(fileName),
		L"Profiling %4d-%02d-%02d %02d.%02d.txt",
		stNowTime.wYear,
		stNowTime.wMonth,
		stNowTime.wDay,
		stNowTime.wHour,
		stNowTime.wMinute
	);

	hFile = CreateFile(fileName,
		GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_DELETE | FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	for (int i = 0; i < e_MAX_THREAD_COUNT; i++)
	{
		if ((ProfileArray[i].dwThreadID) != -1 && (ProfileArray[i].pNode != nullptr))
		{
			WriteFile(hFile, L"-------------------------------------------------------------------------------------------------------------\r\n", 111 * sizeof(WCHAR), &dwBytesWritten, NULL);
			WriteFile(hFile, L"﻿ ThreadID |                Name  |           Average  |            Min   |            Max   |          Call |\r\n", 112 * sizeof(WCHAR), &dwBytesWritten, NULL);
			WriteFile(hFile, L"-------------------------------------------------------------------------------------------------------------\r\n", 111 * sizeof(WCHAR), &dwBytesWritten, NULL);

			for (int j = 0; j < e_MAX_NODE_COUNT; j++)
			{
<<<<<<< Updated upstream
				if (ProfileArray[i].pNode[j].bIsUse == true)
				{
					LONG64 average = ProfileArray[i].pNode[j].iTotalTime;

					for (int t = 0; t < 2; t++)
					{
						average -= ProfileArray[i].pNode[j].iMin[t];
						average -= ProfileArray[i].pNode[j].iMax[t];
					}


					average /= (ProfileArray[i].pNode[j].iCall - 4);

					StringCchPrintf(wBuffer,
						sizeof(wBuffer),
						L" %9d  | %20s | %16.4lld ms | %14.4lldms | %14.4lld ms | %13d |\r\n",
						ProfileArray[i].dwThreadID,
						ProfileArray[i].pNode[j].szName,
						(average * 1000000) / _IFrequency.QuadPart,
						(ProfileArray[i].pNode[j].iMin[1] * 1000000) / _IFrequency.QuadPart,
						(ProfileArray[i].pNode[j].iMax[1] * 1000000) / _IFrequency.QuadPart,
=======
				if (ProfileArray[i].pNode[j].bIsUse == TRUE)
				{
					__int64 average = ProfileArray[i].pNode[j].iTotalTime;



					average -= ProfileArray[i].pNode[j].iMin[1];
					average -= ProfileArray[i].pNode[j].iMax[1];



					average /= (ProfileArray[i].pNode[j].iCall - 2);

					StringCchPrintf(wBuffer,
						sizeof(wBuffer),
						L" %9d  | %20s | %8.4lld ms | %8.4lldms | %8.4lld ms | %13d |\r\n",
						ProfileArray[i].dwThreadID,
						ProfileArray[i].pNode[j].szName,
						average,
						/*ProfileArray[i].pNode[j].iTotalTime*/
						(ProfileArray[i].pNode[j].iMin[1]),
						(ProfileArray[i].pNode[j].iMax[1]),
>>>>>>> Stashed changes
						ProfileArray[i].pNode[j].iCall
					);
					::WriteFile(hFile, wBuffer, wcslen(wBuffer) * sizeof(WCHAR), &dwBytesWritten, NULL);
				}
				else
					break;
			}
			::WriteFile(hFile,
				L"-------------------------------------------------------------------------------------------------------------\r\n",
				111 * sizeof(WCHAR), &dwBytesWritten, NULL);
		}
	}

	for (int i = 0; i < e_MAX_THREAD_COUNT; i++)
	{
		if ((ProfileArray[i].dwThreadID) != -1 && (ProfileArray[i].pNode != nullptr))
		{
			delete[] ProfileArray[i].pNode;
		}
	}
	return true;
}
