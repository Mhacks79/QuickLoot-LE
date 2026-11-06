// QuickLoot IE LE - the item rows the loot menu shows.
//
// Port of QuickLoot IE's Items/ (ItemStack, ItemStack.SkyUiData, ItemStack.SkyUiIcons,
// QuickLootItemStack, Inventory) and LootMenu.Sorting. The Special Edition original
// kept live InventoryEntryData objects inside the menu; here everything the menu needs
// is computed once, on the game's thread, into plain values (Row), and the menu only
// ever sees those. A row names its item by base FormID (and drop handle), so the take
// is resolved again on the game's thread against the container as it is then.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE (Miss Corruption and
// contributors; the SkyUI tables follow SkyUI by schlangster).
// GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

#include "Settings.h"

#include <string>
#include <vector>

class TESObjectREFR;
class TESForm;

namespace QL
{
	struct Row
	{
		UInt32		formID;			// base object
		UInt32		dropHandle;		// 0 unless the stack is a dropped reference
		SInt32		count;
		std::string	displayName;
		SInt32		value;
		float		weight;			// >= 0
		bool		stealing;
		bool		read;
		bool		enchanted;
		bool		knownEnchanted;
		bool		specialEnchanted;
		bool		bestInClass;
		const char * iconLabel;
		SInt32		iconColor;		// -1 = none
		const char * useLabel;

		// sort keys (SkyUI classification)
		UInt8		formType;
		int			itemType;
		int			miscType;		// -1 unset
		int			potionType;		// -1 unset
		int			bookType;		// -1 unset
		int			weightClass;	// -1 unset
	};

	namespace Items
	{
		void Init();	// resolves the known forms; game thread, after the data has loaded

		// Builds the rows for a container, sorted. Game thread only.
		void Build(TESObjectREFR * container, const Settings & s, std::vector<Row> & out);

		// Sort helper exposed for the offline tests.
		void Sort(std::vector<Row> & rows, const std::vector<std::string> & rules);

		bool CanDisplay(TESForm * object);
	}
}
