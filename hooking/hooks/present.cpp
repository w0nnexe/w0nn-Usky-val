#include "present.h"
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_win32.h"
#include "../../imgui/imgui_impl_dx11.h"
#include "../../gui/gui.h"
#pragma comment(lib, "d3d11.lib")
#pragma warning( disable : 6387 )
#pragma warning( disable : 4005 )


LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (uMsg == WM_KEYUP && wParam == MenuKey) // F1 — gui.h'deki MenuKey ile ayni
	{
		isMenuOpen ^= 1;
	}

	// Menü kapalıyken de ImGui'e mesajı iletiyoruz ki WM_SIZE (fullscreen windowed gecisi)
	// sirasinda DisplaySize guncellensin; menü açıkken input mesajlarını yutuyoruz.
	// Guard: ImGui context ilk Present'e kadar yok — ondan once mesaj gelirse ImGui'ye iletmeyiz.
	if (ImGUI_Initalized)
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

	if (isMenuOpen)
	{
		switch (uMsg)
		{
		case WM_MOUSEMOVE: case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
		case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
		case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
		case WM_CHAR: case WM_INPUT:
			return true;
		default:
			break;
		}
	}

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}



// ResizeBuffers (fullscreen windowed / cozunurluk degisimi) sirasinda backbuffer yeniden olusturulur.
// Eski mainRenderTargetView gecersiz kalir -> dangling pointer crash. Burada yeniden olusturuyoruz.
HRESULT __stdcall GUIFunction::hkResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
	// RTV pipeline'a bagliysa Release tam serbest kalmasin ve ResizeBuffers fail etmesin diye once unbind et
	if (pContext)
		pContext->OMSetRenderTargets(0, nullptr, nullptr);

	if (mainRenderTargetView)
	{
		mainRenderTargetView->Release();
		mainRenderTargetView = nullptr;
	}

	HRESULT hr = oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

	if (SUCCEEDED(hr) && pDevice)
	{
		ID3D11Texture2D* pBackBuffer = nullptr;
		if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer)) && pBackBuffer)
		{
			pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
			pBackBuffer->Release();
		}
	}

	return hr;
}



// OnGUI
HRESULT __stdcall GUIFunction::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	// PANIC: passthrough — hicbir sey cizme, sadece orijinali cagir
	if (g_panic)
	{
		// END tusu ile panic aktif — bir kez mutex serbest birak + vtable restore
		static bool s_panicDone = false;
		if (!s_panicDone)
		{
			s_panicDone = true;
			// PostRender vtable restore (orijinal fonksiyona geri)
			if (g_hookedViewportForPanic && g_origPostRender)
			{
				__try
				{
					void*** vt = (void***)g_hookedViewportForPanic;
					(*vt)[99] = g_origPostRender;
				}
				__except (1) { }
			}
			// WndProc restore — KRITIK: ImGui WndProc hook'u geri alinmazsa
			// oyun TUM input'u kaybeder (mouse/klavye donuyor, ses gelir)
			if (oWndProc && window)
			{
				SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);
				oWndProc = nullptr;
			}
			// Mutex'i TAMAMEN yok et
			if (g_onceMutex)
			{
				ReleaseMutex((HANDLE)g_onceMutex);
				CloseHandle((HANDLE)g_onceMutex);
				g_onceMutex = nullptr;
			}
			RadarDbg("PANIC: hooklar pasif, WndProc restore, mutex yok — yeniden inject yap");
		}
		return oPresent(pSwapChain, SyncInterval, Flags);
	}

	if (!ImGUI_Initalized)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			InitalizeImGUI(); // ImGUI Başlatma Fonksiyonu
			ImGUI_Initalized = true;
			RadarDbg("present: ilk kare — ImGui init tamam");
		}
		else
		{
			return oPresent(pSwapChain, SyncInterval, Flags); // Başarısız olur ise orijinal koda yönlendir.
		}
	}

	// Guvenlik agi: RTV yoksa (resize sonrasi bizim hook yakalanmamis olabilir) yeniden olustur
	if (!mainRenderTargetView && pDevice)
	{
		ID3D11Texture2D* pBackBuffer = nullptr;
		if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer)) && pBackBuffer)
		{
			pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRenderTargetView);
			pBackBuffer->Release();
		}
	}

	// Hala gecerli bir RTV/context yoksa cizmeden orijinale don -> crash onlenir
	if (!pContext || !mainRenderTargetView)
	{
		return oPresent(pSwapChain, SyncInterval, Flags);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::GetIO().MouseDrawCursor = isMenuOpen;
	Render(); // Menü kodu bu fonksiyonun içerisine yazılacak.
	ImGui::Render();
	pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	return oPresent(pSwapChain, SyncInterval, Flags); // Orijinal koda yönlendir.
}