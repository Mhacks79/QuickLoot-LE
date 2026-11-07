// QuickLoot IE LE - the loot menu. See LootMenu.h.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE's LootMenu.cpp, UniversalMenu.cpp
// and CLIK/ wrappers (Miss Corruption, AtomCrafty and contributors).
// GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "LootMenu.h"
#include "Loot.h"
#include "Input.h"
#include "Engine.h"
#include "Settings.h"
#include "Guard.h"
#include "Log.h"

#include "skse/GameMenus.h"
#include "skse/ScaleformLoader.h"
#include "skse/ScaleformMovie.h"
#include "skse/ScaleformCallbacks.h"
#include "skse/ScaleformAPI.h"
#include "skse/GameSettings.h"

#include <new>
#include <string>
#include <vector>

namespace
{
	const char * const kMovieName = "LootMenuIE";		// Interface\LootMenuIE.swf
	const UInt32 kMenuFlags = 0x00000800;				// allow saving (F-20); SE: kAllowSaving
	const UInt8  kMenuDepth = 3;

	volatile LONG s_featureLevel = -1;

	// SKSE declares ProcessUnkData1 public but its parameter type private: deduce it.
	template<class> struct MenuMessageArgument;
	template<class R, class C, class A> struct MenuMessageArgument<R(C::*)(A)> { typedef A type; };
	typedef MenuMessageArgument<decltype(&IMenu::ProcessUnkData1)>::type MenuMessagePointer;

	class LootMenu;
	LootMenu * s_menu = 0;		// the live instance (UI thread)

	// State of the instance, kept outside the class so that the guarded bodies below
	// hold no C++ object with a destructor in their own frame (F4).
	struct State
	{
		GFxValue	root, title, weight, itemList, infoBar, buttonBar;
		GFxValue	itemProvider, infoProvider, buttonProvider;
		bool		valid;
		bool		initialized;
		unsigned	viewGeneration;
		unsigned	buttonGeneration;
		bool		gamepad;
		int			selected;
		QL::Loot::View	view;
		QL::Settings	settings;
		unsigned	settingsGeneration;
	};
	State * s_state = 0;

	void SetNumber(GFxValue & obj, const char * name, double v)	{ GFxValue x; x.SetNumber(v); obj.SetMember(name, &x); }
	void SetBool(GFxValue & obj, const char * name, bool v)		{ GFxValue x; x.SetBool(v); obj.SetMember(name, &x); }

	// Game strings (item names, container names, game settings) are handed to Scaleform the way
	// the game's own menus hand them: as an unmanaged char* converted by GFx inside SetMember
	// (TESV 0x00920950). GFx reads it as UTF-8 when it is valid UTF-8 and, otherwise, byte by
	// byte through the CRT (TESV never sets a locale, so "C"). Every language and every
	// translation patch (Chinese, Korean, ...) therefore shows here exactly as in the inventory.
	// CreateString would make a managed GFx string that skips that fallback (boxes for accents),
	// and a code page picked from sLanguage cannot know what a fan translation stored.
	// The value must be passed to SetMember before `s` goes away: the conversion copies it.
	void MakeString(GFxMovieView *, GFxValue & out, const char * s)
	{
		out.SetString(s ? s : "");
	}

	void SetString(GFxMovieView * view, GFxValue & obj, const char * name, const char * s)
	{
		GFxValue x;
		MakeString(view, x, s);
		obj.SetMember(name, &x);
	}

	void ClearArray(GFxValue & arr)
	{
		GFxValue zero;
		zero.SetNumber(0);
		arr.Invoke("splice", 0, &zero, 1);
	}

	void ResolveAnchor(int anchor, double & fx, double & fy)
	{
		fx = (anchor == QL::kTopLeft || anchor == QL::kCenterLeft || anchor == QL::kBottomLeft) ? 0.0 :
			 (anchor == QL::kTopCenter || anchor == QL::kCenter || anchor == QL::kBottomCenter) ? 0.5 : 1.0;
		fy = (anchor == QL::kTopLeft || anchor == QL::kTopCenter || anchor == QL::kTopRight) ? 0.0 :
			 (anchor == QL::kCenterLeft || anchor == QL::kCenter || anchor == QL::kCenterRight) ? 0.5 : 1.0;
	}

	const char * ActionLabel(int action, bool stealing, const char * useLabel)
	{
		switch(action)
		{
		case QL::Input::kTake:		return stealing ? QL::Engine::GameSetting("sSteal", "Steal") : QL::Engine::GameSetting("sTake", "Take");
		case QL::Input::kTakeAll:	return QL::Engine::GameSetting("sTakeAll", "Take All");
		case QL::Input::kTransfer:	return stealing ? QL::Engine::GameSetting("sStealFrom", "Steal From") : QL::Engine::GameSetting("sSearch", "Search");
		case QL::Input::kUse:		return useLabel ? useLabel : "$qlie_Use";
		default:					return "<invalid>";
		}
	}

	class LootMenu : public IMenu
	{
	public:
		LootMenu()
		{
			GFxLoader * loader = *(GFxLoader **)0x01B2E9B0;		// SKSE LE GFxLoader::GetSingleton
			if(loader) CALL_MEMBER_FN(loader, LoadMovie)(this, &view, kMovieName, 1, 0.0f);
			flags = kMenuFlags;
			unk0C = kMenuDepth;
			s_menu = this;
		}

		virtual ~LootMenu()
		{
			if(s_menu == this) s_menu = 0;
			if(s_state)
			{
				State * s = s_state;
				s_state = 0;
				s->~State();
				ScaleformHeap_Free(s);
			}
		}

		// Display only: input never goes to the movie (SE: SetMouseCursorCount(0)).
		virtual UInt32 ProcessUnkData1(MenuMessagePointer) { return 2; }

		virtual void NextFrame(UInt32 arg0, UInt32 arg1);
	};

	// ------------------------------------------------------------------ bodies
	void Setup()
	{
		if(!s_menu || !s_menu->view) return;
		GFxMovieView * v = s_menu->view;
		void * mem = ScaleformHeap_Allocate(sizeof(State));
		if(!mem) return;
		State * s = new (mem) State();
		s->valid = false; s->initialized = false; s->viewGeneration = 0; s->buttonGeneration = 0;
		s->gamepad = false; s->selected = -1; s->settingsGeneration = 0;
		s->view.generation = 0; s->view.show = false; s->view.container = 0; s->view.reinit = false;
		s->view.selectIndex = -1; s->view.stealing = false;
		s_state = s;

		GFxValue t;
		t.SetBool(true);
		v->SetVariable("_global.gfxExtensions", &t, 0);

		bool ok = v->GetVariable(&s->root, "_root.lootMenu") && s->root.IsDisplayObject();
		ok = v->GetVariable(&s->title, "_root.lootMenu.title") && ok;
		ok = v->GetVariable(&s->weight, "_root.lootMenu.weight") && ok;
		ok = v->GetVariable(&s->itemList, "_root.lootMenu.itemList") && ok;
		ok = v->GetVariable(&s->infoBar, "_root.lootMenu.infoBar") && ok;
		ok = v->GetVariable(&s->buttonBar, "_root.lootMenu.buttonBar") && ok;
		if(!ok)
		{
			QL_LOG("LootMenuIE.swf: a required element is missing - the menu stays empty");
			return;
		}
		GFxValue version;
		if(s->root.Invoke("getVersion", &version, 0, 0) && version.GetType() == GFxValue::kType_Number)
			InterlockedExchange(&s_featureLevel, (LONG)version.GetNumber());
		QL_LOG("LootMenuIE.swf loaded, feature level %ld", s_featureLevel);

		SetBool(s->root, "_visible", false);
		GFxValue left;
		v->CreateString(&left, "left");
		s->title.SetMember("autoSize", &left);
		s->weight.SetMember("autoSize", &left);

		v->CreateArray(&s->itemProvider);
		v->CreateArray(&s->infoProvider);
		v->CreateArray(&s->buttonProvider);
		s->itemList.SetMember("dataProvider", &s->itemProvider);
		s->infoBar.SetMember("dataProvider", &s->infoProvider);
		s->buttonBar.SetMember("dataProvider", &s->buttonProvider);
		s->valid = true;
	}

	void InitMovie(State * s)
	{
		GFxMovieView * v = s_menu->view;
		const QL::Settings & c = s->settings;
		GFxValue settings;
		v->CreateObject(&settings);
		SetNumber(settings, "minLines", c.minLines);
		SetNumber(settings, "maxLines", c.maxLines);
		SetNumber(settings, "offsetX", c.windowX);
		SetNumber(settings, "offsetY", c.windowY);
		SetNumber(settings, "scale", c.windowScale);
		SetNumber(settings, "alphaNormal", c.opacityNormal);
		SetNumber(settings, "alphaEmpty", c.opacityEmpty);
		double fx, fy;
		ResolveAnchor(c.windowAnchor, fx, fy);
		SetNumber(settings, "anchorFractionX", fx);
		SetNumber(settings, "anchorFractionY", fy);
		GFxValue columns;
		v->CreateArray(&columns);
		for(size_t i = 0; i < c.infoColumns.size(); ++i)
		{
			GFxValue col;
			v->CreateString(&col, c.infoColumns[i].c_str());
			columns.PushBack(&col);
		}
		settings.SetMember("infoColumns", &columns);
		SetBool(settings, "showItemIcons", c.showItemIcons);
		GFxValue system;
		v->CreateObject(&system);
		settings.SetMember("systemSettings", &system);
		s->root.Invoke("init", 0, &settings, 1);
		s->initialized = true;
	}

	void RefreshButtonBar(State * s)
	{
		GFxMovieView * v = s_menu->view;
		ClearArray(s->buttonProvider);
		std::vector<QL::Input::Button> buttons;
		QL::Input::ButtonBar(s->gamepad, buttons);
		const QL::Row * row = (s->selected >= 0 && s->selected < (int)s->view.rows.size()) ? &s->view.rows[s->selected] : 0;
		for(size_t i = 0; i < buttons.size(); ++i)
		{
			std::string label = ActionLabel(buttons[i].action, s->view.stealing, row ? row->useLabel : 0);
			if(buttons[i].hold) label = "$qlie_Hold{" + label + "}";
			GFxValue obj;
			v->CreateObject(&obj);
			SetString(v, obj, "label", label.c_str());
			SetNumber(obj, "index", buttons[i].artIndex);
			SetBool(obj, "stolen", s->view.stealing);
			s->buttonProvider.PushBack(&obj);
		}
		s->buttonBar.Invoke("invalidateData", 0, 0, 0);
		s->buttonGeneration = QL::Input::ButtonBarGeneration();
	}

	void SetSelected(State * s, int index, bool sound)
	{
		const int count = (int)s->view.rows.size();
		if(index < 0) index = 0;
		if(index >= count) index = count - 1;		// -1 when empty
		if(index != s->selected && sound && s->settings.playScrollSound) QL::Engine::PlaySound("UIMenuFocus");
		s->selected = index;
		SetNumber(s->itemList, "selectedIndex", index);
		QL::Loot::ReportSelection(s->view.container, index);
		RefreshButtonBar(s);
		ClearArray(s->infoProvider);		// info bar lines come only from API subscribers on SE; none on LE
		s->infoBar.Invoke("invalidateData", 0, 0, 0);
	}

	void ApplyView(State * s)
	{
		GFxMovieView * v = s_menu->view;
		QL::Loot::CopyView(s->view);
		s->viewGeneration = s->view.generation;
		if(!s->view.show)
		{
			SetBool(s->root, "_visible", false);
			QL::Input::ClearUIActions();
			return;
		}

		if(s->settingsGeneration != QL::Config::Generation())
		{
			QL::Config::Copy(s->settings);
			s->settingsGeneration = QL::Config::Generation();
			if(s_featureLevel >= 4) s->initialized = false;	// re-init picks the new window settings up
		}
		// LootMenu::Show: init on the first show, and on every new container (or new
		// settings) when the movie supports re-initialisation (feature level 4+).
		if(!s->initialized || (s->view.reinit && s_featureLevel >= 4)) InitMovie(s);

		ClearArray(s->itemProvider);
		for(size_t i = 0; i < s->view.rows.size(); ++i)
		{
			const QL::Row & r = s->view.rows[i];
			GFxValue obj;
			v->CreateObject(&obj);
			SetString(v, obj, "text", r.displayName.c_str());
			SetString(v, obj, "displayName", r.displayName.c_str());
			SetNumber(obj, "count", r.count);
			SetNumber(obj, "value", r.value);
			SetNumber(obj, "weight", r.weight);
			SetBool(obj, "stolen", r.stealing);
			SetBool(obj, "isStealing", r.stealing);
			SetBool(obj, "read", r.read);
			SetBool(obj, "enchanted", r.enchanted);
			SetBool(obj, "knownEnchanted", r.knownEnchanted);
			SetBool(obj, "specialEnchanted", r.specialEnchanted);
			SetBool(obj, "bestInClass", r.bestInClass);
			SetNumber(obj, "formType", r.formType);
			SetNumber(obj, "formId", r.formID);
			if(r.iconLabel && r.iconLabel[0]) SetString(v, obj, "iconLabel", r.iconLabel);
			if(r.iconColor >= 0) SetNumber(obj, "iconColor", r.iconColor);
			SetBool(obj, "quickLoot", true);
			s->itemProvider.PushBack(&obj);
		}
		s->itemList.Invoke("invalidateData", 0, 0, 0);
		s->root.Invoke("refresh", 0, 0, 0);

		GFxValue text;
		MakeString(v, text, s->view.weight.c_str());
		s->weight.SetMember("htmlText", &text);
		SetBool(s->weight, "_visible", true);
		MakeString(v, text, s->view.title.c_str());
		s->title.SetMember("htmlText", &text);
		SetBool(s->title, "_visible", true);

		SetSelected(s, s->view.reinit ? s->view.selectIndex : s->selected, false);
		SetBool(s->root, "_visible", true);
	}

	void RunUIActions(State * s)
	{
		int action;
		while(QL::Input::PopUIAction(action))
		{
			if(!s->view.show) continue;
			if(QL::Engine::IsMenuOpen("Console")) continue;
			const QL::Row * row = (s->selected >= 0 && s->selected < (int)s->view.rows.size()) ? &s->view.rows[s->selected] : 0;
			switch(action)
			{
			case QL::Input::kScrollUp:		if(s->selected > 0) SetSelected(s, s->selected - 1, true); break;
			case QL::Input::kScrollDown:	if(s->selected < (int)s->view.rows.size() - 1) SetSelected(s, s->selected + 1, true); break;
			case QL::Input::kTake:			if(row) QL::Loot::Post(QL::Loot::kTake, s->view.container, row->formID, row->dropHandle); break;
			case QL::Input::kUse:			if(row) QL::Loot::Post(QL::Loot::kUse, s->view.container, row->formID, row->dropHandle); break;
			case QL::Input::kTakeAll:		QL::Loot::Post(QL::Loot::kTakeAll, s->view.container, 0, 0); break;
			case QL::Input::kTransfer:		QL::Loot::Post(QL::Loot::kTransfer, s->view.container, 0, 0); break;
			default: break;
			}
		}
	}

	void FrameBody()
	{
		QL::Log::Thread("the loot menu frame");
		if(!s_state) Setup();
		State * s = s_state;
		if(!s || !s->valid) return;
		if(QL::Guard::Disabled())
		{
			SetBool(s->root, "_visible", false);
			return;
		}
		if(s->viewGeneration != QL::Loot::ViewGeneration()) ApplyView(s);
		RunUIActions(s);
		const bool pad = QL::Engine::GamepadInUse();
		if(s->view.show && (pad != s->gamepad || s->buttonGeneration != QL::Input::ButtonBarGeneration()))
		{
			s->gamepad = pad;
			RefreshButtonBar(s);		// LootMenu::RefreshPlatform
		}
	}

	void LootMenu::NextFrame(UInt32 arg0, UInt32 arg1)
	{
		QL::Guard::Run("loot menu frame", FrameBody, QL::Guard::kUI);
		IMenu::NextFrame(arg0, arg1);
	}

	void ApplyBody()
	{
		QL::Log::Thread("the UI task");
		State * s = s_state;
		if(!s_menu || !s || !s->valid || QL::Guard::Disabled()) return;
		if(s->viewGeneration != QL::Loot::ViewGeneration()) ApplyView(s);
	}

	IMenu * CreateBody()
	{
		void * memory = ScaleformHeap_Allocate(sizeof(LootMenu));
		return memory ? new (memory) LootMenu() : 0;
	}

	IMenu * s_created = 0;
	void CreateGuarded() { s_created = CreateBody(); }

	IMenu * Create()
	{
		s_created = 0;
		QL::Guard::Run("loot menu creation", CreateGuarded, QL::Guard::kUI);
		return s_created;
	}
}

namespace QL
{
	namespace Menu
	{
		void Register()
		{
			MenuManager * m = Engine::Menus();
			if(m) m->Register(Loot::kMenuName, Create);
		}

		int SwfFeatureLevel() { return (int)s_featureLevel; }

		void ApplyPending() { QL::Guard::Run("loot menu UI task", ApplyBody, QL::Guard::kUI); }
	}
}
