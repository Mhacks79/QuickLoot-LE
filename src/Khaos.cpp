// QuickLoot IE LE - KHA-OS bridge and settings page. See Khaos.h.
//
// What the page does to stay safe (rules\threads.md, FATAL F5/F18):
//   * its own Dear ImGui context and DX9 backend; nothing ImGui crosses into KHA-OS;
//   * one recursive lock around every use of that context: KHA-OS calls the window
//     procedure and the frame from different threads, and SetCapture re-enters;
//   * the KHA-OS theme is copied through its plain-C snapshot export and re-applied when
//     its generation moves; the dark style is the fallback;
//   * no typing into sliders (ImGuiSliderFlags_NoInput): the number never goes through
//     the process locale;
//   * the device objects are given back at the end of every frame drawn;
//   * the frame runs inside the fault guard and the page switches off after three faults;
//   * a change is written through Config::Edit (clamped there) and the INI is saved when
//     a control is released; the game task picks it up - nothing here touches the engine.
//
// Copyright (C) 2026 Mhacks79. The page replaces QuickLoot IE's QuickLootIEMCM.psc (Miss
// Corruption, Faen90 and contributors) and follows Freerun LE's KHA-OS bridge.
// GPL-3.0-or-later WITH the Modding and Linking exceptions.

#define KHAOS_ABI extern "C"

#include "Khaos.h"
#include "Khaos/KhaosAPI.h"
#include "Khaos/KhaosThemeAPI.h"
#include "Settings.h"
#include "Loot.h"
#include "LootMenu.h"
#include "Input.h"
#include "Guard.h"
#include "Log.h"

#include "imgui.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"

#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	volatile LONG		s_hosted	= 0;
	volatile LONG		s_open		= 0;
	CRITICAL_SECTION	s_lock;
	bool				s_lockReady	= false;
	ImGuiContext *		s_context	= 0;
	void *				s_hwnd		= 0;
	void *				s_device	= 0;
	bool				s_backends	= false;
	bool				s_failed	= false;
	UInt32				s_faults	= 0;
	UInt32				s_themeGeneration = 0xFFFFFFFF;
	KHAOS_UI_ThemeSnapshot s_theme;

	// key capture: which action, keyboard key or modifier
	int					s_captureAction = -1;
	bool				s_captureModifier = false;

	// the page works on a copy, written back through Config::Edit
	QL::Settings		s_edit;
	unsigned			s_editGeneration = 0;
	char				s_newMenu[128] = { 0 };

	const char * const kAnchorNames[QL::kAnchorCount] =
	{
		"Top left", "Middle left", "Bottom left", "Top centre", "Centre", "Bottom centre", "Top right", "Middle right", "Bottom right",
	};
	const char * const kActionNames[QL::kKeyActionCount] = { "Use / equip / read", "Take", "Take all", "Search (open as usual)", "Turn QuickLoot off", "Turn QuickLoot on" };
	const char * const kColumnPresets[] = { "Value, weight, value per weight", "Value, value per weight, weight", "Value, weight", "None" };
	const char * const kColumnPresetValues[][3] = { { "value", "weight", "valuePerWeight" }, { "value", "valuePerWeight", "weight" }, { "value", "weight", 0 }, { 0, 0, 0 } };

	struct RuleName { const char * key; const char * label; };
	const RuleName kRuleNames[] =
	{
		{ "Gold", "Gold" }, { "Gems", "Gems" }, { "SoulGems", "Soul gems" }, { "Lockpicks", "Lockpicks" },
		{ "OresIngots", "Ores and ingots" }, { "Potions", "Potions" }, { "FoodDrinks", "Food and drinks" },
		{ "Books", "Books (unread first)" }, { "Notes", "Notes and recipes" }, { "Scrolls", "Scrolls" },
		{ "Weapons", "Weapons" }, { "ArrowsBolts", "Arrows and bolts" }, { "Armors", "Armour" }, { "Clothes", "Clothes" },
		{ "Jewelry", "Jewellery" }, { "Weightless", "Weightless items" }, { "Enchanted", "Enchanted items" },
		{ "ByWeight", "By weight (lightest first)" }, { "ByValue", "By value (highest first)" },
		{ "ByV/W", "By value per weight" }, { "ByName", "By name" },
	};
	const char * RuleLabel(const std::string & key)
	{
		for(size_t i = 0; i < sizeof(kRuleNames) / sizeof(kRuleNames[0]); ++i)
			if(key == kRuleNames[i].key) return kRuleNames[i].label;
		return key.c_str();
	}

	const char * const kGamepadNames[] = { "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Start", "Back",
		"Left stick", "Right stick", "Left shoulder", "Right shoulder", "A", "B", "X", "Y", "Left trigger", "Right trigger" };

	// ------------------------------------------------------------------ changes
	void Commit(bool save)
	{
		const QL::Settings copy = s_edit;
		QL::Config::Edit([&](QL::Settings & s) { s = copy; });
		QL::Config::Copy(s_edit);
		s_editGeneration = QL::Config::Generation();
		if(save) QL::Config::Save();
		QL::Loot::OnSettingsChanged();
	}

	void Sync()
	{
		if(s_editGeneration != QL::Config::Generation())
		{
			QL::Config::Copy(s_edit);
			s_editGeneration = QL::Config::Generation();
		}
	}

	bool Toggle(const char * label, bool & value, const char * help = 0)
	{
		bool shown = value;
		const bool clicked = ImGui::Checkbox(label, &shown);
		if(help && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", help);
		if(!clicked) return false;
		value = shown;
		Commit(true);
		return true;
	}

	void SliderI(const char * label, int & value, int low, int high, const char * format)
	{
		int shown = value;
		if(ImGui::SliderInt(label, &shown, low, high, format, ImGuiSliderFlags_NoInput)) { value = shown; Commit(false); }
		if(ImGui::IsItemDeactivatedAfterEdit()) Commit(true);
	}

	void SliderF(const char * label, float & value, float low, float high, const char * format)
	{
		float shown = value;
		if(ImGui::SliderFloat(label, &shown, low, high, format, ImGuiSliderFlags_NoInput)) { value = shown; Commit(false); }
		if(ImGui::IsItemDeactivatedAfterEdit()) Commit(true);
	}

	void Section(const char * text) { ImGui::Spacing(); ImGui::SeparatorText(text); }

	void KeyName(int code, char * out, size_t size)
	{
		if(code <= 0) { sprintf_s(out, size, "none"); return; }
		if(code >= 266 && code < 282) { sprintf_s(out, size, "%s", kGamepadNames[code - 266]); return; }
		if(code >= 256)
		{
			const char * mouse[] = { "Left mouse", "Right mouse", "Middle mouse", "Mouse 4", "Mouse 5", "Mouse 6", "Mouse 7", "Mouse 8", "Wheel up", "Wheel down" };
			sprintf_s(out, size, "%s", mouse[code - 256]);
			return;
		}
		LONG lParam = (LONG)((code & 0x7F) << 16);
		if(code & 0x80) lParam |= (1 << 24);
		char name[64] = { 0 };
		if(GetKeyNameTextA(lParam, name, sizeof(name)) > 0) sprintf_s(out, size, "%s", name);
		else sprintf_s(out, size, "key 0x%02X", code);
	}

	// ------------------------------------------------------------------ pages
	void PageGeneral()
	{
		Section("When the loot window appears");
		Toggle("In combat", s_edit.showInCombat);
		Toggle("When the container is empty", s_edit.showWhenEmpty);
		Toggle("While sneaking", s_edit.showWhenSneaking);
		Toggle("Right after unlocking (instead of opening the container)", s_edit.showWhenUnlocked);
		Toggle("In third person", s_edit.showInThirdPerson);
		Toggle("On horseback", s_edit.showWhenMounted);
		Section("What it looks into");
		Toggle("Containers", s_edit.enableForContainers);
		Toggle("Corpses", s_edit.enableForCorpses);
		Toggle("Animal corpses", s_edit.enableForAnimals);
		Toggle("Dragon corpses", s_edit.enableForDragons);
		Section("Behaviour");
		Toggle("Taking an item breaks invisibility", s_edit.breakInvisibility);
		Toggle("Sound when scrolling", s_edit.playScrollSound);
		Toggle("Hide the game's activation prompt while the window shows", s_edit.suppressActivationPrompt);
	}

	void PageWindow()
	{
		Section("Position");
		int anchor = s_edit.windowAnchor;
		if(ImGui::BeginCombo("Anchor", kAnchorNames[anchor < 0 || anchor >= QL::kAnchorCount ? 0 : anchor]))
		{
			for(int i = 0; i < QL::kAnchorCount; ++i)
				if(ImGui::Selectable(kAnchorNames[i], i == anchor)) { s_edit.windowAnchor = i; Commit(true); }
			ImGui::EndCombo();
		}
		SliderI("Horizontal offset", s_edit.windowX, -2000, 2000, "%d px");
		SliderI("Vertical offset", s_edit.windowY, -2000, 2000, "%d px");
		SliderF("Scale", s_edit.windowScale, 0.25f, 3.0f, "%.2f");
		Section("Size");
		SliderI("Minimum lines", s_edit.minLines, 0, 25, "%d");
		SliderI("Maximum lines", s_edit.maxLines, 1, 25, "%d");
		Section("Opacity");
		SliderF("With items", s_edit.opacityNormal, 0.0f, 1.0f, "%.2f");
		SliderF("When empty", s_edit.opacityEmpty, 0.0f, 1.0f, "%.2f");
		ImGui::Spacing();
		ImGui::TextDisabled("Look at a container: the loot window follows these settings as you move them.");
	}

	void PageDisplay()
	{
		Section("Icons");
		Toggle("Item icons (needs SkyUI)", s_edit.showItemIcons);
		Toggle("Mark the best item of each kind", s_edit.showBestInClass, "Compared with what you carry: armour per slot, weapons per class, ammunition.");
		Toggle("Mark books already read", s_edit.showRead);
		Toggle("Mark enchanted items", s_edit.showEnchanted);
		ImGui::BeginDisabled(!s_edit.showEnchanted);
		Toggle("  ...known enchantments", s_edit.showEnchantedKnown);
		Toggle("  ...enchantments that cannot be learned", s_edit.showEnchantedSpecial);
		ImGui::EndDisabled();
		Section("Columns");
		int current = 3;
		for(int p = 0; p < 4; ++p)
		{
			size_t n = 0;
			while(n < 3 && kColumnPresetValues[p][n]) ++n;
			if(n != s_edit.infoColumns.size()) continue;
			bool same = true;
			for(size_t i = 0; i < n; ++i) if(s_edit.infoColumns[i] != kColumnPresetValues[p][i]) same = false;
			if(same) { current = p; break; }
		}
		if(ImGui::BeginCombo("Columns", kColumnPresets[current]))
		{
			for(int p = 0; p < 4; ++p)
				if(ImGui::Selectable(kColumnPresets[p], p == current))
				{
					s_edit.infoColumns.clear();
					for(int i = 0; i < 3 && kColumnPresetValues[p][i]; ++i) s_edit.infoColumns.push_back(kColumnPresetValues[p][i]);
					Commit(true);
				}
			ImGui::EndCombo();
		}
	}

	void PageSorting()
	{
		Section("Sort order (first rule wins, ties go to the next)");
		int moveUp = -1, moveDown = -1, remove = -1;
		for(size_t i = 0; i < s_edit.sortRules.size(); ++i)
		{
			ImGui::PushID((int)i);
			ImGui::Text("%2u. %s", (unsigned)(i + 1), RuleLabel(s_edit.sortRules[i]));
			ImGui::SameLine(320.0f);
			if(ImGui::SmallButton("Up")) moveUp = (int)i;
			ImGui::SameLine();
			if(ImGui::SmallButton("Down")) moveDown = (int)i;
			ImGui::SameLine();
			if(ImGui::SmallButton("Remove")) remove = (int)i;
			ImGui::PopID();
		}
		if(moveUp > 0) { std::swap(s_edit.sortRules[moveUp], s_edit.sortRules[moveUp - 1]); Commit(true); }
		else if(moveDown >= 0 && moveDown + 1 < (int)s_edit.sortRules.size()) { std::swap(s_edit.sortRules[moveDown], s_edit.sortRules[moveDown + 1]); Commit(true); }
		else if(remove >= 0) { s_edit.sortRules.erase(s_edit.sortRules.begin() + remove); Commit(true); }

		const std::vector<std::string> & all = QL::Config::AllSortRules();
		if(ImGui::BeginCombo("Add a rule", "choose..."))
		{
			for(size_t i = 0; i < all.size(); ++i)
			{
				bool used = false;
				for(size_t j = 0; j < s_edit.sortRules.size(); ++j) if(s_edit.sortRules[j] == all[i]) used = true;
				if(used) continue;
				if(ImGui::Selectable(RuleLabel(all[i]))) { s_edit.sortRules.push_back(all[i]); Commit(true); }
			}
			ImGui::EndCombo();
		}
		if(ImGui::Button("Default order")) { s_edit.sortRules = QL::Config::DefaultSortRules(); Commit(true); }
		ImGui::SameLine();
		if(ImGui::Button("Gold and valuables first"))
		{
			const char * goblin[] = { "Gold", "Jewelry", "Gems", "SoulGems", "Weightless", "ByName" };
			s_edit.sortRules.clear();
			for(size_t i = 0; i < sizeof(goblin) / sizeof(goblin[0]); ++i) s_edit.sortRules.push_back(goblin[i]);
			Commit(true);
		}
	}

	void PageControls()
	{
		Section("Keyboard and mouse");
		ImGui::TextDisabled("Click a key, then press the new one (Esc cancels, Backspace clears).");
		char key[64], mod[64], label[96];
		for(int a = 0; a < QL::kKeyActionCount; ++a)
		{
			ImGui::PushID(a);
			KeyName(s_edit.keyboard[a].key, key, sizeof(key));
			KeyName(s_edit.keyboard[a].modifier, mod, sizeof(mod));
			ImGui::Text("%s", kActionNames[a]);
			ImGui::SameLine(220.0f);
			const bool capKey = s_captureAction == a && !s_captureModifier;
			sprintf_s(label, sizeof(label), "%s##key", capKey ? "press a key..." : key);
			if(ImGui::Button(label, ImVec2(130, 0))) { s_captureAction = a; s_captureModifier = false; }
			ImGui::SameLine();
			const bool capMod = s_captureAction == a && s_captureModifier;
			sprintf_s(label, sizeof(label), "%s##mod", capMod ? "press a key..." : (s_edit.keyboard[a].modifier > 0 ? mod : "no modifier"));
			if(ImGui::Button(label, ImVec2(130, 0))) { s_captureAction = a; s_captureModifier = true; }
			ImGui::PopID();
		}
		Section("Gamepad");
		for(int a = 0; a < QL::kKeyActionCount; ++a)
		{
			ImGui::PushID(100 + a);
			ImGui::Text("%s", kActionNames[a]);
			ImGui::SameLine(220.0f);
			const int k = s_edit.gamepad[a].key;
			ImGui::SetNextItemWidth(130);
			if(ImGui::BeginCombo("##pad", k >= 266 && k < 282 ? kGamepadNames[k - 266] : "none"))
			{
				if(ImGui::Selectable("none", k < 266)) { s_edit.gamepad[a].key = -1; Commit(true); }
				for(int i = 0; i < 16; ++i)
					if(ImGui::Selectable(kGamepadNames[i], k == 266 + i)) { s_edit.gamepad[a].key = 266 + i; Commit(true); }
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			const int m = s_edit.gamepad[a].modifier;
			ImGui::SetNextItemWidth(130);
			if(ImGui::BeginCombo("##padmod", m >= 266 && m < 282 ? kGamepadNames[m - 266] : "no modifier"))
			{
				if(ImGui::Selectable("no modifier", m < 266)) { s_edit.gamepad[a].modifier = -1; Commit(true); }
				for(int i = 0; i < 16; ++i)
					if(ImGui::Selectable(kGamepadNames[i], m == 266 + i)) { s_edit.gamepad[a].modifier = 266 + i; Commit(true); }
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
		ImGui::Spacing();
		ImGui::TextDisabled("A modifier equal to the key itself makes it a hold action.");
		if(ImGui::Button("Default controls"))
		{
			QL::Settings d;
			QL::Config::Defaults(d);
			for(int a = 0; a < QL::kKeyActionCount; ++a) { s_edit.keyboard[a] = d.keyboard[a]; s_edit.gamepad[a] = d.gamepad[a]; }
			s_captureAction = -1;
			Commit(true);
		}
	}

	void EditableList(const char * id, std::vector<std::string> & list)
	{
		int remove = -1;
		for(size_t i = 0; i < list.size(); ++i)
		{
			ImGui::PushID(id);
			ImGui::PushID((int)i);
			ImGui::TextUnformatted(list[i].c_str());
			ImGui::SameLine(360.0f);
			if(ImGui::SmallButton("Remove")) remove = (int)i;
			ImGui::PopID();
			ImGui::PopID();
		}
		if(remove >= 0) { list.erase(list.begin() + remove); Commit(true); }
	}

	void PageExclusions()
	{
		Section("Containers QuickLoot leaves alone");
		EditableList("blacklist", s_edit.containerBlacklist);
		QL::Loot::View view;
		QL::Loot::CopyView(view);
		ImGui::BeginDisabled(!view.show || view.refKey.empty());
		if(ImGui::Button("Add the container in the loot window")) { s_edit.containerBlacklist.push_back(view.refKey); Commit(true); }
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!view.show || view.baseKey.empty());
		if(ImGui::Button("Add every container of that kind")) { s_edit.containerBlacklist.push_back(view.baseKey); Commit(true); }
		ImGui::EndDisabled();
		ImGui::TextDisabled("Containers with the keyword QuickLootIE_Exclude are always left alone.");

		Section("Menus that may stay open with the loot window");
		EditableList("whitelist", s_edit.menuWhitelist);
		ImGui::SetNextItemWidth(260);
		ImGui::InputText("##newmenu", s_newMenu, sizeof(s_newMenu));
		ImGui::SameLine();
		if(ImGui::Button("Add menu") && s_newMenu[0]) { s_edit.menuWhitelist.push_back(s_newMenu); s_newMenu[0] = 0; Commit(true); }
	}

	void PageAdvanced()
	{
		Section("Troubleshooting");
		Toggle("Write a log (Documents\\My Games\\Skyrim\\SKSE\\QuickLootIELE.log)", s_edit.enableLog);
		ImGui::Text("Loot window movie: %s", QL::Menu::SwfFeatureLevel() >= 0 ? "loaded" : "not loaded yet");
		ImGui::Text("Faults caught: %u%s", QL::Guard::Faults(), QL::Guard::Disabled() ? " - QuickLoot is off for this session" : "");
		if(ImGui::Button("Reload the INI")) { QL::Config::Load(); Sync(); QL::Loot::OnSettingsChanged(); }
		ImGui::SameLine();
		if(ImGui::Button("Reset everything to defaults"))
		{
			QL::Config::Defaults(s_edit);
			Commit(true);
		}
	}

	// ------------------------------------------------------------------ theme
	const int kThemeMap[KHAOS_UI_COLOR_COUNT] =
	{
		ImGuiCol_Text, ImGuiCol_TextDisabled, ImGuiCol_WindowBg, ImGuiCol_ChildBg, ImGuiCol_PopupBg,
		ImGuiCol_Border, ImGuiCol_BorderShadow, ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered,
		ImGuiCol_FrameBgActive, ImGuiCol_TitleBg, ImGuiCol_TitleBgActive, ImGuiCol_TitleBgCollapsed,
		ImGuiCol_MenuBarBg, ImGuiCol_ScrollbarBg, ImGuiCol_ScrollbarGrab, ImGuiCol_ScrollbarGrabHovered,
		ImGuiCol_ScrollbarGrabActive, ImGuiCol_CheckMark, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive,
		ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive, ImGuiCol_Header,
		ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive, ImGuiCol_Separator, ImGuiCol_SeparatorHovered,
		ImGuiCol_SeparatorActive, ImGuiCol_ResizeGrip, ImGuiCol_ResizeGripHovered,
		ImGuiCol_ResizeGripActive, ImGuiCol_Tab, ImGuiCol_TabHovered, ImGuiCol_TabSelected,
		ImGuiCol_TabDimmed, ImGuiCol_TabDimmedSelected, ImGuiCol_PlotLines, ImGuiCol_PlotLinesHovered,
		ImGuiCol_PlotHistogram, ImGuiCol_PlotHistogramHovered, ImGuiCol_TextSelectedBg,
		ImGuiCol_DragDropTarget, ImGuiCol_NavCursor, ImGuiCol_NavWindowingHighlight,
		ImGuiCol_NavWindowingDimBg, ImGuiCol_ModalWindowDimBg, ImGuiCol_TableHeaderBg,
		ImGuiCol_TableBorderStrong, ImGuiCol_TableBorderLight, ImGuiCol_TableRowBg,
		ImGuiCol_TableRowBgAlt
	};

	void FollowTheHubTheme()
	{
		HMODULE khaos = GetModuleHandleA("KHAOS.dll");
		if(!khaos) return;
		KHAOS_UI_GetThemeGenerationFn generation = (KHAOS_UI_GetThemeGenerationFn)GetProcAddress(khaos, "KHAOS_UI_GetThemeGeneration");
		KHAOS_UI_GetThemeSnapshotFn snapshot = (KHAOS_UI_GetThemeSnapshotFn)GetProcAddress(khaos, "KHAOS_UI_GetThemeSnapshot");
		if(!generation || !snapshot) return;
		const UInt32 now = generation();
		if(now == s_themeGeneration) return;
		KHAOS_UI_ThemeSnapshot & theme = s_theme;
		memset(&theme, 0, sizeof(theme));
		theme.apiVersion = KHAOS_UI_THEME_API_VERSION;
		theme.structSize = sizeof(theme);
		if(!snapshot(&theme)) return;
		if(theme.apiVersion != KHAOS_UI_THEME_API_VERSION || theme.structSize < sizeof(theme)) return;
		ImGuiStyle & style = ImGui::GetStyle();
		style = ImGuiStyle();
		ImGui::StyleColorsDark(&style);
		const UInt32 count = theme.colorCount < KHAOS_UI_COLOR_COUNT ? theme.colorCount : KHAOS_UI_COLOR_COUNT;
		for(UInt32 i = 0; i < count; ++i)
			style.Colors[kThemeMap[i]] = ImVec4(theme.colors[i][0], theme.colors[i][1], theme.colors[i][2], theme.colors[i][3]);
		style.Alpha				= theme.alpha > 0.0f ? theme.alpha : 1.0f;
		style.DisabledAlpha		= theme.disabledAlpha;
		style.WindowPadding		= ImVec2(theme.windowPadding[0], theme.windowPadding[1]);
		style.WindowRounding	= theme.windowRounding;
		style.WindowBorderSize	= theme.windowBorderSize;
		style.ChildRounding		= theme.childRounding;
		style.ChildBorderSize	= theme.childBorderSize;
		style.PopupRounding		= theme.popupRounding;
		style.PopupBorderSize	= theme.popupBorderSize;
		style.FramePadding		= ImVec2(theme.framePadding[0], theme.framePadding[1]);
		style.FrameRounding		= theme.frameRounding;
		style.FrameBorderSize	= theme.frameBorderSize;
		style.ItemSpacing		= ImVec2(theme.itemSpacing[0], theme.itemSpacing[1]);
		style.ItemInnerSpacing	= ImVec2(theme.itemInnerSpacing[0], theme.itemInnerSpacing[1]);
		style.IndentSpacing		= theme.indentSpacing;
		style.ScrollbarSize		= theme.scrollbarSize;
		style.ScrollbarRounding	= theme.scrollbarRounding;
		style.GrabMinSize		= theme.grabMinSize;
		style.GrabRounding		= theme.grabRounding;
		style.TabRounding		= theme.tabRounding;
		const float scale = (theme.uiScale >= 0.5f && theme.uiScale <= 4.0f) ? theme.uiScale : 1.0f;
		style.ScaleAllSizes(scale);
		style.FontScaleMain = scale;
		s_themeGeneration = now;
	}

	// ------------------------------------------------------------------ frame
	void ShutdownBackends()
	{
		if(s_backends) { ImGui_ImplDX9_Shutdown(); ImGui_ImplWin32_Shutdown(); s_backends = false; }
		s_device = 0;
	}

	bool EnsureContext()
	{
		if(s_context) return true;
		s_context = ImGui::CreateContext();
		if(!s_context) return false;
		ImGui::SetCurrentContext(s_context);
		ImGuiIO & io = ImGui::GetIO();
		io.IniFilename = 0;
		io.LogFilename = 0;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		ImGui::StyleColorsDark();
		return true;
	}

	bool EnsureBackends(void * hwnd, void * device)
	{
		if(s_backends && s_hwnd == hwnd && s_device == device) return true;
		ShutdownBackends();
		if(!ImGui_ImplWin32_Init(hwnd)) return false;
		if(!ImGui_ImplDX9_Init((IDirect3DDevice9 *)device)) { ImGui_ImplWin32_Shutdown(); return false; }
		s_hwnd = hwnd; s_device = device; s_backends = true;
		s_themeGeneration = 0xFFFFFFFF;
		return true;
	}

	void * s_frameHwnd = 0;
	void * s_frameDevice = 0;
	int s_frameW = 0, s_frameH = 0;

	void FrameBody()
	{
		QL::Log::Thread("the KHA-OS frame");
		if(!EnsureContext() || !EnsureBackends(s_frameHwnd, s_frameDevice)) { s_failed = true; return; }
		ImGui::SetCurrentContext(s_context);
		FollowTheHubTheme();
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		if(s_frameW > 0 && s_frameH > 0) ImGui::GetIO().DisplaySize = ImVec2((float)s_frameW, (float)s_frameH);
		ImGui::NewFrame();
		bool open = true;
		QL::Khaos::DrawWindow(&open);
		if(!open) { InterlockedExchange(&s_open, 0); s_captureAction = -1; }
		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		ImGui_ImplDX9_InvalidateDeviceObjects();
	}

	void RenderFrame(void * hwnd, void * device, int width, int height)
	{
		if(s_failed || !hwnd || !device) return;
		s_frameHwnd = hwnd; s_frameDevice = device; s_frameW = width; s_frameH = height;
		if(!QL::Guard::Run("KHA-OS frame", FrameBody, QL::Guard::kKhaos))
		{
			s_captureAction = -1;
			if(QL::Guard::DomainDisabled(QL::Guard::kKhaos)) { s_failed = true; InterlockedExchange(&s_open, 0); }
		}
	}

	// key capture through the window messages: DirectInput scan code = bits 16-23, +0x80 when extended
	bool Capture(unsigned int message, std::uintptr_t wParam, std::intptr_t lParam)
	{
		if(s_captureAction < 0) return false;
		int code = 0;
		if(message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
		{
			if(wParam == VK_ESCAPE) { s_captureAction = -1; return true; }
			if(wParam == VK_BACK) code = -1;
			else
			{
				code = (int)((lParam >> 16) & 0xFF);
				if(lParam & (1 << 24)) code |= 0x80;
				if(!code) return true;
			}
		}
		else if(message == WM_LBUTTONDOWN) code = 256;
		else if(message == WM_RBUTTONDOWN) code = 257;
		else if(message == WM_MBUTTONDOWN) code = 258;
		else if(message == WM_XBUTTONDOWN) code = (HIWORD(wParam) == XBUTTON1) ? 259 : 260;
		else if(message == WM_MOUSEWHEEL) code = ((short)HIWORD(wParam) > 0) ? 264 : 265;
		else return message == WM_KEYUP || message == WM_SYSKEYUP || message == WM_CHAR;
		if(s_captureModifier) s_edit.keyboard[s_captureAction].modifier = code;
		else s_edit.keyboard[s_captureAction].key = code;
		s_captureAction = -1;
		Commit(true);
		return true;
	}
}

namespace QL
{
	namespace Khaos
	{
		void Init()
		{
			if(!s_lockReady) { InitializeCriticalSection(&s_lock); s_lockReady = true; }
			Config::Copy(s_edit);
			s_editGeneration = Config::Generation();
		}

		void DrawWindow(bool * open)
		{
			Sync();
			ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(80.0f, 80.0f), ImGuiCond_FirstUseEver);
			if(ImGui::Begin("QuickLoot IE LE", open))
			{
				if(ImGui::BeginTabBar("ql_pages"))
				{
					if(ImGui::BeginTabItem("General"))		{ PageGeneral();	ImGui::EndTabItem(); }
					if(ImGui::BeginTabItem("Window"))		{ PageWindow();		ImGui::EndTabItem(); }
					if(ImGui::BeginTabItem("Display"))		{ PageDisplay();	ImGui::EndTabItem(); }
					if(ImGui::BeginTabItem("Sorting"))		{ PageSorting();	ImGui::EndTabItem(); }
					if(ImGui::BeginTabItem("Controls"))		{ PageControls();	ImGui::EndTabItem(); }
					if(ImGui::BeginTabItem("Exclusions"))	{ PageExclusions();	ImGui::EndTabItem(); }
					if(ImGui::BeginTabItem("Advanced"))		{ PageAdvanced();	ImGui::EndTabItem(); }
					ImGui::EndTabBar();
				}
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::TextDisabled("Saved to Data\\SKSE\\Plugins\\QuickLootIELE.ini as you change it.");
			}
			ImGui::End();
		}

		void Shutdown()
		{
			s_captureAction = -1;
			if(!s_context) return;
			__try
			{
				ImGui::SetCurrentContext(s_context);
				ShutdownBackends();
				ImGui::DestroyContext(s_context);
			}
			__except(EXCEPTION_EXECUTE_HANDLER) {}
			s_context = 0;
			s_themeGeneration = 0xFFFFFFFF;
		}
	}
}

// ---------------------------------------------------------------------- exports
KHAOS_ABI bool KHAOS_Query(KHAOS_PluginInfo * info)
{
	if(!info || info->structSize < sizeof(KHAOS_PluginInfo) || info->apiVersion < KHAOS_API_VERSION_MIN) return false;
	info->apiVersion		= KHAOS_API_VERSION;
	info->structSize		= sizeof(KHAOS_PluginInfo);
	info->flags				= 0;
	info->pluginId			= "quickloot.ie.le";
	info->displayName		= "QuickLoot IE LE";
	info->category			= "Interface";
	info->dllName			= "QuickLootIELE.dll";
	info->originalHotkey	= "Opened through the KHA-OS hub";
	info->description		= "Loot containers and corpses without opening them: settings, controls, sorting and exclusions.";
	return true;
}

KHAOS_ABI void KHAOS_SetHosted(bool hosted)
{
	InterlockedExchange(&s_hosted, hosted ? 1 : 0);
	if(!hosted)
	{
		InterlockedExchange(&s_open, 0);
		if(s_lockReady) EnterCriticalSection(&s_lock);
		QL::Khaos::Shutdown();
		if(s_lockReady) LeaveCriticalSection(&s_lock);
	}
}

KHAOS_ABI void KHAOS_ToggleMenu()
{
	if(!s_hosted) return;
	InterlockedExchange(&s_open, s_open ? 0 : 1);
	if(!s_open) s_captureAction = -1;
}

KHAOS_ABI bool KHAOS_IsMenuOpen() { return s_open != 0; }

KHAOS_ABI void KHAOS_RenderFrame(void * hwnd, void * device, int width, int height)
{
	if(!s_hosted || !s_open || !s_lockReady) return;
	EnterCriticalSection(&s_lock);
	RenderFrame(hwnd, device, width, height);
	LeaveCriticalSection(&s_lock);
}

namespace
{
	void * s_wpHwnd = 0;
	unsigned int s_wpMessage = 0;
	std::uintptr_t s_wpW = 0;
	std::intptr_t s_wpL = 0;
	bool s_wpResult = false;

	void WndProcBody()
	{
		// Copied first: SetCapture can re-enter this procedure and overwrite the slots.
		void * const hwnd = s_wpHwnd;
		const unsigned int message = s_wpMessage;
		const std::uintptr_t wParam = s_wpW;
		const std::intptr_t lParam = s_wpL;
		bool result = false;
		if(Capture(message, wParam, lParam)) { s_wpResult = true; return; }
		ImGui::SetCurrentContext(s_context);
		ImGui_ImplWin32_WndProcHandler((HWND)hwnd, message, (WPARAM)wParam, (LPARAM)lParam);
		const ImGuiIO & io = ImGui::GetIO();
		switch(message)
		{
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
		case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
			result = io.WantCaptureMouse; break;
		case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_CHAR:
			result = io.WantCaptureKeyboard; break;
		default: break;
		}
		s_wpResult = result;
	}
}

KHAOS_ABI bool KHAOS_ProcessWndProc(void * hwnd, unsigned int message, std::uintptr_t wParam, std::intptr_t lParam)
{
	if(!s_hosted || !s_open || !s_lockReady || !s_context || s_failed) return false;
	EnterCriticalSection(&s_lock);		// recursive: SetCapture re-enters on the same thread
	s_wpHwnd = hwnd; s_wpMessage = message; s_wpW = wParam; s_wpL = lParam;
	bool result = false;
	if(QL::Guard::Run("KHA-OS window procedure", WndProcBody, QL::Guard::kKhaos)) result = s_wpResult;
	LeaveCriticalSection(&s_lock);
	return result;
}
