#include "Combat/Component/FECombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Interface/FECombatPresentation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"

void UFE_CombatComponent::StartReloadPresentation(UAnimMontage* CharacterMontage, UAnimMontage* WeaponMontage, float Duration)
{
	if (!GetOwner()->HasAuthority() || !FMath::IsFinite(Duration) || Duration <= 0.0f) { return; }
	++ReloadState.Revision;
	ReloadState.StartServerTime = GetPresentationServerTime();
	ReloadState.Duration = Duration;
	ReloadState.CharacterMontage = CharacterMontage;
	ReloadState.WeaponMontage = WeaponMontage;
	if (const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>())
	{
		ReloadState.WeaponTag = Equipment->GetCurrentItemTag();
	}
	OnRep_ReloadState();
	GetOwner()->ForceNetUpdate();
}

void UFE_CombatComponent::EndReloadPresentation()
{
	if (!GetOwner()->HasAuthority() || ReloadState.StartServerTime < 0.0f) { return; }
	const uint16 Revision = ReloadState.Revision + 1;
	ReloadState = FFE_ReloadPresentationState();
	ReloadState.Revision = Revision;
	OnRep_ReloadState();
	GetOwner()->ForceNetUpdate();
}

void UFE_CombatComponent::OnRep_ReloadState()
{
	if (ReloadAssetLoadHandle) { ReloadAssetLoadHandle->CancelHandle(); ReloadAssetLoadHandle.Reset(); }
	StopReloadPresentationLocal();
	if (ReloadState.StartServerTime < 0.0f || bDeathCollisionDisabled || GetNetMode() == NM_DedicatedServer) { return; }
	TArray<FSoftObjectPath> Paths;
	if (!ReloadState.CharacterMontage.IsNull() && !ReloadState.CharacterMontage.IsValid()) { Paths.Add(ReloadState.CharacterMontage.ToSoftObjectPath()); }
	if (!ReloadState.WeaponMontage.IsNull() && !ReloadState.WeaponMontage.IsValid()) { Paths.AddUnique(ReloadState.WeaponMontage.ToSoftObjectPath()); }
	if (Paths.IsEmpty()) { RestoreReloadPresentation(); return; }
	ReloadAssetLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateUObject(this, &ThisClass::RestoreReloadPresentation));
}

void UFE_CombatComponent::RestoreReloadPresentation()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>();
	const float Elapsed = FMath::Max(0.0f, GetPresentationServerTime() - ReloadState.StartServerTime);
	if (!Character || !Equipment || !Equipment->GetCurrentWeaponData() ||
		Equipment->GetCurrentItemTag() != ReloadState.WeaponTag || ReloadState.StartServerTime < 0.0f ||
		Elapsed >= ReloadState.Duration || bDeathCollisionDisabled || GetNetMode() == NM_DedicatedServer) { return; }
	ReloadCharacterMontage = ReloadState.CharacterMontage.Get();
	ReloadWeaponMontage = ReloadState.WeaponMontage.Get();
	const auto Restore = [Elapsed](USkeletalMeshComponent* Mesh, UAnimMontage* Montage)
	{
		if (!Mesh || !Montage) { return; }
		const float Position = Elapsed * Montage->RateScale;
		if (Position >= Montage->GetPlayLength()) { return; }
		if (UAnimInstance* Anim = Mesh->GetAnimInstance())
		{
			if (!Anim->Montage_IsActive(Montage)) { Anim->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, Position); }
			else { Anim->Montage_SetPosition(Montage, Position); }
		}
	};
	Restore(Character->GetMesh(), ReloadCharacterMontage);
	if (Character->IsLocallyControlled()) { Restore(IFE_CombatPresentation::FindFirstPersonMesh(Character), ReloadCharacterMontage); }
	Restore(Cast<USkeletalMeshComponent>(Equipment->GetEquippedWorldWeaponMesh()), ReloadWeaponMontage);
	Restore(Cast<USkeletalMeshComponent>(Equipment->GetEquippedFirstPersonWeaponMesh()), ReloadWeaponMontage);
}

void UFE_CombatComponent::StopReloadPresentationLocal()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const auto Stop = [](USkeletalMeshComponent* Mesh, UAnimMontage* Montage)
	{
		if (Mesh && Montage)
		{
			if (UAnimInstance* Anim = Mesh->GetAnimInstance(); Anim && Anim->Montage_IsActive(Montage)) { Anim->Montage_Stop(0.1f, Montage); }
		}
	};
	if (Character)
	{
		Stop(Character->GetMesh(), ReloadCharacterMontage);
		Stop(IFE_CombatPresentation::FindFirstPersonMesh(Character), ReloadCharacterMontage);
	}
	if (const UFE_EquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UFE_EquipmentComponent>())
	{
		Stop(Cast<USkeletalMeshComponent>(Equipment->GetEquippedWorldWeaponMesh()), ReloadWeaponMontage);
		Stop(Cast<USkeletalMeshComponent>(Equipment->GetEquippedFirstPersonWeaponMesh()), ReloadWeaponMontage);
	}
	ReloadCharacterMontage = nullptr;
	ReloadWeaponMontage = nullptr;
}
