// QuickLoot IE LE - engine calls. See Engine.h; evidence in evidence\FINDINGS.md.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Engine.h"
#include "Log.h"

#include "skse/GameAPI.h"
#include "skse/GameData.h"
#include "skse/GameMenus.h"
#include "skse/GameInput.h"
#include "skse/GameRTTI.h"
#include "skse/GameSettings.h"
#include "skse/GameFormComponents.h"

#include <windows.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

namespace
{
	// ------------------------------------------------------------ raw signatures (F8)
	typedef void *		(__thiscall * _GetLock)(TESObjectREFR *);											// F-03
	typedef bool		(__thiscall * _ExtraFlagTest)(BaseExtraList *, UInt32);							// F-04
	typedef TESForm *	(__fastcall * _GetOwner)(TESObjectREFR *);											// F-08
	typedef bool		(__thiscall * _IsAnOwner)(TESObjectREFR *, Actor *, bool, bool);					// F-09
	typedef bool		(__thiscall * _ActorBool)(Actor *);
	typedef bool		(__thiscall * _AllowedToTake)(PlayerCharacter *, TESForm *, SInt32);				// F-10
	typedef Actor *		(__thiscall * _ActorDoingCommand)(PlayerCharacter *);								// F-15
	typedef void		(__thiscall * _Encumbrance)(PlayerCharacter *, float *, float *);					// F-26
	typedef void		(__thiscall * _PlayerVoid)(PlayerCharacter *);
	typedef bool		(__thiscall * _ActivateRef)(TESObjectREFR *, TESObjectREFR *, UInt8, TESForm *, SInt32, bool);	// F-02
	typedef void		(__thiscall * _Dispel)(void *, bool);												// F-27
	typedef TESForm *	(__fastcall * _EntryOwner)(InventoryEntryData *);									// F-11
	typedef BaseExtraList * (__cdecl * _ExtraForRemoval)(InventoryEntryData *, SInt32, bool);				// F-06
	typedef void		(__thiscall * _RemoveItem)(TESObjectREFR *, UInt32 *, TESForm *, SInt32, UInt32, BaseExtraList *, TESObjectREFR *, void *, void *);
	typedef void		(__thiscall * _HandleRemove)(TESForm *, TESObjectREFR *);
	typedef void		(__thiscall * _AddItemEvent)(PlayerCharacter *, TESForm *, TESForm *, TESObjectREFR *, UInt32);
	typedef void		(__thiscall * _StealAlarm)(Actor *, TESObjectREFR *, TESForm *, SInt32, SInt32, TESForm *, bool);	// F-07
	typedef void		(__thiscall * _PickUpObject)(PlayerCharacter *, TESObjectREFR *, SInt32, bool, bool);
	typedef void		(__thiscall * _PlayPickUpSound)(PlayerCharacter *, TESForm *, bool, bool);
	typedef void		(__cdecl * _CdeclActor)(Actor *);
	typedef void		(__thiscall * _ProcessActor)(void *, Actor *);
	typedef void		(__cdecl * _CdeclVoid)(void);
	typedef void		(__thiscall * _EquipEntry)(void *, Actor *, InventoryEntryData *, void *, bool);	// F-30
	typedef bool		(__thiscall * _ReadBook)(TESObjectBOOK *, Actor *);
	typedef void		(__thiscall * _GetDescription)(void *, BSString *, TESForm *, UInt32);
	struct Pt3 { float x, y, z; };
	struct Mat3 { float m[3][3]; };
	typedef void		(__cdecl * _OpenBook)(BSString *, BaseExtraList *, TESObjectREFR *, TESObjectBOOK *, Pt3, Mat3, float, bool);
	typedef void		(__cdecl * _PlaySound)(const char *);												// F-22
	typedef void		(__thiscall * _ToggleControls)(void *, UInt32, bool, bool);						// F-18
	typedef bool		(__thiscall * _IsSneaking)(Actor *);

	const _GetLock			GetLockFn			= (_GetLock)0x004EB5B0;
	const _ExtraFlagTest	ActivationBlockedFn	= (_ExtraFlagTest)0x0040E850;
	const _GetOwner			GetOwnerFn			= (_GetOwner)0x004EC0C0;
	const _IsAnOwner		IsAnOwnerFn			= (_IsAnOwner)0x004EC3B0;
	const _IsSneaking		IsSneakingFn		= (_IsSneaking)0x004D9290;
	const _AllowedToTake	AllowedToTakeFn		= (_AllowedToTake)0x00745060;
	const _ActorDoingCommand ActorDoingCommandFn = (_ActorDoingCommand)0x0073D4B0;
	const _Encumbrance		EncumbranceFn		= (_Encumbrance)0x00733110;
	const _PlayerVoid		StartGrabFn			= (_PlayerVoid)0x0074FE50;
	const _ActivateRef		ActivateRefFn		= (_ActivateRef)0x004E4230;
	const _Dispel			DispelFn			= (_Dispel)0x00657160;
	const _EntryOwner		EntryOwnerFn		= (_EntryOwner)0x004755A0;
	const _ExtraForRemoval	ExtraForRemovalFn	= (_ExtraForRemoval)0x00868950;
	const _AddItemEvent		AddItemEventFn		= (_AddItemEvent)0x0073B020;
	const _StealAlarm		StealAlarmFn		= (_StealAlarm)0x006CC0D0;
	const _CdeclActor		UpdateActorEquipFn	= (_CdeclActor)0x00664B90;
	const _ProcessActor		ProcessEquipFn		= (_ProcessActor)0x007031A0;
	const _CdeclVoid		ChestsLootedFn		= (_CdeclVoid)0x0084A580;
	const _EquipEntry		EquipEntryFn		= (_EquipEntry)0x006EF820;
	const _ReadBook			ReadBookFn			= (_ReadBook)0x004A3AD0;
	const _GetDescription	GetDescriptionFn	= (_GetDescription)0x0044FB70;
	const _OpenBook			OpenBookFn			= (_OpenBook)0x00845A80;
	const _PlaySound		PlaySoundFn			= (_PlaySound)0x00899620;
	const _ToggleControls	ToggleControlsFn	= (_ToggleControls)0x00A67A30;

	// ------------------------------------------------------------ globals
	PlayerCharacter **	const kPlayer			= (PlayerCharacter **)0x01310588;	// SDK g_thePlayer
	void **				const kMenuControls		= (void **)0x01B10D5C;				// F-16
	void **				const kPlayerControls	= (void **)0x012E7454;				// F-23
	void **				const kPlayerCamera		= (void **)0x012E7288;
	void **				const kControlMap		= (void **)0x012E7458;				// F-17
	void **				const kEquipManager		= (void **)0x012E5FAC;				// F-30

	// vtable byte offsets (slot * 4)
	enum
	{
		kSlot_IsInCombat		= 0x388,	// F-13
		kSlot_RemoveItem		= 0x158,	// F-06
		kSlot_HandleRemove		= 0x13C,	// F-06
		kSlot_PickUpObject		= 0x32C,	// PlayerCharacter 0xCB (CommonLibLE row PlayerCharacter_PickUpObject)
		kSlot_PlayPickUpSound	= 0x288,	// 0xA2 (called by PickUpObject and TransferItem)
	};

	template<class T> T VCall(void * object, UInt32 offset)
	{
		void ** vtbl = *(void ***)object;
		return (T)vtbl[offset / 4];
	}

	bool KeywordMatches(BGSKeyword * kw, const char * editorId)
	{
		if(!QL::Engine::Ptr(kw)) return false;
		const char * name = kw->keyword.Get();
		return name && !_stricmp(name, editorId);
	}

	bool FormKeyword(BGSKeywordForm * form, const char * editorId)
	{
		if(!QL::Engine::Ptr(form) || !QL::Engine::Ptr(form->keywords)) return false;
		const UInt32 n = form->numKeywords < 4096 ? form->numKeywords : 4096;
		for(UInt32 i = 0; i < n; ++i)
			if(KeywordMatches(form->keywords[i], editorId)) return true;
		return false;
	}

	// tList<T> is { T* item; node* next } at offset 0; the SDK keeps Head() private.
	struct ListNode { void * item; ListNode * next; };
	inline ListNode * ListHead(const void * list) { return (ListNode *)list; }
}

namespace QL
{
	namespace Engine
	{
		bool Readable(const void * p, UInt32 n)
		{
			if(!Ptr(p)) return false;
			MEMORY_BASIC_INFORMATION mbi;
			if(!VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;
			if(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
			return (const UInt8 *)p + n <= (const UInt8 *)mbi.BaseAddress + mbi.RegionSize;
		}

		PlayerCharacter * Player()
		{
			PlayerCharacter * p = *kPlayer;
			return Ptr(p) ? p : 0;
		}

		UInt32 InvalidHandle() { return *g_invalidRefHandle; }

		UInt32 HandleOf(TESObjectREFR * ref)
		{
			UInt32 h = InvalidHandle();
			if(Ptr(ref)) CreateRefHandleByREFR(&h, ref);
			return h;
		}

		TESObjectREFR * Lookup(UInt32 handle)
		{
			if(!handle || handle == InvalidHandle()) return 0;
			TESObjectREFR * ref = 0;
			UInt32 h = handle;
			LookupREFRByHandle(&h, &ref);
			if(ref && !Ptr(ref)) return 0;
			return ref;
		}

		void Release(TESObjectREFR * ref)
		{
			if(!Ptr(ref)) return;
			volatile LONG * count = (volatile LONG *)((UInt8 *)ref + 0x18);
			const LONG left = InterlockedDecrement(count);
			if((left & 0x3FF) == 0)
			{
				void * handleObject = (UInt8 *)ref + 0x14;
				typedef void (__thiscall * _Delete)(void *);
				VCall<_Delete>(handleObject, 4)(handleObject);
			}
		}

		bool IsActor(TESObjectREFR * r)
		{
			return Ptr(r) && r->formType == kFormType_Character;
		}

		bool IsDeletedOrDisabled(TESObjectREFR * r)
		{
			if(!Ptr(r)) return true;
			return (r->flags & (TESForm::kFlagIsDeleted | 0x800)) != 0;	// 0x800 = initially disabled / disabled
		}

		bool IsLocked(TESObjectREFR * r)
		{
			if(!Ptr(r)) return false;
			UInt8 * lock = (UInt8 *)GetLockFn(r);
			return Ptr(lock) && (lock[8] & 1) != 0;
		}

		bool IsActivationBlocked(TESObjectREFR * r)
		{
			return Ptr(r) && ActivationBlockedFn(&r->extraData, 1);
		}

		TESForm * GetOwner(TESObjectREFR * r)
		{
			if(!Ptr(r)) return 0;
			TESForm * f = GetOwnerFn(r);
			return Ptr(f) ? f : 0;
		}

		bool HasContainer(TESObjectREFR * r)
		{
			return BaseContainer(r) != 0;
		}

		TESContainer * BaseContainer(TESObjectREFR * r)
		{
			TESForm * base = Base(r);
			if(!base) return 0;
			TESContainer * c = DYNAMIC_CAST(base, TESForm, TESContainer);
			return Ptr(c) ? c : 0;
		}

		UInt32 AshPileHandle(TESObjectREFR * r)
		{
			if(!Ptr(r)) return 0;
			BSExtraData * x = r->extraData.GetByType(kExtraData_AshPileRef);
			if(!Ptr(x)) return 0;
			return *(UInt32 *)((UInt8 *)x + 0x08);
		}

		ExtraContainerChanges::Data * Changes(TESObjectREFR * r)
		{
			if(!Ptr(r)) return 0;
			ExtraContainerChanges * x = static_cast<ExtraContainerChanges *>(r->extraData.GetByType(kExtraData_ContainerChanges));
			if(!Ptr(x) || !Ptr(x->data)) return 0;
			return x->data;
		}

		void DroppedHandles(TESObjectREFR * r, UInt32 * out, UInt32 & count, UInt32 max)
		{
			count = 0;
			if(!Ptr(r)) return;
			BSExtraData * x = r->extraData.GetByType(kExtraData_DroppedItemList);
			if(!Ptr(x)) return;
			// BSSimpleList<UInt32 handle> at +0x08: { item, next }
			struct Node { UInt32 item; Node * next; };
			Node * n = (Node *)((UInt8 *)x + 0x08);
			UInt32 guard = 0;
			while(n && count < max && guard++ < 4096)
			{
				if(n->item && n->item != InvalidHandle()) out[count++] = n->item;
				n = n->next;
				if(n && !Ptr(n)) break;
			}
		}

		SInt32 RefCount(TESObjectREFR * r)
		{
			if(!Ptr(r)) return 1;
			ExtraCount * x = static_cast<ExtraCount *>(r->extraData.GetByType(kExtraData_Count));
			if(!Ptr(x)) return 1;
			const SInt32 c = (SInt32)x->count;
			return c > 0 ? c : 1;
		}

		bool IsDead(Actor * a)
		{
			return Ptr(a) && a->IsDead(1);
		}

		bool IsInCombat(Actor * a)
		{
			typedef bool (__thiscall * _Fn)(Actor *);
			return Ptr(a) && VCall<_Fn>(a, kSlot_IsInCombat)(a);
		}

		bool IsSneaking(Actor * a)
		{
			return Ptr(a) && IsSneakingFn(a);
		}

		bool IsSummoned(Actor * a)
		{
			TESForm * base = Base(a);
			if(!base) return false;
			TESActorBaseData * data = DYNAMIC_CAST(base, TESForm, TESActorBaseData);
			return Ptr(data) && (data->flags & 0x4000) != 0;
		}

		bool HasKeyword(Actor * a, const char * editorId)
		{
			TESForm * base = Base(a);
			if(!base) return false;
			if(FormKeyword(DYNAMIC_CAST(base, TESForm, BGSKeywordForm), editorId)) return true;
			TESRaceForm * raceForm = DYNAMIC_CAST(base, TESForm, TESRaceForm);
			if(Ptr(raceForm) && Ptr(raceForm->race))
				return FormKeyword(DYNAMIC_CAST(raceForm->race, TESRace, BGSKeywordForm), editorId);
			return false;
		}

		bool IsGrabbing()
		{
			PlayerCharacter * p = Player();
			if(!p) return false;
			const UInt32 h = *(UInt32 *)((UInt8 *)p + 0x568);
			TESObjectREFR * r = Lookup(h);
			if(!r) return false;
			Release(r);
			return true;
		}

		bool HasActorDoingCommand()
		{
			PlayerCharacter * p = Player();
			return p && ActorDoingCommandFn(p) != 0;
		}

		bool InBeastForm()
		{
			UInt8 * mc = (UInt8 *)*kMenuControls;
			return Ptr(mc) && mc[0x41] != 0;
		}

		UInt32 CameraStateId()
		{
			UInt8 * cam = (UInt8 *)*kPlayerCamera;
			if(!Readable(cam, 0x24)) return 0xFFFFFFFF;
			UInt8 * state = *(UInt8 **)(cam + 0x20);
			if(!Readable(state, 0x10)) return 0xFFFFFFFF;
			return *(UInt32 *)(state + 0x0C);
		}

		void Encumbrance(float & weight, float & max)
		{
			weight = 0.0f; max = 0.0f;
			PlayerCharacter * p = Player();
			if(p) EncumbranceFn(p, &weight, &max);
		}

		bool AllowedToTake(TESForm * owner, SInt32 value)
		{
			PlayerCharacter * p = Player();
			return p && AllowedToTakeFn(p, owner, value);
		}

		bool IsAnOwner(TESObjectREFR * r, Actor * who, bool useFaction, bool requireOwner)
		{
			return Ptr(r) && IsAnOwnerFn(r, who, useFaction, requireOwner);
		}

		void StartGrab()
		{
			PlayerCharacter * p = Player();
			if(!p) return;
			if(*(UInt32 *)((UInt8 *)p + 0x6D4) != 0) return;		// F-24: only from "not grabbing"
			StartGrabFn(p);
		}

		void MarkActivateHeld(bool grabbed)
		{
			UInt8 * pc = (UInt8 *)*kPlayerControls;
			if(!Readable(pc, 0x144)) return;
			UInt8 * handler = *(UInt8 **)(pc + 0x140);
			if(!Readable(handler, 0x10)) return;
			handler[0x0D] = 1;
			handler[0x0E] = grabbed ? 1 : 0;
		}

		void ActivateRef(TESObjectREFR * target, TESObjectREFR * activator)
		{
			if(Ptr(target) && Ptr(activator))
				ActivateRefFn(target, activator, 0, 0, 1, false);
		}

		void DispelInvisibility(Actor * a)
		{
			if(!Ptr(a)) return;
			tList<ActiveEffect> * list = a->magicTarget.GetActiveEffects();
			if(!Ptr(list)) return;
			ActiveEffect * found[32];
			UInt32 n = 0;
			UInt32 guard = 0;
			for(ListNode * node = ListHead(list); node && guard++ < 1024; node = node->next)
			{
				if(!Ptr(node)) break;
				ActiveEffect * e = (ActiveEffect *)node->item;
				if(!Ptr(e) || !Ptr(e->effect)) continue;
				EffectSetting * mgef = e->effect->mgef;
				if(!Ptr(mgef)) continue;
				// Archetype Invisibility (11), and only effects flagged Recover - vanilla
				// interactions do not dispel the others either (QuickLoot SE comment).
				if(mgef->properties.archetype == EffectSetting::Properties::kArchetype_Invisibility &&
				   (mgef->properties.flags & EffectSetting::Properties::kEffectType_Recover) &&
				   n < 32)
					found[n++] = e;
			}
			for(UInt32 i = 0; i < n; ++i) DispelFn(found[i], false);
		}

		// -------------------------------------------------------------- items
		const char * EntryName(InventoryEntryData * e)
		{
			if(!Ptr(e) || !Ptr(e->type)) return "";
			const char * name = CALL_MEMBER_FN(e, GenerateName)();
			return name ? name : "";
		}

		SInt32 EntryValue(InventoryEntryData * e)
		{
			return Ptr(e) && Ptr(e->type) ? CALL_MEMBER_FN(e, GetValue)() : 0;
		}

		TESForm * EntryOwner(InventoryEntryData * e)
		{
			if(!Ptr(e)) return 0;
			TESForm * f = EntryOwnerFn(e);
			return Ptr(f) ? f : 0;
		}

		bool EntryHasExtra(InventoryEntryData * e, UInt32 type)
		{
			if(!Ptr(e) || !Ptr(e->extendDataList)) return false;
			UInt32 guard = 0;
			for(ListNode * n = ListHead(e->extendDataList); n && guard++ < 4096; n = n->next)
			{
				if(!Ptr(n)) break;
				BaseExtraList * list = (BaseExtraList *)n->item;
				if(Ptr(list) && list->HasType(type)) return true;
			}
			return false;
		}

		UInt32 EntrySoulLevel(InventoryEntryData * e)
		{
			if(!Ptr(e)) return 0;
			if(Ptr(e->extendDataList))
			{
				UInt32 guard = 0;
				for(ListNode * n = ListHead(e->extendDataList); n && guard++ < 4096; n = n->next)
				{
					if(!Ptr(n)) break;
					BaseExtraList * list = (BaseExtraList *)n->item;
					if(!Ptr(list)) continue;
					ExtraSoul * soul = static_cast<ExtraSoul *>(list->GetByType(kExtraData_Soul));
					if(Ptr(soul)) return soul->count & 0xFF;
				}
			}
			if(TypeOf(e->type) == kFormType_SoulGem)
				return static_cast<TESSoulGem *>(e->type)->soulSize;
			return 0;
		}

		EnchantmentItem * EntryEnchantment(InventoryEntryData * e)
		{
			if(!Ptr(e) || !Ptr(e->type)) return 0;
			if(Ptr(e->extendDataList))
			{
				UInt32 guard = 0;
				for(ListNode * n = ListHead(e->extendDataList); n && guard++ < 4096; n = n->next)
				{
					if(!Ptr(n)) break;
					BaseExtraList * list = (BaseExtraList *)n->item;
					if(!Ptr(list)) continue;
					ExtraEnchantment * x = static_cast<ExtraEnchantment *>(list->GetByType(kExtraData_Enchantment));
					if(Ptr(x) && Ptr(x->enchant)) return x->enchant;
				}
			}
			TESEnchantableForm * en = DYNAMIC_CAST(e->type, TESForm, TESEnchantableForm);
			return Ptr(en) && Ptr(en->enchantment) ? en->enchantment : 0;
		}

		float FormWeight(TESForm * f)
		{
			if(!Ptr(f)) return 0.0f;
			return (float)GetFormWeight(f);
		}

		float ArmorValue(InventoryEntryData * e)
		{
			PlayerCharacter * p = Player();
			if(!p || !Ptr(e)) return 0.0f;
			return (float)CALL_MEMBER_FN(p, GetArmorValue)(e);
		}

		float Damage(InventoryEntryData * e)
		{
			PlayerCharacter * p = Player();
			if(!p || !Ptr(e)) return 0.0f;
			return (float)CALL_MEMBER_FN(p, GetDamage)(e);
		}

		bool HasKeywordForm(TESForm * f, const char * editorId)
		{
			if(!Ptr(f)) return false;
			return FormKeyword(DYNAMIC_CAST(f, TESForm, BGSKeywordForm), editorId);
		}

		bool HasKeyword(TESForm * f, BGSKeyword * kw)
		{
			if(!Ptr(f) || !Ptr(kw)) return false;
			BGSKeywordForm * k = DYNAMIC_CAST(f, TESForm, BGSKeywordForm);
			if(!Ptr(k) || !Ptr(k->keywords)) return false;
			const UInt32 n = k->numKeywords < 4096 ? k->numKeywords : 4096;
			for(UInt32 i = 0; i < n; ++i) if(k->keywords[i] == kw) return true;
			return false;
		}

		const char * FullName(TESForm * f)
		{
			if(!Ptr(f)) return "";
			TESFullName * n = DYNAMIC_CAST(f, TESForm, TESFullName);
			if(!Ptr(n)) return "";
			const char * s = n->name.data;
			return s ? s : "";
		}

		bool IsPlayable(TESForm * f)
		{
			return Ptr(f) && f->IsPlayable();
		}

		const char * ModelPath(TESForm * f)
		{
			if(!Ptr(f)) return "";
			TESModel * m = DYNAMIC_CAST(f, TESForm, TESModel);
			if(!Ptr(m)) return "";
			const char * s = m->GetModelName();
			return s ? s : "";
		}

		TESForm * LookupForm(const char * plugin, UInt32 localId)
		{
			DataHandler * dh = DataHandler::GetSingleton();
			if(!Ptr(dh)) return 0;
			const UInt8 index = dh->GetModIndex(plugin);
			if(index == 0xFF) return 0;
			TESForm * f = LookupFormByID(((UInt32)index << 24) | (localId & 0x00FFFFFF));
			return Ptr(f) ? f : 0;
		}

		UInt32 ResolveFormID(const char * text)
		{
			if(!text || !*text) return 0;
			const char * bar = strchr(text, '|');
			if(bar)
			{
				char plugin[260];
				const size_t len = (size_t)(bar - text);
				if(len == 0 || len >= sizeof(plugin)) return 0;
				memcpy(plugin, text, len);
				plugin[len] = 0;
				while(len && plugin[0] == ' ') memmove(plugin, plugin + 1, strlen(plugin));
				char * end = 0;
				const unsigned long local = strtoul(bar + 1, &end, 16);
				if(end == bar + 1) return 0;
				TESForm * f = LookupForm(plugin, (UInt32)local);
				return f ? f->formID : 0;
			}
			char * end = 0;
			const unsigned long id = strtoul(text, &end, 16);
			return end == text ? 0 : (UInt32)id;
		}

		bool FormKey(UInt32 formID, char * out, size_t size)
		{
			const UInt32 index = formID >> 24;
			if(index >= 0xFF) return false;
			DataHandler * dh = DataHandler::GetSingleton();
			if(!Ptr(dh) || index >= dh->modList.loadedModCount) return false;
			ModInfo * mod = dh->modList.loadedMods[index];
			if(!Ptr(mod) || !mod->name[0]) return false;
			sprintf_s(out, size, "%s|0x%06X", mod->name, formID & 0x00FFFFFF);
			return true;
		}

		// -------------------------------------------------------------- transfers
		BaseExtraList * ExtraForRemoval(InventoryEntryData * e, SInt32 count)
		{
			if(!Ptr(e)) return 0;
			BaseExtraList * x = ExtraForRemovalFn(e, count, true);
			return Ptr(x) ? x : 0;
		}

		void RemoveItem(TESObjectREFR * from, TESForm * item, SInt32 count, UInt32 reason, BaseExtraList * extra, TESObjectREFR * moveTo)
		{
			if(!Ptr(from) || !Ptr(item) || count <= 0) return;
			UInt32 handle = 0;
			VCall<_RemoveItem>(from, kSlot_RemoveItem)(from, &handle, item, count, reason, extra, moveTo, 0, 0);
		}

		void HandleRemoveFromContainer(TESForm * item, TESObjectREFR * container)
		{
			if(!Ptr(item) || !Ptr(container)) return;
			VCall<_HandleRemove>(item, kSlot_HandleRemove)(item, container);
		}

		void AddPlayerAddItemEvent(TESForm * item, TESForm * owner, TESObjectREFR * container, UInt32 type)
		{
			PlayerCharacter * p = Player();
			if(p && Ptr(item)) AddItemEventFn(p, item, owner, container, type);
		}

		void StealAlarm(Actor * thief, TESObjectREFR * ref, TESForm * item, SInt32 count, SInt32 value, TESForm * owner)
		{
			if(Ptr(thief) && Ptr(ref)) StealAlarmFn(thief, ref, item, count, value, owner, true);
		}

		void PickUpObject(TESObjectREFR * object, SInt32 count)
		{
			PlayerCharacter * p = Player();
			if(!p || !Ptr(object)) return;
			VCall<_PickUpObject>(p, kSlot_PickUpObject)(p, object, count, false, true);
		}

		void ClearActorEquipment(Actor * a)
		{
			if(!Readable(a, 0x8C)) return;
			void * process = *(void **)((UInt8 *)a + 0x88);
			if(!Readable(process, 0x10)) return;
			UpdateActorEquipFn(a);
			process = *(void **)((UInt8 *)a + 0x88);		// re-read: the call may have rebuilt it
			if(Readable(process, 0x10)) ProcessEquipFn(process, a);
		}

		void SendChestsLooted() { ChestsLootedFn(); }

		InventoryEntryData * PlayerEntry(TESForm * item)
		{
			ExtraContainerChanges::Data * c = Changes(Player());
			if(!c || !Ptr(item)) return 0;
			InventoryEntryData * e = c->FindItemEntry(item);
			return Ptr(e) ? e : 0;
		}

		void EquipEntry(InventoryEntryData * playerEntry)
		{
			PlayerCharacter * p = Player();
			void * mgr = *kEquipManager;
			if(p && Ptr(mgr) && Ptr(playerEntry)) EquipEntryFn(mgr, p, playerEntry, 0, true);
		}

		bool ReadSpellBook(TESObjectBOOK * book)
		{
			PlayerCharacter * p = Player();
			return p && Ptr(book) && ReadBookFn(book, p);
		}

		void OpenBook(TESObjectBOOK * book, BaseExtraList * extra)
		{
			if(!Ptr(book)) return;
			BSString text;
			GetDescriptionFn(&book->description, &text, 0, 0x43534544);	// 'DESC' as vanilla pushes it (0x00849F45)
			// QuickLoot SE: rotation (-0.05, -0.05, 1.50), scale 1, default position.
			Mat3 rot;
			const float x = -0.05f, y = -0.05f, z = 1.50f;
			const float sx = sinf(x), sy = sinf(y), sz = sinf(z), cx = cosf(x), cy = cosf(y), cz = cosf(z);
			rot.m[0][0] = cy * cz;				rot.m[0][1] = cy * sz;				rot.m[0][2] = -sy;
			rot.m[1][0] = sx * sy * cz - cx * sz;	rot.m[1][1] = sx * sy * sz + cx * cz;	rot.m[1][2] = sx * cy;
			rot.m[2][0] = cx * sy * cz + sx * sz;	rot.m[2][1] = cx * sy * sz - sx * cz;	rot.m[2][2] = cx * cy;
			Pt3 pos = { 0.0f, 0.0f, 0.0f };
			OpenBookFn(&text, extra, 0, book, pos, rot, 1.0f, true);
		}

		// -------------------------------------------------------------- UI
		MenuManager * Menus()
		{
			MenuManager * m = MenuManager::GetSingleton();
			return Ptr(m) ? m : 0;
		}

		UIManager * UI()
		{
			UIManager * u = UIManager::GetSingleton();
			return Ptr(u) ? u : 0;
		}

		bool IsMenuOpen(const char * name)
		{
			MenuManager * m = Menus();
			if(!m) return false;
			BSFixedString n(name);
			const bool open = m->IsMenuOpen(&n);
			CALL_MEMBER_FN(&n, Release)();
			return open;
		}

		IMenu * GetMenu(const char * name)
		{
			MenuManager * m = Menus();
			if(!m) return 0;
			BSFixedString n(name);
			IMenu * menu = m->GetMenu(&n);
			CALL_MEMBER_FN(&n, Release)();
			return Ptr(menu) ? menu : 0;
		}

		void PostMenuMessage(const char * name, UInt32 message)
		{
			UIManager * u = UI();
			if(!u) return;
			BSFixedString n(name);
			CALL_MEMBER_FN(u, AddMessage)(&n, message, 0);
			CALL_MEMBER_FN(&n, Release)();
		}

		void PlaySound(const char * editorId) { PlaySoundFn(editorId); }

		const char * GameSetting(const char * name, const char * fallback)
		{
			SettingCollectionMap * map = *g_gameSettingCollection;
			if(!Ptr(map)) return fallback;
			Setting * s = map->Get(name);
			if(!Ptr(s) || !s->data.s || !Ptr(s->data.s) || !s->data.s[0]) return fallback;
			return s->data.s;
		}

		float ZKeyDelay()
		{
			Setting * s = GetINISetting("fZKeyDelay:Controls");
			if(!Ptr(s)) return 0.4f;
			const float v = s->data.f32;
			return (v == v && v > 0.0f && v < 10.0f) ? v : 0.4f;
		}

		bool GamepadInUse()
		{
			InputEventDispatcher * d = *g_inputEventDispatcher;
			return Ptr(d) && d->IsGamepadEnabled();
		}

		UInt32 OpenMenus(IMenu ** out, UInt32 max)
		{
			MenuManager * m = Menus();
			if(!m) return 0;
			UInt8 * base = (UInt8 *)m + 0x94;				// menuStack: entries, capacity, count
			IMenu ** entries = *(IMenu ***)base;
			const UInt32 count = *(UInt32 *)(base + 8);
			if(!Ptr(entries) || count > 256) return 0;
			UInt32 n = 0;
			for(UInt32 i = 0; i < count && n < max; ++i)
				if(Ptr(entries[i])) out[n++] = entries[i];
			return n;
		}

		// -------------------------------------------------------------- control map
		void ToggleControls(UInt32 flags, bool enable)
		{
			void * map = *kControlMap;
			if(Ptr(map)) ToggleControlsFn(map, flags, enable, false);
		}

		UInt32 GameplayMappings(UInt32 device, MappingView * out, UInt32 max)
		{
			InputManager * im = InputManager::GetSingleton();
			if(!Ptr(im)) return 0;
			InputManager::InputContext * ctx = im->context[InputManager::kContext_Gameplay];
			if(!Ptr(ctx)) return 0;
			tArray<InputManager::InputContext::Mapping> * list =
				device == kDeviceType_Mouse ? &ctx->mouseMap : device == kDeviceType_Gamepad ? &ctx->gamepadMap : &ctx->keyboardMap;
			if(!Ptr(list->arr.entries) || list->count > 1024) return 0;
			UInt32 n = 0;
			for(UInt32 i = 0; i < list->count && n < max; ++i)
			{
				InputManager::InputContext::Mapping & m = list->arr.entries[i];
				out[n].name		= m.name.data ? m.name.data : "";
				out[n].button	= m.buttonID;
				out[n].flags	= &m.unk0C;
				++n;
			}
			return n;
		}

		UInt32 MappedKey(const char * control, UInt32 device)
		{
			InputManager * im = InputManager::GetSingleton();
			if(!Ptr(im)) return 0xFFFFFFFF;
			BSFixedString n(control);
			const UInt32 key = im->GetMappedKey(n, device, InputManager::kContext_Gameplay);
			CALL_MEMBER_FN(&n, Release)();
			return key;
		}
	}
}
