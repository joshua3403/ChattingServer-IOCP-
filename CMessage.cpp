#include "stdafx.h"
#include "CMessage.h"

CLFFreeList_TLS<CMessage> CMessage::g_PacketPool;

void CMessage::PutData(char* data, int size)
{
	int realSize = GetFreeSize() - size;
	if (realSize < 0)
	{
		while (realSize < 0)
		{
			IncreaseBufferSize(size);
			realSize = GetFreeSize() - size;
		}
	}

	realSize = size;
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)data, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;
}

void CMessage::GetData(char* data, int size)
{
	int realSize = GetDataSize() - size;
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> SHORT ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = size;
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;
	memcpy(data, ptr, size);
	m_iUsingSize -= realSize;
	m_iFront += realSize;
}

void CMessage::SetLanMessageHeader(char* header, int len)
{
	memcpy(m_cpHeadPtr + 3, &header, len);
}

void CMessage::SetEncodingCode()
{
		if (m_bIsEncoded)
			return;

		char* payLoadPtr = GetBufferPtr();
		char* headerPtr = GetWanHeaderPtr();
		char randKey = m_bRandKey;
		char key = FIXKEY;
		short len = GetDataSize();

		unsigned char checkSum = 0;
		for (int i = 0; i < len; i++)
		{
			checkSum += payLoadPtr[i];
		}
		checkSum = checkSum % 256;

		char test = 0;
		char test1 = 0;
		for (int i = 0; i < len; i++)
		{
			test = payLoadPtr[i] ^ (test + randKey + i + 1);
			payLoadPtr[i] = test ^ (test1 + key + i + 1);
			test1 = payLoadPtr[i];
		}
		st_PACKET_HEADER* header = (st_PACKET_HEADER*)GetWanHeaderPtr();
		header->byCode = FIXKEY;
		header->byCheckSum = checkSum;
		header->wLen = len;
		header->byRandKey = randKey;
}

BOOL CMessage::SetDecodingCode()
{
	if(!m_bIsEncoded)
		return FALSE;

	st_PACKET_HEADER* packetHeader = (st_PACKET_HEADER*)m_cpHeadPtr;

	char randKey = packetHeader->byRandKey;
	char fixKey = FIXKEY;
	int dataSize = GetDataSize();
	char* pPayload = GetBufferPtr();

	// pHeader의 랜덤키, 체크섬, Playload 복호화
	packetHeader->byRandKey ^= fixKey;
	packetHeader->byCheckSum ^= fixKey;
	for (int i = 0; i < dataSize; i++)
	{
		pPayload[i] ^= fixKey;
	}

	// 체크섬 계산
	int iCheck = 0;
	for (int i = 0; i < dataSize; i++)
	{
		iCheck += pPayload[i];
	}

	char cCheck = (char)iCheck % 256;
	if (packetHeader->byCheckSum != cCheck)
		return FALSE;
	return TRUE;

}

void CMessage::IncreaseBufferSize(int size)
{
	int realSize = GetFreeSize() - size;
	char newBuffer[1000] = { 0 };
	while (realSize < 0)
	{
		memcpy(newBuffer, m_cpPayloadBuffer, m_iMaxSize);
		m_iMaxSize += size;
		free(m_cpPayloadBuffer);
		m_cpPayloadBuffer = (char*)malloc(sizeof(char) * (m_iMaxSize));
		memcpy(m_cpPayloadBuffer, newBuffer, m_iMaxSize);
		realSize = GetFreeSize() - size;
	}
}

CMessage& CMessage::operator=(CMessage& clSrCMessage)
{
	// TODO: 여기에 return 문을 삽입합니다.
	if (this == &clSrCMessage)
		return *this;

	free(m_cpPayloadBuffer);
	m_cpPayloadBuffer = (char*)malloc(((clSrCMessage).GetBufferSize()));
	memcpy(m_cpPayloadBuffer, (clSrCMessage).GetBufferPtr(), sizeof(m_cpPayloadBuffer));
	m_iFront = (clSrCMessage).GetFront();
	m_iRear = (clSrCMessage).GetRear();
	m_iMaxSize = (clSrCMessage).GetBufferSize();
	m_iUsingSize = (clSrCMessage).GetDataSize();

	return *this;
}

CMessage& CMessage::operator<<(BYTE byValue)
{
	int realSize = GetFreeSize() - sizeof(byValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_BYTE);
		realSize = sizeof(byValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&byValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		//// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		//CExceptClass* Except = new CExceptClass(L"<< BYTE ERROR", m_cpBuffer, m_iMaxSize);
		//throw Except;
	}

	realSize = sizeof(byValue);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&byValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator<<(char chValue)
{
	int realSize = GetFreeSize() - sizeof(chValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_BYTE);
		realSize = sizeof(chValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&chValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		//// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		//CExceptClass* Except = new CExceptClass(L"<< CHAR ERROR", m_cpBuffer, m_iMaxSize);
		//throw Except;
	}

	realSize = sizeof(chValue);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&chValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator<<(short shValue)
{
	int realSize = GetFreeSize() - sizeof(shValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_SHORT);
		realSize = sizeof(shValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&shValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		//// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		//CExceptClass* Except = new CExceptClass(L"<< SHORT ERROR", m_cpBuffer, m_iMaxSize);
		//throw Except;
	}

	realSize = sizeof(shValue);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&shValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator<<(WORD wValue)
{
	int realSize = GetFreeSize() - sizeof(wValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_SHORT);
		realSize = sizeof(wValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&wValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		//// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		//CExceptClass* Except = new CExceptClass(L"<< WORD ERROR", m_cpBuffer, m_iMaxSize);
		//throw Except;
	}

	realSize = sizeof(wValue);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&wValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator<<(int iValue)
{
	int realSize = GetFreeSize() - sizeof(iValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_INT);
		realSize = sizeof(iValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&iValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		//// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		//CExceptClass* Except = new CExceptClass(L"<< INT ERROR", m_cpBuffer, m_iMaxSize);
		//throw Except;
	}

	realSize = sizeof(iValue);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&iValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator<<(DWORD dwValue)
{
	int realSize = GetFreeSize() - sizeof(dwValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_INT);
		realSize = sizeof(dwValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&dwValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		//// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		//CExceptClass* Except = new CExceptClass(L"<< DWORD ERROR", m_cpBuffer, m_iMaxSize);
		//throw Except;
		return *this;
	}

	realSize = sizeof(dwValue);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&dwValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator<<(float fValue)
{
	int realSize = GetFreeSize() - sizeof(fValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_INT);
		realSize = sizeof(fValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&fValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		//// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		//CExceptClass* Except = new CExceptClass(L"<< FLOAT ERROR", m_cpBuffer, m_iMaxSize);
		//throw Except;
		return *this;
	}

	realSize = sizeof(fValue);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&fValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator<<(__int64 iValue)
{
	int realSize = GetFreeSize() - sizeof(iValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_INT64);
		realSize = sizeof(iValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&iValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		//// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		//CExceptClass* Except = new CExceptClass(L"<< INT64 ERROR", m_cpBuffer, m_iMaxSize);
		//throw Except;
		return *this;
	}

	realSize = sizeof(iValue);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&iValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator<<(double dValue)
{
	int realSize = GetFreeSize() - sizeof(dValue);
	// 남은 공간보다 삽입하고자 하는 크기가 더 크면 남아있는 크기만큼 삽입
	if (realSize < 0)
	{
		IncreaseBufferSize(eBUFFER_UPSCALE_INT64);
		realSize = sizeof(dValue);

		char* ptr = m_cpPayloadBuffer;
		ptr += m_iRear;
		memcpy(ptr, (char*)&dValue, realSize);
		m_iUsingSize += realSize;
		m_iRear += realSize;

		// TODO: 여기에 메모리를 다시 늘리고 예외 객체 throw
		CExceptClass* Except = new CExceptClass(L"<< DOUBLE ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(double);

	char* ptr = m_cpPayloadBuffer;
	ptr += m_iRear;
	memcpy(ptr, (char*)&dValue, realSize);
	m_iUsingSize += realSize;
	m_iRear += realSize;

	return *this;
}

CMessage& CMessage::operator>>(BYTE& byValue)
{
	int realSize = GetDataSize() - sizeof(byValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> BYTE ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(BYTE);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&byValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}

CMessage& CMessage::operator>>(char& chValue)
{
	int realSize = GetDataSize() - sizeof(chValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> CHAR ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(char);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&chValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}

CMessage& CMessage::operator>>(short& shValue)
{
	int realSize = GetDataSize() - sizeof(shValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> SHORT ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(short);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&shValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}

CMessage& CMessage::operator>>(WORD& wValue)
{
	int realSize = GetDataSize() - sizeof(wValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> WORD ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(WORD);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&wValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}



CMessage& CMessage::operator>>(int& iValue)
{
	int realSize = GetDataSize() - sizeof(iValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> INT ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(int);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&iValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}

CMessage& CMessage::operator>>(DWORD& dwValue)
{
	int realSize = GetDataSize() - sizeof(dwValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> DWORD ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(DWORD);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&dwValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}

CMessage& CMessage::operator>>(float& fValue)
{
	int realSize = GetDataSize() - sizeof(fValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> FLOAT ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(float);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&fValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}

CMessage& CMessage::operator>>(__int64& iValue)
{
	int realSize = GetDataSize() - sizeof(iValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> INT64 ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(__int64);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&iValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}

CMessage& CMessage::operator>>(double& dValue)
{
	int realSize = GetDataSize() - sizeof(dValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> DOUBLE ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(double);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&dValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}

CMessage& CMessage::operator>>(UINT64& dValue)
{
	int realSize = GetDataSize() - sizeof(dValue);
	if (realSize < 0)
	{
		CExceptClass* Except = new CExceptClass(L">> DOUBLE ERROR", m_cpPayloadBuffer, m_iMaxSize);
		throw Except;
	}

	realSize = sizeof(dValue);
	char* ptr = m_cpPayloadBuffer;
	ptr += m_iFront;

	memcpy(&dValue, ptr, realSize);
	m_iUsingSize -= realSize;
	m_iFront += realSize;

	return *this;
}
