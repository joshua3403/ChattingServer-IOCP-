#include "stdafx.h"
#include "CNetworkTest.h"

void Network::OnClientJoin(SOCKADDR_IN* sockAddr, UINT64 sessionID)
{
	CMessage* cPacket = CMessage::Alloc();
	WORD wHeader = 8;
	(*cPacket) << wHeader;

	UINT64 lPayLoad = 0x7fffffffffffffff;
	(*cPacket).PutData((char*)&lPayLoad, sizeof(UINT64));
	SendPacket(sessionID, cPacket);

	cPacket->SubRef();

}

void Network::OnClientLeave(UINT64 sessionID)
{
}

bool Network::OnConnectionRequest(SOCKADDR_IN* sockAddr)
{
    return true;
}

void Network::OnRecv(UINT64 sessionID, CMessage* message)
{
	SendPacket(sessionID, message);
}

void Network::OnSend(UINT64 sessionID, int sendsize)
{
}

void Network::OnError(int errorcode, WCHAR*)
{
}
