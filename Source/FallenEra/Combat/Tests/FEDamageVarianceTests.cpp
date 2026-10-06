#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"
#include "Combat/GameplayEffect/FEDamageExecutionCalculation.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_DamageVarianceTest, "FallenEra.Combat.Damage.Variance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_DamageVarianceTest::RunTest(const FString& Parameters)
{
	UFE_DamageExecutionCalculation* Calculation = NewObject<UFE_DamageExecutionCalculation>();
	TestEqual(TEXT("Default attack variance is ten percent"), Calculation->DamageVarianceRatio, 0.1f);
	TestTrue(TEXT("Minimum attack 20, defense 5, head x2"),
		FMath::IsNearlyEqual(Calculation->CalculateDamage(20.0f, 5.0f, 2.0f, -1.0f), 26.0f));
	TestEqual(TEXT("Center preserves the previous formula"), Calculation->CalculateDamage(20.0f, 5.0f, 2.0f, 0.0f), 30.0f);
	TestTrue(TEXT("Maximum attack 20, defense 5, head x2"),
		FMath::IsNearlyEqual(Calculation->CalculateDamage(20.0f, 5.0f, 2.0f, 1.0f), 34.0f));
	TestEqual(TEXT("Zero attack cannot cause random bonus damage"), Calculation->CalculateDamage(0.0f, 0.0f, 1.0f, 1.0f), 0.0f);
	TestEqual(TEXT("Negative attack cannot heal"), Calculation->CalculateDamage(-20.0f, 0.0f, 1.0f, 1.0f), 0.0f);
	TestEqual(TEXT("Defense above maximum attack always blocks"), Calculation->CalculateDamage(20.0f, 25.0f, 2.0f, 1.0f), 0.0f);
	TestEqual(TEXT("Defense threshold clamps damage, never heals"), Calculation->CalculateDamage(20.0f, 20.0f, 1.0f, -1.0f), 0.0f);
	TestTrue(TEXT("Strong roll can overcome defense equal to base attack"),
		FMath::IsNearlyEqual(Calculation->CalculateDamage(20.0f, 20.0f, 1.0f, 1.0f), 2.0f));
	TestEqual(TEXT("Negative region multiplier never heals"), Calculation->CalculateDamage(20.0f, 0.0f, -2.0f, 1.0f), 0.0f);
	TestEqual(TEXT("Invalid attack cannot corrupt Health"),
		Calculation->CalculateDamage(std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f, 0.0f), 0.0f);
	TestEqual(TEXT("Overflow cannot corrupt Health"),
		Calculation->CalculateDamage(std::numeric_limits<float>::max(), 0.0f, 2.0f, 1.0f), 0.0f);

	// An isolated, seeded stream makes statistical tests reproducible without reseeding gameplay RNG.
	FRandomStream Stream(61006);
	constexpr int32 SampleCount = 20000;
	double TotalDamage = 0.0;
	int32 CentralSamples = 0;
	bool bAllInRange = true;
	TSet<float> UniqueDamage;
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const float CenteredRoll = Stream.GetFraction() + Stream.GetFraction() - 1.0f;
		const float Damage = Calculation->CalculateDamage(100.0f, 20.0f, 1.0f, CenteredRoll);
		TotalDamage += Damage;
		CentralSamples += FMath::Abs(Damage - 80.0f) <= 5.0f ? 1 : 0;
		bAllInRange &= Damage >= 70.0f - KINDA_SMALL_NUMBER && Damage <= 90.0f + KINDA_SMALL_NUMBER;
		UniqueDamage.Add(Damage);
	}
	TestTrue(TEXT("Every roll respects proportional bounds"), bAllInRange);
	TestTrue(TEXT("Mean stays near attack minus defense when zero clamp is inactive"), FMath::Abs(TotalDamage / SampleCount - 80.0) < 0.15);
	TestTrue(TEXT("Triangular distribution places about 75 percent in the middle half"),
		FMath::Abs(static_cast<float>(CentralSamples) / SampleCount - 0.75f) < 0.02f);
	TestTrue(TEXT("Continuous rolls avoid the eleven-value limitation of 2d6"), UniqueDamage.Num() > SampleCount / 2);

	Calculation->DamageVarianceRatio = 0.0f;
	TestEqual(TEXT("Zero variance restores deterministic damage"), Calculation->CalculateDamage(20.0f, 5.0f, 2.0f, 1.0f), 30.0f);
	Calculation->DamageVarianceRatio = -1.0f;
	TestEqual(TEXT("Negative variance clamps to zero"), Calculation->CalculateDamage(20.0f, 5.0f, 2.0f, -1.0f), 30.0f);
	Calculation->DamageVarianceRatio = 2.0f;
	TestEqual(TEXT("Excess variance clamps at one"), Calculation->CalculateDamage(20.0f, 0.0f, 1.0f, 1.0f), 40.0f);
	TestEqual(TEXT("Roll outside support clamps to the endpoint"), Calculation->CalculateDamage(20.0f, 0.0f, 1.0f, 5.0f), 40.0f);
	Calculation->DamageVarianceRatio = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Invalid variance disables randomization"), Calculation->CalculateDamage(20.0f, 5.0f, 2.0f, 1.0f), 30.0f);
	return true;
}

#endif
