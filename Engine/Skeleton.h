#pragma once
#include "Class/Class.h"
#include "Offsets.h"
#include "../gui/gui.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// Saf bellek okumali iskelet hatti. ProcessEvent KULLANILMAZ (eski BoneMatrix native
// cagrisi render thread'i ile yarisiyordu — dllmain.cpp ust not).
//
// Guvenlik deseni: HICBIR offset'e guvenilmez — liste adaylari once denenir, tutmazsa
// yapisal tarama devreye girer:
//   - Mesh pointer adaylari: ACharacter + 0x300..0x600 (listenin 0x4F0'u dahil)
//   - Kemik TArray'i: {Data ptr, Num, Max} deseni — listenin 0x760/0x768'i TArray
//     duzenine zaten uyar (Num = Data+8); tutmazsa 0x100..0xA00 desen taramasi
//   - ComponentToWorld: listenin 0x300'u once; tutmazsa 0x100..0x400 taramasi
//     (quat normu ~1 + translasyon aktore yakin sartiyla)
//   - Kok kemik (idx 0) aktor konumuna yakin olmali (<3m) — yanlis kombinasyon burada elenir
// Kemik sayisi penceresi 20..512 (rig 96'dan buyuk olabilir; kullanim MAX_BONES'a kirpilir).
// Bulunamazsa ozellik sessizce kapali kalir; radar_debug.log asama bilgisini yazar.
namespace skel
{
	constexpr int BC_MIN = 20;
	constexpr int BC_MAX = 512;

	struct FTransformD
	{
		double qx, qy, qz, qw;
		double tx, ty, tz;
		double sx, sy, sz;
	};

	struct MeshInfo { int off; size_t stride; int arrOff; int cntOff; int c2wOff; };

	inline bool FiniteVec(const FVector& v)
	{
		return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z);
	}

	inline FVector QuatRotate(const FTransformD& q, const FVector& v)
	{
		const double xx = q.qx * q.qx, yy = q.qy * q.qy, zz = q.qz * q.qz;
		const double xy = q.qx * q.qy, xz = q.qx * q.qz, yz = q.qy * q.qz;
		const double wx = q.qw * q.qx, wy = q.qw * q.qy, wz = q.qw * q.qz;
		return FVector(
			(1.0 - 2.0 * (yy + zz)) * v.X + (2.0 * (xy - wz)) * v.Y + (2.0 * (xz + wy)) * v.Z,
			(2.0 * (xy + wz)) * v.X + (1.0 - 2.0 * (xx + zz)) * v.Y + (2.0 * (yz - wx)) * v.Z,
			(2.0 * (xz - wy)) * v.X + (2.0 * (yz + wx)) * v.Y + (1.0 - 2.0 * (xx + yy)) * v.Z);
	}

	// stride 0x50: LWC double FTransform (quat 0x00, trans 0x20, scale 0x38)
	// stride 0x30: float FTransform (quat 0x00, trans 0x10, scale 0x1C; son 8 byte padding)
	inline bool ReadXform(const uint8_t* p, size_t stride, FTransformD& out)
	{
		if (!p || IsBadReadPtr(p, stride)) return false;
		if (stride >= 0x50)
		{
			memcpy(&out.qx, p + 0x00, 8); memcpy(&out.qy, p + 0x08, 8);
			memcpy(&out.qz, p + 0x10, 8); memcpy(&out.qw, p + 0x18, 8);
			memcpy(&out.tx, p + 0x20, 8); memcpy(&out.ty, p + 0x28, 8); memcpy(&out.tz, p + 0x30, 8);
		}
		else
		{
			float f[10];
			memcpy(f, p, 40);
			out.qx = f[0]; out.qy = f[1]; out.qz = f[2]; out.qw = f[3];
			out.tx = f[4]; out.ty = f[5]; out.tz = f[6];
		}
		return true;
	}

	// aday ComponentToWorld: quat normu ~1 ve translasyon aktore yakin (<50m) olmali
	inline bool TryC2W(uint8_t* comp, int cwOff, const FVector& actorLoc, FTransformD& c2w, size_t& stride)
	{
		const size_t strides[2] = { 0x50, 0x30 };
		for (int s = 0; s < 2; s++)
		{
			FTransformD t{};
			if (!ReadXform(comp + cwOff, strides[s], t)) continue;
			const double qn = std::sqrt(t.qx * t.qx + t.qy * t.qy + t.qz * t.qz + t.qw * t.qw);
			if (qn < 0.7 || qn > 1.3) continue;
			const FVector tr{ t.tx, t.ty, t.tz };
			if (!FiniteVec(tr)) continue;
			if (std::fabs(tr.X - actorLoc.X) > 5000.0 || std::fabs(tr.Y - actorLoc.Y) > 5000.0 ||
				std::fabs(tr.Z - actorLoc.Z) > 5000.0) continue;
			c2w = t;
			stride = strides[s];
			return true;
		}
		return false;
	}

	// kok kemigi c2w ile dunyaya tasi ve aktore yakınlığını kontrol et (<3m)
	inline bool RootNear(const uint8_t* arr, size_t stride, const FTransformD& c2w, const FVector& actorLoc)
	{
		FTransformD rt{};
		if (!ReadXform(arr, stride, rt)) return false;
		FVector w = QuatRotate(c2w, FVector(rt.tx, rt.ty, rt.tz));
		w = FVector(w.X + c2w.tx, w.Y + c2w.ty, w.Z + c2w.tz);
		if (!FiniteVec(w)) return false;
		return std::fabs(w.X - actorLoc.X) <= 300.0 &&
			std::fabs(w.Y - actorLoc.Y) <= 300.0 &&
			std::fabs(w.Z - actorLoc.Z) <= 300.0;
	}

	// ilk 8 kemigin en az 4'u dunyada aktore yakin olmali (<5m) — yanlis diziyi eler:
	// yanlis TArray'in ilk elemani tesadufen yakinda olabilir ama geri kalani dagilir.
	inline int NearCount(const uint8_t* arr, size_t stride, const FTransformD& c2w, const FVector& actorLoc, int lim)
	{
		if (lim > 8) lim = 8;
		int cnt = 0;
		for (int i = 0; i < lim; i++)
		{
			FTransformD bt{};
			if (!ReadXform(arr + (size_t)i * stride, stride, bt)) continue;
			FVector w = QuatRotate(c2w, FVector(bt.tx, bt.ty, bt.tz));
			w = FVector(w.X + c2w.tx, w.Y + c2w.ty, w.Z + c2w.tz);
			if (!FiniteVec(w)) continue;
			if (std::fabs(w.X - actorLoc.X) < 500.0 && std::fabs(w.Y - actorLoc.Y) < 500.0 &&
				std::fabs(w.Z - actorLoc.Z) < 500.0)
				cnt++;
		}
		return cnt;
	}

	// hareket dogrulamasi ile elenen c2w ofsetleri (relative-transform tuzagi)
	inline int s_cwBlack[8] = {};
	inline bool CwBanned(int off)
	{
		if (!off) return false;
		for (int i = 0; i < 8; i++) if (s_cwBlack[i] == off) return true;
		return false;
	}
	inline void CwBan(int off)
	{
		if (!off || CwBanned(off)) return;
		for (int i = 0; i < 8; i++) if (s_cwBlack[i] == 0) { s_cwBlack[i] = off; return; }
	}

	// hizli yol: listenin bone(0x760/0x768) + c2w(0x300) offsetleri — liste guncelse burada biter
	inline bool QuickValidate(uint8_t* comp, const FVector& actorLoc, MeshInfo& out, int candOff)
	{
		if (!comp || IsBadReadPtr(comp, 0x780)) return false;
		if (CwBanned((int)offs::mesh::component_to_world)) return false;
		const int bc = *(const int*)(comp + offs::mesh::bone_count);
		if (bc < BC_MIN || bc > BC_MAX) return false;
		uint8_t* arr = *(uint8_t**)(comp + offs::mesh::bone_array);
		if (!arr) return false;
		FTransformD c2w{};
		size_t st = 0x50;
		if (!TryC2W(comp, (int)offs::mesh::component_to_world, actorLoc, c2w, st)) return false;
		if (IsBadReadPtr(arr, 0x50)) return false;
		if (!RootNear(arr, st, c2w, actorLoc)) return false;
		if (NearCount(arr, st, c2w, actorLoc, bc) < 4) return false;
		out = { candOff, st, (int)offs::mesh::bone_array, (int)offs::mesh::bone_count,
			(int)offs::mesh::component_to_world };
		return true;
	}

	// yapisal yol: bone TArray deseni + c2w ofset taramasi (comp tek guard ile 0xA10 kapali)
	inline bool ValidateComp(uint8_t* comp, const FVector& actorLoc, MeshInfo& out, int candOff)
	{
		if (!comp || IsBadReadPtr(comp, 0xA10)) return false;

		struct ArrCand { int arrOff, cntOff; };
		ArrCand arrCands[6];
		int nArr = 0;
		arrCands[nArr++] = { (int)offs::mesh::bone_array, (int)offs::mesh::bone_count };
		for (int o = 0x100; o <= 0xA00 - 16 && nArr < 6; o += 8)
		{
			if (o == (int)offs::mesh::bone_array) continue;
			uint8_t* d = *(uint8_t**)(comp + o);
			if (!d) continue;
			const int num = *(const int*)(comp + o + 8);
			const int mx = *(const int*)(comp + o + 12);
			if (num < BC_MIN || num > BC_MAX || mx < num || mx > 4096) continue;
			if (IsBadReadPtr(d, 0x50)) continue;
			arrCands[nArr++] = { o, o + 8 };
		}

		static ULONGLONG nextDiag = 0;
		const bool diag = GetTickCount64() > nextDiag;
		int diagStage = 0, diagBc = 0, diagArr = 0;

		for (int a = 0; a < nArr; a++)
		{
			uint8_t* arr = *(uint8_t**)(comp + arrCands[a].arrOff);
			if (!arr) continue;
			const int bc = *(const int*)(comp + arrCands[a].cntOff);
			if (diag && diagStage < 1) { diagStage = 1; diagBc = bc; diagArr = arrCands[a].arrOff; }

			FTransformD c2w{};
			size_t st = 0x50;
			int usedCw = -1;
			// once listenin c2w'si, sonra 0x100..0x400 taramasi (banlanmis ofsetler atlanir)
			if (!CwBanned((int)offs::mesh::component_to_world) &&
				TryC2W(comp, (int)offs::mesh::component_to_world, actorLoc, c2w, st))
				usedCw = (int)offs::mesh::component_to_world;
			else
			{
				for (int o = 0x100; o <= 0x400; o += 8)
				{
					if (o == (int)offs::mesh::component_to_world || CwBanned(o)) continue;
					if (TryC2W(comp, o, actorLoc, c2w, st)) { usedCw = o; break; }
				}
			}
			if (usedCw < 0) continue;
			if (diag && diagStage < 2) { diagStage = 2; diagArr = usedCw; }

			if (!IsBadReadPtr(arr, 0x50) && RootNear(arr, st, c2w, actorLoc) &&
				NearCount(arr, st, c2w, actorLoc, bc) >= 4)
			{
				out = { candOff, st, arrCands[a].arrOff, arrCands[a].cntOff, usedCw };
				return true;
			}
		}

		if (diag)
		{
			nextDiag = GetTickCount64() + 5000;
			char m[160];
			// stage 0: TArray deseni hic yok | 1: kemik var, c2w yok | 2: ikisi de var, kok kemik uzak
			snprintf(m, sizeof(m), "skel: aday 0x%X reddi (stage=%d bc=%d son=0x%X)",
				candOff, diagStage, diagBc, diagArr);
			RadarDbg(m);
		}
		return false;
	}

	// Mesh offset'ini bul/önbellekle. Donus: -1 = bu gecis bulunamadi, >0 = offset.
	// Hizli gecis her saniye; yapisal gecis pas basina 4 pointer (maliyet amortize).
	// Bulunamadiginda status -1 YALNIZCA gercek tarama sonrasi yazilir (throttle beklerken dokunulmaz).
	inline int MeshOffsetFor(ACharacter* actor, const FVector& actorLoc, MeshInfo& mi)
	{
		static MeshInfo cached{};
		static int cachedOff = 0;
		static ULONGLONG nextScan = 0;
		static int structCursor = 0x300;
		static FVector s_actorRef = {};
		static double s_c2wRefX = 0, s_c2wRefY = 0;
		static ULONGLONG refNext = 0;

		if (cachedOff > 0)
		{
			if (!IsBadReadPtr((uint8_t*)actor + cachedOff, 8))
			{
				uint8_t* comp = *(uint8_t**)((uint8_t*)actor + cachedOff);
				if (comp && QuickValidate(comp, actorLoc, cached, cachedOff))
				{
					// hareket dogrulamasi (~2 sn'de bir): aktor 3m+ yuruduyse GERCEK
					// ComponentToWorld translasyonu da takip etmeli; sabit kalan c2w
					// relative-transform'dur -> banla ve yeniden tara
					const ULONGLONG nowT = GetTickCount64();
					if (nowT > refNext)
					{
						refNext = nowT + 2000;
						FTransformD cw{};
						size_t stx = 0x50;
						if (TryC2W(comp, cached.c2wOff, actorLoc, cw, stx))
						{
							const double moved = std::fabs(actorLoc.X - s_actorRef.X) + std::fabs(actorLoc.Y - s_actorRef.Y);
							const double tracked = std::fabs(cw.tx - s_c2wRefX) + std::fabs(cw.ty - s_c2wRefY);
							if (moved > 300.0 && tracked < 150.0)
							{
								CwBan(cached.c2wOff);
								cachedOff = 0;
								RadarDbg("skel: c2w hareketi takip etmiyor (relative tuzagi) — ofset banlandi");
							}
							else
							{
								s_actorRef = actorLoc;
								s_c2wRefX = cw.tx; s_c2wRefY = cw.ty;
							}
						}
					}
					if (cachedOff > 0)
					{
						mi = cached;
						features::meshOffsetStatus = cachedOff;
						return cachedOff;
					}
				}
			}
		}

		if (GetTickCount64() < nextScan)
			return -1;
		nextScan = GetTickCount64() + 1000;

		if (IsBadReadPtr((uint8_t*)actor, 0x900)) // 0x300..0x600 tarama bolgesi tek guard
		{
			features::meshOffsetStatus = -1;
			return -1;
		}

		for (int off = 0x300; off <= 0x600; off += 8) // hizli yol: liste offsetleri
		{
			uint8_t* comp = *(uint8_t**)((uint8_t*)actor + off);
			if (!comp) continue;
			if (QuickValidate(comp, actorLoc, cached, off))
			{
				cachedOff = off;
				mi = cached;
				features::meshOffsetStatus = off;
				{
					FTransformD cw{}; size_t stx = 0x50;
					if (TryC2W(comp, cached.c2wOff, actorLoc, cw, stx))
					{ s_c2wRefX = cw.tx; s_c2wRefY = cw.ty; }
					s_actorRef = actorLoc;
				}
				char m[128];
				snprintf(m, sizeof(m), "skel: mesh bulundu 0x%X (stride=0x%zX arr=0x%X c2w=0x%X)",
					off, mi.stride, mi.arrOff, mi.c2wOff);
				RadarDbg(m);
				return off;
			}
		}

		for (int i = 0; i < 4; i++) // yapisal yol: pas basina 4 aday
		{
			const int off = structCursor;
			structCursor += 8;
			if (structCursor > 0x600) structCursor = 0x300;
			uint8_t* comp = *(uint8_t**)((uint8_t*)actor + off);
			if (!comp) continue;
			MeshInfo t{};
			if (ValidateComp(comp, actorLoc, t, off))
			{
				cachedOff = off;
				cached = t;
				mi = t;
				features::meshOffsetStatus = off;
				{
					FTransformD cw{}; size_t stx = 0x50;
					if (TryC2W(comp, t.c2wOff, actorLoc, cw, stx))
					{ s_c2wRefX = cw.tx; s_c2wRefY = cw.ty; }
					s_actorRef = actorLoc;
				}
				char m[128];
				snprintf(m, sizeof(m), "skel: mesh YAPISAL bulundu 0x%X (stride=0x%zX arr=0x%X c2w=0x%X)",
					off, t.stride, t.arrOff, t.c2wOff);
				RadarDbg(m);
				return off;
			}
		}

		features::meshOffsetStatus = -1; // gercek tarama gecisi basarisiz
		return -1;
	}

	// playerData.boneX/boneY doldurur. Otomatik mod: kafa = en yuksek kemik, cizgiler = MST.
	inline void FillBones(ACharacter* Player, APlayerController* PC, const FVector& actorLoc, playerData& data)
	{
		MeshInfo mi{};
		const int meshOff = MeshOffsetFor(Player, actorLoc, mi);
		if (meshOff <= 0) return;

		uint8_t* comp = *(uint8_t**)((uint8_t*)Player + meshOff);
		if (!comp) return;
		const int bc = *(const int*)(comp + mi.cntOff);
		if (bc < BC_MIN || bc > BC_MAX) return;
		uint8_t* arr = *(uint8_t**)(comp + mi.arrOff);
		if (!arr)
		{
			arr = *(uint8_t**)(comp + offs::mesh::bone_array_cache);
			if (!arr) return;
		}

		int n = bc;
		if (!features::boneDebug && n > features::boneMax) n = features::boneMax;
		if (n >= playerData::MAX_BONES) n = playerData::MAX_BONES - 1;
		if (n <= 0) return;

		if (IsBadReadPtr(arr, (size_t)n * mi.stride)) return;

		FTransformD c2w{};
		size_t st = mi.stride;
		if (!TryC2W(comp, mi.c2wOff, actorLoc, c2w, st)) return;
		mi.stride = st;

		const bool autoMode = features::boneAuto && !features::boneDebug;
		const int headIdx = features::boneHeadIdx < 0 ? 0 :
			(features::boneHeadIdx >= n ? n - 1 : features::boneHeadIdx);

		FVector wpos[playerData::MAX_BONES];
		for (int i = 0; i < n; i++)
		{
			bool need = features::boneDebug || autoMode || i == headIdx;
			if (!need)
				for (int p = 0; p < 16 && !need; p++)
					if (features::bonePairs[p][0] == i || features::bonePairs[p][1] == i) need = true;
			if (!need) continue; // boneX[i] ctor'dan -1

			FTransformD bt{};
			if (!ReadXform(arr + (size_t)i * mi.stride, mi.stride, bt)) continue;
			FVector w = QuatRotate(c2w, FVector(bt.tx, bt.ty, bt.tz));
			w = FVector(w.X + c2w.tx, w.Y + c2w.ty, w.Z + c2w.tz);
			if (!FiniteVec(w)) continue;
			wpos[i] = w;

			FVector2D scr;
			if (PC->W2S(w, scr) && std::isfinite(scr.X) && std::isfinite(scr.Y))
			{
				data.boneX[i] = (float)scr.X;
				data.boneY[i] = (float)scr.Y;
			}
		}

		data.boneCount = n;

		if (autoMode)
		{
			// kafa secimi: en yuksek 5 kemik arasindan ILK gecerli kume — kafa kemiginin
			// altinda boyun/omuz/kol kemikleri yigini olur (>=3 kemik 70cm icinde);
			// silah kemigi gibi tek basina yukselenler boylece elenir. Yatayda aktorden
			// uzak adaylar da (takilmis silah) gecersiz.
			int cand[5] = { -1, -1, -1, -1, -1 };
			for (int i = 0; i < n; i++)
			{
				if (data.boneX[i] < 0.f) continue;
				for (int c = 0; c < 5; c++)
				{
					if (cand[c] < 0 || wpos[i].Z > wpos[cand[c]].Z)
					{
						for (int m = 4; m > c; m--) cand[m] = cand[m - 1];
						cand[c] = i;
						break;
					}
				}
			}
			int head = -1;
			for (int c = 0; c < 5 && head < 0; c++)
			{
				const int b = cand[c];
				if (b < 0) continue;
				if (std::fabs(wpos[b].X - actorLoc.X) > 120.0 || std::fabs(wpos[b].Y - actorLoc.Y) > 120.0)
					continue;
				int below = 0;
				for (int j = 0; j < n && below < 3; j++)
				{
					if (j == b || data.boneX[j] < 0.f) continue;
					const double dz = wpos[b].Z - wpos[j].Z;
					if (dz >= 0.0 && dz < 70.0) below++;
				}
				if (below >= 3) head = b;
			}
			if (head < 0 && cand[0] >= 0) head = cand[0]; // kume yoksa en yuksek (eski davranis)

			if (head >= 0)
			{
				data.headX = data.boneX[head];
				data.headY = data.boneY[head];
				data.headWX = (float)wpos[head].X;
				data.headWY = (float)wpos[head].Y;
				data.headWZ = (float)wpos[head].Z;
			}

			struct Edge { double len; int a, b; };
			Edge edges[playerData::MAX_BONES];
			int nEdges = 0;

			bool inT[playerData::MAX_BONES] = {};
			double dist[playerData::MAX_BONES];
			int par[playerData::MAX_BONES];
			for (int i = 0; i < n; i++)
			{
				par[i] = -1;
				dist[i] = (data.boneX[i] >= 0.f) ? 1e30 : 1e31;
			}
			const int root = head >= 0 ? head : 0;
			if (data.boneX[root] >= 0.f)
			{
				inT[root] = true;
				for (int i = 0; i < n; i++)
					if (!inT[i] && data.boneX[i] >= 0.f)
					{
						const double dx = wpos[i].X - wpos[root].X, dy = wpos[i].Y - wpos[root].Y,
							dz = wpos[i].Z - wpos[root].Z;
						dist[i] = sqrt(dx * dx + dy * dy + dz * dz);
						par[i] = root;
					}
				for (int iter = 1; iter < n; iter++) // Prim MST
				{
					int best = -1;
					for (int v = 0; v < n; v++)
						if (!inT[v] && dist[v] < 1e30 && (best < 0 || dist[v] < dist[best])) best = v;
					if (best < 0) break;
					inT[best] = true;
					if (par[best] >= 0 && dist[best] < 150.0)
						edges[nEdges++] = { dist[best], par[best], best };
					for (int v = 0; v < n; v++)
						if (!inT[v] && data.boneX[v] >= 0.f)
						{
							const double dx = wpos[v].X - wpos[best].X, dy = wpos[v].Y - wpos[best].Y,
								dz = wpos[v].Z - wpos[best].Z;
							const double nd = sqrt(dx * dx + dy * dy + dz * dz);
							if (nd < dist[v]) { dist[v] = nd; par[v] = best; }
						}
				}
			}
			for (int i = 1; i < nEdges; i++)
			{
				const Edge e = edges[i];
				int j = i - 1;
				while (j >= 0 && edges[j].len > e.len) { edges[j + 1] = edges[j]; j--; }
				edges[j + 1] = e;
			}
			data.skeletonPairCount = nEdges < 32 ? nEdges : 32;
			for (int i = 0; i < data.skeletonPairCount; i++)
			{
				data.skeletonPairs[i][0] = edges[i].a;
				data.skeletonPairs[i][1] = edges[i].b;
			}
			features::skelEdges = data.skeletonPairCount;
			if (data.skeletonPairCount == 0)
			{
				static ULONGLONG zeroNext = 0;
				if (GetTickCount64() > zeroNext)
				{
					zeroNext = GetTickCount64() + 8000;
					RadarDbg("skel: MST kenar cikmadi — kemik dizisi hala supheli (dizin degisebilir)");
				}
			}
		}
		else if (data.boneX[headIdx] >= 0.f)
		{
			data.headX = data.boneX[headIdx];
			data.headY = data.boneY[headIdx];
			data.headWX = (float)wpos[headIdx].X;
			data.headWY = (float)wpos[headIdx].Y;
			data.headWZ = (float)wpos[headIdx].Z;
		}
	}
}
