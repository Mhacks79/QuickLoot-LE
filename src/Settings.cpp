// QuickLoot IE LE - settings. See Settings.h.
//
// Copyright (C) 2026 Mhacks79. Defaults and sort presets from QuickLoot IE's
// QuickLootIEMCM.psc and Config/Papyrus.h (Miss Corruption and contributors).
// GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Settings.h"
#include "Log.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace
{
	CRITICAL_SECTION	s_lock;
	volatile LONG		s_ready = 0;
	QL::Settings		s_current;
	volatile LONG		s_generation = 1;
	char				s_path[MAX_PATH] = { 0 };

	void EnsureLock()
	{
		if(InterlockedCompareExchange(&s_ready, 1, 0) == 0)
		{
			InitializeCriticalSection(&s_lock);
			QL::Config::Defaults(s_current);
			InterlockedExchange(&s_ready, 2);
		}
		while(s_ready != 2) Sleep(0);
	}

	// Short rule names, in the order of the original's ruleTable.
	const char * const kAllRules[] =
	{
		"Gold", "Gems", "SoulGems", "Lockpicks", "OresIngots", "Potions", "FoodDrinks",
		"Books", "Notes", "Scrolls", "Weapons", "ArrowsBolts", "Armors", "Clothes", "Jewelry",
		"Weightless", "Enchanted", "ByWeight", "ByValue", "ByV/W", "ByName",
	};

	// SortingPresets_Default from Config/Papyrus.h, minus the two rules that only
	// served SE-only integrations (ArtifactNeeded, CompletionistNeeded).
	const char * const kDefaultRules[] =
	{
		"Gold", "Gems", "SoulGems", "Lockpicks", "OresIngots", "Potions", "FoodDrinks",
		"Books", "Notes", "Scrolls", "Weapons", "ArrowsBolts", "Armors", "Clothes", "Jewelry",
		"Weightless", "ByWeight", "ByValue", "ByV/W", "ByName",
	};

	std::vector<std::string> s_allRules;
	std::vector<std::string> s_defaultRules;

	const char * const kActionKeys[QL::kKeyActionCount] = { "Use", "Take", "TakeAll", "Transfer", "Disable", "Enable" };

	void Trim(std::string & s)
	{
		size_t a = 0, b = s.size();
		while(a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
		while(b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
		s = s.substr(a, b - a);
	}

	bool ParseInt(const char * text, int & out)
	{
		if(!text) return false;
		while(*text == ' ' || *text == '\t') ++text;
		bool neg = false;
		if(*text == '-' || *text == '+') { neg = (*text == '-'); ++text; }
		if(*text < '0' || *text > '9') return false;
		long long v = 0;
		while(*text >= '0' && *text <= '9')
		{
			v = v * 10 + (*text - '0');
			if(v > 100000000) return false;
			++text;
		}
		while(*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
		if(*text) return false;
		out = (int)(neg ? -v : v);
		return true;
	}

	bool ParseBool(const char * text, bool & out)
	{
		if(!text) return false;
		while(*text == ' ' || *text == '\t') ++text;
		if(!_strnicmp(text, "1", 1) || !_strnicmp(text, "true", 4) || !_strnicmp(text, "yes", 3) || !_strnicmp(text, "on", 2)) { out = true; return true; }
		if(!_strnicmp(text, "0", 1) || !_strnicmp(text, "false", 5) || !_strnicmp(text, "no", 2) || !_strnicmp(text, "off", 3)) { out = false; return true; }
		return false;
	}

	void JoinList(const std::vector<std::string> & list, std::string & out)
	{
		out.clear();
		for(size_t i = 0; i < list.size(); ++i)
		{
			if(i) out += ',';
			out += list[i];
		}
	}

	template<class T> T Bound(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

	bool KnownRule(const std::string & name)
	{
		for(size_t i = 0; i < s_allRules.size(); ++i)
			if(!_stricmp(s_allRules[i].c_str(), name.c_str())) return true;
		return false;
	}

	bool ValidKey(int code)
	{
		return code == -1 || (code > 0 && code < 282);
	}
}

namespace QL
{
	namespace Config
	{
		const std::vector<std::string> & AllSortRules()
		{
			if(s_allRules.empty())
				for(size_t i = 0; i < sizeof(kAllRules) / sizeof(kAllRules[0]); ++i) s_allRules.push_back(kAllRules[i]);
			return s_allRules;
		}

		const std::vector<std::string> & DefaultSortRules()
		{
			if(s_defaultRules.empty())
				for(size_t i = 0; i < sizeof(kDefaultRules) / sizeof(kDefaultRules[0]); ++i) s_defaultRules.push_back(kDefaultRules[i]);
			return s_defaultRules;
		}

		void Defaults(Settings & s)
		{
			s.showInCombat			= true;
			s.showWhenEmpty			= false;
			s.showWhenSneaking		= true;
			s.showWhenUnlocked		= true;
			s.showInThirdPerson		= true;
			s.showWhenMounted		= false;
			s.enableForContainers	= true;
			s.enableForCorpses		= true;
			s.enableForAnimals		= true;
			s.enableForDragons		= true;
			s.breakInvisibility		= true;
			s.playScrollSound		= true;

			s.windowX			= 100;
			s.windowY			= -200;
			s.windowScale		= 1.0f;
			s.windowAnchor		= kTopLeft;
			s.minLines			= 0;
			s.maxLines			= 7;
			s.opacityNormal		= 1.0f;
			s.opacityEmpty		= 0.3f;

			s.showItemIcons			= true;
			s.showBestInClass		= false;
			s.showRead				= true;
			s.showEnchanted			= true;
			s.showEnchantedKnown	= true;
			s.showEnchantedSpecial	= true;
			s.infoColumns.clear();
			s.infoColumns.push_back("value");
			s.infoColumns.push_back("weight");
			s.infoColumns.push_back("valuePerWeight");

			s.sortRules = DefaultSortRules();

			// QuickLootIEMCM.psc: E take, Shift+E use, R take all, Q search.
			const KeySetting kb[kKeyActionCount] = { { 18, 42 }, { 18, -1 }, { 19, -1 }, { 16, -1 }, { -1, -1 }, { -1, -1 } };
			// A take, Y use, X take all, Back search.
			const KeySetting gp[kKeyActionCount] = { { 279, -1 }, { 276, -1 }, { 278, -1 }, { 271, -1 }, { -1, -1 }, { -1, -1 } };
			for(int i = 0; i < kKeyActionCount; ++i) { s.keyboard[i] = kb[i]; s.gamepad[i] = gp[i]; }

			s.suppressActivationPrompt = true;
			s.menuWhitelist.clear();
			s.menuWhitelist.push_back("Durability Menu");
			s.menuWhitelist.push_back("UIHS_WidgetMenu");
			s.menuWhitelist.push_back("WidgetMenu");
			s.menuWhitelist.push_back("CombatAlertOverlayMenu");
			s.menuWhitelist.push_back("AchievementWidget");
			s.containerBlacklist.clear();
			s.containerBlacklist.push_back("Skyrim.esm|09B0AD");	// Gunjar: must be interacted with directly

			s.enableLog = false;
		}

		void Clamp(Settings & s)
		{
			s.windowX		= Bound(s.windowX, -4000, 4000);
			s.windowY		= Bound(s.windowY, -4000, 4000);
			if(!(s.windowScale == s.windowScale)) s.windowScale = 1.0f;	// NaN
			s.windowScale	= Bound(s.windowScale, 0.25f, 3.0f);
			s.windowAnchor	= Bound(s.windowAnchor, 0, (int)kAnchorCount - 1);
			s.maxLines		= Bound(s.maxLines, 1, 25);
			s.minLines		= Bound(s.minLines, 0, s.maxLines);
			if(!(s.opacityNormal == s.opacityNormal)) s.opacityNormal = 1.0f;
			if(!(s.opacityEmpty == s.opacityEmpty)) s.opacityEmpty = 0.3f;
			s.opacityNormal	= Bound(s.opacityNormal, 0.0f, 1.0f);
			s.opacityEmpty	= Bound(s.opacityEmpty, 0.0f, 1.0f);

			std::vector<std::string> columns;
			for(size_t i = 0; i < s.infoColumns.size() && columns.size() < 3; ++i)
			{
				const char * c = s.infoColumns[i].c_str();
				const char * norm = !_stricmp(c, "value") ? "value" : !_stricmp(c, "weight") ? "weight" :
									!_stricmp(c, "valuePerWeight") ? "valuePerWeight" : 0;
				if(!norm) continue;
				bool dup = false;
				for(size_t j = 0; j < columns.size(); ++j) if(columns[j] == norm) dup = true;
				if(!dup) columns.push_back(norm);
			}
			s.infoColumns.swap(columns);

			std::vector<std::string> rules;
			AllSortRules();
			for(size_t i = 0; i < s.sortRules.size(); ++i)
			{
				if(!KnownRule(s.sortRules[i])) continue;
				bool dup = false;
				for(size_t j = 0; j < rules.size(); ++j) if(!_stricmp(rules[j].c_str(), s.sortRules[i].c_str())) dup = true;
				if(dup) continue;
				for(size_t j = 0; j < s_allRules.size(); ++j)
					if(!_stricmp(s_allRules[j].c_str(), s.sortRules[i].c_str())) { rules.push_back(s_allRules[j]); break; }
			}
			s.sortRules.swap(rules);

			for(int i = 0; i < kKeyActionCount; ++i)
			{
				if(!ValidKey(s.keyboard[i].key)) s.keyboard[i].key = -1;
				if(!ValidKey(s.keyboard[i].modifier)) s.keyboard[i].modifier = -1;
				if(!ValidKey(s.gamepad[i].key)) s.gamepad[i].key = -1;
				if(!ValidKey(s.gamepad[i].modifier)) s.gamepad[i].modifier = -1;
			}

			if(s.menuWhitelist.size() > 64) s.menuWhitelist.resize(64);
			if(s.containerBlacklist.size() > 256) s.containerBlacklist.resize(256);
		}

		bool ParseFloat(const char * text, float & out)
		{
			if(!text) return false;
			while(*text == ' ' || *text == '\t') ++text;
			bool neg = false;
			if(*text == '-' || *text == '+') { neg = (*text == '-'); ++text; }
			double v = 0.0;
			int digits = 0;
			while(*text >= '0' && *text <= '9') { v = v * 10.0 + (*text - '0'); ++text; ++digits; if(digits > 9) return false; }
			if(*text == '.' || *text == ',')
			{
				++text;
				double scale = 0.1;
				while(*text >= '0' && *text <= '9') { v += (*text - '0') * scale; scale *= 0.1; ++text; ++digits; }
			}
			while(*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
			if(*text || !digits) return false;
			out = (float)(neg ? -v : v);
			return true;
		}

		void FormatFloat(float value, int decimals, char * out, size_t size)
		{
			if(!out || size < 2) return;
			if(!(value == value)) value = 0.0f;
			if(decimals < 0) decimals = 0;
			if(decimals > 6) decimals = 6;
			long long scale = 1;
			for(int i = 0; i < decimals; ++i) scale *= 10;
			const bool neg = value < 0.0f;
			double v = neg ? -(double)value : (double)value;
			long long whole = (long long)(v * (double)scale + 0.5);
			const long long ip = whole / scale, fp = whole % scale;
			if(decimals)	sprintf_s(out, size, "%s%lld.%0*lld", neg && whole ? "-" : "", ip, decimals, fp);
			else			sprintf_s(out, size, "%s%lld", neg && whole ? "-" : "", ip);
		}

		void SplitList(const char * text, std::vector<std::string> & out)
		{
			out.clear();
			if(!text) return;
			std::string cur;
			for(const char * p = text; ; ++p)
			{
				if(*p == ',' || *p == ';' || *p == 0)
				{
					Trim(cur);
					if(!cur.empty()) out.push_back(cur);
					cur.clear();
					if(!*p) break;
				}
				else cur += *p;
			}
		}

		const char * IniPath()
		{
			if(!s_path[0])
			{
				// Next to this DLL (Data\SKSE\Plugins), never relative to the working directory (F5).
				HMODULE self = NULL;
				GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
								   (LPCSTR)&IniPath, &self);
				char path[MAX_PATH] = { 0 };
				if(self && GetModuleFileNameA(self, path, MAX_PATH))
				{
					char * slash = strrchr(path, '\\');
					if(slash) *(slash + 1) = 0;
					strcat_s(path, sizeof(path), "QuickLootIELE.ini");
					strcpy_s(s_path, sizeof(s_path), path);
				}
				else strcpy_s(s_path, sizeof(s_path), "Data\\SKSE\\Plugins\\QuickLootIELE.ini");
			}
			return s_path;
		}

		void Load()
		{
			EnsureLock();
			Settings s;
			Defaults(s);

			FILE * f = 0;
			if(fopen_s(&f, IniPath(), "rb") == 0 && f)
			{
				char section[64] = { 0 };
				char line[4096];
				while(fgets(line, sizeof(line), f))
				{
					std::string l(line);
					Trim(l);
					if(l.empty() || l[0] == ';' || l[0] == '#') continue;
					if(l[0] == '[')
					{
						size_t e = l.find(']');
						std::string sec = l.substr(1, e == std::string::npos ? std::string::npos : e - 1);
						strcpy_s(section, sizeof(section), sec.c_str());
						continue;
					}
					size_t eq = l.find('=');
					if(eq == std::string::npos) continue;
					std::string key = l.substr(0, eq), val = l.substr(eq + 1);
					Trim(key); Trim(val);
					// strip a trailing comment
					size_t sc = val.find(" ;");
					if(sc != std::string::npos) { val = val.substr(0, sc); Trim(val); }
					const char * k = key.c_str();
					const char * v = val.c_str();
					bool b; int i; float fl;

					if(!_stricmp(section, "General"))
					{
						struct { const char * n; bool * p; } flags[] = {
							{ "ShowInCombat", &s.showInCombat }, { "ShowWhenEmpty", &s.showWhenEmpty },
							{ "ShowWhenSneaking", &s.showWhenSneaking }, { "ShowWhenUnlocked", &s.showWhenUnlocked },
							{ "ShowInThirdPerson", &s.showInThirdPerson }, { "ShowWhenMounted", &s.showWhenMounted },
							{ "EnableForContainers", &s.enableForContainers }, { "EnableForCorpses", &s.enableForCorpses },
							{ "EnableForAnimals", &s.enableForAnimals }, { "EnableForDragons", &s.enableForDragons },
							{ "BreakInvisibility", &s.breakInvisibility }, { "PlayScrollSound", &s.playScrollSound },
						};
						for(size_t n = 0; n < sizeof(flags) / sizeof(flags[0]); ++n)
							if(!_stricmp(k, flags[n].n) && ParseBool(v, b)) *flags[n].p = b;
					}
					else if(!_stricmp(section, "Window"))
					{
						if(!_stricmp(k, "OffsetX") && ParseInt(v, i)) s.windowX = i;
						else if(!_stricmp(k, "OffsetY") && ParseInt(v, i)) s.windowY = i;
						else if(!_stricmp(k, "Scale") && ParseFloat(v, fl)) s.windowScale = fl;
						else if(!_stricmp(k, "Anchor") && ParseInt(v, i)) s.windowAnchor = i;
						else if(!_stricmp(k, "MinLines") && ParseInt(v, i)) s.minLines = i;
						else if(!_stricmp(k, "MaxLines") && ParseInt(v, i)) s.maxLines = i;
						else if(!_stricmp(k, "OpacityNormal") && ParseFloat(v, fl)) s.opacityNormal = fl;
						else if(!_stricmp(k, "OpacityEmpty") && ParseFloat(v, fl)) s.opacityEmpty = fl;
					}
					else if(!_stricmp(section, "Display"))
					{
						if(!_stricmp(k, "ShowItemIcons") && ParseBool(v, b)) s.showItemIcons = b;
						else if(!_stricmp(k, "ShowBestInClass") && ParseBool(v, b)) s.showBestInClass = b;
						else if(!_stricmp(k, "ShowRead") && ParseBool(v, b)) s.showRead = b;
						else if(!_stricmp(k, "ShowEnchanted") && ParseBool(v, b)) s.showEnchanted = b;
						else if(!_stricmp(k, "ShowEnchantedKnown") && ParseBool(v, b)) s.showEnchantedKnown = b;
						else if(!_stricmp(k, "ShowEnchantedSpecial") && ParseBool(v, b)) s.showEnchantedSpecial = b;
						else if(!_stricmp(k, "InfoColumns")) SplitList(v, s.infoColumns);
					}
					else if(!_stricmp(section, "Sorting"))
					{
						if(!_stricmp(k, "Rules")) SplitList(v, s.sortRules);
					}
					else if(!_stricmp(section, "Controls"))
					{
						for(int a = 0; a < kKeyActionCount; ++a)
						{
							char name[48];
							sprintf_s(name, sizeof(name), "%s", kActionKeys[a]);
							if(!_stricmp(k, name) && ParseInt(v, i)) s.keyboard[a].key = i;
							sprintf_s(name, sizeof(name), "%sModifier", kActionKeys[a]);
							if(!_stricmp(k, name) && ParseInt(v, i)) s.keyboard[a].modifier = i;
							sprintf_s(name, sizeof(name), "%sGamepad", kActionKeys[a]);
							if(!_stricmp(k, name) && ParseInt(v, i)) s.gamepad[a].key = i;
							sprintf_s(name, sizeof(name), "%sGamepadModifier", kActionKeys[a]);
							if(!_stricmp(k, name) && ParseInt(v, i)) s.gamepad[a].modifier = i;
						}
					}
					else if(!_stricmp(section, "Compatibility"))
					{
						if(!_stricmp(k, "SuppressActivationPrompt") && ParseBool(v, b)) s.suppressActivationPrompt = b;
						else if(!_stricmp(k, "MenuWhitelist")) SplitList(v, s.menuWhitelist);
						else if(!_stricmp(k, "ContainerBlacklist")) SplitList(v, s.containerBlacklist);
					}
					else if(!_stricmp(section, "Debug"))
					{
						if(!_stricmp(k, "EnableLog") && ParseBool(v, b)) s.enableLog = b;
					}
				}
				fclose(f);
			}

			Clamp(s);
			Log::Enable(s.enableLog);
			EnterCriticalSection(&s_lock);
			s_current = s;
			InterlockedIncrement(&s_generation);
			LeaveCriticalSection(&s_lock);
		}

		bool Save()
		{
			Settings s;
			Copy(s);
			std::string out;
			char buf[512];
			char fa[32], fb[32];
			out += "; QuickLoot IE LE settings. Edited from the KHA-OS page, or by hand while the game is closed.\r\n";
			out += "; Keys are SKSE key codes: 0-255 keyboard (DirectInput scan codes), 256-265 mouse, 266-281 gamepad, -1 = none.\r\n\r\n";
			out += "[General]\r\n";
			sprintf_s(buf, sizeof(buf),
				"ShowInCombat=%d\r\nShowWhenEmpty=%d\r\nShowWhenSneaking=%d\r\nShowWhenUnlocked=%d\r\nShowInThirdPerson=%d\r\n"
				"ShowWhenMounted=%d\r\nEnableForContainers=%d\r\nEnableForCorpses=%d\r\nEnableForAnimals=%d\r\n"
				"EnableForDragons=%d\r\nBreakInvisibility=%d\r\nPlayScrollSound=%d\r\n\r\n",
				s.showInCombat, s.showWhenEmpty, s.showWhenSneaking, s.showWhenUnlocked, s.showInThirdPerson,
				s.showWhenMounted, s.enableForContainers, s.enableForCorpses, s.enableForAnimals,
				s.enableForDragons, s.breakInvisibility, s.playScrollSound);
			out += buf;
			out += "[Window]\r\n";
			FormatFloat(s.windowScale, 2, fa, sizeof(fa));
			sprintf_s(buf, sizeof(buf), "OffsetX=%d\r\nOffsetY=%d\r\nScale=%s\r\nAnchor=%d\r\nMinLines=%d\r\nMaxLines=%d\r\n",
					  s.windowX, s.windowY, fa, s.windowAnchor, s.minLines, s.maxLines);
			out += buf;
			FormatFloat(s.opacityNormal, 2, fa, sizeof(fa));
			FormatFloat(s.opacityEmpty, 2, fb, sizeof(fb));
			sprintf_s(buf, sizeof(buf), "OpacityNormal=%s\r\nOpacityEmpty=%s\r\n\r\n", fa, fb);
			out += buf;
			out += "[Display]\r\n";
			sprintf_s(buf, sizeof(buf), "ShowItemIcons=%d\r\nShowBestInClass=%d\r\nShowRead=%d\r\nShowEnchanted=%d\r\n"
					  "ShowEnchantedKnown=%d\r\nShowEnchantedSpecial=%d\r\n",
					  s.showItemIcons, s.showBestInClass, s.showRead, s.showEnchanted, s.showEnchantedKnown, s.showEnchantedSpecial);
			out += buf;
			std::string list;
			JoinList(s.infoColumns, list);
			out += "InfoColumns=" + list + "\r\n\r\n";
			out += "[Sorting]\r\n";
			JoinList(s.sortRules, list);
			out += "Rules=" + list + "\r\n\r\n";
			out += "[Controls]\r\n";
			for(int a = 0; a < kKeyActionCount; ++a)
			{
				sprintf_s(buf, sizeof(buf), "%s=%d\r\n%sModifier=%d\r\n%sGamepad=%d\r\n%sGamepadModifier=%d\r\n",
						  kActionKeys[a], s.keyboard[a].key, kActionKeys[a], s.keyboard[a].modifier,
						  kActionKeys[a], s.gamepad[a].key, kActionKeys[a], s.gamepad[a].modifier);
				out += buf;
			}
			out += "\r\n[Compatibility]\r\n";
			sprintf_s(buf, sizeof(buf), "SuppressActivationPrompt=%d\r\n", s.suppressActivationPrompt);
			out += buf;
			JoinList(s.menuWhitelist, list);
			out += "MenuWhitelist=" + list + "\r\n";
			JoinList(s.containerBlacklist, list);
			out += "ContainerBlacklist=" + list + "\r\n\r\n";
			out += "[Debug]\r\n";
			sprintf_s(buf, sizeof(buf), "EnableLog=%d\r\n", s.enableLog);
			out += buf;

			char tmp[MAX_PATH];
			sprintf_s(tmp, sizeof(tmp), "%s.tmp", IniPath());
			HANDLE h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if(h == INVALID_HANDLE_VALUE) return false;
			DWORD written = 0;
			const BOOL ok = WriteFile(h, out.data(), (DWORD)out.size(), &written, NULL) && written == out.size();
			FlushFileBuffers(h);
			CloseHandle(h);
			if(!ok) { DeleteFileA(tmp); return false; }
			if(!MoveFileExA(tmp, IniPath(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				DeleteFileA(tmp);
				return false;
			}
			return true;
		}

		void Copy(Settings & out)
		{
			EnsureLock();
			EnterCriticalSection(&s_lock);
			out = s_current;
			LeaveCriticalSection(&s_lock);
		}

		unsigned Generation() { return (unsigned)s_generation; }

		void EditBegin(Settings *& s)
		{
			EnsureLock();
			EnterCriticalSection(&s_lock);
			s = &s_current;
		}

		void EditEnd(bool changed)
		{
			if(changed)
			{
				Clamp(s_current);
				Log::Enable(s_current.enableLog);
				InterlockedIncrement(&s_generation);
			}
			LeaveCriticalSection(&s_lock);
		}
	}
}
