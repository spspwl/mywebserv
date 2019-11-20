#include "base64.h"

LPCSTR __base64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

CHAR reverse_table[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
};

VOID 
__base64_encode(
	LPSTR Dst, 
	LPCSTR Src, 
	DWORD length
) 
{
	LPCBYTE current = (LPCBYTE)Src;
	UINT Index = 0;

	while (length > 2) 
	{ 
		Dst[Index++] = __base64_table[current[0] >> 2];
		Dst[Index++] = __base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
		Dst[Index++] = __base64_table[((current[1] & 0x0f) << 2) + (current[2] >> 6)];
		Dst[Index++] = __base64_table[current[2] & 0x3f];

		current += 3;
		length -= 3;
	}

	if (length) 
	{
		Dst[Index++] = __base64_table[current[0] >> 2];
		if (length > 1) 
		{
			Dst[Index++] = __base64_table[((current[0] & 0x03) << 4) + (current[1] >> 4)];
			Dst[Index++] = __base64_table[(current[1] & 0x0f) << 2];
			Dst[Index++] = '=';
		}
		else 
		{
			Dst[Index++] = __base64_table[(current[0] & 0x03) << 4];
			Dst[Index++] = '=';
			Dst[Index++] = '=';
		}
	}

	Dst[Index] = 0;
}

VOID 
__base64_decode(
	LPSTR Dst, 
	LPCSTR Src, 
	INT length
) 
{
	LPCBYTE current = (LPCBYTE)Src;

	INT ch, i = 0, j = 0, k;

	while (ch = *current++)
	{
		if (ch == '=')
			break;

		if (ch == ' ') 
			ch = '+';

		ch = reverse_table[ch];

		if (ch < 0) 
			continue;

		switch (i % 4) 
		{
			case 0:
				Dst[j] = ch << 2;
				break;
			case 1:
				Dst[j++] |= ch >> 4;
				Dst[j] = (ch & 0x0f) << 4;
				break;
			case 2:
				Dst[j++] |= ch >> 2;
				Dst[j] = (ch & 0x03) << 6;
				break;
			case 3:
				Dst[j++] |= ch;
				break;
		}

		i++;
	}

	k = j;

	if (ch == '=') 
	{
		switch (i % 4) 
		{
			case 0:
			case 1:
				return;
			case 2:
				k++;
			case 3:
				Dst[k++] = 0;
		}
	}

	Dst[k] = 0;
}