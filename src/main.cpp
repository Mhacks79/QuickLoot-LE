// QuickLoot IE LE
//
// Look into a container or a corpse and see what is inside without opening it; take
// one item, take everything, or open it as usual - for Skyrim Legendary Edition
// 1.9.32.0.8 with SKSE 1.7.3.
//
// This is a port of QuickLoot IE 4.1.3 by Miss Corruption (with the work of the
// contributors credited in Source\original), itself based on QuickLootEE and
// QuickLootRE. QuickLoot IE is licensed GPL-3.0-or-later WITH the Modding and the
// GPL-3.0 Linking exceptions; this port is released under the same terms. The
// original MIT notice of QuickLootRE (c) 2018 ryan-rsm-mckenzie is kept in MIT.
//
// Modified by Mhacks79 for Skyrim LE, 2026-11-06. What changed, file by file, is listed
// in CHANGES.md.
//
// Copyright (C) 2026 Mhacks79.

#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/GameAPI.h"
#include "skse/GameEvents.h"
#include "skse/GameMenus.h"
#include "skse/PapyrusEvents.h"

#include "Settings.h"
#include "Engine.h"
#include "Items.h"
#include "Loot.h"
#include "LootMenu.h"
#include "Input.h"
#include "Hooks.h"
#include "Guard.h"
#include "Khaos.h"
#include "Log.h"
#include "Version.h"
#include "Verify.h"

#include <string.h>

// Script event payloads SKSE LE does not declare (layouts not read by this plugin).
struct TESGrabReleaseEvent { TESObjectREFR * ref; bool grabbed; };
struct TESLockChangedEvent { TESObjectREFR * ref; };
template<> BSTEventSink<TESGrabReleaseEvent>::~BSTEventSink() {}
template<> BSTEventSink<TESLockChangedEvent>::~BSTEventSink() {}
template<> EventResult BSTEventSink<TESGrabReleaseEvent>::ReceiveEvent(TESGrabReleaseEvent *, EventDispatcher<TESGrabReleaseEvent> *) { return kEvent_Continue; }
template<> EventResult BSTEventSink<TESLockChangedEvent>::ReceiveEvent(TESLockChangedEvent *, EventDispatcher<TESLockChangedEvent> *) { return kEvent_Continue; }

namespace
{
	PluginHandle				s_handle	= kPluginHandle_Invalid;
	SKSEMessagingInterface *	s_messaging	= 0;
	SKSETaskInterface *			s_tasks		= 0;
	bool						s_installed	= false;

	EventDispatcher<TESGrabReleaseEvent> *	const kGrabReleaseDispatcher	= (EventDispatcher<TESGrabReleaseEvent> *)0x012E4F30;	// SKSE LE GameEvents.cpp
	EventDispatcher<TESLockChangedEvent> *	const kLockChangedDispatcher	= (EventDispatcher<TESLockChangedEvent> *)0x012E4FF0;

	// ------------------------------------------------------------------ sinks
	// Each only records and asks the game task to look; none touches the engine beyond
	// what the event itself hands over (Observers/*.cpp in the original).

	TESObjectREFR * s_crosshairRef = 0;	// guarded body argument
	void CrosshairBody()
	{
		QL::Log::Thread("the crosshair event");
		QL::Loot::OnCrosshair(s_crosshairRef ? QL::Engine::HandleOf(s_crosshairRef) : 0);
	}

	class CrosshairSink : public BSTEventSink<SKSECrosshairRefEvent>
	{
	public:
		virtual EventResult ReceiveEvent(SKSECrosshairRefEvent * e, EventDispatcher<SKSECrosshairRefEvent> *)
		{
			s_crosshairRef = (e && QL::Engine::Ptr(e->crosshairRef)) ? e->crosshairRef : 0;
			QL::Guard::Run("crosshair event", CrosshairBody, QL::Guard::kHook);
			return kEvent_Continue;
		}
	};

	class CameraSink : public BSTEventSink<SKSECameraEvent>
	{
	public:
		virtual EventResult ReceiveEvent(SKSECameraEvent *, EventDispatcher<SKSECameraEvent> *)
		{
			QL::Loot::RequestRefresh();
			return kEvent_Continue;
		}
	};

	class MenuSink : public BSTEventSink<MenuOpenCloseEvent>
	{
	public:
		virtual EventResult ReceiveEvent(MenuOpenCloseEvent * e, EventDispatcher<MenuOpenCloseEvent> *)
		{
			// Never react to the loot menu itself (feedback loop, MenuVisibilityManager).
			if(e && e->menuName.data && strcmp(e->menuName.data, QL::Loot::kMenuName) != 0)
				QL::Loot::RequestRefresh();
			return kEvent_Continue;
		}
	};

	class CombatSink : public BSTEventSink<TESCombatEvent>
	{
	public:
		virtual EventResult ReceiveEvent(TESCombatEvent * e, EventDispatcher<TESCombatEvent> *)
		{
			PlayerCharacter * p = QL::Engine::Player();
			if(e && p && (e->source == (TESObjectREFR *)p || e->target == (TESObjectREFR *)p))
				QL::Loot::RequestRefresh();
			return kEvent_Continue;
		}
	};

	class ContainerSink : public BSTEventSink<TESContainerChangedEvent>
	{
	public:
		virtual EventResult ReceiveEvent(TESContainerChangedEvent * e, EventDispatcher<TESContainerChangedEvent> *)
		{
			if(e)
			{
				if(e->fromFormId) QL::Loot::OnContainerChanged(e->fromFormId);
				if(e->toFormId) QL::Loot::OnContainerChanged(e->toFormId);
			}
			return kEvent_Continue;
		}
	};

	class DeathSink : public BSTEventSink<TESDeathEvent>
	{
	public:
		virtual EventResult ReceiveEvent(TESDeathEvent *, EventDispatcher<TESDeathEvent> *)
		{
			QL::Loot::RequestRefresh();		// SE patched Actor::SetLifeState; the death event covers the same moment
			return kEvent_Continue;
		}
	};

	class GrabSink : public BSTEventSink<TESGrabReleaseEvent>
	{
	public:
		virtual EventResult ReceiveEvent(TESGrabReleaseEvent *, EventDispatcher<TESGrabReleaseEvent> *)
		{
			QL::Loot::RequestRefresh();
			return kEvent_Continue;
		}
	};

	class LockSink : public BSTEventSink<TESLockChangedEvent>
	{
	public:
		virtual EventResult ReceiveEvent(TESLockChangedEvent *, EventDispatcher<TESLockChangedEvent> *)
		{
			QL::Loot::RequestRefresh();
			return kEvent_Continue;
		}
	};

	CrosshairSink	s_crosshairSink;
	CameraSink		s_cameraSink;
	MenuSink		s_menuSink;
	CombatSink		s_combatSink;
	ContainerSink	s_containerSink;
	DeathSink		s_deathSink;
	GrabSink		s_grabSink;
	LockSink		s_lockSink;

	void EmergencyRelease()
	{
		QL::Input::RestoreControlGroups();
		QL::Engine::ToggleControls(QL::Input::kGroupFlag, true);
	}

	void InstallBody()
	{
		QL::Log::Thread("kDataLoaded");
		s_installed = true;
		if(!QL::Verify::Run()) return;	// not the executable every address was read from: stay inert
		QL::Items::Init();
		QL::Loot::Init(s_tasks);
		QL::Menu::Register();
		QL::Input::Install();

		typedef EventDispatcher<SKSECrosshairRefEvent> CrosshairDispatcher;
		typedef EventDispatcher<SKSECameraEvent> CameraDispatcher;
		CrosshairDispatcher * crosshair = (CrosshairDispatcher *)s_messaging->GetEventDispatcher(SKSEMessagingInterface::kDispatcher_CrosshairEvent);
		CameraDispatcher * camera = (CameraDispatcher *)s_messaging->GetEventDispatcher(SKSEMessagingInterface::kDispatcher_CameraEvent);
		if(crosshair) crosshair->AddEventSink(&s_crosshairSink);
		else QL_LOG("no SKSE crosshair dispatcher: the loot menu cannot follow the crosshair");
		if(camera) camera->AddEventSink(&s_cameraSink);
		MenuManager * menus = QL::Engine::Menus();
		if(menus) menus->MenuOpenCloseEventDispatcher()->AddEventSink(&s_menuSink);
		g_combatEventDispatcher->AddEventSink(&s_combatSink);
		g_containerChangedEventDispatcher->AddEventSink(&s_containerSink);
		g_deathEventDispatcher->AddEventSink(&s_deathSink);
		kGrabReleaseDispatcher->AddEventSink(&s_grabSink);
		kLockChangedDispatcher->AddEventSink(&s_lockSink);

		QL::Hooks::Install();
		QL::Guard::SetCleanup(EmergencyRelease);
	}

	void OnMessage(SKSEMessagingInterface::Message * msg)
	{
		if(!msg) return;
		switch(msg->type)
		{
		case SKSEMessagingInterface::kMessage_DataLoaded:
			if(!s_installed) QL::Guard::Run("data loaded", InstallBody, QL::Guard::kGame);
			break;
		case SKSEMessagingInterface::kMessage_PreLoadGame:
		case SKSEMessagingInterface::kMessage_PostLoadGame:
		case SKSEMessagingInterface::kMessage_NewGame:
			if(s_installed) QL::Loot::OnLoadBoundary();
			break;
		default:
			break;
		}
	}
}

extern "C"
{
	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{
		QL::Config::Load();		// [Debug] EnableLog decides whether anything is ever written
		QL_LOG("QuickLoot IE LE %s", QL_VERSION_STRING);

		info->infoVersion	= PluginInfo::kInfoVersion;
		info->name			= "QuickLootIELE";
		info->version		= QL_VERSION_NUMBER;

		s_handle = skse->GetPluginHandle();
		if(skse->isEditor) return false;
		if(skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0)
		{
			QL_LOG("not loading: runtime %08X, built for 1.9.32.0 (%08X)", skse->runtimeVersion, RUNTIME_VERSION_1_9_32_0);
			return false;
		}
		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface * skse)
	{
		QL::Log::Thread("SKSEPlugin_Load");
		s_messaging = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
		s_tasks = (SKSETaskInterface *)skse->QueryInterface(kInterface_Task);
		if(!s_messaging || !s_tasks)
		{
			QL_LOG("not loading: SKSE messaging or task interface missing");
			return false;
		}
		if(!s_messaging->RegisterListener(s_handle, "SKSE", OnMessage))
		{
			QL_LOG("not loading: SKSE refused the message listener");
			return false;
		}
		QL::Khaos::Init();
		return true;
	}
}
