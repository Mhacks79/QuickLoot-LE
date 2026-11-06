// QuickLoot IE LE - the fault guard. See Guard.h.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Guard.h"
#include "Log.h"

#include <windows.h>

namespace
{
	volatile LONG	s_faults[QL::Guard::kDomainCount];
	volatile LONG	s_total = 0;
	volatile LONG	s_disabled = 0;
	QL::Guard::Body	s_cleanup = 0;
	enum { kMaxFaults = 3 };

	LONG Report(const char * what, EXCEPTION_POINTERS * info)
	{
		const EXCEPTION_RECORD * r = info ? info->ExceptionRecord : 0;
		if(r)
		{
			const ULONG_PTR addr = r->NumberParameters >= 2 ? r->ExceptionInformation[1] : 0;
			QL::Log::Write("FAULT in %s: code %08lX at %p, data address %08lX, thread %lu",
						   what, r->ExceptionCode, r->ExceptionAddress, (unsigned long)addr, GetCurrentThreadId());
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}

	bool RawRun(const char * what, QL::Guard::Body body)
	{
		__try
		{
			body();
			return true;
		}
		__except(Report(what, GetExceptionInformation()))
		{
			return false;
		}
	}
}

namespace QL
{
	namespace Guard
	{
		bool Run(const char * what, Body body, Domain domain)
		{
			if(s_disabled || s_faults[domain] >= kMaxFaults) return false;
			if(RawRun(what, body)) return true;
			InterlockedIncrement(&s_faults[domain]);
			const LONG total = InterlockedIncrement(&s_total);
			if(s_cleanup) RawRun("cleanup after a fault", s_cleanup);
			if(total >= kMaxFaults && InterlockedExchange(&s_disabled, 1) == 0)
				Log::Write("QuickLoot IE LE has faulted %ld times: it is off for the rest of this session", total);
			return false;
		}

		bool Disabled()					{ return s_disabled != 0; }
		bool DomainDisabled(Domain d)	{ return s_disabled != 0 || s_faults[d] >= kMaxFaults; }
		unsigned Faults()				{ return (unsigned)s_total; }
		void SetCleanup(Body cleanup)	{ s_cleanup = cleanup; }
	}
}
