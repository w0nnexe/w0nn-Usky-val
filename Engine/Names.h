#pragma once
#include "Class/Class.h"
#include "Offsets.h"
#include "../gui/gui.h"
#include <string>
#include <cstring>
#include <cstdio>

// FName cozumleme — FNamePool uzerinden UE4.23+ blok duzeni:
//   entry = Pool.Blocks[id >> 16] + 2 * (id & 0xFFFF)
// Kesin olmayan degiskenler (pool pointer mi / dogrudan mi, Blocks[] ofseti, header ofseti,
// icerik duz metin mi XOR'lu mu) ILK KULLANIMDA runtime kalibrasyonuyla secilir
// (VisCalibrate/mesh-tarama deseni): gelen id icin cozulen string "isim benzeri" ise
// (yazdirilabilir, [A-Za-z0-9_] agirlikli, 1..64) kombinasyon gecerli sayilir ve
// onbelleklenir. Hicbiri tutmazsa mod kendini kapatir (-2) ve "[====]" doner — crash YOK.
//
// XOR modu: decrypt_xor_keys(NameKey degeri, NameState[7]) ile turetilen 64-bit anahtar
// entry'nin ilk 8 byte bloklarina uygulanir (header dahil). Semada blok-basi anahtar
// rotasyonu varsa dogrulama basarisiz olur -> radar_debug.log'a not duser, orijinal
// GetName cagiran koddan detay istenir.
namespace names
{
	struct Layout
	{
		int mode = -1;        // -1 kalibre edilmedi, -2 devre disi, 0 duz metin, 1 xor
		int blocksOff = 0x8;  // FNamePool icinde Blocks[] ofseti (stock UE: 0x8)
		int hdrOff = 0x0;     // entry icinde FNameEntryHeader ofseti (0=stock, 4=Class.h FNameEntry)
		uint64_t pool = 0;    // cozumlenen pool adresi (Blocks dizisinin adresi)
	};
	inline Layout s_layout;

	inline bool LooksLikeName(const char* s, int len)
	{
		if (len < 1 || len > 64) return false;
		int good = 0;
		for (int i = 0; i < len; i++)
		{
			unsigned char c = (unsigned char)s[i];
			if (c == 0) return false;
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				c == '_' || c == '.' || c == '-')
				good++;
		}
		return good * 10 >= len * 6; // en az %60 isim-karakteri
	}

	// entry'den header+ismi coz; basariysa out doldurur
	inline bool DecodeEntry(uint8_t* entry, int hdrOff, bool xorMode, std::string& out)
	{
		if (!entry || IsBadReadPtr(entry + hdrOff, 0x48)) return false; // tam okunan bolge

		uint8_t buf[0x48];
		memcpy(buf, entry + hdrOff, sizeof(buf));

		if (xorMode)
		{
			if (IsBadReadPtr((void*)(ImageBase + NameState), 0x40)) return false;
			const uint32_t key32 = *(const uint32_t*)(ImageBase + NameKey);
			const uint64_t xk = decrypt_xor_keys(key32, (const uint64_t*)(ImageBase + NameState));
			for (size_t i = 0; i + 8 <= sizeof(buf); i += 8)
				*(uint64_t*)(buf + i) ^= xk;
		}

		const uint16_t hdr = *(const uint16_t*)buf;
		const int len = hdr >> 1;
		const bool wide = (hdr & 1) != 0;
		if (wide || len < 1 || len > 64) return false; // ANSI yeter — motor nesneleri dar kume

		const char* p = (const char*)(buf + 2);
		if (!LooksLikeName(p, len)) return false;

		out.assign(p, len);
		return true;
	}

	inline uint8_t* EntryFor(uint64_t pool, int blocksOff, uint32_t id)
	{
		const uint32_t block = id >> 16;
		if (block >= 8192) return nullptr;
		if (IsBadReadPtr((void*)(pool + blocksOff + (size_t)block * 8), 8)) return nullptr;
		uint8_t* blk = *(uint8_t**)(pool + blocksOff + (size_t)block * 8);
		if (!blk) return nullptr; // giris bolgesinin guard'i DecodeEntry'de (0x48) — blok boyutu varsayma YOK
		return blk + (size_t)(id & 0xFFFF) * 2;
	}

	// Tum duzen adaylarini dener; ilk gecerli kombinasyonu onbellekler
	inline bool Calibrate(uint32_t id, std::string& firstOut)
	{
		uint64_t poolCands[4];
		{
			uint64_t p0 = 0, p2 = 0;
			if (!IsBadReadPtr((void*)(ImageBase + offs::core::fname_pool), 8))
				p0 = *(uint64_t*)(ImageBase + offs::core::fname_pool);
			if (!IsBadReadPtr((void*)(ImageBase + NameAddr), 8)) // eski nesil sabit de aday
				p2 = *(uint64_t*)(ImageBase + NameAddr);
			poolCands[0] = p0;
			poolCands[1] = ImageBase + offs::core::fname_pool;
			poolCands[2] = p2;
			poolCands[3] = ImageBase + NameAddr;
		}

		const int blocksOffs[3] = { 0x8, 0x10, 0x0 };
		const int hdrOffs[2] = { 0x0, 0x4 };
		const bool modes[2] = { false, true };

		for (int pc = 0; pc < 4; pc++)
		{
			const uint64_t pool = poolCands[pc];
			if (!pool || IsBadReadPtr((void*)pool, 0x20)) continue;
			for (int bc = 0; bc < 3; bc++)
			{
				uint8_t* entry = EntryFor(pool, blocksOffs[bc], id);
				if (!entry) continue;
				for (int hc = 0; hc < 2; hc++)
				{
					for (int mc = 0; mc < 2; mc++)
					{
						std::string s;
						if (!DecodeEntry(entry, hdrOffs[hc], modes[mc], s)) continue;
						s_layout = { mc, blocksOffs[bc], hdrOffs[hc], pool };
						firstOut = s;
						char m[160];
						snprintf(m, sizeof(m), "names: kalibre edildi mode=%s blocks=+0x%X hdr=+0x%X ilk='%s'",
							mc ? "xor" : "plain", blocksOffs[bc], hdrOffs[hc], s.c_str());
						RadarDbg(m);
						return true;
					}
				}
			}
		}
		return false;
	}

	inline std::string Get(uint32_t id)
	{
		if (s_layout.mode == -2) return "[====]";
		if (!id) return std::string();

		if (s_layout.mode < 0)
		{
			std::string first;
			if (!Calibrate(id, first))
			{
				static ULONGLONG nextLog = 0;
				if (GetTickCount64() > nextLog)
				{
					nextLog = GetTickCount64() + 15000;
					RadarDbg("names: pool duzeni kalibre edilemedi — GetName devre disi");
				}
				s_layout.mode = -2;
				return "[====]";
			}
		}

		uint8_t* entry = EntryFor(s_layout.pool, s_layout.blocksOff, id);
		std::string s;
		if (!entry || !DecodeEntry(entry, s_layout.hdrOff, s_layout.mode == 1, s))
			return "[====]";
		return s;
	}

	// "BasePistol_C_2147483647" -> "BasePistol_C" (instance sayi ekini kirp)
	inline std::string CleanInstanceName(const std::string& in)
	{
		size_t last = in.find_last_of('_');
		if (last != std::string::npos && last > 0 && last + 1 < in.size())
		{
			bool digits = true;
			for (size_t i = last + 1; i < in.size(); i++)
				if (in[i] < '0' || in[i] > '9') { digits = false; break; }
			if (digits) return in.substr(0, last);
		}
		return in;
	}
}
