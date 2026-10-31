#include"Utils.h"


namespace Utils
{
	float Fmod(float X, float Y) {
		return fmodf(X, Y);

	}

	void SinCos(float* OutSin, float* OutCos, float Radians) {
		*OutSin = sinf(Radians);
		*OutCos = cosf(Radians);
	}

	int ContainsNaN(FVector V) {
		return isnan(V.X) || isnan(V.Y) || isnan(V.Z);
	}

	string WstrToUtf8(const wstring Wstr)
	{
		int Len = WideCharToMultiByte(CP_UTF8, 0, Wstr.c_str(), -1, NULL, 0, 0, NULL);

		string NewStr;
		NewStr.resize(Len);

		WideCharToMultiByte(CP_UTF8, 0, Wstr.c_str(), -1, (char*)NewStr.c_str(), Len, 0, NULL);
		return NewStr;
	}


	void VTableHook::Initialize(void* pTarget)
	{
		Target = pTarget;
		oVTable = *(void***)pTarget;

		uint32_t Size = CalcVTableSize();
		VTable = new void* [Size];

		memcpy(VTable, oVTable, Size * sizeof(void*));
		*(void***)pTarget = VTable;
	}
	void VTableHook::Bind(uint32_t Index, void* Function)
	{

		VTable[Index] = Function;


	}
	void VTableHook::UnBind(uint32_t Index)
	{
		VTable[Index] = oVTable[Index];
	}

	void VTableHook::UnAllBind()
	{
		delete[] VTable;
		*(void***)Target = oVTable;
	}

	uint32_t VTableHook::CalcVTableSize()
	{
		uint32_t i = 0;
		while (oVTable[i] != NULL) {
			i++;
		}

		return i;
	}


	Pattern Pattern::FindPattern(const char* pattern)
	{
		static uintptr_t moduleAdressmm = 0;
		if (!moduleAdressmm)
			moduleAdressmm = (uintptr_t)GetModuleHandle(NULL);

		if (!moduleAdressmm)
			return 0;

		static auto patternToByteZmm = [](const char* pattern)
			{
				auto       bytesmm = std::vector<int>{};
				const auto startmm = const_cast<char*>(pattern);
				const auto endmm = const_cast<char*>(pattern) + strlen(pattern);

				for (auto currentmm = startmm; currentmm < endmm; ++currentmm)
				{
					if (*currentmm == '?')
					{
						++currentmm;
						if (*currentmm == '?')
							++currentmm;
						bytesmm.push_back(-1);
					}
					else { bytesmm.push_back(strtoul(currentmm, &currentmm, 16)); }
				}
				return bytesmm;
			};

		const auto dosHeadermm = (PIMAGE_DOS_HEADER)moduleAdressmm;
		const auto ntHeadersmm = (PIMAGE_NT_HEADERS)((std::uint8_t*)moduleAdressmm + dosHeadermm->e_lfanew);

		const auto sizeOfImage = ntHeadersmm->OptionalHeader.SizeOfImage;
		auto       patternBytesmm = patternToByteZmm(pattern);
		const auto scanBytesmm = reinterpret_cast<std::uint8_t*>(moduleAdressmm);

		const auto smm = patternBytesmm.size();
		const auto dmm = patternBytesmm.data();

		for (auto imm = 0ul; imm < sizeOfImage - smm; ++imm)
		{
			bool foundmm = true;
			for (auto jmm = 0ul; jmm < smm; ++jmm)
			{
				if (scanBytesmm[imm + jmm] != dmm[jmm] && dmm[jmm] != -1)
				{
					foundmm = false;
					break;
				}
			}
			if (foundmm) { return reinterpret_cast<uintptr_t>(&scanBytesmm[imm]); }
		}
		return NULL;
	}

	Pattern Pattern::add(uint32_t val)
	{
		Address += val;
		return *this;
	}

	Pattern Pattern::sub(uint32_t val)
	{
		Address -= val;
		return *this;
	}

	Pattern Pattern::Absolute()
	{
		long dwOffset = *(long*)Address;
		Address = Address + dwOffset + 4;
		return *this;
	}

	Pattern Pattern::Deref()
	{
		Address = *(uint64_t*)Address;
		return *this;
	}

	uint64_t Pattern::get() const
	{
		return Address;
	}

	uintptr_t Pattern::RelativeToAbsolute(uintptr_t address, int offset, int size)
	{
		if (address)
		{
			uintptr_t addrs = address + size + *(int*)(address + offset);
			return addrs;
		}
		return NULL;
	}
}