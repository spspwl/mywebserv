#include "Socket.h"

DATAPROCESSPROC		DataProc;
SOCKETEX			ClientSck[MAX_USER] = { 0 };
INT					_WebServStatus_ = 0;
SOCKET				ServerSock = INVALID_SOCKET;

SOCKET 
InitializeSocket(
)
{
	/*
		������ �ʱ�ȭ�ϰ� ���� ������ ��ȯ�ϴ� �Լ� �Դϴ�.
	*/

	WSADATA		wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR)
		return INVALID_SOCKET;

	return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

BOOLEAN 
BindAndListenSocket(
	SOCKET			sck, 
	WORD			wSrvPort,
	INT				send_timeout,
	INT				recv_timeout
)
{
	/*
		������ ������ ��ȯ�ϰ� Ŭ���̾�Ʈ�� ���� �غ� �ϴ� �Լ��Դϴ�.
	*/

	sockaddr_in	socket = { 0 };

	if (INVALID_SOCKET == sck)
		return FALSE;

	// �۽� Ÿ�Ӿƿ��� �����ϴ� �ڵ� �Դϴ�.
	if (SOCKET_ERROR == setsockopt(sck, SOL_SOCKET, SO_SNDTIMEO, (const char*)&send_timeout, sizeof(send_timeout)))
		return FALSE;

	// ���� Ÿ�Ӿƿ��� �����ϴ� �ڵ� �Դϴ�.
	if (SOCKET_ERROR == setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, (const char*)&recv_timeout, sizeof(recv_timeout)))
		return FALSE;

	socket.sin_family = AF_INET;
	socket.sin_addr.s_addr = INADDR_ANY;
	socket.sin_port = htons(wSrvPort);

	if (SOCKET_ERROR == bind(sck, (const sockaddr*)&socket, sizeof(SOCKADDR_IN)))
		goto RELEASE;

	if (SOCKET_ERROR == listen(sck, SOMAXCONN))
		goto RELEASE;

	return TRUE;

RELEASE:

	CloseSocket(sck);
	WSACleanup();

	return FALSE;
}

VOID 
CloseSocket(
	PSOCKETEX psckEx
)
{
	/*
		������ �ݴ� �Լ� �Դϴ�.
	*/

	if (NULL == psckEx)
		return;

	shutdown(psckEx->sck, 0x02); // SD_BOTH = 0x02
	closesocket(psckEx->sck);

	memset(psckEx, 0, sizeof(SOCKETEX));
}

VOID
CloseSocket(
	SOCKET sck
)
{
	/*
		������ �ݴ� �Լ� �Դϴ�.
	*/

	shutdown(sck, 0x02); // SD_BOTH = 0x02
	closesocket(sck);
}

DWORD WINAPI 
ListenSocket(
	LPVOID psck
)
{
	/*
		������ ������ Ŭ���̾�Ʈ�� ������ ó���� ���ִ� ������ �Դϴ�.
	*/

	SOCKET			mySocket = (SOCKET)psck;

	UINT			Index;
	SOCKADDR_IN		sckaddr;
	INT				addrlen = sizeof(SOCKADDR);
	BOOLEAN			FullSock = FALSE;

	while (_WebServStatus_)
	{
		// Ŭ���̾�Ʈ ���Ϲ迭�� ������� �ִ��� Ȯ���ϰ� ������ ���� Ŭ���̾�Ʈ ���� �ÿ� ����մϴ�.
		for (Index = 0; (Index < MAX_USER) && (NULL != ClientSck[Index].sck); ++Index);

		if (Index >= MAX_USER - 1)
			FullSock = TRUE;

		// Ŭ���̾�Ʈ�� ��������� �մϴ�.
		if (SOCKET_ERROR == (ClientSck[Index].sck = accept(mySocket, (LPSOCKADDR)&sckaddr, (LPINT)&addrlen)))
			break;

		// Ŭ���̾�Ʈ ���� �迭�� ��á�ٸ�
		if (FullSock)
		{
			CloseSocket(ClientSck[Index].sck);
			FullSock = FALSE;

			continue;
		}

		ClientSck[Index].ip = sckaddr.sin_addr;
		ClientSck[Index].Type = SOCK_HTTP;
		ClientSck[Index].RcvPong = 0;

		// Ŭ���̾�Ʈ ���� �����带 ������ ó���� �մϴ�.
		CloseHandle(CreateThread(NULL, 0, DataProc, (LPVOID)(ClientSck + Index), 0, NULL));
	}

	WSACleanup();

	_WebServStatus_ = WEBSERV_END;
	return FALSE;
}

BOOLEAN 
StartWebServer(
	WORD			wSrvPort,
	INT				send_timeout,
	INT				recv_timeout,
	DATAPROCESSPROC ProcessProc
)
{
	/*
		������ �����ϴ� �Լ� �Դϴ�.
	*/

	if (INVALID_SOCKET != ServerSock)
		return FALSE;

	if (INVALID_SOCKET == (ServerSock = InitializeSocket()))
		return FALSE;
		
	if( FALSE == BindAndListenSocket(ServerSock, wSrvPort, send_timeout, recv_timeout))
		return FALSE;

	DataProc = ProcessProc;
	_WebServStatus_ = WEBSERV_START;
	CloseHandle(CreateThread(NULL, 0, ListenSocket, (LPVOID)ServerSock, 0, NULL));

	return TRUE;
}

BOOLEAN
StopWebServer(
)
{
	/*
		������ �����ϴ� �Լ� �Դϴ�.
	*/

	_WebServStatus_ = WEBSERV_STOP;
	CloseSocket(ServerSock);

	while (_WebServStatus_ != WEBSERV_END)
		Sleep(500);
	
	_WebServStatus_ = WEBSERV_STOP;
	ServerSock = INVALID_SOCKET;

	return TRUE;
}

BOOLEAN 
ReadSocketHeader(
	SOCKET sck, 
	MUST_FREE_MEMORY LPSTR &Buffer,
	LPSTR &pEnd, 
	LPDWORD lpContentRead
)
{
	/*
		Ŭ���̾�Ʈ�κ��� HTTP ����� �д� �Լ� �Դϴ�.
	*/

	LONG		bytesReceived = 0;
	DWORD		amountRead = 0;

	Buffer = (LPSTR)malloc(sizeof(CHAR) * (MAX_HDRBUF + 1));

	if (NULL == Buffer)
		return FALSE;

	do
	{
		if (0 >= (bytesReceived = recv(sck, Buffer + amountRead, MAX_HDRBUF - amountRead, 0)) )
			return FALSE;

		amountRead += bytesReceived;

		if (pEnd = strstr(Buffer, "\r\n\r\n"))
		{
			*pEnd = NULL;
			pEnd += 4;
			break;
		}

	} while (MAX_HDRBUF > bytesReceived);


	Buffer[amountRead] = NULL;

	// ����� �д� ���߿� Content ������� ���� ��� lpContentRead ���� ���� ����Ʈ ���� �Է��մϴ�.
	if (lpContentRead)
		*lpContentRead = amountRead - (pEnd - Buffer);

	return TRUE;
}

DWORD
ReadSocketContent(
	SOCKET sck,
	LPSTR Buffer,
	DWORD ReadSize,
	QWORD &ReadPos,
	QWORD ContentLength
)
{
	/*
		Ŭ���̾�Ʈ�κ��� Content�� �дµ� ����ϴ� �Լ� �Դϴ�.
	*/


	LONG		bytesReceived = 0;
	DWORD		amountRead = 0;

	if (ReadPos + ReadSize >= ContentLength)
		ReadSize = (DWORD)(ContentLength - ReadPos);

	do
	{
		if (0 >= (bytesReceived = recv(sck, Buffer + amountRead, ReadSize - amountRead, 0)))
			return FALSE;

		amountRead += bytesReceived;

	} while (ReadSize > amountRead);

	ReadPos += amountRead;

	return amountRead;
}

BOOLEAN
GetHostIP(
	HWND	hComboBox
)
{
	/*
		����(ȣ��Ʈ) �����Ǹ� ���ϴ� �Լ� �Դϴ�.
	*/

	CHAR		hostn[MAX_PATH];
	CHAR		IP[50] = { 0 };
	IN_ADDR		addr = { 0 };
	ADDRINFO	hints;
	ADDRINFO	*result = NULL;
	WSADATA		wsaData;
	BOOLEAN		bResult = FALSE;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR)
		return FALSE;

	ZeroMemory(&hints, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (SOCKET_ERROR == gethostname(hostn, MAX_PATH))
		goto RELEASE;

	if (getaddrinfo(hostn, "80", &hints, &result))
		goto RELEASE;

	SendMessage(hComboBox, CB_ADDSTRING, (WPARAM)sizeof(IP), (LPARAM)"127.0.0.1");

	for (addrinfo *ptr = result; ptr; ptr = ptr->ai_next)
	{
		if (AF_INET == ptr->ai_family) //IPv4
		{
			inet_ntop(AF_INET, 
				(LPVOID)&((PSOCKADDR_IN)ptr->ai_addr)->sin_addr,
				IP,
				sizeof(IP)
			);

			SendMessage(hComboBox, CB_ADDSTRING, (WPARAM)sizeof(IP), (LPARAM)IP);
		}
	}

	bResult = TRUE;

RELEASE:

	WSACleanup();

	return bResult;
}