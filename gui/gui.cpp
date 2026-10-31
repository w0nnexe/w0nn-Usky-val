#include "gui.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"
#include "../protection/xorstr.hpp"
#include <string>
#include <cmath>

// ============================ AIM ASSIST ============================
// Kutu ESP verisi (globalData) tabanli: PostRender her karede guncel kutu uretir,
// kutu da oyuncunun anlik crouch durumunu icerdigi icin egilince hedef asagi takip
// eder — stale yukari kilitlenme olmaz. Hedef o karede yoksa (oldu/ekrandan cikti)
// hic fare hareketi yapilmaz; bir sonraki gecerli karede yeniden hesaplanir.
//
// Insan davranisi (humanize):
//  - exponential smoothing (FPS bagimsiz, dt ile normalize)
//  - basili tutma baslangic rampi (~0.15s smoothstep — anida snapping yok)
//  - kare basina hiz dalgalanmasi (%86-114)
//  - kucuk mikroskobik titresim (ortalamasi sifir, cap 2.5px)
//  - 2px deadzone (hedefteyken titreme yok)
//  - hedef yapiskanligi: mevcut hedefe yakin adaya %25 avantaj (hedef ziplamasi az)
// tus adini insan okur forma cevir (bind gostergesi icin)
static const char* KeyName(int vk)
{
	static char buf[32];
	if (vk == VK_LBUTTON) return "Sol Tik";
	if (vk == VK_RBUTTON) return "Sag Tik";
	if (vk == VK_MBUTTON) return "Orta Tik";
	if (vk == VK_XBUTTON1) return "Yan Dugme 1";
	if (vk == VK_XBUTTON2) return "Yan Dugme 2";
	if (vk == VK_SHIFT) return "Shift";
	if (vk == VK_CONTROL) return "Ctrl";
	if (vk == VK_MENU) return "Alt";
	if (vk == VK_SPACE) return "Bosluk";
	if (vk >= '0' && vk <= '9') { snprintf(buf, sizeof(buf), "%c", (char)vk); return buf; }
	if (vk >= 'A' && vk <= 'Z') { snprintf(buf, sizeof(buf), "%c", (char)(vk + 32)); return buf; }
	if (vk >= VK_F1 && vk <= VK_F24) { snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1); return buf; }
	snprintf(buf, sizeof(buf), "0x%02X", vk);
	return buf;
}

static unsigned s_rng = 0x9E3779B9u;
static float Rnd01()
{
	s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
	return (s_rng & 0xFFFFFFu) / 16777215.f;
}

static float s_aimDist = -1.f;   // menu gostergesi: son hedef uzakligi (px)
static int s_aimCands = 0;       // bu kare aday dusman sayisi (ekranda)
static int s_aimVis = 0;         // gorunur olanlari
static int s_sendFail = 0;       // SendInput hatasi sayaci
static ULONGLONG s_aimLast = 0;
static float s_aimPressT = 0.f;
static float s_aimAccX = 0.f, s_aimAccY = 0.f;
static float s_aimLastX = -1.f, s_aimLastY = -1.f; // yapiskanlik icin son hedef

void AimAssistTick()
{
	if (g_panic) return; // panic: hicbir sey yapma
	if (!features::aimAssist) return;
	if (isMenuOpen) { s_aimPressT = 0.f; s_aimLast = 0; return; } // menu acikken calismaz

	bool held = (GetAsyncKeyState(features::aimKey) & 0x8000) != 0;
	features::aimDbgHeld = held;
	if (!held)
	{
		s_aimPressT = 0.f;
		s_aimLast = 0;
		s_aimLastX = -1.f; s_aimLastY = -1.f;
		s_aimDist = -1.f;
		features::aimDbgLockX = -1.f; features::aimDbgLockY = -1.f; features::aimDbgLockD = -1.f;
		features::aimDbgCands = 0; features::aimDbgVis = 0;
		InterlockedExchange(&g_aimReady, 0);
		InterlockedExchange(&g_aimTgtId, 0);
		return;
	}

	// dt (FPS'ten bagimsiz hiz icin); hitch sonrasi siçrama engeli
	ULONGLONG now = GetTickCount64();
	float dt = s_aimLast ? (float)(now - s_aimLast) * 0.001f : 0.016f;
	if (dt < 0.001f) dt = 0.001f;
	if (dt > 0.05f) dt = 0.05f;
	s_aimLast = now;
	s_aimPressT += dt;

	// merkez: OYUN PENCERESININ gercek client alani (DPI olcegi GetSystemMetrics'i bozabilir;
	// W2S koordinatlari oyunun gercek cozunurlugunde uretiliyor, merkez de oyle olmali)
	RECT rc = {};
	if (window && GetClientRect(window, &rc) && rc.right > 0 && rc.bottom > 0) { }
	else { rc.right = GetSystemMetrics(SM_CXSCREEN); rc.bottom = GetSystemMetrics(SM_CYSCREEN); }
	const float cx = (rc.right - rc.left) * 0.5f;
	const float cy = (rc.bottom - rc.top) * 0.5f;

	// hedef secimi: FOV cemberi icindeki adaylar arasindan DUNYADA EN YAKIN (metre) olan.
	// Merkez yakınlığı yalnız eşitlikte belirleyici. Kilit KIMLIK tabanli yapiskan:
	// baska aday cok daha yakin (%45+) olmadikca ya da 6 kare ustuste hafif-iyi olmadikca
	// hedef degismez -> arka arkaya peak atan adamlar arasinda ziplama/asagi çekilme yok.
	float bestX = -1.f, bestY = -1.f, bestSd = 0.f;
	float bestWX = 0.f, bestWY = 0.f, bestWZ = 0.f;
	uint64_t bestId = 0;
	static uint64_t s_aimLastId = 0;
	static int s_switchCnt = 0;
	static int s_allBlocked = 0;     // ardisik "hepsi gorunmez" kareleri
	static bool s_visBroken = false; // trace bozuksa filtre bypass
	{
		float curScore = 1e9f, curX = -1.f, curY = -1.f, curSd = 0.f, curWX = 0.f, curWY = 0.f, curWZ = 0.f;
		float othScore = 1e9f, oX = -1.f, oY = -1.f, oSd = 0.f, oWX = 0.f, oWY = 0.f, oWZ = 0.f;
		uint64_t oId = 0;

		std::lock_guard<std::mutex> lock(g_radarMutex);
		for (const auto& p : globalData)
		{
			if ((p.ally && !features::aimAllies) || p.boxW <= 1.f) continue; // takim + ekranda olmayanlar
			if (features::aimVisFilter && !s_visBroken && !p.vis) continue; // duvar arkasi

			// dublieri ele: ayni oyuncunun cift pawn kaydi (100cm icinde) tek aday sayilir —
			// ust uste binen kayitlar hedef ziplatip aim'i saga-sola cekiyordu
			if (p.id != s_aimLastId)
			{
				bool dup = false;
				if (curX >= 0.f && std::fabs(p.worldX - curWX) < 100.f && std::fabs(p.worldY - curWY) < 100.f) dup = true;
				else if (oX >= 0.f && std::fabs(p.worldX - oWX) < 100.f && std::fabs(p.worldY - oWY) < 100.f) dup = true;
				if (dup) continue;
			}

			float px = p.boxX + p.boxW * 0.5f;
			float py = p.boxY + p.boxH * (features::aimPart == 1 ? 0.28f : features::aimPart == 2 ? 0.45f : 0.10f);

			// kafa kemigi CIFT sanity ister: (1) dunya konumu aktore yakin (60cm yatay —
			// eski 200cm toleransi silah/sirt kemiklerini geciriyordu), (2) ekrandaki
			// nokta oyuncunun kutusuna dusuyor (stale kemik dizisi / yanlis kemik elenir).
			// Ikisi de gecerse hem ekran hem dunya hedefi kemikten; yoksa kutu yedegi.
			bool headOk = false;
			if (features::aimPart == 0 && p.headX >= 0.f)
			{
				const bool wOk =
					std::fabs(p.headWX - p.worldX) < 60.f && std::fabs(p.headWY - p.worldY) < 60.f &&
					p.headWZ > p.worldZ + 30.f && p.headWZ < p.worldZ + 140.f;
				const bool sOk =
					p.headX > p.boxX - p.boxW * 0.5f && p.headX < p.boxX + p.boxW * 1.5f &&
					p.headY > p.boxY - p.boxH * 0.15f && p.headY < p.boxY + p.boxH * 0.35f;
				headOk = wOk && sOk;
				if (headOk) { px = p.headX; py = p.headY; }
			}
			const float sd = std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
			if (sd > features::aimFovPx) continue; // odak (FOV) disi

			// dunya hedefi: kafa kemigi yalniz headOk iken (ekran + dunya sanity birlikte gecti)
			float wx = p.worldX, wy = p.worldY, wz;
			if (headOk)
			{
				wx = p.headWX; wy = p.headWY; wz = p.headWZ;
			}
			else
			{
				wz = p.worldZ + (features::aimPart == 0
					? features::aimHeadZ * (p.crouch ? 0.6f : 1.f)
					: features::aimPart == 1 ? (p.crouch ? 25.f : 45.f)
					: (p.crouch ? 0.f : 15.f));
			}

			const float meters = p.distance > 0.f ? p.distance : sd * 0.05f;
			const float score = meters + sd * 0.001f; // yakinlik birincil, merkez yalniz esitlikte

			if (p.id == s_aimLastId)
			{
				if (score < curScore) { curScore = score; curX = px; curY = py; curSd = sd; curWX = wx; curWY = wy; curWZ = wz; }
			}
			else if (score < othScore) { othScore = score; oX = px; oY = py; oSd = sd; oWX = wx; oWY = wy; oWZ = wz; oId = p.id; }
		}

		bool useOther = false;
		if (curX >= 0.f && oX >= 0.f)
		{
			if (othScore < curScore * 0.55f) useOther = true;                        // cok daha yakin: hemen gec
			else if (othScore < curScore && ++s_switchCnt >= 6) useOther = true;     // hafif iyi: 6 kare bekle
			else s_switchCnt = 0;
		}
		else s_switchCnt = 0;

		if (useOther)
		{
			bestX = oX; bestY = oY; bestSd = oSd; bestWX = oWX; bestWY = oWY; bestWZ = oWZ; bestId = oId;
			s_switchCnt = 0;
		}
		else if (curX >= 0.f)
		{
			bestX = curX; bestY = curY; bestSd = curSd; bestWX = curWX; bestWY = curWY; bestWZ = curWZ; bestId = s_aimLastId;
		}
		else if (oX >= 0.f)
		{
			bestX = oX; bestY = oY; bestSd = oSd; bestWX = oWX; bestWY = oWY; bestWZ = oWZ; bestId = oId;
		}
	}

	// gorunurluk saglik kontrolu: aday var ama HICBIRI gorunmuyorsa trace bozuk olabilir.
	// (bozuk LineTrace layout'u her izi 'carpti' dondurur -> aim hic hedef bulamaz).
	// 3 saniye boyunca hep boyleyse filtre bypass edilir ve radar_debug'a not duser.
	{
		std::lock_guard<std::mutex> lock(g_radarMutex);
		s_aimCands = 0; s_aimVis = 0;
		for (const auto& p : globalData)
			if (!p.ally && p.boxW > 1.f) { s_aimCands++; if (p.vis) s_aimVis++; }
	}
	features::aimDbgCands = s_aimCands;
	features::aimDbgVis = s_aimVis;
	if (s_aimCands > 0 && s_aimVis == 0)
	{
		if (++s_allBlocked > 180) // ~3sn @60fps — surekli "hepsi gorunmez" = trace supheli
		{
			if (!s_visBroken) RadarDbg("aim: visibility trace supheli — filtre bypass edildi");
			s_visBroken = true;
		}
	}
	else
	{
		s_allBlocked = 0;
		// tek gorunur aday bile trace canli oldugunu kanitlar: latch'i kaldir.
		// eski kod s_visBroken'u hic geri acmiyordu — duvar arkasi ~1.5sn gecirmek
		// filtreyi OTURUM BOYU iptal ediyor, aim gorunmeyen dusmanlara kilitleniyordu.
		if (s_visBroken && s_aimVis > 0)
		{
			s_visBroken = false;
			RadarDbg("aim: visibility trace geri geldi — filtre yeniden aktif");
		}
	}

	if (bestX < 0.f)
	{
		// bu karede gecerli hedef yok: STALE konuma fare oynatMA — bekle
		s_aimLastX = -1.f; s_aimLastY = -1.f; s_aimDist = -1.f;
		s_aimLastId = 0;
		s_switchCnt = 0;
		features::aimDbgLockX = -1.f; features::aimDbgLockY = -1.f; features::aimDbgLockD = -1.f;
		InterlockedExchange(&g_aimReady, 0);
		InterlockedExchange(&g_aimTgtId, 0);
		return;
	}
	s_aimLastX = bestX; s_aimLastY = bestY;
	s_aimDist = bestSd;
	features::aimDbgLockX = bestX;
	features::aimDbgLockY = bestY;
	features::aimDbgLockD = bestSd;

	if (features::aimMethod == 1)
	{
		// Pozisyon on-yumusatma KALDIRILDI: rotasyon tarafinda zaten yay-sonum
		// denetleyici var; ikinci yumuşatma faz gecikmesi uretip lead ile cakisiyor,
		// hedef ziplamalari yaratıyordu. Ham dunya hedefi dogrudan gider.
		g_aimTgtX = bestWX;
		g_aimTgtY = bestWY;
		g_aimTgtZ = bestWZ;
		InterlockedExchange(&g_aimTgtId, (long)bestId);
		InterlockedExchange(&g_aimReady, 1);
		return;
	}

	const float tdx = bestX - cx, tdy = bestY - cy;
	if (std::sqrt(tdx * tdx + tdy * tdy) < 2.f) return; // deadzone

	// basma rampi: insan reaksiyonu (~0.15s), smoothstep
	float ramp = s_aimPressT / 0.15f;
	if (ramp > 1.f) ramp = 1.f;
	ramp = ramp * ramp * (3.f - 2.f * ramp);

	// exponential smoothing + hiz dalgalanmasi
	float frac = (1.f - std::exp(-features::aimSmooth * dt)) * ramp;
	frac *= 0.86f + 0.28f * Rnd01();

	float dx = tdx * frac, dy = tdy * frac;

	// mikroskobik titresim (kalibrasyonsuz jitter — ortalamasi 0)
	if (features::aimHuman > 0.f)
	{
		float amp = features::aimHuman * (0.4f + bestSd * 0.004f);
		if (amp > 2.5f) amp = 2.5f;
		dx += (Rnd01() * 2.f - 1.f) * amp;
		dy += (Rnd01() * 2.f - 1.f) * amp * 0.8f;
	}

	// piksel artigi biriketir (yavas smooth + yuksek FPS'te kayip olmasin)
	s_aimAccX += dx; s_aimAccY += dy;
	int sx = (int)s_aimAccX, sy = (int)s_aimAccY;
	if (sx != 0 || sy != 0)
	{
		INPUT in = {};
		in.type = INPUT_MOUSE;
		in.mi.dwFlags = MOUSEEVENTF_MOVE;
		in.mi.dx = sx; in.mi.dy = sy;
		if (SendInput(1, &in, sizeof(INPUT)) != 1) s_sendFail++;
		s_aimAccX -= sx; s_aimAccY -= sy;
	}
}
// ====================================================================

static void ApplyW0nnStyle()
{
	ImGui::StyleColorsDark();
	auto& s = ImGui::GetStyle();
	s.WindowRounding = 10.f;
	s.FrameRounding = 6.f;
	s.GrabRounding = 6.f;
	s.PopupRounding = 8.f;
	s.ScrollbarRounding = 6.f;
	s.ScrollbarSize = 10.f;
	s.WindowPadding = ImVec2(14, 14);
	s.FramePadding = ImVec2(10, 6);
	s.ItemSpacing = ImVec2(8, 8);
	s.WindowBorderSize = 1.f;
	s.FrameBorderSize = 0.f;
	s.GrabMinSize = 9.f;

	const ImVec4 accent = ImVec4(0.94f, 0.276f, 0.333f, 1.f);   // #FF4655
	const ImVec4 bg = ImVec4(0.066f, 0.075f, 0.094f, 0.995f);  // #101318
	const ImVec4 bg2 = ImVec4(0.105f, 0.118f, 0.145f, 1.f);    // frame
	const ImVec4 bg3 = ImVec4(0.145f, 0.160f, 0.195f, 1.f);    // hover

	ImVec4* c = s.Colors;
	c[ImGuiCol_WindowBg] = bg;
	c[ImGuiCol_PopupBg] = ImVec4(0.075f, 0.085f, 0.105f, 0.985f);
	c[ImGuiCol_Border] = ImVec4(0.17f, 0.19f, 0.23f, 0.70f);
	c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = bg;
	c[ImGuiCol_FrameBg] = bg2;
	c[ImGuiCol_FrameBgHovered] = bg3;
	c[ImGuiCol_FrameBgActive] = bg3;
	c[ImGuiCol_Header] = bg2;
	c[ImGuiCol_HeaderHovered] = bg3;
	c[ImGuiCol_HeaderActive] = accent;
	c[ImGuiCol_CheckMark] = accent;
	c[ImGuiCol_SliderGrab] = accent;
	c[ImGuiCol_SliderGrabActive] = accent;
	c[ImGuiCol_Button] = bg2;
	c[ImGuiCol_ButtonHovered] = bg3;
	c[ImGuiCol_ButtonActive] = accent;
	c[ImGuiCol_Separator] = ImVec4(0.20f, 0.22f, 0.27f, 0.60f);
	c[ImGuiCol_ScrollbarBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.16f, 0.18f, 0.22f, 1.f);
	c[ImGuiCol_ScrollbarGrabHovered] = bg3;
	c[ImGuiCol_ScrollbarGrabActive] = accent;
	c[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.95f, 1.f);
	c[ImGuiCol_TextDisabled] = ImVec4(0.47f, 0.50f, 0.56f, 1.f);
}

void InitalizeImGUI() {
	ImGui::CreateContext();
	auto io = ImGui::GetIO();
	auto& style = ImGui::GetStyle();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;
	style.WindowMinSize = ImVec2(256, 300);
	ApplyW0nnStyle();
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);
}

void UpdateKeys() {
	// Menu toggle artik SADECE WndProc'taki WM_KEYUP (F1) ile yapiliyor.
	// Burada GetAsyncKeyState(MenuKey) oylamasi vardi — WndProc ile birlikte
	// ayni tus icin cift toggle yapip menunun acilip kapanmasina neden oluyordu.

	// PANIC: END tusu — tum ozellikleri aninda kes, hooklari passthrough yap
	if (GetAsyncKeyState(VK_END) & 1)
	{
		if (!g_panic)
		{
			g_panic = true;
			isMenuOpen = false;
			// hkPresent icinde vtable restore + mutex serbest birakma yapilacak
		}
	}

	for (int i = 0; i < 5; i++) {
		ImGui::GetIO().MouseDown[i] = false;
	}
	int Button = -1;
	if (GetAsyncKeyState(VK_LBUTTON)) {
		Button = 0;
	}
	if (Button != -1) {
		ImGui::GetIO().MouseDown[Button] = true;
	}
}

// ---- w0nn menu widget'lari (Valorant temali) ----
static void WSection(const char* title)
{
	ImGui::Dummy(ImVec2(0, 3));
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImVec2 ts = ImGui::CalcTextSize(title);
	dl->AddRectFilled(p, ImVec2(p.x + 3.f, p.y + ts.y), IM_COL32(255, 70, 85, 255), 1.f);
	dl->AddText(ImVec2(p.x + 11.f, p.y), IM_COL32(228, 231, 236, 255), title);
	dl->AddLine(ImVec2(p.x + 16.f + ts.x, p.y + ts.y * 0.6f),
		ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + ts.y * 0.6f), IM_COL32(255, 255, 255, 22));
	ImGui::Dummy(ImVec2(0, ts.y + 8));
}

static bool WToggleRow(const char* label, const char* desc, bool* v)
{
	const float tw = 42.f, th = 21.f, rowH = desc ? 36.f : 26.f;
	ImVec2 p = ImGui::GetCursorScreenPos();
	const float w = ImGui::GetContentRegionAvail().x;
	ImGui::PushID(label);
	const bool pressed = ImGui::InvisibleButton("##tgl", ImVec2(w, rowH));
	ImGui::PopID();
	if (pressed) *v = !*v;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	if (ImGui::IsItemHovered())
		dl->AddRectFilled(p, ImVec2(p.x + w, p.y + rowH), IM_COL32(255, 255, 255, 8), 5.f);
	dl->AddText(ImVec2(p.x + 2, p.y + (desc ? 3.f : 6.f)), IM_COL32(228, 231, 236, 255), label);
	if (desc)
		dl->AddText(ImVec2(p.x + 2, p.y + 18.f), IM_COL32(122, 130, 144, 255), desc);
	const float py = p.y + (rowH - th) * 0.5f, px = p.x + w - tw;
	dl->AddRectFilled(ImVec2(px, py), ImVec2(px + tw, py + th),
		*v ? IM_COL32(255, 70, 85, 235) : IM_COL32(44, 49, 59, 255), th * 0.5f);
	if (!*v)
		dl->AddRect(ImVec2(px, py), ImVec2(px + tw, py + th), IM_COL32(255, 255, 255, 25), th * 0.5f);
	const float r = th * 0.5f - 3.f;
	const float kx = *v ? px + tw - r - 3.f : px + r + 3.f;
	dl->AddCircleFilled(ImVec2(kx, py + th * 0.5f), r, IM_COL32(246, 247, 249, 255));
	return pressed;
}

static void WSlider(const char* label, float* v, float mn, float mx, const char* fmt)
{
	ImGui::TextDisabled("%s", label);
	ImGui::PushID(label);
	ImGui::PushItemWidth(-1);
	ImGui::SliderFloat("##s", v, mn, mx, fmt);
	ImGui::PopItemWidth();
	ImGui::PopID();
}

static void WCombo(const char* label, int* v, const char* items)
{
	ImGui::TextDisabled("%s", label);
	ImGui::PushID(label);
	ImGui::PushItemWidth(-1);
	ImGui::Combo("##c", v, items);
	ImGui::PopItemWidth();
	ImGui::PopID();
}

static void WChip(const char* txt, bool accent = false)
{
	ImVec2 p = ImGui::GetCursorScreenPos();
	ImVec2 ts = ImGui::CalcTextSize(txt);
	const float h = ts.y + 10.f, pad = 9.f;
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p, ImVec2(p.x + ts.x + pad * 2.f, p.y + h),
		accent ? IM_COL32(255, 70, 85, 40) : IM_COL32(255, 255, 255, 12), h * 0.5f);
	dl->AddText(ImVec2(p.x + pad, p.y + 5.f),
		accent ? IM_COL32(255, 128, 138, 255) : IM_COL32(162, 169, 180, 255), txt);
	ImGui::Dummy(ImVec2(ts.x + pad * 2.f, h));
}

static void WStatRow(const char* label, const char* value, bool accent = false)
{
	ImVec2 p = ImGui::GetCursorScreenPos();
	const float w = ImGui::GetContentRegionAvail().x;
	ImVec2 vs = ImGui::CalcTextSize(value);
	ImGui::Dummy(ImVec2(w, vs.y + 7.f));
	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddText(ImVec2(p.x + 2, p.y + 3), IM_COL32(140, 147, 158, 255), label);
	dl->AddText(ImVec2(p.x + w - vs.x, p.y + 3),
		accent ? IM_COL32(255, 118, 128, 255) : IM_COL32(228, 231, 236, 255), value);
}

static void DrawMenu()
{
	ImGui::SetNextWindowSize(ImVec2(600, 548), ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin(_("w0nn"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 wp = ImGui::GetWindowPos();
	const ImVec2 ws = ImGui::GetWindowSize();
	const float W = ws.x, H = ws.y;
	const float headH = 56.f, footH = 26.f, sbw = 132.f;

	// ---- ust bant: logo + chip'ler ----
	dl->AddRectFilled(wp, ImVec2(wp.x + W, wp.y + headH), IM_COL32(17, 19, 24, 255), 10.f, ImDrawFlags_RoundCornersTop);
	dl->AddRectFilled(ImVec2(wp.x + 12, wp.y + headH - 2), ImVec2(wp.x + W - 12, wp.y + headH), IM_COL32(255, 70, 85, 255));
	{
		ImGui::PushFont(NULL, ImGui::GetStyle().FontSizeBase * 1.55f);
		const ImVec2 ls = ImGui::CalcTextSize("W0NN");
		dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
			ImVec2(wp.x + 16, wp.y + (headH - ls.y) * 0.5f - 1), IM_COL32(245, 246, 248, 255), "W0NN");
		ImGui::PopFont();
		dl->AddText(ImVec2(wp.x + 24 + ls.x, wp.y + headH * 0.5f - 9), IM_COL32(255, 70, 85, 255), "//");
		dl->AddText(ImVec2(wp.x + 46 + ls.x, wp.y + headH * 0.5f - 9), IM_COL32(140, 147, 158, 255), "VALORANT");
	}
	char fpsTxt[24];
	snprintf(fpsTxt, sizeof(fpsTxt), "%.0f FPS", ImGui::GetIO().Framerate);
	const float chipF1w = ImGui::CalcTextSize("F1").x + 18.f;
	const float chipFw = ImGui::CalcTextSize(fpsTxt).x + 18.f;
	ImGui::SetCursorScreenPos(ImVec2(wp.x + W - 14 - chipF1w - 6 - chipFw, wp.y + (headH - 23.f) * 0.5f));
	WChip("F1");
	ImGui::SetCursorScreenPos(ImVec2(wp.x + W - 14 - chipFw, wp.y + (headH - 23.f) * 0.5f));
	WChip(fpsTxt);

	// ---- sol sekme cubugu ----
	dl->AddRectFilled(ImVec2(wp.x, wp.y + headH), ImVec2(wp.x + sbw, wp.y + H - footH),
		IM_COL32(13, 15, 19, 255), 10.f, ImDrawFlags_RoundCornersBottomLeft);
	dl->AddLine(ImVec2(wp.x + sbw, wp.y + headH), ImVec2(wp.x + sbw, wp.y + H), IM_COL32(40, 45, 54, 255));

	static int s_tab = 0;
	const char* tabNames[4] = { "GORSEL", "AIM", "RADAR", "DURUM" };
	for (int i = 0; i < 4; i++)
	{
		const ImVec2 tp(wp.x + 10, wp.y + headH + 14 + (float)i * 40.f);
		ImGui::SetCursorScreenPos(tp);
		ImGui::PushID(i);
		if (ImGui::InvisibleButton("##tab", ImVec2(sbw - 20, 32)))
			s_tab = i;
		ImGui::PopID();
		const bool hov = ImGui::IsItemHovered();
		if (s_tab == i)
		{
			dl->AddRectFilled(tp, ImVec2(tp.x + sbw - 20, tp.y + 32), IM_COL32(255, 70, 85, 32), 6.f);
			dl->AddRectFilled(ImVec2(tp.x, tp.y + 6), ImVec2(tp.x + 3, tp.y + 26), IM_COL32(255, 70, 85, 255), 1.5f);
		}
		else if (hov)
			dl->AddRectFilled(tp, ImVec2(tp.x + sbw - 20, tp.y + 32), IM_COL32(255, 255, 255, 10), 6.f);
		const ImVec2 ts = ImGui::CalcTextSize(tabNames[i]);
		dl->AddText(ImVec2(tp.x + (sbw - 20 - ts.x) * 0.5f, tp.y + (32 - ts.y) * 0.5f),
			s_tab == i ? IM_COL32(245, 246, 248, 255) : (hov ? IM_COL32(190, 196, 205, 255) : IM_COL32(130, 137, 148, 255)),
			tabNames[i]);
	}

	// ---- alt bant ----
	dl->AddRectFilled(ImVec2(wp.x, wp.y + H - footH), ImVec2(wp.x + W, wp.y + H),
		IM_COL32(17, 19, 24, 255), 10.f, ImDrawFlags_RoundCornersBottom);
	dl->AddText(ImVec2(wp.x + 14, wp.y + H - footH + 6), IM_COL32(110, 117, 128, 255), "w0nn // valorant");
	{
		const char* fr = "F1 - kapat";
		const ImVec2 fs = ImGui::CalcTextSize(fr);
		dl->AddText(ImVec2(wp.x + W - 14 - fs.x, wp.y + H - footH + 6), IM_COL32(110, 117, 128, 255), fr);
	}

	// ---- icerik ----
	ImGui::SetCursorScreenPos(ImVec2(wp.x + sbw + 16, wp.y + headH + 10));
	ImGui::BeginChild("##content", ImVec2(W - sbw - 30, H - headH - footH - 20));

	if (s_tab == 0) // ---------------- GORSEL ----------------
	{
		WSection("KUTU ESP");
		WToggleRow("Kutu ESP", "2D kutu + can bari + mesafe etiketi", &features::myFeature1);
		WToggleRow("Skin kilidi ac", "koleksiyondaki skinler secilebilir — lobide ac", &features::unlockSkins);
		ImGui::TextDisabled("Kutu rengi");
		ImGui::SameLine();
		ImGui::PushItemWidth(150);
		ImGui::ColorEdit4("##boxcol", features::enemyColor,
			ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		ImGui::PopItemWidth();

		WSection("ONIZLEME");
		{
			ImDrawList* cdl = ImGui::GetWindowDrawList();
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const float bw = 54.f, bh = 82.f;
			const ImVec2 bp(p.x + 30, p.y + 10);
			const float* ec = features::enemyColor;
			const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(ec[0], ec[1], ec[2], 1.f));
			const char* wt = "VANDAL";
			const ImVec2 wts = ImGui::CalcTextSize(wt);
			cdl->AddText(ImVec2(bp.x + bw * 0.5f - wts.x * 0.5f, bp.y - 18), IM_COL32(255, 220, 150, 235), wt);
			cdl->AddRectFilled(bp, ImVec2(bp.x + bw, bp.y + bh),
				ImGui::ColorConvertFloat4ToU32(ImVec4(ec[0], ec[1], ec[2], 0.14f)));
			cdl->AddRect(bp, ImVec2(bp.x + bw, bp.y + bh), col, 0.f, 0, 1.4f);
			cdl->AddRectFilled(ImVec2(bp.x - 6, bp.y), ImVec2(bp.x - 3, bp.y + bh), IM_COL32(0, 0, 0, 170));
			cdl->AddRectFilled(ImVec2(bp.x - 6, bp.y + bh * 0.3f), ImVec2(bp.x - 3, bp.y + bh), IM_COL32(80, 220, 110, 255));
			const char* dt = "23m";
			const ImVec2 dts = ImGui::CalcTextSize(dt);
			cdl->AddText(ImVec2(bp.x + bw * 0.5f - dts.x * 0.5f, bp.y + bh + 4), IM_COL32(240, 240, 240, 230), dt);
			ImGui::Dummy(ImVec2(bw + 60, bh + 28));
		}
	}
	else if (s_tab == 1) // ---------------- AIM ----------------
	{
		WSection("GENEL");
		WToggleRow("Aim assist", "tusu basili tuttugun surece calisir", &features::aimAssist);
		WToggleRow("Gorus filtresi", "duvar arkasindaki dusmanlar aday sayilmaz", &features::aimVisFilter);

		WSection("NISAN");
		WCombo("Hareket yontemi", &features::aimMethod, "Rotasyon (oyun ici)\0Fare (SendInput)\0");
		if (features::aimMethod == 0)
			ImGui::TextDisabled("SendInput bu oyunda yok sayiliyorsa Rotasyon'u sec");
		WCombo("Hitbox", &features::aimPart, "Kafa\0Gogus\0Karin\0");
		{
			static bool s_listen = false;
			static bool s_armed = false;
			ImGui::TextDisabled("Aim tusu");
			char lbl[48];
			if (s_listen) snprintf(lbl, sizeof(lbl), "BIR TUSA BAS  (ESC iptal)");
			else snprintf(lbl, sizeof(lbl), "%s", KeyName(features::aimKey));
			if (s_listen)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.94f, 0.276f, 0.333f, 1.f));
			if (ImGui::Button(lbl, ImVec2(-1, 0)))
				s_listen = !s_listen, s_armed = false;
			if (s_listen)
				ImGui::PopStyleColor();
			if (s_listen)
			{
				if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
				{
					s_listen = false; s_armed = false;
				}
				else
				{
					int hit = 0; bool anyDown = false;
					for (int vk = 0x01; vk <= 0x06; ++vk) if (GetAsyncKeyState(vk) & 0x8000) { if (!hit) hit = vk; anyDown = true; }
					for (int vk = 0x10; vk <= 0x12; ++vk) if (GetAsyncKeyState(vk) & 0x8000) { if (!hit) hit = vk; anyDown = true; }
					for (int vk = 0x20; vk <= 0x39; ++vk) if (GetAsyncKeyState(vk) & 0x8000) { if (!hit) hit = vk; anyDown = true; }
					for (int vk = 0x41; vk <= 0x5A; ++vk) if (GetAsyncKeyState(vk) & 0x8000) { if (!hit) hit = vk; anyDown = true; }
					for (int vk = 0x70; vk <= 0x87; ++vk) if (GetAsyncKeyState(vk) & 0x8000) { if (!hit) hit = vk; anyDown = true; }
					if (!s_armed) { if (!anyDown) s_armed = true; } // tikin kendisi atanmasin: once birak
					else if (hit) { features::aimKey = hit; s_listen = false; s_armed = false; }
				}
			}
			ImGui::TextDisabled("atamak icin tikla, sonra diledigin tusa bas");
		}

		WSection("AYARLAR");
		WSlider("Hiz (smooth)", &features::aimSmooth, 1.f, 20.f, "%.0f");
		WSlider("FOV yaricapi (px)", &features::aimFovPx, 5.f, 800.f, "%.0f");
		WSlider("Kafa yuksekligi (cm)", &features::aimHeadZ, 50.f, 110.f, "%.0f");
		WSlider("Ondeme (kosan hedef)", &features::aimLead, 0.f, 2.f, "%.1f");
		WSlider("Insan davranisi", &features::aimHuman, 0.f, 3.f, "%.1f");

		WSection("CANLI DURUM");
		{
			char b1[32], b2[32], b3[32];
			snprintf(b1, sizeof(b1), "aday %d", features::aimDbgCands);
			snprintf(b2, sizeof(b2), "gorunur %d", features::aimDbgVis);
			WChip(b1); ImGui::SameLine(0, 6); WChip(b2);
			if (s_aimDist >= 0.f)
			{
				snprintf(b3, sizeof(b3), "kilit %.0f px", s_aimDist);
				ImGui::SameLine(0, 6); WChip(b3, true);
			}
			if (s_sendFail > 0)
				ImGui::TextDisabled("SendInput basarisiz: %d", s_sendFail);
		}
	}
	else if (s_tab == 2) // ---------------- RADAR ----------------
	{
		WSection("WEB RADAR");
		WToggleRow("Yayin acik (/data)", "web radar sayfasi canli konumlari ceker", &features::webRadar);
		WSection("DURUM");
		{
			int nAl = 0, nEn = 0;
			std::string mapName;
			{
				std::lock_guard<std::mutex> lock(g_radarMutex);
				mapName = g_mapName;
				for (const auto& p : globalData) p.ally ? nAl++ : nEn++;
			}
			char buf[64];
			snprintf(buf, sizeof(buf), "%d / %d", nEn, nAl);
			WStatRow("Dusman / Takim", buf);
			WStatRow("Harita", mapName.empty() ? "-" : mapName.c_str());
			WStatRow("Veri", features::webRadar ? "yayinda" : "kapali", features::webRadar);
		}
		ImGui::Dummy(ImVec2(0, 4));
		ImGui::TextDisabled("URL: konsol ciktisi ve radar_debug.log icinde");
	}
	else // ---------------- DURUM ----------------
	{
		WSection("OYUN");
		{
			int nAl = 0, nEn = 0;
			std::string mapName;
			{
				std::lock_guard<std::mutex> lock(g_radarMutex);
				mapName = g_mapName;
				for (const auto& p : globalData) p.ally ? nAl++ : nEn++;
			}
			char buf[64];
			snprintf(buf, sizeof(buf), "%d dusman / %d takim", nEn, nAl);
			WStatRow("Oyuncular", buf);
			WStatRow("Harita", mapName.empty() ? "-" : mapName.c_str());
			snprintf(buf, sizeof(buf), "%.0f FPS", ImGui::GetIO().Framerate);
			WStatRow("Kare hizi", buf);
		}
		WSection("AIM");
		{
			char buf[48];
			const bool down = (GetAsyncKeyState(features::aimKey) & 0x8000) != 0;
			snprintf(buf, sizeof(buf), "%s", KeyName(features::aimKey));
			WStatRow("Tus", buf);
			WStatRow("Tus durumu", down ? "BASILI (algilandi)" : "basili degil", down);
			snprintf(buf, sizeof(buf), "%d / %d", features::aimDbgCands, features::aimDbgVis);
			WStatRow("Aday / Gorunur", buf);
			if (s_aimDist >= 0.f) snprintf(buf, sizeof(buf), "%.0f px", s_aimDist);
			else snprintf(buf, sizeof(buf), "-");
			WStatRow("Kilit mesafesi", buf, s_aimDist >= 0.f);
		}
		WSection("KISAYOL");
		WStatRow("Menu", "F1");
		WStatRow("PANIC (unload)", "END");
		ImGui::Dummy(ImVec2(0, 2));
		ImGui::TextDisabled("menu acikken aim assist pasiftir");
		ImGui::TextDisabled("END: tum ozellikler kapanir, mutex serbest kalir");
		ImGui::TextDisabled("yeniden inject yapabilirsin — oyun kapatmadan");
	}

	ImGui::EndChild();

	ImGui::End();
	ImGui::PopStyleVar();
}

static void DrawOverlays()
{
	auto bgList = ImGui::GetBackgroundDrawList();
	auto& ec = features::enemyColor;
	ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(ec[0], ec[1], ec[2], ec[3]));

	size_t playerCount = 0;
	{
		std::lock_guard<std::mutex> lock(g_radarMutex);
		playerCount = globalData.size();
		if (playerCount)
		{
			for (size_t i = 0; i < playerCount; i++)
			{
				auto& val = globalData[i];

				// iskelet: gercek kemiklerden nokta + MST cizgileri (guncel offset testi)
				if (features::skeletonESP)
				{
					for (int b = 0; b < val.boneCount; b++)
					{
						if (val.boneX[b] < 0.f) continue;
						bgList->AddCircleFilled(ImVec2(val.boneX[b], val.boneY[b]), features::boneThick, col);
					}
					const bool useAuto = features::boneAuto;
					const int pairCount = useAuto ? val.skeletonPairCount : 16;
					for (int pr = 0; pr < pairCount; pr++)
					{
						const int a = useAuto ? val.skeletonPairs[pr][0] : features::bonePairs[pr][0];
						const int b2 = useAuto ? val.skeletonPairs[pr][1] : features::bonePairs[pr][1];
						if (a <= 0 && b2 <= 0) continue; // {0,0} = bos slot
						if (a < 0 || b2 < 0) continue;
						if (a >= val.boneCount || b2 >= val.boneCount) continue;
						if (val.boneX[a] < 0.f || val.boneX[b2] < 0.f) continue;
						bgList->AddLine(ImVec2(val.boneX[a], val.boneY[a]),
							ImVec2(val.boneX[b2], val.boneY[b2]), col, 2.f);
					}
				}

				if (features::myFeature1 && val.boxW > 1.f && (!val.ally || features::aimAllies))
				{
					float x = val.boxX, y = val.boxY, w = val.boxW, h = val.boxH;

					bgList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
						ImGui::ColorConvertFloat4ToU32(ImVec4(ec[0], ec[1], ec[2], 0.14f)));
					bgList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col, 0.f, 0, 1.4f);

					// hp bar (sol)
					float hp = val.health;
					if (hp < 0.f) hp = 0.f;
					if (hp > 100.f) hp = 100.f;
					float hpH = h * hp / 100.f;
					ImU32 hpCol = hp > 50.f ? IM_COL32(80, 220, 110, 255)
						: hp > 25.f ? IM_COL32(255, 190, 60, 255) : IM_COL32(255, 70, 70, 255);
					bgList->AddRectFilled(ImVec2(x - 6, y), ImVec2(x - 3, y + h), IM_COL32(0, 0, 0, 170));
					bgList->AddRectFilled(ImVec2(x - 6, y + h - hpH), ImVec2(x - 3, y + h), hpCol);

					// kalkan bari (sagda, mavi) — SDK: CachedLife[1]
					float sh = val.shield;
					if (sh < 0.f) sh = 0.f;
					if (sh > 100.f) sh = 100.f;
					if (sh > 0.f)
					{
						float shH = h * sh / 100.f;
						bgList->AddRectFilled(ImVec2(x + w + 3, y), ImVec2(x + w + 6, y + h), IM_COL32(0, 0, 0, 170));
						bgList->AddRectFilled(ImVec2(x + w + 3, y + h - shH), ImVec2(x + w + 6, y + h), IM_COL32(80, 160, 255, 255));
					}

					// mesafe (metre) kutunun altinda
					char txt[32];
					snprintf(txt, sizeof(txt), "%dm", (int)(val.distance + 0.5f));
					ImVec2 ts = ImGui::CalcTextSize(txt);
					bgList->AddText(ImVec2(x + w * 0.5f - ts.x * 0.5f + 1, y + h + 3), IM_COL32(0, 0, 0, 200), txt);
					bgList->AddText(ImVec2(x + w * 0.5f - ts.x * 0.5f, y + h + 2), IM_COL32(240, 240, 240, 230), txt);
				}
			}
		}
	}

	// aim assist FOV cemberi (crosshair etrafinda)
	if (features::aimAssist)
	{
		float cx = GetSystemMetrics(SM_CXSCREEN) * 0.5f;
		float cy = GetSystemMetrics(SM_CYSCREEN) * 0.5f;
		bgList->AddCircle(ImVec2(cx, cy), features::aimFovPx, IM_COL32(255, 180, 84, 90), 48, 1.f);
	}

	// kilit noktasi: aim'in sectigi nisanci noktasi (elmas) — geribildirim
	if (features::aimAssist && features::aimDbgHeld &&
		features::aimDbgLockX >= 0.f && features::aimDbgLockY >= 0.f)
	{
		const ImVec2 lp(features::aimDbgLockX, features::aimDbgLockY);
		bgList->AddCircle(lp, 7.f, IM_COL32(255, 70, 85, 230), 4, 1.6f);
		bgList->AddCircle(lp, 12.f, IM_COL32(255, 255, 255, 90), 24, 1.f);
	}

}

void Render()
{
	UpdateKeys();
	if (isMenuOpen)
		DrawMenu();
	DrawOverlays();
	AimAssistTick();
}
