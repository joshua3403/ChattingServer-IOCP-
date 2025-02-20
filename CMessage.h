#pragma once
#include "stdafx.h"
#include "CExceptClass.h"
#include "CMemoryPool(LockFree)_TLS.h"

#define dfFIX_KEY 0x32
#define dfPACKET_CODE 0x77
#define dfMESSAGE_HEADER_LEN 5



#pragma pack(push, 1) 
struct st_PACKET_HEADER
{
	BYTE byCode;
	WORD wLen;
	BYTE byRandKey;
	BYTE byCheckSum;
};
#pragma pack(pop)

class CMessage
{
	/*---------------------------------------------------------------
	Packet Enum.

	----------------------------------------------------------------*/
	enum en_PACKET
	{
		eBUFFER_DEFAULT = 300,		// 패킷의 기본 버퍼 사이즈.
		eBUFFER_UPSCALE_BYTE = 1,
		eBUFFER_UPSCALE_SHORT = 2,
		eBUFFER_UPSCALE_INT = 4,
		eBUFFER_UPSCALE_INT64 = 8,
		eBUFFER_HEADER_SIZE = 5
	};



private:
	int m_iMaxSize;
	int m_iFront;
	int m_iRear;
	int m_iUsingSize;
	char* m_cpPayloadBuffer;
	char* m_cpHeadPtr;
	LONG m_lRefCount;
	friend class CLFFreeList_TLS<CMessage>;
	CRITICAL_SECTION m_CS;

	BYTE m_bRandKey;

	BOOL m_bIsEncoded;

public:
	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CMessage()
	{
		m_iMaxSize = en_PACKET::eBUFFER_DEFAULT;
		m_iFront = m_iRear = m_iUsingSize = m_lRefCount = 0;
		m_cpHeadPtr = (char*)malloc(sizeof(char) * m_iMaxSize);
		ZeroMemory(m_cpHeadPtr, sizeof(char) * m_iMaxSize);
		m_cpPayloadBuffer = m_cpHeadPtr + 5;
		InitializeCriticalSection(&m_CS);
		m_bIsEncoded = false;
	}

	virtual	~CMessage()
	{
		Release();
	}

	//////////////////////////////////////////////////////////////////////////
	// 패킷  파괴.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	Release(void)
	{
		if (m_cpPayloadBuffer != nullptr)
		{
			free(m_cpHeadPtr);
		}
	}

public:
	static CLFFreeList<CMessage> g_PacketPool;

public:

	void AddRef()
	{
		InterlockedIncrement(&m_lRefCount);
	}

	void SubRef()
	{
		if (InterlockedDecrement(&m_lRefCount) == 0)
		{
			g_PacketPool.Free(this);
		}
	}

	static CMessage* Alloc()
	{
		CMessage* newMessage = g_PacketPool.Alloc();
		newMessage->Clear();
		newMessage->AddRef();
		return newMessage;
	}
	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	Clear(void)
	{
		m_iFront = m_iRear = m_iUsingSize = m_lRefCount = 0;
		m_bIsEncoded = FALSE;
	}

	void ClearPayload(void)
	{
		ZeroMemory(m_cpPayloadBuffer, eBUFFER_DEFAULT - 5);
	}

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	int	GetBufferSize(void) { return m_iMaxSize; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	GetDataSize(void) { return m_iUsingSize; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용 가능한 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int) 사용 가능한 데이터 사이즈
	//////////////////////////////////////////////////////////////////////////
	int GetFreeSize(void) { return m_iMaxSize - m_iUsingSize; }

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	char* GetBufferPtr(void) { return  m_cpHeadPtr + 5; }

	char* GetLanHeaderPtr(void) { return m_cpHeadPtr + 3; }

	char* GetWanHeaderPtr(void) { return m_cpHeadPtr; }


	//////////////////////////////////////////////////////////////////////////
	// Front위치 얻기
	//
	// Parameters: 없음.
	// Return: (int) 현재 Front의 위치
	//////////////////////////////////////////////////////////////////////////
	int GetFront(void) { return m_iFront; }

	//////////////////////////////////////////////////////////////////////////
	// Rear위치 얻기
	//
	// Parameters: 없음.
	// Return: (int) 현재 Rear의 위치
	//////////////////////////////////////////////////////////////////////////
	int GetRear(void) { return m_iRear; }

	void PutData(char* data, int size);

	void GetData(char* data, int size);

	static int GetPacketUsingSize()
	{
		return g_PacketPool.GetUseCount();
	}

	void SetLanMessageHeader(char* header, int len);

	void SetWanMessageHeader(char* header, int len);

	void SetEncodingCode();

	BOOL SetDecodingCode();

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		MoveWritePos(int iSize)
	{
		int temp = m_iRear + iSize;
		if (temp < m_iMaxSize)
		{
			m_iRear += iSize;
			temp = iSize;
			m_iUsingSize += iSize;
		}
		else if (temp == m_iMaxSize)
		{
			m_iRear = m_iMaxSize;
			temp = iSize;
			m_iUsingSize = m_iMaxSize;
		}
		else
		{
			temp = temp - m_iMaxSize;
			m_iRear = m_iMaxSize;
			m_iUsingSize = m_iMaxSize;
		}
		return temp;
	}

	int		MoveReadPos(int iSize)
	{
		int temp = m_iFront + iSize;
		if (temp <= m_iRear)
		{
			m_iFront += iSize;
			m_iUsingSize -= iSize;
			temp = iSize;
		}
		else if (temp > m_iRear)
		{
			temp = m_iRear - m_iFront;
			m_iUsingSize -= temp;
		}
		return temp;
	}

	// 버퍼 사이즈가 작으면 필요한 만큼 키우는 함수
	void IncreaseBufferSize(int size);

	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	CMessage& operator = (CMessage& clSrCMessage);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CMessage& operator << (BYTE byValue);
	CMessage& operator << (char chValue);

	CMessage& operator << (short shValue);
	CMessage& operator << (WORD wValue);

	CMessage& operator << (int iValue);
	CMessage& operator << (DWORD dwValue);
	CMessage& operator << (float fValue);

	CMessage& operator << (__int64 iValue);
	CMessage& operator << (double dValue);

	//////////////////////////////////////////////////////////////////////////
// 빼기.	각 변수 타입마다 모두 만듬.
//////////////////////////////////////////////////////////////////////////
	CMessage& operator >> (BYTE& byValue);
	CMessage& operator >> (char& chValue);

	CMessage& operator >> (short& shValue);
	CMessage& operator >> (WORD& wValue);


	CMessage& operator >> (int& iValue);
	CMessage& operator >> (DWORD& dwValue);
	CMessage& operator >> (float& fValue);

	CMessage& operator >> (__int64& iValue);
	CMessage& operator >> (double& dValue);
	CMessage& operator >> (UINT64& dValue);
};


