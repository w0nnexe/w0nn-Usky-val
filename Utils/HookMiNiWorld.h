#pragma once
#include<iostream>

/*
* LogRiotGamesApiClient: Warning: [RiotGamesApi LogMessage] loyalty-v2: External process of product 'valorant' is not running. This is possibly a false session RMS notification. Skip session of sessionID: '681b7082-d9c6-4b67-809f-d68d958be663'
LogCoreGameManager: Warning: RefreshPlayer invoked while loop state is not INGAME.
LogJson: Error: JsonValueToUProperty - Unable to import JSON string into Guid property SeasonID
LogAresCommonAnalogCursor: Error: PopIgnoreVirtualClickContext - Ignore Virtual Click Contexts does not have context: UnknownScreen.WBP_Screen_ProxyShell_PC_C.WBP_Screen_ProxyShell_PC_C_2147431067
LogStreaming: Display: FlushAsyncLoading(21890): 1 QueuedPackages, 0 AsyncPackages
LogAkAudio: Warning: Enabled Late Reverb component for room SAV_Asylum_JumpingPuzzle_1 without an assigned Late Reverb Aux Bus
LogAkAudio: Warning: Enabled Late Reverb component for room SAV_Asylum_JumpingPuzzle_2 without an assigned Late Reverb Aux Bus
LogAkAudio: Warning: Enabled Late Reverb component for room SAV_Asylum_ShootingRange_Roof_1 without an assigned Late Reverb Aux Bus
LogAkAudio: Warning: Enabled Late Reverb component for room SAV_Asylum_ShootingRange_Roof_2 without an assigned Late Reverb Aux Bus
LogAkAudio: Warning: Enabled Late Reverb component for room SAV_Gallery_Roof_1 without an assigned Late Reverb Aux Bus
LogAkAudio: Warning: Enabled Late Reverb component for room SAV_Tower_Interior_2 without an assigned Late Reverb Aux Bus
LogWindows: Warning: Reporting crash.
LogWindows: Warning: Crash event triggered.
LogWindows: Warning: Running crash report client:
LogWindows: Error: === Critical error: ===
LogWindows: Error:
LogWindows: Error: Fatal error!
LogWindows: Error:
LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION 0x0000000000000000 IP: 0x0000000000000000
LogWindows: Error:
LogWindows: Error:

*/
namespace Hook
{
	class CFunction
	{
	public:
		inline bool Init(void* pTarget, void* pDetour) {
			//return (MH_CreateHook(pTarget, pDetour, &m_pOriginal) == MH_STATUS::MH_OK);
			return false;
		}

	public:
		template<typename FN>
		inline FN Original() const {
			return reinterpret_cast<FN>(m_pOriginal);
		}

	private:
		void* m_pOriginal = nullptr;
	};

	static inline void memcpy_(void* _Dst, void const* _Src, size_t _Size)
	{
		auto csrc = (char*)_Src;
		auto cdest = (char*)_Dst;

		for (int i = 0; i < _Size; i++)
		{
			cdest[i] = csrc[i];
		}
	}

	static inline void SwapVtable(void* obj, uint32_t index, void* hook) {
		auto currVt = *(void**)(obj);

		auto vtable = *(void***)(obj);
		int i = 0;

		for (; vtable[i]; i++)
			__noop();

		auto newVt = new uintptr_t[i];

		memcpy_(newVt, currVt, i * 0x8);

		newVt[index] = (uintptr_t)hook;

		*(uintptr_t**)(obj) = newVt;
	}

	class CTable
	{
	public:
		inline bool Init(const void* pTable)
		{
			m_pBase = (unsigned int**)(pTable);

			while (reinterpret_cast<unsigned int*>(*m_pBase)[m_nSize])
				m_nSize += 1u;

			m_pOriginals = std::make_unique<void* []>(m_nSize);

			return (m_pBase && m_nSize);
		}

		inline bool Hook(void* pDetour, const unsigned int nIndex)
		{
			if (m_pBase && m_nSize)
				return false;

			return false;
		}

	public:
		template<typename FN>
		inline FN Original(const unsigned int nIndex) const {
			return reinterpret_cast<FN>(m_pOriginals[nIndex]);
		}

	private:
		unsigned int** m_pBase = 0u;
		unsigned int		      m_nSize = 0u;
		std::unique_ptr<void* []> m_pOriginals = { };
	};

	class CVMTable
	{
	public:
		inline bool Initialize(const void* pTable)
		{
			m_pTable = (uintptr_t**)(pTable);

			if (m_nSize <= 0u)
			{
				while (static_cast<uintptr_t*>(*m_pTable)[m_nSize])
					m_nSize += 1u;
			}

			m_pOriginalTable = *m_pTable;
			m_pCurrent = std::make_unique<uintptr_t[]>(m_nSize);
			memcpy(m_pCurrent.get(), m_pOriginalTable, (m_nSize * sizeof(size_t)));
			*m_pTable = m_pCurrent.get();

			return ((m_nSize > 0u) && m_pOriginalTable && *m_pTable);
		}

		template<typename FN>
		inline FN Original(const size_t nIndex)
		{
			return reinterpret_cast<FN>(m_pOriginalTable[nIndex]);
		}

		inline void Hook(void* pNewFunc, const size_t nIndex)
		{
			m_pCurrent[nIndex] = reinterpret_cast<uintptr_t>(pNewFunc);
		}

		inline void RemoveHook(const size_t nIndex)
		{
			m_pCurrent[nIndex] = m_pOriginalTable[nIndex];
		}

		inline void RestoreTable()
		{
			if ((m_nSize > 0u) && m_pOriginalTable && *m_pTable)
			{
				*m_pTable = m_pOriginalTable;
			}
		}

		inline bool IsInitialized()
		{
			return ((m_nSize > 0u) && m_pOriginalTable && *m_pTable);
		}

	private:
		uintptr_t** m_pTable = nullptr;
		uintptr_t* m_pOriginalTable = nullptr;
		size_t m_nSize = 0u;

		std::unique_ptr<uintptr_t[]> m_pCurrent = { };
	};
}


wchar_t* StringToWchar(const char* c)
{
	const size_t cSize = strlen(c) + 1;
	wchar_t* wc = new wchar_t[cSize];
	mbstowcs(wc, c, cSize);

	return wc;
}