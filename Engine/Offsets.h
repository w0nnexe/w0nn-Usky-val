#pragma once
#include <cstddef>

// ============================================================================
// w0nn-valo OFFSET DOSYASI — TEK KAYNAK
// ============================================================================
// Oyun guncellemesinde DEGISENLER: asagidaki "GUNCELLEMEDE DEGISIR" bolumu
// Oyun guncellemesinde DEGISMEYENLER: "STABIL" bolumu (sinif alanlari)
//
// Guncelleme geldiginde YAPMAN GEREEN:
//   1. Yeni dump al (Dumper-7 veya offset araci)
//   2. Asagidaki "GUNCELLEMEDE DEGISIR" degerlerini guncelle
//   3. Derle — bitti
// ============================================================================

namespace offs
{
	// ========================================================================
	// GUNCELLEMEDE DEGISIR — Bu degerleri her oyuncu guncellemesinde yenile!
	// (Bunlar RVA = oyun exe'sine goreli adres, GetModuleHandle(NULL) + deger)
	// Son guncelleme: 29.10.2026 12:52
	// ========================================================================
	namespace rva
	{
		// --- ZORUNLU — bunlar olmadan hicbir sey calismaz ---
		inline constexpr std::ptrdiff_t gworld              = 0xD727560; // GWorld pointer (oWorld makrosu)
		inline constexpr std::ptrdiff_t static_find_object  = 0x1D07DD0; // StaticFindObject fonksiyonu
		// ProcessEvent: vtable[83] kullaniyoruz (SDK ProcessEventIdx=0x53) — RVA gerekmez

		// --- ISTEGER — skin unlock icin ---
		inline constexpr std::ptrdiff_t gobjects            = 0xD7B68D0; // GUObjectArray (TUM objelerin listesi)
		inline constexpr std::ptrdiff_t gobjects_decrypt    = 0x1CD9610; // GObjects sifre cozucu

		// --- ISTEGER — FName/isim cozme ---
		inline constexpr std::ptrdiff_t fname_pool          = 0xD8C0800; // FNamePool (GNames)
		inline constexpr std::ptrdiff_t fname_to_string     = 0x1A11F30; // FName::ToString fonksiyonu
		inline constexpr std::ptrdiff_t get_name_entry      = 0x1A7B630; // GetNameEntryFromName

		// --- ISTEGER — glow/chams ---
		inline constexpr std::ptrdiff_t set_ares_outline    = 0x9A04D90; // SetAresOutlineMode

		// --- ISTEGER — diger ---
		inline constexpr std::ptrdiff_t bone_matrix         = 0x42463A0; // GetBoneMatrix
		inline constexpr std::ptrdiff_t append_string       = 0x1A11F30; // FString append (isim cozme)
	}

	// ========================================================================
	// STABIL — Bunlar oyun guncellemelerinde NADIREN degisir
	// (Sinif icerisindeki alan konumlari; Dumper-7 SDK'dan dogrulanir)
	// ========================================================================

	// ---- UWorld / Engine ----
	namespace core
	{
		inline constexpr std::ptrdiff_t uworld_pointer        = 0xC0;   // UWorld* (viewport'tan)
		inline constexpr std::ptrdiff_t game_instance         = 0x1D8;  // OwningGameInstance
		inline constexpr std::ptrdiff_t gamestate             = 0x178;
		inline constexpr std::ptrdiff_t persistent_level      = 0x38;
		inline constexpr std::ptrdiff_t local_players         = 0x40;   // GameInstance.LocalPlayers
		inline constexpr std::ptrdiff_t localplayer           = 0x40;
		inline constexpr std::ptrdiff_t player_controller     = 0x38;   // LocalPlayer.PlayerController
		inline constexpr std::ptrdiff_t control_rotation      = 0x4E8;
		inline constexpr std::ptrdiff_t fname_pool            = 0xD8C0800; // Names.h bunu kullanir (rva::fname_pool ile ayni)
	}

	// ---- Pawn / Character ----
	namespace player
	{
		inline constexpr std::ptrdiff_t pawn                  = 0x518;  // PlayerController.AcknowledgedPawn
		inline constexpr std::ptrdiff_t player_state          = 0x488;  // Pawn.PlayerState
		inline constexpr std::ptrdiff_t root_component        = 0x290;
		inline constexpr std::ptrdiff_t root_position         = 0x170;
		inline constexpr std::ptrdiff_t inventory             = 0xC08;  // ShooterCharacter.Inventory
		inline constexpr std::ptrdiff_t current_equippable    = 0x278;  // Inventory.CurrentEquippable
		inline constexpr std::ptrdiff_t was_ally              = 0xF58;
		inline constexpr std::ptrdiff_t damage_handler        = 0xC70;
		inline constexpr std::ptrdiff_t root_velocity         = 0x1B8;  // RootComponent.ComponentVelocity
	}

	// ---- Mesh / Skeleton ----
	namespace mesh
	{
		inline constexpr std::ptrdiff_t mesh                  = 0x4F0;  // Character.Mesh (3P)
		inline constexpr std::ptrdiff_t component_to_world    = 0x300;
		inline constexpr std::ptrdiff_t bone_array            = 0x760;
		inline constexpr std::ptrdiff_t bone_array_cache      = 0x770;
		inline constexpr std::ptrdiff_t bone_count            = 0x768;
	}

	// ---- Health ----
	namespace hp
	{
		inline constexpr std::ptrdiff_t health                = 0x200;  // DamageableComponent.CachedLife[0]
		inline constexpr std::ptrdiff_t shield_current        = 0x208;  // CachedLife[1]
	}

	// ---- PlayerState ----
	namespace player_state
	{
		inline constexpr std::ptrdiff_t player_name           = 0x508;  // APlayerState.PlayerNamePrivate
		inline constexpr std::ptrdiff_t competitive_tier      = 0x734;  // AresPlayerStateBase
		inline constexpr std::ptrdiff_t pawn_private          = 0x4E8;
	}

	// ---- Level / Actors ----
	namespace actor
	{
		inline constexpr std::ptrdiff_t actor_array           = 0xA0;   // ULevel.Actors
		inline constexpr std::ptrdiff_t actors_count          = 0xA8;
		inline constexpr std::ptrdiff_t class_private         = 0x10;   // UObject.ClassPrivate
		inline constexpr std::ptrdiff_t name_index            = 0x18;   // UObject.NamePrivate (FName)
		inline constexpr std::ptrdiff_t actor_id              = 0x18;   // UObject.InternalIndex (ayni alan)
	}

	// ---- Skin Unlock ----
	namespace skin
	{
		inline constexpr std::ptrdiff_t b_is_owned            = 0x10A;  // UBaseInventoryModel.bIsOwned
		inline constexpr std::ptrdiff_t b_unlocked_config     = 0x116;  // UBaseInventoryModel.bIsUnlockedByConfig
		inline constexpr std::ptrdiff_t possible_skins        = 0x168;  // UEquippableInventoryModel
		inline constexpr std::ptrdiff_t possible_chromas      = 0x140;  // UEquippableSkinInventoryModel
		inline constexpr std::ptrdiff_t possible_levels       = 0x150;  // UEquippableSkinInventoryModel
	}

	// ---- UObject yapisal ----
	namespace uobject
	{
		inline constexpr std::ptrdiff_t super_struct          = 0x48;   // UStruct.SuperStruct (sinif zinciri)
		inline constexpr std::ptrdiff_t internal_index        = 0xC;    // UObject.InternalIndex
	}

	// ---- Minimap (UV kesfi — radar) ----
	namespace minimap
	{
		inline constexpr std::ptrdiff_t minimap_component     = 0x1578;
		inline constexpr std::ptrdiff_t visioncones           = 0x1580;
		inline constexpr std::ptrdiff_t shooter_character     = 0x1218; // eski aday (fallback)
		inline constexpr std::ptrdiff_t character_portrait    = 0x1210; // eski aday (fallback)
	}
}
