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

	typedef struct st_SECTOR_AROUND
	{
		int iCount;
		SECTORPOS Around[9];
	}SECTORAROUND;

	typedef struct st_PLAYER
	{
		UINT64 SessionID;
		INT64 iAccountNo;
		BOOL bLogined;
		BOOL bDisconnected;
		WCHAR Nick[dfNiCK_LEN];
		WCHAR ID[dfID_LEN];
		char SessionKey[dfSESSIONKEY_LEN];
		SECTORPOS CurrentSector;
		UINT64 recvTime;
		WCHAR SendBuffer[1024];
	}PLAYER;

	enum en_MSG_TYPE
	{
		en_MSG_CONNECTION = 0,
		en_MSG_PACKET,
		en_MSG_DISCONNECTION,
	};

	typedef struct st_MSG
	{
		en_MSG_TYPE eType;
		UINT64 SessionID;
		CMessage* pPacket;
	}MSG;

private:
	CLFFreeList<PLAYER> m_PlayerPool;
	CLFFreeList<MSG> m_MSGPool;

	BOOL m_bShutdown;

	std::unordered_map<UINT64, PLAYER*> m_PlayerMap;

	// 전체 섹터와 섹터에 존재하는 클라이언트를 리스트로 관리할 자료구조
	std::unordered_map<UINT64, PLAYER*> m_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

	CQueue<MSG*> m_MsgQ;

	HANDLE m_hEvent;

	LONG64 m_lUpdateTps;
	LONG64 m_lPlayerCountTps;


private:

	// MSG header에 따른 동작
	BOOL CreatePlayer(UINT64 sessionID);
	BOOL SectorUpdate(PLAYER* player, WORD SecX, WORD SecY);
	BOOL DeletePlayer(UINT64 sessionID);

	// CMessage type에 따른 동작
	BOOL Packet_Proc_REQ_Login(UINT64 sessionID, CMessage* message);
	BOOL Packet_Proc_REQ_SectorMove(UINT64 sessionID, CMessage* message);
	BOOL Packet_Proc_REQ_Chat(UINT64 sessionID, CMessage* message);
	BOOL Packet_Proc_REQ_HEARBEAT(UINT64 sessionID);


	CMessage* Packet_Proc_RES_Login(INT64 acconutNo, BYTE status);
	CMessage* Packet_Proc_RES_SectorMove(INT64 acconutNo, short SecX, short SecY);
	CMessage* Packet_Proc_RES_Chat(PLAYER* player, WCHAR* message, WORD len);

	MSG* Make_Message_Create_Client(UINT64 sessionID);
	MSG* Make_Message_Packet(UINT64 sessionID, CMessage* message);
	MSG* Make_Message_Disconnect_Client(UINT64 sessionID);

	static unsigned int WINAPI Monitoring_Thread(LPVOID lpParam);
	static unsigned int WINAPI Update_Thread(LPVOID lpParam);

	void Monitoring_Update();
	void Update_Thread();

	PLAYER* FindPlayer(UINT64 SessionID);
	void InsertPlayer(UINT64 sessionID, PLAYER* player);
	void ErasePlayer(UINT64 sessionID);
	void InsertPlayerInSector(PLAYER* player, WORD SecX, WORD SecY);
	void ErasePlayerInSector(UINT64 sessionID, WORD SecX, WORD SecY);
	void GetSectorAround(int iSectorX, int iSectorY, SECTORAROUND* pSectorAround);


	BOOL CompleteMsg(MSG* msg);
	BOOL CompletePacket(UINT64 sessionID, CMessage* message);

	// 패킷 보내는 함수
	void SendPacket_Unicast(UINT64 sessionID, CMessage* message);
	void SendPacket_Around(PLAYER* player, CMessage* message, BOOL bSendMe);
	void SendPacket_SectorOne(int x, int y, CMessage* message, PLAYER* player = nullptr);

public:

	ChattingServer();

	void Start(DWORD port, BOOL nagle, const WCHAR* ip = nullptr, DWORD threadCount = 0, __int64 MaxClient = 0);

	void OnClientJoin(SOCKADDR_IN* sockAddr, UINT64 sessionID);
	void OnClientLeave(UINT64 sessionID);

	bool OnConnectionRequest(SOCKADDR_IN* sockAddr);

	void OnRecv(UINT64 sessionID, CMessage* message);

	void OnSend(UINT64 sessionID, int sendsize);

	void OnError(int errorcode, WCHAR*);

};