#include "Combat/Editor/FEAmmoWidgetEditorLibrary.h"

#if WITH_EDITOR
#include "BaseWidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

bool UFE_AmmoWidgetEditorLibrary::SetDesignerRoot(UObject* WidgetBlueprint, UWidget* Root)
{
#if WITH_EDITOR
	UBaseWidgetBlueprint* Blueprint = Cast<UBaseWidgetBlueprint>(WidgetBlueprint);
	if (!Blueprint || !Blueprint->WidgetTree || !Root || Root->GetOuter() != Blueprint->WidgetTree) { return false; }
	Blueprint->Modify();
	Blueprint->WidgetTree->Modify();
	Blueprint->WidgetTree->RootWidget = Root;
	Blueprint->WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		Widget->SetFlags(RF_Transactional);
		Widget->bIsVariable = true;
	});
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
#else
	return false;
#endif
}
