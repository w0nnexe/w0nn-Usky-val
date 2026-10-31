#include <winsock2.h>
#include <ws2tcpip.h>
#include "webserver.h"
#include "cloudflared.h"
#include "../gui/gui.h"
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cmath>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

namespace web
{
	// ---- harita minimap kaynagi: media.valorant-api.com (kalici CDN linkleri, wiki minimaplari ile ayni gorsel) ----
	// aff[6]: OTOMATIK kalibrasyon (world cm -> minimap UV 0..1):
	//   u = aff[0]*x + aff[1]*y + aff[2] ; v = aff[3]*x + aff[4]*y + aff[5]
	// Kaynak: valorant-api.com harita verisi (Riot oyun dosyalari: xMultiplier/yMultiplier/xScalarToAdd/yScalarToAdd)
	// + minimap goruntusu uzerinde callout noktalariyla piksel duzeyinde dogrulandi (olcek sapmasi < %1).
	// Riot bu veriyi yayimlamadigi haritalar (Kasbah/Glitch/Piazza) -> sifir (otomatik yok).
	// District/Drift (TDM): Riot verisi yok; callout+minimap eslestirmesinden turetildi (skor <1px).
	struct MapDef { const char* name; const char* url; double aff[6]; };
	static const MapDef kMaps[] = {
		{"Ascent",   "https://media.valorant-api.com/maps/7eaecc1b-4337-bbf6-6ab9-04b8f06b3319/displayicon.png",
			{-0.0000047545054, 0.000068856046056, 0.826916799830249, -0.000068856046056, -0.0000047545054, 0.568510574589328}},
		{"Bind",     "https://media.valorant-api.com/maps/2c9d57ec-4431-9c5e-2939-8f9ef6dd5cba/displayicon.png",
			{-0.000003087821418, 0.000058919142551, 0.607587490108697, -0.000058919142551, -0.000003087821418, 0.960566272775346}},
		{"Haven",    "https://media.valorant-api.com/maps/2bee0dc9-4ffe-519b-1cbd-7fbe763a6047/displayicon.png",
			{-0.000006536680706, 0.000074714602357, 1.08891945297104, -0.000074714602357, -0.000006536680706, 0.580357130118676}},
		{"Split",    "https://media.valorant-api.com/maps/d960549e-485c-e861-8d71-aa9d1aed12a2/displayicon.png",
			{-0.000004557947356, 0.000077866713786, 0.866962533583106, -0.000077866713786, -0.000004557947356, 0.667794517484826}},
		{"Icebox",   "https://media.valorant-api.com/maps/e2ad5c54-4114-a870-9641-8ea21279579a/displayicon.png",
			{0.000017279029471, 0.000069302401954, 0.459319555662088, -0.000069302401954, 0.000017279029471, 0.341828842832907}},
		{"Breeze",   "https://media.valorant-api.com/maps/2fb9a4fd-47b8-4e7d-a969-74b4046ebd53/displayicon.png",
			{-0.000001236328472, 0.000070829210725, 0.449403680020274, -0.000070829210725, -0.000001236328472, 0.778308513019441}},
		{"Fracture", "https://media.valorant-api.com/maps/b529448b-4d60-346e-e89e-00a4c527a405/displayicon.png",
			{-0.000006798147934, 0.000077703186451, 0.621673145810577, -0.000077703186451, -0.000006798147934, 1.16108791210142}},
		{"Pearl",    "https://media.valorant-api.com/maps/fd267378-4d1d-484f-ff52-77821ed10dc2/displayicon.png",
			{0.00007568306665, 0.000018869907857, 0.067254684509054, 0.000018869907857, -0.00007568306665, 0.406548285661001}},
		{"Lotus",    "https://media.valorant-api.com/maps/2fe4ed3a-450a-948b-6d6b-e89a78e680a9/displayicon.png",
			{0.000006275213478, 0.000071726018263, 0.418280773421944, -0.000071726018263, 0.000006275213478, 0.906275510651827}},
		{"Sunset",   "https://media.valorant-api.com/maps/92584fbe-486a-b1b2-9faa-39b0f486b498/displayicon.png",
			{0.000004082204587, 0.000077893103711, 0.536682912072742, -0.000077893103711, 0.000004082204587, 0.467039582644614}},
		{"Abyss",    "https://media.valorant-api.com/maps/224b0a95-48b9-f703-1bd8-67aca101a61f/displayicon.png",
			{-0.000001413644921, 0.000080987663308, 0.505028728077195, -0.000080987663308, -0.000001413644921, 0.493678937702851}},
		{"Corrode",  "https://media.valorant-api.com/maps/1c18ab1f-420d-0d8b-71d0-77ad3c439115/displayicon.png",
			{0.000006076498384, 0.000069454694351, 0.513879319079977, -0.000069454694351, 0.000006076498384, 0.54954738561254}},
		{"District", "https://media.valorant-api.com/maps/690b3ed2-4dff-945b-8223-6da834e30d24/displayicon.png",
			{-0.000003114288309, 0.000127428616564, 0.573122572043724, -0.000127428616564, -0.000003114288309, 0.375991449324929}},
		{"Kasbah",   "https://media.valorant-api.com/maps/12452a9d-48c3-0b02-e7eb-0381c3520404/displayicon.png",
			{0, 0, 0, 0, 0, 0}},
		{"Drift",    "https://media.valorant-api.com/maps/2c09d728-42d5-30d8-43dc-96a05cc7ee9d/displayicon.png",
			{0.000094498208253, -0.000004125880875, 0.501417942090712, 0.000004125880875, 0.000094498208253, 0.501213715860253}},
		{"Glitch",   "https://media.valorant-api.com/maps/d6336a5a-428f-c591-98db-c8a291159134/displayicon.png",
			{0, 0, 0, 0, 0, 0}},
		{"Piazza",   "https://media.valorant-api.com/maps/de28aa9b-4cbe-1003-320e-6cb3ec309557/displayicon.png",
			{0, 0, 0, 0, 0, 0}},
		{"Summit",   "https://media.valorant-api.com/maps/756da597-416b-c0f2-f47b-afbdf28670bc/displayicon.png",
			{-0.00000267213974, 0.000074652191322, 0.08722886965204, -0.000074652191322, -0.00000267213974, 0.965849453877504}},
	};
	static const size_t kMapCount = sizeof(kMaps) / sizeof(kMaps[0]);

	// level adi normallestirme ("Ascent_P_0" -> "ascentp"...) ve eslestirme — JS normMap ile ayni kural
	static std::string NormMapName(const std::string& in)
	{
		std::string s;
		for (char ch : in)
		{
			ch = (char)tolower((unsigned char)ch);
			if (isalnum((unsigned char)ch) || ch == '_') s += ch;
		}
		for (int pass = 0; pass < 2; pass++)
		{
			if (s.size() > 2 && s.compare(s.size() - 2, 2, "_p") == 0) s = s.substr(0, s.size() - 2);
			while (true)
			{
				const size_t us = s.find_last_of('_');
				if (us == std::string::npos || us + 1 >= s.size()) break;
				bool digits = true;
				for (size_t i = us + 1; i < s.size(); i++)
					if (!isdigit((unsigned char)s[i])) { digits = false; break; }
				if (!digits) break;
				s = s.substr(0, us);
			}
		}
		return s;
	}

	// bilinen bir harita ise afin dondurur (Kasbah/Glitch/Piazza icin false — veri yok)
	bool GetMapAffine(const std::string& rawName, double out[6])
	{
		if (rawName.empty()) return false;
		const std::string n = NormMapName(rawName);
		if (n.size() < 3) return false;
		for (size_t i = 0; i < kMapCount; i++)
		{
			const std::string m = NormMapName(kMaps[i].name);
			if (m == n || (m.size() > 3 && n.size() > 3 &&
				(m.rfind(n, 0) == 0 || n.rfind(m, 0) == 0)))
			{
				double sum = 0;
				for (int j = 0; j < 6; j++) sum += fabs(kMaps[i].aff[j]);
				if (sum <= 0) return false;
				memcpy(out, kMaps[i].aff, sizeof(double) * 6);
				return true;
			}
		}
		return false;
	}

	// ---- disk konumlari (DLL klasoru) ----
	static std::string ModuleDir()
	{
		char buf[MAX_PATH] = {};
		HMODULE m = nullptr;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&ModuleDir, &m);
		if (!m || !GetModuleFileNameA(m, buf, MAX_PATH)) return ".";
		std::string s(buf);
		size_t p = s.find_last_of("\\/");
		return p == std::string::npos ? "." : s.substr(0, p);
	}
	static std::string MapCachePath(const std::string& name)
	{
		std::string safe;
		for (char c : name) if (isalnum((unsigned char)c) || c == '_' || c == '-') safe += c;
		if (safe.empty()) safe = "map";
		return ModuleDir() + "\\web_radar_maps\\" + safe + ".png";
	}
	static std::string CalibPath() { return ModuleDir() + "\\web_radar_calib.json"; }

	static bool ReadFileAll(const std::string& path, std::string& out)
	{
		FILE* f = nullptr; fopen_s(&f, path.c_str(), "rb");
		if (!f) return false;
		char buf[16384]; size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
		fclose(f);
		return !out.empty();
	}
	static bool WriteFileAll(const std::string& path, const std::string& data)
	{
		FILE* f = nullptr; fopen_s(&f, path.c_str(), "wb");
		if (!f) return false;
		bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
		fclose(f);
		return ok;
	}

	// ---- basit HTTPS GET (WinHTTP) ----
	static std::wstring Wide(const char* s)
	{
		int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
		if (n <= 0) return L"";
		std::wstring w((size_t)n - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
		return w;
	}

	static bool SplitUrl(const std::string& url, std::string& host, int& port, bool& secure, std::string& path)
	{
		const char* p = strstr(url.c_str(), "://");
		if (!p) return false;
		secure = (strncmp(url.c_str(), "https", 6) == 0);
		const char* h = p + 3;
		const char* slash = strchr(h, '/');
		std::string hp = slash ? std::string(h, slash - h) : std::string(h);
		port = secure ? 443 : 80;
		size_t colon = hp.find(':');
		if (colon != std::string::npos)
		{
			host = hp.substr(0, colon);
			port = atoi(hp.c_str() + colon + 1);
		}
		else host = hp;
		path = slash ? slash : "/";
		return !host.empty();
	}

	static bool HttpsGet(const std::string& url, std::string& out)
	{
		std::string host, path; int port; bool secure;
		if (!SplitUrl(url, host, port, secure, path)) return false;

		HINTERNET hSes = WinHttpOpen(L"w0nn-valo-radar/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSes) return false;
		WinHttpSetTimeouts(hSes, 5000, 5000, 5000, 20000);

		HINTERNET hCon = WinHttpConnect(hSes, Wide(host.c_str()).c_str(), (INTERNET_PORT)port, 0);
		if (!hCon) { WinHttpCloseHandle(hSes); return false; }

		HINTERNET hReq = WinHttpOpenRequest(hCon, L"GET", Wide(path.c_str()).c_str(), NULL,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
		if (hReq)
		{
			// varsayilan redirect politikasi yonlendirmeleri takip eder (https->http haric)
			DWORD pol = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
			WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &pol, sizeof(pol));
			// UA + Accept: bazi CDN/proxy kurulumlari basliksiz istegi reddediyor
			WinHttpAddRequestHeaders(hReq,
				L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\nAccept: */*",
				(ULONG)-1, WINHTTP_ADDREQ_FLAG_ADD);
		}
		// TLS 1.2+ garantisi (eski WinHTTP varsayilanlari TLS 1.0/1.1'e takilabiliyor)
		{
			DWORD tls = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
			tls |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
			WinHttpSetOption(hSes, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof(tls));
		}

		bool ok = false;
		if (hReq && WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
			WinHttpReceiveResponse(hReq, nullptr))
		{
			DWORD code = 0, sz = sizeof(code);
			if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_NO_HEADER_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX) && code == 200)
			{
				char buf[16384];
				for (;;)
				{
					DWORD avail = 0;
					if (!WinHttpQueryDataAvailable(hReq, &avail) || avail == 0) break;
					DWORD take = avail < sizeof(buf) ? avail : (DWORD)sizeof(buf);
					DWORD rd = 0;
					if (!WinHttpReadData(hReq, buf, take, &rd) || rd == 0) break;
					out.append(buf, rd);
				}
				ok = !out.empty();
			}
		}
		if (hReq) WinHttpCloseHandle(hReq);
		if (hCon) WinHttpCloseHandle(hCon);
		WinHttpCloseHandle(hSes);
		return ok;
	}

	// ---- minimap cache (bellek + disk) ----
	static std::mutex g_mapMutex;
	static std::map<std::string, std::string> g_mapCache; // name -> png bytes

	static bool GetMinimap(const std::string& name, std::string& png)
	{
		{
			std::lock_guard<std::mutex> lock(g_mapMutex);
			auto it = g_mapCache.find(name);
			if (it != g_mapCache.end()) { png = it->second; return true; }
		}
		if (ReadFileAll(MapCachePath(name), png) && png.size() > 4 && png[0] == (char)0x89 && png[1] == 'P')
		{
			std::lock_guard<std::mutex> lock(g_mapMutex);
			g_mapCache[name] = png;
			return true;
		}
		png.clear();

		std::string url;
		for (size_t i = 0; i < kMapCount; i++)
			if (name == kMaps[i].name) { url = kMaps[i].url; break; }
		if (url.empty())
		{
			// bilinmeyen harita: wiki kalibi kaldirildi (403 donduruyordu) — listede olmayan harita yok
			RadarPrintf("[web] bilinmeyen harita, minimap yok: %s\n", name.c_str());
			return false;
		}
		if (!HttpsGet(url, png)) { png.clear(); return false; }

		CreateDirectoryA((ModuleDir() + "\\web_radar_maps").c_str(), nullptr);
		WriteFileAll(MapCachePath(name), png);
		std::lock_guard<std::mutex> lock(g_mapMutex);
		g_mapCache[name] = png;
		RadarPrintf("[web] minimap indirildi: %s (%zu bytes)\n", name.c_str(), png.size());
		return true;
	}

	// ---- 2D ustten radar sayfasi (govdeye gomulu, harici dosya yok) ----
	static const char* kIndexHtml = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>radar</title>
<style>
*{box-sizing:border-box}
html,body{margin:0;height:100%;background:#0a0d12;color:#c8d3e0;font-family:Consolas,monospace;overflow:hidden}
#cv{display:block;width:100vw;height:100vh;cursor:crosshair;touch-action:none}
#bar{position:fixed;top:0;left:0;right:0;height:46px;display:flex;align-items:center;gap:8px;padding:0 14px;background:linear-gradient(180deg,#10141c,#0d1118);border-bottom:1px solid #1d2432;font-size:12px;z-index:10}
#bar b{color:#ff4655;letter-spacing:2px}
#bar .brand i{color:#5a6478;font-style:normal;margin-left:4px}
#bar button,#bar select{background:#161c28;color:#c8d3e0;border:1px solid #26314a;border-radius:6px;font:11px Consolas,monospace;padding:4px 9px;cursor:pointer}
#bar button:hover{background:#1e2736;border-color:#38466a}
#bar label.chip{display:flex;align-items:center;gap:4px;color:#8b94a6;cursor:pointer;font-size:11px;white-space:nowrap}
#stat{margin-left:auto}
#stat span{background:#121826;border:1px solid #26314a;border-radius:20px;padding:4px 12px;white-space:nowrap}
#panelE,#panelA{position:fixed;top:56px;width:212px;max-height:calc(100vh - 70px);overflow-y:auto;background:rgba(13,16,23,.93);border:1px solid #1d2432;border-radius:10px;padding:10px;z-index:9}
#panelE{left:10px}
#panelA{right:10px}
.ptitle{font-size:11px;letter-spacing:2px;color:#8b94a6;margin:0 0 8px 2px}
.plist .card{display:flex;gap:8px;align-items:center;background:#121722;border:1px solid #1a2130;border-radius:8px;padding:7px 8px;margin-bottom:6px}
.card .avatar{width:26px;height:26px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:bold;flex:none}
.card.e .avatar{background:rgba(255,70,85,.15);color:#ff6b78;border:1px solid rgba(255,70,85,.4)}
.card.a .avatar{background:rgba(58,160,255,.15);color:#6cb8ff;border:1px solid rgba(58,160,255,.4)}
.cinfo{flex:1;min-width:0}
.cname{font-size:11px;color:#e8ebf0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.hpbar{height:4px;background:#0b0f16;border-radius:2px;margin:4px 0 3px}
.hpbar i{display:block;height:4px;border-radius:2px}
.cmeta{font-size:10px;color:#77839a}
.empty{color:#5a6478;font-size:11px;padding:4px 2px}
#calPanel{position:fixed;top:56px;left:232px;background:rgba(10,14,20,.95);border:1px solid #35465e;padding:10px 12px;font-size:12px;border-radius:8px;display:none;max-width:360px;line-height:1.5;z-index:11}
#calPanel b{color:#ffb454}
#calPanel button{background:#161c28;color:#c8d3e0;border:1px solid #2a3a50;border-radius:5px;font:11px Consolas,monospace;padding:3px 8px;cursor:pointer;margin:2px 2px 2px 0}
#calMsg{color:#7ec97e;min-height:16px}
.pt{color:#8ab4f8}
#legend{position:fixed;bottom:8px;left:50%;transform:translateX(-50%);background:rgba(10,14,20,.8);border:1px solid #1d2432;padding:4px 12px;font-size:11px;border-radius:20px;color:#77839a;z-index:9}
#hint{display:none}
</style></head>
<body>
<canvas id="cv"></canvas>
<div id="bar"><span class="brand"><b>w0nn</b><i>RADAR</i></span> | baglaniyor...</div>
<div id="panelE"><div class="ptitle">DUSMANLAR</div><div id="elist" class="plist"><div class="empty">veri bekleniyor</div></div></div>
<div id="panelA"><div class="ptitle">TAKIM</div><div id="alist" class="plist"><div class="empty">veri bekleniyor</div></div></div>
<div id="calPanel">
<b>KALIBRASYON</b>
<div style="margin:6px 0">
<button id="tabMan">Manuel Kalibrasyon</button><button id="tabOto">Otomatik Kalibrasyon</button>
</div>
<div id="calManBox">
Oyunda ayirt edici bir yere git &rarr; <button id="calCap">1) Pozisyonu Al</button> &rarr; haritada ayni yere <u>tikla</u>.<br>
3 uzak nokta icin tekrarla (ucgen gibi yayilmis; duz cizgi uzerine olmasin).<br>
<span id="calPts"></span><br>
<button id="calSolve">Hesapla &amp; Kaydet</button>
</div>
<div id="calOtoBox" style="display:none">
Riot oyun verisi (valorant-api.com) ile tek tikla kalibrasyon — oyun icinde gezinmeye gerek yok.<br>
<span id="otoInfo" style="color:#8ab4f8"></span><br>
<button id="otoCal">Otomatik Kalibre Et &amp; Kaydet</button>
<label style="display:block;margin-top:4px;cursor:pointer"><input type="checkbox" id="otoAuto" checked> harita yuklenince otomatik uygula</label>
</div>
<div id="otoMsg" style="color:#7ec97e;min-height:16px"></div>
<div id="calMsg" style="color:#7ec97e;min-height:16px"></div>
<button id="calReset">Bu haritanin kayitli kalibrasyonunu sil</button>
</div>
<div id="legend">scroll: zoom | surukle: kaydir | beyaz: sen | yesil: takim | kirmizi: dusman</div>
<div id="hint"></div>
<input type="file" id="mfile" accept="image/*" style="display:none">
<script>
const cv=document.getElementById('cv'),ctx=cv.getContext('2d');
let W,H;function resize(){W=cv.width=innerWidth;H=cv.height=innerHeight}
addEventListener('resize',resize);resize();

// ---- durum ----
let pxPerM=6;                 // relative mode zoom
let st=null,lastOk=0;
let polling=false;
let MAPS=[];                  // sunucudaki harita listesi
let mapName=localStorage.getItem('rm_map')||'';
let mapImg=null,mapErr='';
let calib={};                 // sunucudaki kalibrasyonlar {Map:{m:[6],src}}
let M=null;                   // aktif affine [a,b,c,d,e,f] : img = M*world
let Msrc='';                  // kalibrasyon kaynagi: '' | 'manuel' | 'oto'
let calMode=false;
let calTab='m';               // 'm' manuel | 'a' otomatik
let mapMeta={};               // sunucudan otomatik kalibrasyon verileri {Map:{auto:[6]}}
let autoMap=true;             // oyun verisinden haritayi otomatik sec
let mapGuess='',guessCnt=0;   // karararli otomatik harita algilama (3 ustuste eslesme)
let cal={pts:[{w:null,i:null},{w:null,i:null},{w:null,i:null}]};

// ---- yardimci ----
const $=id=>document.getElementById(id);
function aff(w){return [M[0]*w[0]+M[1]*w[1]+M[2], M[3]*w[0]+M[4]*w[1]+M[5]];}
function affDir(f){return [M[0]*f[0]+M[1]*f[1], M[3]*f[0]+M[4]*f[1]];}
function det3(m){return m[0][0]*(m[1][1]*m[2][2]-m[1][2]*m[2][1])-m[0][1]*(m[1][0]*m[2][2]-m[1][2]*m[2][0])+m[0][2]*(m[1][0]*m[2][1]-m[1][1]*m[2][0]);}
function col3(A,j,v){const B=A.map(r=>r.slice());for(let k=0;k<3;k++)B[k][j]=v[k];return B;}
function solveAffine(p){
 const A=p.map(q=>[q.w[0],q.w[1],1]);
 const D=det3(A);
 if(!isFinite(D)||Math.abs(D)<1e5)return{err:'noktalar birbirine cok yakin veya duz cizgi uzerinde — daha uzak noktalar sec'};
 const ix=p.map(q=>q.i[0]),iy=p.map(q=>q.i[1]);
 const m=[det3(col3(A,0,ix))/D,det3(col3(A,1,ix))/D,det3(col3(A,2,ix))/D,
          det3(col3(A,0,iy))/D,det3(col3(A,1,iy))/D,det3(col3(A,2,iy))/D];
 const s=Math.hypot(m[0],m[1]);
 if(s<0.005||s>2)return{err:'olcek sacma gorunuyor ('+s.toFixed(3)+' px/cm) — noktalar yanlis olabilir'};
 return{m};
}

// ---- gorunum (harita modu): imgPx -> ekran ----
let view={s:1,z:1,ox:0,oy:0};
function fitView(){if(!mapImg||!mapImg.naturalWidth)return;view.s=Math.min(W/mapImg.naturalWidth,H/mapImg.naturalHeight)*0.95;view.z=1;view.ox=(W-mapImg.naturalWidth*view.s)/2;view.oy=(H-mapImg.naturalHeight*view.s)/2;}
function sOf(){return view.s*view.z;}
function imgToScr(x,y){return [x*sOf()+view.ox,y*sOf()+view.oy];}
function scrToImg(x,y){return [(x-view.ox)/sOf(),(y-view.oy)/sOf()];}

 // ---- ag: REST polling (WebSocket kapali — sunucu 501 donuyor) ----
 async function poll(){
  try{const r=await fetch('/data',{cache:'no-store'});st=await r.json();lastOk=Date.now()}catch(e){}
  setTimeout(poll,300)
 }
 poll();
async function boot(){
 try{
  const mr=await fetch('/maps',{cache:'no-store'});MAPS=await mr.json();
  const cr=await fetch('/calib',{cache:'no-store'});calib=await cr.json()||{};
  const mm=await fetch('/mapmeta',{cache:'no-store'});mapMeta=await mm.json()||{};
  for(const mn in mapMeta)inkFor(mn);
  if(barInit)initBar();
  if(mapName)loadMap(mapName);
 }catch(e){
  setTimeout(boot,2000); // fetch basarisiz — 2 sn sonra tekrar dene
 }
}
boot();

function applyCalib(){const c=mapName?calib[mapName]:null;M=(c&&c.m&&c.m.length===6)?c.m:null;Msrc=M?(c.src||'manuel'):'';}
function loadMap(n){
 mapName=n;localStorage.setItem('rm_map',n);mapErr='';
 applyCalib();
 if(M)exitCal();
 const img=new Image();
 img.onload=()=>{mapImg=img;fitView();maybeAutoCal()};
 img.onerror=()=>{mapImg=null;mapErr='minimap yuklenemedi — "dosyadan" butonu ile elle yukle'};
 img.src='/map?name='+encodeURIComponent(n);
}

// ---- otomatik kalibrasyon: Riot oyun verisi (valorant-api.com) ----
// mapMeta[Map].auto = world cm -> minimap UV (0..1) afin; piksel katsayilari yuklenen goruntunun gercek boyutundan
function autoAffine(){
 if(!mapImg||!mapImg.naturalWidth)return null;
 const md=mapMeta[mapName];if(!md||!md.auto||md.auto.length!==6)return null;
 const W=mapImg.naturalWidth,H=mapImg.naturalHeight,a=md.auto;
 const m=[a[0]*W,a[1]*W,a[2]*W,a[3]*H,a[4]*H,a[5]*H];
 const s=Math.hypot(m[0],m[1]);
 if(s<0.005||s>2)return null; // olcek saglamasi (px/cm)
 return m;
}
 // canli cozulen afin (DLL, minimap UV kesfi — Kasbah/Glitch/Piazza gibi Riotsuz haritalar)
 function liveAffine(){
  if(st&&st.daff&&st.daff.length===6&&st.dmap&&gameMapMatch(st.dmap)===mapName)
   return st.daff.slice();
  return null
 }
 function maybeAutoCal(){
  if(M)return; // zaten kalibre
  const m=autoAffine();
  if(m){
   M=m;Msrc='oto';
   if(barInit)$('stat').innerHTML='<span style="color:#7ec97e">kalibre (oto)</span> '+mapName;
   return;
  }
  const lv=liveAffine();
  if(lv){M=lv;Msrc='canli';return}
  // Kalibrasyon yok — kullaniciya soyle
  if(barInit&&mapImg){
   const md=mapMeta[mapName];
   if(!md||!md.auto||!md.auto.length){
    $('stat').innerHTML='<span style="color:#e58a45">bu harita icin oto kalibrasyon yok — "kalibre" > Manuel</span>';
   }
  }
 }
function renderOtoInfo(){
 const box=$('otoInfo');if(!box)return;
 if(!mapName){box.textContent='';return}
 const md=mapMeta[mapName];
 if(!md||!md.auto||!md.auto.length){box.innerHTML='bu harita icin Riot verisi yok (TDM haritasi) — <b>manuel</b> sekmeyi kullan';if($('otoCal'))$('otoCal').disabled=true;return}
 if($('otoCal'))$('otoCal').disabled=false;
 let html='kaynak: Riot oyun dosyalari (valorant-api.com), piksel duzeyinde dogrulandi';
 if(mapImg&&mapImg.naturalWidth){
  const m=autoAffine();
  if(m){
   const wM=Math.round(mapImg.naturalWidth/Math.hypot(m[0],m[1])/100);
   const hM=Math.round(mapImg.naturalHeight/Math.hypot(m[3],m[4])/100);
   html+='<br>harita olcegi: ~'+wM+'m x '+hM+'m';
  }
 }
 box.innerHTML=html;
}
// ---- otomatik harita secimi: sinir + minimap cizimi (ink) eslestirmesi ----
// Haritalarin dunya koordinat araliklari cakistigi icin yalniz sinir testi yetmez;
// oyuncularin afinle haritaya dusup minimap CIZIMININ ustune gelmesi (alfa>0) sart.
// Sinav: tum haritalarda kendisi 0.88-1.00, yanlis haritalarda <=0.79 skor veriyor.
// Esikler: skor>=0.70, ikinciye >=0.08 marj, en az 2 oyuncu, 2 ustuste ayni karar.
let mapInk={}; // name -> {g:Uint8Array(256*256)} | null (yukleme basarisiz)
function inkFor(name){
 if(mapInk[name])return mapInk[name].g;
 if(mapInk[name]===null)return null;
 const img=new Image();
 img.onload=()=>{
  const N=256,c=document.createElement('canvas');c.width=N;c.height=N;
  const x=c.getContext('2d');x.drawImage(img,0,0,N,N);
  let d;try{d=x.getImageData(0,0,N,N).data}catch(e){mapInk[name]=null;return}
  const g=new Uint8Array(N*N);
  for(let i=0;i<N*N;i++)g[i]=d[i*4+3]>24?1:0;
  mapInk[name]={g};
 };
 img.onerror=()=>{mapInk[name]=null};
 img.src='/map?name='+encodeURIComponent(name);
 return null;
}
function inkAt(g,u,v){
 const N=256,x=Math.round(u*(N-1)),y=Math.round(v*(N-1));
 for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++){
  const xx=x+dx,yy=y+dy;
  if(xx<0||yy<0||xx>=N||yy>=N)continue;
  if(g[yy*N+xx])return 1;
 }
 return 0;
}
function inkScore(name,L,ps){
 const a=mapMeta[name]&&mapMeta[name].auto;if(!a)return -1;
 const e=mapInk[name];if(!e||!e.g)return -1;
 let ok=0,tot=0;
 for(const p of [L,...ps]){
  const u=a[0]*p.x+a[1]*p.y+a[2],v=a[3]*p.x+a[4]*p.y+a[5];
  if(u<-0.02||u>1.02||v<-0.02||v>1.02)return -1; // sinir disi -> aday degil
  tot++;if(inkAt(e.g,u,v))ok++;
 }
 return tot?ok/tot:-1;
}
 // ---- oyunun soyledigi level adi -> bilinen harita eslestirmesi ----
 // "Ascent"/"Ascent_P"/"Ascent_P_0" gibi varyantlarini normallestirir; ink tahminden
 //_once gelir (tek dogru kaynak: oyunun kendisi). Elesmezse ink yontemi devam eder.
 function normMap(s){return String(s).toLowerCase().replace(/_[0-9]+$/,'').replace(/_p$/,'').replace(/[^a-z0-9]/g,'')}
 function gameMapMatch(n){
  const nn=normMap(n);if(nn.length<3)return null;
  for(const m of MAPS){
   const mm=normMap(m);
   if(mm===nn||(mm.length>3&&nn.length>3&&(mm.startsWith(nn)||nn.startsWith(mm))))return m
  }
  return null
 }
 function detectMap(){
  if(!autoMap||calMode)return;
  if(!st||!st.local)return;
  if(st.map){const gm=gameMapMatch(st.map);if(gm&&gm!==mapName){loadMap(gm);return}}
  const L=st.local,ps=st.players||[];
 if(!L.x&&!L.y&&!L.z)return;
 // FAZ 1: dunya koordinat sinirlari (aninda — indirme gerekmez)
 // Her haritanin afini ile [0,1] UV alanina dusuyor mu? Tek adaysa sec.
 let boundCands=[];
 for(const mn in mapMeta){
  const a=mapMeta[mn]&&mapMeta[mn].auto;if(!a)continue;
  const u=a[0]*L.x+a[1]*L.y+a[2],v=a[3]*L.x+a[4]*L.y+a[5];
  if(u>=0.03&&u<=0.97&&v>=0.03&&v<=0.97)boundCands.push(mn);
 }
 // Tum oyuncular da sinir icinde mi kontrol et
 if(boundCands.length>1&&ps.length>0){
  const fc=boundCands.filter(mn=>{
   const a=mapMeta[mn].auto;
   return ps.every(p=>{
    const u=a[0]*p.x+a[1]*p.y+a[2],v=a[3]*p.x+a[4]*p.y+a[5];
    return u>=0.0&&u<=1.0&&v>=0.0&&v<=1.0;
   });
  });
  if(fc.length>=1)boundCands=fc;
 }
 if(boundCands.length===1&&boundCands[0]!==mapName){
  loadMap(boundCands[0]);return; // tek aday — hemen sec
 }
 if(boundCands.length===0)return; // hicbir harita sinirlarina dusmuyor
 // FAZ 2: ink eslestirmesi (coklu aday varsa — maskeler hazirsa)
 if(ps.length<2){mapGuess='';guessCnt=0;return}
 let best='',bestS=-1,second=-1;
 for(const mn in mapMeta){
  const s=inkScore(mn,L,ps);
  if(s>bestS){second=bestS;bestS=s;best=mn}
  else if(s>second)second=s;
 }
 if(bestS<0.70||best===''||(second>=0&&bestS-second<0.08)){mapGuess='';guessCnt=0;return}
 if(best===mapName){mapGuess='';guessCnt=0;return}
 if(best===mapGuess){guessCnt++;if(guessCnt>=2){guessCnt=0;loadMap(best)}}
 else{mapGuess=best;guessCnt=1}
}
// radar nokta rengi: takim = yesil, dusman = kirmizi (kan azaldikca ton degisir)
function dotColor(p,hp){return p.team===1?(hp>50?'#3fd06a':'#7fce7f'):(hp>50?'#ff4040':'#ff9040')}
function setCalTab(t){
 calTab=t;
 if($('calManBox')){$('calManBox').style.display=t==='m'?'block':'none';$('calOtoBox').style.display=t==='a'?'block':'none';
 $('tabMan').style.background=t==='m'?'#2a3a50':'';$('tabOto').style.background=t==='a'?'#2a3a50':'';}
 if(t==='a')renderOtoInfo();
}

// ---- bar UI (ilk cizimde bir kez kur) ----
let barInit=false;
function initBar(){
 barInit=true;
 $('bar').innerHTML='<span class="brand"><b>w0nn</b><i>RADAR</i></span><select id="msel"></select><button id="bload">yukle</button><button id="bfile">dosyadan</button><button id="bcal">kalibre</button><button id="bshare">linki kopyala</button><span id="stat" class="pill"></span>';
 $('bshare').onclick=()=>{try{navigator.clipboard.writeText(location.href)}catch(e){}$('bshare').textContent='kopyalandi';setTimeout(()=>{$('bshare').textContent='linki kopyala'},1200)};
 const sel=$('msel');
 sel.innerHTML='<option value="">-- harita yok --</option>'+MAPS.map(n=>'<option'+(n===mapName?' selected':'')+'>'+n+'</option>').join('');
 sel.onchange=()=>{
  if(!sel.value){mapName='';mapImg=null;M=null;Msrc='';localStorage.removeItem('rm_map');return}
  loadMap(sel.value)
 };
 $('bload').onclick=()=>{if(sel.value)loadMap(sel.value)};
 $('bfile').onclick=()=>$('mfile').click();
 $('mfile').onchange=e=>{
  const f=e.target.files[0];if(!f)return;
  mapName=sel.value||'custom';localStorage.setItem('rm_map',mapName);applyCalib();
  const img=new Image();
  img.onload=()=>{mapImg=img;fitView();maybeAutoCal()};
  img.src=URL.createObjectURL(f);
 };
 $('bcal').onclick=()=>{
  if(!mapImg){$('calMsg').textContent='once harita yukle';$('calPanel').style.display='block';setCalTab('m');return}
  calMode=!calMode;$('calPanel').style.display=calMode?'block':'none';
  if(calMode){
   cal={pts:[{w:null,i:null},{w:null,i:null},{w:null,i:null}]};renderCal();
   setCalTab((!M&&mapMeta[mapName])?'a':(Msrc==='oto'?'a':'m'));
  }
 };
 }
function exitCal(){calMode=false;$('calPanel').style.display='none'}

// ---- kalibrasyon ----
function renderCal(){
 let h='';
 for(let k=0;k<3;k++){
  const p=cal.pts[k];
  const ws=p.w?Math.round(p.w[0])+','+Math.round(p.w[1]):'-';
  const is=p.i?Math.round(p.i[0])+','+Math.round(p.i[1]):'-';
  h+='<span class="pt">P'+(k+1)+': w('+ws+') &rarr; img('+is+')</span><br>';
 }
 $('calPts').innerHTML=h;
}
$('calCap').onclick=()=>{
 if(!st||!st.local){$('calMsg').textContent='veri yok (oyun aktif mi?)';return}
 if(Date.now()-lastOk>1500){$('calMsg').textContent='veri bayat — tekrar dene';return}
 const slot=cal.pts.find(p=>!p.w);
 if(!slot){$('calMsg').textContent='3 pozisyon zaten alindi';return}
 slot.w=[st.local.x,st.local.y];
 $('calMsg').textContent='alindi — simdi haritada ayni yere tikla';
 renderCal();
};
function calClick(ix,iy){
 if(calTab!=='m')return; // otomatik mod acikken harita tiklari manuel kalibrasyona girmez
 const slot=cal.pts.find(p=>p.w&&!p.i);
 if(!slot){$('calMsg').textContent='once "Pozisyonu Al" bas';return}
 slot.i=[ix,iy];
 $('calMsg').textContent=cal.pts.every(p=>p.w&&p.i)?'3 nokta tamam — "Hesapla & Kaydet"':'sonraki: oyunda baska yere git, Pozisyonu Al + tikla';
 renderCal();
}
$('calSolve').onclick=async()=>{
 if(!cal.pts.every(p=>p.w&&p.i)){$('calMsg').textContent='3 nokta eksik';return}
 const r=solveAffine(cal.pts);
 if(r.err){$('calMsg').textContent='HATA: '+r.err;return}
 M=r.m;Msrc='manuel';calib[mapName]={m:M,ts:Date.now(),src:'manuel'};
 try{
  const resp=await fetch('/calib',{method:'POST',body:JSON.stringify(calib)});
  if(!resp.ok)throw 0;
  $('calMsg').textContent='kaydedildi + sunucuya yuklendi (herkes gorer)';
 }catch(e){$('calMsg').textContent='kaydedildi (yalniz bu tarayici — sunucuya yazilamadi)';localStorage.setItem('rm_calib',JSON.stringify(calib))}
 renderCal();
};
$('calReset').onclick=async()=>{
 delete calib[mapName];M=null;Msrc='';
 // Kalibrasyon NOKTALARINI da sifirla — yoksa "3 pozisyon zaten secili" hatasi
 cal={pts:[{w:null,i:null},{w:null,i:null},{w:null,i:null}]};
 try{await fetch('/calib',{method:'POST',body:JSON.stringify(calib)})}catch(e){}
 $('calMsg').textContent='kalibrasyon silindi — yeni nokta alabilirsin';
 maybeAutoCal();renderCal();
};

// ---- otomatik kalibrasyon panel baglantilari ----
$('tabMan').onclick=()=>setCalTab('m');
$('tabOto').onclick=()=>setCalTab('a');
$('otoCal').onclick=async()=>{
 if(!mapImg){$('otoMsg').textContent='once harita yukle';return}
 const m=autoAffine();
 if(!m){$('otoMsg').textContent='bu harita icin otomatik veri yok — manuel kalibrasyon kullan';return}
 M=m;Msrc='oto';calib[mapName]={m:M,ts:Date.now(),src:'oto'};
 try{
  const resp=await fetch('/calib',{method:'POST',body:JSON.stringify(calib)});
  if(!resp.ok)throw 0;
  $('otoMsg').textContent='otomatik kalibrasyon kaydedildi — tum istemcilerde gecerli';
 }catch(e){$('otoMsg').textContent='uygulandi (sunucuya yazilamadi — yalniz bu tarayici)';localStorage.setItem('rm_calib',JSON.stringify(calib))}
 renderOtoInfo();
};
{
 const oa=$('otoAuto');oa.checked=localStorage.getItem('rm_otoauto')!=='0';
 oa.onchange=()=>{localStorage.setItem('rm_otoauto',oa.checked?'1':'0');if(oa.checked)maybeAutoCal()};
}

// ---- mouse / touch ----
let drag=null,moved=0;
cv.addEventListener('mousedown',e=>{drag={x:e.clientX,y:e.clientY,ox:view.ox,oy:view.oy};moved=0});
addEventListener('mousemove',e=>{if(!drag)return;const dx=e.clientX-drag.x,dy=e.clientY-drag.y;moved=Math.max(moved,Math.hypot(dx,dy));if(mapImg){view.ox=drag.ox+dx;view.oy=drag.oy+dy}});
addEventListener('mouseup',e=>{
 if(drag&&moved<5&&calMode&&mapImg){const [ix,iy]=scrToImg(e.clientX,e.clientY);calClick(ix,iy)}
 drag=null;
});
addEventListener('wheel',e=>{
 if(!mapImg)return;
 const f=e.deltaY<0?1.15:0.87,mx=e.clientX,my=e.clientY;
 const [ix,iy]=scrToImg(mx,my);
 view.z=Math.min(60,Math.max(0.05,view.z*f));
 const [sx,sy]=imgToScr(ix,iy);
 view.ox+=mx-sx;view.oy+=my-sy;
},{passive:true});
let t0=null,pinch0=0;
cv.addEventListener('touchstart',e=>{
 if(e.touches.length===1){t0={x:e.touches[0].clientX,y:e.touches[0].clientY,ox:view.ox,oy:view.oy};moved=0}
 else if(e.touches.length===2){pinch0=Math.hypot(e.touches[0].clientX-e.touches[1].clientX,e.touches[0].clientY-e.touches[1].clientY)}
},{passive:true});
cv.addEventListener('touchmove',e=>{
 if(e.touches.length===1&&t0&&mapImg){const dx=e.touches[0].clientX-t0.x,dy=e.touches[0].clientY-t0.y;moved=Math.max(moved,Math.hypot(dx,dy));view.ox=t0.ox+dx;view.oy=t0.oy+dy}
 else if(e.touches.length===2&&pinch0){
  const d=Math.hypot(e.touches[0].clientX-e.touches[1].clientX,e.touches[0].clientY-e.touches[1].clientY);
  const mx=(e.touches[0].clientX+e.touches[1].clientX)/2,my=(e.touches[0].clientY+e.touches[1].clientY)/2;
  const [ix,iy]=scrToImg(mx,my);view.z=Math.min(60,Math.max(0.05,view.z*d/pinch0));pinch0=d;
  const [sx,sy]=imgToScr(ix,iy);view.ox+=mx-sx;view.oy+=my-sy;
 }
},{passive:true});
cv.addEventListener('touchend',e=>{
 if(t0&&moved<8&&calMode&&mapImg&&e.changedTouches[0]){const [ix,iy]=scrToImg(e.changedTouches[0].clientX,e.changedTouches[0].clientY);calClick(ix,iy)}
 if(e.touches.length===0){t0=null;pinch0=0}
});

// ---- cizim ----
let lastBar=0,lastLocKey='',lastLocTime=0;
// kalibrasyon yokken haritanin ustunde kosede gosterilen mini radar — componentler
// asla gorunmez kalmasin diye (foto tek basina kalmasin)
function drawMini(L,ps,ox,oy,sz){
 ctx.fillStyle='rgba(8,11,16,.78)';ctx.strokeStyle='#2a3a50';ctx.lineWidth=1;
 ctx.beginPath();ctx.arc(ox+sz/2,oy+sz/2,sz/2,0,7);ctx.fill();ctx.stroke();
 const cx=ox+sz/2,cy=oy+sz/2,u=(sz/2-10)/2500; // ~25m yaricap
 const yaw=(L.yaw||0)*Math.PI/180;
 const hd=Math.atan2(-Math.sin(yaw),Math.cos(yaw));
 ctx.fillStyle='rgba(255,255,255,.06)';
 ctx.beginPath();ctx.moveTo(cx,cy);ctx.arc(cx,cy,sz/2-10,hd-0.7,hd+0.7);ctx.closePath();ctx.fill();
 for(const p of ps){
  const x=cx+(p.x-L.x)*u,y=cy-(p.y-L.y)*u;
  if(Math.hypot(x-cx,y-cy)>sz/2-10)continue;
  const hp=Math.max(0,Math.min(100,p.hp||0));
  ctx.fillStyle=dotColor(p,hp);
  ctx.beginPath();ctx.arc(x,y,4,0,7);ctx.fill();
 }
 ctx.save();ctx.translate(cx,cy);ctx.rotate(hd+Math.PI/2);
 ctx.fillStyle='#fff';ctx.beginPath();ctx.moveTo(0,-8);ctx.lineTo(5,6);ctx.lineTo(-5,6);ctx.closePath();ctx.fill();ctx.restore();
}
let lastList=0;
function updateLists(ps,L){
 const now=Date.now();if(now-lastList<250)return;lastList=now;
 const eb=$('elist'),ab=$('alist');if(!eb||!ab)return;
 let eh='',ah='',ne=0,na=0;
 for(let i=0;i<ps.length;i++){
  const p=ps[i],ally=p.team===1;
  const hp=Math.max(0,Math.min(100,p.hp||0));
  const hc=hp>50?'#50dc6e':hp>25?'#ffbe3c':'#ff4646';
  const dist=L?Math.round(Math.hypot(p.x-L.x,p.y-L.y)/100)+'m':'-';
  const n=ally?++na:++ne;
  const nm=(ally?'Takim #':'Dusman #')+n;
  const card='<div class="card '+(ally?'a':'e')+'"><div class="avatar">'+nm.charAt(0)+'</div><div class="cinfo"><div class="cname">'+nm+'</div><div class="hpbar"><i style="width:'+hp+'%;background:'+hc+'"></i></div><div class="cmeta">'+dist+' | '+hp+' hp</div></div></div>';
  if(ally)ah+=card;else eh+=card;
 }
 eb.innerHTML=eh||'<div class="empty">yok</div>';
 ab.innerHTML=ah||'<div class="empty">yok</div>';
}
function draw(){
  requestAnimationFrame(draw);
  ctx.fillStyle='#0a0d12';ctx.fillRect(0,0,W,H);
  if(!barInit&&(MAPS.length||mapName))initBar();
  if(!st||!st.local){
  ctx.fillStyle='#667';ctx.font='15px Consolas';ctx.fillText('veri bekleniyor (/data)...',20,60);
  if(barInit&&Date.now()-lastBar>400){lastBar=Date.now();$('stat').innerHTML='<span style="color:#e55">OFFLINE</span> — oyun aktif mi?'}
  return;
 }
 if(!barInit)initBar();
 const L=st.local,ps=st.players||[];
 if(Date.now()-lastBar>400){lastBar=Date.now();$('stat').innerHTML='<span style="color:#7ee787">BAGLI</span>';}
 updateLists(ps,L);
 // veri tazeligi: local pozisyon hic degismiyorsa oyun tarafi donmus olabilir
 const lk=L.x.toFixed(1)+'|'+L.y.toFixed(1)+'|'+L.z.toFixed(1);
 if(lk!==lastLocKey){lastLocKey=lk;lastLocTime=Date.now()}
 const dataStale=(Date.now()-lastLocTime)>5000&&(L.x||L.y||L.z);
 const staleTag=dataStale?' <span style="color:#e55">VERI DONUK!</span>':'';

  if(mapImg&&mapImg.naturalWidth){
   // ================= HARITA MODU =================
   if(!M)maybeAutoCal(); // Riot verisi ya da canli cozulen afin gelmis olabilir
   ctx.imageSmoothingEnabled=true;
  ctx.drawImage(mapImg,view.ox,view.oy,mapImg.naturalWidth*sOf(),mapImg.naturalHeight*sOf());
  ctx.font='12px Consolas';
  if(!M){
   ctx.fillStyle='#ffb454';ctx.font='15px Consolas';
   ctx.fillText('harita yuklu ama kalibrasyon yok — "kalibre" > Otomatik (tek tik) ya da manuel 3 nokta',16,70);
   drawMini(L,ps,W-170,H-200,150); // componentler her kosulda gorunur: mini radar fotografin USTUNDE
  }else{
   // oyuncular: once takim (yesil), sonra dusman (kirmizi) — harita en altta, noktalar ustunde
   for(const pass of [1,0]){
   for(const p of ps){
    if((p.team===1?1:0)!==pass)continue;
    const [ix,iy]=aff([p.x,p.y]);
    const [sx,sy]=imgToScr(ix,iy);
    if(sx<-60||sy<-60||sx>W+60||sy>H+60)continue;
    const hp=Math.max(0,Math.min(100,p.hp||0));
    ctx.fillStyle=dotColor(p,hp);
    ctx.beginPath();ctx.arc(sx,sy,6,0,7);ctx.fill();
    ctx.strokeStyle='rgba(255,255,255,.6)';ctx.lineWidth=1;ctx.stroke();
    const md=Math.hypot(p.x-L.x,p.y-L.y)/100;
    ctx.fillStyle='#eef';ctx.fillText(Math.round(md)+'m',sx+9,sy-6);
    ctx.fillStyle='#222';ctx.fillRect(sx-12,sy+9,24,4);
    ctx.fillStyle=p.team===1?'#2f9e57':'#e33';ctx.fillRect(sx-12,sy+9,24*hp/100,4);
   }}
   // yerel oyuncu: nokta + yon cizgisi
   {
    const [ix,iy]=aff([L.x,L.y]);
    const [sx,sy]=imgToScr(ix,iy);
    const yaw=(L.yaw||0)*Math.PI/180;
    const fw=affDir([Math.cos(yaw),Math.sin(yaw)]);
    const a=Math.atan2(fw[1],fw[0]);
    ctx.strokeStyle='#fff';ctx.lineWidth=2;
    ctx.beginPath();ctx.moveTo(sx,sy);ctx.lineTo(sx+Math.cos(a)*20,sy+Math.sin(a)*20);ctx.stroke();
    ctx.fillStyle='#fff';ctx.beginPath();ctx.arc(sx,sy,6,0,7);ctx.fill();
    ctx.strokeStyle='#000';ctx.lineWidth=1;ctx.stroke();
   }
  }
  // kalibrasyon isaretleri
  if(calMode){
   for(let k=0;k<3;k++){
    const p=cal.pts[k];if(!p.i)continue;
    const [sx,sy]=imgToScr(p.i[0],p.i[1]);
    ctx.strokeStyle='#ffb454';ctx.lineWidth=2;
    ctx.beginPath();ctx.arc(sx,sy,9,0,7);ctx.stroke();
    ctx.beginPath();ctx.moveTo(sx-13,sy);ctx.lineTo(sx+13,sy);ctx.moveTo(sx,sy-13);ctx.lineTo(sx,sy+13);ctx.stroke();
    ctx.fillStyle='#ffb454';ctx.fillText('P'+(k+1),sx+12,sy-10);
   }
  }
  if(Date.now()-lastBar>400){
   lastBar=Date.now();
   const nAl=ps.filter(q=>q.team===1).length,nEn=ps.length-nAl;
     $('stat').innerHTML=mapName+(M?(' <span style="color:#7ec97e">[kalibre &#10003;'+(Msrc==='oto'?' oto':Msrc==='canli'?' canli':'')+']</span>'):' <span style="color:#e58a45">[kalibrasyon yok]</span>')
     +(mapName||!autoMap?'':' <span style="color:#8ab4f8">[harita otomatik bekleniyor]</span>')
     +(st.map?' ['+st.map+']':'')
     +' | dusman: '+nEn+' | takim: '+nAl+' | ben: '+Math.round(L.x)+','+Math.round(L.y)
     +' | '+('HTTP')+' | guncelleme: '+((Date.now()-lastOk)/1000).toFixed(1)+'s'+staleTag;
   $('hint').textContent=mapErr;
  }
 }else{
  // ================= RELATIVE MOD (harita yok) =================
  const cx=W/2,cy=H/2,u2p=pxPerM/100;
  const g=1000*u2p;
  if(g>8){ctx.strokeStyle='rgba(70,90,120,.15)';ctx.lineWidth=1;ctx.beginPath();
   for(let x=cx%g;x<W;x+=g){ctx.moveTo(x,0);ctx.lineTo(x,H)}
   for(let y=cy%g;y<H;y+=g){ctx.moveTo(0,y);ctx.lineTo(W,y)}
   ctx.stroke();}
  const yaw=(L.yaw||0)*Math.PI/180;
  const dx=Math.cos(yaw),dy=-Math.sin(yaw);
  const hd=Math.atan2(dy,dx);
  ctx.fillStyle='rgba(255,255,255,.05)';
  ctx.beginPath();ctx.moveTo(cx,cy);ctx.arc(cx,cy,150,hd-0.7,hd+0.7);ctx.closePath();ctx.fill();
  ctx.font='12px Consolas';
  for(const pass of [1,0]){
  for(const p of ps){
   if((p.team===1?1:0)!==pass)continue;
   const x=cx+(p.x-L.x)*u2p,y=cy-(p.y-L.y)*u2p;
   if(x<-60||y<-60||x>W+60||y>H+60)continue;
   const hp=Math.max(0,Math.min(100,p.hp||0));
   ctx.fillStyle=dotColor(p,hp);
   ctx.beginPath();ctx.arc(x,y,5,0,7);ctx.fill();
   const m=Math.hypot(p.x-L.x,p.y-L.y)/100;
   ctx.fillStyle='#eef';ctx.fillText(Math.round(m)+'m',x+9,y-6);
    ctx.fillStyle='#222';ctx.fillRect(x-12,y+8,24,4);
    ctx.fillStyle=p.team===1?'#2f9e57':'#e33';ctx.fillRect(x-12,y+8,24*hp/100,4);
  }}
  ctx.save();ctx.translate(cx,cy);ctx.rotate(hd+Math.PI/2);
  ctx.fillStyle='#fff';ctx.beginPath();ctx.moveTo(0,-9);ctx.lineTo(6,7);ctx.lineTo(-6,7);ctx.closePath();ctx.fill();ctx.restore();
  // haritasiz mod rehberi: veri akiyor ama harita yuklu degil — ne yapilacagi soylenir
  ctx.textAlign='center';
  ctx.fillStyle='#7ee787';ctx.font='bold 14px Consolas';
  ctx.fillText('VERI AKIYOR - haritasiz mod (sen merkezdesin, kusbakisi)',W/2,86);
  ctx.fillStyle='#c8d3e0';ctx.font='13px Consolas';
  ctx.fillText("Asil radar icin ustteki harita listesinden haritani sec ve 'yukle'ye bas",W/2,108);
  ctx.fillStyle='#77839a';ctx.font='11px Consolas';
  ctx.fillText("('oto harita' isaretliyse botlu/normal maçta kendiliginden bulur; sectigin harita bir sonraki oturumda da hatirlanir)",W/2,126);
  ctx.textAlign='left';
  if(Date.now()-lastBar>400){
   lastBar=Date.now();
   const nAl=ps.filter(q=>q.team===1).length,nEn=ps.length-nAl;
     $('stat').innerHTML=(autoMap?'harita otomatik algilanacak':'harita yok')+(st.map?' ['+st.map+']':'')+' | dusman: '+nEn+' | takim: '+nAl+' | ben: '+Math.round(L.x)+','+Math.round(L.y)+' | yaw: '+Math.round(L.yaw||0)+'&deg; | '+('HTTP')+' | zoom: '+pxPerM.toFixed(1)+'px/m | guncelleme: '+((Date.now()-lastOk)/1000).toFixed(1)+'s'+staleTag;
   $('hint').textContent=mapErr;
  }
 }
}
draw();
</script></body></html>)HTML";

	// ---- JSON: globalData + local player anlik goruntusu ----
	static std::string BuildJson()
	{
		// NaN/inf %.1f ile "nan"/"inf" uretir -> JSON.parse patlar -> sayfa veri beklerde takilir.
		// Bu yuzden tum sayilar finite degilse 0 yazilir.
		auto jnum = [](double v) { return std::isfinite(v) ? v : 0.0; };

		std::vector<playerData> snap;
		float lx = 0.f, ly = 0.f, lz = 0.f, yaw = 0.f;
		std::string map;
		{
			std::lock_guard<std::mutex> lock(g_radarMutex);
			snap = globalData;
			lx = g_localX; ly = g_localY; lz = g_localZ; yaw = g_localYaw;
			map = g_mapName;
		}
		if (!features::webRadar)
			snap.clear(); // menuden kapatildiysa oyuncu akisi bosalir, local pozisyon kalir

		char line[288];
		std::string out;
		out.reserve(96 + snap.size() * 128);
		snprintf(line, sizeof(line), "{\"local\":{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"yaw\":%.1f},\"map\":\"%s\",\"players\":[",
			jnum(lx), jnum(ly), jnum(lz), jnum(yaw), map.c_str());
		out += line;
		for (size_t i = 0; i < snap.size(); i++)
		{
			snprintf(line, sizeof(line), "%s{\"x\":%.1f,\"y\":%.1f,\"z\":%.1f,\"hp\":%.0f,\"team\":%d}",
				i ? "," : "", jnum(snap[i].worldX), jnum(snap[i].worldY), jnum(snap[i].worldZ),
				jnum(snap[i].health), snap[i].ally ? 1 : 0);
			out += line;
		}
		double daff[6] = {};
		std::string dmap;
		{
			std::lock_guard<std::mutex> lock(g_radarMutex);
			dmap = g_derivedMap;
			if (!dmap.empty()) memcpy(daff, g_derivedAff, sizeof(double) * 6);
		}

		out += "]";
		if (!dmap.empty())
		{
			char l2[288];
			snprintf(l2, sizeof(l2), ",\"dmap\":\"%s\",\"daff\":[%.15g,%.15g,%.15g,%.15g,%.15g,%.15g]",
				dmap.c_str(), daff[0], daff[1], daff[2], daff[3], daff[4], daff[5]);
			out += l2;
		}
		out += "}";
		return out;
	}

	static bool SendAll(SOCKET s, const char* p, int len)
	{
		while (len > 0)
		{
			int n = send(s, p, len, 0);
			if (n <= 0) return false;
			p += n; len -= n;
		}
		return true;
	}

	// ---- WebSocket (RFC6455, sunucu tarafi) — /ws uzerinden radar push ----
	// Polling'deki 300ms + her istekte yeni TCP baglantisi gecikmesinin yerine
	// tek baglanti uzerinden ~120ms'lik sunucu push'u.
	static void Sha1(const uint8_t* d, size_t n, uint8_t out[20])
	{
		uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
		std::vector<uint8_t> msg(d, d + n);
		msg.push_back(0x80);
		while (msg.size() % 64 != 56) msg.push_back(0);
		const uint64_t bits = (uint64_t)n * 8;
		for (int i = 7; i >= 0; i--) msg.push_back((uint8_t)(bits >> (i * 8)));
		for (size_t off = 0; off < msg.size(); off += 64)
		{
			uint32_t w[80];
			for (int i = 0; i < 16; i++)
				w[i] = ((uint32_t)msg[off + i * 4] << 24) | ((uint32_t)msg[off + i * 4 + 1] << 16) |
				((uint32_t)msg[off + i * 4 + 2] << 8) | (uint32_t)msg[off + i * 4 + 3];
			for (int i = 16; i < 80; i++)
			{
				const uint32_t v = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
				w[i] = (v << 1) | (v >> 31);
			}
			uint32_t a = h[0], b = h[1], c = h[2], e2 = h[3], e = h[4];
			for (int i = 0; i < 80; i++)
			{
				uint32_t f, k;
				if (i < 20) { f = (b & c) | ((~b) & e2); k = 0x5A827999; }
				else if (i < 40) { f = b ^ c ^ e2; k = 0x6ED9EBA1; }
				else if (i < 60) { f = (b & c) | (b & e2) | (c & e2); k = 0x8F1BBCDC; }
				else { f = b ^ c ^ e2; k = 0xCA62C1D6; }
				const uint32_t t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
				e = e2; e2 = c; c = (b << 30) | (b >> 2); b = a; a = t;
			}
			h[0] += a; h[1] += b; h[2] += c; h[3] += e2; h[4] += e;
		}
		for (int i = 0; i < 5; i++)
		{
			out[i * 4] = (uint8_t)(h[i] >> 24); out[i * 4 + 1] = (uint8_t)(h[i] >> 16);
			out[i * 4 + 2] = (uint8_t)(h[i] >> 8); out[i * 4 + 3] = (uint8_t)h[i];
		}
	}

	static const char* kB64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	static std::string B64(const uint8_t* d, size_t n)
	{
		std::string o;
		o.reserve((n + 2) / 3 * 4);
		for (size_t i = 0; i < n; i += 3)
		{
			uint32_t v = (uint32_t)d[i] << 16;
			int rem = 1;
			if (i + 1 < n) { v |= (uint32_t)d[i + 1] << 8; rem = 2; }
			if (i + 2 < n) { v |= d[i + 2]; rem = 3; }
			o += kB64Chars[(v >> 18) & 63];
			o += kB64Chars[(v >> 12) & 63];
			o += rem > 1 ? kB64Chars[(v >> 6) & 63] : '=';
			o += rem > 2 ? kB64Chars[v & 63] : '=';
		}
		return o;
	}

	// sunucu -> istemci text frame (maske YOK; payload < 64KB -> 16-bit uzunluk)
	static std::string WsFrame(const std::string& payload)
	{
		std::string f;
		f.reserve(payload.size() + 4);
		f += (char)0x81;
		if (payload.size() < 126) f += (char)payload.size();
		else
		{
			f += (char)126;
			f += (char)(payload.size() >> 8);
			f += (char)(payload.size() & 0xFF);
		}
		f += payload;
		return f;
	}

	static std::vector<SOCKET> g_wsClients;
	static std::mutex g_wsMutex;
	static bool s_pushStarted = false;

	static void WsPushLoop()
	{
		RadarDbg("web: /ws push hatti basladi (~120ms) — polling devri kapaniyor");
		for (;;)
		{
			Sleep(120);
			std::vector<SOCKET> clients;
			{
				std::lock_guard<std::mutex> lock(g_wsMutex);
				clients = g_wsClients;
			}
			if (clients.empty()) continue;

			const std::string frame = WsFrame(BuildJson());
			std::vector<SOCKET> dead;
			for (SOCKET s : clients)
				if (!SendAll(s, frame.data(), (int)frame.size())) dead.push_back(s);
			for (SOCKET s : dead)
			{
				closesocket(s);
				std::lock_guard<std::mutex> lock(g_wsMutex);
				for (size_t i = 0; i < g_wsClients.size(); i++)
					if (g_wsClients[i] == s) { g_wsClients.erase(g_wsClients.begin() + i); break; }
			}
		}
	}

	static void WsRegister(SOCKET s)
	{
		std::lock_guard<std::mutex> lock(g_wsMutex);
		g_wsClients.push_back(s);
		if (!s_pushStarted)
		{
			s_pushStarted = true;
			std::thread(WsPushLoop).detach();
		}
	}

	static void WsUnregister(SOCKET s)
	{
		std::lock_guard<std::mutex> lock(g_wsMutex);
		for (size_t i = 0; i < g_wsClients.size(); i++)
			if (g_wsClients[i] == s) { g_wsClients.erase(g_wsClients.begin() + i); break; }
	}

	// ---- kucuk yardimcilar: query string + url decode ----
	static const char* FindCI(const char* hay, const char* needle)
	{
		size_t nl = strlen(needle);
		for (const char* p = hay; *p; p++)
		{
			size_t i = 0;
			for (; i < nl && p[i]; i++)
				if (tolower((unsigned char)p[i]) != tolower((unsigned char)needle[i])) break;
			if (i == nl) return p;
		}
		return nullptr;
	}
	static std::string UrlDecode(const std::string& in)
	{
		std::string out; out.reserve(in.size());
		for (size_t i = 0; i < in.size(); i++)
		{
			if (in[i] == '%' && i + 2 < in.size())
			{
				int v = 0;
				if (sscanf_s(in.c_str() + i + 1, "%2x", &v) == 1) { out += (char)v; i += 2; continue; }
			}
			out += in[i] == '+' ? ' ' : in[i];
		}
		return out;
	}
	static bool QueryParam(const std::string& q, const char* key, std::string& val)
	{
		std::string k = key, cur;
		size_t pos = 0;
		while (pos <= q.size())
		{
			size_t amp = q.find('&', pos);
			if (amp == std::string::npos) amp = q.size();
			cur = q.substr(pos, amp - pos);
			size_t eq = cur.find('=');
			if (eq != std::string::npos && cur.substr(0, eq) == k)
			{
				val = UrlDecode(cur.substr(eq + 1));
				return !val.empty();
			}
			pos = amp + 1;
		}
		return false;
	}

	static std::atomic<int> g_clients{ 0 };

	static std::string BuildResponse(const std::string& req)
	{
		if (req.size() < 16)
			return "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

		char method[16] = {}, path[512] = {};
		sscanf_s(req.c_str(), "%15s %511s", method, (unsigned)sizeof(method), path, (unsigned)sizeof(path));

		// path/query ayir
		std::string p = path, q;
		size_t qm = p.find('?');
		if (qm != std::string::npos) { q = p.substr(qm + 1); p = p.substr(0, qm); }

		// WebSocket kapali: polling kullanilir
		if (p == "/ws")
			return "HTTP/1.1 501 WebSocket kapali\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

		std::string body;
		const char* type = "text/html; charset=utf-8";
		int code = 200;
		const char* status = "OK";

		if (p == "/data" || p == "/api")
		{
			body = BuildJson();
			type = "application/json; charset=utf-8";
		}
		else if (p == "/maps")
		{
			body = "[";
			for (size_t i = 0; i < kMapCount; i++)
				body += (i ? ",\"" : "\"") + std::string(kMaps[i].name) + "\"";
			body += "]";
			type = "application/json; charset=utf-8";
		}
		else if (p == "/mapmeta")
		{
			// otomatik kalibrasyon verisi: world cm -> minimap UV (0..1) afin donusumu
			body = "{";
			bool first = true;
			for (size_t i = 0; i < kMapCount; i++)
			{
				const double* a = kMaps[i].aff;
				if (a[0] == 0 && a[1] == 0 && a[3] == 0 && a[4] == 0) continue; // TDM: veri yok
				char line[512];
				snprintf(line, sizeof(line),
					"%s\"%s\":{\"auto\":[%.15g,%.15g,%.15g,%.15g,%.15g,%.15g]}",
					first ? "" : ",", kMaps[i].name, a[0], a[1], a[2], a[3], a[4], a[5]);
				body += line;
				first = false;
			}
			body += "}";
			type = "application/json; charset=utf-8";
		}
		else if (p == "/calib" && !strcmp(method, "POST"))
		{
			// govde zaten req icinde (event loop Content-Length kadar bekledi)
			int clen = 0;
			const char* cl = FindCI(req.c_str(), "Content-Length:");
			if (cl) clen = atoi(cl + 15);
			if (clen < 0 || clen > 65536) clen = 0;
			size_t hdrEnd = req.find("\r\n\r\n");
			hdrEnd = hdrEnd == std::string::npos ? req.size() : hdrEnd + 4;
			body = req.substr(hdrEnd, (size_t)clen);
			bool ok = body.size() >= 2 && body.size() < 65536 && body[0] == '{';
			if (ok) ok = WriteFileAll(CalibPath(), body);
			body = ok ? "{\"ok\":true}" : "{\"ok\":false}";
			type = "application/json; charset=utf-8";
			if (!ok) { code = 500; status = "Save Failed"; }
		}
		else if (p == "/calib")
		{
			body.clear(); // ONCE TEMIZLE — yoksa "{}" + dosya = gecersiz JSON
			ReadFileAll(CalibPath(), body);
			if (body.empty() || body[0] != '{') body = "{}";
			type = "application/json; charset=utf-8";
		}
		else if (p == "/map")
		{
			std::string name;
			if (QueryParam(q, "name", name) && GetMinimap(name, body))
			{
				type = "image/png";
			}
			else
			{
				body = "{\"error\":\"minimap indirilemedi - 'dosyadan' butonu ile elle yukle\"}";
				type = "application/json; charset=utf-8";
				code = 404; status = "Not Found";
			}
		}
		else if (p == "/" || p == "/index.html")
		{
			body = kIndexHtml;
		}
		else
		{
			body = "{\"error\":\"not found\"}";
			type = "application/json; charset=utf-8";
			code = 404; status = "Not Found";
		}

		char header[512];
		int hl = snprintf(header, sizeof(header),
			"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
			"Access-Control-Allow-Origin: *\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",
			code, status, type, body.size());
		if (hl <= 0) return "HTTP/1.1 500 Internal\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
		return std::string(header, (size_t)hl) + body;
	}

	static void PrintLanIp(uint16_t port)
	{
		char host[256] = {};
		if (gethostname(host, sizeof(host) - 1) != 0) return;
		struct addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		struct addrinfo* res = nullptr;
		if (getaddrinfo(host, NULL, &hints, &res) == 0 && res)
		{
			for (struct addrinfo* p = res; p; p = p->ai_next)
			{
				struct sockaddr_in* a = (struct sockaddr_in*)p->ai_addr;
				unsigned char* ip = (unsigned char*)&a->sin_addr;
				if (ip[0] == 127) continue;
				RadarPrintf("[web] LAN  : http://%u.%u.%u.%u:%u\n", ip[0], ip[1], ip[2], ip[3], port);
				break;
			}
			freeaddrinfo(res);
		}
	}

	static void ServerLoop(uint16_t port)
	{
		WSADATA wsa = {};
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		{
			RadarPrintf("[web] WSAStartup hatasi\n");
			return;
		}

		SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listener == INVALID_SOCKET)
		{
			RadarDbg("web: socket olusturulamadi");
			return;
		}

		// SO_REUSEADDR KULLANMIYORUZ: Windows'ta ayni porta IKI listener bind etmeye izin
		// veriyor -> cift inject / zombie session durumunda cloudflared yanlis (olu) listener'a
		// dusup bad gateway veriyordu. SO_EXCLUSIVEADDRUSE ile ikinci bind acikca PATLAR.
		BOOL opt = TRUE;
		setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&opt, sizeof(opt));

		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		// port doluysa (cift inject / zombie) sonraki 9 portu dene — radar hep acilsin
		uint16_t bound = 0;
		for (uint16_t p = port; p < (uint16_t)(port + 10); p++)
		{
			addr.sin_port = htons(p);
			if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR)
			{
				bound = p;
				break;
			}
			char m[128];
			snprintf(m, sizeof(m), "web: port %u mesgul (err=%d) — sonraki port deneniyor", p, WSAGetLastError());
			RadarDbg(m);
		}
		if (!bound || listen(listener, 32) == SOCKET_ERROR)
		{
			RadarDbg("web: 31337-31346 arasi hicbir port acilamadi — vazgecildi");
			closesocket(listener);
			return;
		}

		{
			char m[96];
			snprintf(m, sizeof(m), "web: dinlemede (LISTEN) 0.0.0.0:%u — tunnel origin hazir", bound);
			RadarDbg(m);
		}

		// /ws push hatti KALDIRILDI: baglanti basina std::thread yaratmak oyun icinde
		// loader-lock deadlock'ine dusuyor (Vanguard thread bildirimleri) — ilk sayfa
		// yukleniyor, sonraki istekler sonsuza kadar asili kaliyordu. Artik HTTP istekleri
		// ASLA thread yaratmadan, sirali islenir; sayfa /data polling ile akar.

		char url[64];
		snprintf(url, sizeof(url), "http://127.0.0.1:%u", port);
		RadarPrintf("[web] radar local: %s\n", url);
		{
			char m[160];
			snprintf(m, sizeof(m), "web: YEREL RADAR -> http://127.0.0.1:%u  (tunnel calismazsa bunu ac)", bound);
			RadarDbg(m);
		}
		RadarPrintf("[web] /maps (liste) /map?name=X (minimap) /calib (paylasilan kalibrasyon)\n");
		PrintLanIp(bound);
		// Tarayiciyi burada ACMIyoruz; cloudflared public URL (trycloudflare.com) hazir olunca
		// cloudflared.cpp icinde otomatik acilir — paylasilan link neyse o acilir.

		// cloudflared quick tunnel -> herkese acik trycloudflare.com URL'si
		cf::StartTunnel(bound);

		for (;;)
		{
			// ---- WSAPoll event loop: thread YOK, preconnect yarisi YOK ----
			// Tarayicilar bos "preconnect" soketleri acar; sirali sunucuda bunlar gercek
			// istekleri aclikta birakiyordu. Bu mimaride bos soketler poll setinde
			// sessizce bekler - tarayici ne zaman veri gonderirse o an islenir.
			u_long nb = 1;
			ioctlsocket(listener, FIONBIO, &nb);

			struct Conn
			{
				SOCKET s;
				std::string inbuf;
				std::string outbuf;
				size_t sent = 0;
				bool writing = false;
				ULONGLONG act = 0; // son aktivite
			};
			std::vector<Conn> conns;
			std::vector<pollfd> fds;

			RadarDbg("web: event loop basladi (WSAPoll, thread'siz)");

			for (;;)
			{
				fds.clear();
				pollfd lf{}; lf.fd = listener; lf.events = POLLRDNORM;
				fds.push_back(lf);
				for (const Conn& c : conns)
				{
					pollfd f{}; f.fd = c.s;
					f.events = c.writing ? POLLWRNORM : POLLRDNORM;
					fds.push_back(f);
				}
				const int nready = WSAPoll(fds.data(), (ULONG)fds.size(), 200);
				const ULONGLONG nowMs = GetTickCount64();

				if (nready > 0 && (fds[0].revents & POLLRDNORM))
				{
					for (;;)
					{
						SOCKET cs = accept(listener, nullptr, nullptr);
						if (cs == INVALID_SOCKET) break;
						if (conns.size() >= 64) { closesocket(cs); continue; }
						u_long cb = 1; ioctlsocket(cs, FIONBIO, &cb);
						BOOL nd = TRUE; setsockopt(cs, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof(nd));
						Conn cc; cc.s = cs; cc.act = nowMs;
						conns.push_back(cc);
						g_clients++;
					}
				}

				for (size_t i = 0; i < conns.size(); )
				{
					Conn& c = conns[i];
					bool drop = false;
					const short re = (i + 1 < fds.size()) ? fds[i + 1].revents : 0;

					if (re & (POLLHUP | POLLERR | POLLNVAL)) drop = true;
					else if (!c.writing && (re & POLLRDNORM))
					{
						char buf[8192];
						const int n = recv(c.s, buf, sizeof(buf), 0);
						if (n <= 0) drop = true;
						else
						{
							c.act = nowMs;
							c.inbuf.append(buf, buf + n);
							if (c.inbuf.size() > 65536) drop = true;
							else if (c.inbuf.find("\r\n\r\n") != std::string::npos)
							{
								size_t need = 0;
								const char* cl = FindCI(c.inbuf.c_str(), "Content-Length:");
								if (cl)
								{
									const int clen = atoi(cl + 15);
									const size_t hdrEnd = c.inbuf.find("\r\n\r\n") + 4;
									if (clen > 0 && clen < 65536)
										need = hdrEnd + (size_t)clen;
								}
								if (need == 0 || c.inbuf.size() >= need)
								{
									c.outbuf = BuildResponse(c.inbuf);
									c.writing = true;
									c.sent = 0;
								}
							}
							else if (nowMs - c.act > 10000) drop = true;
						}
					}
					else if (c.writing && (re & POLLWRNORM))
					{
						const int n = send(c.s, c.outbuf.data() + c.sent, (int)(c.outbuf.size() - c.sent), 0);
						if (n <= 0) drop = true;
						else
						{
							c.act = nowMs;
							c.sent += (size_t)n;
							if (c.sent >= c.outbuf.size()) drop = true; // Connection: close
						}
					}
					else if (nowMs - c.act > 15000) drop = true; // bos preconnect temizligi

					if (drop)
					{
						closesocket(c.s);
						conns.erase(conns.begin() + i);
						g_clients--;
					}
					else i++;
				}
			}
		}
	}

	bool StartWebServer(uint16_t port)
	{
		std::thread(ServerLoop, port).detach();
		return true;
	}
}


