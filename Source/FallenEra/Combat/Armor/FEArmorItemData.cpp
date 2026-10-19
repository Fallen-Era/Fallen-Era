#include "Combat/Armor/FEArmorItemData.h"
#include "Serialization/Archive.h"

void UFE_ArmorItemData::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving()) { ItemDataVersion = 1; }
	Super::Serialize(Ar);
}

void UFE_ArmorItemData::PostLoad()
{
	Super::PostLoad();
	if (ItemDataVersion != 0) { return; }
	Template.ItemTag = ItemTag;
	Template.EquipmentSlotTag = EquipmentSlotTag;
	Template.ArmorStat = ArmorStat;
	Instance.ItemData = ArmorStat;
	ItemDataVersion = 1;
}
