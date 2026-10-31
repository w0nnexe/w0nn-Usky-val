#pragma once
#include "../imgui/imgui_impl_win32.h"
#include <d3d11.h>
#include <vector>
#include <string>
#include <mutex>
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef HRESULT(__stdcall* Present) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__stdcall* ResizeBuffersFn) (IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

inline Present oPresent = nullptr;
inline ResizeBuffersFn oResizeBuffers = nullptr;
inline HWND window = nullptr;
inline WNDPROC oWndProc = nullptr;
inline ID3D11Device* pDevice = nullptr;
inline ID3D11DeviceContext* pContext = nullptr;
inline ID3D11RenderTargetView* mainRenderTargetView = nullptr;
inline bool ImGUI_Initalized = false;
inline bool Hook_Initalized = false;

// ---- PANIC / UNLOAD ----
// END tusu basildiginda: tum hooklar passthrough, mutex serbest, DLL pasif
inline volatile bool g_panic = false;
inline void* g_origPostRender = nullptr;
inline void* g_hookedViewportForPanic = nullptr;
inline void* g_onceMutex = nullptr; // DllMain'deki mutex handle — panic'te kapatilir

void Render();
void InitalizeImGUI();

inline bool isMenuOpen = false;
inline int MenuKey = VK_F1;
struct playerData {
    // ekran uzayi 2D kutu (kafa/ayak projeksiyonundan — her acidan dogru, ziplarken bozulmaz)
    // boxW <= 0 => oyuncu ekranda gorunmuyor (radar-only kayit; kutu ESP cizilmez)
    float boxX = 0.f;
    float boxY = 0.f;
    float boxW = 0.f;
    float boxH = 0.f;

    uint64_t id = 0; // aktor pointer — aim hedef kimligi (yapisik kilit icin kararli)

    bool ally = false; // true = takim arkadasi (radar yesil), false = dusman (radar kirmizi)
    bool vis = true;   // dusman gorus alaninda mi (LineTrace; aim sadece gorunenleri kilitler)
    bool crouch = false; // egik mi (aim dunya-hedef Z ofseti icin)
    bool hasBomb = false; // minimap bHasBomb (0x690) — bomb tasiyicisi

    float distance;   // metre (local player'a)
    float health;

    float shield = 0.f;    // UAresAttributeSet::Shield (ASC 0x940)

    float worldX; // dunya koordinati (2D map icin)
    float worldY;
    float worldZ;

    // iskelet: kemik noktalari ekran uzayinda (-1 = gecilsiz), head = gercek kafa kemiği
    static constexpr int MAX_BONES = 96;
    float boneX[MAX_BONES];
    float boneY[MAX_BONES];
    int boneCount = 0;
    float headX = -1.f;
    float headY = -1.f;
    float headWX = 0.f; // kafa kemigi dunya konumu (aim icin; headX>=0 iken gecerli)
    float headWY = 0.f;
    float headWZ = 0.f;

    int skeletonPairs[32][2]; // otomatik modda MST kenarlari (geometrik iskelet)
    int skeletonPairCount = 0;

    playerData()
    {
        for (int i = 0; i < MAX_BONES; i++) { boneX[i] = -1.f; boneY[i] = -1.f; }
        memset(skeletonPairs, 0, sizeof(skeletonPairs));
    }
};
inline std::vector<playerData> globalData;

// Web radar icin local player verisi (globalData ile ayni mutex altinda guncellenir)
inline std::mutex g_radarMutex;
inline float g_localX = 0.f;
inline float g_localY = 0.f;
inline float g_localZ = 0.f;
inline float g_localYaw = 0.f; // FRotator.Y (derece)
inline std::string g_mapName;  // aktif level adi (web radar otomatik harita eslestirmesi)

// minimap bileseninden canli cozulen afin (Kasbah/Glitch/Piazza gibi Riotsuz haritalar);
// g_derivedMap bos ise gecerli degil — g_radarMutex altinda yazilir
inline double g_derivedAff[6] = { 0, 0, 0, 0, 0, 0 };
inline std::string g_derivedMap;

// aim rotasyon komutu: Render thread hedefi secer, oyun thread'i (WallHack) uygular —
// SetControlRotation ProcessEvent'i yalnizca oyun thread'inden cagirmak guvenli
inline volatile long g_aimReady = 0;
inline float g_aimTgtX = 0.f, g_aimTgtY = 0.f, g_aimTgtZ = 0.f;
inline volatile long g_aimTgtId = 0; // aktif hedef aktor pointer'i (hedef degisince hiz sifirlanir)

// DLL klasorune radar_debug.log yazar (init asamalari / ilk kare kirintilari)
void RadarDbg(const char* msg);
void RadarPrintf(const char* fmt, ...); // printf gibi ama KONSOLA DEGIL log dosyasina yazar

// kutu ESP tabanli aim assist — her kare (Render) cagrilir
void AimAssistTick();
namespace features
{
	inline bool myFeature1 = false; // kutu ESP
	inline bool myFeature2 = false;

	// gorunur ozellikler (hepsi varsayilan KAPALI, menuden acilir)
	inline bool skeletonESP = false;   // iskelet cizgileri (saf bellek okuma, Engine/Skeleton.h)
	inline bool boneAuto = true;       // geometrik otomatik iskelet (MST) - manuel ciftleri ezer
	inline bool boneDebug = false;     // tum kemikleri indeks etiketiyle cizer (offset bulmak icin)
	inline bool webRadar = true;       // web radar /data oyuncu akisi
	inline bool unlockSkins = false;   // skinchanger: bIsOwned=1 zinciri (saf okuma)

	inline int boneHeadIdx = 8;        // kafa kemiği indeksi (kemik debug'tan dogrulanir)
	inline int boneMax = 80;           // taranacak maksimum kemik sayisi
	inline float boneThick = 2.f;      // kemik nokta capi
	inline int bonePairs[16][2] = {};  // iskelet cizgi ciftleri; {0,0} = bos slot
	inline float enemyColor[4] = { 0.94f, 0.276f, 0.333f, 1.f }; // #FF4655 (kutu ESP rengi)

	// ---- aim assist (kutu ESP verisi tabanli, tus basiliyken) ----
	inline bool aimAssist = false;
	inline bool aimVisFilter = true;   // duvar arkasi (LineTrace gorunmez) hedefleri aday yapma
	inline bool aimAllies = false;     // dost/takim hedefleri de aday yap (poligon/test modu)
	inline int aimMethod = 1;          // 0=SendInput(fare), 1=SetControlRotation(oyun thread'i)
	inline float aimLead = 0.6f;        // hedef hizini ongorme katsayisi (kosan hedefe arkadan takip onler)
	inline float aimHeadZ = 75.f;      // kafa yuksekligi (cm, iskelet yoksa yedek) — pawn merkezinden
	inline int aimKey = VK_XBUTTON2;   // basili tutulacak tus (default: fare yan dugme 2)
	inline int aimPart = 0;            // 0=kafa, 1=gogus, 2=karin
	inline float aimSmooth = 8.f;      // 1=cok yavas/insansi .. 20=cok hizli
	inline float aimFovPx = 250.f;     // merkez etrafinda hedef alinacak yaricap (px)
	inline float aimHuman = 1.2f;      // humanize siddeti (mikro titresim + hiz dalgalanmasi)

	// mesh tarama durumu: 0=araniyor, -1=bulunamadi, baska=offset
	inline int meshOffsetStatus = 0;
	inline int skelEdges = -1;        // son iskelet kaydinin MST kenar sayisi (menu teshisi)

	// aim canli tesHis (HUD/menu) — AimAssistTick her karede doldurur
	inline bool aimDbgHeld = false;
	inline int aimDbgCands = 0;     // ekrandaki dusman kutu sayisi
	inline int aimDbgVis = 0;       // gorunur olanlari
	inline float aimDbgLockX = -1.f;
	inline float aimDbgLockY = -1.f;
	inline float aimDbgLockD = -1.f;
}