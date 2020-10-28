#include "ChattingServer.h"
#include "stdafx.h"

BOOL ChattingServer::CreatePlayer(UINT64 sessionID)
{
	PLAYER* newPlayer = m_PlayerPool.Alloc();
	if (newPlayer == nullptr)
		return FALSE;

	newPlayer->bLogined = FALSE;
	newPlayer->recvTime = timeGetTime();

	memset(newPlayer->Nick, 0, dfID_LEN);
	memset(newPlayer->ID, 0, dfNiCK_LEN);
	memset(newPlayer->SessionKey, 0, dfSESSIONKEY_LEN);

	newPlayer->CurrentSector.iX = -1;
	newPlayer->CurrentSector.iY = -1;
	newPlayer->SessionID = 0;
	newPlayer->iAccountNo = 0;
	newPlayer->bDisconnected = FALSE;
	InsertPlayer(sessionID, newPlayer);

	return TRUE;
}

BOOL ChattingServer::SectorUpdate(PLAYER* player, WORD SecX, WORD SecY)
{
	// 유효하지 않은 섹터
	if (SecX < 0  || SecX >= 50 )
		return FALSE;
	if (SecY < 0 || SecY >= 50)
		return FALSE;
	// 변경할 필요 없음
	if ((player->CurrentSector.iX == SecX) && (player->CurrentSector.iY == SecY))
		return TRUE;

	if((player->CurrentSector.iX != -1) && (player->CurrentSector.iY != -1))
		ErasePlayerInSector(player->SessionID, player->CurrentSector.iX, player->CurrentSector.iY);

	player->CurrentSector.iX = SecX;
	player->CurrentSector.iY = SecY;

	InsertPlayerInSector(player, SecX, SecY);

	return TRUE;
}

BOOL ChattingServer::DeletePlayer(UINT64 sessionID)
{
	PLAYER* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		return TRUE;
	}
	else
	{
		if (player->bDisconnected == FALSE)
		{
			if (player->CurrentSector.iX != -1 && player->CurrentSector.iY != -1)
				ErasePlayerInSector(player->SessionID, player->CurrentSector.iX, player->CurrentSector.iY);
			
			player->iAccountNo = 0;
			ErasePlayer(sessionID);
			m_AccountNoMap.erase(player->iAccountNo);
			m_PlayerPool.Free(player);

			InterlockedDecrement64(&m_lPlayerCountTps);
			return TRUE;
		}
	}
	return TRUE;
}

BOOL ChattingServer::Packet_Proc_REQ_Login(UINT64 sessionID, CMessage* message)
{
	BOOL result = TRUE;
	PLAYER* player = FindPlayer(sessionID);
	CMessage* pPacket = nullptr;
	// 상태
	BYTE status = 1;
	INT64 iAccountNo = 0;
	(*message) >> iAccountNo;

	// OnclientJoin()때 생성되지 않은 Player에 대해서 Packet이 온다면
	if (player == nullptr)
	{
		LOG(L"SERVER", LOG_ERROR, L"REQ_Login error, sessionID = %lld", sessionID);
		status = 0;
		result = FALSE;
	}
	else
	{
		// TODO 중복 로그인 제거(기존 세션을 유지 할 것인지, 새로 들어온 세션을 유지할 것인지. 기존 플레이어는?)
		std::map<UINT64, PLAYER*>::iterator itor = m_PlayerMap.begin();
		for (; itor != m_PlayerMap.end(); itor++)
		{
			if ((itor)->second->iAccountNo == iAccountNo && (itor->second->SessionID != sessionID))
			{
				if (Disconnect(itor->second->SessionID) == FALSE)
				{
					itor->second->bDisconnected = TRUE;
					if (itor->second->CurrentSector.iX != -1 && itor->second->CurrentSector.iY != -1)
						ErasePlayerInSector(itor->second->SessionID, itor->second->CurrentSector.iX, itor->second->CurrentSector.iY);

					itor->second->iAccountNo = 0;
					ErasePlayer(itor->second->SessionID);
					m_AccountNoMap.erase(itor->second->iAccountNo);
					m_PlayerPool.Free(itor->second);

					InterlockedDecrement64(&m_lPlayerCountTps);
				}
				break;
			}
		}

		player->bLogined = TRUE;
		player->recvTime = timeGetTime();
		player->iAccountNo = iAccountNo;
		player->SessionID = sessionID;
		message->GetData((char*)player->ID, dfID_LEN);
		message->GetData((char*)player->Nick, dfNiCK_LEN);

		message->GetData(player->SessionKey, dfSESSIONKEY_LEN);
		m_AccountNoMap.insert(std::make_pair(player->iAccountNo, player));
		InterlockedIncrement64(&m_lPlayerCountTps);
	}

	pPacket = Packet_Proc_RES_Login(player->iAccountNo, status);
	SendPacket_Unicast(sessionID, pPacket);
	pPacket->SubRef();

	return result;
}

BOOL ChattingServer::Packet_Proc_REQ_SectorMove(UINT64 sessionID, CMessage* message)
{
	BOOL result = TRUE;
	INT64 iAccountNo;
	PLAYER* player = FindPlayer(sessionID);
	short wSectorX, wSectorY;
	CMessage* pPacket = nullptr;

	if (player == nullptr)
	{
		LOG(L"SERVER", LOG_ERROR, L"Packet_ProcREQ_SectorMove() Player is null, SessionID = %lld", sessionID);
		CRASH();
		return FALSE;
	}

	if (player->bLogined == FALSE)
	{
		LOG(L"SERVER", LOG_ERROR, L"Packet_ProcREQ_SectorMove() Player is not login, SessionID = %lld", sessionID);
		CRASH();
		return FALSE;
	}
	player->recvTime = timeGetTime();

	(*message) >> iAccountNo;

	if (iAccountNo != player->iAccountNo)
	{
		LOG(L"SERVER", LOG_ERROR, L"Packet_ProcREQ_SectorMove() Player AccountNo is not correct, SessionID = %lld, REQ_accountNo = %lld, player_accountNo = %lld", sessionID, iAccountNo, player->iAccountNo);
		CRASH();
		return FALSE;
	}


	(*message) >> wSectorX;
	(*message) >> wSectorY;

	if (!SectorUpdate(player, wSectorX, wSectorY))
	{
		LOG(L"SERVER", LOG_ERROR, L"Packet_ProcREQ_SectorMove() SectorUpdate() failed, SessionID = %lld, player_SecX = %d, player_SecY = %d, REQ_SecX = %d, REQ_SecY = %d", sessionID, player->CurrentSector.iX, player->CurrentSector.iY, wSectorX, wSectorY);
		CRASH();
		return FALSE;
	}

	pPacket = Packet_Proc_RES_SectorMove(iAccountNo, wSectorX, wSectorY);
	SendPacket_Unicast(sessionID, pPacket);

	pPacket->SubRef();

	return result;
}

BOOL ChattingServer::Packet_Proc_REQ_Chat(UINT64 sessionID, CMessage* message)
{
	BOOL result = TRUE;
	PLAYER* player = FindPlayer(sessionID);
	INT64 iAccountNo;
	CMessage* pPacket = nullptr;
	WCHAR		szMessage[1024];
	WORD len;
	if (player == nullptr)
	{
		LOG(L"SERVER", LOG_ERROR, L"Packet_ProcREQ_SectorMove() Player is null, SessionID = %lld", sessionID);
		CRASH();
		return FALSE;
	}

	(*message) >> iAccountNo;
	if (iAccountNo != player->iAccountNo)
	{
		LOG(L"SERVER", LOG_ERROR, L"Packet_ProcREQ_SectorMove() Player AccountNo is not correct, SessionID = %lld, REQ_accountNo = %lld, player_accountNo = %lld", sessionID, iAccountNo, player->iAccountNo);
		CRASH();
		return FALSE;
	}
	player->recvTime = timeGetTime();

	if (!player->bLogined || player->bDisconnected)
	{
		LOG(L"SERVER", LOG_ERROR, L"Packet_Proc_REQ_Chat() Player is error, SessionID = %lld, player_bLogined = %B, player_bDisconnected = %B", sessionID, player->bLogined, player->bDisconnected);
		CRASH();
		return FALSE;
	}	
	(*message) >> len;


	memset(szMessage, 0, 1024);
	message->GetData((char*)szMessage, len);

	pPacket = Packet_Proc_RES_Chat(player, szMessage, len);
	SendPacket_Around(player, pPacket, TRUE);

	pPacket->SubRef();

	return TRUE;
}

BOOL ChattingServer::Packet_Proc_REQ_HEARTBEAT(UINT64 sessionID)
{
	PLAYER* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		LOG(L"SERVER", LOG_ERROR, L"Packet_ProcREQ_SectorMove() Player is null, SessionID = %lld", sessionID);
		CRASH();
		return FALSE;
	}

	player->recvTime = timeGetTime();

	return TRUE;
}

CMessage* ChattingServer::Packet_Proc_RES_Login(INT64 accountNo, BYTE status)
{
	CMessage* pPacket = CMessage::Alloc();
	(*pPacket) << (WORD)en_PACKET_CS_CHAT_RES_LOGIN;
	(*pPacket) << status;
	(*pPacket) << accountNo;

	pPacket->SetEncodingCode();
	return pPacket;
}


CMessage* ChattingServer::Packet_Proc_RES_SectorMove(INT64 accountNo, short SecX, short SecY)
{
	CMessage* pPacket = CMessage::Alloc();
	(*pPacket) << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	(*pPacket) << (INT64)accountNo;
	(*pPacket) << (WORD)SecX;
	(*pPacket) << (WORD)SecY;
	pPacket->SetEncodingCode();
	return pPacket;
}

CMessage* ChattingServer::Packet_Proc_RES_Chat(PLAYER* player, WCHAR* message, WORD len)
{
	CMessage* pPacket = CMessage::Alloc();
	(*pPacket) << (WORD)en_PACKET_CS_CHAT_RES_MESSAGE;
	(*pPacket) << (INT64)player->iAccountNo;
	pPacket->PutData((char*)player->ID, dfID_LEN);
	pPacket->PutData((char*)player->Nick, dfNiCK_LEN);
	(*pPacket) << len;
	pPacket->PutData((char*)message, len);
	message[len] = L'\0';
	pPacket->SetEncodingCode();
	return pPacket;
}

ChattingServer::MSG* ChattingServer::Make_Message_Create_Client(UINT64 sessionID)
{
	MSG* newMSG = m_MSGPool.Alloc();
	newMSG->eType = en_MSG_CONNECTION;
	newMSG->SessionID = GetSessionID(sessionID);
	newMSG->pPacket = nullptr;
	return newMSG;
}

ChattingServer::MSG* ChattingServer::Make_Message_Packet(UINT64 sessionID, CMessage* message)
{
	MSG* newMSG = m_MSGPool.Alloc();
	message->AddRef();
	newMSG->eType = en_MSG_PACKET;
	newMSG->SessionID = sessionID;
	newMSG->pPacket = message;

	return newMSG;
}

ChattingServer::MSG* ChattingServer::Make_Message_Disconnect_Client(UINT64 sessionID)
{
	MSG* newMSG = m_MSGPool.Alloc();
	newMSG->eType = en_MSG_DISCONNECTION;
	newMSG->SessionID = sessionID;
	newMSG->pPacket = nullptr;
	return newMSG;
}

unsigned int __stdcall ChattingServer::Monitoring_Thread(LPVOID lpParam)
{
	((ChattingServer*)lpParam)->Monitoring_Update();
	return 0;
}

unsigned int __stdcall ChattingServer::Update_Thread(LPVOID lpParam)
{
	((ChattingServer*)lpParam)->Update_Thread();
	return 0;
}

void ChattingServer::Monitoring_Update()
{
	char chkey = 0;

	while (!m_bShutdown)
	{
		PrintPacketCount();
		wprintf(L"====================== Contents Monitor =======================\n");
		wprintf(L" - MSGPoolUsingCount    : %08d\n", m_MSGPool.GetUseCount());
		wprintf(L" - MSGPoolAllocCount    : %08d\n", m_MSGPool.GetAllocCount());
		wprintf(L" - PlayerPoolUsingCount : %08d\n", m_PlayerPool.GetUseCount());
		wprintf(L" - PlayerPoolAllocCount : %08d\n", m_PlayerPool.GetAllocCount());
		wprintf(L" - UpdateThread TPS     : %08lld\n", m_lUpdateTps);
		wprintf(L" - PlayerMap Size       : %08lld\n", m_PlayerMap.size());

		wprintf(L"===============================================================\n");
		m_lUpdateTps = 0;
		if (_kbhit() != 0)
		{
			chkey = _getch();

			switch (chkey)
			{
			case 'w':
			case 'W':
				CCrashDump::Crash();
			}
		}
		HeartBeat();
		Sleep(999);
	}
}

void ChattingServer::Update_Thread()
{
	MSG* newMsg = nullptr;
	srand(GetCurrentThreadId());
	while (!m_bShutdown)
	{
		DWORD result;
		result = WaitForSingleObject(m_hEvent, INFINITE);
		if (result != WAIT_OBJECT_0)
			CRASH();
		while (!m_MsgQ.isEmpty())
		{
			m_MsgQ.Dequeue(newMsg);
			if (CompleteMsg(newMsg) != TRUE)
			{
				LOG(L"SERVER", LOG_ERROR, L"CompleteMsg() Error");
				CRASH();
			}

			InterlockedIncrement64(&m_lUpdateTps);
		}

		std::map<UINT64, PLAYER*>::iterator itor = m_PlayerMap.begin();
		for (; itor != m_PlayerMap.end(); itor++)
		{
			if ((timeGetTime() - itor->second->recvTime) >= dfHEARTBEADT_MAXTIME)
			{
				Disconnect(itor->second->SessionID);
			}
		}
	}
}

ChattingServer::PLAYER* ChattingServer::FindPlayer(UINT64 SessionID)
{
	PLAYER* player = nullptr;
	UINT ID = GetSessionID(SessionID);
	std::map<UINT64, PLAYER*>::iterator itor = m_PlayerMap.find(ID);

	if (itor != m_PlayerMap.end())
	{
		player = itor->second;
		return player;
	}
	else
		return player;
}

BOOL ChattingServer::CompleteMsg(MSG* msg)
{
	BOOL result = FALSE;

	switch (msg->eType)
	{
	case en_MSG_CONNECTION:
		result = CreatePlayer(msg->SessionID);
		break;
	case en_MSG_PACKET:
		result = CompletePacket(msg->SessionID, msg->pPacket);
		break;
	case en_MSG_DISCONNECTION:
		result = DeletePlayer(msg->SessionID);
		break;
	}

	m_MSGPool.Free(msg);


	return result;
}

BOOL ChattingServer::CompletePacket(UINT64 sessionID, CMessage* message)
{
	BOOL result;
	WORD type = 0;
	(*message) >> type;
	switch (type)
	{
	case en_PACKET_CS_CHAT_REQ_LOGIN:
		result = Packet_Proc_REQ_Login(sessionID, message);
		break;
	case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
		result = Packet_Proc_REQ_SectorMove(sessionID, message);
		break;
	case en_PACKET_CS_CHAT_REQ_MESSAGE:
		result = Packet_Proc_REQ_Chat(sessionID, message);
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		result = Packet_Proc_REQ_HEARTBEAT(sessionID);
		break;
	default:
		Disconnect(sessionID);
		result = FALSE;
		LOG(L"SERVER", LOG_ERROR, L"Packet Error");
		break;
	}
	message->SubRef();

	return result;
}

void ChattingServer::SendPacket_Unicast(UINT64 sessionID, CMessage* message)
{
	SendPacket(sessionID, message);
}

void ChattingServer::SendPacket_Around(PLAYER* player, CMessage* message, BOOL bSendMe)
{
	SECTORAROUND CurSector;
	GetSectorAround(player->CurrentSector.iX, player->CurrentSector.iY, &CurSector);
	// 찾은 섹터의 Client들에게 Unicast 전송
	int iCnt = 0;

	for (; iCnt < CurSector.iCount; iCnt++)
	{
		if (bSendMe == FALSE)
			SendPacket_SectorOne(CurSector.Around[iCnt].iX, CurSector.Around[iCnt].iY, message, player);
		else
			SendPacket_SectorOne(CurSector.Around[iCnt].iX, CurSector.Around[iCnt].iY, message);
	}
}

void ChattingServer::SendPacket_SectorOne(int x, int y, CMessage* message, PLAYER* player)
{
	std::unordered_map<UINT64, PLAYER*>::iterator beginItor = m_Sector[y][x].begin();
	std::unordered_map<UINT64, PLAYER*>::iterator endItor = m_Sector[y][x].end();
	for (; beginItor != endItor; beginItor++)
	{
		if (beginItor->second == player)
			continue;
		SendPacket_Unicast(beginItor->first, message);
	}
}

void ChattingServer::HeartBeat()
{
	SetEvent(m_hEvent);
}

void ChattingServer::InsertPlayer(UINT64 sessionID, PLAYER* player)
{
	m_PlayerMap.insert(std::make_pair(GetSessionID(sessionID), player));
}

void ChattingServer::ErasePlayer(UINT64 sessionID)
{
	m_PlayerMap.erase(GetSessionID(sessionID));
}

void ChattingServer::InsertPlayerInSector(PLAYER* player, WORD SecX, WORD SecY)
{
	m_Sector[SecY][SecX].insert(std::make_pair(player->SessionID, player));
}

void ChattingServer::ErasePlayerInSector(UINT64 sessionID, WORD SecX, WORD SecY)
{
	m_Sector[SecY][SecX].erase(sessionID);
}

void ChattingServer::GetSectorAround(int iSectorX, int iSectorY, SECTORAROUND* pSectorAround)
{
	int iCntX, iCntY;

	iSectorX--;
	iSectorY--;
	pSectorAround->iCount = 0;

	for (iCntY = 0; iCntY < 3; iCntY++)
	{
		if (iSectorY + iCntY < 0 || iSectorY + iCntY >= dfSECTOR_MAX_Y)
			continue;

		for (iCntX = 0; iCntX < 3; iCntX++)
		{
			if (iSectorX + iCntX < 0 || iSectorX + iCntX >= dfSECTOR_MAX_X)
				continue;

			pSectorAround->Around[pSectorAround->iCount].iX = iSectorX + iCntX;
			pSectorAround->Around[pSectorAround->iCount].iY = iSectorY + iCntY;
			pSectorAround->iCount++;
		}
	}
}

ChattingServer::ChattingServer()
{
	m_bShutdown = FALSE;
	m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	HANDLE hThread = ((HANDLE)_beginthreadex(NULL, 0, Monitoring_Thread, (LPVOID)this, NULL, NULL));
	//CloseHandle(hThread);

	m_lUpdateTps = 0;
}

void ChattingServer::Start(DWORD port, BOOL nagle, const WCHAR* ip, DWORD threadCount, __int64 MaxClient)
{
	if (!NetworkLibraryWan::Start(port, nagle, ip, threadCount, MaxClient))
	{
		LOG(L"SERVER", LOG_ERROR, L"Strat() error!");
		return;
	}

	HANDLE hThread = ((HANDLE)_beginthreadex(NULL, 0, Update_Thread, (LPVOID)this, NULL, NULL));
	//CloseHandle(hThread);
}

void ChattingServer::OnClientJoin(SOCKADDR_IN* sockAddr, UINT64 sessionID)
{
	MSG* newMSG = Make_Message_Create_Client(sessionID);
	m_MsgQ.Enqueue(newMSG);
	SetEvent(m_hEvent);
}

void ChattingServer::OnClientLeave(UINT64 sessionID)
{
	MSG* newMSG = Make_Message_Disconnect_Client(sessionID);
	m_MsgQ.Enqueue(newMSG);
	SetEvent(m_hEvent);
}

bool ChattingServer::OnConnectionRequest(SOCKADDR_IN* sockAddr)
{
	return TRUE;
}

void ChattingServer::OnRecv(UINT64 sessionID, CMessage* message)
{
	MSG* newMSG = Make_Message_Packet(sessionID, message);
	m_MsgQ.Enqueue(newMSG);
	SetEvent(m_hEvent);
}

void ChattingServer::OnSend(UINT64 sessionID, int sendsize)
{
}

void ChattingServer::OnError(int errorcode, WCHAR*)
{
}