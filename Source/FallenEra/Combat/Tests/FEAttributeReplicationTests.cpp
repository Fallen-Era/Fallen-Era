#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AttributeLifetimeReplicationTest,
	"FallenEra.Combat.Replication.AttributeLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_AttributeLifetimeReplicationTest::RunTest(const FString& Parameters)
{
	// No World, ASC or NetDriver: mirror the debugger's direct query before network initialization.
	UFallenEraAttributeSet* Attributes = NewObject<UFallenEraAttributeSet>();
	UClass* AttributeClass = Attributes->GetClass();
	AddInfo(FString::Printf(TEXT("Before debugger query: initialized=%d, Health.RepIndex=%d, Accuracy.RepIndex=%d"),
		AttributeClass->HasAnyClassFlags(CLASS_ReplicationDataIsSetUp),
		FindFieldChecked<FProperty>(AttributeClass, TEXT("Health"))->RepIndex,
		FindFieldChecked<FProperty>(AttributeClass, TEXT("Accuracy"))->RepIndex));

	TArray<FLifetimeProperty> LifetimeProps;
	Attributes->GetLifetimeReplicatedProps(LifetimeProps);
	TestTrue(TEXT("Direct query initializes property indices"), AttributeClass->HasAnyClassFlags(CLASS_ReplicationDataIsSetUp));
	TestEqual(TEXT("All ten attributes have separate registrations"), LifetimeProps.Num(), 10);
	TSet<uint16> RepIndices;
	for (const FLifetimeProperty& Entry : LifetimeProps)
	{
		RepIndices.Add(Entry.RepIndex);
	}
	TestEqual(TEXT("No attributes share a replication index"), RepIndices.Num(), LifetimeProps.Num());

	for (const FName Name : {FName(TEXT("Health")), FName(TEXT("MaxHealth")), FName(TEXT("Stamina")),
		FName(TEXT("MaxStamina")), FName(TEXT("AttackPower")), FName(TEXT("DefensePower")),
		FName(TEXT("KnockbackResistance")), FName(TEXT("Accuracy")), FName(TEXT("RecoilControl")), FName(TEXT("StaminaRecoveryRate"))})
	{
		const FProperty* Property = FindFieldChecked<FProperty>(AttributeClass, Name);
		const FLifetimeProperty* Entry = LifetimeProps.FindByPredicate(
			[Property](const FLifetimeProperty& Item) { return Item.RepIndex == Property->RepIndex; });
		if (TestNotNull(*FString::Printf(TEXT("%s has a lifetime entry"), *Name.ToString()), Entry))
		{
			const ELifetimeCondition Expected = Name == TEXT("Accuracy") || Name == TEXT("RecoilControl") || Name == TEXT("StaminaRecoveryRate")
				? COND_OwnerOnly : COND_None;
			TestTrue(*FString::Printf(TEXT("%s preserves its network audience"), *Name.ToString()), Entry->Condition == Expected);
			TestTrue(*FString::Printf(TEXT("%s always invokes GAS RepNotify"), *Name.ToString()), Entry->RepNotifyCondition == REPNOTIFY_Always);
		}
	}
	TestFalse(TEXT("Healing remains a non-replicated meta attribute"),
		FindFieldChecked<FProperty>(AttributeClass, TEXT("Healing"))->HasAnyPropertyFlags(CPF_Net));

	// Re-selecting actors must be safe both with a fresh list and with the same list.
	TArray<FLifetimeProperty> SecondQuery;
	Attributes->GetLifetimeReplicatedProps(SecondQuery);
	Attributes->GetLifetimeReplicatedProps(LifetimeProps);
	TestEqual(TEXT("Repeated query never duplicates registrations"), LifetimeProps.Num(), SecondQuery.Num());
	for (const FLifetimeProperty& Entry : LifetimeProps)
	{
		const FLifetimeProperty* Other = SecondQuery.FindByPredicate(
			[&Entry](const FLifetimeProperty& Item) { return Item.RepIndex == Entry.RepIndex; });
		if (TestNotNull(TEXT("Repeated query keeps the same indices"), Other))
		{
			TestTrue(TEXT("Repeated query keeps the same conditions"), Entry.Condition == Other->Condition);
			TestTrue(TEXT("Repeated query keeps GAS notifications"), Entry.RepNotifyCondition == Other->RepNotifyCondition);
		}
	}
	return true;
}

#endif
