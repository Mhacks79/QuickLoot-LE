// QuickLoot IE LE - settings.
//
// In the Special Edition original these lived in a SkyUI MCM script and were read
// back through Papyrus. Here they live in Data\SKSE\Plugins\QuickLootIELE.ini and are
// edited from the KHA-OS page (or by hand). Defaults are the MCM script's defaults.
//
// Thread model: one copy under a lock. Writers (the KHA-OS page, the loader) change
// it with Edit(); every reader takes a private copy with Copy() and refreshes it
// when Generation() moves. Numbers are parsed and printed by hand (F5): the file
// reads the same on a French or a Russian machine.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE's Config/UserSettings and
// Config/SystemSettings (Miss Corruption and contributors).
// GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

#include <string>
#include <vector>

namespace QL
{
	enum AnchorPoint
	{
		kTopLeft, kCenterLeft, kBottomLeft,
		kTopCenter, kCenter, kBottomCenter,
		kTopRight, kCenterRight, kBottomRight,
		kAnchorCount
	};

	// SKSE key codes, as the MCM stored them: 0-255 keyboard scan codes, 256-265
	// mouse, 266-281 gamepad. -1 = not bound.
	struct KeySetting
	{
		int key;
		int modifier;
	};

	enum KeyAction
	{
		kKeyUse, kKeyTake, kKeyTakeAll, kKeyTransfer, kKeyDisable, kKeyEnable,
		kKeyActionCount
	};

	struct Settings
	{
		// General
		bool showInCombat;
		bool showWhenEmpty;
		bool showWhenSneaking;
		bool showWhenUnlocked;
		bool showInThirdPerson;
		bool showWhenMounted;
		bool enableForContainers;
		bool enableForCorpses;
		bool enableForAnimals;
		bool enableForDragons;
		bool breakInvisibility;
		bool playScrollSound;

		// Window
		int		windowX;
		int		windowY;
		float	windowScale;
		int		windowAnchor;
		int		minLines;
		int		maxLines;
		float	opacityNormal;
		float	opacityEmpty;

		// Display
		bool showItemIcons;
		bool showBestInClass;
		bool showRead;
		bool showEnchanted;
		bool showEnchantedKnown;
		bool showEnchantedSpecial;
		std::vector<std::string> infoColumns;		// value, weight, valuePerWeight

		// Sorting: rule names without the "$qlie_SortRule_" prefix, in priority order
		std::vector<std::string> sortRules;

		// Controls
		KeySetting keyboard[kKeyActionCount];
		KeySetting gamepad[kKeyActionCount];

		// Compatibility
		bool suppressActivationPrompt;
		std::vector<std::string> menuWhitelist;
		std::vector<std::string> containerBlacklist;	// "Plugin.esp|FormID" or a full FormID

		// Debug
		bool enableLog;
	};

	namespace Config
	{
		void		Defaults(Settings & out);
		void		Clamp(Settings & s);			// the one place every value is bounded

		void		Load();							// reads the INI (missing file = defaults)
		bool		Save();							// atomic: temp file, then replace
		void		Copy(Settings & out);
		unsigned	Generation();
		template<class F> void Edit(F f);			// f(Settings&), then Clamp, then generation++

		const char * IniPath();

		// Exposed for the offline tests.
		bool		ParseFloat(const char * text, float & out);
		void		FormatFloat(float value, int decimals, char * out, size_t size);
		void		SplitList(const char * text, std::vector<std::string> & out);

		// Sort rule catalogue (short names) and the default order of the MCM.
		const std::vector<std::string> & AllSortRules();
		const std::vector<std::string> & DefaultSortRules();

		void		EditBegin(Settings *& s);	// lock + pointer
		void		EditEnd(bool changed);		// clamp + generation + unlock
	}

	template<class F> void Config::Edit(F f)
	{
		Settings * s = 0;
		EditBegin(s);
		try { f(*s); }
		catch(...) { EditEnd(false); throw; }
		EditEnd(true);
	}
}
