#pragma once

// used: mem_pad
#include "../../utilities/memory.h"


class IGlobalVars
{
public:
	float flRealTime; //0x0000
	int32_t nFrameCount; //0x0004
	float flFrameTime; //0x0008
	float flFrameTime2; //0x000C
	int32_t nMaxClients; //0x0010
	MEM_PAD(0xC);
	// Everything under this commend is suppose to be in it's own class inside globalvariables,
	// but I'm too lazy to do that, so I'm just going to leave it here.
	MEM_PAD(0x8);
	float flIntervalPerTick; // 0x0008
	float flCurtime; //0x000C
	float flCurtime2; //0x0010
	MEM_PAD(0xC); //0x0034
	__int32 nTickCount; //0x0040
	float flIntervalPerTick2; //0x0044
	void* pCurrentNetChannel; //0x0048

};
