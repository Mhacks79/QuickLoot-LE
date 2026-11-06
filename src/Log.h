// QuickLoot IE LE - logging.
//
// Silent by default: nothing is written, no file is created and no folder is
// resolved unless [Debug] EnableLog=1 is set in QuickLootIELE.ini. Every call
// returns before doing any work while logging is off.
//
// Copyright (C) 2026 Mhacks79. Part of QuickLoot IE LE, a port of QuickLoot IE
// by Miss Corruption. GPL-3.0-or-later WITH the Modding and Linking exceptions
// (see LICENSE and EXCEPTIONS.md).
#pragma once

namespace QL
{
	namespace Log
	{
		void Enable(bool on);
		bool Enabled();
		void Write(const char * fmt, ...);

		// One line per call site and thread kind, no more: "ticked on thread N".
		void Thread(const char * what);
	}
}

#define QL_LOG(...) do { if(::QL::Log::Enabled()) ::QL::Log::Write(__VA_ARGS__); } while(0)
