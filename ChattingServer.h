#pragma once
#include "stdafx.h"
#include "CNetWorkLibrary(MemoryPool).h"
class ChattingServer : public joshua::NetworkLibraryWan
{
	typedef struct st_SECTOR_POS
	{
		int iX;
		int iY;
	} SECTORPOS;

	struct st_PLAYER
	{
		UINT64 PlayerID;
		UINT64 SessionID;
		WCHAR Nick[20];
		SECTORPOS OldSector;
		SECTORPOS CurrentSector;
		UINT64 bigTime;
	};

private:
	std::unordered_map< UINT64, st_PLAYER> m_PlayerMap;

public:
	void OnClientJoin(SOCKADDR_IN* sockAddr, UINT64 sessionID);
	void OnClientLeave(UINT64 sessionID);

	bool OnConnectionRequest(SOCKADDR_IN* sockAddr);

	void OnRecv(UINT64 sessionID, CMessage* message);

	void OnSend(UINT64 sessionID, int sendsize);

	void OnError(int errorcode, WCHAR*);

};