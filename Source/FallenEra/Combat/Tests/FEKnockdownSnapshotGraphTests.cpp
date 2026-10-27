#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_PoseSnapshot.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "Combat/Animation/FEEnemyAnimInstance.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "HAL/FileManager.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
	template<typename T>
	T* AddRecoveryNode(UEdGraph* Graph, FName Name, int32 X, int32 Y)
	{
		T* Node = NewObject<T>(Graph, Name, RF_Transactional);
		Graph->AddNode(Node, false, false);
		Node->CreateNewGuid();
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Node->PostPlacedNewNode();
		return Node;
	}

	bool AddRecoveryTransition(UEdGraph* Graph, UAnimStateNode* From, UAnimStateNode* To, bool bEnterRagdoll)
	{
		UAnimStateTransitionNode* Transition = AddRecoveryNode<UAnimStateTransitionNode>(Graph, NAME_None, 200, bEnterRagdoll ? 180 : -80);
		Transition->CrossfadeDuration = 0.0f;
		Transition->AllocateDefaultPins();
		Transition->CreateConnections(From, To);
		UAnimationTransitionGraph* Rule = CastChecked<UAnimationTransitionGraph>(Transition->BoundGraph);
		UK2Node_VariableGet* IsRagdoll = AddRecoveryNode<UK2Node_VariableGet>(Rule, NAME_None, -400, 0);
		IsRagdoll->VariableReference.SetSelfMember(TEXT("bIsRagdoll"));
		IsRagdoll->AllocateDefaultPins();
		UEdGraphPin* Condition = IsRagdoll->FindPin(TEXT("bIsRagdoll"));
		const UEdGraphSchema* Schema = Rule->GetSchema();
		if (!bEnterRagdoll)
		{
			UK2Node_CallFunction* Not = AddRecoveryNode<UK2Node_CallFunction>(Rule, NAME_None, -200, 0);
			Not->FunctionReference.SetExternalMember(TEXT("Not_PreBool"), UKismetMathLibrary::StaticClass());
			Not->AllocateDefaultPins();
			if (!Schema->TryCreateConnection(Condition, Not->FindPin(TEXT("A")))) { return false; }
			Condition = Not->FindPin(TEXT("ReturnValue"));
		}
		return Schema->TryCreateConnection(Condition, Rule->GetResultNode()->FindPin(TEXT("bCanEnterTransition")));
	}

	bool InstallRagdollStates(UEdGraph* Graph, UAnimGraphNode_TwoWayBlend* Blend)
	{
		if (Blend->FindPin(TEXT("A"))->LinkedTo.Num() != 1) { return false; }
		UEdGraphPin* NormalPose = Blend->FindPin(TEXT("A"))->LinkedTo[0];
		Blend->FindPin(TEXT("A"))->BreakAllPinLinks();
		Blend->FindPin(TEXT("B"))->BreakAllPinLinks();
		if (UAnimGraphNode_PoseSnapshot* Old = FindObject<UAnimGraphNode_PoseSnapshot>(Graph, TEXT("FE_RagdollSnapshot"))) { Old->DestroyNode(); }
		const UEdGraphSchema* Schema = Graph->GetSchema();
		UAnimGraphNode_SaveCachedPose* Cache = AddRecoveryNode<UAnimGraphNode_SaveCachedPose>(Graph, TEXT("FE_NormalCombatPose"), Blend->NodePosX - 700, Blend->NodePosY - 200);
		Cache->CacheName = TEXT("FE_NormalCombatPose");
		Cache->Node.CachePoseName = TEXT("FE_NormalCombatPose");
		Cache->AllocateDefaultPins();
		UAnimGraphNode_UseCachedPose* Normal = AddRecoveryNode<UAnimGraphNode_UseCachedPose>(Graph, TEXT("FE_UseNormalCombatPose"), Blend->NodePosX - 350, Blend->NodePosY - 150);
		Normal->SaveCachedPoseNode = Cache;
		Normal->AllocateDefaultPins();
		UAnimGraphNode_StateMachine* Machine = AddRecoveryNode<UAnimGraphNode_StateMachine>(Graph, TEXT("FE_RagdollStates"), Blend->NodePosX - 350, Blend->NodePosY + 150);
		FBlueprintEditorUtils::RenameGraph(Machine->EditorStateMachineGraph, TEXT("FE_RagdollStates"));
		// A client can first become relevant during recovery: select the snapshot on the very first update.
		Machine->Node.bSkipFirstUpdateTransition = false;
		Machine->AllocateDefaultPins();
		UEdGraph* States = Machine->EditorStateMachineGraph;
		UAnimStateNode* Ragdoll = AddRecoveryNode<UAnimStateNode>(States, TEXT("FE_InRagdoll"), 100, 0);
		FBlueprintEditorUtils::RenameGraph(Ragdoll->BoundGraph, TEXT("In Ragdoll"));
		Ragdoll->AllocateDefaultPins();
		UAnimStateNode* BlendOut = AddRecoveryNode<UAnimStateNode>(States, TEXT("FE_BlendOutPose"), 420, 0);
		FBlueprintEditorUtils::RenameGraph(BlendOut->BoundGraph, TEXT("Blend Out Pose"));
		BlendOut->AllocateDefaultPins();
		UAnimGraphNode_UseCachedPose* PhysicalBase = AddRecoveryNode<UAnimGraphNode_UseCachedPose>(Ragdoll->BoundGraph, TEXT("FE_PhysicalBasePose"), -200, 0);
		PhysicalBase->SaveCachedPoseNode = Cache;
		PhysicalBase->AllocateDefaultPins();
		UAnimGraphNode_PoseSnapshot* Snapshot = AddRecoveryNode<UAnimGraphNode_PoseSnapshot>(BlendOut->BoundGraph, TEXT("FE_RagdollSnapshot"), -200, 0);
		Snapshot->Node.SnapshotName = UFE_EnemyAnimInstance::RagdollPoseSnapshotName;
		Snapshot->AllocateDefaultPins();
		return Schema->TryCreateConnection(NormalPose, Cache->FindPin(TEXT("Pose"))) &&
			Schema->TryCreateConnection(Normal->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("A"))) &&
			Schema->TryCreateConnection(Machine->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("B"))) &&
			States->GetSchema()->TryCreateConnection(Machine->EditorStateMachineGraph->EntryNode->FindPin(TEXT("Entry")), Ragdoll->GetInputPin()) &&
			Ragdoll->BoundGraph->GetSchema()->TryCreateConnection(PhysicalBase->FindPin(TEXT("Pose")), Ragdoll->GetPoseSinkPinInsideState()) &&
			BlendOut->BoundGraph->GetSchema()->TryCreateConnection(Snapshot->FindPin(TEXT("Pose")), BlendOut->GetPoseSinkPinInsideState()) &&
			AddRecoveryTransition(States, Ragdoll, BlendOut, false) && AddRecoveryTransition(States, BlendOut, Ragdoll, true);
	}
}

/** Read-only normally. The explicit setup flag is for an intentional, backed-up editor asset migration. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_KnockdownSnapshotGraphTest,
	"FallenEra.Combat.Reaction.KnockdownSnapshotGraph", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_KnockdownSnapshotGraphTest::RunTest(const FString& Parameters)
{
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Combat/Character/EnemyCharacter/Animations/ABP_HT_EnemyCharacter.ABP_HT_EnemyCharacter"));
	if (!TestNotNull(TEXT("Enemy AnimBP"), Blueprint) ||
		!TestTrue(TEXT("Uses enemy animation base"), Blueprint->ParentClass->IsChildOf(UFE_EnemyAnimInstance::StaticClass()))) { return false; }
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UEdGraph* Graph = nullptr;
	UAnimGraphNode_Root* Root = nullptr;
	for (UEdGraph* Candidate : Graphs)
	{
		if (Candidate->GetFName() != TEXT("AnimGraph")) { continue; }
		Graph = Candidate;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if ((Root = Cast<UAnimGraphNode_Root>(Node))) { break; }
		}
		break;
	}
	if (!TestNotNull(TEXT("Main animation output"), Root)) { return false; }
	UAnimGraphNode_TwoWayBlend* Blend = FindObject<UAnimGraphNode_TwoWayBlend>(Graph, TEXT("FE_RagdollRecoveryBlend"));
	if (!Blend && FParse::Param(FCommandLine::Get(), TEXT("FEConfigureKnockdownSnapshot")))
	{
		UEdGraphPin* Result = Root->FindPin(TEXT("Result"));
		if (!TestTrue(TEXT("Existing final pose preserved"), Result && Result->LinkedTo.Num() == 1)) { return false; }
		const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("Backups/KnockdownSnapshot") /
			FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) / TEXT("ABP_HT_EnemyCharacter.uasset");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
		if (!TestEqual(TEXT("Backup current user asset before graph edit"), IFileManager::Get().Copy(*Backup, *Filename), COPY_OK)) { return false; }
		AddInfo(TEXT("AnimBP backup: ") + FPaths::ConvertRelativePathToFull(Backup));
		UEdGraphPin* PreviousOutput = Result->LinkedTo[0];
		Blueprint->Modify();
		Graph->Modify();
		Blend = AddRecoveryNode<UAnimGraphNode_TwoWayBlend>(Graph, TEXT("FE_RagdollRecoveryBlend"), Root->NodePosX, Root->NodePosY);
		// Update the montage even on the first frame, when the physical snapshot has full weight.
		FBoolProperty* AlwaysUpdate = FindFProperty<FBoolProperty>(FAnimNode_TwoWayBlend::StaticStruct(), TEXT("bAlwaysUpdateChildren"));
		AlwaysUpdate->SetPropertyValue_InContainer(&Blend->BlendNode, true);
		Blend->AllocateDefaultPins();
		UAnimGraphNode_PoseSnapshot* Snapshot = AddRecoveryNode<UAnimGraphNode_PoseSnapshot>(Graph, TEXT("FE_RagdollSnapshot"), Root->NodePosX - 400, Root->NodePosY + 240);
		Snapshot->Node.SnapshotName = UFE_EnemyAnimInstance::RagdollPoseSnapshotName;
		Snapshot->AllocateDefaultPins();
		UK2Node_VariableGet* Alpha = AddRecoveryNode<UK2Node_VariableGet>(Graph, TEXT("FE_RagdollPoseAlpha"), Root->NodePosX - 400, Root->NodePosY + 420);
		Alpha->VariableReference.SetSelfMember(TEXT("RagdollPoseAlpha"));
		Alpha->AllocateDefaultPins();
		Root->NodePosX += 400;
		Result->BreakAllPinLinks();
		const UEdGraphSchema* Schema = Graph->GetSchema();
		const bool bConnected = Schema->TryCreateConnection(PreviousOutput, Blend->FindPin(TEXT("A"))) &&
			Schema->TryCreateConnection(Snapshot->FindPin(TEXT("Pose")), Blend->FindPin(TEXT("B"))) &&
			Schema->TryCreateConnection(Alpha->FindPin(TEXT("RagdollPoseAlpha")), Blend->FindPin(TEXT("Alpha"))) &&
			Schema->TryCreateConnection(Blend->FindPin(TEXT("Pose")), Result);
		if (!TestTrue(TEXT("Snapshot and normal output connected"), bConnected)) { return false; }
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		if (!TestEqual(TEXT("Updated graph compiles"), Results.NumErrors, 0)) { return false; }
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!TestTrue(TEXT("Save updated AnimBP"), UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args))) { return false; }
	}
	if (!TestNotNull(TEXT("Final snapshot recovery blend installed"), Blend)) { return false; }
	UAnimGraphNode_StateMachine* Machine = FindObject<UAnimGraphNode_StateMachine>(Graph, TEXT("FE_RagdollStates"));
	if (!Machine && FParse::Param(FCommandLine::Get(), TEXT("FEConfigureKnockdownSnapshot")))
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("Backups/KnockdownSnapshot") /
			FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) / TEXT("ABP_HT_EnemyCharacter_BeforeRagdollStates.uasset");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
		if (!TestEqual(TEXT("Backup before installing recovery states"), IFileManager::Get().Copy(*Backup, *Filename), COPY_OK)) { return false; }
		AddInfo(TEXT("AnimBP backup: ") + FPaths::ConvertRelativePathToFull(Backup));
		Blueprint->Modify();
		Graph->Modify();
		if (!TestTrue(TEXT("Install ALS-style ragdoll states while preserving normal combat pose"), InstallRagdollStates(Graph, Blend))) { return false; }
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		if (!TestEqual(TEXT("Ragdoll state graph compiles"), Results.NumErrors, 0)) { return false; }
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!TestTrue(TEXT("Save ragdoll states"), UPackage::SavePackage(Blueprint->GetOutermost(), Blueprint, *Filename, Args))) { return false; }
		Machine = FindObject<UAnimGraphNode_StateMachine>(Graph, TEXT("FE_RagdollStates"));
	}
	if (!TestNotNull(TEXT("ALS-style ragdoll state machine installed"), Machine)) { return false; }
	TestFalse(TEXT("No default pose on first recovery update"), Machine->Node.bSkipFirstUpdateTransition);
	TestTrue(TEXT("Recovery wraps the final output, after slots and IK"), Root->FindPin(TEXT("Result"))->LinkedTo.Contains(Blend->FindPin(TEXT("Pose"))));
	TestEqual(TEXT("Normal animation remains connected"), Blend->FindPin(TEXT("A"))->LinkedTo.Num(), 1);
	TestEqual(TEXT("Snapshot alpha connected"), Blend->FindPin(TEXT("Alpha"))->LinkedTo.Num(), 1);
	UAnimStateNode* BlendOut = FindObject<UAnimStateNode>(Machine->EditorStateMachineGraph, TEXT("FE_BlendOutPose"));
	if (!TestNotNull(TEXT("Blend Out Pose state"), BlendOut)) { return false; }
	UAnimGraphNode_PoseSnapshot* Snapshot = FindObject<UAnimGraphNode_PoseSnapshot>(BlendOut->BoundGraph, TEXT("FE_RagdollSnapshot"));
	if (TestNotNull(TEXT("Named snapshot node"), Snapshot))
	{
		TestEqual(TEXT("Snapshot name matches capture"), Snapshot->Node.SnapshotName, UFE_EnemyAnimInstance::RagdollPoseSnapshotName);
		TestEqual(TEXT("Named mode avoids copying a bone array through Blueprint each frame"), Snapshot->Node.Mode, ESnapshotSourceMode::NamedSnapshot);
	}
	return true;
}

#endif
