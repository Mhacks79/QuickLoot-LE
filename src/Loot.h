// QuickLoot IE LE - the game-thread half: when the loot menu shows, what it shows,
// and what a take does.
//
// Port of QuickLoot IE's MenuVisibilityManager, LootMenuManager and the action half
// of LootMenu/ItemStack. Every engine read and write happens inside one SKSE task
// (BSTaskPool, the game's own thread, rules\threads.md). Event sinks, the input
// thread and the menu only record what happened and ask for that task.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE (Miss Corruption and
// contributors). GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

#include "Items.h"

#include <string>
#include <vector>

struct SKSETaskInterface;

namespace QL
{
	namespace Loot
	{
		const char * const kMenuName = "LootMenu";

		void Init(SKSETaskInterface * tasks);	// kDataLoaded
		bool Ready();

		// ------------------------------------------------------------ inputs (any thread)
		void OnCrosshair(UInt32 handle);		// SKSE crosshair dispatcher
		void RequestRefresh();					// re-evaluate the open state
		void OnContainerChanged(UInt32 formID);
		void OnLoadBoundary();					// PreLoad/PostLoad/NewGame: drop everything
		void ToggleDisabledByKey(bool disabled);
		void OnSettingsChanged();
		void PollTick();						// HUD frame: cheap state probe (sneak)

		enum Action { kTake, kTakeAll, kUse, kTransfer, kGrab };
		void Post(Action action, UInt32 container, UInt32 formID, UInt32 dropHandle);

		// ------------------------------------------------------------ state for the hooks
		bool PromptBlocked();					// crosshair HUD message hook (game thread)
		bool LockpickActivationBlocked();		// lockpick re-activation hooks (game thread)
		bool IsShowing();						// any thread

		// ------------------------------------------------------------ the view the menu draws
		struct View
		{
			unsigned			generation;
			bool				show;
			UInt32				container;
			bool				reinit;			// a different container: re-run init/select
			int					selectIndex;	// for a new container
			bool				stealing;
			std::string			title;
			std::string			weight;
			std::string			refKey;			// "Plugin|0xID" of the reference (empty for a created one)
			std::string			baseKey;		// same for its base object
			std::vector<Row>	rows;
		};
		unsigned	ViewGeneration();
		void		CopyView(View & out);
		void		ReportSelection(UInt32 container, int index);	// from the menu
	}
}
