// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef _UTILS_H_
#define _UTILS_H_

#ifndef _CBASETYPES_H_
#include "../common/cbasetypes.h"
#endif
#include <stdarg.h>

// Function that dumps the hex of the first num bytes of the buffer to the screen
//#define UTIL_DUMP
#ifdef UTIL_DUMP
void dump(const unsigned char* buffer, int num);
#endif

struct StringBuf
{
	char *buf_;
	char *ptr_;
	unsigned int max_;
};
typedef struct StringBuf StringBuf;

StringBuf* StringBuf_Malloc(void);
void StringBuf_Init(StringBuf* self);
int StringBuf_Printf(StringBuf* self, const char* fmt, ...);
int StringBuf_Vprintf(StringBuf* self, const char* fmt, va_list args);
int StringBuf_Append(StringBuf* self, const StringBuf *sbuf);
int StringBuf_AppendStr(StringBuf* self, const char* str);
int StringBuf_Length(StringBuf* self);
char* StringBuf_Value(StringBuf* self);
void StringBuf_Clear(StringBuf* self);
void StringBuf_Destroy(StringBuf* self);
void StringBuf_Free(StringBuf* self);

void findfile(const char *p, const char *pat, void (func)(const char*));

//Caps values to min/max
#define cap_value(a, min, max) ((a >= max) ? max : (a <= min) ? min : a)

//////////////////////////////////////////////////////////////////////////
// byte word dword access [Shinomori]
//////////////////////////////////////////////////////////////////////////

extern uint8 GetByte(uint32 val, int idx);
extern uint16 GetWord(uint32 val, int idx);
extern uint16 MakeWord(uint8 byte0, uint8 byte1);
extern uint32 MakeDWord(uint16 word0, uint16 word1);

#endif /* _UTILS_H_ */
