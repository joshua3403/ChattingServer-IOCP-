#pragma once
#include "stdafx.h"
#include "CCrashDumpClass.h"
#include "CLog.h"
#include "MemoryPool(LockFree).h"
#include "Profiler(TLS).h"

#define MAXDUMPCOUNT 3000
#define CHECKCODE 0x0000000019921107

template<class DATA>
class CLFFreeList_TLS
{
private:
	class CDataDump;
	struct __declspec(align(64))st_DataDump_Node
	{
		CDataDump* pDataDump;
<<<<<<< Updated upstream
=======
		__declspec(align(64))
>>>>>>> Stashed changes
		DATA Data;
	};

public:
	CLFFreeList_TLS(int count,BOOL bPlacementNew = false);
	virtual ~CLFFreeList_TLS();
	DATA* Alloc();
	bool Free(DATA* data);

	int GetUseCount() { return m_dwUseCount; }
	int GetAllocCount() { return m_pDataDump->GetAllocCount(); }

private:
	inline CDataDump* DataDumpAlloc()
	{
		//PRO_BEGIN(_T("MemoryPool Alloc()"));
		CDataDump* DataDump = m_pDataDump->Alloc();
		//PRO_END(_T("MemoryPool Alloc()"));

		if (!TlsSetValue(m_dwTlsIndex, DataDump))
			return nullptr;
<<<<<<< Updated upstream

		new (DataDump)CDataDump();

		DataDump->m_pFreeList = this;

		InterlockedIncrement(&m_dwUseCount);
=======

		//new (DataDump)CDataDump();

		DataDump->m_pFreeList = this;

		//InterlockedIncrement(&m_dwUseCount);
		//m_dwUseCount++;
>>>>>>> Stashed changes

		return DataDump;
	}

private:
	DWORD m_dwTlsIndex;

	DWORD m_dwUseCount;

	CLFFreeList<CDataDump>* m_pDataDump;	

private:

	class CDataDump
	{
	public:
<<<<<<< Updated upstream
		CDataDump() : m_dwNodeCount(MAXDUMPCOUNT), m_dwFreeCount(MAXDUMPCOUNT), m_dwUsingCount(0) {};
		virtual ~CDataDump() { delete[] m_pDataDumpNode; };
=======
		CDataDump() : m_dwNodeCount(MAXDUMPCOUNT - 1), m_dwFreeCount(MAXDUMPCOUNT - 1), m_dwUsingCount(0) 
		{
			for (int i = 0; i < MAXDUMPCOUNT; i++)
			{
				m_pDataDumpNode[i].pDataDump = this;
			}
		};
		virtual ~CDataDump() { };
>>>>>>> Stashed changes
		DATA* Alloc();
		bool Free();
	private:
		friend class CLFFreeList_TLS<DATA>;
		CLFFreeList_TLS<DATA>* m_pFreeList;
		st_DataDump_Node m_pDataDumpNode[MAXDUMPCOUNT];
		// 노드 덤프 내의 Data 개수
		LONG m_dwNodeCount;
		// 총 데이터의 개수 중에서 할당된 개수
		LONG m_dwUsingCount;
		// 총 데이터의 개수 중에서 남은 개수
		LONG m_dwFreeCount;

	};
};

template<class DATA>
inline DATA* CLFFreeList_TLS<DATA>::CDataDump::Alloc()
{
<<<<<<< Updated upstream
	m_dwNodeCount--;
	m_pDataDumpNode[m_dwNodeCount].pDataDump = this;
	
	if (0 ==  m_dwNodeCount)
		m_pFreeList->DataDumpAlloc();

	return &m_pDataDumpNode[m_dwNodeCount].Data;
}
=======


	return data;
}


>>>>>>> Stashed changes


template<class DATA>
inline bool CLFFreeList_TLS<DATA>::CDataDump::Free()
{
<<<<<<< Updated upstream
	if (!(--m_dwFreeCount))
	{
=======
	if (!(m_dwFreeCount))
	{
		m_dwUsingCount = 0;
		m_dwNodeCount = MAXDUMPCOUNT - 1;
		m_dwFreeCount = MAXDUMPCOUNT - 1;
>>>>>>> Stashed changes
		m_pFreeList->m_pDataDump->Free(this);

		return true;
	}
	return false;
}

template<class DATA>
CLFFreeList_TLS<DATA>::CLFFreeList_TLS(int count,BOOL bPlacementNew)
{
	m_dwTlsIndex = TlsAlloc();
	if (m_dwTlsIndex == TLS_OUT_OF_INDEXES)
		CRASH();

	m_pDataDump = new CLFFreeList<CDataDump>(count, false);
	m_dwUseCount = 0;
}

template<class DATA>
CLFFreeList_TLS<DATA>::~CLFFreeList_TLS()
{
	TlsFree(m_dwTlsIndex);
}

template<class DATA>
inline DATA* CLFFreeList_TLS<DATA>::Alloc()
{
	CDataDump* DataDump = (CDataDump*)TlsGetValue(m_dwTlsIndex);
	//PRO_BEGIN(_T("DataDumpAlloc()"));
	if (DataDump == nullptr)
	{
		DataDump = DataDumpAlloc();
	}
<<<<<<< Updated upstream
	InterlockedIncrement(&m_dwUseCount);	 

	return DataDump->Alloc();
=======
	//PRO_END(_T("DataDumpAlloc()"));
	InterlockedIncrement(&m_dwUseCount);	
	//m_dwUseCount++;

	//PRO_BEGIN(_T("Alloc()"));
	DATA* data = &DataDump->m_pDataDumpNode[DataDump->m_dwNodeCount].Data;

	if (-1 == (--DataDump->m_dwNodeCount))
		DataDump->m_pFreeList->DataDumpAlloc();
	//PRO_END(_T("Alloc()"));
	return data;
>>>>>>> Stashed changes
}

template<class DATA>
inline bool CLFFreeList_TLS<DATA>::Free(DATA* data)
{
<<<<<<< Updated upstream
	st_DataDump_Node* DataDump = (st_DataDump_Node*)((char*)data - sizeof(st_DataDump_Node::pDataDump));

	InterlockedDecrement(&m_dwUseCount);
	
	return DataDump->pDataDump->Free();
=======
	st_DataDump_Node* DataDump = (st_DataDump_Node*)((char*)data - 64);

	InterlockedDecrement(&m_dwUseCount);
	
	if ((--DataDump->pDataDump->m_dwFreeCount) == -1)
	{
		DataDump->pDataDump->m_dwUsingCount = 0;
		DataDump->pDataDump->m_dwNodeCount = MAXDUMPCOUNT - 1;
		DataDump->pDataDump->m_dwFreeCount = MAXDUMPCOUNT - 1;
		m_pDataDump->Free(DataDump->pDataDump);
		return true;
	}
	return false;
>>>>>>> Stashed changes
}




