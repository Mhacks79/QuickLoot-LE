// QuickLoot IE LE - logging. See Log.h.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Log.h"

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdarg.h>

namespace
{
	volatile LONG		s_enabled	= 0;
	CRITICAL_SECTION	s_lock;
	volatile LONG		s_lockReady	= 0;
	HANDLE				s_file		= INVALID_HANDLE_VALUE;
	bool				s_triedOpen	= false;

	// Thread lines already written: what + thread id, a fixed table so that no
	// allocation and no function-local static lives on a logged path (F4).
	struct Seen { const char * what; DWORD thread; };
	Seen	s_seen[32];
	int		s_seenCount = 0;

	void EnsureLock()
	{
		if(InterlockedCompareExchange(&s_lockReady, 1, 0) == 0)
		{
			InitializeCriticalSection(&s_lock);
			InterlockedExchange(&s_lockReady, 2);
		}
		while(s_lockReady != 2) Sleep(0);
	}

	// Under the lock, and only once logging is on.
	void OpenFile()
	{
		if(s_triedOpen) return;
		s_triedOpen = true;
		char path[MAX_PATH];
		if(FAILED(SHGetFolderPathA(NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, path)))
			return;
		strcat_s(path, sizeof(path), "\\My Games\\Skyrim\\SKSE");
		CreateDirectoryA(path, NULL);
		strcat_s(path, sizeof(path), "\\QuickLootIELE.log");
		s_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
}

namespace QL
{
	namespace Log
	{
		void Enable(bool on)	{ InterlockedExchange(&s_enabled, on ? 1 : 0); }
		bool Enabled()			{ return s_enabled != 0; }

		void Write(const char * fmt, ...)
		{
			if(!s_enabled) return;
			EnsureLock();
			char line[1024];
			SYSTEMTIME t;
			GetLocalTime(&t);
			int n = sprintf_s(line, sizeof(line), "[%02u:%02u:%02u.%03u] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
			if(n < 0) n = 0;
			va_list args;
			va_start(args, fmt);
			int m = _vsnprintf_s(line + n, sizeof(line) - n - 3, _TRUNCATE, fmt, args);
			va_end(args);
			if(m < 0) m = (int)strlen(line + n);
			n += m;
			line[n++] = '\r';
			line[n++] = '\n';
			EnterCriticalSection(&s_lock);
			OpenFile();
			if(s_file != INVALID_HANDLE_VALUE)
			{
				DWORD written = 0;
				WriteFile(s_file, line, (DWORD)n, &written, NULL);
			}
			LeaveCriticalSection(&s_lock);
		}

		void Thread(const char * what)
		{
			if(!s_enabled) return;
			EnsureLock();
			const DWORD id = GetCurrentThreadId();
			bool known = false;
			EnterCriticalSection(&s_lock);
			for(int i = 0; i < s_seenCount; ++i)
				if(s_seen[i].what == what) { known = true; break; }
			if(!known && s_seenCount < 32)
			{
				s_seen[s_seenCount].what = what;
				s_seen[s_seenCount].thread = id;
				++s_seenCount;
			}
			LeaveCriticalSection(&s_lock);
			if(!known) Write("%s runs on thread %lu", what, id);
		}
	}
}
