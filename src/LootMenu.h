// QuickLoot IE LE - the loot menu itself (Scaleform, UI side).
//
// Port of QuickLoot IE's LootMenu (+ the CLIK wrappers it used). Registered as the
// menu "LootMenu" with movie Interface\LootMenuIE.swf; it only reads the view the
// game task publishes (Loot::CopyView) and turns input actions into requests.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE (Miss Corruption and
// contributors). GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

namespace QL
{
	namespace Menu
	{
		void Register();		// kDataLoaded, game thread
		int  SwfFeatureLevel();	// -1 until a movie was loaded
		void ApplyPending();	// UI thread (SKSE UI task): draw a newly published view now
	}
}
