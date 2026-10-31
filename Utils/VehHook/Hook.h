#ifndef HOOK_H
#define HOOK_H

#include"../Includes.h"

void HookInitialize();

class Int3Hook
{
public:
    void Initialize(void* HookAddr, int Size, PVECTORED_EXCEPTION_HANDLER VehFunc)
    {
        hVEH = AddVectoredExceptionHandler(1, VehFunc);//增加VEH异常
        this->HookAddr = (BYTE*)HookAddr;
        oHookbyte = *(BYTE*)HookAddr;//读入原始字节
        JmpAddr = (BYTE*)(this->HookAddr + Size);//计算下一条指令的地址
        oJmpbyte = *(BYTE*)JmpAddr;
    }
    void Hook()
    {
        DWORD Protect;
        VirtualProtect(HookAddr, 1, PAGE_EXECUTE_READWRITE, &Protect);//可读可写
        *HookAddr = 0xCC;//int3异常hook
        VirtualProtect(HookAddr, 1, Protect, &Protect);//还原保护
    }
    void HookJmpAddr()
    {
        DWORD Protect;
        VirtualProtect((BYTE*)JmpAddr, 1, PAGE_EXECUTE_READWRITE, &Protect);//可读可写
        *(BYTE*)JmpAddr = 0xCC;//int3异常hook
        VirtualProtect((BYTE*)JmpAddr, 1, Protect, &Protect);//还原保护
    }
    void UnHook()
    {
        DWORD Protect;
        VirtualProtect(HookAddr, 1, PAGE_EXECUTE_READWRITE, &Protect);//可读可写
        *HookAddr = oHookbyte;//还原原始字节
        VirtualProtect(HookAddr, 1, Protect, &Protect);//还原保护

    }
    void UnHookJmpAddr()
    {
        DWORD Protect;
        VirtualProtect((BYTE*)JmpAddr, 1, PAGE_EXECUTE_READWRITE, &Protect);//可读可写
        *(BYTE*)JmpAddr = oJmpbyte;//还原原始字节
        VirtualProtect((BYTE*)JmpAddr, 1, Protect, &Protect);//还原保护
    }
    void* GetHookAddr()
    {
        return HookAddr;
    }
    BYTE* GetJmpAddr()
    {
        return JmpAddr;
    }
private:
    BYTE oHookbyte;//原始字节
    BYTE oJmpbyte;
    void* hVEH;//异常处理函数句柄
    BYTE* HookAddr;//Hook地址
    BYTE* JmpAddr;//下一条指令的地址
};

#endif // !HOOK_H