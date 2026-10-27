#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FallenEraCharacter.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimMontage.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Animation/FEAnimNotify_SendGameplayEvent.h"
#include "FallenEraPlayerState.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_FirstPersonHierarchyTest, "FallenEra.Combat.Presentation.FirstPersonHierarchy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_FirstPersonHierarchyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	for (const TCHAR* BlueprintPath : {
		TEXT("/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter.BP_HT_PlayerCharacter_C"),
		TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter.BP_FirstPersonCharacter_C") })
	{
		UClass* CharacterClass = LoadClass<AFallenEraCharacter>(nullptr, BlueprintPath);
		if (!TestNotNull(BlueprintPath, CharacterClass)) { continue; }
		AFallenEraCharacter* Character = World->SpawnActor<AFallenEraCharacter>(CharacterClass);
		if (!TestNotNull(TEXT("Character spawns"), Character)) { continue; }
		if (!Character->IsActorInitialized()) { Character->PostInitializeComponents(); }
		USkeletalMeshComponent* Arms = Character->GetFirstPersonMesh();
		USkeletalMeshComponent* Legs = Character->GetFirstPersonLegsMesh();
		UCameraComponent* Camera = Character->GetFirstPersonCameraComponent();
		TestTrue(TEXT("World body remains below capsule"), Character->GetMesh()->GetAttachParent() == Character->GetCapsuleComponent());
		TestTrue(TEXT("Camera belongs to capsule, not head"), Camera->GetAttachParent() == Character->GetCapsuleComponent());
		TestTrue(TEXT("Camera has no head socket"), Camera->GetAttachSocketName().IsNone());
		TestTrue(TEXT("Arms follow camera"), Arms->GetAttachParent() == Camera);
		TestTrue(TEXT("Legs follow capsule"), Legs->GetAttachParent() == Character->GetCapsuleComponent());
		TestTrue(TEXT("Split arms asset assigned"), Arms->GetSkeletalMeshAsset()->GetName() == TEXT("SKM_HT_Arms"));
		TestTrue(TEXT("Split legs asset assigned"), Legs->GetSkeletalMeshAsset()->GetName() == TEXT("SKM_HT_Legs"));
		TestTrue(TEXT("Legs use body pose without extra AnimBP"), Legs->LeaderPoseComponent.Get() == Character->GetMesh());
		TestTrue(TEXT("Arms owner-only"), Arms->bOnlyOwnerSee);
		TestTrue(TEXT("Legs owner-only"), Legs->bOnlyOwnerSee);
		TestTrue(TEXT("Legs have no collision"), Legs->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Legs do not duplicate body shadows"), Legs->CastShadow);
		TestTrue(TEXT("Legs stay world-space"), Legs->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
		TestTrue(TEXT("Authored first-person leg offset is preserved"), Legs->GetRelativeTransform().Equals(
			CharacterClass->GetDefaultObject<AFallenEraCharacter>()->GetFirstPersonLegsMesh()->GetRelativeTransform()));
		for (int32 Initialization = 0; Initialization < 2; ++Initialization)
		{
			if (Initialization) { Arms->InitAnim(true); }
			UAnimInstance* AnimInstance = Arms->GetAnimInstance();
			if (!TestNotNull(TEXT("Existing arm AnimBP still initializes"), AnimInstance)) { break; }
			const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(AnimInstance->GetClass());
			for (const FStructProperty* Property : AnimClass->GetAnimNodeProperties())
			{
				if (Property->Struct == FAnimNode_CopyPoseFromMesh::StaticStruct())
				{
					TestTrue(TEXT("Copy Pose source survives camera attachment and animation reinitialization"),
						Property->ContainerPtrToValuePtr<FAnimNode_CopyPoseFromMesh>(AnimInstance)->SourceMeshComponent == Character->GetMesh());
				}
			}
			// Independent first-person AnimBPs do not need Copy Pose; validate bindings only when present.
		}
		Character->Destroy();
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_FirstPersonAttackMontageTest, "FallenEra.Combat.Presentation.FirstPersonAttackMontage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_FirstPersonAttackMontageTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UClass* CharacterClass = LoadClass<AFallenEraCharacter>(nullptr,
		TEXT("/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter.BP_HT_PlayerCharacter_C"));
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/Combat/Character/RetargetAnimation/AM_Rifle_Fire.AM_Rifle_Fire"));
	if (!TestNotNull(TEXT("Player class"), CharacterClass) || !TestNotNull(TEXT("Attack montage"), Montage))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	AFallenEraCharacter* Character = World->SpawnActor<AFallenEraCharacter>(CharacterClass);
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Character);
	TestTrue(TEXT("Test player is locally controlled"), Character->IsLocallyControlled());
	Character->GetMesh()->InitAnim(true);
	Character->GetFirstPersonMesh()->InitAnim(true);
	UAnimInstance* BodyAnim = Character->GetMesh()->GetAnimInstance();
	UAnimInstance* ArmsAnim = Character->GetFirstPersonMesh()->GetAnimInstance();
	if (TestNotNull(TEXT("Body animation instance"), BodyAnim) && TestNotNull(TEXT("Arms animation instance"), ArmsAnim))
	{
		UFE_CombatComponent* Combat = Character->FindComponentByClass<UFE_CombatComponent>();
		Combat->PlayAttackMontage(Montage, true);
		TestTrue(TEXT("Body montage plays"), BodyAnim->Montage_IsPlaying(Montage));
		TestTrue(TEXT("Independent local arms montage plays"), ArmsAnim->Montage_IsPlaying(Montage));
		Combat->StopWeaponActionPresentation(true);
		TestTrue(TEXT("Body montage starts stopping on cancel"), BodyAnim->GetActiveInstanceForMontage(Montage) == nullptr ||
			BodyAnim->GetActiveInstanceForMontage(Montage)->IsStopped());
		TestTrue(TEXT("Arms montage starts stopping on cancel"), ArmsAnim->GetActiveInstanceForMontage(Montage) == nullptr ||
			ArmsAnim->GetActiveInstanceForMontage(Montage)->IsStopped());
	}
	AFallenEraPlayerState* PlayerState = World->SpawnActor<AFallenEraPlayerState>();
	Character->SetPlayerState(PlayerState);
	PlayerState->InitializeAbilitySystem(Character);
	int32 EventCount = 0;
	UFE_AnimNotify_SendGameplayEvent* Notify = NewObject<UFE_AnimNotify_SendGameplayEvent>();
	Notify->EventTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Attack.TraceStart"));
	PlayerState->GetFallenEraAbilitySystemComponent()->GenericGameplayEventCallbacks.FindOrAdd(Notify->EventTag)
		.AddLambda([&EventCount](const FGameplayEventData* Payload) { ++EventCount; });
	Notify->Notify(Character->GetMesh(), Montage, FAnimNotifyEventReference());
	Notify->Notify(Character->GetFirstPersonMesh(), Montage, FAnimNotifyEventReference());
	TestEqual(TEXT("Mirrored arms montage does not send gameplay events twice"), EventCount, 1);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
