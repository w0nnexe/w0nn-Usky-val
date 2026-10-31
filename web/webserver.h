#pragma once
#include <cstdint>
#include <string>

namespace web
{
	// Gomulu HTTP sunucusunu arka planda baslatir (thread detach), hemen geri doner.
	//   "/"            -> 2D ustten radar sayfasi (harita modu + kalibrasyon sihirbazi)
	//   "/ws"          -> WebSocket push (~120ms, RFC6455) — sayfa bunu kullanir;
	//                     eski DLL/engelli ag durumunda sayfa /data polling'e duser
	//   "/data"        -> JSON (globalData + local player, oyun thread'inden mutex ile)
	//   "/maps"        -> desteklenen harita isimleri (JSON array)
	//   "/mapmeta"     -> otomatik kalibrasyon verileri (world cm -> minimap UV afin, valorant-api.com)
	//   "/map?name=X"  -> minimap PNG; once disk/bellek cache, yoksa media.valorant-api.com'dan
	//                     indirir (bilinmeyen isim icin wiki.playvalorant.com kalibi dener)
	//   "/calib" GET   -> paylasilan kalibrasyon JSON (web_radar_calib.json)
	//   "/calib" POST  -> kalibrasyon JSON'u kaydet (harita basina affine [a,b,c,d,e,f])
	bool StartWebServer(uint16_t port);

	// level adini kMaps tablosunda eslestirir; bilinen afin varsa kopyalar ve true doner
	bool GetMapAffine(const std::string& rawName, double out[6]);
}
