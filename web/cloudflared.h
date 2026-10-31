#pragma once
#include <cstdint>

namespace cf
{
	// cloudflared quick tunnel baslatir; trycloudflare.com URL'sini konsola basar (async).
	// cloudflared.exe (veya cloudflared-windows-amd64.exe) sirasiyla:
	// DLL klasoru -> Indirilenler (Downloads) -> PATH seklinde aranir.
	void StartTunnel(uint16_t port);
}
