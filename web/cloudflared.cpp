#include "cloudflared.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cctype>
#include <string>
#include <thread>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

 void RadarDbg(const char* msg); // dllmain.cpp icinde tanimli (global scope)
 void RadarPrintf(const char* fmt, ...); // printf yerine — yalniz log dosyasina yazar

namespace cf
{
	static bool FileExists(const std::string& p)
	{
		return !p.empty() && p.size() < MAX_PATH && GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	static const char* kCfExeNames[] = { "cloudflared.exe", "cloudflared-windows-amd64.exe" };

	static std::string FindInDir(const std::string& dir)
	{
		if (dir.empty()) return {};
		std::string base = dir;
		if (base.back() != '\\' && base.back() != '/') base += '\\';
		for (const char* name : kCfExeNames)
		{
			std::string cand = base + name;
			if (FileExists(cand)) return cand;
		}
		return {};
	}

	static std::string GetModuleDir()
	{
		char path[MAX_PATH] = {};
		HMODULE self = nullptr;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&GetModuleDir, &self) && self && GetModuleFileNameA(self, path, MAX_PATH))
		{
			std::string s(path);
			size_t slash = s.find_last_of("\\/");
			if (slash != std::string::npos) return s.substr(0, slash + 1);
		}
		return {};
	}

	// Indirilenler klasoru (redirect edilmis Downloads dahil dogru sekilde cozer)
	static std::string GetDownloadsDir()
	{
		PWSTR down = nullptr;
		char ansi[MAX_PATH] = {};
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &down)) && down)
		{
			int n = WideCharToMultiByte(CP_ACP, 0, down, -1, ansi, sizeof(ansi), NULL, NULL);
			CoTaskMemFree(down);
			if (n > 0) return std::string(ansi);
		}
		return {};
	}

	static std::string FindCloudflared()
	{
		// 1) DLL'nin bulundugu klasor
		std::string r = FindInDir(GetModuleDir());
		if (!r.empty()) return r;

		// 2) Indirilenler klasoru (statik path)
		r = FindInDir(GetDownloadsDir());
		if (!r.empty())
		{
			RadarPrintf("[web] cloudflared bulundu (Indirilenler): %s\n", r.c_str());
			return r;
		}

		// 3) PATH
		char found[MAX_PATH] = {};
		DWORD n = SearchPathA(NULL, "cloudflared.exe", NULL, MAX_PATH, found, NULL);
		if (n && n < MAX_PATH)
			return std::string(found);

		return {};
	}

	// log metninden ilk https://....trycloudflare.com adresini cikarir.
	// NOT: fallback YOK — cloudflared'in "terms" satirindaki www.cloudflare.com gibi
	// adresleri yanlislikla public URL sanip tarayici acmasin.
	static std::string ExtractUrl(const std::string& log)
	{
		size_t pos = 0;
		while ((pos = log.find("https://", pos)) != std::string::npos)
		{
			size_t s = pos + 8, e = s;
			while (e < log.size())
			{
				char c = log[e];
				if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_' || c == ':'))
					break;
				e++;
			}
			std::string url = log.substr(s, e - s);
			if (url.find("trycloudflare.com") != std::string::npos)
				return url;
			pos = e;
		}
		return {};
	}

	// Oyun kapansa da onceki session'in cloudflared'i yasar ve log dosyasini acik tutar
	// (sonraki acilista sharing violation yiyorduk). Once eskileri temizle.
	static void KillStaleCloudflared()
	{
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snap == INVALID_HANDLE_VALUE) return;
		PROCESSENTRY32W pe = {};
		pe.dwSize = sizeof(pe);
		int killed = 0;
		if (Process32FirstW(snap, &pe))
		{
			do {
				if (_wcsicmp(pe.szExeFile, L"cloudflared.exe") == 0 ||
					_wcsicmp(pe.szExeFile, L"cloudflared-windows-amd64.exe") == 0)
				{
					HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
					if (h)
					{
						// yalnizca kendi kopyamizi oldur (bizim klasorden baslayan), baskasinin isine karisma
						char img[MAX_PATH] = {};
						DWORD n = MAX_PATH;
						bool ours = true;
						if (QueryFullProcessImageNameA(h, 0, img, &n))
						{
							std::string dir = GetModuleDir();
							ours = strnicmp(img, dir.c_str(), dir.size()) == 0;
						}
						if (ours && TerminateProcess(h, 0))
							killed++;
						CloseHandle(h);
					}
				}
			} while (Process32NextW(snap, &pe));
		}
		CloseHandle(snap);
		if (killed)
		{
			char m[64];
			snprintf(m, sizeof(m), "cloudflared: %d eski surec temizlendi", killed);
			RadarDbg(m);
			Sleep(1500);
		}
	}

	static void TunnelThread(uint16_t port)
	{
		RadarDbg("cloudflared: tunel thread basladi");
		KillStaleCloudflared();
		std::string exe = FindCloudflared();
		if (exe.empty())
		{
			RadarPrintf("[web] cloudflared.exe bulunamadi -> public URL yok.\n");
			RadarPrintf("[web] indir: https://github.com/cloudflare/cloudflared/releases (DLL yanina koy)\n");
			RadarDbg("cloudflared: exe BULUNAMADI — Hack.dll ile AYNI klasore koy (veya PATH)");
			return;
		}
		RadarPrintf("[web] cloudflared: %s\n", exe.c_str());
		{
			char m[420];
			snprintf(m, sizeof(m), "cloudflared bulundu: %s", exe.c_str());
			RadarDbg(m);
		}

		// log dosyasi SESSION'A OZEL: process id icerir, asla sharing violation olmaz
		char tmpPath[MAX_PATH] = {}, logPath[MAX_PATH] = {};
		GetTempPathA(MAX_PATH, tmpPath);
		snprintf(logPath, MAX_PATH, "%scf_radar_tunnel_%u.log", tmpPath, GetCurrentProcessId());

		SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
		HANDLE hLog = CreateFileA(logPath, GENERIC_WRITE, FILE_SHARE_READ, &sa,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hLog == INVALID_HANDLE_VALUE)
		{
			char m[96];
			snprintf(m, sizeof(m), "cloudflared: log acilamadi err=%lu", GetLastError());
			RadarDbg(m);
			return;
		}

		char cmd[1024] = {};
		snprintf(cmd, sizeof(cmd), "\"%s\" tunnel --url http://127.0.0.1:%u --no-autoupdate", exe.c_str(), port);

		STARTUPINFOA si = {};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = hLog;
		si.hStdError = hLog;
		PROCESS_INFORMATION pi = {};
		if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
		{
			RadarPrintf("[web] cloudflared baslatilamadi (err=%lu)\n", GetLastError());
			char m[64];
			snprintf(m, sizeof(m), "cloudflared: baslatilamadi err=%lu", GetLastError());
			RadarDbg(m);
			CloseHandle(hLog);
			return;
		}
		CloseHandle(pi.hThread);

		RadarPrintf("[web] cloudflared calisiyor, tunel URL bekleniyor (max 90s)...\n");
		RadarDbg("cloudflared: calisiyor, URL bekleniyor (max 90s)");
		for (int i = 0; i < 180; i++)
		{
			Sleep(500);

			HANDLE h = CreateFileA(logPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, OPEN_EXISTING, NULL, NULL);
			if (h == INVALID_HANDLE_VALUE) continue;

			LARGE_INTEGER sz = {};
			if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0)
			{
				SetFilePointer(h, 0, NULL, FILE_BEGIN);
				static thread_local char buf[16384];
				DWORD rd = 0;
				if (ReadFile(h, buf, sizeof(buf) - 1, &rd, NULL) && rd > 0)
				{
					buf[rd] = 0;
					std::string url = ExtractUrl(std::string(buf, rd));
					if (!url.empty())
					{
						char full[512];
						snprintf(full, sizeof(full), "https://%s", url.c_str());
						RadarPrintf("[web] ============================================\n");
						RadarPrintf("[web]  PUBLIC RADAR: %s\n", full);
						RadarPrintf("[web] ============================================\n");
						char m[300];
						snprintf(m, sizeof(m), "cloudflared: PUBLIC URL hazir -> %s ( Manuel ac: log'daki link )", full);
						RadarDbg(m);
						// ShellExecuteA KALDIRILDI: oyun sureci icinden shell/COM baslatmak
						// loader-lock deadlock ile TUM SURECI dondurabiliyordu — radar sayfasi
						// yukleniyor gibi gorunup veri akisi donuyordu. Link radar_debug.log'da.
						CloseHandle(h);
						CloseHandle(hLog);
						CloseHandle(pi.hProcess);
						return;
					}
				}
			}
			CloseHandle(h);
		}

		RadarPrintf("[web] tunel URL alinamadi, log: %s\n", logPath);
		RadarDbg("cloudflared: 90s icinde URL alinamadi (log: %TEMP%\\cf_radar_tunnel.log)");
		CloseHandle(hLog);
		CloseHandle(pi.hProcess);
	}

	void StartTunnel(uint16_t port)
	{
		std::thread(TunnelThread, port).detach();
	}
}
