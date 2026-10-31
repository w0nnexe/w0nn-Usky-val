#pragma once
#include"../../Utils/Includes.h"
#include <codecvt>
#include "../IDA.h"
#include "../Offsets.h"
inline uint64_t ImageBase = NULL;
inline float smoothY = 5.8f;
inline float smoothX = 4.5f;

class APlayerState;
class AGameStateBase;
class ACharacter;
class APlayerController;
class FBaseTeamComponent;
class UWorld;
class UFont;
class FTextData;
class UInventoryManager;
class UGameInstance;

typedef __int8 int8;
typedef __int16 int16;
typedef __int32 int32;
typedef __int64 int64;

typedef unsigned __int8 uint8;
typedef unsigned __int16 uint16;
typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;

template<class T>
#define NameAddr 0xC25FB80 //need
#define NameState 0xC449340 //need
#define NameKey NameState + 0x38 //need
#define oWorld offs::rva::gworld // TEK KAYNAK: Engine/Offsets.h → rva::gworld
class TArray
{
//protected:
public:
	T* Data;
	int32 NumElements;
	int32 MaxElements;

public:

	inline TArray()
		:NumElements(0), MaxElements(0), Data(nullptr)
	{
	}

	inline TArray(int32 Size)
		: NumElements(0), MaxElements(Size), Data(reinterpret_cast<T*>(malloc(sizeof(T)* Size)))
	{
	}

	inline T& operator[](uint32 Index)
	{
		return Data[Index];
	}
	inline const T& operator[](uint32 Index) const
	{
		return Data[Index];
	}

	inline int32 Num() const
	{
		return NumElements;
	}

	inline int32 Max() const
	{
		return MaxElements;
	}

	inline int32 GetSlack() const
	{
		return MaxElements - NumElements;
	}

	inline bool IsValid() const
	{
		return Data != nullptr;
	}

	inline bool IsValidIndex(int32 Index) const
	{
		return Index >= 0 && Index < NumElements;
	}

	inline void ResetNum()
	{
		NumElements = 0;
	}
	inline T* GetData() { return Data; }
	inline const T* GetData() const { return Data; }
};


struct FRotator
{
	FRotator() {
		RtlZeroMemory(this, sizeof(FRotator));
	}

	FRotator(double Pitch, double Yaw, double Roll) : pitch(Pitch), yaw(Yaw), roll(Roll) {}

	FRotator operator+(FRotator In) { return FRotator(this->pitch + In.pitch, this->yaw + In.yaw, this->roll + In.roll); }
	FRotator operator-(FRotator In) { return FRotator(this->pitch - In.pitch, this->yaw - In.yaw, this->roll - In.roll); }

	double pitch, yaw, roll;
};

class BoneIndex
{
public:
	BoneIndex() {
		RtlZeroMemory(this, sizeof(BoneIndex));
	}

	uint32_t head, neck_01, spine_03, spine_02, spine_01, pelvis;
	uint32_t hand_l, lowerarm_l, upperarm_l, clavicle_l, clavicle_r, upperarm_r, lowerarm_r, hand_r;
	uint32_t foot_l, calf_l, thigh_l, thigh_r, calf_r, foot_r;
	uint32_t root;
	uint32_t NumBones;
};

class FVector2D
{
public:
	double X, Y;
public:

	//FVector2D(std::initializer_list<double> list) {
	//	auto it = list.begin();
	//	X = (list.size() > 0) ? *it++ : 0.0f;
	//	Y = (list.size() > 1) ? *it : 0.0f;
	//}
	inline FVector2D() : X(0.0), Y(0.0) {}
	inline FVector2D(double X, double Y) : X(X), Y(Y) {}
	inline FVector2D operator +(const FVector2D& other) const { return FVector2D(X + other.X, Y + other.Y); }
	inline FVector2D operator +(double scalar) const { return FVector2D(X + scalar, Y + scalar); }
	inline FVector2D operator -(double scalar) const { return FVector2D(X - scalar, Y - scalar); }
	inline FVector2D operator -(const FVector2D& other) const { return FVector2D(X - other.X, Y - other.Y); }
	inline FVector2D operator *(double scalar) const { return FVector2D(X * scalar, Y * scalar); }
	inline FVector2D operator *(const FVector2D& other) const { return FVector2D(X * other.X, Y * other.Y); }
	inline FVector2D operator /(double scalar) const { return FVector2D(X / scalar, Y / scalar); }
	inline FVector2D operator /(const FVector2D& other) const { return FVector2D(X / other.X, Y / other.Y); }
	inline FVector2D& operator =(const FVector2D& other) { X = other.X; Y = other.Y; return *this; }
	inline FVector2D& operator +=(double scalar) { X += scalar; Y += scalar; return *this; }
	inline FVector2D& operator +=(const FVector2D& other) { X += other.X; Y += other.Y; return *this; }
	inline FVector2D& operator -=(double scalar) { X -= scalar; Y -= scalar; return *this; }
	inline FVector2D& operator -=(const FVector2D& other) { X -= other.X; Y -= other.Y; return *this; }
	inline FVector2D& operator *=(double scalar) { X *= scalar; Y *= scalar; return *this; }
	inline FVector2D& operator *=(const FVector2D& other) { X *= other.X; Y *= other.Y; return *this; }
	inline FVector2D& operator /=(const FVector2D& other) { X /= other.X; Y /= other.Y; return *this; }
	inline FVector2D& operator /=(const double other) { X /= other; Y /= other; return *this; }
	inline bool operator==(const FVector2D& other) const { return X == other.X && Y == other.Y; }
};

struct FVector
{
	double X, Y, Z;

	FVector() {
		RtlZeroMemory(this, sizeof(FVector));
	}

	static double Distance(FVector HeadPos, FVector CameraPos)
	{
		if (&HeadPos == nullptr || &CameraPos == nullptr) {
			return 0.0f;
		}
		const double deltaX = static_cast<double>(HeadPos.X) - static_cast<double>(CameraPos.X);
		const double deltaY = static_cast<double>(HeadPos.Y) - static_cast<double>(CameraPos.Y);
		const double deltaZ = static_cast<double>(CameraPos.Z) - static_cast<double>(CameraPos.Z);

		const double dXSq = deltaX * deltaX;
		const double dYSq = deltaY * deltaY;
		const double dZSq = deltaZ * deltaZ;

		if (dXSq < 0.0f || dYSq < 0.0f || dZSq < 0.0f) {
			return 0.0f;
		}

		const double distanceSquared = dXSq + dYSq + dZSq;
		const double distance = sqrt(distanceSquared);
		return distance;
	}

	FVector(double X, double Y, double Z) : X(X), Y(Y), Z(Z) {};


};

struct FQuat
{
	double X;
	double Y;
	double Z;
	double W;
};

struct FLinearColor
{
	float r;
	float g;
	float b;
	float a;

	FLinearColor() {
		RtlZeroMemory(this, sizeof FLinearColor);
	}

	FLinearColor(float R, float G, float B, float A = 255.f)
	{
		r = R;
		g = G;
		b = B;
		a = A;
	}
};

class FColor
{
public:
	uint8_t B;
	uint8_t G;
	uint8_t R;
	uint8_t A;
public:
	inline FColor() : R(0), G(0), B(0), A(0) {}
	inline FColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) { R = r; G = g; B = b; A = a; }
	class FLinearColor ToLinearColor() const
	{
		return FLinearColor(GammaToLinear(R), GammaToLinear(G), GammaToLinear(B), A / 255.f);
	}
private:
	inline static double GammaToLinear(int Value)
	{
		const double InvGamma = 1.0f / 1.5f;
		double LinearValue = static_cast<double>(Value) / 255.0f; // ĎÔĘ˝×Ş»»ÎŞ¸ˇµăĘý
		LinearValue = LinearValue <= 0.04045f ? LinearValue / 12.92f : pow((LinearValue + 0.055f) / 1.055f, 2.4f);
		return LinearValue * InvGamma;  // ĘąÓĂ InvGamma ˝řĐĐ·´ĐŁŐý
	}
};

class FText
{
public:
	FTextData* Data;
private:
	std::uint8_t Padding[0x10];
};

class FString : public TArray<wchar_t>
{
public:
	FString() = default;

	FString(uint32 InitSize)
		: TArray(InitSize)
	{
	}

	FString(size_t Len, const wchar_t* other)
	{
		MaxElements = NumElements = Len + 1;

		if (NumElements)
		{
			Data = const_cast<wchar_t*>(other);
		}
	};

	FString(const wchar_t* other)
	{
		MaxElements = NumElements = (int32)(other ? wcslen(other) + 1 : 0);

		if (NumElements)
		{
			Data = const_cast<wchar_t*>(other);
		}
	};

	bool IsValid() const
	{
		return Data != nullptr;
	}

	const wchar_t* ToWString() const
	{
		if (IsValid())
			return Data;

		return L"";
	}

	inline explicit operator bool() const { return Data && NumElements > 0; };
	inline FString operator=(const wchar_t* wstr) const { return FString(wstr); }
	inline FString operator=(wchar_t* wstr) { return FString(wstr); }
};


class FTextData
{
private:
	std::uint8_t Padding[0x28];

public:
	wchar_t* Name;
	std::int32_t* Length;
};

struct MagicCircle
{
public:
	MagicCircle()
	{
		RtlZeroMemory(this, sizeof MagicCircle);
	}
	uint32_t MagicCircleTime = 0;
	double MagicCircleAngle = 0.f;
	double Rad = 0.f;
	FLinearColor Color = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);

};

struct FUObjectIem
{
	class UObject* Object;
	char Buff[0x28];
};

class UAresClientGameInstance;
class UArsenalViewController;
class APlayerCameraManager;

class UObject
{
public:

	static void* GetEngineClass();
	static void* GetGameplayStatics();
	static void* GetPlayerClass();
	static void* GetShooterPlayerControllerDebugComponent();
	static void* GetActorClass();
	static void* GetIteamClass();

	static void Free(void* Addr);
	static void* GetAresEquippable();

	static void* GetContentLibrary();
	static void* GetBaseMainMenuPlayerController();
	static void* GetUMainMenuCheatManager();
	static void* GetUAttachmentControllerComponent();
	static void* GetInventoryManager();
	static void* GetUAresPersonalizationViewModel();
	static void* GetGameFlowStateManager();
	static void* GetStaticGameFlowState(); 
	static void* GetWeaponDetailsSkin_C();
	static void* GetAresClientGameInstance();
	static void* GetAresOutlineRendering();	//Renderer.AresOutlineRendering
	static UAresClientGameInstance* GetAresClientGameInstance(UWorld* WorldContextObject); // Function ShooterGame.AresClientGameInstance.GetAresClientGameInstance // (BlueprintCosmetic) // @ game+0x50f1c80
	static void* GetAresPrespawnedEquippableCacheComponent();

	static APlayerCameraManager* GetPlayerCameraManager(struct UObject* WorldContextObject); // Function Engine.GameplayStatics.GetPlayerCameraManager
	static APlayerController* GetLocalPlayerController(UWorld* WorldContextObject);

	//0x3F0, 0x3F8, 0x448  0x8
	void ProcessEvent(void* fn, void* parms)
	{
		auto vtable = *reinterpret_cast<void***>(this);
		using T_ProcessEvent = void (*)(UObject*, void*, void*);
		// ProcessEventIdx = 0x53 = 83 (CppSDK dogrulandi, 28.10 guncellemesi sonrasi gecerli)
		reinterpret_cast<T_ProcessEvent>(vtable[83])(this, fn, parms);
	}
	
	//void ProcessEvent(void* Function, void* Args);

	static UObject* StaticFindObject(void* ObjectClass, UObject* InObjectPackage, const TCHAR* OrigInName, bool ExactClass);
};

class UAresClientGameInstance : public UObject
{
public:
	UArsenalViewController* GetArsenalViewController(); // Function ShooterGame.AresClientGameInstance.GetArsenalViewController // (None) // @ game+0x50a7a80
	UAresClientGameInstance* GetAresClientGameInstance(UWorld* WorldContextObject); // Function ShooterGame.AresClientGameInstance.GetAresClientGameInstance
};

class UField : public UObject {

public:

};

class UStruct : public UField {

public:

};

class UFunction: public UStruct {

public:

};




class FGameViewportClient : public UObject
{
public:
	char pad_0000[0x80];
	UWorld* World;
	UGameInstance* GameInstance; //0x88
}; 

class ULocalPlayer : public UObject
{
public:
	char pad_50[0x80];  
	FGameViewportClient* ViewportClient; // 0x80(0x08)
};


class UGameInstance : public UObject
{
public:
	ACharacter* GetLocalPlayer();
	APlayerController* GetPlayerController();

};


class UEngine : public UObject
{
public:
	char pad_0732[0xbd0];
	FGameViewportClient* GameViewport;// 0x8c0(0x08) //need
};

class UCanvas : public UObject
{
public:
	FVector2D GetScreenSize() {
		return FVector2D((double)(*(int32_t*)(std::uintptr_t(this) + 0x48)), (double)(*(int32_t*)(std::uintptr_t(this) + 0x4c)));
	}

	void K2_DrawBox(FVector2D ScreenPosition, FVector2D ScreenSize, float Thickness, FLinearColor RenderColor); // Function Engine.Canvas.K2_DrawBox // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x4835e90
	void K2_DrawText(UFont* RenderFont, FString RenderText, FVector2D ScreenPosition, FVector2D Scale, 
		FLinearColor RenderColor, float Kerning, struct FLinearColor ShadowColor, 
		struct FVector2D ShadowOffset, bool bCentreX, bool bCentreY, bool bOutlined, struct FLinearColor OutlineColor); // Function Engine.Canvas.K2_DrawText // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x4836df0

};


class UKismetStringLibrary : public UObject
{
public:

	static FVector GetForwardVector(const FRotator* InRo);

};
class APlayerCameraManager : public UObject
{
public:
	struct FVector GetCameraRotation(); // Function Engine.PlayerCameraManager.GetCameraRotation // (None) // @ game+0x3a8bce0
	struct FVector GetCameraLocation(); // Function Engine.PlayerCameraManager.GetCameraLocation // (None) // @ game+0x3a8bc90
};
class AActor : public UObject
{
public:

};


class APlayerController : public UObject
{
public:
	//char pa[0x520];
	//APlayerCameraManager* PlayerCameraManager; // 0x520(0x08)
	bool LineOfSightTo(AActor* Other,  FVector ViewPoint, bool bAlternateChecks); // Function Engine.Controller.LineOfSightTo // (Final) // @ game+0x3eb4420
	APlayerCameraManager* GetPlayerCameraManager();
	FVector GetControlRotation(); // Function Engine.Controller.GetControlRotation
	void SetControlRotation(struct FVector& NewRotation); // Function Engine.Controller.SetControlRotation
	bool W2S(FVector WorldLocation, FVector2D& Screenlocation);
	void SetMouseLocation(int32_t X, int32_t Y); // Function Engine.PlayerController.SetMouseLocation // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x453a1d0
	ACharacter* K2_GetPawn(); // Function Engine.Controller.K2_GetPawn // (None) // @ game+0x3f363c0

	void StopMovement(); //Engine.Controller.StopMovement
};



class ULevel : public UObject
{
public:
	char pa[0xA8];
	int32_t ActorsCount;
};

class UWorld : public UObject
{
public:

};

UEngine* GetEngine();

class UGameplayStatics : public UObject
{
public:
	static void GetAllActorsOfClass(UWorld* WorldContxetObject, void* ActorClass, TArray<ACharacter*>* OutActors);
	static UGameInstance* GetGameInstance(UWorld* WorldContextObject); // Function Engine.GameplayStatics.GetGameInstance // (BlueprintCosmetic) // @ game+0x41890c0

	// K2 LineTraceSingle — raw param-frame (FHitResult boyu dump'tan bilinmedigi icin HitSize ile
	// cagirilir; ReturnValue, 0x54 + HitSize + 0x28 ofsetinden okunur). Fonksiyon bulunamazsa false.
	static bool LineTraceRaw(UWorld* WorldContextObject, const FVector& Start, const FVector& End,
		void** IgnoreActors, int IgnoreCount, uint8_t TraceChannel, size_t HitSize, bool& bBlockingHit);

	//static void GetAllIteamsOfClass(UWorld* WorldContextObject, void* ActorClass, TArray<AActor*>* OutActors);

	static bool ProjectWorldToScreen(APlayerController* Player, FVector WorldPosition, FVector2D& ScreenPosition, bool bPlayerViewportRelative);
};

class USceneComponent : public UObject
{
public:



};

struct UAttachmentControllerComponent : public USceneComponent
{

};

// Enum ShooterGame.EAresTeam
enum class EAresTeam : uint8 {
	AresTeam_Red = 0,
	AresTeam_Blue = 1,
	AresTeam_Invalid = 254,
	EAresTeam_MAX = 255
};

class APlayerState : public UObject
{
public:
	FString GetPlayerName();
};

class UBombTeamComponent
{
public:
	char pa[0xe8];
	enum EAresTeam Team; // 0xe8(0x01)
};

class AAresPlayerStateBase : public APlayerState
{
public:
	UBombTeamComponent* GetTeamComponent();// Function ShooterGame.AresPlayerStateBase.GetTeamComponent // (None) // @ game+0x50eca10
};
class AAresEquippable;
class UItemSlot : public UObject
{
public:
	char pa[0x38];
	AActor* Contents; // 0x38(0x08)
};

class UAresInventory : public UObject
{
public:
	char pa[0x188];
	UItemSlot* ItemSlots[0x10]; // 0x188(0x80)
	char pa1[0x40];
	AAresEquippable* CurrentEquippable;// 0x248(0x8)

	AAresEquippable* GetCurrentEquippable();
};

enum EAresOutlineMode : uint8 {
	None = 0,
	Outline = 1,
	Block = 2,
	Enemy = 3,
	AlwaysOutline = 4,
	AlwaysEnemy = 5,
	EAresOutlineMode_MAX = 6
};

class UEquippableDataAsset { public:

};
class UEquippableSkinChromaDataAsset { public: 

};
class UAresOutlineComponent : public USceneComponent {
public:

	void UpdateOutlineMode(); // Function ShooterGame.AresOutlineComponent.UpdateOutlineMode // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x5183090
	void SetOutlineModeAlly(EAresOutlineMode OutlineMode); // Function ShooterGame.AresOutlineComponent.SetOutlineMode // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x5182ae0

	void SetOutlineMode(EAresOutlineMode OutlineMode); // Function ShooterGame.AresOutlineComponent.SetOutlineMode // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x5182ae0
};

struct UAresOutlineRendering : UObject {
	void SetOutlineColorsForRender(UWorld* WorldContextObject, FLinearColor AllyColor, FLinearColor EnemyColor); // Function Renderer.AresOutlineRendering.SetOutlineColorsForRender // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x26e06c0
};

class AAresEquippable : public AActor
{
public: //need

	FText GetSkinNameText(); // Function ShooterGame.AresEquippable.GetSkinNameText // (None) // @ game+0x5616ef0
	bool IsEquippable(); // Function ShooterGame.AresItem.IsEquippable // (None) // @ game+0x565b4e0
};

class ABaseMainMenuPlayerController : public APlayerController
{
public:
	
};
//TMap<struct UEquippableCharmDataAsset*, struct UEquippableCharmInventoryModel*> DataAssetToEquippableCharmMap; // 0x430(0x50)
class UEquippableInventoryModel;

class UArsenalViewModel : public UObject
{
public:
	TArray<struct UEquippableInventoryModel*> GetGunModels();
	void SetSkinAndChromaForGun(struct UEquippableDataAsset* Equippable, struct UEquippableSkinDataAsset* Skin, struct UEquippableSkinChromaDataAsset* Chroma); // Function ShooterGame.ArsenalViewController.SetSkinAndChromaForGun // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x5237f40
};

class UEquippableSkinLevelDataAsset : UObject
{
public:
	char pa[0x48];
	bool bHideIfNotOwned; // 0x48(0x01)
	int32_t GetLevel(UWorld* WorldContextObject);
};
class UEquippableSkinInventoryModel;
class UArsenalViewController : public UObject
{
public:
	char pa_0[0x80];
	UArsenalViewModel* ViewModel; // 0x80(0x08)
	void SetSkinWithLevelAndChromaForGun(UEquippableDataAsset* Equippable, UEquippableSkinDataAsset* Skin, UEquippableSkinLevelDataAsset* SkinLevel, UEquippableSkinChromaDataAsset* Chroma); // Function ShooterGame.ArsenalViewController.SetSkinWithLevelAndChromaForGun // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x5262620

	//void EquipSkin(UEquippableSkinInventoryModel* Value); // Function ShooterGame.ArsenalViewController.EquipSkin // (Final|RequiredAPI|BlueprintAuthorityOnly|
}; 



class UGameFlowStateManager;
enum EGameFlowStateType 
{
	Initialization = 0,
	TransitionToMainMenu = 1,
	MainMenu = 2,
	TransitionToPregame = 3,
	Pregame = 4,
	TransitionToInGame = 5,
	InGame = 6,
	TransitionToWatchingReplay = 7,
	WatchingReplay = 8,
	TransitionToPlatformFaulted = 9,
	PlatformFaulted = 10,
	Invalid = 11,
	Count = 12,
	EGameFlowStateType_MAX = 13,
};

class UGameFlowState : UObject {
public:
//char pa[0x80];
	//EGameFlowStateType TransitionStateType;
};


class UWeaponDetailsSkin_C : UObject {
public:
	void SetSkin(struct UEquippableSkinDataAsset* NewSkinDataAsset, int32_t Quantity, void* AdditionalContext); // Function WeaponDetailsSkin.WeaponDetailsSkin_C.SetSkin // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x1934dc0

};


class UStaticGameFlowState : UGameFlowState {
public:
	//char pa[0x88];
	//EGameFlowStateType TransitionStateType; // 0x88(0x01) 80
};

class UInGameStateV2 : UStaticGameFlowState {
public:
	char pa[0x80];
	EGameFlowStateType TransitionStateType;
};


class UGameFlowStateManager : UObject
{
public:
	char pa[0xb0];
	UInGameStateV2* CurrentState; // 0xb0(0x08)
	UGameFlowState* GetCurrentState();
	UGameFlowStateManager* GetGameFlowStateManager(UWorld* WorldContextObject);


};




class UEquippableSkinChromaInventoryModel
{
public:
	
};


class UEquippableSkinLevelInventoryModel
{
public:


};

class UEquippableSkinInventoryModel : UObject
{
public:
	


}; 



class UMovementComponent : public UObject
{
public:
	bool IsCrouching(); // Function Engine.NavMovementComponent.IsCrouching
	void StopMovementImmediately(); // Function Engine.MovementComponent.StopMovementImmediately                                          // 0x0178(0x0008)
};

class  UShooterCharacterDeathReactionComponent : public UObject
{
public:
	AAresEquippable* GetKillingEquippable();
};

class ACharacter : public AActor
{
public:	        
	char pa_03[0x4f0];
	UMovementComponent* CharacterMovement; // 0x4f0(0x08) 
	char pa_01[0xA60];//need
	bool WasAlly; // 0xf58 (0x01)
	char pa_02[0x656];//need
	UAresOutlineComponent* Outline3P; // 0x1570(0x08)// 0x0478(0x0001)(Edit, BlueprintVisible, ZeroConstructor, DisableEditOnTemplate, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPublic)
	UShooterCharacterDeathReactionComponent* GetDeathReactionComponent();
	UAresOutlineComponent* GetAresOutlineComponent();

	USceneComponent* K2_GetRootComponent();

	FVector K2_GetActorLocation();

	float GetHealth();

	AAresPlayerStateBase* GetPlayerState();

	UAresInventory* GetInventory();

	bool CanJump();

	void Jump(bool bHide);
};


class Render {
public:

	wchar_t* s2wc(const char8_t* c);


};

namespace R { inline Render Render_; }

inline std::string WcharToString(const wchar_t* wstr) {
	wstring_convert<codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(wstr);
}


/** Implementation detail exposed for debug visualizers */
struct FNameEntryHeader
{
	uint16_t bIsWide : 1;
#ifdef WITH_CASE_PRESERVING_NAME
	uint16_t Len : 15;
#else
	static constexpr uint32_t ProbeHashBits = 5;
	uint16_t LowercaseProbeHash : ProbeHashBits;
	uint16_t Len : 10;
#endif
};
struct FNameEntryHandle
{
	uint16_t bIsWide : 1;
	uint16_t Len : 15;
};
/** A global deduplicated name stored in the global name table. */
struct FNameEntry {
	int32_t ComparisonId;
	FNameEntryHandle Header;
	union {
		char AnsiName[1024];
		wchar_t WideName[1024];
	};
	wchar_t const* GetWideName() const { return WideName; }
	char const* GetAnsiName() const { return AnsiName; }
	bool IsWide() const { return Header.bIsWide; }
};


__forceinline uint64_t decrypt_xor_keys(const uint32_t key, const uint64_t* state)
{
	const uint64_t hash =
		0x2545F4914F6CDD1DULL *
		(key ^ ((key ^ (key >> 15)) >> 12) ^ (key << 25));
	const uint64_t idx = hash % 7;
	uint64_t val = state[idx];
	const uint32_t hi = (uint32_t)(hash >> 32);
	const uint32_t mod7 = (uint32_t)(idx) % 7;

	if (mod7 == 0) {
		val = val - (2ULL * hi - 1);
	}
	else if (mod7 == 1) {
		uint64_t v = ~(uint64_t)(uint32_t)(hi + 2 * idx) ^ val;
		val = (v >> 1) ^ (((v >> 1) ^ (2 * v)) & 0xAAAAAAAAAAAAAAAAULL);
	}
	else if (mod7 == 2) {
		uint64_t v = val;
		v = (v >> 1) ^ (((v >> 1) ^ (2 * v)) & 0xAAAAAAAAAAAAAAAAULL);
		v = (v >> 2) ^ (((v >> 2) ^ (4 * v)) & 0xCCCCCCCCCCCCCCCCULL);
		v = (v >> 4) ^ (((v >> 4) ^ (16 * v)) & 0xF0F0F0F0F0F0F0F0ULL);
		v = (v >> 8) ^ (((v >> 8) ^ (v << 8)) & 0xFF00FF00FF00FF00ULL);
		val = (uint32_t)(hi + idx) + _rotr64(v, 32);
	}
	else if (mod7 == 3) {
		val = (val >> 1) ^ ~(uint64_t)(uint32_t)(hi + idx) ^ (((val >> 1) ^ (2 * val)) & 0xAAAAAAAAAAAAAAAAULL);
	}
	else if (mod7 == 4) {
		uint64_t v = ~(uint64_t)(uint32_t)(hi + 2 * idx) ^ val;
		uint8_t l = (uint8_t)(((int)hi + (int)idx) % 0x3F) + 1;
		uint8_t r = 63 * (uint8_t)(((int)hi + (int)idx) / 0x3F)
			- ((uint8_t)hi + (uint8_t)idx) + 63;
		val = (v << l) | (v >> r);
	}
	else if (mod7 == 5) {
		uint64_t v = (val >> 1) ^ (((val >> 1) ^ (2 * val)) & 0xAAAAAAAAAAAAAAAAULL);
		val = (uint32_t)(hi + idx) + v;
	}
	else if (mod7 == 6) {
		uint64_t v = val;
		v = (v >> 2) ^ (((v >> 2) ^ (4 * v)) & 0xCCCCCCCCCCCCCCCCULL);
		v = (v >> 4) ^ (((v >> 4) ^ (16 * v)) & 0xF0F0F0F0F0F0F0F0ULL);
		v = (v >> 8) ^ (((v >> 8) ^ (v << 8)) & 0xFF00FF00FF00FF00ULL);
		val = _rotr64(v, 32);
	}

	return val ^ key;
}
inline uintptr_t tempXorKey = NULL;
inline string GetName(uint32_t NameId)
{
	return string("[====]");
}

inline int getid(DWORD64 addr)
{
	if (addr)
		return *(int*)(addr + 0x18);

	return 0;
}


/*
		model:0 | weaponNum: 161
		model:5 | weaponNum: 37
		model:6 | weaponNum: 44
		model:7 | weaponNum: 97
		model:8 | weaponNum: 60
		model:9 | weaponNum: 98
		model:10 | weaponNum: 53
		model:11 | weaponNum: 43
		model:12 | weaponNum: 54
		model:13 | weaponNum: 193
		model:14 | weaponNum: 3
		model:15 | weaponNum: 66
		model:16 | weaponNum: 65
		model:17 | weaponNum: 37
		model:18 | weaponNum: 60
		model:19 | weaponNum: 62
		model:20 | weaponNum: 21
		model:21 | weaponNum: 48
		model:22 | weaponNum: 72
		model:23 | weaponNum: 44
*/

inline int Weapon_Equippable_index(string Name)
{
	if (Name == "BasePistol_C") return 13;
	if (Name == "SawedOffShotgun_C") return 17;
	if (Name == "AutomaticPistol_C") return 12;
	if (Name == "LugerPistol_C") return 15;
	if (Name == "CompactPistol_C") return 14;
	if (Name == "RevolverPistol_C") return 16;
	if (Name == "Vector_C") return 23;
	if (Name == "SubMachineGun_MP5_C") return 22;
	if (Name == "PumpShotgun_C") return 11;
	if (Name == "AutomaticShotgun_C") return 10;
	if (Name == "AssaultRifle_Burst_C") return 8;
	if (Name == "DMR_C") return 19;
	if (Name == "AssaultRifle_ACR_C") return 9;
	if (Name == "AssaultRifle_AK_C") return 7;
	if (Name == "LeverSniperRifle_C") return 21;
	if (Name == "DS_Gun_C") return 20;
	if (Name == "BoltSniper_C") return 18;
	if (Name == "LightMachineGun_C") return 6;
	if (Name == "HeavyMachineGun_C") return 5;
	if (Name == "Ability_Melee_Base_C") return 0;
}


inline void normalize(FVector& in)
{
	if (in.X > 89.f) in.X -= 360.f;
	if (in.X < -89.f) in.X += 360.f;

	while (in.Y > 180) in.Y -= 360;
	while (in.Y < -180) in.Y += 360;
	in.Z = 0;
}

inline FVector SmoothAim(FVector target, FVector delta_rotation, float smoothX, float smoothY) {
	FVector diff = { target.X - delta_rotation.X,target.Y - delta_rotation.Y,target.Z- delta_rotation.Z };
	normalize(diff);
	FVector diff2 = { diff.X / smoothX, diff.Y / smoothY, diff.Z / smoothY };

	return { delta_rotation.X + diff2.X, delta_rotation.Y + diff2.Y,delta_rotation.Z + diff2.Z };
}

inline void aimbot_recoil(FVector& CameraRot,
	FVector& ControlRotation,
	FVector Target, FVector& CameraPos, FVector& recoil, APlayerController* MyController) {
	FVector vector_pos = { Target.X - CameraPos.X, Target.Y-CameraPos.Y, Target.Z - CameraPos.Z };//Target - CameraPos;
	float distance = sqrtf(vector_pos.X * vector_pos.X + vector_pos.Y * vector_pos.Y + vector_pos.Z * vector_pos.Z);
	float X, Y, z;
	X = -((acosf(vector_pos.Z / distance) * (double)(180.0f / 3.14159265358979323846f)) - 90.f);
	Y = atan2f(vector_pos.Y, vector_pos.X) * (double)(180.0f / 3.14159265358979323846f);
	z = 0;

	FVector target_rotation = FVector(X, Y, z);
	FVector new_aim_rotation;
	new_aim_rotation.X = target_rotation.X - recoil.X - recoil.X;
	new_aim_rotation.Y = target_rotation.Y - recoil.Y - recoil.Y;
	new_aim_rotation.Z = 0;

	FVector new_rotation = SmoothAim(new_aim_rotation, ControlRotation, smoothX, smoothY);

	//if (new_rotation.X < 0) {
	//	//new_rotation.X += 360.f;
	//}

	//if (new_rotation.Y < 0) {
	//	//new_rotation.Y += 360.f;
	//}

	new_rotation.Z = 0;


	MyController->SetControlRotation(new_rotation);
}

class UAresPrespawnedEquippableCacheComponent : public UObject
{
public:
	void AuthOnEquippableDataAssetGunSkinModified(UEquippableDataAsset* EquippableDataAsset); // Function ShooterGame.AresPrespawnedEquippableCacheComponent.AuthOnEquippableDataAssetGunSkinModified // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x32cd990
};

class ShooterPlayerControllerDebugComponent : public UObject {
public:
	void ServerGunSkinModifySkinByEquippable(struct UEquippableSkinDataAsset* SkinDataAsset, UEquippableDataAsset* EquippableDataAsset,
		UEquippableSkinChromaDataAsset* GunSkinChromaAsset, int32_t Level); // Function ShooterGame.ShooterPlayerControllerDebugComponent.ServerGunSkinModifySkinByEquippable // (Final|RequiredAPI|BlueprintAuthorityOnly|BlueprintCosmetic|Net|NetReliableNetRequest|Exec|Native|Event|NetResponse|Static|NetMulticast|UbergraphFunction) // @ game+0x5c1bb00

};


