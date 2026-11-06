// QuickLoot IE LE - the KHA-OS settings page (replaces the SE MCM).
//
// Contract: Khaos\KhaosAPI.h, KHAOS_API_VERSION 2. KHA-OS finds the plugin through
// the exported KHAOS_Query and calls the other exports; nothing here needs KHA-OS to
// be installed, and without it the INI is the settings menu.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

namespace QL
{
	namespace Khaos
	{
		void Init();
		void Shutdown();
		void DrawWindow(bool * open);		// the window itself, no device: used by the offline test
	}
}
