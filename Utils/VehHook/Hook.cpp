#include "hook.h"




Int3Hook Hook;

//LONG NTAPI AimbotException(_EXCEPTION_POINTERS* ExceptionInfo)
//{
//    if (ExceptionInfo->ExceptionRecord->ExceptionAddress == Hook.GetHookAddr())
//    {
//        Hook.UnHook();
//        Hook.HookJmpAddr();
//        Aimbot& Instance = Aimbot::GetInstance();
//        FVector* SpawnLocation = (FVector*)(ExceptionInfo->ContextRecord->Rcx);
//        if (SAFE_PTR(SpawnLocation))
//        {
//            bool IsAim = Instance.CanAim();
//            if (IsAim && Options.OpenAimbot && Options.AimMod == 1)
//            {
//                float Distance = Instance.GetAimDistance();
//                FRotator AimRota = Instance.GetAimRotation();
//                FVector AimVector = UKismetStringLibrary::GetForwardVector(&AimRota);
//                if (Options.SilentRange >= Distance && AimVector.X != 0 && AimVector.Y != 0)
//                {
//                    if (GetAsyncKeyState(Options.Aim_hotkey))
//                    {
//                        SpawnLocation->X = AimVector.X;
//                        SpawnLocation->Y = AimVector.Y;
//                        SpawnLocation->Z = AimVector.Z;
//                    }
//                }
//            }
//
//        }
//
//        return -1;
//    }
//    else if (ExceptionInfo->ExceptionRecord->ExceptionAddress == Hook.GetJmpAddr())
//    {
//        Hook.Hook();
//        Hook.UnHookJmpAddr();
//        return -1;
//    }
//    return 0;
//}
//
//void HookInitialize()
//{
//    ULONG64 SpawAddress = Utils::Pattern::FindPattern("E8 ?? ?? ?? ?? 48 8B BC 24 18 04 00 00 48 8B F0 B9 0C 00 00 00 F3 A4 4C 8D 8C 24 ?? ?? ?? ??").get();
//    Hook.Initialize((LPVOID)(SpawAddress), 5, AimbotException);
//    Hook.Hook();
//
//    // Hook.UnHook();
//}
