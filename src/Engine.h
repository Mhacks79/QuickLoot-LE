// QuickLoot IE LE - every engine address the port uses, in one place.
//
// Nothing here comes across from the Special Edition source: each address,
// offset and calling convention was read out of TESV.exe 1.9.32.0.8 (Ghidra and
// capstone) or taken from the SKSE LE 1.7.3 source, and the evidence for each is in
// quickloot-ie-le\evidence\FINDINGS.md (F-numbers below) and in CommonLibLE.
//
// Rules applied throughout (rules\FATAL.md):
//   F2  every pointer read from the engine is checked before it is followed,
//   F8  every raw call states its convention, read at a real call site,
//   F3  everything in here is called on the game's own thread (BSTaskPool), never
//       from the input, render or UI threads - see Loot.cpp.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

#include "skse/GameTypes.h"
#include "skse/GameForms.h"
#include "skse/GameObjects.h"
#include "skse/GameReferences.h"
#include "skse/GameExtraData.h"
#include "skse/GameBSExtraData.h"

class MenuManager;
class UIManager;
class IMenu;

namespace QL
{
	namespace Engine
	{
		// ---------------------------------------------------------------- checks
		inline bool Ptr(const void * p)
		{
			const UInt32 v = (UInt32)p;
			return v >= 0x10000 && v < 0xFFFF0000 && (v & 3) == 0;	// F2/F9: LAA heap may sit above 2 GB
		}

		// F2: committed and readable for n bytes (VirtualQuery). Used on every hand-walked chain.
		bool Readable(const void * p, UInt32 n);

		// tList<T> walk ({ T* item; node* next } at offset 0; the SDK keeps Head() private).
		struct ListNodeIter
		{
			struct Node { void * item; Node * next; };
			Node * n;
			explicit ListNodeIter(const void * list) : n((Node *)list) {}
			bool	Valid() const	{ return n && Ptr(n); }
			void *	Item() const	{ return n->item; }
			void	Next()			{ n = n->next; }
		};

		// InventoryEntryData's layout (0x0C) without the SDK's missing default constructor.
		struct RawEntry { TESForm * type; ExtendDataList * extendDataList; SInt32 countDelta; };
		inline InventoryEntryData * AsEntry(RawEntry & e) { return reinterpret_cast<InventoryEntryData *>(&e); }

		// ---------------------------------------------------------------- refs
		PlayerCharacter *	Player();
		UInt32				InvalidHandle();
		UInt32				HandleOf(TESObjectREFR * ref);		// CreateRefHandleByREFR 0x0065CC00
		TESObjectREFR *		Lookup(UInt32 handle);				// counted; Release() it
		void				Release(TESObjectREFR * ref);		// BSHandleRefObject decrement (+0x18, low 10 bits)

		inline UInt8 TypeOf(const TESForm * f) { return Ptr(f) ? f->formType : 0; }
		inline TESForm * Base(TESObjectREFR * r) { return Ptr(r) && Ptr(r->baseForm) ? r->baseForm : 0; }
		bool				IsActor(TESObjectREFR * r);
		bool				IsDeletedOrDisabled(TESObjectREFR * r);

		// ---------------------------------------------------------------- reference state
		bool				IsLocked(TESObjectREFR * r);			// F-03 0x004EB5B0
		bool				IsActivationBlocked(TESObjectREFR * r);	// F-04 0x0040E850
		TESForm *			GetOwner(TESObjectREFR * r);			// F-08 0x004EC0C0
		bool				HasContainer(TESObjectREFR * r);
		UInt32				AshPileHandle(TESObjectREFR * r);		// ExtraAshPileRef +0x08
		ExtraContainerChanges::Data * Changes(TESObjectREFR * r);
		TESContainer *		BaseContainer(TESObjectREFR * r);
		void				DroppedHandles(TESObjectREFR * r, UInt32 * out, UInt32 & count, UInt32 max);
		SInt32				RefCount(TESObjectREFR * r);			// ExtraCount, default 1

		// ---------------------------------------------------------------- actors
		bool				IsDead(Actor * a);						// vtable slot 0x99 (+0x264), arg 1
		bool				IsInCombat(Actor * a);					// F-13 slot 0xE2 (+0x388)
		bool				IsSneaking(Actor * a);					// 0x004D9290
		bool				IsSummoned(Actor * a);					// F-28: base ACBS flag 0x4000
		bool				HasKeyword(Actor * a, const char * editorId);

		// ---------------------------------------------------------------- player
		bool				IsGrabbing();							// F-14 handle +0x568
		bool				HasActorDoingCommand();				// F-15 0x0073D4B0
		bool				InBeastForm();							// F-16 MenuControls+0x41
		UInt32				CameraStateId();						// PlayerCamera [0x012E7288] +0x20 -> +0x0C
		void				Encumbrance(float & weight, float & max);	// F-26
		bool				AllowedToTake(TESForm * owner, SInt32 value);	// F-10 0x00745060
		bool				IsAnOwner(TESObjectREFR * r, Actor * who, bool useFaction, bool requireOwner);	// F-09
		void				StartGrab();							// F-24 0x0074FE50 (only when grab type is 0)
		void				MarkActivateHeld(bool grabbed);			// F-24 handler +0x0D/+0x0E
		void				ActivateRef(TESObjectREFR * target, TESObjectREFR * activator);	// F-02
		void				DispelInvisibility(Actor * a);			// F-27

		// ---------------------------------------------------------------- items
		const char *		EntryName(InventoryEntryData * e);		// SDK GenerateName 0x00475AA0
		SInt32				EntryValue(InventoryEntryData * e);		// SDK GetValue 0x00475450
		TESForm *			EntryOwner(InventoryEntryData * e);		// F-11 0x004755A0
		bool				EntryHasExtra(InventoryEntryData * e, UInt32 type);
		UInt32				EntrySoulLevel(InventoryEntryData * e);
		EnchantmentItem *	EntryEnchantment(InventoryEntryData * e);
		float				FormWeight(TESForm * f);				// SDK GetFormWeight
		float				ArmorValue(InventoryEntryData * e);		// SDK PlayerCharacter::GetArmorValue 0x007307E0
		float				Damage(InventoryEntryData * e);			// SDK PlayerCharacter::GetDamage 0x00730810
		bool				HasKeywordForm(TESForm * f, const char * editorId);
		bool				HasKeyword(TESForm * f, BGSKeyword * kw);
		const char *		FullName(TESForm * f);
		bool				IsPlayable(TESForm * f);
		const char *		ModelPath(TESForm * f);
		TESForm *			LookupForm(const char * plugin, UInt32 localId);	// 0 when the plugin is absent
		UInt32				ResolveFormID(const char * text);		// "Plugin.esp|0x123" or "0x00012345"
		bool				FormKey(UInt32 formID, char * out, size_t size);	// "Plugin.esp|0x123456"; false for created forms

		// ---------------------------------------------------------------- transfers (F-06/F-07)
		BaseExtraList *		ExtraForRemoval(InventoryEntryData * e, SInt32 count);	// 0x00868950
		void				RemoveItem(TESObjectREFR * from, TESForm * item, SInt32 count, UInt32 reason,
									   BaseExtraList * extra, TESObjectREFR * moveTo);	// slot 0x56
		void				HandleRemoveFromContainer(TESForm * item, TESObjectREFR * container);	// slot 0x4F
		void				AddPlayerAddItemEvent(TESForm * item, TESForm * owner, TESObjectREFR * container, UInt32 type);
		void				StealAlarm(Actor * thief, TESObjectREFR * ref, TESForm * item, SInt32 count, SInt32 value, TESForm * owner);
		void				PickUpObject(TESObjectREFR * object, SInt32 count);	// player slot 0xCB, playSound
		void				ClearActorEquipment(Actor * a);			// 0x00664B90 + 0x007031A0
		void				SendChestsLooted();						// F-12 0x0084A580
		InventoryEntryData * PlayerEntry(TESForm * item);
		void				EquipEntry(InventoryEntryData * playerEntry);	// F-30 0x006EF820
		bool				ReadSpellBook(TESObjectBOOK * book);	// F-30 0x004A3AD0
		void				OpenBook(TESObjectBOOK * book, BaseExtraList * extra);	// F-30 0x00845A80

		// ---------------------------------------------------------------- UI
		MenuManager *		Menus();
		UIManager *			UI();
		bool				IsMenuOpen(const char * name);
		IMenu *				GetMenu(const char * name);
		void				PostMenuMessage(const char * name, UInt32 message);	// 1 open, 3 close
		void				PlaySound(const char * editorId);		// F-22 0x00899620
		const char *		GameSetting(const char * name, const char * fallback);
		float				ZKeyDelay();
		bool				GamepadInUse();

		// Menu stack walk: calls f(IMenu*, flags) for each open menu.
		UInt32				OpenMenus(IMenu ** out, UInt32 max);

		// ---------------------------------------------------------------- control map (F-17..F-19)
		void				ToggleControls(UInt32 flags, bool enable);
		struct MappingView { const char * name; UInt32 button; UInt32 * flags; };
		UInt32				GameplayMappings(UInt32 device, MappingView * out, UInt32 max);
		UInt32				MappedKey(const char * control, UInt32 device);
	}
}
