// QuickLoot IE LE - the fault guard (rules\FATAL.md F4).
//
// Every entry point the engine calls into runs inside Guard::Run. A fault is logged
// with its exact address, the controls QuickLoot may hold are handed back, and after
// three faults the mod switches itself off for the session instead of limping.
// No C++ object with a destructor lives in a guarded frame.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

namespace QL
{
	namespace Guard
	{
		enum Domain { kGame, kUI, kInput, kHook, kKhaos, kDomainCount };

		typedef void (*Body)();
		bool Run(const char * what, Body body, Domain domain);	// false when it faulted or is disabled
		bool Disabled();
		bool DomainDisabled(Domain d);
		unsigned Faults();

		// For bodies that need an argument: the caller passes a context through this slot.
		void SetCleanup(Body cleanup);	// run (guarded) when the mod switches itself off
	}
}
