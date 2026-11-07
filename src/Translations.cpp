// QuickLoot IE LE - translation loader. New file (no counterpart in QuickLoot IE 4.1.3).
// SPDX-License-Identifier: GPL-3.0-or-later WITH Modding Exception AND GPL-3.0 Linking Exception (with Corresponding Source)
//
// SKSE LE only reads Interface\Translations\<name>_<sLanguage>.txt for a <name>.esp/.esm listed
// in plugins.txt (skse/Translation.cpp). This port has no plugin file, so the $qlie_ keys of the
// loot window would never be translated. The file is read here and added to the same
// translator table SKSE fills, without replacing any key that is already there.
//  - language: sLanguage, then ENGLISH when there is no file for it (Chinese, Korean, any
//    language a translation patch adds), so a key never shows as "$qlie_...";
//  - encoding: UTF-16 LE with BOM (the game's format), UTF-8 with or without BOM, and anything
//    else read with the ANSI code page of Windows.
#include "skse/GameSettings.h"
#include "skse/ScaleformLoader.h"
#include "skse/ScaleformState.h"
#include "Translations.h"
#include "Engine.h"
#include "Log.h"
#include <windows.h>
#include <string>
#include <vector>

namespace
{
	// TESV 1.9.32.0.8: the GFxLoader singleton and the translator's string cache (both as in SKSE 1.7.3).
	const UInt32 kGFxLoaderSingleton = 0x01B2E9B0;
	typedef void (__cdecl * _GetCachedString)(wchar_t ** out, wchar_t * in);
	const _GetCachedString GetCachedString = (_GetCachedString)0x00A51800;

	bool ReadFile(const std::string & lang, std::vector<unsigned char> & out)
	{
		char exe[MAX_PATH] = { 0 };
		if(!GetModuleFileNameA(0, exe, MAX_PATH)) return false;
		std::string path = exe;
		path.erase(path.find_last_of("\\/") + 1);
		path += "Data\\Interface\\Translations\\QuickLootIE_" + lang + ".txt";
		HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		if(h == INVALID_HANDLE_VALUE) return false;
		const DWORD size = GetFileSize(h, 0);
		bool ok = false;
		if(size != INVALID_FILE_SIZE && size > 0 && size < (1u << 20))
		{
			out.resize(size);
			DWORD got = 0;
			ok = ::ReadFile(h, &out[0], size, &got, 0) && got == size;
		}
		CloseHandle(h);
		if(ok) QL_LOG("translations: %s", path.c_str());
		return ok;
	}

	std::wstring Decode(const std::vector<unsigned char> & b)
	{
		std::wstring w;
		if(b.size() >= 2 && b[0] == 0xFF && b[1] == 0xFE)
		{
			w.assign((const wchar_t *)&b[2], (b.size() - 2) / 2);
			return w;
		}
		size_t start = (b.size() >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) ? 3 : 0;
		const char * s = (const char *)&b[0] + start;
		const int len = (int)(b.size() - start);
		UINT cp = CP_UTF8;
		int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, len, 0, 0);
		if(n <= 0) { cp = CP_ACP; n = MultiByteToWideChar(CP_ACP, 0, s, len, 0, 0); }
		if(n <= 0) return w;
		w.resize(n);
		MultiByteToWideChar(cp, 0, s, len, &w[0], n);
		return w;
	}

	int AddLines(BSScaleformTranslator * tr, const std::wstring & text)
	{
		int added = 0;
		size_t pos = 0;
		while(pos < text.size())
		{
			size_t end = text.find(L'\n', pos);
			if(end == std::wstring::npos) end = text.size();
			std::wstring line = text.substr(pos, end - pos);
			pos = end + 1;
			if(!line.empty() && line[line.size() - 1] == L'\r') line.erase(line.size() - 1);
			const size_t tab = line.rfind(L'\t');	// same rule as SKSE: the last tab splits
			if(line.size() < 4 || line[0] != L'$' || tab == std::wstring::npos || tab < 2) continue;
			std::wstring key = line.substr(0, tab), value = line.substr(tab + 1);
			wchar_t * k = 0;
			wchar_t * v = 0;
			GetCachedString(&k, &key[0]);
			GetCachedString(&v, value.empty() ? (wchar_t *)L"" : &value[0]);
			if(!k || !v) continue;
			TranslationTableItem item(k, v);
			if(tr->translations.Add(&item)) ++added;	// a key already there is kept
		}
		return added;
	}
}

namespace QL
{
	namespace Translations
	{
		void Import()
		{
			if(!QL::Engine::Readable((void *)kGFxLoaderSingleton, 4)) return;
			GFxLoader * loader = *(GFxLoader **)kGFxLoaderSingleton;
			if(!QL::Engine::Readable(loader, sizeof(GFxLoader)) || !QL::Engine::Readable(loader->stateBag, 4))
			{ QL_LOG("translations: no Scaleform loader"); return; }
			BSScaleformTranslator * tr = loader->stateBag->GetTranslator();	// reference kept, as SKSE does
			if(!QL::Engine::Readable(tr, sizeof(BSScaleformTranslator))) { QL_LOG("translations: no translator"); return; }

			Setting * lang = GetINISetting("sLanguage:General");
			std::string l = (QL::Engine::Readable(lang, sizeof(Setting)) && lang->GetType() == Setting::kType_String &&
				QL::Engine::Readable(lang->data.s, 1)) ? lang->data.s : "ENGLISH";
			std::vector<unsigned char> raw;
			int added = 0;
			if(ReadFile(l, raw)) added = AddLines(tr, Decode(raw));
			if(_stricmp(l.c_str(), "ENGLISH") && ReadFile("ENGLISH", raw))
				added += AddLines(tr, Decode(raw));	// fills whatever the language file lacks
			QL_LOG("translations: language '%s', %d keys added", l.c_str(), added);
		}
	}
}
