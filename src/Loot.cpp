// QuickLoot IE LE - game-thread core. See Loot.h.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE's MenuVisibilityManager.cpp,
// LootMenuManager.cpp, LootMenu.cpp and Items/ItemStack.cpp (Miss Corruption and
// contributors). GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Loot.h"
#include "Engine.h"
#include "Input.h"
#include "LootMenu.h"
#include "Guard.h"
#include "Log.h"

#include "skse/PluginAPI.h"
#include "skse/gamethreads.h"
#include "skse/Hooks_UI.h"
#include "skse/GameMenus.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

using namespace QL::Engine;

namespace
{
	SKSETaskInterface *	s_tasks		= 0;
	volatile LONG		s_ready		= 0;

	// ------------------------------------------------------------ pending work
	enum { kWorkVisibility = 1, kWorkInventory = 2, kWorkLoad = 4, kWorkSettings = 8 };
	volatile LONG		s_work		= 0;
	volatile LONG		s_queued	= 0;
	volatile LONG		s_focus		= 0;		// crosshair ref handle (0 = none)
	volatile LONG		s_disabledByKey = 0;
	volatile LONG		s_showing	= 0;
	volatile LONG		s_promptBlocked = 0;
	volatile LONG		s_lockpickBlocked = 1;
	volatile LONG		s_shownFormID = 0;		// formID of the container shown, for the container-changed filter

	struct PostedAction { int action; UInt32 container, formID, drop; };
	CRITICAL_SECTION	s_actionLock;
	PostedAction		s_actions[16];
	int					s_actionCount = 0;
	bool				s_actionOverflowLogged = false;

	// ------------------------------------------------------------ game-thread state (only the task touches these)
	QL::Settings		s_settings;
	unsigned			s_settingsGeneration = 0;
	std::vector<UInt32>	s_blacklist;
	UInt32				s_current		= 0;	// handle of the container shown
	UInt32				s_lastContainer	= 0;
	int					s_lastIndex		= 0;
	bool				s_lootedSent	= false;
	bool				s_wasSneaking	= false;

	// ------------------------------------------------------------ view
	CRITICAL_SECTION	s_viewLock;
	QL::Loot::View		s_view;
	volatile LONG		s_viewGeneration = 1;
	volatile LONG		s_uiSelection = 0;
	volatile LONG		s_uiSelectionContainer = 0;

	void Schedule(LONG work);

	// A new view is drawn by the menu on the UI thread (F3): one static delegate,
	// one in flight at a time (rules	hreads.md).
	volatile LONG s_uiQueued = 0;
	class UITask : public UIDelegate_v1
	{
	public:
		virtual void Run()		{ QL::Menu::ApplyPending(); }
		virtual void Dispose()	{ InterlockedExchange(&s_uiQueued, 0); }
	};
	UITask s_uiTask;

	class GameTask : public TaskDelegate
	{
	public:
		virtual void Run();
		virtual void Dispose() { InterlockedExchange(&s_queued, 0); }
	};
	GameTask s_task;	// namespace scope, never a function-local static (F4)

	void Schedule(LONG work)
	{
		if(work) InterlockedOr(&s_work, work);
		if(!s_tasks || !s_ready) return;
		if(InterlockedCompareExchange(&s_queued, 1, 0) != 0) return;
		s_tasks->AddTask(&s_task);
	}

	void RefreshSettings()
	{
		if(s_settingsGeneration == QL::Config::Generation()) return;
		s_settingsGeneration = QL::Config::Generation();
		QL::Config::Copy(s_settings);
		s_blacklist.clear();
		for(size_t i = 0; i < s_settings.containerBlacklist.size(); ++i)
		{
			const UInt32 id = ResolveFormID(s_settings.containerBlacklist[i].c_str());
			if(id) s_blacklist.push_back(id);
		}
		InterlockedExchange(&s_lockpickBlocked, s_settings.showWhenUnlocked ? 1 : 0);
		QL::Input::Reload(s_settings);
	}

	// ------------------------------------------------------------ publishing
	void Publish(bool show, UInt32 container, bool reinit, int select, const std::vector<QL::Row> * rows, TESObjectREFR * ref)
	{
		std::string title, weight, refKey, baseKey;
		bool stealing = false;
		if(show && ref)
		{
			const char * name = CALL_MEMBER_FN(ref, GetReferenceName)();
			title = name ? name : "";
			float w = 0, m = 0;
			Encumbrance(w, m);
			char buf[64];
			sprintf_s(buf, sizeof(buf), "%d / %d", (int)w, (int)m);
			weight = buf;
			char key[300];
			if(FormKey(ref->formID, key, sizeof(key))) refKey = key;
			if(Base(ref) && FormKey(Base(ref)->formID, key, sizeof(key))) baseKey = key;
			// PlayerCharacter::WouldBeStealing: a dead body is never stolen from; otherwise the
			// player must be an owner (or the reference unowned).
			if(!(IsActor(ref) && IsDead(static_cast<Actor *>(ref))) && GetOwner(ref))
				stealing = !IsAnOwner(ref, Player(), true, false);
		}
		EnterCriticalSection(&s_viewLock);
		s_view.show			= show;
		s_view.container	= container;
		s_view.reinit		= reinit;
		s_view.selectIndex	= select;
		s_view.stealing		= stealing;
		s_view.title.swap(title);
		s_view.weight.swap(weight);
		s_view.refKey.swap(refKey);
		s_view.baseKey.swap(baseKey);
		if(rows) s_view.rows = *rows; else s_view.rows.clear();
		s_view.generation	= (unsigned)InterlockedIncrement(&s_viewGeneration);
		LeaveCriticalSection(&s_viewLock);
		if(s_tasks && InterlockedCompareExchange(&s_uiQueued, 1, 0) == 0) s_tasks->AddUITask(&s_uiTask);
	}

	// ------------------------------------------------------------ MenuVisibilityManager
	TESObjectREFR * ContainerOf(UInt32 handle, int depth)
	{
		TESObjectREFR * ref = Lookup(handle);
		if(!ref) return 0;
		TESForm * base = Base(ref);
		if(base && base->formType == kFormType_Activator && depth < 2)
		{
			const UInt32 ash = AshPileHandle(ref);		// enemies that leave an ash pile
			Release(ref);
			return ash ? ContainerOf(ash, depth + 1) : 0;
		}
		if(HasContainer(ref)) return ref;
		Release(ref);
		return 0;
	}

	bool ValidCamera()
	{
		const UInt32 id = CameraStateId();
		if(id == 0xFFFFFFFF) return true;
		switch(id)
		{
		case 0:			return true;							// first person
		case 8: case 9:	return s_settings.showInThirdPerson;	// third person (LE has two states)
		case 10:		return s_settings.showWhenMounted;		// horse
		default:		return false;
		}
	}

	bool BlockingMenuOpen()
	{
		IMenu * open[64];
		const UInt32 n = OpenMenus(open, 64);
		if(!n) return false;
		IMenu * allowed[72];
		UInt32 a = 0;
		IMenu * m;
		if((m = GetMenu(QL::Loot::kMenuName)) != 0) allowed[a++] = m;
		if((m = GetMenu("Cursor Menu")) != 0) allowed[a++] = m;
		for(size_t i = 0; i < s_settings.menuWhitelist.size() && a < 72; ++i)
			if((m = GetMenu(s_settings.menuWhitelist[i].c_str())) != 0) allowed[a++] = m;
		for(UInt32 i = 0; i < n; ++i)
		{
			if(open[i]->flags & 0x2) continue;		// always open (HUD, fader...)
			bool ok = false;
			for(UInt32 j = 0; j < a; ++j) if(allowed[j] == open[i]) { ok = true; break; }
			if(!ok) return true;
		}
		return false;
	}

	bool Blacklisted(TESObjectREFR * ref)
	{
		for(size_t i = 0; i < s_blacklist.size(); ++i)
		{
			if(s_blacklist[i] == ref->formID) return true;
			TESForm * base = Base(ref);
			if(base && s_blacklist[i] == base->formID) return true;
		}
		return false;
	}

	const char * CanOpen(TESObjectREFR * ref)
	{
		PlayerCharacter * player = Player();
		if(!player) return "no player";
		if(IsLocked(ref)) return "locked";
		if(IsActivationBlocked(ref)) return "activation blocked";
		if(!s_settings.showInCombat && IsInCombat(player)) return "in combat";
		if(!s_settings.showWhenSneaking && IsSneaking(player)) return "sneaking";
		if(IsGrabbing()) return "grabbing";
		if(HasActorDoingCommand()) return "commanding a follower";
		if(InBeastForm()) return "beast form";
		if(!ValidCamera()) return "camera state";
		if(Blacklisted(ref)) return "blacklisted";
		if(IsActor(ref) ? HasKeyword(static_cast<Actor *>(ref), "QuickLootIE_Exclude") : HasKeywordForm(Base(ref), "QuickLootIE_Exclude"))
			return "QuickLootIE_Exclude";
		if(BlockingMenuOpen()) return "a menu is open";
		if(s_disabledByKey) return "disabled by key";
		if(IsActor(ref))
		{
			Actor * actor = static_cast<Actor *>(ref);
			if(!IsDead(actor)) return "alive";
			if(IsSummoned(actor)) return "summoned";
			if(!s_settings.enableForCorpses) return "corpses off";
			if(!s_settings.enableForAnimals && HasKeyword(actor, "ActorTypeAnimal")) return "animals off";
			if(!s_settings.enableForDragons && HasKeyword(actor, "ActorTypeDragon")) return "dragons off";
		}
		else if(!s_settings.enableForContainers) return "containers off";
		return 0;
	}

	void Hide()
	{
		if(!s_current) return;
		if(s_uiSelectionContainer == (LONG)s_current)
		{
			s_lastContainer	= s_current;
			s_lastIndex		= s_uiSelection;
		}
		InterlockedExchange(&s_promptBlocked, 0);
		QL::Input::RestoreControlGroups();
		ToggleControls(QL::Input::kGroupFlag, true);
		s_current = 0;
		InterlockedExchange(&s_shownFormID, 0);
		InterlockedExchange(&s_showing, 0);
		Publish(false, 0, false, -1, 0, 0);
		if(IsMenuOpen(QL::Loot::kMenuName)) PostMenuMessage(QL::Loot::kMenuName, UIMessage::kMessage_Close);
		QL_LOG("hide");
	}

	void ShowOrRefresh(TESObjectREFR * ref, bool newContainer)
	{
		std::vector<QL::Row> rows;
		QL::Items::Build(ref, s_settings, rows);
		const UInt32 handle = HandleOf(ref);

		if(rows.empty() && !s_settings.showWhenEmpty)
		{
			if(s_current) Hide();
			return;
		}

		int select = -1;
		if(newContainer)
		{
			if(s_current && s_uiSelectionContainer == (LONG)s_current)
			{
				s_lastContainer = s_current;
				s_lastIndex = s_uiSelection;
			}
			select = (handle == s_lastContainer) ? s_lastIndex : 0;
			s_current = handle;
			InterlockedExchange(&s_shownFormID, (LONG)ref->formID);
			s_lootedSent = false;
			InterlockedExchange(&s_promptBlocked, s_settings.suppressActivationPrompt ? 1 : 0);
			QL::Input::ApplyControlGroups();
			ToggleControls(QL::Input::kGroupFlag, false);
			InterlockedExchange(&s_showing, 1);
			if(!IsMenuOpen(QL::Loot::kMenuName)) PostMenuMessage(QL::Loot::kMenuName, UIMessage::kMessage_Open);
			QL_LOG("show %08X (%u items)", ref->formID, (unsigned)rows.size());
		}
		Publish(true, s_current, newContainer, select, &rows, ref);
	}

	void RefreshOpenState()
	{
		if(IsMenuOpen("Console")) return;		// SE: any menu event under the console loses the cursor
		TESObjectREFR * ref = ContainerOf((UInt32)s_focus, 0);
		const char * why = ref ? CanOpen(ref) : "nothing in the crosshair";
		if(!why)
		{
			const UInt32 handle = HandleOf(ref);
			if(handle != s_current) ShowOrRefresh(ref, true);
		}
		else
		{
			if(s_current) QL_LOG("closed: %s", why);
			Hide();
		}
		if(ref) Release(ref);
	}

	void RefreshInventory()
	{
		if(!s_current) return;
		TESObjectREFR * ref = Lookup(s_current);
		if(!ref) { Hide(); return; }
		ShowOrRefresh(ref, false);
		Release(ref);
	}

	// ------------------------------------------------------------ the take (ItemStack::Take)
	bool BuildEntry(TESObjectREFR * container, TESForm * object, RawEntry & out)
	{
		out.type = object;
		out.extendDataList = 0;
		out.countDelta = 0;
		ExtraContainerChanges::Data * changes = Changes(container);
		bool leveled = false;
		if(changes && Ptr(changes->objList))
		{
			UInt32 guard = 0;
			for(ListNodeIter it(changes->objList); it.Valid() && guard++ < 8192; it.Next())
			{
				InventoryEntryData * e = (InventoryEntryData *)it.Item();
				if(Ptr(e) && e->type == object)
				{
					out.extendDataList = Ptr(e->extendDataList) ? e->extendDataList : 0;
					out.countDelta = e->countDelta;
					leveled = EntryHasExtra(e, kExtraData_LeveledItem);
					break;
				}
			}
		}
		TESContainer * base = BaseContainer(container);
		if(base && Ptr(base->entries) && !leveled)
			for(UInt32 i = 0; i < base->numEntries && i < 65536; ++i)
			{
				TESContainer::Entry * c = base->entries[i];
				if(Ptr(c) && c->form == object) out.countDelta += (SInt32)c->count;
			}
		return out.countDelta > 0;
	}

	void TakeFromContainer(TESObjectREFR * container, TESForm * object, SInt32 wanted)
	{
		RawEntry entry;
		if(!BuildEntry(container, object, entry)) return;
		const SInt32 count = wanted > 0 && wanted < entry.countDelta ? wanted : entry.countDelta;
		PlayerCharacter * player = Player();
		Actor * actor = IsActor(container) ? static_cast<Actor *>(container) : 0;
		const SInt32 value = EntryValue(AsEntry(entry));
		// ItemStack::IsStealing
		bool stealing = false;
		TESForm * owner = GetOwner(container);
		if(!(actor && IsDead(actor)) && (actor || owner))
		{
			TESForm * itemOwner = EntryOwner(AsEntry(entry));
			if(!itemOwner) itemOwner = actor ? (TESForm *)actor : owner;
			stealing = owner && !AllowedToTake(itemOwner, value);
		}
		BaseExtraList * extra = ExtraForRemoval(AsEntry(entry), count);
		const UInt32 type = stealing ? 1 : (actor && IsDead(actor)) ? 6 : 5;		// AQUIRE_TYPE steal / dead body / container
		AddPlayerAddItemEvent(object, owner, container, type);
		typedef void (__thiscall * _Sound)(PlayerCharacter *, TESForm *, bool, bool);
		(*(_Sound *)(*(UInt8 **)player + 0x288))(player, object, true, false);		// PlayPickUpSound, slot 0xA2
		RemoveItem(container, object, count, stealing ? 1 : 4, extra, player);
		HandleRemoveFromContainer(object, container);
		if(actor) ClearActorEquipment(actor);
		if(stealing) StealAlarm(player, container, object, count, value, owner);
		if(!actor && !s_lootedSent) { SendChestsLooted(); s_lootedSent = true; }
	}

	void TakeDropped(UInt32 dropHandle)
	{
		TESObjectREFR * ref = Lookup(dropHandle);
		if(!ref) return;
		if(!IsDeletedOrDisabled(ref)) PickUpObject(ref, RefCount(ref));
		Release(ref);
	}

	void UseItem(TESObjectREFR * container, TESForm * object, UInt32 drop)
	{
		const UInt8 type = TypeOf(object);
		const bool whole = type == kFormType_Book || type == kFormType_Ammo;
		if(drop) TakeDropped(drop);
		else TakeFromContainer(container, object, whole ? 0 : 1);
		InventoryEntryData * mine = PlayerEntry(object);
		if(!mine) return;
		if(type == kFormType_Book)
		{
			TESObjectBOOK * book = static_cast<TESObjectBOOK *>(object);
			if(book->data.flags & TESObjectBOOK::Data::kType_Spell)
			{
				if(ReadSpellBook(book))
					RemoveItem(Player(), book, 1, 0, 0, 0);
			}
			else
			{
				BaseExtraList * extra = 0;
				if(Ptr(mine->extendDataList))
				{
					ListNodeIter it(mine->extendDataList);
					if(it.Valid() && Ptr(it.Item())) extra = (BaseExtraList *)it.Item();
				}
				OpenBook(book, extra);
			}
			return;
		}
		EquipEntry(mine);
	}

	void RunAction(const PostedAction & a)
	{
		if(!s_current || a.container != s_current) return;
		TESObjectREFR * container = Lookup(s_current);
		if(!container) { Hide(); return; }
		PlayerCharacter * player = Player();
		TESForm * object = a.formID ? LookupFormByID(a.formID) : 0;
		switch(a.action)
		{
		case QL::Loot::kTake:
			if(a.drop) TakeDropped(a.drop);
			else if(Ptr(object)) TakeFromContainer(container, object, 0);
			break;
		case QL::Loot::kTakeAll:
		{
			std::vector<QL::Row> rows;
			QL::Items::Build(container, s_settings, rows);
			for(size_t i = 0; i < rows.size(); ++i)
			{
				if(rows[i].dropHandle) TakeDropped(rows[i].dropHandle);
				else
				{
					TESForm * f = LookupFormByID(rows[i].formID);
					if(Ptr(f)) TakeFromContainer(container, f, 0);
				}
			}
			break;
		}
		case QL::Loot::kUse:
			if(Ptr(object) || a.drop) UseItem(container, object, a.drop);
			break;
		case QL::Loot::kTransfer:
		{
			typedef void (__thiscall * _ActivatePick)(PlayerCharacter *);
			((_ActivatePick)0x00743530)(player);		// F-24 PlayerCharacter::ActivatePickRef
			break;
		}
		case QL::Loot::kGrab:
		{
			StartGrab();
			const bool grabbed = player && *(UInt32 *)((UInt8 *)player + 0x6D4) != 0;
			if(grabbed)
			{
				MarkActivateHeld(true);
				Release(container);
				Hide();
				return;
			}
			break;
		}
		}
		if(a.action == QL::Loot::kTake || a.action == QL::Loot::kTakeAll || a.action == QL::Loot::kUse)
		{
			if(s_settings.breakInvisibility) DispelInvisibility(player);
			InterlockedOr(&s_work, kWorkInventory);
		}
		Release(container);
	}

	void RunBody()
	{
		QL::Log::Thread("the game task");
		RefreshSettings();

		const LONG work = InterlockedExchange(&s_work, 0);
		if(work & kWorkLoad)
		{
			InterlockedExchange(&s_promptBlocked, 0);
			QL::Input::RestoreControlGroups();
			ToggleControls(QL::Input::kGroupFlag, true);	// a save taken while shown carries the cleared bit (F6)
			s_current = 0;
			s_lastContainer = 0;
			InterlockedExchange(&s_showing, 0);
			Publish(false, 0, false, -1, 0, 0);
		}

		PostedAction actions[16];
		int n = 0;
		EnterCriticalSection(&s_actionLock);
		n = s_actionCount;
		for(int i = 0; i < n; ++i) actions[i] = s_actions[i];
		s_actionCount = 0;
		LeaveCriticalSection(&s_actionLock);
		for(int i = 0; i < n; ++i) RunAction(actions[i]);

		if(work & (kWorkVisibility | kWorkSettings)) RefreshOpenState();
		if((s_work & kWorkInventory) || (work & (kWorkInventory | kWorkSettings)))
		{
			InterlockedAnd(&s_work, ~kWorkInventory);
			RefreshInventory();
		}
	}

	void GameTask::Run()
	{
		QL::Guard::Run("game task", RunBody, QL::Guard::kGame);
	}
}

namespace QL
{
	namespace Loot
	{
		void Init(SKSETaskInterface * tasks)
		{
			InitializeCriticalSection(&s_actionLock);
			InitializeCriticalSection(&s_viewLock);
			s_view.generation = 1;
			s_view.show = false;
			s_view.container = 0;
			s_view.reinit = false;
			s_view.selectIndex = -1;
			s_view.stealing = false;
			s_tasks = tasks;
			Config::Copy(s_settings);
			s_settingsGeneration = 0;
			InterlockedExchange(&s_ready, 1);
		}

		bool Ready() { return s_ready != 0; }

		void OnCrosshair(UInt32 handle)
		{
			if(!s_ready || Guard::Disabled()) return;
			if(InterlockedExchange(&s_focus, (LONG)handle) != (LONG)handle) Schedule(kWorkVisibility);
		}

		void RequestRefresh()
		{
			if(s_ready && !Guard::Disabled()) Schedule(kWorkVisibility);
		}

		void OnContainerChanged(UInt32 formID)
		{
			if(!s_ready || !s_showing || Guard::Disabled()) return;
			if(formID && (LONG)formID == s_shownFormID) Schedule(kWorkInventory);	// MenuVisibilityManager::OnContainerChanged
		}

		void OnLoadBoundary()
		{
			InterlockedExchange(&s_focus, 0);
			if(s_ready) Schedule(kWorkLoad);
		}

		void ToggleDisabledByKey(bool disabled)
		{
			InterlockedExchange(&s_disabledByKey, disabled ? 1 : 0);
			RequestRefresh();
		}

		void OnSettingsChanged()
		{
			if(s_ready) Schedule(kWorkSettings);
		}

		void PollTick()
		{
			// SE patched ActorState::SetMoveModeBits; on LE the HUD's frame looks at the flag
			// instead (covers script-driven sneak too). One flag read per frame.
			if(!s_ready || Guard::Disabled()) return;
			PlayerCharacter * p = Player();
			if(!p) return;
			const bool sneaking = (*(UInt32 *)((UInt8 *)p + 0x68) & 0x600) == 0x200;	// ActorState flags04: sneak, not swimming
			if(sneaking != s_wasSneaking)
			{
				s_wasSneaking = sneaking;
				if(s_focus) Schedule(kWorkVisibility);
			}
		}

		void Post(Action action, UInt32 container, UInt32 formID, UInt32 dropHandle)
		{
			if(!s_ready || Guard::Disabled()) return;
			EnterCriticalSection(&s_actionLock);
			if(s_actionCount < 16)
			{
				PostedAction & a = s_actions[s_actionCount++];
				a.action = action; a.container = container; a.formID = formID; a.drop = dropHandle;
			}
			else if(!s_actionOverflowLogged)
			{
				s_actionOverflowLogged = true;
				QL_LOG("action queue full: an input was dropped (fails closed, nothing taken)");
			}
			LeaveCriticalSection(&s_actionLock);
			Schedule(0);
		}

		bool PromptBlocked()				{ return s_promptBlocked != 0; }
		bool LockpickActivationBlocked()	{ return s_lockpickBlocked != 0 && !Guard::Disabled(); }
		bool IsShowing()					{ return s_showing != 0; }

		unsigned ViewGeneration() { return (unsigned)s_viewGeneration; }

		void CopyView(View & out)
		{
			EnterCriticalSection(&s_viewLock);
			out = s_view;
			LeaveCriticalSection(&s_viewLock);
		}

		void ReportSelection(UInt32 container, int index)
		{
			InterlockedExchange(&s_uiSelection, index);
			InterlockedExchange(&s_uiSelectionContainer, (LONG)container);
		}
	}
}
