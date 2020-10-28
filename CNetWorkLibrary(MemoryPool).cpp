#include "stdafx.h"
#include "CCrashDumpClass.h"
#include "CNetWorkLibrary(MemoryPool).h"

BOOL joshua::NetworkLibraryWan::InitialNetwork(const WCHAR* ip, DWORD port, BOOL Nagle)
{
	timeBeginPeriod(1);
	int retval;
	bool fail = false;
	_bNagle = Nagle;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"WSAStartup() Error!");
		return fail;
	}

	// socket
	_slisten_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_slisten_socket == INVALID_SOCKET)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"socket() Error!");
		return fail;
	}

	// bind()
	ZeroMemory(&_serveraddr, sizeof(_serveraddr));
	_serveraddr.sin_family = AF_INET;
	if (ip == NULL)
		_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	else
		InetPton(AF_INET, ip, &_serveraddr.sin_addr);
	_serveraddr.sin_port = htons(port);
	retval = bind(_slisten_socket, (SOCKADDR*)&_serveraddr, sizeof(_serveraddr));
	if (retval == SOCKET_ERROR)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"bind() Error!");
		return fail;
	}

	//wprintf(L"sendbuf size : %d\n", optval);

	setsockopt(_slisten_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&Nagle, sizeof(Nagle));

	// listen()
	retval = listen(_slisten_socket, SOMAXCONN);
	if (retval == SOCKET_ERROR)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"listen() Error!");
		return fail;
	}

	return (!fail);
}

BOOL joshua::NetworkLibraryWan::CreateThread(DWORD threadCount)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	HANDLE hThread = NULL;
	//  Accept 스레드 생성
	_AcceptThread = ((HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)this, NULL, NULL));
	if (_AcceptThread == NULL)
	{
		return FALSE;
	}
	CloseHandle(_AcceptThread);
	// 스레드 제한
	_iThreadCount = threadCount;
	if (_iThreadCount == NULL || _iThreadCount > si.dwNumberOfProcessors * 2)
	{
		for (int i = 0; i < si.dwNumberOfProcessors * 2; i++)
		{
			hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)this, 0, NULL);
			if (hThread == NULL)
				return FALSE;
			CloseHandle(hThread);
			_ThreadVector.push_back(hThread);
		}
		_iThreadCount = si.dwNumberOfProcessors * 2;
	}
	else
	{
		for (int i = 0; i < _iThreadCount; i++)
		{
			hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)this, 0, NULL);
			if (hThread == NULL)
				return FALSE;
			CloseHandle(hThread);
			_ThreadVector.push_back(hThread);
		}
	}
	return TRUE;
}

BOOL joshua::NetworkLibraryWan::CreateSession()
{
	_SessionArray = new st_SESSION[_dwSessionMax];
	for (__int64 i = _dwSessionMax - 1; i >= 0; --i)
	{
		_SessionArray[i].index = i;
		PushIndex(i);
	}
	return TRUE;
}

void joshua::NetworkLibraryWan::PushIndex(UINT64 index)
{
	EnterCriticalSection(&_IndexStackCS);
	_ArrayIndex.push(index);
	LeaveCriticalSection(&_IndexStackCS);
}

UINT64 joshua::NetworkLibraryWan::PopIndex()
{
	EnterCriticalSection(&_IndexStackCS);
	UINT64 temp = _ArrayIndex.top();
	_ArrayIndex.pop();
	LeaveCriticalSection(&_IndexStackCS);

	return temp;
}

joshua::st_SESSION* joshua::NetworkLibraryWan::InsertSession(SOCKET sock, SOCKADDR_IN* sockaddr)
{
	UINT64 temp = -1;
	temp = PopIndex();
	if (temp == -1)
	{
		LOG(L"SERVER", LOG_ERROR, L"IndexStack Lack. CurrentSesionCount : %d\n", _dwSessionCount);
		return nullptr;
	}
	else
	{
		// TCP 소켓에서 좀비세션 처리
		// 좀비 세션은 비정상적으로 종료(랜선 문제, 혹은 외부에서 세션을 일부러 끊은 경우)되어 서버나 클라이언트에서
		// 세션이 종료된지 모르는 상태의 세션을 의미 (Close 이벤트를 받지 못함)
		// TCP KeepAlive사용
				// - SO_KEEPALIVE : 시스템 레지스트리 값 변경. 시스템의 모든 SOCKET에 대해서 KEEPALIVE 설정
		// - SIO_KEEPALIVE_VALS : 특정 SOCKET만 KEEPALIVE 설정
		//tcp_keepalive tcpkl;
		//tcpkl.onoff = TRUE;
		//tcpkl.keepalivetime = 30000; // ms
		//tcpkl.keepaliveinterval = 1000;
		//WSAIoctl(sock, SIO_KEEPALIVE_VALS, &tcpkl, sizeof(tcp_keepalive), 0, 0, NULL, NULL, NULL);
	/*	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&_bNagle, sizeof(_bNagle));*/

		st_SESSION* pSession = &_SessionArray[temp];
		pSession->SessionID = CreateSessionID(++_dwSessionID, temp);
		pSession->socket = sock;
		pSession->bIsSend = FALSE;
		pSession->dwPacketCount = 0;
		pSession->SendBuffer.Clear();
		pSession->RecvBuffer.ClearBuffer();
		pSession->lIO->lIOCount = 0;
		pSession->lIO->bIsReleased = FALSE;
		pSession->recvTime = timeGetTime();
		InterlockedIncrement64(&pSession->lIO->lIOCount);
		memcpy(&pSession->clientaddr, sockaddr, sizeof(SOCKADDR_IN));
		InterlockedIncrement64(&_dwSessionCount);
		InterlockedIncrement64(&_dwCount);
		return pSession;
	}
}

unsigned int __stdcall joshua::NetworkLibraryWan::AcceptThread(LPVOID lpParam)
{
	((NetworkLibraryWan*)lpParam)->AcceptThread();

	return 0;
}

unsigned int __stdcall joshua::NetworkLibraryWan::WorkerThread(LPVOID lpParam)
{
	((NetworkLibraryWan*)lpParam)->WorkerThread();
	return 0;
}

void joshua::NetworkLibraryWan::AcceptThread(void)
{
	BOOL bFlag = false;
	srand(GetTickCount64() + 1);
	SYSLOGCLASS* _pLog = SYSLOGCLASS::GetInstance();
	while (!bFlag)
	{
		SOCKET clientsocket;
		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);

		clientsocket = accept(_slisten_socket, reinterpret_cast<sockaddr*>(&clientaddr), &addrlen);

		//소켓 Send버퍼의 크기를 0으로 만들자
		int optval;

		int optlen = sizeof(optval);

		getsockopt(clientsocket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, &optlen);

		optval = 0;

		setsockopt(clientsocket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));

		getsockopt(clientsocket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, &optlen);

		_lAcceptCount++;
		_lAcceptTPS++;

		if (!_bServerOn)
			break;
		//clientsocket = accept(_slisten_socket, (SOCKADDR*)&clientaddr, &addrlen);
		if (clientsocket == INVALID_SOCKET)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_ERROR, L"%s client IP : %s, port : %d\n", L"accept() Error!", szParam, ntohs(clientaddr.sin_port));
			DisconnectSocket(clientsocket);
			continue;
		}

		if (OnConnectionRequest(&clientaddr) == FALSE)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_WARNNING, L"client IP : %s, port : %d connection denied\n", szParam, ntohs(clientaddr.sin_port));
			DisconnectSocket(clientsocket);
			continue;
		}

		if (_dwSessionCount >= _dwSessionMax)
		{
			WCHAR szParam[16] = { 0 };
			InetNtop(AF_INET, &clientaddr.sin_addr, szParam, 16);
			LOG(L"SERVER", LOG_WARNNING, L"client IP : %s, port : %d connection denied, MaxClient Over\n", szParam, ntohs(clientaddr.sin_port));
			DisconnectSocket(clientsocket);
			continue;
		}
		st_SESSION* pSession = InsertSession(clientsocket, &clientaddr);
		if (pSession == nullptr)
		{
			// 서버 꺼야함
			// TODO dump내고 끄자
			InterlockedDecrement64(&_dwSessionCount);
			DisconnectSocket(clientsocket);
			continue;
		}
		// 소켓과 입출력 완료 포트 연결
		if (CreateIoCompletionPort((HANDLE)clientsocket, (HANDLE)_hCP, (ULONG_PTR)pSession, 0) == NULL)
		{
			//  HANDLE 인자에 소켓이 아닌 값이 올 경우 잘못된 핸들(6번 에러) 발생
			// 소켓이 아닌 값을 넣었다는 것은 다른 스레드에서 소켓을 반환했다는 의미이므로 동기화 문제일 가능성이 높다.
			LOG(L"SYSTEM", LOG_ERROR, L"AcceptThread() - CreateIoCompletionPort() failed : %d", WSAGetLastError());
			if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
			{
				//DisconnectSession(pSession);
				SessionRelease(pSession);
				continue;
			}
		}
		pSession->lIO->bIsReleased = FALSE;

		OnClientJoin(&pSession->clientaddr, pSession->SessionID);

		PostRecv(pSession);

		if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
		{
			SessionRelease(pSession);
		}
	}
	LOG(L"SYSTEM", LOG_DEBUG, L"AcceptThread Exit");
	return;
}

void joshua::NetworkLibraryWan::WorkerThread(void)
{
	int retval;
	// 비동기 입출력 완료 기다리기
	DWORD cbTransferred;
	LPOVERLAPPED  pOverlapped;
	st_SESSION* pSession;
	while (true)
	{
		cbTransferred = 0;
		pOverlapped = 0;
		pSession = 0;

		// GQCS return 경우의 수.
		// 1. IOCP Queue로부터 완료패킷을 얻어내는 데 성공한 경우 => TRUE 리턴, lpCompletionKey 세팅

		// 2. IOCP Queue로부터 완료패킷을 얻어내는 데 실패한 경우와 동시에 lpOverlapped가 NULL인 경우 => FALSE 리턴

		// 3. IOCP Queue로부터 완료패킷을 얻어내는 데 성공한 경우와 동시에
		//	  lpOverlapped가 NULL이 아닌 경우이면서
		//    dequeue한 요소가 실패한 I/O인 경우
		// => FALSE리턴, lpCompletionKey 세팅

		// 4. IOCP에 등록된 소켓이 close된 경우
		// => FALSE 리턴, lpOverlapped에 NULL이 아니고,
		// => lpNumberOfBytes에 0 세팅.

		// 5. 정상종료 (PQCS)
		// => lpCompletionKey는 NULL, lpOverlapped는 NULL, lpNumberOfBytes또한 NULL

		retval = GetQueuedCompletionStatus(_hCP, &cbTransferred, reinterpret_cast<PULONG_PTR>(&pSession), &pOverlapped, INFINITE);

		if (retval == true)
		{
			if (cbTransferred == 0 && pSession == 0 && pOverlapped == 0)
			{
				PostQueuedCompletionStatus(_hCP, 0, 0, 0);	// GQCS 에러 시 조치를 취할만한게 없으므로 종료
				break;
			}

			if (cbTransferred == 0)
			{
				if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
				{
					if (pSession->lIO->lIOCount < 0)
						CRASH();
					SessionRelease(pSession);
				}
				continue;
			}

			else
			{
				if (pOverlapped == &pSession->RecvOverlapped)
				{
					RecvComplete(pSession, cbTransferred);
				}
				if (pOverlapped == &pSession->SendOverlapped)
				{
					SendComplete(pSession, cbTransferred);
				}

				if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
				{
					if (pSession->lIO->lIOCount < 0)
						CRASH();
					SessionRelease(pSession);
				}
			}
		}
		else
		{
			if (cbTransferred == 0)
			{
				if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
				{
					SessionRelease(pSession);
				}
				continue;
				//DisconnectSession(pSession);
			}
			else
			{
				LOG(L"SYSTEM", LOG_ERROR, L"GetQueuedCompletionStatus() failed %d", GetLastError());
				PostQueuedCompletionStatus(_hCP, 0, 0, 0);	// GQCS 에러 시 조치를 취할만한게 없으므로 종료
				break;
			}
		}
	}
	return;
}

void joshua::NetworkLibraryWan::HeartBeatThread(void)
{
	for (int i = 0; i < _dwSessionMax; i++)
	{
		if ((timeGetTime() - _SessionArray[i].recvTime >= dfHEARTBEADT_MAXTIME))
		{
			st_SESSION* pSession = &_SessionArray[i];
			if (InterlockedDecrement64(&pSession[i].lIO->lIOCount) == 0)
				SessionRelease(pSession);
		}
	}
}

bool joshua::NetworkLibraryWan::PostSend(st_SESSION* pSession)
{
	while (1)
	{
		if (pSession->SendBuffer.GetUsingCount() == 0)
			return false;
		if (InterlockedCompareExchange(&pSession->bIsSend, TRUE, FALSE) == TRUE)
			return false;
		if (pSession->SendBuffer.GetUsingCount() == 0)
		{
			InterlockedExchange(&pSession->bIsSend, FALSE);
			continue;
		}
		break;
	}

	int iBufCnt;
	WSABUF wsabuf[2000];
	CMessage* pPacket;
	int iPacketCnt = pSession->SendBuffer.GetUsingCount();
	for (iBufCnt = 0; iBufCnt < iPacketCnt && iBufCnt < 100; ++iBufCnt)
	{
		if (!pSession->SendBuffer.Peek(pPacket, iBufCnt))
			break;
		wsabuf[iBufCnt].buf = pPacket->GetWanHeaderPtr();
		wsabuf[iBufCnt].len = pPacket->GetDataSize() + 5;
		//st_PACKET_HEADER* header = (st_PACKET_HEADER*)pPacket->GetWanHeaderPtr();
		//wprintf(L"byCode %x, len %d, randKey %d, CheckSum %d\n", header->byCode, header->wLen, header->byRandKey, header->byCheckSum);
	}
	pSession->dwPacketCount = iBufCnt;
	DWORD dwTransferred = 0;
	ZeroMemory(&pSession->SendOverlapped, sizeof(OVERLAPPED));
	InterlockedIncrement64(&pSession->lIO->lIOCount);
	if (WSASend(pSession->socket, wsabuf, iBufCnt, &dwTransferred, 0, &pSession->SendOverlapped, NULL) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();

		// WSASend가 pending이 걸릴 경우는 현재 send 임시 버퍼가 가득찬 상태
		if (error != WSA_IO_PENDING)
		{
			if (error != 10038 && error != 10053 && error != 10054 && error != 10058)
				LOG(L"SYSTEM", LOG_ERROR, L"WSASend() # failed%d / Socket:%d / IOCnt:%d / SendQ Size:%d", error, pSession->socket, pSession->lIO->lIOCount, pSession->SendBuffer.GetUsingCount());

			//DisconnectSession(pSession);
			if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
				SessionRelease(pSession);

			return false;
		}
	}

	return true;
}

bool joshua::NetworkLibraryWan::PostRecv(st_SESSION* pSession)
{
	WSABUF wsabuf[2];
	int iBufCnt = 1;
	wsabuf[0].buf = pSession->RecvBuffer.GetWriteBufferPtr();
	wsabuf[0].len = pSession->RecvBuffer.GetNotBrokenPutSize();

	if (pSession->RecvBuffer.GetNotBrokenPutSize() < pSession->RecvBuffer.GetFreeSize())
	{
		wsabuf[1].buf = pSession->RecvBuffer.GetBufferPtr();
		wsabuf[1].len = pSession->RecvBuffer.GetFreeSize() - wsabuf[0].len;
		++iBufCnt;
	}

	DWORD dwTransferred = 0;
	DWORD dwFlag = 0;
	ZeroMemory(&pSession->RecvOverlapped, sizeof(OVERLAPPED));
	InterlockedIncrement64(&pSession->lIO->lIOCount);
	//wprintf(L"IO COUNT : %d\n", pSession->dwIOCount);
	if (WSARecv(pSession->socket, wsabuf, iBufCnt, &dwTransferred, &dwFlag, &pSession->RecvOverlapped, NULL) == SOCKET_ERROR)
	{
		int error = WSAGetLastError();

		if (error != WSA_IO_PENDING)
		{			// WSAENOTSOCK(10038) : 소켓이 아닌 항목에 소켓 작업 시도
			// WSAECONNABORTED(10053) : 호스트가 연결 중지. 데이터 전송 시간 초과, 프로토콜 오류 발생
			// WSAECONNRESET(10054) : 원격 호스트에 의해 기존 연결 강제 해제. 원격 호스트가 갑자기 중지되거나 다시 시작되거나 하드 종료를 사용하는 경우
			// WSAESHUTDOWN(10058) : 소켓 종료 후 전송
			if (error != 10038 && error != 10053 && error != 10054 && error != 10058)
			{
				LOG(L"SYSTEM", LOG_ERROR, L"WSASend() # failed%d / Socket:%d / IOCnt:%d / SendQ Size:%d", error, pSession->socket, pSession->lIO->lIOCount, pSession->SendBuffer.GetUsingCount()); 
				CRASH();
			}


			//DisconnectSession(pSession);
			if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
				SessionRelease(pSession);

			return false;
		}
	}

	return true;
}

BOOL joshua::NetworkLibraryWan::Start(DWORD port, BOOL nagle, const WCHAR* ip, DWORD threadCount, __int64 MaxClient)
{
	// 소켓 초기화
	_dwSessionMax = MaxClient;
	_bServerOn = TRUE;

	if (InitialNetwork(ip, port, nagle) == false)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"Initialize failed!");
		return FALSE;
	}

	// IOCP 생성
	_hCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (_hCP == NULL)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"CreateIoCompletionPort() ERROR");
		return FALSE;
	}

	// 스레드 생성
	if (CreateThread(threadCount) == FALSE)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"CreateThread() ERROR");
		return FALSE;
	}

	// 세션 할당 및 인덱스 스택 설정
	if (CreateSession() == FALSE)
	{
		LOG(L"SERVER", LOG_ERROR, L"%s\n", L"CreateSession() ERROR");
		return FALSE;
	}

	return TRUE;
}

void joshua::NetworkLibraryWan::SessionRelease(st_SESSION* pSession)
{
	if (pSession->lIO->bIsReleased == TRUE)
		return;

	st_SESSION_FLAG temp(0, FALSE);
	if (!InterlockedCompareExchange128((LONG64*)pSession->lIO, TRUE, 0, (LONG64*)&temp))
		return;

	DisconnectSession(pSession);

	OnClientLeave(pSession->SessionID);

	ZeroMemory(&pSession->clientaddr, sizeof(SOCKADDR_IN));
	CMessage* pPacket = nullptr;
	while (pSession->SendBuffer.Dequeue(pPacket))
	{
		if (pPacket != nullptr)
			pPacket->SubRef();
		pPacket = nullptr;
	}

	InterlockedExchange(&pSession->bIsSend, FALSE);
	pSession->SessionID = -1;
	pSession->socket = INVALID_SOCKET;
	pSession->dwPacketCount = 0;
	pSession->lMessageList.clear();
	PushIndex(pSession->index);
	InterlockedDecrement64(&_dwSessionCount);
}

void joshua::NetworkLibraryWan::DisconnectSocket(SOCKET sock)
{
	LINGER optval;
	int retval;
	optval.l_onoff = 1;
	optval.l_linger = 0;

	retval = setsockopt(sock, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
	closesocket(sock);
}

void joshua::NetworkLibraryWan::SendPacket(UINT64 id, CMessage* message)
{
	st_SESSION* pSession = SessionReleaseCheck(id);
	if (pSession == nullptr)
		return;

	message->AddRef();
	pSession->SendBuffer.Enqueue(message);

	PostSend(pSession);


	if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
		SessionRelease(pSession);

	return;
}

BOOL joshua::NetworkLibraryWan::Disconnect(UINT64 id)
{
	st_SESSION* pSession = SessionReleaseCheck(id);
	if (pSession == nullptr)
		return FALSE;

	DisconnectSocket(pSession->socket);
	
	if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
		SessionRelease(pSession);


	return TRUE;
}

void joshua::NetworkLibraryWan::Stop()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	_bServerOn = FALSE;

	closesocket(_slisten_socket);

	WaitForSingleObject(_AcceptThread, INFINITE);

	for (int i = 0; i < si.dwNumberOfProcessors * 2; i++)
	{
		PostQueuedCompletionStatus(_hCP, 0, 0, 0);
	}

	WaitForMultipleObjects(_iThreadCount, (HANDLE*)&_ThreadVector, TRUE, INFINITE);

	WSACleanup();

	CloseHandle(_hCP);

	for (int i = 0; i < MAX_CLIENT_COUNT; i++)
	{
		if (_SessionArray[i].socket != INVALID_SOCKET)
			SessionRelease(&_SessionArray[i]);
	}

	if (_SessionArray != NULL)
	{
		delete[] _SessionArray;
		_SessionArray = NULL;
	}

	_slisten_socket = INVALID_SOCKET;
	_dwSessionID = 0;
	_dwSessionCount = _dwSessionMax = 0;
	_hCP = INVALID_HANDLE_VALUE;
	ZeroMemory(&_serveraddr, sizeof(_serveraddr));
	_bNagle = FALSE;
	_dwCount = 0;
	_iThreadCount = 0;
	_ThreadVector.clear();
	for (int i = 0; i < _ArrayIndex.size(); i++)
	{
		_ArrayIndex.pop();
	}

	// TODO SessionArray 초기화 필요
}

void joshua::NetworkLibraryWan::PrintPacketCount()
{
	wprintf(L"==================== IOCP Echo Server Test ====================\n");
	wprintf(L" - SessionCount         : %lld\n", _dwSessionCount);
	wprintf(L" - Accept Count         : %08lld\n", _lAcceptCount);
	wprintf(L" - Accept TPS           : %08lld\n", _lAcceptTPS);
	wprintf(L" - Send TPS             : %08lld\n", _lSendTPS);
	wprintf(L" - Recv TPS             : %08lld\n", _lRecvTPS);
	wprintf(L" - PacketUse            : %08d\n", CMessage::GetPacketUsingSize());
	_lAcceptTPS = _lSendTPS = _lRecvTPS = 0;
}

void joshua::NetworkLibraryWan::DisconnectSession(st_SESSION* pSession)
{
	LINGER optval;
	int retval;
	optval.l_onoff = 1;
	optval.l_linger = 0;

	retval = setsockopt(pSession->socket, SOL_SOCKET, SO_LINGER, (char*)&optval, sizeof(optval));
	closesocket(pSession->socket);
}

joshua::st_SESSION* joshua::NetworkLibraryWan::SessionReleaseCheck(UINT64 iSessionID)
{
	// multi-thread 환경에서 release, connectm send, accept 등이 동시에 발생할 수 있음을 생각해야 한다.
	int nIndex = GetSessionIndex(iSessionID);
	st_SESSION* pSession = &_SessionArray[nIndex];

	// 이미 해제 되었다면(이미 해제 코드를 탔다면)
	if (pSession->lIO->bIsReleased == TRUE)
		return nullptr;

	// ioCount가 증가해서 1이라는 뜻은 어디선가 이 세션에 대한 release가 진행되고 있다는 뜻.
	if (InterlockedIncrement64((LONG64*)&pSession->lIO->lIOCount) == 1)
	{
		if (InterlockedDecrement64(&pSession->lIO->lIOCount) == 0)
			SessionRelease(pSession);

		return nullptr;
	}

	// 세션을 검색해서 얻었으나 이미 다른 세션으로 대체된 경우
	if (pSession->SessionID != iSessionID)
	{
		if (InterlockedDecrement64((LONG64*)&pSession->lIO->lIOCount) == 0)
		{
			SessionRelease(pSession);
		}
		return nullptr;
	}

	if (pSession->lIO->bIsReleased == FALSE)
		return pSession;

	if (InterlockedDecrement64((LONG64*)&pSession->lIO->lIOCount) == 0)
		SessionRelease(pSession);

	//if (pSession == nullptr)
	//	return nullptr;

	return nullptr;
}

void joshua::NetworkLibraryWan::RecvComplete(st_SESSION* pSession, DWORD dwTransferred)
{	// 수신량만큼 RecvQ.MoveWritePos 이동
	pSession->RecvBuffer.MoveWritePos(dwTransferred);
	st_PACKET_HEADER packetHeader;
	pSession->recvTime = timeGetTime();

	// Header는 Payload 길이를 담은 2Byte 크기의 타입
	while (1)
	{
		// 1. RecvQ에 'sizeof(Header)'만큼 있는지 확인
		int iRecvSize = pSession->RecvBuffer.GetUseSize();
		if (iRecvSize < dfMESSAGE_HEADER_LEN)
		{
			// 더 이상 처리할 패킷이 없음
			// 만약, Header 크기보다 작은 모종의 데이터가 누적된다면 2번에서 걸러짐
			break;
		}

		pSession->RecvBuffer.Peek(reinterpret_cast<char*>(&packetHeader), sizeof(packetHeader));
		// 2. Packet 길이 확인 : Header크기('sizeof(packetHeader)') + Payload길이('Header')
		if (pSession->RecvBuffer.GetUseSize() < sizeof(packetHeader) + packetHeader.wLen)
		{			// Header의 Payload의 길이가 실제와 다름
			//DisconnectSession(pSession);
			//LOG(L"SYSTEM", LOG_WARNNING, L"Header Error");
			break;
		}


		CMessage* pPacket = CMessage::Alloc();
		//pPacket->PutData((char*)&packetHeader, sizeof(packetHeader));

		pPacket->SetWanMessageHeader((char*)&packetHeader, sizeof(packetHeader));


		//// 3. Payload 길이 확인 : PacketBuffer의 최대 크기보다 Payload가 클 경우
		//if (pPacket->GetBufferSize() < wHeader)
		//{
		//	pPacket->SubRef();
		//	DisconnectSession(pSession);
		//	LOG(L"SYSTEM", LOG_WARNNING, L"PacketBufferSize < PayloadSize ");
		//	break;
		//}
		pSession->RecvBuffer.RemoveData(sizeof(packetHeader));

		// 4. PacketPool에 Packet 포인터 할당
		if (pSession->RecvBuffer.Peek(pPacket->GetBufferPtr(), packetHeader.wLen) != packetHeader.wLen)
		{
			pPacket->SubRef();
			LOG(L"SYSTEM", LOG_WARNNING, L"RecvQ dequeue error");
			DisconnectSession(pSession);
			break;
		}

		pPacket->MoveWritePos(packetHeader.wLen);
		//UINT64 data;
		//memcpy(&data, pPacket->GetBufferPtr() + 2, 8);
		////wprintf(L"Recv data : %08ld\n", data);
		// 5. Packet 처리
		pSession->RecvBuffer.RemoveData(packetHeader.wLen);
		InterlockedIncrement64(&_lRecvTPS);

		if (pPacket->SetDecodingCode() == FALSE)
		{
			pPacket->SubRef();
			LOG(L"SYSTEM", LOG_WARNNING, L"Decoding Error! SessionID %lld, Packet %x", pSession->SessionID, pPacket->GetWanHeaderPtr());
			DisconnectSession(pSession);
			break;
		}

		OnRecv(pSession->SessionID, pPacket);

		pPacket->SubRef();
	}
	PostRecv(pSession);
}

void joshua::NetworkLibraryWan::SendComplete(st_SESSION* pSession, DWORD dwTransferred)
{
	OnSend(pSession->SessionID, dwTransferred);

	// 보낸 패킷 수 만큼 지우기
	CMessage* pPacket;
	for (int i = 0; i < pSession->dwPacketCount; ++i)
	{
		if (pSession->SendBuffer.Dequeue(pPacket))
		{
			InterlockedIncrement64(&_lSendTPS);
			pPacket->SubRef();
		}
	}
	pSession->dwPacketCount = 0;

	InterlockedExchange(&pSession->bIsSend, FALSE);
	PostSend(pSession);
}