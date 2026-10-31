#include "Class.h"
#include "../../Utils/Utils.h"



void PatchHook(void* dst, const void* src, SIZE_T size) {
	DWORD oldProtect = 0;
	if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect))
		return;

	memcpy(dst, src, size);

	DWORD tmp = 0;
	VirtualProtect(dst, size, oldProtect, &tmp);
}


//48 89 5C 24 08                          mov[rsp + 8], rbx
//.text:0000000145AEC495 48 89 74 24 10                          mov[rsp + 10h], rsi
//.text : 0000000145AEC49A 48 89 7C 24 18                          mov[rsp + 18h], rdi
//.text : 0000000145AEC49F 4C 89 64 24 20                          mov[rsp + 20h], r12
//.text : 0000000145AEC4A4 55                                      push    rbp
//.text : 0000000145AEC4A5 41 56                                   push    r14
//.text : 0000000145AEC4A7 41 57                                   push    r15
//.text : 0000000145AEC4A9 48 8B EC                                mov     rbp, rsp
//.text : 0000000145AEC4AC 48 83 EC 70

UObject* UObject::StaticFindObject(void* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, bool ExactClass)
{
	//����
	using T_FindObject = UObject * (*)(void*, UObject*, const TCHAR*, bool);

	static T_FindObject FindObjectFunc = nullptr;

	if (!FindObjectFunc)
	{
		Utils::Pattern pattern;

		ULONG64 StaticFindObjectAddr = pattern.FindPattern(
			"40 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 58 FC FF FF 48 81 EC A8 04 00 00 0F 29 B4 24 90 04 00 00"
		).get(); // absolute VA doner — pattern guncellemelere dayaniklidir

		// pattern tutmazsa GUNCEL RVA'ya dus (28.10 guncelleme sonrasi dump: 0x1D07C40;
		// eski 0x1FE0EC0 kaldirildi — koşulsuz eziyordu, pattern hiç kullanilmiyordu!)
		if (!StaticFindObjectAddr)
			StaticFindObjectAddr = (uintptr_t)GetModuleHandle(NULL) + (uintptr_t)offs::rva::static_find_object;

		if (!StaticFindObjectAddr)
			return NULL;

		FindObjectFunc = reinterpret_cast<T_FindObject>(StaticFindObjectAddr);

		if (!FindObjectFunc)
			return NULL;
	}

	// ֱ�ӵ��ú�����x64 ��������ʽָ�� __fastcall��
	return FindObjectFunc(ObjectClass, InObjectPackage, OrigInName, ExactClass);
}



//GPGameplay.GPCharacterBase
//Class GPGameplay.IntCharacter
//Class GPGameplay.GPSafeHouseCharacterBase
void* UObject::GetPlayerClass()
{
	static void* PlayerClass = NULL;
	if (!PlayerClass) {
		PlayerClass = StaticFindObject(NULL, NULL, L"Engine.Character", false);	
	}


	//printf("PlayerClass: %p\n", PlayerClass);

	return PlayerClass;
}

void* UObject::GetActorClass()//?   SDK 
{
	static void* ActorClass = NULL;
	if (!ActorClass) {
		ActorClass = StaticFindObject(NULL, NULL, L"Engine.Actor", false);
	}

	//printf("ActorClass: %p\n", ActorClass);

	return ActorClass;
}



void* UObject::GetEngineClass()
{//Transient.AresGameEngine_2147482593
	static void* ActorClass = NULL; 
	if (!ActorClass) {
		ActorClass = StaticFindObject(NULL, NULL, L"Engine.Engine", false);
	}
 
	return  ActorClass;
}

void* UObject::GetGameplayStatics()
{
	static void* ActorClass = NULL;
	if (!ActorClass) {
		ActorClass = StaticFindObject(NULL, NULL, L"Engine.Default__GameplayStatics", false);
	}

	return  ActorClass;
}


wchar_t* Render::s2wc(const char8_t* c)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(c), -1, NULL, 0);
	wchar_t* wc = new wchar_t[len];
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(c), -1, wc, len);

	return wc;
}

void* UObject::GetAresEquippable()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.AresEquippable", false);
	}

	return Class;
}



void* UObject::GetContentLibrary()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.Default__ContentLibrary", true);
	}

	return Class;
}


void* UObject::GetShooterPlayerControllerDebugComponent()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.ShooterPlayerControllerDebugComponent", false);
	}

	return Class;
}

void* UObject::GetBaseMainMenuPlayerController()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.Default__BaseMainMenuPlayerController", false);
	}

	return Class;
}

void* UObject::GetUMainMenuCheatManager()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.Default__MainMenuCheatManager", false);
	}

	return Class;
}

void* UObject::GetUAttachmentControllerComponent()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.AttachmentControllerComponent", false);
	}

	return Class;
}

void* UObject::GetInventoryManager()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.InventoryManager", false);
	}

	return Class;
}

void* UObject::GetUAresPersonalizationViewModel()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.GunSkinPersonalizationViewModel", false);
	}
	printf("Class: %p\n", Class);

	return Class;
}

void* UObject::GetGameFlowStateManager()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.GameFlowStateManager", false);
	}

	return Class;
}

void* UObject::GetStaticGameFlowState()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.StaticGameFlowState", false);
	}

	return Class;
}

void* UObject::GetWeaponDetailsSkin_C()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"WeaponDetailsSkin.WeaponDetailsSkin_C", false);
	}

	return Class;
}

void* UObject::GetAresClientGameInstance()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.AresClientGameInstance", false);
	}
	
	return Class;
}

void* UObject::GetAresOutlineRendering()
{
	static void* Class = NULL;

	if (!Class) {
		// SetOutlineColorsForRender STATIC BlueprintCallable -> dogru cagri hedefi CDO'dur.
		// Class uzerinde cagrilirsa instance alanlarini cop okur -> AV/crash.
		Class = StaticFindObject(NULL, NULL, L"Renderer.Default__AresOutlineRendering", false);
	}

	return Class;
}

UAresClientGameInstance* UObject::GetAresClientGameInstance(UWorld* WorldContextObject)
{
	static void* Function = NULL;

	if (!Function)
	{
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresClientGameInstance.GetAresClientGameInstance", false);
	}
	
	if (!Function) return 0;

	struct {
		UObject* WorldContextObject;
		UAresClientGameInstance* ReturnValue;

	} Params = { WorldContextObject };

	WorldContextObject->ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

void* UObject::GetAresPrespawnedEquippableCacheComponent()
{
	static void* Class = NULL;

	if (!Class) {
		Class = StaticFindObject(NULL, NULL, L"ShooterGame.AresPrespawnedEquippableCacheComponent", false);
	}

	return Class;
}

APlayerCameraManager* UObject::GetPlayerCameraManager(UObject* WorldContextObject)
{
	static void* Function = NULL;

	if (!Function)
	{
		Function = StaticFindObject(NULL, NULL, L"GameplayStatics.GetPlayerCameraManager", false);
	}
	
	if (!Function) return 0;

	struct {
		UObject* WorldContextObject;
		int PlayerIndex;
		APlayerCameraManager* ReturnValue;
	} Params = { WorldContextObject, 0 };

	WorldContextObject->ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

APlayerController* UObject::GetLocalPlayerController(UWorld* WorldContextObject)
{
	static void* Function = NULL;

	if (!Function) 
	{
		Function = StaticFindObject(NULL, NULL, L"GameplayStatics.GetPlayerController", false);
	}
	
	if (!Function) 
		return 0;

	struct {
		UObject* WorldContextObject;
		int PlayerIndex;
		APlayerController* ReturnValue;
	} Params = { WorldContextObject, 0 };

	WorldContextObject->ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

void UGameplayStatics::GetAllActorsOfClass(UWorld* WorldContextObject, void* ActorClass, TArray<ACharacter*>* OutActors)
{

	static void* Function = NULL;
	if (!Function) {            
		Function = StaticFindObject(NULL, NULL, L"GameplayStatics.GetAllActorsOfClass", NULL);
	}

	struct
	{
		UWorld* WorldContextObject;
		void* ActorClass;
		TArray<ACharacter*> OutActors;
	}Params = { WorldContextObject,ActorClass,TArray<ACharacter*>() };

	WorldContextObject->ProcessEvent(Function, &Params);
	
	if (!Params.OutActors.IsValid())
		return;

	*OutActors = Params.OutActors;  //		
}

bool UGameplayStatics::LineTraceRaw(UWorld* WorldContextObject, const FVector& Start, const FVector& End,
	void** IgnoreActors, int IgnoreCount, uint8_t TraceChannel, size_t HitSize, bool& bBlockingHit)
{
	static void* Function = NULL;
	if (!Function)
		Function = StaticFindObject(NULL, NULL, L"GameplayStatics.LineTraceSingle", NULL);
	if (!Function || !WorldContextObject || HitSize < 0x80 || HitSize > 0x180) return false;

	// UE4.26 K2 param-frame yerlesimi:
	//   0x00 WorldContext*, 0x08 Start(FVector), 0x20 End, 0x38 TraceChannel(u8),
	//   0x39 bTraceComplex(u8), 0x40 ActorsToIgnore(TArray, align 8), 0x50 DrawDebugType(u8),
	//   0x54 FHitResult OutHit (HitSize), ardindan: bIgnoreSelf(u8) + TraceColor(16) +
	//   TraceHitColor(16) + DrawTime(4) + ReturnValue(u8)  ->  ReturnValue = 0x54+HitSize+0x28
	// HitSize yanlisysa ReturnValue okunusu sacmalar ama ASLA buffer tasmaz (0x240 pay).
	unsigned char buf[0x240] = {};
	memcpy(buf + 0x00, &WorldContextObject, 8);
	memcpy(buf + 0x08, &Start, sizeof(FVector));
	memcpy(buf + 0x20, &End, sizeof(FVector));
	buf[0x38] = TraceChannel;
	buf[0x39] = 0;                                    // bTraceComplex
	unsigned long long arrPtr = (unsigned long long)(uintptr_t)IgnoreActors; // TArray<AActor*>
	int32_t arrNum = IgnoreCount, arrMax = IgnoreCount;
	memcpy(buf + 0x40, &arrPtr, 8);
	memcpy(buf + 0x48, &arrNum, 4);
	memcpy(buf + 0x4C, &arrMax, 4);
	buf[0x50] = 0;                                    // DrawDebugType: None
	buf[0x54 + HitSize] = 1;                          // bIgnoreSelf (yedek — ignore listesinde de var)
	size_t rvOff = 0x54 + HitSize + 0x28;
	if (rvOff + 1 > sizeof(buf)) return false;

	WorldContextObject->ProcessEvent(Function, buf);

	uint8_t rv = 0;
	memcpy(&rv, buf + rvOff, 1);
	bBlockingHit = rv != 0;
	return true;
}

UGameInstance* UGameplayStatics::GetGameInstance(UWorld* WorldContextObject)
{
	static void* Function = NULL;

	if (!Function)
	{
		Function = StaticFindObject(NULL, NULL, L"Engine.GameplayStatics.GetGameInstance", false);
	}

	if (!Function) return 0;

	struct {
		UWorld* WorldContextObject;
		UGameInstance* ReturnValue;

	} Params = { WorldContextObject };

	WorldContextObject->ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}



bool UGameplayStatics::ProjectWorldToScreen(APlayerController* Player, FVector WorldPosition, FVector2D& ScreenPosition, bool bPlayerViewportRelative)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"GameplayStatics.ProjectWorldToScreen", NULL);
	}

	if (!Function)
		return {};

//	printf("Function: %p\n", Function);

	struct {
		APlayerController* Player;
		FVector WorldPosition;
		FVector2D ScreenPosition;
		bool bPlayerViewportRelative;
		bool ReturnValue;
	}Params{ Player, WorldPosition, FVector2D(), bPlayerViewportRelative, false};

	Player->ProcessEvent(Function, &Params);

	ScreenPosition = Params.ScreenPosition;

	return Params.ReturnValue;
}




bool APlayerController::LineOfSightTo(AActor* Other, FVector ViewPoint, bool bAlternateChecks)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Engine.Controller.LineOfSightTo", NULL);
	}

	if (!Function)
		return {};

	struct {
		AActor* Other;
		FVector ViewPoint;
		bool bAlternateChecks;
		bool ReturnValue;
	}Params{ Other, ViewPoint, bAlternateChecks };

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

APlayerCameraManager* APlayerController::GetPlayerCameraManager()
{
	static void* Function = NULL;

	if (!Function)
	{
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresPlayerController.GetPlayerCameraManager", false);
	}

	if (!Function) return {};

	struct {
		APlayerCameraManager* ReturnValue;
	} Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

FVector APlayerController::GetControlRotation()
{
	static void* Function = NULL;

	if (!Function)
	{
		Function = StaticFindObject(NULL, NULL, L"Engine.Controller.GetControlRotation", false);
	}

	if (!Function) return {};

	struct {
		FVector ReturnValue;
	} Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

void APlayerController::SetControlRotation(FVector& NewRotation)
{
	static void* Function = NULL;

	if (!Function)
	{
		Function = StaticFindObject(NULL, NULL, L"Engine.Controller.SetControlRotation", false);
	}

	if (!Function) return;

	struct {
		FVector Rot;
	} Parameters;

	Parameters.Rot = NewRotation;
	ProcessEvent(Function, &Parameters);
}

bool APlayerController::W2S(FVector WorldLocation, FVector2D& Screenlocation)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"PlayerController.ProjectWorldLocationToScreen", NULL);
	}

	if (!Function)
		return {};

	struct MyStruct
	{
		FVector WorldLocation;
		FVector2D Screenlocation;
		bool bPlayerViewportRelativebPlayerViewportRelative;
		bool ReturnValue;
	}Params{ WorldLocation,FVector2D(),false };

	ProcessEvent(Function, &Params);

	Screenlocation = Params.Screenlocation;

	return Params.ReturnValue;
}

void APlayerController::SetMouseLocation(int32_t X, int32_t Y)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Engine.PlayerController.SetMouseLocation", NULL);
	}

	if (!Function)
		return;

	struct MyStruct
	{
		int32_t X;
		int32_t Y;
	}Params{ X,Y };

	ProcessEvent(Function, &Params);
}

ACharacter* APlayerController::K2_GetPawn()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Engine.Controller.K2_GetPawn", false);
	}

	if (!Function)
		return {};

	struct {
		ACharacter* ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

void APlayerController::StopMovement()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Engine.Controller.StopMovement", NULL);
	}

	if (!Function)
		return;

	ProcessEvent(Function, nullptr);
}


AAresEquippable* UShooterCharacterDeathReactionComponent::GetKillingEquippable()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.ShooterCharacterDeathReactionComponent.GetKillingEquippable", false);
	}

	if (!Function)
		return {};

	struct {
		AAresEquippable* ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

UShooterCharacterDeathReactionComponent* ACharacter::GetDeathReactionComponent()
{

	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.ShooterCharacter.GetDeathReactionComponent", false);
	}

	if (!Function)
		return {};

	struct {
		UShooterCharacterDeathReactionComponent* ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

UAresOutlineComponent* ACharacter::GetAresOutlineComponent()
{
	static UAresOutlineComponent* Class = NULL;

	if (!Class) {
		Class = (UAresOutlineComponent*)StaticFindObject(NULL, NULL, L"ShooterGame.AresOutlineComponent", false);
	}

	return Class;
}

USceneComponent* ACharacter::K2_GetRootComponent()
{

	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Actor.K2_GetRootComponent", false);
	}

	if (!Function)
		return {};

	struct {
		ACharacter* This;
		USceneComponent* ReturnValue;
	}Params{ this,nullptr };

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}




float ACharacter::GetHealth()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.ShooterCharacter.GetHealth", false);
	}

	struct {
		float ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

AAresPlayerStateBase* ACharacter::GetPlayerState()
{
	return *(AAresPlayerStateBase**)(this + 0x480);
}

FString APlayerState::GetPlayerName()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"PlayerState.GetPlayerName", false);
	}

	struct {
		FString Ret;
	}Params;

	ProcessEvent(Function, &Params);
	
	return Params.Ret;
}
 

FVector ACharacter::K2_GetActorLocation()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Actor.K2_GetActorLocation", false);
	}
	if (!Function)
		return {};
	struct {
		FVector ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}



APlayerController* UGameInstance::GetPlayerController()
{
	uint8_t* Player = **(uint8_t***)((uint8_t*)this + 0x40);
	if (!Player)
		return *(APlayerController**)(Player + 0x38);
	return nullptr;
}

ACharacter* UGameInstance::GetLocalPlayer()
{
	uint8_t* Player = **(uint8_t***)((uint8_t*)this + 0x40);

	if (Player)
	{
		APlayerController* Controller = *(APlayerController**)(Player + 0x38);

		if (Controller)
		{
			return *(ACharacter**)(Controller + 0x510);
		}

	}
	return nullptr;
}


//(struct AActor* Parent, struct UEquippableSkinDataAsset* EquippableSkinAsset, struct UEquippableSkinChromaDataAsset* EquippableChromaAsset, int32_t SkinLevel,
//	struct UEquippableCharmDataAsset* EquippableCharmAsset, int32_t CharmLevel);




UAresInventory* ACharacter::GetInventory() //ShooterGame.ShooterCharacter.GetInventory
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.ShooterCharacter.GetInventory", false);
	}
	if (!Function)
		return 0;

	struct {
		UAresInventory* ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

bool ACharacter::CanJump()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Character.CanJump", false);
	}
	if (!Function)
		return 0;

	struct {
		bool ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

void ACharacter::Jump(bool bHide)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.ShooterCharacter.SetHideFirstPersonOverride", false);
	}
	if (!Function)
		return;

	struct {
		bool bHide;
	}Params{ bHide };

	ProcessEvent(Function, &Params);

}



int32_t UEquippableSkinLevelDataAsset::GetLevel(UWorld* WorldContextObject)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.EquippableSkinLevelDataAsset.GetLevel", false);
	}

	if (!Function)
		return 0;

	struct {
		UWorld* WorldContextObject;
		int32_t Return;
	}Params{ WorldContextObject };

	ProcessEvent(Function, &Params);

	return Params.Return;
}

UGameFlowState* UGameFlowStateManager::GetCurrentState()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.GameFlowStateManager.GetCurrentState", false);
	}

	if (!Function)
		return 0;

	struct {
		UGameFlowState* Return;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.Return;
}


UArsenalViewController* UAresClientGameInstance::GetArsenalViewController()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresClientGameInstance.GetArsenalViewController", false);
	}

	if (!Function)
		return 0;

	struct {
		UArsenalViewController* Return;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.Return;
}

UAresClientGameInstance* UAresClientGameInstance::GetAresClientGameInstance(UWorld* WorldContextObject)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresClientGameInstance.GetAresClientGameInstance", false);
	}

	if (!Function)
		return 0;

	struct {
		UWorld* WorldContextObject;
		UAresClientGameInstance* Return;
	}Params{ WorldContextObject };

	WorldContextObject->ProcessEvent(Function, &Params);

	return Params.Return;
}

TArray<struct UEquippableInventoryModel*> UArsenalViewModel::GetGunModels()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.ArsenalViewModel.GetGunModels", false);
	}

	if (!Function)
		return 0;

	struct {
		TArray<UEquippableInventoryModel*> Return;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.Return;
}

void UArsenalViewModel::SetSkinAndChromaForGun(UEquippableDataAsset* Equippable, UEquippableSkinDataAsset* Skin, UEquippableSkinChromaDataAsset* Chroma)
{
	static void* Function = NULL;	
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresPersonalizationViewModel.SetSkinAndChromaForGun", false);
	}

	if (!Function)
		return;

	struct {
		UEquippableDataAsset* Equippable;
		UEquippableSkinDataAsset* Skin;
		UEquippableSkinChromaDataAsset* Chroma;
	}Params{ Equippable, Skin, Chroma };

	ProcessEvent(Function, &Params);
}

void UAresOutlineComponent::UpdateOutlineMode()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresOutlineComponent.UpdateOutlineMode", false);
	}

	if (!Function)
		return;	

	ProcessEvent(Function, NULL);
}

void UAresOutlineComponent::SetOutlineModeAlly(EAresOutlineMode OutlineMode)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresOutlineComponent.SetAllyOutlineMode", false);
	}

	if (!Function)
		return;

	struct {
		EAresOutlineMode OutlineMode;
	}Params{ OutlineMode };

	ProcessEvent(Function, &Params);
}
void UAresOutlineComponent::SetOutlineMode(EAresOutlineMode OutlineMode)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresOutlineComponent.SetOutlineMode", false);
	}

	if (!Function)
		return;

	struct {
		EAresOutlineMode OutlineMode;
	}Params{ OutlineMode };

	ProcessEvent(Function, &Params);
}


void UAresOutlineRendering::SetOutlineColorsForRender(UWorld* WorldContextObject, FLinearColor AllyColor, FLinearColor EnemyColor)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Renderer.AresOutlineRendering.SetOutlineColorsForRender", false);
	}

	if (!Function)
		return;

	struct {
		UWorld* WorldContextObject;
		FLinearColor AllyColor;
		FLinearColor EnemyColor;
	}Params{ WorldContextObject, AllyColor, EnemyColor };

	ProcessEvent(Function, &Params);
}

FVector APlayerCameraManager::GetCameraRotation()
{
	static void* Function = NULL;
	
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"PlayerCameraManager.GetCameraRotation", false);
	}

	if (!Function)
		return FVector();

	struct {
		FVector Return;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.Return;
}

FVector APlayerCameraManager::GetCameraLocation()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"PlayerCameraManager.GetCameraLocation", false);
	}

	if (!Function)
		return FVector();

	struct {
		FVector Return;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.Return;
}

UBombTeamComponent* AAresPlayerStateBase::GetTeamComponent()
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresPlayerStateBase.GetTeamComponent", false);
	}	

	if (!Function)
		return 0;

	struct {
		UBombTeamComponent* Return;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.Return;
}

bool UMovementComponent::IsCrouching()
{
	static void* Function = NULL;

	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Engine.NavMovementComponent.IsCrouching", false);
	}

	if (!Function)	
		return 0;

	struct 
	{
		bool Return;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.Return;
}

void UMovementComponent::StopMovementImmediately()
{
	static void* Function = NULL;

	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"Engine.NavMovementComponent.StopMovementKeepPathing", false);
	}

	if (!Function)
		return;

	ProcessEvent(Function, NULL);
}

void UAresPrespawnedEquippableCacheComponent::AuthOnEquippableDataAssetGunSkinModified(UEquippableDataAsset* EquippableDataAsset)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresPrespawnedEquippableCacheComponent.AuthOnEquippableDataAssetGunSkinModified", false);
	}

	if (!Function)
		return;

	struct {
		UEquippableDataAsset* EquippableDataAsset;
	}Params{ EquippableDataAsset };

	ProcessEvent(Function, &Params);
}

void UArsenalViewController::SetSkinWithLevelAndChromaForGun(
	UEquippableDataAsset* Equippable, 
	UEquippableSkinDataAsset* Skin, 
	UEquippableSkinLevelDataAsset* SkinLevel, 
	UEquippableSkinChromaDataAsset* Chroma)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.ArsenalViewController.SetSkinWithLevelAndChromaForGun", false);
	}

	if (!Function)
		return;

	struct {
		UEquippableDataAsset* Equippable;
		UEquippableSkinDataAsset* Skin;
		UEquippableSkinLevelDataAsset* SkinLevel;
		UEquippableSkinChromaDataAsset* Chroma;
	}Params{ Equippable, Skin, SkinLevel, Chroma };

	ProcessEvent(Function, &Params);
}

void UCanvas::K2_DrawBox(FVector2D ScreenPosition, FVector2D ScreenSize, float Thickness, FLinearColor RenderColor)
{
	static void* Function = NULL;

	if (!Function) 
	{
		Function = StaticFindObject(NULL, NULL, L"Engine.Canvas.K2_DrawBox", false);
	}

	if (!Function)
		return;

	struct {
		FVector2D ScreenPosition;
		FVector2D ScreenSize;
		float Thickness;
		FLinearColor RenderColor;
	}
	Params{ ScreenPosition, ScreenSize, Thickness, RenderColor };

	ProcessEvent(Function, &Params);
}

void UCanvas::K2_DrawText(UFont* RenderFont, FString RenderText, FVector2D ScreenPosition, FVector2D Scale, FLinearColor RenderColor, float Kerning, FLinearColor ShadowColor, FVector2D ShadowOffset, bool bCentreX, bool bCentreY, bool bOutlined, FLinearColor OutlineColor)
{
	static void* Function = NULL;
	if (!Function)
	{
		Function = StaticFindObject(NULL, NULL, L"Engine.Canvas.K2_DrawText", false);
	}

	if (!Function)
		return;

	struct {
		UFont* RenderFont;
		FString RenderText;
		FVector2D ScreenPosition;
		FVector2D Scale;
		FLinearColor RenderColor;
		float Kerning;
		FLinearColor ShadowColor;
		FVector2D ShadowOffset;
		bool bCentreX;
		bool bCentreY;
		bool bOutlined;
		FLinearColor OutlineColor;
	}
	Params{ RenderFont, RenderText, ScreenPosition, Scale,RenderColor,Kerning,ShadowColor,ShadowOffset,bCentreX,bCentreY,bOutlined,OutlineColor };

	ProcessEvent(Function, &Params);

}

void UWeaponDetailsSkin_C::SetSkin(UEquippableSkinDataAsset* NewSkinDataAsset, int32_t Quantity, void* AdditionalContext)
{
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"WeaponDetailsSkin.WeaponDetailsSkin_C.SetSkin", false);
	}

	if (!Function)
		return;

	struct {
		UEquippableSkinDataAsset* EquippableSkinAsset;
		int32_t SkinLevel;
		void* add;
	}Params{ NewSkinDataAsset, Quantity, AdditionalContext };

	ProcessEvent(Function, &Params);
}

void ShooterPlayerControllerDebugComponent::ServerGunSkinModifySkinByEquippable(UEquippableSkinDataAsset* SkinDataAsset, 
	UEquippableDataAsset* EquippableDataAsset, UEquippableSkinChromaDataAsset* GunSkinChromaAsset, int32_t Level)
{
	//ShooterGame.ShooterPlayerControllerDebugComponent.ServerGunSkinModifySkinByEquippable
	static void* Function = NULL;
	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.ShooterPlayerControllerDebugComponent.ServerGunSkinModifySkinByEquippable", false);
	}

	if (!Function)
		return;

	struct {
		UEquippableSkinDataAsset* EquippableSkinAsset;
		UEquippableDataAsset* EquippableDataAsset;
		UEquippableSkinChromaDataAsset* GunSkinChromaAsset;
		int32_t Level;
	}Params{ SkinDataAsset, EquippableDataAsset, GunSkinChromaAsset,Level };

	ProcessEvent(Function, &Params);
}

bool AAresEquippable::IsEquippable()
{
	static void* Function = NULL;

	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresItem.IsEquippable", false);
	}

	if (!Function)
		return 0;

	struct {
		bool ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

FText AAresEquippable::GetSkinNameText()
{
	static void* Function = NULL;

	if (!Function) {
		Function = StaticFindObject(NULL, NULL, L"ShooterGame.AresEquippable.GetSkinNameText", false);
	}

	if (!Function)
		return {};

	struct {
		FText ReturnValue;
	}Params;

	ProcessEvent(Function, &Params);

	return Params.ReturnValue;
}

UEngine* GetEngine()
{
	UEngine* Engine = (UEngine*)UObject::GetEngineClass();

	if (Engine && !IsBadReadPtr(Engine, 8))
		return Engine;

	return nullptr;
}