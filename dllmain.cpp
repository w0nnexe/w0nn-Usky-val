// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include <windows.h>
#include <iostream>
#include <string>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <map>
#include "Utils/Utils.h"
#include <thread>
#include"Utils/HookMiNiWorld.h"
#include "hooking/kiero.h"
#include "gui/gui.h"
#include "hooking/hooks/present.h"
#include "web/webserver.h"
#include "Engine/Skeleton.h"
#include "Engine/Names.h"

inline Hook::CVMTable Table;
using FN = void(__fastcall*)(void*, UCanvas* Canvas);
int32_t previousIndex = -1;
bool sstate = 1;

inline float fov = 200.f;
float dissource = FLT_MAX;
bool enableis = 0, unlockall = 0;

ACharacter* AimPlayer = NULL;

// Kemik sistemi YENIDEN AKTIF (Engine/Skeleton.h): BoneMatrix native cagrisi YOK — saf
// bellek okuma (Mesh tarama + ComponentSpaceTransforms + ComponentToWorld dogrulamali).
// Mesh offset'i runtime'da yapisal olarak bulunur (liste adayi 0x4F0 dahil); bulunamazsa
// iskelet kendini kapatir. Kalkan: DamageHandler uzerinden saf okuma (offs::hp).

// Glow denemeleri TAMAMEN KALDIRILDI: motor glow'u mevcut offsetlerle mumkun degil
// (visibility bileseni oyunda yok, custom depth stencil cizilmiyor, outline_mode eziliyor).

// DLL klasorune radar_debug.log yazar. CONSOLE YAZMA YOK: injector konsolu donup
// WriteConsole bloklandiginda cagiran thread (oyun) de donuyor ve cokuyordu.
// Tum loglar yalniz dosyaya gider — gormek icin dosyayi acik tut (Notepad++ tail vb).
void RadarDbg(const char* msg)
{
	char path[MAX_PATH] = {};
	HMODULE m = nullptr;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&RadarDbg, &m);
	if (m && GetModuleFileNameA(m, path, MAX_PATH))
	{
		std::string s(path);
		size_t p = s.find_last_of("\\/");
		if (p != std::string::npos) s = s.substr(0, p + 1) + "radar_debug.log";
		else s = "radar_debug.log";
		FILE* f = nullptr;
		fopen_s(&f, s.c_str(), "a");
		if (f)
		{
			SYSTEMTIME t; GetLocalTime(&t);
			fprintf(f, "[%02u:%02u:%02u] %s\n", t.wHour, t.wMinute, t.wSecond, msg);
			fclose(f);
		}
	}
}

// printf yerine: ayni log dosyasina yazar (konsola HIC yazmaz)
void RadarPrintf(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
	va_end(ap);
	RadarDbg(buf);
}

// ---- VISIBILITY CHECK (K2 LineTraceSingle) ----
// FHitResult'in dump'ta boyu bilinmedigi icin runtime kalibrasyonu yapilir:
// gokyuzu izi (hit YOK) + yere izi (hit VAR) beklenen davranisi veren hitSize x TraceChannel
// kombinasyonu aranir. Hicbiri tutmazsa (veya fonksiyon yoksa) visibility FILTRESIZ calisir —
// asla crash etmez: param buffer'i gercek frame'den buyuk tutuldu.
struct VisCacheEntry { ULONGLONG stamp; bool vis; };
static std::map<ACharacter*, VisCacheEntry> s_visCache;
static int s_visHitSize = -1;     // -1: kalibre edilmedi, -2: devre disi, digeri: secilen boyut
static uint8_t s_visChannel = 0;
static ULONGLONG s_visCalibNext = 0;

static bool VisCalibrate(UWorld* World, ACharacter* LocalPlayer)
{
	FVector eye = LocalPlayer->K2_GetActorLocation();
	eye.Z += 60.f;
	FVector sky = eye; sky.Z += 50000.f;
	FVector flr = eye; flr.Z -= 5000.f;
	void* self[1] = { LocalPlayer };
	const int sizes[3] = { 0xB0, 0xB8, 0xA8 };
	const uint8_t chans[2] = { 0, 1 };
	for (int si = 0; si < 3; si++)
		for (int ci = 0; ci < 2; ci++)
		{
			bool hitSky = true, hitFloor = false;
			if (!UGameplayStatics::LineTraceRaw(World, eye, sky, self, 1, chans[ci], sizes[si], hitSky))
			{
				s_visHitSize = -2; // fonksiyon bulunamadi
				RadarDbg("vis: LineTraceSingle bulunamadi — visibility check kapali");
				return false;
			}
			UGameplayStatics::LineTraceRaw(World, eye, flr, self, 1, chans[ci], sizes[si], hitFloor);
			if (!hitSky && hitFloor)
			{
				s_visHitSize = sizes[si];
				s_visChannel = chans[ci];
				char m[96];
				snprintf(m, sizeof(m), "vis: LineTrace kalibre edildi (hitSize=0x%X, channel=%u)", sizes[si], chans[ci]);
				RadarDbg(m);
				return true;
			}
		}
	s_visHitSize = -2;
	RadarDbg("vis: kalibrasyon basarisiz — visibility check filtresiz (duvar kontrolu yok)");
	return false;
}

// Target hedef noktasi (goz seviyesi) LocalPlayer'dan gorunuyor mu?
static bool IsTargetVisible(UWorld* World, ACharacter* LocalPlayer, ACharacter* Target, const FVector& TargetEye)
{
	ULONGLONG now = GetTickCount64();
	if (s_visHitSize == -2) return true;  // devre disi: filtre yok
	if (s_visHitSize == -1)
	{
		if (now < s_visCalibNext) return true;
		s_visCalibNext = now + 5000;      // 5 sn'de bir yeniden dene
		if (!VisCalibrate(World, LocalPlayer)) return true;
	}
	// 120ms cache — ayni hedef icin her karede trace atmayla PostRender yorulmasin
	auto it = s_visCache.find(Target);
	if (it != s_visCache.end() && now - it->second.stamp < 120)
		return it->second.vis;
	FVector eye = LocalPlayer->K2_GetActorLocation();
	eye.Z += 60.f;
	void* ignore[2] = { LocalPlayer, Target };
	bool hit = false;
	UGameplayStatics::LineTraceRaw(World, eye, TargetEye, ignore, 2, s_visChannel, (size_t)s_visHitSize, hit);
	bool vis = !hit; // hedef yoksayildi: bir sey carptiysa duvar/sis arkasindadir
	if (s_visCache.size() > 64) s_visCache.clear();
	s_visCache[Target] = { now, vis };
	return vis;
}


// RVA 29.10 dump: FName::ToString = 0x1A11F30 (tek fonksiyon)
struct MiniFString { wchar_t* Data; int32_t Num; int32_t Max; };
static void* s_fnFNameToString = nullptr;

static bool GameFNameToString(int32_t cmpIdx, char* out, size_t outLen)
{
	if (!s_fnFNameToString)
		s_fnFNameToString = (void*)((DWORD64)GetModuleHandleA(0) + offs::rva::fname_to_string);
	if (!s_fnFNameToString || IsBadCodePtr((FARPROC)s_fnFNameToString)) return false;
	wchar_t buf[256] = {};
	MiniFString fs = { buf, 0, 256 };
	// FName::ToString — UE4 x64 convention denemeleri:
	// Form A: ToString(FString& Out) → RCX=this(FName*), RDX=&Out
	// Form B: FString ToString() → RCX=&ret, RDX=this(FName*)
	int32_t fname[3] = { cmpIdx, 0, cmpIdx };
	__try
	{
		// Once Form A dene
		((void(*)(void*, void*))s_fnFNameToString)((void*)fname, &fs);
		if (fs.Num <= 0 || fs.Num > 255 || fs.Data != buf)
		{
			// Form B dene (hidden return)
			fs = { buf, 0, 256 };
			((void(*)(void*, void*))s_fnFNameToString)((void*)&fs, (void*)fname);
		}
	}
	__except (1) { return false; }
	if (fs.Num <= 0 || fs.Num > 255 || fs.Data != buf) return false;
	int i = 0;
	for (; i < fs.Num && i < (int)outLen - 1; i++)
	{
		const wchar_t c = buf[i];
		out[i] = (c >= 32 && c < 127) ? (char)c : '?';
	}
	out[i] = 0;
	return i > 0;
}
// GObjects = 0xd7b97c0 (guncel dump). TUObjectArray iterasyonu ile tum objeler
// taranir, UBaseInventoryModel turevleri bulunur, bIsOwned=1 yazilir.
// SEH korumali — yanlis pointer'da o objeyi atlar, ASLA crash olmaz.

static void* s_invClass = (void*)1;   // UClass* UBaseInventoryModel (cache)
static ULONGLONG s_sk4Next = 0;
static int s_sk4Pass = 0;

// guvenli ptr oku (SEH korumali)
static bool Sk4Read(void* addr, void** out)
{
	__try { *out = *(void**)addr; return true; }
	__except (1) { return false; }
}

// sinif zinciri UBaseInventoryModel'e ulasiyor mu?
static bool Sk4IsInvModel(uint8_t* cls)
{
	if (s_invClass == (void*)1)
		s_invClass = UObject::StaticFindObject(NULL, NULL, L"ShooterGame.BaseInventoryModel", false);
	if (!s_invClass) return false;
	for (int i = 0; i < 12 && cls; i++)
	{
		if ((void*)cls == s_invClass) return true;
		void* sup = nullptr;
		if (!Sk4Read(cls + 0x48, &sup)) return false;
		cls = (uint8_t*)sup;
	}
	return false;
}

static void SkinUnlockTick(UWorld* World)
{
	static bool s_firstCall = true;
	if (s_firstCall) { s_firstCall = false; RadarDbg("skin5: fonksiyon cagrildi (toggle durumunu bekliyor)"); }
	if (!features::unlockSkins) return;
	const ULONGLONG now = GetTickCount64();
	if (now < s_sk4Next) return;
	s_sk4Next = now + 3000;

	// GObjects: 0xD7B68D0 (29.10 guncel dump) + Decrypt: 0x1CD9610
	uint8_t* base = (uint8_t*)GetModuleHandleA(0);
	uint8_t* goArr = base + offs::rva::gobjects;
	if (IsBadReadPtr(goArr, 0x20)) { RadarDbg("skin5: GObjects adresi okunamadi"); return; }

	uint8_t** chunks = *(uint8_t***)goArr;
	int numEl = *(const int*)(goArr + 0x14);
	int numCh = *(const int*)(goArr + 0x1C);

	// degerler sacmaysa decrypt fonksiyonunu dene
	if (!chunks || numEl <= 0 || numEl > 5000000 || numCh <= 0 || numCh > 1000)
	{
		typedef uint8_t* (*DecryptFn)(uint8_t*);
		DecryptFn decrypt = (DecryptFn)(base + offs::rva::gobjects_decrypt);
		if (!IsBadCodePtr((FARPROC)decrypt))
		{
			__try
			{
				uint8_t* decrypted = decrypt(*(uint8_t**)goArr);
				if (decrypted && !IsBadReadPtr(decrypted, 0x20))
				{
					chunks = *(uint8_t***)decrypted;
					numEl = *(const int*)(decrypted + 0x14);
					numCh = *(const int*)(decrypted + 0x1C);
					if (s_sk4Pass == 0) RadarDbg("skin5: GObjects decrypt CALISTI");
				}
			}
			__except (1) { }
		}
	}

	if (!chunks || numEl <= 0 || numEl > 5000000 || numCh <= 0 || numCh > 1000)
	{
		if (s_sk4Pass == 0)
		{
			char m[128];
			snprintf(m, sizeof(m), "skin5: GObjects gecersiz (chunks=%p el=%d ch=%d)", (void*)chunks, numEl, numCh);
			RadarDbg(m);
			s_sk4Pass = 1;
		}
		return;
	}
	if (IsBadReadPtr(chunks, 8) || IsBadReadPtr(chunks[0], 0x100))
	{
		if (s_sk4Pass <= 1) RadarDbg("skin5: chunks pointer gecersiz");
		s_sk4Pass = 2;
		return;
	}

	if (s_sk4Pass <= 1)
	{
		char m[96];
		snprintf(m, sizeof(m), "skin5: GObjects OK (obje=%d chunk=%d)", numEl, numCh);
		RadarDbg(m);
		s_sk4Pass = 2;
	}

	// sinif bul
	if (s_invClass == (void*)1)
		s_invClass = UObject::StaticFindObject(NULL, NULL, L"ShooterGame.BaseInventoryModel", false);
	if (!s_invClass) { RadarDbg("skin5: BaseInventoryModel sinifi bulunamadi"); return; }

	int nGun = 0, nSkin = 0, nSub = 0;
	const int limit = numEl < 400000 ? numEl : 400000;

	for (int i = 0; i < limit; i++)
	{
		const int ci = i / 0x10000;
		if (ci >= numCh) break;
		uint8_t* chunk = nullptr;
		if (!Sk4Read(chunks + ci, (void**)&chunk) || !chunk) continue;
		uint8_t* obj = nullptr;
		if (!Sk4Read(chunk + (size_t)(i % 0x10000) * 0x18, (void**)&obj) || !obj) continue;
		uint8_t* cls = nullptr;
		if (!Sk4Read(obj + 0x10, (void**)&cls) || !cls) continue;
		if (!Sk4IsInvModel(cls)) continue;

		__try
		{
			obj[offs::skin::b_is_owned] = 1;
			obj[offs::skin::b_unlocked_config] = 1;
			nGun++;

			uint8_t** sk = *(uint8_t***)((uint8_t*)obj + 0x168);
			const int sn = *(const int*)(((uint8_t*)obj) + offs::skin::possible_skins + 8);
			if (sk && sn > 0 && sn < 500)
			{
				for (int j = 0; j < sn; j++)
				{
					uint8_t* sm = nullptr;
					if (!Sk4Read(sk + j, (void**)&sm) || !sm) continue;
					sm[offs::skin::b_is_owned] = 1;
					sm[offs::skin::b_unlocked_config] = 1;
					nSkin++;

					for (int sub = 0; sub < 2; sub++)
					{
						const int so = sub == 0 ? offs::skin::possible_chromas : offs::skin::possible_levels;
						uint8_t** sa = *(uint8_t***)((uint8_t*)sm + so);
						const int sc = *(const int*)((uint8_t*)sm + so + 8);
						if (!sa || sc <= 0 || sc > 500) continue;
						for (int k = 0; k < sc; k++)
						{
							uint8_t* cm = nullptr;
							if (!Sk4Read(sa + k, (void**)&cm) || !cm) continue;
							cm[offs::skin::b_is_owned] = 1;
							cm[offs::skin::b_unlocked_config] = 1;
							nSub++;
						}
					}
				}
			}
		}
		__except (1) { continue; }
	}

	static int s_lastLog = 0;
	if (nGun > 0 || s_lastLog == 0)
	{
		s_lastLog = 1;
		char m[128];
		snprintf(m, sizeof(m), "skin5: silah=%d skin=%d chroma/level=%d ACILDI", nGun, nSkin, nSub);
		RadarDbg(m);
	}
}

auto WallHack(void* View) -> void
{
	// PANIC: her sey passthrough — oyun kapatilmadan test dongusu
	if (g_panic) return;

	///Engine/Transient.GPGameEngine_2147482525   ENGINE
	UWorld* World = ((FGameViewportClient*)View)->World;
	if (IsBadReadPtr(World, 8) || !World)
	{
		return;
	}

	


	// skin unlock v3 (saf okuma, ProcessEvent yok)
	SkinUnlockTick(World);

	auto LocalPlayerController = UObject::GetLocalPlayerController(World);
	if (!LocalPlayerController)
	{
		return;
	}

	ACharacter* LocalPlayer = LocalPlayerController->K2_GetPawn();
	if (IsBadReadPtr(LocalPlayer, 8) || !LocalPlayer)
	{
		// Menu / yukleme ekrani: pawn yok. Eski pozisyon kalirsa web radar'da isaret
		// oldugu yerde DONUK kaliyordu — veriyi sifirla, sayfa 'veri bekleniyor'a donsun.
		std::lock_guard<std::mutex> lock(g_radarMutex);
		globalData.clear();
		g_localX = 0.f; g_localY = 0.f; g_localZ = 0.f; g_localYaw = 0.f;
		return;
	}

	void* PlayerClass = UObject::GetPlayerClass();
	if (!PlayerClass)
	{
		return;
	}

	


	TArray<ACharacter*> Actors = NULL;
	UGameplayStatics::GetAllActorsOfClass(World, PlayerClass, &Actors);
	if (!Actors.IsValid()) {

		return;
	}
	APlayerCameraManager* Camera = UObject::GetPlayerCameraManager(World);
	if (!Camera) {
		return;
	}
	std::vector<playerData> tempData;
	for (size_t i = 0; i < Actors.Num(); i++)
	{
		if (IsBadReadPtr(Actors.Data[i], 8) || !Actors.Data[i])
		{
			continue;
		}

		ACharacter* Player = Actors.Data[i];

		if (IsBadReadPtr(Player, 8) || !Player)
		{
			continue;
		}

		if (Player == LocalPlayer) // kendi pawn'i radar/ESP listesinde olmaz
			continue;

		float health = Player->GetHealth();
		if (health <= 0)
			continue;

		UMovementComponent* CharacterMovement = Player->CharacterMovement;

		if (!CharacterMovement)
			continue;

		bool crouch = Player->CharacterMovement->IsCrouching();

		FVector ActorLoc = Player->K2_GetActorLocation();
		if (!ActorLoc.X && !ActorLoc.Y && !ActorLoc.Z) continue;

		playerData data;

		// RADAR: butun canli oyuncular toplanir — ekran disi/aradaki oyuncular DAHIL.
		// (Eski kod W2S + ekran kirpma yuzunden sadece onumuzdekileri gonderiyordu;
		// radar bu yuzden oyunculari kaciriyordu.) Takim arkadaslari ally bayragiyla gider.
		data.id = (uint64_t)Player;
		data.ally = Player->WasAlly ? true : false;
		data.health = health;
		data.crouch = crouch;
		data.worldX = ActorLoc.X; // 2D web radar icin dunya koordinatlari
		data.worldY = ActorLoc.Y;
		data.worldZ = ActorLoc.Z;
		data.distance = 0.f; // metre: local pozisyon bulunduktan sonra asagida dolduruluyor
		data.boxW = 0.f;    // 0 = ekranda gorunmuyor (kutu ESP bu kaydi cizmez)

		// KUTU ESP (yalnizca dusman + kafa/ayak projeksiyonu ekrana dusuyorsa):
		// acidan bagimsiz dikdortgen; basarisiz W2S kaydi atmaz, sadece kutuyu bos birakir.
		// aimAllies aciksa dostlar da kutu uretir (aim testi icin; cizim yine ayri kosullu)
		if (!data.ally || features::aimAllies)
		{
			FVector headPt = { ActorLoc.X, ActorLoc.Y, ActorLoc.Z + (crouch ? 55.f : 95.f) };
			FVector feetPt = { ActorLoc.X, ActorLoc.Y, ActorLoc.Z - 90.f };
			FVector2D headScr = {}, feetScr = {};
			if (LocalPlayerController->W2S(headPt, headScr) &&
				LocalPlayerController->W2S(feetPt, feetScr))
			{
				float boxH = feetScr.Y - headScr.Y;
				if (boxH >= 6.f) // cok uzak / degenerate kutulari at
				{
					float boxW = boxH * 0.45f;
					float cx = (headScr.X + feetScr.X) * 0.5f;
					bool offX = cx + boxW < 0.f || cx - boxW > (float)GetSystemMetrics(SM_CXSCREEN);
					bool offY = feetScr.Y < 0.f || headScr.Y > (float)GetSystemMetrics(SM_CYSCREEN);
					if (!offX && !offY)
					{
						data.boxX = cx - boxW * 0.5f;
						data.boxY = headScr.Y;
						data.boxW = boxW;
						data.boxH = boxH;
					}
				}
			}
		}

		// visibility: LineTrace (minimap okumasi kaldirildi — crash)
		if (!data.ally && data.boxW > 1.f)
		{
			FVector tgtEye = ActorLoc;
			tgtEye.Z += crouch ? 45.f : 75.f;
			data.vis = IsTargetVisible(World, LocalPlayer, Player, tgtEye);
		}


		// iskelet: yalnizca ekrandaki dusmanlar ve mod acikken - W2S (ProcessEvent)
		// kemik basina cagrildigi icin FillBones zaten uc noktalarla sinirlar
		if ((features::skeletonESP || features::boneDebug) && !data.ally && data.boxW > 1.f)
			skel::FillBones(Player, LocalPlayerController, ActorLoc, data);

		tempData.push_back(data);
	}

	// Context swap: PostRender (oyun thread'i) -> globalData. Artik web sunucu thread'i de
	// okudugu icin kilit altinda takas yapiliyor; ImGui Render ve web /data ayni mutex'i kullanir.
	//
	// DIKKAT: Burada APlayerCameraManager->GetCameraLocation/Rotation (ProcessEvent) KULLANILMIYOR.
	// Level yukleme gecisinde Camera stale pointer oluyor -> AV -> oyun loading'de donuyordu.
	// Yerine zaten null-check'li LocalPlayer (pawn) ve LocalPlayerController kullaniliyor.
	FVector LocalLoc = LocalPlayer->K2_GetActorLocation();
	bool localValid = LocalLoc.X || LocalLoc.Y || LocalLoc.Z;
	FVector CtrlRot = {};
	if (localValid)
		CtrlRot = LocalPlayerController->GetControlRotation(); // FVector layout: X=Pitch, Y=Yaw, Z=Roll

	// kutu ESP mesafe etiketi (metre)
	if (localValid)
		for (auto& d : tempData)
			d.distance = sqrt(pow(d.worldX - LocalLoc.X, 2) + pow(d.worldY - LocalLoc.Y, 2)) / 100.f;

	// ---- aim rotasyon uygulamasi (features::aimMethod==1) ----
	// Render thread (AimAssistTick) hedefi secer -> g_aimTgt*; SetControlRotation ProcessEvent'ini
	// yalnizca oyun thread'inden cagirmak guvenli (render thread yarisi yok). SendInput yolu
	// bu oyunta yok sayildigi icin varsayilan yontem budur.
	static ULONGLONG s_apLast = 0;
	static float s_apPress = 0.f;
	// hedef hiz ongorme (lead): kosan hedefe exponential-smoothing yarattigi arkadan
	// takimayi kapatir — hedef dunya hizi olculup hareket yonunde onde nisi alinir
	static FVector s_prevTgt = {};
	static ULONGLONG s_prevTgtT = 0;
	static double s_velX = 0, s_velY = 0, s_velZ = 0;
	// insanimsi takip durumu: acisal hizlar (deg/sn) — yay-sonum denetleyici tasir
	static double s_avP = 0.0, s_avY = 0.0;
	if (localValid && g_aimReady == 1)
	{
		const ULONGLONG nowMs = GetTickCount64();
		float adt = s_apLast ? (float)(nowMs - s_apLast) * 0.001f : 0.016f;
		if (adt < 0.001f) adt = 0.001f;
		if (adt > 0.05f) adt = 0.05f;
		s_apLast = nowMs;
		s_apPress += adt;

		// hedef hizi (yumusatilmis). Hedef kimligi degistiyse Onceki hedefin hizi
		// YENI hedefe tasamaz — eski filtre kare basina 300 birim (60fps'te ~180 m/sn!)
		// kabul ediyordu: yakin iki dusman arasinda hedef degisimi devasa sahte hiz
		// uretip lead ile nisani METRELERCE yana atiyordu (saga-sola cekilme buydu).
		static long s_velId = 0;
		if ((long)g_aimTgtId != s_velId)
		{
			s_velId = (long)g_aimTgtId;
			s_velX = s_velY = s_velZ = 0;
			s_prevTgtT = 0;
		}
		if (s_prevTgtT && nowMs > s_prevTgtT)
		{
			const double dts = (double)(nowMs - s_prevTgtT) * 0.001;
			const double jx = (double)g_aimTgtX - s_prevTgt.X, jy = (double)g_aimTgtY - s_prevTgt.Y,
				jz = (double)g_aimTgtZ - s_prevTgt.Z;
			const double jsp = dts > 0.001 ? std::sqrt(jx * jx + jy * jy + jz * jz) / dts : 1e9;
			if (dts > 0.001 && dts < 0.2 && jsp < 900.0) // gercek oyuncu hizi ~9 m/sn siniri
			{
				const bool flip = (jx * s_velX + jy * s_velY) < 0.0 &&
					(std::fabs(s_velX) + std::fabs(s_velY)) > 200.0;
				if (flip)
				{
					// yon degisimi (AD-AD): eski hizi aninda birak — aksi halde lead
					// ters yone itip nisani hedefin otesine savuruyordu (ziplama)
					s_velX = jx / dts; s_velY = jy / dts; s_velZ = jz / dts;
				}
				else
				{
					s_velX = s_velX * 0.75 + (jx / dts) * 0.25;
					s_velY = s_velY * 0.75 + (jy / dts) * 0.25;
					s_velZ = s_velZ * 0.75 + (jz / dts) * 0.25;
				}
			}
			else { s_velX = s_velY = s_velZ = 0; } // hedef degisti / spike — hiz sifirla
		}
		s_prevTgt = FVector((double)g_aimTgtX, (double)g_aimTgtY, (double)g_aimTgtZ);
		s_prevTgtT = nowMs;

		// ondeki nisi: hedefin GERCEK hizi (root velocity) varsa o kullanilir —
		// pozisyon turevli tahmin yalniz yedek. Toplam onleme 30cm ile sinirli.
		const double leadSec = (double)features::aimLead / (double)(features::aimSmooth < 1 ? 1 : features::aimSmooth);
		double lvx = s_velX, lvy = s_velY, lvz = s_velZ; // yedek: pozisyon turevi
		if (g_aimTgtId)
		{
			uint8_t* tgt = (uint8_t*)g_aimTgtId;
			if (!IsBadReadPtr(tgt + offs::player::root_component, 8))
			{
				uint8_t* root = *(uint8_t**)(tgt + offs::player::root_component);
				if (root && !IsBadReadPtr(root + offs::player::root_velocity, 12))
				{
					const FVector tv = *(const FVector*)(root + offs::player::root_velocity);
					if (std::isfinite(tv.X) && std::isfinite(tv.Y) && std::isfinite(tv.Z) &&
						std::fabs(tv.X) < 2000.0 && std::fabs(tv.Y) < 2000.0 && std::fabs(tv.Z) < 2000.0)
					{
						lvx = (double)tv.X; lvy = (double)tv.Y; lvz = (double)tv.Z;
					}
				}
			}
		}
		double lx = lvx * leadSec, ly = lvy * leadSec, lz = lvz * leadSec;
		const double lm = std::sqrt(lx * lx + ly * ly + lz * lz);
		if (lm > 30.0) { const double sc = 30.0 / lm; lx *= sc; ly *= sc; lz *= sc; }
		const double tx = (double)g_aimTgtX + lx;
		const double ty = (double)g_aimTgtY + ly;
		const double tz = (double)g_aimTgtZ + lz;

		FVector eye = LocalLoc;
		eye.Z += 60.f;
		const double adx = tx - eye.X;
		const double ady = ty - eye.Y;
		const double adz = tz - eye.Z;
		const double add = sqrt(adx * adx + ady * ady + adz * adz);
		if (add > 1.0)
		{
			const double DEG = 57.29577951308232;
			double tPitch = asin(adz / add) * DEG;
			const double tYaw = atan2(ady, adx) * DEG;

			double dp = tPitch - CtrlRot.X;   // FVector layout: X=Pitch, Y=Yaw, Z=Roll
			double dyw = tYaw - CtrlRot.Y;
			if (dp > 180.0) dp -= 360.0; else if (dp < -180.0) dp += 360.0;
			if (dyw > 180.0) dyw -= 360.0; else if (dyw < -180.0) dyw += 360.0;

			// ---- insanimsi takip: kritik sondurumlu yay-sonum denetleyici ----
			// Eski tasarim hatayi her karede sabit oranla eritiyordu (exp smoothing):
			// ani hiz degisimleri + hedefe makina gibi dogru acisal hizla oturma.
			// Yeni: acisal HIZ durumu kareler arasi tasinir — ivmelenme/yavaslama
			// sureklidir, hedefe dogal olarak yavaslayarak oturur (insan bilek gibi).
			// "Uyarlanabilir toparlama" kaldirildi: gercek insan buyuk sapmada da
			// aninda hizlanamaz, o snap geri donusler makina gorunumunun kaynagiydi.
			const double om = (std::max)(1.0, (double)features::aimSmooth); // dogal frekans
			const double K = om * om, D = 2.0 * om;                          // kritik sondurum
			float ramp = s_apPress / 0.15f;                                  // baslangic ivme rampi
			if (ramp > 1.f) ramp = 1.f;
			ramp = ramp * ramp * (3.f - 2.f * ramp);
			s_avP += (dp * K * (double)ramp - s_avP * D) * (double)adt;
			s_avY += (dyw * K * (double)ramp - s_avY * D) * (double)adt;
			double stepP = s_avP * (double)adt, stepY = s_avY * (double)adt;

			// insan davranisi: kare basina hiz dalgalanmasi (aimHuman=0 -> kapali)
			if (features::aimHuman > 0.f)
			{
				static unsigned s_rng = 0x9E3779B9u;
				s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
				const float fl = 0.86f + 0.28f * ((s_rng & 0xFFFFFFu) / 16777215.f);
				stepP *= fl; stepY *= fl;
			}

			// asiri gitme engeli (dusuk FPS / buyuk adimta salinim)
			if (std::fabs(stepP) > std::fabs(dp)) stepP = dp;
			if (std::fabs(stepY) > std::fabs(dyw)) stepY = dyw;

			// fovea olu bolgesi: hata 0.30° altina inince duzeltme birakilir — insan
			// gibi mikro hata kalir, hedefte son-piksel kovalamaca titremesi kesilir
			if (std::sqrt(dp * dp + dyw * dyw) < 0.30)
			{
				stepP = stepY = 0.0;
				s_avP *= 0.5; s_avY *= 0.5;
			}

			FVector out{ CtrlRot.X + stepP, CtrlRot.Y + stepY, CtrlRot.Z };
			LocalPlayerController->SetControlRotation(out);
		}
	}
	else
	{
		s_apLast = 0;
		s_apPress = 0.f;
		s_prevTgtT = 0;
		s_velX = s_velY = s_velZ = 0;
		s_avP = s_avY = 0.0;
	}

	// ---- harita adi: Level FName'i coz → g_mapName (radar otomatik harita secimi) ----
	{
		static ULONGLONG s_mapNext = 0;
		const ULONGLONG nowT = GetTickCount64();
		if (nowT > s_mapNext)
		{
			s_mapNext = nowT + 3000; // 3 sn'de bir dene
			uint8_t* lvl = nullptr;
			if (!IsBadReadPtr((uint8_t*)World + offs::core::persistent_level, 8))
				lvl = *(uint8_t**)((uint8_t*)World + offs::core::persistent_level);
			uint8_t* src = (lvl && !IsBadReadPtr(lvl + offs::actor::name_index, 4)) ? lvl : (uint8_t*)World;
			if (!IsBadReadPtr(src + offs::actor::name_index, 4))
			{
				char mbuf[96] = {};
				const int32_t fid = *(const int32_t*)(src + offs::actor::name_index);
				if (GameFNameToString(fid, mbuf, sizeof(mbuf)))
				{
					char* us = strchr(mbuf, '_');
					if (us) *us = 0; // "Ascent_P" → "Ascent"
					std::lock_guard<std::mutex> lock(g_radarMutex);
					if (g_mapName != mbuf)
					{
						g_mapName = mbuf;
						char m[96];
						snprintf(m, sizeof(m), "radar: harita algilandi = '%s'", mbuf);
						RadarDbg(m);
					}
				}
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(g_radarMutex);
		globalData = tempData;

		if (localValid)
		{
			g_localX = LocalLoc.X;
			g_localY = LocalLoc.Y;
			g_localZ = LocalLoc.Z;
			g_localYaw = CtrlRot.Y;
		}
	}

	static bool s_firstPass = false;
	if (!s_firstPass)
	{
		s_firstPass = true;
		RadarDbg("wallhack: ilk tur tamamlandi (PostRender aktif)");
	}
}


auto Postrender(void* View, UCanvas* Canvas) -> void
{
	WallHack(View);
}

// ---- PostRender hook watchdog ----
// Map load / level gecisinde oyun GameViewportClient'i yeniden yaratabiliyor: vtable
// hook'umuz eski objeyle oluyordu -> WallHack cagrilmayi birakiyordu -> web radar
// verisi DONUYORDU ("local oyuncu sabitleniyor" bug'i). Watchdog her 2 sn'de
// LocalPlayer->ViewportClient alanini (saf bellek okuma, ProcessEvent YOK) kontrol
// eder; yeni obje gorurse hook'u yeniden kurar.
static ULocalPlayer* g_localPlayerSlot = nullptr;
static FGameViewportClient* g_hookedView = nullptr;

static DWORD ViewportWatchdog(LPVOID); // ileri bildirim (SetupViewportHook altinda tanimli)

// Viewport/hook kurulumu: DllMain icindeydi — oyun guncellemesinde sabit offset
// zinciri kirilinca 'return 0' yapip DLL'i bosaltiyordu; detach edilmis thread'ler
// unloaded kodda kosup OYUNU CRASH ediyordu. Artik ayri thread + SEH + retry:
// offset kiriksa loglar, crash YOK.
static bool ResolveViewportChain(ULocalPlayer** outLP, FGameViewportClient** outView)
{
	// asama kodlari: 1=p1 null, 2=GWorld gecersiz, 3=GamePlayStatics yok,
	// 4=Instance yok, 5=Local null, 6=LocalPlayer null, 7=View yok, 8=vtable bozuk
	static ULONGLONG s_chainLogNext = 0;
	auto failStage = [](int st) {
		const ULONGLONG now = GetTickCount64();
		if (now > s_chainLogNext)
		{
			s_chainLogNext = now + 10000;
			char m[96];
			snprintf(m, sizeof(m), "hook: viewport zinciri ASAMA %d'de kirildi (10 sn'de bir tekrar loglanir)", st);
			RadarDbg(m);
		}
		return false;
	};
	int stage = 0;
	__try
	{
		static bool s_firstChainLog = true;
		auto p1 = *(DWORD64*)(ImageBase + oWorld);
		if (s_firstChainLog) { char m[96]; snprintf(m, sizeof(m), "hook: ilk deneme — p1=%p (oWorld=0x%X)", (void*)p1, (unsigned)oWorld); RadarDbg(m); }
		if (!p1) return failStage(1);
		stage = 2;
		// dump aracina gore "uworld" ya registry hucre gostericisi (cift deref) ya
		// dogrudan UWorld* (tek deref) — ikisini de dene, zincir kendini dogrular
		// ONCE cift deref: bu adres world_registry_map'tir (eski calisan form) —
		// hücre isaretcisi -> UWorld*. Tek deref yanlislkla "okunabilir" cöp obje
		// kabul edilip ProcessEvent'te AV yiyorduk.
		auto* GWorld = *(UWorld**)(p1);
		if (!GWorld || IsBadReadPtr(GWorld, 0x40))
			GWorld = (UWorld*)(p1);
		if (!GWorld || IsBadReadPtr(GWorld, 0x40)) return failStage(2);
		// GWorld sert dogrulama: vtable[83] (ProcessEvent) cagrilabilir kod olmali
		{
			void** wvt = *(void***)GWorld;
			if (!wvt || IsBadReadPtr(wvt, 8 * 84) || IsBadCodePtr((FARPROC)wvt[83]))
				return failStage(2);
		}
		if (s_firstChainLog) { char m[96]; snprintf(m, sizeof(m), "hook: GWorld=%p ok", (void*)GWorld); RadarDbg(m); }
		stage = 3;
		UGameplayStatics* GamePlayStatics = (UGameplayStatics*)UObject::GetGameplayStatics();
		if (s_firstChainLog) { char m[96]; snprintf(m, sizeof(m), "hook: GamePlayStatics=%p", (void*)GamePlayStatics); RadarDbg(m); s_firstChainLog = false; }
		if (!GamePlayStatics) return failStage(3);
		stage = 4;
		UGameInstance* Instance = GamePlayStatics->GetGameInstance(GWorld);
		if (!Instance) return failStage(4);
		DWORD64 Local = *(DWORD64*)(Instance + 0x40);
		if (!Local) return failStage(5);
		ULocalPlayer* LocalPlayer = *(ULocalPlayer**)(Local);
		if (!LocalPlayer) return failStage(6);
		FGameViewportClient* View = LocalPlayer->ViewportClient;
		if (!View || IsBadReadPtr(View, 8)) return failStage(7);
		// vtable dogrulamasi: yanlis pointer'a hook atmak crash'tir
		void** vt = *(void***)View;
		if (!vt || IsBadReadPtr(vt, 8 * 100)) return failStage(8);
		*outLP = LocalPlayer;
		*outView = View;
		return true;
	}
	__except (1)
	{
		char m[96];
		snprintf(m, sizeof(m), "hook: zincirde AV yakalandi (asama %d sonrasi) — SEH ile kapatildi", stage);
		RadarDbg(m);
		return false;
	}
}

static void SetupViewportHook()
{
	for (int attempt = 0; attempt < 300; attempt++) // ~10 dk retry (oyun acilisi yavas olabilir)
	{
		Sleep(2000);
		ULocalPlayer* lp = nullptr;
		FGameViewportClient* view = nullptr;
		if (!ResolveViewportChain(&lp, &view)) continue;
		if (!view || view == g_hookedView || IsBadReadPtr(view, 8)) continue;
		// PANIC icin orijinal PostRender kaydet (vtable restore'da kullanilacak)
		{
			void*** vt = (void***)view;
			g_origPostRender = (*vt)[99];
			g_hookedViewportForPanic = view;
		}
		Table.Initialize(view);
		Table.Hook(Postrender, 99);
		g_hookedView = view;
		g_localPlayerSlot = lp;
		std::thread(ViewportWatchdog, nullptr).detach();
		RadarDbg("PostRender vtable hook kuruldu + watchdog - web server basliyor");
		web::StartWebServer(31337);
		return;
	}
	RadarDbg("hook: viewport zinciri cozulemedi (10 dk) - GUNCELLEME offsetleri bozmus olabilir, yeni dump gerekli");
}

static DWORD ViewportWatchdog(LPVOID)
{
	for (;;)
	{
		Sleep(2000);
		if (!g_localPlayerSlot || IsBadReadPtr(g_localPlayerSlot, 8)) continue;
		FGameViewportClient* v = g_localPlayerSlot->ViewportClient;
		if (!v || v == g_hookedView || IsBadReadPtr(v, 8)) continue;

		Table.Initialize(v);
		Table.Hook(Postrender, 99);
		g_hookedView = v;
		RadarDbg("watchdog: PostRender hook YENIDEN kuruldu (viewport degismis)");
	}
	return 0;
}

DWORD InitMenuHookThread()
{
	RadarDbg("kiero init basliyor (D3D11)");
	do
	{
		if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
		{
			kiero::Status::Enum s8 = kiero::bind(8, (void**)&oPresent, GUIFunction::hkPresent);
			kiero::Status::Enum s13 = kiero::bind(13, (void**)&oResizeBuffers, GUIFunction::hkResizeBuffers); // ResizeBuffers: fullscreen windowed / resize crash fixi
			RadarDbg(s8 == kiero::Status::Success ? "kiero bind(8) Present OK" : "kiero bind(8) Present HATA");
			RadarDbg(s13 == kiero::Status::Success ? "kiero bind(13) ResizeBuffers OK" : "kiero bind(13) ResizeBuffers HATA");
			Hook_Initalized = true;
		}
	} while (!Hook_Initalized);

	return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		// Ayni surecte ikinci inject'i reddet: cift web server / cift hook = port cakismasi
		// ve unstable davranis (netstat'ta ayni portta 2 listener gorup bad gateway yedik).
		HANDLE hOnce = CreateMutexA(nullptr, TRUE, "Local\\w0nn_valo_once");
		if (!hOnce || GetLastError() == ERROR_ALREADY_EXISTS)
		{
			RadarDbg("DLL bu surecte zaten yuklu - ikinci inject reddedildi");
			return FALSE;
		}
		g_onceMutex = hOnce; // panic'te kapatmak icin global'a kaydet

		RadarDbg("DLL_PROCESS_ATTACH");
		Beep(500, 200);
		ImageBase = (DWORD64)GetModuleHandleA(0);
		RadarDbg("InitMenuHookThread baslatiliyor");
		std::thread(InitMenuHookThread).detach(); // CreateThread cagrisi izlendigi icin std::thread kullaniliyor
		std::thread(SetupViewportHook).detach();  // hook kurulumu: SEH'li, retry'li, loader-lock disinda
		return TRUE; // ASLA return 0 yapma: DLL unload = detach thread'ler unloaded kodda = CRASH
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

/*  UENGINE
	1. 3D pos 3d to 2d
	entity
	{
		health ..
	}
	UEngine GameEngine */


