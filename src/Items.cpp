// QuickLoot IE LE - item rows. See Items.h.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE's Items/ItemStack*.cpp,
// Items/QuickLootItemStack.cpp, Items/Inventory.cpp, Items/ItemDefines.h and
// LootMenu.Sorting.cpp (Miss Corruption, AtomCrafty and contributors).
// GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Items.h"
#include "Engine.h"
#include "Log.h"

#include "skse/GameRTTI.h"
#include "skse/GameFormComponents.h"

#include <algorithm>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <intrin.h>

using namespace QL::Engine;

namespace
{
	// ---------------------------------------------------------------- SkyUI enums (ItemDefines.h)
	enum ItemType { kItemNone = 0, kItemArmor = 1, kItemWeapon = 2, kItemMisc = 3, kItemBook = 4, kItemFood = 5,
					kItemMagic = 6, kItemIngredient = 8, kItemKey = 9, kItemSoulGem = 12 };
	enum WeightClass { kLight = 0, kHeavy = 1, kNoneClass = 2, kClothing = 3, kJewelry = 4 };
	enum ArmorSub { kArmNone = -1, kHead = 0, kHair, kLongHair, kBody, kForearms, kHands, kShield, kCalves, kFeet,
					kCirclet, kAmulet, kEars, kRing, kTail };
	enum WeaponSub { kWNone = -1, kMelee = 0, kSword, kDagger, kWarAxe, kMace, kGreatsword, kBattleaxe, kWarhammer,
					 kBow, kCrossbow, kStaff, kPickaxe, kWoodAxe };
	enum BookSub { kBNone = -1, kSpellTome = 0, kNote = 1, kRecipe = 2 };
	enum PotionSub { kPNone = -1, kHealth = 0, kHealRate, kHealRateMult, kMagicka, kMagickaRate, kMagickaRateMult,
					 kStamina, kStaminaRate, kStaminaRateMult, kFireResist, kElectricResist, kFrostResist, kPotion,
					 kDrink, kFood, kPoison };
	enum MiscSub { kMNone = -1, kGem = 0, kDragonClaw, kArtifact, kLeather, kLeatherStrips, kHide, kRemains, kIngot,
				   kTool, kChildrensClothes, kToy, kFirewood, kFastener, kWeaponRack, kShelf, kFurniture, kExterior,
				   kContainerPart, kHousePart, kClutter, kLockpick, kGold };
	enum SoulLevel { kSoulNone = 0, kPetty, kLesser, kCommon, kGreater, kGrand, kAzura };

	// Biped slots (LE BGSBipedObjectForm parts, same bit order as SE)
	enum Slot
	{
		kSlotHead = 1 << 0, kSlotHair = 1 << 1, kSlotBody = 1 << 2, kSlotHands = 1 << 3, kSlotForearms = 1 << 4,
		kSlotAmulet = 1 << 5, kSlotRing = 1 << 6, kSlotFeet = 1 << 7, kSlotCalves = 1 << 8, kSlotShield = 1 << 9,
		kSlotTail = 1 << 10, kSlotLongHair = 1 << 11, kSlotCirclet = 1 << 12, kSlotEars = 1 << 13,
	};

	// Actor values (LE numbering, same as SE for these)
	enum AV { kAV_Health = 24, kAV_Magicka = 25, kAV_Stamina = 26, kAV_HealRate = 27, kAV_MagickaRate = 28,
			  kAV_StaminaRate = 29, kAV_ResistFire = 41, kAV_ResistFrost = 42, kAV_ResistShock = 43,
			  kAV_HealRateMult = 0x95, kAV_MagickaRateMult = 0x96, kAV_StaminaRateMult = 0x97 };

	// ---------------------------------------------------------------- known forms (ItemDefines.h KnownForms)
	struct Known { const char * plugin; UInt32 id; UInt32 formID; };
	enum
	{
		kfWeddingWreath, kfVampireLordArmor, kfPickaxe, kfRocksplinter, kfVolunruudPickaxe, kfAxe01, kfPoachersAxe,
		kfBoundArrow, kfCWArrow, kfCWArrowShort, kfDaedricArrow, kfDraugrArrow, kfPracticeArrow, kfSigdisArrows,
		kfDwarvenArrow, kfSphereArrow, kfSphereBolt01, kfSphereBolt02, kfEbonyArrow, kfElvenArrow, kfFalmerArrow,
		kfFollowerIronArrow, kfForswornArrow, kfGlassArrow, kfIronArrow, kfMQ101SteelArrow, kfNordHeroArrow,
		kfOrcishArrow, kfSteelArrow, kfTrapDart, kfElvenArrowBlessed, kfElvenArrowBlood, kfTestBolt, kfBallistaBolt,
		kfRieklingSpear, kfPotionUse, kfAzuraBroken, kfAzurasStar, kfBlackStar, kfLockpick, kfGold, kfLeather,
		kfLeatherStrips, kfAmethystFlawless, kfClaw1, kfClaw2, kfClaw3, kfClaw4, kfClaw5, kfClaw6, kfClaw7, kfClaw8,
		kfClaw9, kfClaw10, kfClaw11, kfDisallowEnchanting, kfCount
	};
	Known s_known[kfCount] =
	{
		{ "Skyrim.esm", 0x08895A }, { "Dawnguard.esm", 0x011A84 }, { "Skyrim.esm", 0x0E3C16 }, { "Skyrim.esm", 0x06A707 },
		{ "Skyrim.esm", 0x1019D4 }, { "Skyrim.esm", 0x02F2F4 }, { "Skyrim.esm", 0x0AE086 },
		{ "Skyrim.esm", 0x10B0A7 }, { "Skyrim.esm", 0x020DDF }, { "Skyrim.esm", 0x020F02 }, { "Skyrim.esm", 0x0139C0 },
		{ "Skyrim.esm", 0x034182 }, { "Skyrim.esm", 0x0CAB52 }, { "Skyrim.esm", 0x0E738A }, { "Skyrim.esm", 0x0139BC },
		{ "Skyrim.esm", 0x07B932 }, { "Skyrim.esm", 0x07B935 }, { "Skyrim.esm", 0x10EC8C }, { "Skyrim.esm", 0x0139BF },
		{ "Skyrim.esm", 0x0139BD }, { "Skyrim.esm", 0x038341 }, { "Skyrim.esm", 0x10E2DE }, { "Skyrim.esm", 0x0CEE9E },
		{ "Skyrim.esm", 0x0139BE }, { "Skyrim.esm", 0x01397D }, { "Skyrim.esm", 0x105EE7 }, { "Skyrim.esm", 0x0EAFDF },
		{ "Skyrim.esm", 0x0139BB }, { "Skyrim.esm", 0x01397F }, { "Skyrim.esm", 0x0236DD }, { "Dawnguard.esm", 0x0098A1 },
		{ "Dawnguard.esm", 0x0098A0 }, { "Dawnguard.esm", 0x00590C }, { "Dragonborn.esm", 0x0339A1 },
		{ "Dragonborn.esm", 0x017720 }, { "Skyrim.esm", 0x0B6435 }, { "Skyrim.esm", 0x028AD7 }, { "Skyrim.esm", 0x063B27 },
		{ "Skyrim.esm", 0x063B29 }, { "Skyrim.esm", 0x00000A }, { "Skyrim.esm", 0x00000F }, { "Skyrim.esm", 0x0DB5D2 },
		{ "Skyrim.esm", 0x0800E4 }, { "Skyrim.esm", 0x06851E }, { "Skyrim.esm", 0x04B56C }, { "Skyrim.esm", 0x0AB7BB },
		{ "Skyrim.esm", 0x07C260 }, { "Skyrim.esm", 0x05AF48 }, { "Skyrim.esm", 0x0ED417 }, { "Skyrim.esm", 0x0AB375 },
		{ "Skyrim.esm", 0x08CDFA }, { "Skyrim.esm", 0x0B634C }, { "Skyrim.esm", 0x0999E7 }, { "Skyrim.esm", 0x0663D7 },
		{ "Skyrim.esm", 0x039647 }, { "Skyrim.esm", 0x0C27BD },
	};

	inline bool Is(TESForm * f, int k) { return f && s_known[k].formID && f->formID == s_known[k].formID; }

	// ---------------------------------------------------------------- SkyUI tables
	struct Material { const char * keywords[3]; };	// only "has a material keyword" matters here (no material column)

	struct MiscEntry { int type; const char * keywords[7]; };
	const MiscEntry kMiscTable[] =
	{
		{ kChildrensClothes, { "BYOHAdoptionClothesKeyword" } },
		{ kToy, { "BYOHAdoptionToyKeyword" } },
		{ kHousePart, { "BYOHHouseCraftingCategoryWeaponRacks", "BYOHHouseCraftingCategoryShelf", "BYOHHouseCraftingCategoryFurniture",
						"BYOHHouseCraftingCategoryExterior", "BYOHHouseCraftingCategoryContainers", "BYOHHouseCraftingCategoryBuilding",
						"BYOHHouseCraftingCategorySmithing" } },
		{ kArtifact, { "VendorItemDaedricArtifact" } },
		{ kGem, { "VendorItemGem" } },
		{ kHide, { "VendorItemAnimalHide" } },
		{ kTool, { "VendorItemTool" } },
		{ kRemains, { "VendorItemAnimalPart" } },
		{ kIngot, { "VendorItemOreIngot" } },
		{ kClutter, { "VendorItemClutter" } },
		{ kFirewood, { "VendorItemFirewood" } },
	};

	const UInt32 kSlotPrecedence[] =
	{
		kSlotBody, kSlotHair, kSlotHands, kSlotForearms, kSlotFeet, kSlotCalves, kSlotShield, kSlotAmulet, kSlotRing,
		kSlotLongHair, kSlotEars, kSlotHead, kSlotCirclet, kSlotTail,
		1u << 14, 1u << 15, 1u << 16, 1u << 17, 1u << 18, 1u << 19, 1u << 20, 1u << 21, 1u << 22, 1u << 23,
		1u << 24, 1u << 25, 1u << 26, 1u << 27, 1u << 28, 1u << 29, 1u << 30, 1u << 31,
	};

	struct SlotSub { UInt32 slot; int sub; };
	const SlotSub kSlotSubs[] =
	{
		{ kSlotHead, kHead }, { kSlotHair, kHair }, { kSlotLongHair, kLongHair }, { kSlotBody, kBody }, { kSlotHands, kHands },
		{ kSlotForearms, kForearms }, { kSlotAmulet, kAmulet }, { kSlotRing, kRing }, { kSlotFeet, kFeet },
		{ kSlotCalves, kCalves }, { kSlotShield, kShield }, { kSlotCirclet, kCirclet }, { kSlotEars, kEars }, { kSlotTail, kTail },
	};

	// ---------------------------------------------------------------- one inventory stack
	struct Stack
	{
		RawEntry			entry;		// a copy; its extra lists are the container's own, read on this thread only
		UInt32				dropHandle;
		TESObjectREFR *		dropRef;	// counted while the rows are built
	};

	// Everything the icon/sort logic needs, in SkyUI terms.
	struct Data
	{
		int itemType;
		int weightClass;	// -1 unset
		int armorSub;		// -1 unset
		UInt32 mainSlot;
		int weaponSub;
		int ammoBolt;		// -1 n/a, 0 arrow, 1 bolt
		int bookSub;
		bool bookRead;
		int miscSub;
		int potionSub;
		int soulSub;		// -1 unset
		int soulStatus;		// -1 none, 0 empty, 1 partial, 2 full
		UInt32 scrollResist;
		UInt32 potionAV;
	};

	bool AnyKeyword(TESForm * f, const char * const * kws, int n)
	{
		for(int i = 0; i < n && kws[i]; ++i)
			if(HasKeywordForm(f, kws[i])) return true;
		return false;
	}

	EffectSetting * CostliestEffect(TESForm * f, MagicItem::EffectItem ** itemOut)
	{
		MagicItem * m = DYNAMIC_CAST(f, TESForm, MagicItem);
		if(!Ptr(m)) return 0;
		MagicItem::EffectItem * e = CALL_MEMBER_FN(m, GetCostliestEffectItem)(5, false);	// SKSE: delivery 5 = any
		if(!Ptr(e) || !Ptr(e->mgef)) return 0;
		if(itemOut) *itemOut = e;
		return e->mgef;
	}

	int ItemTypeOf(TESForm * f)
	{
		switch(TypeOf(f))
		{
		case kFormType_Armor:		return kItemArmor;
		case kFormType_Weapon:
		case kFormType_Ammo:		return kItemWeapon;
		case kFormType_Book:		return (static_cast<TESObjectBOOK *>(f)->data.flags & TESObjectBOOK::Data::kType_Spell) ? kItemMagic : kItemBook;
		case kFormType_Ingredient:	return kItemIngredient;
		case kFormType_Misc:
		case kFormType_Light:		return kItemMisc;
		case kFormType_Key:			return kItemKey;
		case kFormType_ScrollItem:	return kItemMagic;
		case kFormType_Potion:		return static_cast<AlchemyItem *>(f)->IsFood() ? kItemFood : kItemMagic;
		case kFormType_SoulGem:		return kItemSoulGem;
		default:					return kItemNone;
		}
	}

	bool NoteModel(const char * path)
	{
		// QuickLoot SE: ^.*(?:Note|FishMap)[^\\/]+$ (case-insensitive): "note" or "fishmap" followed by
		// at least one character and no path separator up to the end.
		if(!path || !*path) return false;
		char lower[260];
		size_t n = 0;
		for(const char * p = path; *p && n < sizeof(lower) - 1; ++p) lower[n++] = (char)tolower((unsigned char)*p);
		lower[n] = 0;
		for(size_t i = 0; i < n; ++i)
		{
			const bool note = !strncmp(lower + i, "note", 4), fish = !strncmp(lower + i, "fishmap", 7);
			if(!note && !fish) continue;
			const size_t end = i + (note ? 4 : 7);
			if(end >= n) continue;
			bool sep = false;
			for(size_t j = end; j < n; ++j) if(lower[j] == '\\' || lower[j] == '/') sep = true;
			if(!sep) return true;
		}
		return false;
	}

	void Classify(TESForm * object, InventoryEntryData * entry, Data & d)
	{
		d.itemType = ItemTypeOf(object);
		d.weightClass = -1; d.armorSub = -1; d.mainSlot = 0; d.weaponSub = -1; d.ammoBolt = -1;
		d.bookSub = -1; d.bookRead = false; d.miscSub = -1; d.potionSub = -1; d.soulSub = -1; d.soulStatus = -1;
		d.scrollResist = 0xFFFFFFFF; d.potionAV = 0xFFFFFFFF;

		switch(TypeOf(object))
		{
		case kFormType_Armor:
		{
			TESObjectARMO * armor = static_cast<TESObjectARMO *>(object);
			const UInt32 parts = armor->bipedObject.data.parts;
			// SkyUiProcessArmorClass
			switch(armor->bipedObject.data.weightClass)
			{
			case kLight:	d.weightClass = kLight; break;
			case kHeavy:	d.weightClass = kHeavy; break;
			default:
				if(HasKeywordForm(object, "VendorItemClothing"))		d.weightClass = kClothing;
				else if(HasKeywordForm(object, "VendorItemJewelry"))	d.weightClass = kJewelry;
				else													d.weightClass = -1;
				break;
			}
			// SkyUiProcessArmorPartMask
			for(size_t i = 0; i < sizeof(kSlotPrecedence) / sizeof(kSlotPrecedence[0]); ++i)
				if(parts & kSlotPrecedence[i]) { d.mainSlot = kSlotPrecedence[i]; break; }
			for(size_t i = 0; i < sizeof(kSlotSubs) / sizeof(kSlotSubs[0]); ++i)
				if(kSlotSubs[i].slot == d.mainSlot) { d.armorSub = kSlotSubs[i].sub; break; }
			// SkyUiProcessArmorOther
			if(d.weightClass < 0)
			{
				switch(d.mainSlot)
				{
				case kSlotHead: case kSlotHair: case kSlotLongHair: case kSlotBody: case kSlotHands:
				case kSlotForearms: case kSlotFeet: case kSlotCalves: case kSlotShield: case kSlotTail:
					d.weightClass = kClothing; break;
				case kSlotAmulet: case kSlotRing: case kSlotCirclet: case kSlotEars:
					d.weightClass = kJewelry; break;
				default: break;
				}
			}
			// SkyUiProcessArmorKnownForms
			if(Is(object, kfWeddingWreath)) d.weightClass = kJewelry;
			if(Is(object, kfVampireLordArmor)) d.armorSub = kBody;
			break;
		}
		case kFormType_Book:
		{
			TESObjectBOOK * book = static_cast<TESObjectBOOK *>(object);
			d.bookRead = (book->data.flags & TESObjectBOOK::Data::kType_Read) != 0;
			UInt8 type = book->data.type;
			if(NoteModel(ModelPath(object))) type = 0xFF;	// kNoteScroll
			if(HasKeywordForm(object, "VendorItemRecipe"))			d.bookSub = kRecipe;
			else if(HasKeywordForm(object, "VendorItemSpellTome"))	d.bookSub = kSpellTome;
			else if(type == 0xFF)									d.bookSub = kNote;
			break;
		}
		case kFormType_Misc:
		{
			for(size_t i = 0; i < sizeof(kMiscTable) / sizeof(kMiscTable[0]); ++i)
				if(AnyKeyword(object, kMiscTable[i].keywords, 7)) { d.miscSub = kMiscTable[i].type; break; }
			if(Is(object, kfGold)) d.miscSub = kGold;
			else if(Is(object, kfLockpick)) d.miscSub = kLockpick;
			else if(Is(object, kfLeather)) d.miscSub = kLeather;
			else if(Is(object, kfLeatherStrips)) d.miscSub = kLeatherStrips;
			else if(Is(object, kfAmethystFlawless)) d.miscSub = kGem;
			else
				for(int k = kfClaw1; k <= kfClaw11; ++k)
					if(Is(object, k)) { d.miscSub = kDragonClaw; break; }
			break;
		}
		case kFormType_SoulGem:
		{
			TESSoulGem * gem = static_cast<TESSoulGem *>(object);
			// SkyUiProcessSoulGemType / Status: capacity is gemSize, the base form's own soul is soulSize.
			d.soulSub = gem->gemSize ? (int)gem->gemSize : -1;
			if(!gem->gemSize || !gem->soulSize)		d.soulStatus = 0;
			else if(gem->soulSize < gem->gemSize)	d.soulStatus = 1;
			else									d.soulStatus = 2;
			if(Is(object, kfAzurasStar) || Is(object, kfBlackStar)) d.soulSub = kAzura;
			break;
		}
		case kFormType_Weapon:
		{
			TESObjectWEAP * weapon = static_cast<TESObjectWEAP *>(object);
			UInt8 t = weapon->gameData.type;
			if(t >= 10) t -= 10;
			switch(t)
			{
			case TESObjectWEAP::GameData::kType_HandToHandMelee:	d.weaponSub = kMelee; break;
			case TESObjectWEAP::GameData::kType_OneHandSword:		d.weaponSub = kSword; break;
			case TESObjectWEAP::GameData::kType_OneHandDagger:		d.weaponSub = kDagger; break;
			case TESObjectWEAP::GameData::kType_OneHandAxe:			d.weaponSub = kWarAxe; break;
			case TESObjectWEAP::GameData::kType_OneHandMace:		d.weaponSub = kMace; break;
			case TESObjectWEAP::GameData::kType_TwoHandSword:		d.weaponSub = kGreatsword; break;
			case TESObjectWEAP::GameData::kType_TwoHandAxe:
				d.weaponSub = HasKeywordForm(object, "WeapTypeWarhammer") ? kWarhammer : kBattleaxe; break;
			case TESObjectWEAP::GameData::kType_Bow:				d.weaponSub = kBow; break;
			case TESObjectWEAP::GameData::kType_Staff:				d.weaponSub = kStaff; break;
			case TESObjectWEAP::GameData::kType_CrossBow:			d.weaponSub = kCrossbow; break;
			default:												d.weaponSub = -1; break;
			}
			if(Is(object, kfPickaxe) || Is(object, kfRocksplinter) || Is(object, kfVolunruudPickaxe)) d.weaponSub = kPickaxe;
			else if(Is(object, kfAxe01) || Is(object, kfPoachersAxe)) d.weaponSub = kWoodAxe;
			break;
		}
		case kFormType_Ammo:
			d.ammoBolt = static_cast<TESAmmo *>(object)->isBolt() ? 1 : 0;
			break;
		case kFormType_Potion:
		{
			AlchemyItem * a = static_cast<AlchemyItem *>(object);
			if(a->IsFood())
				d.potionSub = (s_known[kfPotionUse].formID && Ptr(a->itemData.useSound) &&
							   a->itemData.useSound->formID == s_known[kfPotionUse].formID) ? kDrink : kFood;
			else if(a->IsPoison())
				d.potionSub = kPoison;
			else
			{
				EffectSetting * e = CostliestEffect(object, 0);
				d.potionAV = e ? e->properties.primaryValue : 0xFFFFFFFF;
				switch(d.potionAV)
				{
				case kAV_Health:			d.potionSub = kHealth; break;
				case kAV_HealRate:			d.potionSub = kHealRate; break;
				case kAV_HealRateMult:		d.potionSub = kHealRateMult; break;
				case kAV_Magicka:			d.potionSub = kMagicka; break;
				case kAV_MagickaRate:		d.potionSub = kMagickaRate; break;
				case kAV_MagickaRateMult:	d.potionSub = kMagickaRateMult; break;
				case kAV_Stamina:			d.potionSub = kStamina; break;
				case kAV_StaminaRate:		d.potionSub = kStaminaRate; break;
				case kAV_StaminaRateMult:	d.potionSub = kStaminaRateMult; break;
				case kAV_ResistFire:		d.potionSub = kFireResist; break;
				case kAV_ResistShock:		d.potionSub = kElectricResist; break;
				case kAV_ResistFrost:		d.potionSub = kFrostResist; break;
				default:					d.potionSub = kPotion; break;
				}
			}
			break;
		}
		case kFormType_ScrollItem:
		{
			EffectSetting * e = CostliestEffect(object, 0);
			d.scrollResist = e ? e->properties.resistance : 0xFFFFFFFF;
			break;
		}
		default:
			break;
		}
		(void)entry;
	}

	void SelectIcon(TESForm * object, const Data & d, const char *& label, SInt32 & color)
	{
		label = 0;
		color = -1;
		switch(TypeOf(object))
		{
		case kFormType_ScrollItem:
			label = "default_scroll";
			if(d.scrollResist == kAV_ResistFire) color = 0xC73636;
			else if(d.scrollResist == kAV_ResistShock) color = 0xFFFF00;
			else if(d.scrollResist == kAV_ResistFrost) color = 0x1FFBFF;
			break;
		case kFormType_Armor:
		{
			label = "default_armor";
			color = 0xEDDA87;
			const char * jewelry = d.armorSub == kAmulet ? "armor_amulet" : d.armorSub == kRing ? "armor_ring" :
								   d.armorSub == kCirclet ? "armor_circlet" : 0;
			if(d.weightClass == kLight || d.weightClass == kHeavy)
			{
				const bool light = d.weightClass == kLight;
				color = light ? 0x756000 : 0x6B7585;
				switch(d.armorSub)
				{
				case kHead: case kHair: case kLongHair:	label = light ? "lightarmor_head" : "armor_head"; break;
				case kBody: case kTail:					label = light ? "lightarmor_body" : "armor_body"; break;
				case kHands:							label = light ? "lightarmor_hands" : "armor_hands"; break;
				case kForearms:							label = light ? "lightarmor_forearms" : "armor_forearms"; break;
				case kFeet:								label = light ? "lightarmor_feet" : "armor_feet"; break;
				case kCalves:							label = light ? "lightarmor_calves" : "armor_calves"; break;
				case kShield:							label = light ? "lightarmor_shield" : "armor_shield"; break;
				default:								if(jewelry) label = jewelry; break;
				}
			}
			else if(d.weightClass == kClothing)
			{
				switch(d.armorSub)
				{
				case kHead: case kHair: case kLongHair:	label = "clothing_head"; break;
				case kBody: case kTail:					label = "clothing_body"; break;
				case kHands:							label = "clothing_hands"; break;
				case kForearms:							label = "clothing_forearms"; break;
				case kFeet:								label = "clothing_feet"; break;
				case kCalves:							label = "clothing_calves"; break;
				case kShield:							label = "clothing_shield"; break;
				default: break;
				}
			}
			else if(d.weightClass == kJewelry)
			{
				if(jewelry) label = jewelry;
			}
			break;
		}
		case kFormType_Book:
			label = d.bookSub == kSpellTome ? "book_tome" : (d.bookSub == kNote || d.bookSub == kRecipe) ? "book_note" : "default_book";
			break;
		case kFormType_Ingredient:	label = "default_ingredient"; break;
		case kFormType_Light:		label = "misc_torch"; break;
		case kFormType_Misc:
			switch(d.miscSub)
			{
			case kGem:				label = "misc_gem"; color = 0xFFB0D1; break;
			case kDragonClaw:		label = "misc_dragonclaw"; break;
			case kArtifact:			label = "misc_artifact"; break;
			case kLeather:			label = "misc_leather"; color = 0xBA8D23; break;
			case kLeatherStrips:	label = "misc_strips"; color = 0xBA8D23; break;
			case kHide:				label = "misc_hide"; color = 0xDBB36E; break;
			case kRemains:			label = "misc_remains"; break;
			case kIngot:			label = "misc_ingot"; color = 0x828282; break;
			case kChildrensClothes:	label = "clothing_body"; color = 0xEDDA87; break;
			case kFirewood:			label = "misc_wood"; color = 0xA89E8C; break;
			case kClutter:			label = "misc_clutter"; break;
			case kLockpick:			label = "misc_lockpick"; break;
			case kGold:				label = "misc_gold"; color = 0xCCCC33; break;
			default:				label = "default_misc"; break;
			}
			break;
		case kFormType_Weapon:
			color = 0xA4A5BF;
			switch(d.weaponSub)
			{
			case kSword:		label = "weapon_sword"; break;
			case kDagger:		label = "weapon_dagger"; break;
			case kWarAxe:		label = "weapon_waraxe"; break;
			case kMace:			label = "weapon_mace"; break;
			case kGreatsword:	label = "weapon_greatsword"; break;
			case kBattleaxe:	label = "weapon_battleaxe"; break;
			case kWarhammer:	label = "weapon_hammer"; break;
			case kBow:			label = "weapon_bow"; break;
			case kCrossbow:		label = "weapon_crossbow"; break;
			case kStaff:		label = "weapon_staff"; break;
			case kPickaxe:		label = "weapon_pickaxe"; break;
			case kWoodAxe:		label = "weapon_woodaxe"; break;
			default:			label = "default_weapon"; break;
			}
			break;
		case kFormType_Ammo:
			color = 0xA89E8C;
			label = d.ammoBolt == 1 ? "weapon_bolt" : "weapon_arrow";
			break;
		case kFormType_Key:
			label = "default_key";
			break;
		case kFormType_Potion:
			switch(d.potionSub)
			{
			case kHealth: case kHealRate: case kHealRateMult:			label = "potion_health"; color = 0xDB2E73; break;
			case kMagicka: case kMagickaRate: case kMagickaRateMult:	label = "potion_magic"; color = 0x2E9FDB; break;
			case kStamina: case kStaminaRate: case kStaminaRateMult:	label = "potion_stam"; color = 0x51DB2E; break;
			case kFireResist:		label = "potion_fire"; color = 0xC73636; break;
			case kElectricResist:	label = "potion_shock"; color = 0xEAAB00; break;
			case kFrostResist:		label = "potion_frost"; color = 0x1FFBFF; break;
			case kDrink:			label = "food_wine"; break;
			case kFood:				label = "default_food"; break;
			case kPoison:			label = "potion_poison"; color = 0xAD00B3; break;
			default:				label = "default_potion"; break;
			}
			break;
		case kFormType_SoulGem:
		{
			const bool grand = d.soulSub == kGreater || d.soulSub == kGrand;
			const char * status = d.soulStatus == 0 ? (grand ? "soulgem_grandempty" : "soulgem_empty") :
								  d.soulStatus == 1 ? (grand ? "soulgem_grandpartial" : "soulgem_partial") :
								  d.soulStatus == 2 ? (grand ? "soulgem_grandfull" : "soulgem_full") : "misc_soulgem";
			switch(d.soulSub)
			{
			case kPetty:	label = status; color = 0xD7D4FF; break;
			case kLesser:	label = status; color = 0xC0BAFF; break;
			case kCommon:	label = status; color = 0xABA3FF; break;
			case kGreater:	label = status; color = 0x948BFC; break;
			case kGrand:	label = status; color = 0x7569FF; break;
			case kAzura:	label = "soulgem_azura"; color = 0x7569FF; break;
			default:		label = "misc_soulgem"; color = 0xE3E0FF; break;
			}
			break;
		}
		default:
			break;
		}
	}

	// ItemStack::GetEnchantmentType (from moreHUD SE), on the entry's own extra data too.
	void EnchantmentKind(Stack & s, bool & enchanted, bool & known, bool & special)
	{
		enchanted = known = special = false;
		EnchantmentItem * e = EntryEnchantment(AsEntry(s.entry));
		if(Ptr(s.dropRef))
		{
			ExtraEnchantment * x = static_cast<ExtraEnchantment *>(s.dropRef->extraData.GetByType(kExtraData_Enchantment));
			if(Ptr(x) && Ptr(x->enchant)) e = x->enchant;
		}
		if(!Ptr(e)) return;
		enchanted = true;
		BGSKeyword * disallow = s_known[kfDisallowEnchanting].formID ?
			DYNAMIC_CAST(LookupFormByID(s_known[kfDisallowEnchanting].formID), TESForm, BGSKeyword) : 0;
		if(disallow && HasKeyword(e, disallow)) { special = true; return; }
		if(e->flags & TESForm::kFlagPlayerKnows) { known = true; return; }
		EnchantmentItem * base = e->data.baseEnchantment;
		if(Ptr(base))
		{
			if(disallow && HasKeyword(base, disallow)) { special = true; return; }
			if(base->flags & TESForm::kFlagPlayerKnows) { known = true; return; }
		}
	}

	// ItemStack::IsStealing
	bool IsStealing(TESObjectREFR * container, InventoryEntryData * entry, SInt32 value)
	{
		PlayerCharacter * player = Player();
		if(!player || container == (TESObjectREFR *)player) return false;
		Actor * actor = IsActor(container) ? static_cast<Actor *>(container) : 0;
		if(actor && IsDead(actor)) return false;
		TESForm * owner = GetOwner(container);
		if(!actor && !owner) return false;
		TESForm * itemOwner = EntryOwner(entry);
		if(!itemOwner) itemOwner = actor ? (TESForm *)actor : owner;
		const bool allowed = AllowedToTake(itemOwner, value);
		return owner && !allowed;
	}

	const char * UseLabel(TESForm * object, const Data & d)
	{
		switch(d.itemType)
		{
		case kItemArmor:
		case kItemWeapon:	return "$qlie_Equip";
		case kItemBook:		return "$qlie_Read";
		case kItemFood:
		case kItemIngredient:
			if(TypeOf(object) == kFormType_Potion && d.potionSub == kDrink) return "$qlie_Use";
			return "$qlie_Eat";
		default:			return "$qlie_Use";
		}
	}

	// ---------------------------------------------------------------- sorting (LootMenu.Sorting.cpp)
	int Sign(double v) { return (v > 0) - (v < 0); }
	int Desc(bool a, bool b) { return (int)b - (int)a; }

	bool IsMisc(const QL::Row & r, int t) { return r.itemType == kItemMisc && r.miscType == t; }
	bool IsBookRow(const QL::Row & r) { return r.formType == kFormType_Book && r.bookType != kNote && r.bookType != kRecipe; }
	bool IsNoteRow(const QL::Row & r) { return r.formType == kFormType_Book && (r.bookType == kNote || r.bookType == kRecipe); }

	int ByName(const QL::Row & a, const QL::Row & b) { return strcmp(a.displayName.c_str(), b.displayName.c_str()); }

	int Compare(const char * rule, const QL::Row & a, const QL::Row & b)
	{
		if(!strcmp(rule, "Gold"))		return Desc(IsMisc(a, kGold), IsMisc(b, kGold));
		if(!strcmp(rule, "Gems"))		return Desc(IsMisc(a, kGem), IsMisc(b, kGem));
		if(!strcmp(rule, "SoulGems"))	return Desc(a.itemType == kItemSoulGem, b.itemType == kItemSoulGem);
		if(!strcmp(rule, "Lockpicks"))	return Desc(IsMisc(a, kLockpick), IsMisc(b, kLockpick));
		if(!strcmp(rule, "OresIngots"))	return Desc(IsMisc(a, kIngot), IsMisc(b, kIngot));
		if(!strcmp(rule, "Potions"))
			return Desc(a.formType == kFormType_Potion && a.potionType != kFood && a.potionType != kDrink,
						b.formType == kFormType_Potion && b.potionType != kFood && b.potionType != kDrink);
		if(!strcmp(rule, "FoodDrinks"))
			return Desc(a.formType == kFormType_Potion && (a.potionType == kFood || a.potionType == kDrink),
						b.formType == kFormType_Potion && (b.potionType == kFood || b.potionType == kDrink));
		if(!strcmp(rule, "Books"))
		{
			const bool ba = IsBookRow(a), bb = IsBookRow(b);
			if(ba && bb) return (int)a.read - (int)b.read;
			return (int)bb - (int)ba;
		}
		if(!strcmp(rule, "Notes"))
		{
			const bool na = IsNoteRow(a), nb = IsNoteRow(b);
			if(na && nb) return (int)a.read - (int)b.read;
			return (int)nb - (int)na;
		}
		if(!strcmp(rule, "Scrolls"))		return Desc(a.formType == kFormType_ScrollItem, b.formType == kFormType_ScrollItem);
		if(!strcmp(rule, "Weapons"))		return Desc(a.formType == kFormType_Weapon, b.formType == kFormType_Weapon);
		if(!strcmp(rule, "ArrowsBolts"))	return Desc(a.formType == kFormType_Ammo, b.formType == kFormType_Ammo);
		if(!strcmp(rule, "Armors"))
			return Desc(a.formType == kFormType_Armor && a.weightClass != kClothing && a.weightClass != kJewelry,
						b.formType == kFormType_Armor && b.weightClass != kClothing && b.weightClass != kJewelry);
		if(!strcmp(rule, "Clothes"))	return Desc(a.formType == kFormType_Armor && a.weightClass == kClothing, b.formType == kFormType_Armor && b.weightClass == kClothing);
		if(!strcmp(rule, "Jewelry"))	return Desc(a.formType == kFormType_Armor && a.weightClass == kJewelry, b.formType == kFormType_Armor && b.weightClass == kJewelry);
		if(!strcmp(rule, "Weightless"))	return Desc(a.weight <= 0, b.weight <= 0);
		if(!strcmp(rule, "Enchanted"))	return Desc(a.enchanted, b.enchanted);
		if(!strcmp(rule, "ByWeight"))	return Sign((double)a.weight - (double)b.weight);
		if(!strcmp(rule, "ByValue"))	return Sign((double)b.value - (double)a.value);
		if(!strcmp(rule, "ByV/W"))
		{
			const double inf = 1e300;
			const double va = a.weight <= 0 ? inf : (a.value <= 0 ? 0.0 : a.value / (double)a.weight);
			const double vb = b.weight <= 0 ? inf : (b.value <= 0 ? 0.0 : b.value / (double)b.weight);
			return Sign(vb - va);
		}
		if(!strcmp(rule, "ByName"))		return ByName(a, b);
		return 0;
	}

	// LootMenu::CanDisplay
	bool Displayable(TESForm * object)
	{
		if(!Ptr(object)) return false;
		if(!IsPlayable(object)) return false;
		const char * name = FullName(object);
		if(!name || !name[0]) return false;
		switch(object->formType)
		{
		case kFormType_ScrollItem: case kFormType_Armor: case kFormType_Book: case kFormType_Ingredient:
		case kFormType_Misc: case kFormType_Weapon: case kFormType_Key: case kFormType_Potion:
		case kFormType_Note: case kFormType_SoulGem:
			return true;
		case kFormType_Ammo:
			// SE tested a keyword whose FormID (Skyrim.esm 0x10B0A7) is in fact the boundArrow AMMO record,
			// so its test never fired; the intent - hide the conjured arrow - is applied here.
			return !Is(object, kfBoundArrow);
		case kFormType_Light:
			// TESObjectLIGH DATA flags (time, radius, colour, flags): 0x2 = can be carried.
			return (static_cast<TESObjectLIGH *>(object)->unk78.unk0C & 0x2) != 0;
		default:
			return false;
		}
	}

	// Inventory::LoadContainerInventory
	void Collect(TESObjectREFR * container, std::vector<Stack> & out)
	{
		out.clear();
		ExtraContainerChanges::Data * changes = Changes(container);
		if(changes && Ptr(changes->objList))
		{
			UInt32 guard = 0;
			for(ListNodeIter it = ListNodeIter(changes->objList); it.Valid() && guard++ < 8192; it.Next())
			{
				InventoryEntryData * e = (InventoryEntryData *)it.Item();
				if(!Ptr(e) || !Displayable(e->type)) continue;
				Stack s;
				s.entry.type = e->type;
				s.entry.extendDataList = Ptr(e->extendDataList) ? e->extendDataList : 0;
				s.entry.countDelta = e->countDelta;
				s.dropHandle = 0;
				s.dropRef = 0;
				out.push_back(s);
			}
		}

		TESContainer * base = BaseContainer(container);
		if(base && Ptr(base->entries) && base->numEntries < 65536)
		{
			for(UInt32 i = 0; i < base->numEntries; ++i)
			{
				TESContainer::Entry * c = base->entries[i];
				if(!Ptr(c) || !Ptr(c->form)) continue;
				TESForm * object = c->form;
				if(object->formType == kFormType_LeveledItem || !Displayable(object)) continue;
				bool merged = false;
				for(size_t j = 0; j < out.size(); ++j)
				{
					if(out[j].dropHandle == 0 && out[j].entry.type == object)
					{
						if(!EntryHasExtra(AsEntry(out[j].entry), kExtraData_LeveledItem)) out[j].entry.countDelta += (SInt32)c->count;
						merged = true;
						break;
					}
				}
				if(!merged)
				{
					Stack s;
					s.entry.type = object;
					s.entry.extendDataList = 0;
					s.entry.countDelta = (SInt32)c->count;
					s.dropHandle = 0;
					s.dropRef = 0;
					out.push_back(s);
				}
			}
		}

		UInt32 handles[256];
		UInt32 count = 0;
		DroppedHandles(container, handles, count, 256);
		for(UInt32 i = 0; i < count; ++i)
		{
			TESObjectREFR * ref = Lookup(handles[i]);
			if(!ref) continue;
			TESForm * object = Base(ref);
			if(IsDeletedOrDisabled(ref) || !Displayable(object)) { Release(ref); continue; }
			Stack s;
			s.entry.type = object;
			s.entry.extendDataList = 0;
			s.entry.countDelta = RefCount(ref);
			s.dropHandle = handles[i];
			s.dropRef = ref;		// released by the caller
			out.push_back(s);
		}

		for(size_t i = 0; i < out.size(); )
		{
			if(out[i].entry.countDelta <= 0)
			{
				if(out[i].dropRef) Release(out[i].dropRef);
				out.erase(out.begin() + i);
			}
			else ++i;
		}
	}
}

namespace QL
{
	namespace Items
	{
		void Init()
		{
			int found = 0;
			for(int i = 0; i < kfCount; ++i)
			{
				TESForm * f = LookupForm(s_known[i].plugin, s_known[i].id);
				s_known[i].formID = f ? f->formID : 0;
				if(f) ++found;
			}
			QL_LOG("known forms resolved: %d of %d", found, (int)kfCount);
		}

		bool CanDisplay(TESForm * object) { return Displayable(object); }

		void Sort(std::vector<Row> & rows, const std::vector<std::string> & rules)
		{
			std::stable_sort(rows.begin(), rows.end(), [&](const Row & a, const Row & b) {
				for(size_t i = 0; i < rules.size(); ++i)
				{
					const int c = Compare(rules[i].c_str(), a, b);
					if(c < 0) return true;
					if(c > 0) return false;
				}
				return ByName(a, b) < 0;
			});
		}

		void Build(TESObjectREFR * container, const Settings & s, std::vector<Row> & out)
		{
			out.clear();
			if(!Ptr(container)) return;
			std::vector<Stack> stacks;
			Collect(container, stacks);

			std::vector<Data> data(stacks.size());
			std::vector<float> score(stacks.size(), 0.0f);
			out.reserve(stacks.size());
			for(size_t i = 0; i < stacks.size(); ++i)
			{
				Stack & st = stacks[i];
				TESForm * object = st.entry.type;
				Classify(object, AsEntry(st.entry), data[i]);

				Row r;
				r.formID		= object->formID;
				r.dropHandle	= st.dropHandle;
				r.count			= st.entry.countDelta;
				r.displayName	= EntryName(AsEntry(st.entry));
				if(r.displayName.empty()) r.displayName = FullName(object);
				r.value			= EntryValue(AsEntry(st.entry));
				const float w	= FormWeight(object);
				r.weight		= w > 0.0f ? w : 0.0f;
				r.stealing		= !st.dropHandle && IsStealing(container, AsEntry(st.entry), r.value);
				r.read			= s.showRead && TypeOf(object) == kFormType_Book && data[i].bookRead;
				bool en, known, special;
				EnchantmentKind(st, en, known, special);
				r.enchanted			= s.showEnchanted && en;
				r.knownEnchanted	= s.showEnchanted && s.showEnchantedKnown && known;
				r.specialEnchanted	= s.showEnchanted && s.showEnchantedSpecial && special;
				r.bestInClass		= false;
				SelectIcon(object, data[i], r.iconLabel, r.iconColor);
				if(!r.iconLabel) r.iconLabel = "";
				r.useLabel		= UseLabel(object, data[i]);
				r.formType		= object->formType;
				r.itemType		= data[i].itemType;
				r.miscType		= data[i].miscSub;
				r.potionType	= data[i].potionSub;
				r.bookType		= data[i].bookSub;
				r.weightClass	= data[i].weightClass;
				out.push_back(r);
			}

			// Inventory::FindBestInClassItems, the player's own items included.
			if(s.showBestInClass && !out.empty())
			{
				struct Best { int index; float value; };
				Best armor[32], oneHand = { -1, 0 }, twoHand = { -1, 0 }, ranged = { -1, 0 }, ammo = { -1, 0 };
				for(int i = 0; i < 32; ++i) { armor[i].index = -1; armor[i].value = 0; }
				auto check = [&](InventoryEntryData * e, int index) {
					TESForm * object = e->type;
					switch(TypeOf(object))
					{
					case kFormType_Armor:
					{
						const UInt32 parts = static_cast<TESObjectARMO *>(object)->bipedObject.data.parts;
						unsigned long slot = 0;
						if(_BitScanForward(&slot, parts))
						{
							const float v = ArmorValue(e);
							if(v > armor[slot].value) { armor[slot].value = v; armor[slot].index = index; }
						}
						break;
					}
					case kFormType_Weapon:
					{
						const UInt8 t = static_cast<TESObjectWEAP *>(object)->gameData.type;
						Best * b = 0;
						if(t == TESObjectWEAP::GameData::kType_OneHandAxe || t == TESObjectWEAP::GameData::kType_OneHandMace ||
						   t == TESObjectWEAP::GameData::kType_OneHandSword || t == TESObjectWEAP::GameData::kType_OneHandDagger) b = &oneHand;
						else if(t == TESObjectWEAP::GameData::kType_TwoHandAxe || t == TESObjectWEAP::GameData::kType_TwoHandSword) b = &twoHand;
						else if(t == TESObjectWEAP::GameData::kType_Bow || t == TESObjectWEAP::GameData::kType_CrossBow) b = &ranged;
						if(b) { const float v = Damage(e); if(v > b->value) { b->value = v; b->index = index; } }
						break;
					}
					case kFormType_Ammo:
					{
						const float v = Damage(e);
						if(v > ammo.value) { ammo.value = v; ammo.index = index; }
						break;
					}
					default: break;
					}
				};
				for(size_t i = 0; i < stacks.size(); ++i) check(AsEntry(stacks[i].entry), (int)i);
				ExtraContainerChanges::Data * pc = Changes(Player());
				if(pc && Ptr(pc->objList))
				{
					UInt32 guard = 0;
					for(ListNodeIter it = ListNodeIter(pc->objList); it.Valid() && guard++ < 8192; it.Next())
					{
						InventoryEntryData * e = (InventoryEntryData *)it.Item();
						if(Ptr(e) && Ptr(e->type)) check(e, -1);
					}
				}
				for(int i = 0; i < 32; ++i) if(armor[i].index >= 0) out[armor[i].index].bestInClass = true;
				if(oneHand.index >= 0) out[oneHand.index].bestInClass = true;
				if(twoHand.index >= 0) out[twoHand.index].bestInClass = true;
				if(ranged.index >= 0) out[ranged.index].bestInClass = true;
				if(ammo.index >= 0) out[ammo.index].bestInClass = true;
			}

			for(size_t i = 0; i < stacks.size(); ++i)
				if(stacks[i].dropRef) Release(stacks[i].dropRef);

			Sort(out, s.sortRules);
		}
	}
}
