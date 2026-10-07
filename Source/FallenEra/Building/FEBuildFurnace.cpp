// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildFurnace.h"
#include "FEBuildPieceDefinition.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

namespace
{
	/** 가공·연소 판정 간격. 진행도는 이 간격으로만 바뀐다 */
	constexpr float FireTickInterval = 0.5f;
}

AFEBuildFurnace::AFEBuildFurnace()
{
	Capacity = SlotNum; // 연료·재료·결과 고정. BP 자식에서 Capacity 를 바꾸지 말 것
	EnsureSlots();

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(Mesh);
	FireLight->SetVisibility(false);
}

void AFEBuildFurnace::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFEBuildFurnace, bIsLit);
	DOREPLIFETIME(AFEBuildFurnace, ProcessPercent);
}

void AFEBuildFurnace::EnsureSlots()
{
	const int32 OldNum = Contents.Num();
	Contents.SetNum(SlotNum);
	for (int32 Index = OldNum; Index < SlotNum; ++Index)
	{
		// FFEBuildItemCost 의 기본 Count 는 1 이다. 빈 칸은 명시적으로 0 으로
		Contents[Index] = FFEBuildItemCost();
		Contents[Index].Count = 0;
	}
}

// ---- 칸 ----

int32 AFEBuildFurnace::GetSlotForItem(FGameplayTag ItemTag) const
{
	if (FuelSeconds.Contains(ItemTag))
	{
		return FuelSlot;
	}
	const bool bIsInput = Recipes.ContainsByPredicate([&ItemTag](const FFEFurnaceRecipe& Recipe) { return Recipe.InputTag == ItemTag; });
	return bIsInput ? InputSlot : INDEX_NONE;
}

bool AFEBuildFurnace::AcceptsItem(FGameplayTag ItemTag, FText& OutReason) const
{
	const int32 SlotIndex = GetSlotForItem(ItemTag);
	if (SlotIndex == INDEX_NONE)
	{
		OutReason = LOCTEXT("FurnaceReject", "연료나 가공할 재료만 넣을 수 있습니다");
		return false;
	}
	const FFEBuildItemCost& Slot = Contents[SlotIndex];
	if (Slot.Count > 0 && Slot.ItemTag != ItemTag)
	{
		OutReason = LOCTEXT("FurnaceSlotBusy", "칸에 다른 아이템이 들어 있습니다");
		return false;
	}
	return true;
}

int32 AFEBuildFurnace::StoreItems(FGameplayTag ItemTag, int32 Count)
{
	FText Unused;
	if (!HasAuthority() || Count <= 0 || !AcceptsItem(ItemTag, Unused))
	{
		return 0;
	}

	FFEBuildItemCost& Slot = Contents[GetSlotForItem(ItemTag)];
	const int32 Added = FMath::Min(Count, MaxStackSize - Slot.Count);
	if (Added <= 0)
	{
		return 0; // 칸이 가득
	}

	Slot.ItemTag = ItemTag;
	Slot.Count += Added;
	OnRep_Contents();
	return Added;
}

void AFEBuildFurnace::ClearSlot(int32 SlotIndex)
{
	// 칸 위치가 역할이므로 지우지 않고 비워 둔다
	Contents[SlotIndex] = FFEBuildItemCost();
	Contents[SlotIndex].Count = 0;
}

FText AFEBuildFurnace::GetSlotLabel(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case FuelSlot: 
		return LOCTEXT("FurnaceFuel", "연료");
	case InputSlot: 
		return LOCTEXT("FurnaceInput", "재료");
	case OutputSlot: 
		return LOCTEXT("FurnaceOutput", "결과");
	default: 
		return FText::GetEmpty();
	}
}

// ---- 불 ----

const FFEFurnaceRecipe* AFEBuildFurnace::GetActiveRecipe() const
{
	const FFEBuildItemCost& Input = Contents[InputSlot];
	if (Input.Count <= 0) return nullptr;

	const FFEFurnaceRecipe* Recipe = Recipes.FindByPredicate([&Input](const FFEFurnaceRecipe& Candidate) { return Candidate.InputTag == Input.ItemTag; });
	if (Recipe == nullptr) return nullptr;

	// 결과 칸이 비었거나, 같은 결과가 999 미만일 때만 진행
	const FFEBuildItemCost& Output = Contents[OutputSlot];
	const bool bOutputHasRoom = Output.Count <= 0 || (Output.ItemTag == Recipe->OutputTag && Output.Count < MaxStackSize);
	return bOutputHasRoom ? Recipe : nullptr;
}

bool AFEBuildFurnace::CanIgnite(FText& OutReason) const
{
	if (GetActiveRecipe() == nullptr)
	{
		OutReason = LOCTEXT("FurnaceNothing", "가공할 재료가 없거나 결과 칸에 자리가 없습니다");
		return false;
	}
	const FFEBuildItemCost& Fuel = Contents[FuelSlot];
	if (Fuel.Count <= 0 || !FuelSeconds.Contains(Fuel.ItemTag))
	{
		OutReason = LOCTEXT("FurnaceNoFuel", "연료가 없습니다");
		return false;
	}
	return true;
}

bool AFEBuildFurnace::ConsumeFuelUnit()
{
	FFEBuildItemCost& Fuel = Contents[FuelSlot];
	const float* Seconds = Fuel.Count > 0 ? FuelSeconds.Find(Fuel.ItemTag) : nullptr;
	if (Seconds == nullptr) return false;

	FuelLeft += *Seconds;
	if (--Fuel.Count <= 0)
	{
		ClearSlot(FuelSlot);
	}
	OnRep_Contents();
	return true;
}

void AFEBuildFurnace::SetLit(bool bNewLit)
{
	FText Reason;
	const bool bCanChange = HasAuthority() && bIsLit != bNewLit && (!bNewLit || CanIgnite(Reason));
	if (!bCanChange) return;

	bIsLit = bNewLit;
	FuelLeft = 0.f;       // ponytail: 꺼질 때 타던 연료 1개의 남은 시간과 진행도를 버린다. 아깝다는 피드백이 오면 보존
	ProcessElapsed = 0.f;
	ProcessPercent = 0;

	if (bIsLit)
	{
		ConsumeFuelUnit(); // 첫 연료에 불을 붙인다 (CanIgnite 가 연료 있음을 보장)
		GetWorldTimerManager().SetTimer(FireTimer, this, &AFEBuildFurnace::TickFire, FireTickInterval, true);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(FireTimer);
	}

	UE_LOG(LogFEBuilding, Log, TEXT("%s fire %s"), *GetName(), bIsLit ? TEXT("lit") : TEXT("out"));
	OnRep_IsLit();          // 서버 로컬 반영
	OnRep_ProcessPercent();
}

void AFEBuildFurnace::TickFire()
{
	const FFEFurnaceRecipe* Recipe = GetActiveRecipe();
	if (Recipe == nullptr)
	{
		SetLit(false); // 재료가 떨어졌거나 결과 칸이 막힘 → 자동 소화
		return;
	}

	FuelLeft -= FireTickInterval;
	ProcessElapsed += FireTickInterval;

	if (ProcessElapsed >= Recipe->Seconds)
	{
		ProcessElapsed = 0.f;

		FFEBuildItemCost& Input = Contents[InputSlot];
		if (--Input.Count <= 0)
		{
			ClearSlot(InputSlot);
		}
		FFEBuildItemCost& Output = Contents[OutputSlot];
		Output.ItemTag = Recipe->OutputTag; // Recipe 는 Recipes 배열을 가리키므로 칸을 비워도 유효
		++Output.Count;

		OnRep_Contents();
		UE_LOG(LogFEBuilding, Log, TEXT("%s produced %s (now %d)"), *GetName(), *Output.ItemTag.ToString(), Output.Count);
	}

	if (FuelLeft <= 0.f && !ConsumeFuelUnit())
	{
		SetLit(false); // 연료 소진
		return;
	}

	const uint8 NewPercent = static_cast<uint8>(FMath::Clamp(ProcessElapsed / Recipe->Seconds, 0.f, 1.f) * 100.f);
	if (NewPercent != ProcessPercent)
	{
		ProcessPercent = NewPercent;
		OnRep_ProcessPercent();
	}
}

bool AFEBuildFurnace::IsLit() const
{
	return bIsLit;
}

uint8 AFEBuildFurnace::GetProcessPercent() const
{
	return ProcessPercent;
}

void AFEBuildFurnace::OnRep_IsLit()
{
	FireLight->SetVisibility(bIsLit);
	OnLitChanged(bIsLit);
	OnFurnaceStateChangedNative.Broadcast();
}

void AFEBuildFurnace::OnRep_ProcessPercent()
{
	OnFurnaceStateChangedNative.Broadcast();
}

// ---- 상호작용 / 저장 ----

FText AFEBuildFurnace::GetInteractTextBuilt(AActor* InstigatorActor) const
{
	const UFEBuildPieceDefinition* PieceDefinition = GetDefinition(); // 부모의 private Definition 과 이름이 겹치지 않게
	return FText::Format(LOCTEXT("FurnacePrompt", "{0} 사용"), PieceDefinition ? PieceDefinition->DisplayName : FText::GetEmpty());
}

void AFEBuildFurnace::ReadRecord(const FFEBuildPieceRecord& Record)
{
	Super::ReadRecord(Record);
	EnsureSlots(); // 레코드가 3칸이 아니어도 인덱스 접근이 깨지지 않게. 점화 상태는 저장하지 않아 꺼진 채로 복원
	OnRep_Contents();
}

#undef LOCTEXT_NAMESPACE