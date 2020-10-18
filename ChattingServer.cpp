#include "ChattingServer.h"
#include "stdafx.h"

BOOL ChattingServer::CreatePlayer(UINT64 sessionID)
{
	PLAYER* newPlayer = m_PlayerPool.Alloc();

	if (newPlayer == nullptr)
		return FALSE;

	newPlayer->bLogined = FALSE;
	newPlayer->recvTime = 0;

	memset(newPlayer->Nick, 0, dfID_LEN);
	memset(newPlayer->ID, 0, dfNiCK_LEN);
	memset(newPlayer->SessionKey, 0, dfSESSIONKEY_LEN);

	newPlayer->CurrentSector.iX = -1;
	newPlayer->CurrentSector.iY = -1;

	newPlayer->OldSector.iX = -1;
	newPlayer->OldSector.iY = -1;
	newPlayer->SessionID = sessionID;
	newPlayer->iAccountNo = 0;
	newPlayer->bDisconnected = FALSE;
	InsertPlayer(sessionID, newPlayer);

	return 0;
}

BOOL ChattingServer::SectorUpdate(PLAYER* player, WORD SecX, WORD SecY)
{
	// 유효하지 않은 섹터
	if (SecX == -1 || SecY == -1 || SecX == 50 || SecY == 50)
		return FALSE;
	// 변경할 필요 없음
	if (player->CurrentSector.iX == SecX && player->CurrentSector.iY == SecY)
	{
		return TRUE;
	}

	// 기존 섹터에서 Player 제거
	ErasePlayerInSector(player->SessionID, player->CurrentSector.iX, player->CurrentSector.iY);

	player->CurrentSector.iX = SecX;
	player->CurrentSector.iY = SecY;

	InsertPlayerInSector(player, SecX, SecY);

	return TRUE;
}

BOOL ChattingServer::Packet_Proc_REQ_Login(UINT64 sessionID, CMessage* message)
{
	PLAYER* player = FindPlayer(sessionID);
	CMessage* pPacket = nullptr;
	// 상태
	BYTE status = 1;
	UINT64 iAccountNo = 0;
	(*message) >> iAccountNo;

	// OnclientJoin()때 생성되지 않은 Player에 대해서 Packet이 온다면
	if (player == nullptr)
	{
		LOG(L"SERVER", LOG_ERROR,L"REQ_Login error, sessionID = %lld", sessionID);
		status = 1;
	}
	else
	{
		// TODO 중복 로그인 제거(기존 세션을 유지 할 것인지, 새로 들어온 세션을 유지할 것인지. 기존 플레이어는?)
		std::unordered_map<UINT64, PLAYER*>::iterator itor = m_PlayerMap.begin();
		for (; itor != m_PlayerMap.end(); itor++)
		{
			if ((itor)->second->iAccountNo == iAccountNo && (itor->first == sessionID))
			{
				Dissconnect(sessionID);
				itor->second->bDisconnected = TRUE;
				break;
			}
		}
		
		player->bLogined = TRUE;
		player->recvTime = GetTickCount64();
		player->iAccountNo = iAccountNo;
		message->GetData((char*)player->ID, dfID_LEN);
		message->GetData((char*)player->Nick, dfNiCK_LEN);

		message->GetData(player->SessionKey, dfSESSIONKEY_LEN);

		InterlockedIncrement64(&m_lPlayerCountTps);
	}

	pPacket = Packet_Proc_RES_Login(player->iAccountNo, status);
	SendPacket_Unicast(player->SessionID, pPacket);
	pPacket->SubRef();

	return 0;
}

CMessage* ChattingServer::Packet_Proc_RES_Login(INT64 accountNo, BYTE status)
{
	CMessage* pPacket = CMessage::Alloc();
	(*pPacket) << (WORD)en_PACKET_CS_CHAT_RES_LOGIN;
	(*pPacket) << status;
	(*pPacket) << accountNo;
	return pPacket;
}

BOOL ChattingServer::Packet_Proc_REQ_SectorMove(UINT64 sessionID, CMessage* message)
{
	BOOL result = FALSE;
	INT64 iAccountNo;
	PLAYER* player = FindPlayer(sessionID);
	WORD wSectorX, wSectorY;
	CMessage* pPacket = nullptr;

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

	return TRUE;
}

CMessage* ChattingServer::Packet_Proc_RES_SectorMove(INT64 accountNo, WORD SecX, WORD SecY)
{
	CMessage* pPacket = CMessage::Alloc();
	(*pPacket) << (WORD)en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	(*pPacket) << (INT64)accountNo;
	(*pPacket) << (WORD)SecX;
	(*pPacket) << (WORD)SecY;
	return pPacket;
}

ChattingServer::MSG* ChattingServer::Make_Create_Client(UINT64 sessionID)
{
	MSG* newMSG = m_MSGPool.Alloc();
	newMSG->eType = en_MSG_CONNECTION;
	newMSG->SessionID = sessionID;
	newMSG->pPacket = nullptr;
	return newMSG;
}

ChattingServer::MSG* ChattingServer::Make_Message_Packet(UINT64 sessionID, CMessage* message)
{
	MSG* newMSG = m_MSGPool.Alloc();
	newMSG->eType = en_MSG_PACKET;
	newMSG->SessionID = sessionID;
	newMSG->pPacket = message;

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
}

void ChattingServer::Update_Thread()
{
	MSG* newMsg = nullptr;
	while (!m_bShutdown)
	{
		DWORD result;
		result = WaitForSingleObject(m_hEvent, INFINITE);
		if (result != WAIT_OBJECT_0)
			CRASH();
		while (!m_MsgQ.isEmpty())
		{
			m_MsgQ.Dequeue(newMsg);
			if (CompleteMsg(newMsg) != FALSE)
			{
				LOG(L"SERVER", LOG_ERROR, L"CompleteMsg() Error");
				CRASH();
			}

			InterlockedIncrement64(&m_lUpdateTps);
		}

	}
}

ChattingServer::PLAYER* ChattingServer::FindPlayer(UINT64 SessionID)
{
	PLAYER* player = nullptr;
	std::unordered_map<UINT64, PLAYER*>::iterator itor = m_PlayerMap.find(SessionID);

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
	while (!m_MsgQ.isEmpty() == 0)
	{
		switch (msg->eType)
		{
		case en_MSG_CONNECTION:
			result = CreatePlayer(msg->SessionID);
			break;
		case en_MSG_PACKET:
			result = CompletePacket(msg->SessionID, msg->pPacket);
			break;
		case en_MSG_DISCONNECTION:
			break;

		}
	}

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
		break;
	case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
		break;
	}
	return 0;
}

void ChattingServer::SendPacket_Unicast(UINT64 sessionID, CMessage* message)
{
	SendPacket(sessionID, message);
}

void ChattingServer::InsertPlayer(UINT64 sessionID, PLAYER* player)
{
	m_PlayerMap.insert(std::make_pair(sessionID, player));
}

void ChattingServer::ErasePlayer(UINT64 sessionID)
{
	m_PlayerMap.erase(sessionID);
}

void ChattingServer::InsertPlayerInSector(PLAYER* player, WORD SecX, WORD SecY)
{
	m_Sector[SecY][SecX].insert(std::make_pair(player->SessionID,player));
}

void ChattingServer::ErasePlayerInSector(UINT64 sessionID, WORD SecX, WORD SecY)
{
	m_Sector[SecY][SecX].erase(sessionID);
}

ChattingServer::ChattingServer()
{
	m_bShutdown = FALSE;
	m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	HANDLE hThread = ((HANDLE)_beginthreadex(NULL, 0, Monitoring_Thread, (LPVOID)this, NULL, NULL));
	CloseHandle(hThread);

	m_lUpdateTps = 0;
}

void ChattingServer::Start(DWORD port, BOOL nagle, const WCHAR* ip = nullptr, DWORD threadCount = 0, __int64 MaxClient = 0)
{
	if (!NetworkLibraryWan::Start(port, nagle, ip, threadCount, MaxClient))
	{
		LOG(L"SERVER", LOG_ERROR, L"Strat() error!");
		return;
	}

	HANDLE hThread = ((HANDLE)_beginthreadex(NULL, 0, Monitoring_Thread, (LPVOID)this, NULL, NULL));
	CloseHandle(hThread);
}

void ChattingServer::OnClientJoin(SOCKADDR_IN* sockAddr, UINT64 sessionID)
{
	MSG* newMSG = Make_Create_Client(sessionID);
	m_MsgQ.Enqueue(newMSG);
	SetEvent(m_hEvent);	
}

void ChattingServer::OnRecv(UINT64 sessionID, CMessage* message)
{
	MSG* newMSG = Make_Message_Packet(sessionID, message);
	m_MsgQ.Enqueue(newMSG);
	SetEvent(m_hEvent);
}
