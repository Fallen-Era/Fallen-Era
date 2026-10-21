#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FEAmmoWidgetEditorLibrary.generated.h"

class UWidget;

/** Editor scripting bridge for the UMG designer properties not exposed by UE Python. */
UCLASS()
class FALLENERA_API UFE_AmmoWidgetEditorLibrary : public UBlueprintFunctionLibrary
{

	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Editor|Ammo Widgets", meta=(DevelopmentOnly))
	static bool SetDesignerRoot(UObject* WidgetBlueprint, UWidget* Root);
};
