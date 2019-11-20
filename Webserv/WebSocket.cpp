#include "WebSocket.h"

extern SOCKETEX		ClientSck[MAX_USER];		// 클라이언트 소켓 배열입니다.
DWORD				ClientCount = 0;			// 클라이언트 접속 카운트 변수입니다.
BOOLEAN				CreatePingThread = FALSE;	// 핑 보내는 스레드가 생성되었는지 확인하는 변수 입니다.

BOOLEAN
WebSocket_Send(
	PSOCKETEX		psckEx,
	LPSTR			Buffer,
	DWORD			Len
);

VOID
WebSocket_SendData(
	PSOCKETEX		psckEx,
	_Opcode			PayloadType,
	LPCSTR			Buffer,
	QWORD			BufSize
);

VOID
WebSocket_NewConnection(
	PSOCKETEX		psckEx
);

VOID
WebSocket_Message(
	PSOCKETEX		psckEx,
	LPCSTR			Msg,
	DWORD			Len
);

DWORD WINAPI
WebSocket_Ping(
	LPVOID			arg
)
{
	/*
		1개 이상의 클라이언트가 접속 시 60 초마다 핑을 보내며 PONG 패킷이 5초
		이내로 답신하지 않는 클라이언트는 응답 없음으로 간주하고 접속 종료 시키며
		클라이언트가 하나도 없을 시 자동으로 스레드가 종료됩니다.
	*/

	BOOLEAN			Found;
	DWORD			Index;

	while (ClientCount)
	{
		Sleep(60000);

		Found = FALSE;
		for (Index = 0; Index < MAX_USER; ++Index)
		{
			if (ClientSck[Index].Type == SOCK_WEBSOCK &&
				ClientSck[Index].sck > 0)
			{
				// 핑 패킷을 보냈으니 PONG을 받을 준비를 한다.
				WebSocket_SendData(ClientSck + Index, UTF_8, "PING", 4);
				ClientSck[Index].RcvPong = 1; // PONG 패킷 수신여부를 결정짓는 변수이다.
				Found = TRUE; // 접속한 클라이언트를 발견했으니 핑스레드는 종료하지 않는다.
			}
		}
		
		if (FALSE == Found)
			break;

		Sleep(5000);

		for (Index = 0; Index < MAX_USER; ++Index)
		{
			if (ClientSck[Index].Type == SOCK_WEBSOCK &&
				ClientSck[Index].sck > 0 &&
				ClientSck[Index].RcvPong)
			{
				// 클라이언트로부터 PONG 패킷을 받지 않았으므로 종료한다,
				CloseSocket(ClientSck + Index);
			}
		}

	}

	CreatePingThread = FALSE;
	return FALSE;
}

BOOLEAN 
WebSocket_HandShake(
	PSOCKETEX		psckEx, 
	pHdr			pHeader
)
{
	/*
		웹 소켓은 HTTP(80) 와 같은 포트를 쓰며, 두 프로토콜 간의 차이점은 'Upgrade: websocket'라는 헤더의 유무입니다.
		서버는 클라이언트로부터 웹소켓 연결 요청 헤더를 받을 때, Sec-WebSocket-Key 헤더의 값을 파싱 해 매직 GUID를 붙인
		값에 SHA1 해시 작업을 하여 base64 인코딩을 마치고 그 결괏값을 클라이언트에게 전송하며 클라이언트는 서버로부터 
		받은 base64 인코딩 값이 올바른지 검사해 Handshake 가 완료됩니다.
	*/

	CHAR			Key[70] = { 0 };
	CHAR			Result[100] = { 0 };
	CHAR			Hdr[300];
	LPSTR			pValue = NULL;
	LONG			bytesTransferred;
	DWORD			Len;
	DWORD			TotalTrnasferred;
	time_t			_time;
	struct tm		_tm;
	INT				nTimeOutValue = 3000;

	if (NULL == psckEx)
		return FALSE;

	// 1. HTTP 헤더에서 base64( 임의의 문자열 16바이트 ) 인코딩 된 Key 값을 파싱합니다.
	if (NULL == (pValue = FindHeaderValue(pHeader, "Sec-WebSocket-Key")))
		return FALSE;

	Len = strlen(pValue);
	memcpy(Key, pValue, Len);

	// 2. Key 값에 GUID(Magic String)값을 더해, 더한값을 SHA1 Hash 합니다.
	memcpy(Key + Len, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36);
	Len += 36;

	SHA1(Result, Key, Len);

	// 3. SHA1 Hash 된 값을 2바이트 씩 쪼갠다음 해당 Hex값을 -> Decimal로 변환 후 Ascii 로 변환합니다. ( 이미 끝냄 )
	// 4. 3. 의 결과 값을 base64 인코딩 합니다.
	memset(Key, 0, sizeof(Key));
	__base64_encode(Key, Result, 20);

	Len = sprintf_s(Hdr, sizeof(Hdr), "HTTP/1.1 %s\n", REQUEST_HEADER_101);
	time(&_time);	// 현재 시간을 불러옵니다.
	gmtime_s(&_tm, &_time); // 현재 시간을 GMT 시간으로 바꿔줍니다.
	strftime(Result, sizeof(Result), "%a, %d %b %Y %T", &_tm); // HTTP 헤더에 알맞는 시간 포맷으로 변환시킵니다.
	Len += sprintf_s(Hdr + Len, sizeof(Hdr) - Len, "Date: %s GMT\n", Result);
	Len += sprintf_s(Hdr + Len, sizeof(Hdr) - Len, "Connection: Upgrade\n");
	Len += sprintf_s(Hdr + Len, sizeof(Hdr) - Len, "Upgrade: websocket\n");
	Len += sprintf_s(Hdr + Len, sizeof(Hdr) - Len, "Sec-WebSocket-Accept: %s\n", Key);
	Len += sprintf_s(Hdr + Len, sizeof(Hdr) - Len, "Server: %s\n\n", SERVER_NAME);

	bytesTransferred = 0;
	TotalTrnasferred = 0;

	while (TotalTrnasferred < Len)
	{
		if (SOCKET_ERROR == (bytesTransferred = send(psckEx->sck, Hdr + TotalTrnasferred, (LONG)Len - TotalTrnasferred, 0)))
			return FALSE;

		TotalTrnasferred += bytesTransferred;
	}

	psckEx->Type = SOCK_WEBSOCK;
	
	if (SOCKET_ERROR == setsockopt(psckEx->sck, SOL_SOCKET, SO_SNDTIMEO, (const char*)&nTimeOutValue, sizeof(nTimeOutValue)))
		return FALSE;

	if (SOCKET_ERROR == setsockopt(psckEx->sck, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeOutValue, sizeof(nTimeOutValue)))
		return FALSE;

	// 닉네임(세션)의 메모리 공간을 할당하는 부분입니다.
	if (NULL == (psckEx->Session = (LPSTR)malloc(sizeof(CHAR) * (WEBSERV_NICKLEN + 1))))
		return FALSE;

	MakeRandomString(psckEx->Session, WEBSERV_NICKLEN);
	WebSocket_NewConnection(psckEx);

	if (FALSE == CreatePingThread)
	{
		CreatePingThread = TRUE;
		CreateThread(NULL, 0, WebSocket_Ping, NULL, 0, NULL);
	}

	return TRUE;
}

BOOLEAN
WebSocket_Read(
	PSOCKETEX		psckEx,
	MUST_FREE_MEMORY LPSTR &Buffer,
	DWORD			ReadSize,
	BOOLEAN			AutoFree
)
{
	LPSTR			lpszbuf = NULL;
	LONG			bytesReceived = 0;
	DWORD			amountReads = 0;
	myfd_set		fd;
	struct timeval	timeout;

	if (NULL == psckEx)
		return FALSE;

	fd.fd_array[0] = psckEx->sck;

	timeout.tv_sec = 1;
	timeout.tv_usec = 1000;

	if (AutoFree)
	{
		FREE_DATA(Buffer);

		 // 문자열은 무조건 길이 + 1 만큼 할당 해줘야한다. (NULL 문자)
		if (NULL == (lpszbuf = (LPSTR)malloc(sizeof(CHAR) * (ReadSize + 1))))
			return FALSE;

		Buffer = lpszbuf;
	}
	else
	{
		lpszbuf = Buffer;
	}

	do
	{
		fd.fd_count = 1;
		int res = select(1, (fd_set*)&fd, NULL, NULL, &timeout); // 소켓에 읽을 데이터가 있는지 관찰한다

		if (res > 0) // 읽을 데이터가 있으면,
		{
			if ((bytesReceived = recv(psckEx->sck, lpszbuf + amountReads, ReadSize - amountReads, 0)) == SOCKET_ERROR)
			{
				// 에러처리

				if (WSAGetLastError() != WSAETIMEDOUT) // WSAETIMEDOUT : 시간의 경과가 SO_RCVTIMEO 의 값을 넘어섰기 때문에, 예외로 둔다.
					goto RELEASE;

				continue;
			}
			else if (bytesReceived == 0) // 소켓 버퍼에 읽을 데이터가 있음에도 불구하고, 읽은 데이터의 수가 0이면 접속종료로 판단
				goto RELEASE;

			amountReads += bytesReceived;
		}
		else if (res < 0)
			goto RELEASE;

	} while (ReadSize > amountReads);

	return TRUE;

RELEASE:

	if (AutoFree)
		FREE_DATA(Buffer);

	return FALSE;

}

BOOLEAN
WebSocket_Send(
	PSOCKETEX		psckEx,
	LPSTR			Buffer,
	DWORD			Len
)
{
	DWORD			TotalTransferred = 0;
	LONG			bytesTransferred = 0;
	BOOLEAN			bResult = FALSE;

	if (NULL			== psckEx ||
		INVALID_SOCKET	== psckEx->sck ||
		0				== Len)
		return FALSE;

	while (TotalTransferred < Len)
	{
		if (0 >= (bytesTransferred = send(psckEx->sck, Buffer + TotalTransferred, Len - TotalTransferred, 0)))
			goto RELEASE;

		TotalTransferred += bytesTransferred;
	}

	bResult = TRUE;

RELEASE:

	return bResult;
}

VOID 
WebSocket_SendData(
	PSOCKETEX		psckEx,
	_Opcode			PayloadType, 
	LPCSTR			Buffer, 
	QWORD			BufSize
)
{
	/*
		https://tools.ietf.org/html/rfc6455

		 0                   1                   2                   3
		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-------+-+-------------+-------------------------------+
		|F|R|R|R| opcode|M| Payload len |    확장된 payload length      |
		|I|S|S|S|  (4)  |A|     (7)     |            (16/64)            |
		|N|V|V|V|       |S|             |   (만약 payload len 의 값이   |
		| |1|2|3|       |K|             |		 126 or 127 이면)       |
		+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
		|		확장된 payload length (payload len 의 값이 127 이면)	|
		+ - - - - - - - - - - - - - - - +-------------------------------+
		|		확장된 payload length    |	   마스킹 키 ( 4 바이트 )	|
		+-------------------------------+-------------------------------+
		|			마스킹 키            |			  데이터		    |
		+-------------------------------- - - - - - - - - - - - - - - - +
		:							  데이터  ...						:
		+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
		|							  데이터  ...						|
		+---------------------------------------------------------------+

		FIN :			수신된 패킷 뒤에 다른 패킷이 없다는걸 의미하는 플래그입니다.

		RSV 1 ~ 3 :		미래를 위해 예약된 플래그 입니다. ( 기본값은 0 입니다. )

		opcode :		수신된 데이터의 종류를 나타내는 플래그 입니다. ( UTF-8 : 1, Binary : 2 )

		MASK :			이 값이 1 이면, 수신자는 마스킹 키값을 이용해 데이터에 xor 연산을 하여
						실제 데이터를 얻을 수 있습니다. 0 이면, 헤더에 마스킹 키가 포함되지 않습니다.

		Payload len :	이 값이 125 이하면, 이 값은 데이터의 길이를 나타냅니다.
						126 이면, 데이터의 길이는 16 ~ 31 ( 2 바이트 ) 의 값이 됩니다.
						127 이면, 데이터의 길이는 16 ~ 79 ( 8 바이트 ) 의 값이 됩니다.
	*/

	sckDataFrame	Frame;
	CHAR			Hdr[8];
	DWORD			HdrLen = 0;
	LPSTR			sndData = NULL;

	if (NULL == psckEx)
		return;

	Frame.FIN = TRUE; // 데이터의 끝을 나타내는 플래그 입니다.
	Frame.OpCode = PayloadType; // 데이터의 종류를 나타내는 값 입니다.
	Frame.MASK = FALSE; // Client->Server 로 전달되는 메세지는 이 값이 항상 TRUE 어야 하며, 
	// 이 값이 참이면 별도의 과정을 통해 Pure 한 데이터를 얻을 수 있습니다.
	Frame.RSV = 0; // 미래를 위해 예약된 플래그 입니다.

	if (UTF_8 == PayloadType)
	{
		if (0 == (BufSize = ANSItoUTF8(Buffer, sndData)))
			return;

		--BufSize;
	}
	else
	{
		sndData = (LPSTR)Buffer;
	}

	// Payload len을 구하는 코드 입니다.
	if (BufSize < 0x7E)
	{
		Frame.PayloadLength = (BYTE)BufSize;
	}
	else if (BufSize >= 0x7E && BufSize <= 0xFFFF)
	{
		Frame.PayloadLength = 126;

		Hdr[0] = (BufSize >> 8) & 0xFF;
		Hdr[1] = BufSize & 0xFF;

		HdrLen = 0x02;
	}
	else
	{
		Frame.PayloadLength = 127;

		for (UINT i = 0; i < 8; ++i)
			Hdr[i] = (BufSize >> ((7 - i) * 8)) & 0xFF;

		HdrLen = 0x08;
	}

	WebSocket_Send(psckEx, (LPSTR)&Frame.Data, 2);

	if (HdrLen)
		WebSocket_Send(psckEx, Hdr, HdrLen);

	WebSocket_Send(psckEx, sndData, (INT)BufSize);

	if (UTF_8 == PayloadType)
		free(sndData);
}

VOID 
WebSocket_ReceiveData(
	PSOCKETEX		psckEx
)
{
	/*
		https://tools.ietf.org/html/rfc6455

		 0                   1                   2                   3
		 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-------+-+-------------+-------------------------------+
		|F|R|R|R| opcode|M| Payload len |    확장된 payload length      |
		|I|S|S|S|  (4)  |A|     (7)     |            (16/64)            |
		|N|V|V|V|       |S|             |   (만약 payload len 의 값이   |
		| |1|2|3|       |K|             |		 126 or 127 이면)       |
		+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
		|		확장된 payload length (payload len 의 값이 127 이면)	|
		+ - - - - - - - - - - - - - - - +-------------------------------+
		|		확장된 payload length    |	   마스킹 키 ( 4 바이트 )	|
		+-------------------------------+-------------------------------+
		|			마스킹 키            |			  데이터		    |
		+-------------------------------- - - - - - - - - - - - - - - - +
		:							  데이터  ...						:
		+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
		|							  데이터  ...						|
		+---------------------------------------------------------------+

		FIN :			수신된 패킷 뒤에 다른 패킷이 없다는걸 의미하는 플래그입니다.

		RSV 1 ~ 3 :		미래를 위해 예약된 플래그 입니다. ( 기본값은 0 입니다. )

		opcode :		수신된 데이터의 종류를 나타내는 플래그 입니다. ( UTF-8 : 1, Binary : 2 )

		MASK :			이 값이 1 이면, 수신자는 마스킹 키값을 이용해 데이터에 xor 연산을 하여
						실제 데이터를 얻을 수 있습니다. 0 이면, 헤더에 마스킹 키가 포함되지 않습니다.

		Payload len :	이 값이 125 이하면, 이 값은 데이터의 길이를 나타냅니다.
						126 이면, 데이터의 길이는 16 ~ 31 ( 2 바이트 ) 의 값이 됩니다.
						127 이면, 데이터의 길이는 16 ~ 79 ( 8 바이트 ) 의 값이 됩니다.



		WebSocket의 PayloadLength를 나타내는 패킷은 0 ~ 18,446,744,073,709,551,615(2^64-1) 까지 지원하는데
		이 프로그램은 간단하게 4바이트만(2^32-1) 사용하여 프로그래밍 되었습니다.
	*/



	CHAR			MaskData[4];			// 마스킹 키 값을 보관하는 배열 입니다.
	LPSTR			Packet = NULL;			// 수신된 패킷의 메모리 주소를 담는 변수입니다.
	LPSTR			RealPayload = NULL;		// 
	LPSTR			MaskPacket = NULL;		//
	DWORD			PayloadLength;			// 수신해야할 데이터의 길이를 담는 변수 입니다.
	DWORD			RealPayloadLength = 0;  // 
	sckDataFrame	Frame;
	sckDataFrame	Frag;
	BOOLEAN			Continuous = FALSE;

	MaskPacket = MaskData;

	while (psckEx->sck)
	{
		PayloadLength = 0;
		Packet = (LPSTR)&Frame.Data;

		if (FALSE == WebSocket_Read(psckEx, Packet, 2, FALSE)) // No Alloc
			return;

		Packet = NULL;

		if (Close == Frame.OpCode)
			return;

		
		if (126 > Frame.PayloadLength)
		{
			PayloadLength = Frame.PayloadLength;
		}
		else if (126 == Frame.PayloadLength)
		{
			if (FALSE == WebSocket_Read(psckEx, Packet, 2, TRUE)) // Alloc
				return;

			memcpy(&PayloadLength, Packet, 2);
			// 네트워크 바이트 순서를 호스트 바이트 순서로 바꿉니다.
			PayloadLength = (DWORD)ntohs((u_short)PayloadLength);

			FREE_DATA(Packet);
		}
		else if (127 == Frame.PayloadLength)
		{
			if (FALSE == WebSocket_Read(psckEx, Packet, 8, TRUE))
				return;

			memcpy(&PayloadLength, Packet + 4, 4);
			PayloadLength = (DWORD)ntohl(PayloadLength);

			FREE_DATA(Packet);
		}
		else if (0 == Frame.PayloadLength)
		{
			if (Frame.MASK && (FALSE == WebSocket_Read(psckEx, MaskPacket, 4, FALSE)))
				return;

			continue;
		}

		// 채팅 메세지가 4096 바이트가 넘어가면 종료 시킵니다.
		if (PayloadLength > 2048)
			return;

		if (Continuous)
		{
			if (NULL == (RealPayload = (LPSTR)realloc(RealPayload, RealPayloadLength + PayloadLength + 1)))
				return;

			Packet = RealPayload + RealPayloadLength;
		}

		// 마스크 플래그가 1인데도 불구하고 패킷 수신버퍼에 마스킹 키 데이터를 수신할 수 없으면
		if (Frame.MASK && (FALSE == WebSocket_Read(psckEx, MaskPacket, 4, FALSE)))
			return;

		// 실제 데이터를 수신 받습니다.
		if (FALSE == WebSocket_Read(psckEx, Packet, PayloadLength, !Continuous))
			return;

		if (Frame.MASK)
		{
			// 마스킹 키값을 이용해 xor 연산하여 original payload data로 변환하는 코드 입니다.
			for (QWORD Index = 0; Index < PayloadLength; ++Index)
				Packet[Index] ^= MaskData[Index % 4];
		}

		// FIN 플래그 값이 0 이면 
		if (FALSE == Frame.FIN)
		{
			// 뒤에 수신할 메세지가 더 있다면
			if (FALSE == Continuous)
			{
				Continuous = TRUE;
				Frag = Frame;
				RealPayload = Packet;
			}

			RealPayloadLength += PayloadLength;
			continue;
		}

		if (Continuous)
		{
			Continuous = FALSE;
			Frame = Frag;
			Packet = RealPayload;
			PayloadLength += RealPayloadLength;
			RealPayloadLength = 0;
		}

		Packet[PayloadLength] = NULL;

		switch (Frame.OpCode)
		{
			case UTF_8:
			{
				// UTF-8 인코딩된 문자열이 수신되므로 UTF-8 -> ANSI 로 변환해주는 작업이 필요합니다.
				LPSTR Ansi;

				if (FALSE == UTF8toANSI(Packet, Ansi))
				{
					FREE_DATA(Packet);
					return;
				}

				// 클라이언트에게 PING 메세지를 보내 PONG 응답을 받았다면
				if (psckEx->RcvPong && 0 == strncmp(Ansi, "PONG", 4))
					psckEx->RcvPong = 0; // 받았다고 처리한다.
				else // PONG 메세지가 아니면 
					WebSocket_Message(psckEx, Ansi, PayloadLength);

				FREE_DATA(Ansi);
				break;
			}
			case Binary:
				break;
			case Continuation:
				break;
		}

		FREE_DATA(Packet);
	}
}

// WebSocket Events

VOID
WebSocket_NewConnection(
	PSOCKETEX		psckEx
)
{
	CHAR			ConnMsg[100];

	++ClientCount;

	sprintf_s(ConnMsg, sizeof(ConnMsg), 
		"익명_%s 님이 접속하셨습니다. (접속 인원 : %d)",
		psckEx->Session,
		ClientCount
	);

	//BroadCast
	for (UINT Index = 0; Index < MAX_USER; ++Index)
	{
		if (ClientSck[Index].sck && ClientSck[Index].Type == SOCK_WEBSOCK)
		{
			if (psckEx != ClientSck + Index &&
				ClientSck[Index].ip.S_un.S_addr == psckEx->ip.S_un.S_addr)
			{
				// 다중 클라이언트 사용자는 즉시 종료시킨다.
				shutdown(ClientSck[Index].sck, 0x02);
				continue;
			}

			WebSocket_SendData(ClientSck + Index, UTF_8, ConnMsg, 4096);
		}
	}
}

VOID 
WebSocket_ConnectionClose(
	PSOCKETEX		psckEx
)
{	
	CHAR			ConnMsg[100];

	--ClientCount;

	if (NULL == psckEx->Session)
		return;

	sprintf_s(ConnMsg, sizeof(ConnMsg), 
		"익명_%s 님이 접속종료하셨습니다. (접속 인원 : %d)",
		psckEx->Session,
		ClientCount
	);

	FREE_DATA(psckEx->Session);

	//BroadCast
	for (UINT Index = 0; Index < MAX_USER; ++Index)
	{
		if (ClientSck[Index].sck &&
			ClientSck[Index].Type == SOCK_WEBSOCK &&
			psckEx != ClientSck + Index)
		{
			WebSocket_SendData(ClientSck + Index, UTF_8, ConnMsg, 4096);
		}
	}
}

VOID 
WebSocket_Message(
	PSOCKETEX		psckEx,
	LPCSTR			Msg,
	DWORD			Len
)
{
	LPSTR			Message = NULL;
	UINT			spLen;

	//BroadCast
	for (UINT Index = 0; Index < MAX_USER; ++Index)
	{
		if (ClientSck[Index].sck && ClientSck[Index].Type == SOCK_WEBSOCK)
		{
			if (Message = (LPSTR)malloc(sizeof(CHAR) * (Len + WEBSERV_NICKLEN + 10)))
			{
				spLen = sprintf_s(Message, Len + WEBSERV_NICKLEN + 10, "익명_%s: ",
					psckEx->Session
				);

				memcpy(Message + spLen, Msg, Len);
				Message[Len + spLen] = NULL;
				Len = CovTag(Message);

				WebSocket_SendData(ClientSck + Index, UTF_8, Message, Len);
				free(Message);
			}
		}
	}
}

