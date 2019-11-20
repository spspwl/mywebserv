#include "unit.h"

VOID 
SizeToUnit(
	LPSTR dst, 
	size_t dstlen,
	DWORD High, 
	DWORD Low
)
{
	/*
		파일 사이즈를 인간이 정의한 용량단위로 변환시켜주는 함수 입니다.
	*/


	BOOLEAN				_else = TRUE;
	unsigned long long	qword, seg;

	LPCSTR Unit[] = { "KB", "MB", "GB", "TB", "PB", "EB" };

	qword = High << 28;
	qword |= Low;
	seg = 0x1000000000000000;

	for (int i = 5; i >= 0; --i, seg /= 1024)
	{
		if (qword / seg > 0)
		{
			sprintf_s(dst, dstlen, "%.2f %s", (double)qword / seg, Unit[i]);
			_else = FALSE;
			return;
		}
	}

	if (_else)
		sprintf_s(dst, dstlen, "%d   B", Low);
}

DWORD
StrCount(
	LPCSTR Str,
	LPCSTR Substr
)
{
	/*
		문자열 안에 특정 문자열이 몇개나 들어가 있는지 알려주는 함수 입니다.
	*/

	DWORD len;
	DWORD count = 0;
	LPSTR sch = (LPSTR)Str;


	if (NULL == Str ||
		NULL == Substr ||
		0 == (len = strlen(Substr)) ||
		0 == strlen(Str))
		return 0;

	while (sch = strstr(sch, Substr))
	{
		++count;
		sch += len;
	}

	return count;
}

DWORD
Replace(
	LPSTR &Dst,
	LPSTR Src, 
	LPCSTR find, 
	LPCSTR replace
) 
{
	/*
		문자열 안에 특정 문자열을 다른 문자열로 바꿔주는 함수 입니다.
	*/

	LPSTR sch;
	DWORD count = 0;
	DWORD len = 0;
	DWORD findLen = strlen(find); 
	DWORD repLen = strlen(replace);
	UINT Index = 0;

	Dst = Src;

	if (0 == findLen ||
		0 == repLen)
		return FALSE;

	Index = strlen(Src);

	if (findLen != repLen)
	{
		sch = Src;

		while (sch = strstr(sch, find))
		{
			++count;
			sch += findLen;
		}

		if (0 == count)
			return FALSE;
	}

	len = Index + count * (repLen - findLen);

	if (NULL == (Dst = (LPSTR)malloc(len + 1)))
		return FALSE;

	Index = 0;
	sch = Dst;

	while (*Src)
	{
		if (0 == memcmp(Src, find, findLen))
		{
			memcpy(sch, replace, repLen);
			sch += repLen;
			Src += findLen;
		}
		else
		{
			*sch++ = *Src++;
		}
	}

	*sch = NULL;

	return len;
}

DWORD 
CovTag(
	LPSTR &Dst
)
{
	/*
		문자열 안에 태그 관련 문자가 들어가 있으면 변환 시켜주는 함수 입니다.
	*/

	LPSTR Message = Dst;
	LPSTR CovMsg;

	DWORD Len = strlen(Dst);
	DWORD res;

	if (res = Replace(CovMsg, Message, "<", "&lt;"))
	{
		free(Message);
		Len = res;
	}

	if (res = Replace(Message, CovMsg, ">", "&gt;"))
	{
		free(CovMsg);
		Len = res;
	}

	Dst = Message;
	return Len;
}

VOID
MakeRandomString(
	LPSTR Session,
	DWORD RandomSize
)
{
	/*
		아무 의미없는 문자열을 생성해주는 함수 입니다.
	*/

	LPCSTR randomchars = "abcdefghijklmnopqrstuvwxyz0123456789";
	UINT Index = 0;

	srand((UINT)GetTickCount());

	for (; Index < RandomSize; ++Index)
		Session[Index] = randomchars[rand() % 36];

	Session[Index] = NULL;
}

INT CALLBACK 
BrowseCallbackProc(
	HWND hwnd, 
	UINT uMsg, 
	LPARAM lParam,
	LPARAM lpData
)
{
	/*
		폴더 찾아보기 다이얼로그의 프로시져 입니다.
	*/

	if (uMsg == BFFM_INITIALIZED)
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)lpData);

	return 0;
}

BOOLEAN 
BrowserFolder(
	HWND		hWndOwner,
	LPCSTR		DialogTitle,
	LPSTR		ResultPath,
	size_t		PathLen
)
{
	/*
		폴더 찾아보기 다이얼로그를 띄워주는 함수입니다.
	*/

	LPITEMIDLIST	pidlBrowse;
	BROWSEINFO		brInfo = { 0 };

	brInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT | BIF_VALIDATE;
	brInfo.hwndOwner = hWndOwner;
	brInfo.pszDisplayName = ResultPath;
	brInfo.lpszTitle = DialogTitle;
	brInfo.lpfn = BrowseCallbackProc;
	brInfo.lParam = (LPARAM)ResultPath;

	memset(ResultPath, 0, PathLen);

	if (pidlBrowse = SHBrowseForFolder(&brInfo))
	{
		SHGetPathFromIDList(pidlBrowse, ResultPath);
		return TRUE;
	}

	return FALSE;
}

VOID
RemoveSpecialChar(
	LPSTR szName,
	size_t len
)
{
	/*
		파일 이름에 들어가면 안되는 문자들을 걸러주는 함수 입니다.
	*/

	LPCSTR SpecialChar = "\\/:*?\"<>|";
	UINT c = 0;

	for (UINT i = 0; i < len; ++i)
		for (UINT j = 0; j < 9; ++j)
			if (szName[i] == SpecialChar[j])
			{
				j = 0;
				++c;
				for (UINT k = i; k < len - 1; ++k)
					szName[k] = szName[k + 1];
			}

	szName[len - c] = NULL;
}