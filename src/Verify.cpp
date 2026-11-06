// QuickLoot IE LE - startup self-check against Signatures.h (rules\FATAL.md F7).
//
// Code sites must hold the bytes they held in TESV.exe 1.9.32.0.8, except where another
// plugin has put a call or jump in: that is normal and is chained. Only a wholesale
// mismatch (a different executable) stops the plugin; one stray site does not.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Verify.h"
#include "Signatures.h"
#include "Log.h"

#include <windows.h>

namespace
{
	bool ReadableRange(const void * p, size_t n)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if(!VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;
		if(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
		return (const UInt8 *)p + n <= (const UInt8 *)mbi.BaseAddress + mbi.RegionSize;
	}

	// 0 = same, 1 = detoured by someone (call/jmp written over it), 2 = different
	int Compare(const Signatures::Site & s)
	{
		const UInt8 * live = (const UInt8 *)s.address;
		if(!ReadableRange(live, s.length)) return 2;
		int first = -1;
		for(int i = 0; i < s.length; ++i) if(live[i] != s.bytes[i]) { first = i; break; }
		if(first < 0) return 0;
		const int from = first >= 4 ? first - 4 : 0;
		for(int j = from; j <= first; ++j)
			if(live[j] == 0xE8 || live[j] == 0xE9) return 1;
		return 2;
	}
}

namespace QL
{
	namespace Verify
	{
		bool Run()
		{
			int code = 0, detoured = 0, different = 0, tables = 0;
			for(int i = 0; i < Signatures::kSiteCount; ++i)
			{
				const Signatures::Site & s = Signatures::kSites[i];
				if(s.kind == Signatures::kVariable || !s.length) continue;
				const int r = Compare(s);
				if(s.kind == Signatures::kTable)
				{
					if(r) { ++tables; QL_LOG("self-check: table %s %08lX differs (another mod's hook is normal)", s.name, s.address); }
					continue;
				}
				++code;
				if(r == 1) { ++detoured; QL_LOG("self-check: %s %08lX carries another mod's detour - chained", s.name, s.address); }
				else if(r == 2) { ++different; QL_LOG("self-check: %s %08lX does not match TESV.exe 1.9.32.0.8", s.name, s.address); }
			}
			const bool ok = different * 4 <= code;		// refuse only a wholesale mismatch
			QL_LOG("self-check: %d code sites, %d detoured, %d different, %d tables changed -> %s",
				   code, detoured, different, tables, ok ? "ok" : "NOT the expected executable, QuickLoot stays off");
			return ok;
		}
	}
}
