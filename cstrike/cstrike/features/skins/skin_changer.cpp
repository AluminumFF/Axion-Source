#include <vector>
#include "skin_changer.hpp"
#include "../cstrike/sdk/interfaces/iengineclient.h"
#include "../../core/config.h"
#include "../../core/variables.h"
#include "../../sdk/datatypes/usercmd.h"
#include "../../core/sdk.h"
#include "../../sdk/entity.h"
#include "../../sdk/interfaces/iglobalvars.h"
#include "../../sdk/interfaces/cgameentitysystem.h"
#include "../cstrike/sdk/interfaces/iengineclient.h"
#include "../../sdk/datatypes/qangle.h"
#include "../../sdk/datatypes/vector.h"
#include "../cstrike/sdk/interfaces/inetworkclientservice.h"
#include "../cstrike/sdk/interfaces/ccsgoinput.h"
#include "../misc/movement.h"
#include "../cstrike/sdk/interfaces/ccsgoinput.h"
#include "../cstrike/sdk/interfaces/ienginecvar.h"
#include "../lagcomp/lagcomp.h"
#include "../cstrike/sdk/interfaces/events.h"
#include "../penetration/penetration.h"
#include "../cstrike/sdk/interfaces/itrace.h"
#include "../cstrike/core/spoofcall/syscall.h"
#include <iostream>
#include <memoryapi.h>
#include <mutex>
#include <array>
#include "../cstrike/sdk/interfaces/iengineclient.h"
#include "../../core/spoofcall/virtualization/VirtualizerSDK64.h"
#include "../../utilities/inputsystem.h"
#include "ccsinventorymanager.hpp"
#include "ccsplayerinventory.hpp"
#include "../cstrike/core/hooks.h"
static std::vector<uint64_t> g_vec_added_items_ids;

static int glove_frame = 0;
struct GloveInfo {
    int item_id;
    uint64_t item_high_id;
    uint64_t item_low_id;
    int item_def_id;
};
static GloveInfo added_gloves;

struct MaterialInfo {
    skin_changer::material_record* p_mat_records;
    uint32_t ui32_count;
};

void invalidate_glove_material(C_BaseViewModel* viewmodel) {
    MaterialInfo* p_mat_info = reinterpret_cast<MaterialInfo*>(reinterpret_cast<uint8_t*>(viewmodel) + 0xf80);

    for (uint32_t i = 0; i < p_mat_info->ui32_count; i++) {
        if (p_mat_info->p_mat_records[i].identifer == skin_changer::material_magic_number__gloves) {
            p_mat_info->p_mat_records[i].ui32_type_index = 0xffffffff;
            break;
        }
    }
}

void skin_changer::OnGlove(CCSPlayerInventory* p_inventory, C_CSPlayerPawn* p_local_pawn, C_BaseViewModel* p_view_model, C_EconItemView* gloves_item, CEconItemDefinition* gloves_definition) {

    if (!p_local_pawn) return;

    if (p_local_pawn->GetHealth() <= 0)
        return;

    if (!p_view_model)
        return;

    if (!gloves_item)
        return;

    if (!gloves_definition) return;

    if (added_gloves.item_id == 0)
        return;

    if (gloves_item->m_iItemID() != added_gloves.item_id || Vars.should_update) {
        glove_frame = 3;
        L_PRINT(LOG_INFO) << "applying glove skin";
        gloves_item->m_bDisallowSOC() = false;

        gloves_item->m_iItemID() = added_gloves.item_id;
        gloves_item->m_iItemIDHigh() = added_gloves.item_high_id;
        gloves_item->m_iItemIDLow() = added_gloves.item_low_id;
        gloves_item->m_iAccountID() = uint32_t(p_inventory->GetOwner().m_id);
        gloves_item->m_iItemDefinitionIndex() = added_gloves.item_def_id;

        p_view_model->GetGameSceneNode()->SetMeshGroupMask(1);

        Vars.should_update = false;
    }

    if (glove_frame > 0) {
        invalidate_glove_material(p_view_model);

        gloves_item->m_bInitialized() = true;
        p_local_pawn->m_bNeedToReApplyGloves() = true;

        glove_frame--;
    }
}

void skin_changer::OnFrameStageNotify(int frame_stage) {
    if (frame_stage != 6) return;

    CCSPlayerInventory* p_inventory = CCSPlayerInventory::GetInstance();
    if (!p_inventory) return;

    CGameEntitySystem* p_entity_system = I::GameResourceService->pGameEntitySystem;
    if (!p_entity_system) return;

    const uint64_t steam_id = p_inventory->GetOwner().m_id;

    CCSPlayerController* p_local_player_controller = CCSPlayerController::GetLocalPlayerController();
    if (!p_local_player_controller) return;

    C_CSPlayerPawn* p_local_pawn = I::GameResourceService->pGameEntitySystem->Get<C_CSPlayerPawn>(p_local_player_controller->GetPawnHandle());
    if (!p_local_pawn) return;

    if (p_local_pawn->GetHealth() <= 0)
        return;

    CCSPlayer_ViewModelServices* p_view_model_services = p_local_pawn->GetViewModelServices();
    if (!p_view_model_services) return;

    C_CSGOViewModel* p_view_model = I::GameResourceService->pGameEntitySystem->Get<C_CSGOViewModel>(p_view_model_services->m_hViewModel());
    if (!p_view_model)
        return;
    C_EconItemView* p_gloves = &p_local_pawn->m_EconGloves();
    if (p_gloves) {
        CEconItemDefinition* p_gloves_definition = p_gloves->GetStaticData();
        if (p_gloves_definition) {
            skin_changer::OnGlove(p_inventory, p_local_pawn, p_view_model, p_gloves, p_gloves_definition);
        }
    }

    int highest_index = p_entity_system->GetHighestEntityIndex();
    for (int i = 64 + 1; i <= highest_index; ++i) {
        C_BaseEntity* p_entity = p_entity_system->Get(i);
        if (!p_entity || !p_entity->IsWeapon()) continue;

        C_CSWeaponBase* p_weapon = reinterpret_cast<C_CSWeaponBase*>(p_entity);
        if (p_weapon->GetOriginalOwnerXuid() != steam_id) continue;

        CAttributeManager* p_attribute_container = &p_weapon->m_AttributeManager();
        if (!p_attribute_container) continue;

        C_EconItemView* p_weapon_item_view = &p_attribute_container->m_Item();
        if (!p_weapon_item_view) continue;

        CEconItemDefinition* p_weapon_definition =
            p_weapon_item_view->GetStaticData();
        if (!p_weapon_definition) continue;

        CGameSceneNode* p_weapon_scene_node = p_weapon->GetGameSceneNode();
        if (!p_weapon_scene_node) continue;

        C_EconItemView* p_weapon_in_loadout_item_view = nullptr;

        if (p_weapon_definition->IsWeapon()) {
            for (int i = 0; i <= 56; ++i) {
                C_EconItemView* pItemView = p_inventory->GetItemInLoadout(
                    p_weapon->m_iOriginalTeamNumber(), i);
                if (!pItemView) continue;

                if (pItemView->m_iItemDefinitionIndex() ==
                    p_weapon_definition->m_nDefIndex) {
                    p_weapon_in_loadout_item_view = pItemView;
                    break;
                }
            }
        }
        else {
            p_weapon_in_loadout_item_view = p_inventory->GetItemInLoadout(
                p_weapon->m_iOriginalTeamNumber(),
                p_weapon_definition->GetLoadoutSlot());
        }

        if (!p_weapon_in_loadout_item_view)
            continue;

        auto it = std::find(g_vec_added_items_ids.cbegin(), g_vec_added_items_ids.cend(), p_weapon_in_loadout_item_view->m_iItemID());
        if (it == g_vec_added_items_ids.cend()) continue;

        CEconItemDefinition* p_weapon_in_loadout_definition = p_weapon_in_loadout_item_view->GetStaticData();
        if (!p_weapon_in_loadout_definition) continue;

        const bool is_knife = p_weapon_definition->IsKnife(false, p_weapon_in_loadout_definition->m_pszItemBaseName);

        p_weapon_item_view->m_iItemID() = p_weapon_in_loadout_item_view->m_iItemID();
        p_weapon_item_view->m_iItemIDHigh() = p_weapon_in_loadout_item_view->m_iItemIDHigh();
        p_weapon_item_view->m_iItemIDLow() = p_weapon_in_loadout_item_view->m_iItemIDLow();
        p_weapon_item_view->m_iAccountID() = uint32_t(p_inventory->GetOwner().m_id);

        p_weapon_item_view->m_bDisallowSOC() = p_weapon_in_loadout_item_view->m_bDisallowSOC();

        CBaseHandle h_weapon = p_weapon->GetRefEHandle();
        if (is_knife) {
            const char* knife_model = p_weapon_in_loadout_definition->GetModelName();
            if (p_weapon_item_view->m_iItemDefinitionIndex() != p_weapon_in_loadout_definition->m_nDefIndex) {
                p_weapon_item_view->m_iItemDefinitionIndex() = p_weapon_in_loadout_definition->m_nDefIndex;
                p_weapon->SetModel(knife_model);
            }

            auto h_view_model_weapon = p_view_model->m_hWeapon();

            if (h_view_model_weapon.GetEntryIndex() == h_weapon.GetEntryIndex()) {
                p_view_model->SetModel(knife_model);
            }
            p_view_model->pAnimationGraphInstance->pAnimGraphNetworkedVariables = nullptr;
            if (Vars.full_update) {
                p_weapon->UpdateModel(1);
                p_weapon->UpdateComposite();
                p_weapon->UpdateCompositeSec();

                Vars.full_update = false;
            }
        }
        else {
            auto paint_kit =
                I::Client->GetEconItemSystem()
                ->GetEconItemSchema()
                ->GetPaintKits()
                .FindByKey(
                    p_weapon_in_loadout_item_view->GetCustomPaintKitIndex());

            const bool uses_old_model =
                paint_kit.has_value() && paint_kit.value()->UsesOldModel();

            p_weapon_scene_node->SetMeshGroupMask(1 + uses_old_model);
            if (p_view_model && p_view_model->m_hWeapon() == h_weapon) {
                CGameSceneNode* p_view_model_scene_node =
                    p_view_model->GetGameSceneNode();

                if (p_view_model_scene_node)
                    p_view_model_scene_node->SetMeshGroupMask(1 + uses_old_model);
            }

            if (Vars.full_update) {
                p_weapon->UpdateModel(1);
                p_weapon->UpdateComposite();
                p_weapon->UpdateCompositeSec();

                Vars.full_update = false;
            }
        }
    }
}

#include "../cstrike/sdk/interfaces/ccsgoinput.h"

void skin_changer::OnEquipItemInLoadout(int team, int slot, uint64_t item_id) {
    auto it =
        std::find(g_vec_added_items_ids.begin(), g_vec_added_items_ids.end(), item_id);
    if (it == g_vec_added_items_ids.end()) return;

    CCSInventoryManager* p_inventory_manager = CCSInventoryManager::GetInstance();
    if (!p_inventory_manager) return;

    CCSPlayerInventory* p_inventory = CCSPlayerInventory::GetInstance();
    if (!p_inventory) return;

    C_EconItemView* pItemViewToEquip = p_inventory->GetItemViewForItem(*it);
    if (!pItemViewToEquip) return;

    C_EconItemView* pItemInLoadout = p_inventory->GetItemInLoadout(team, slot);
    if (!pItemInLoadout) return;

    CEconItemDefinition* pItemInLoadoutStaticData = pItemInLoadout->GetStaticData();
    if (!pItemInLoadoutStaticData)
        return;

    L_PRINT(LOG_ERROR) << "pItemInLoadoutStaticData id" << pItemViewToEquip->m_iItemID();

    const uint64_t defaultItemID = (std::uint64_t(0xF) << 60) | pItemViewToEquip->m_iItemDefinitionIndex();
    p_inventory_manager->EquipItemInLoadout(team, slot, defaultItemID);
    CEconItem* pItemInLoadoutSOCData2 = nullptr;
    CEconItem* pItemInLoadoutSOCData = pItemInLoadout->GetSOCData(nullptr);
    if (!pItemInLoadoutSOCData) {
        p_inventory_manager->EquipItemInLoadout(team, slot, defaultItemID);
        pItemInLoadoutSOCData2 = pItemViewToEquip->GetSOCData(nullptr);
        if (pItemInLoadoutSOCData2) {
            pItemInLoadoutSOCData = pItemInLoadoutSOCData2;
        }
        else {
            return;
        }
    }

    CEconItemDefinition* toEquipData = pItemViewToEquip->GetStaticData();
    if (!toEquipData)
        return;

    if (toEquipData->IsWeapon() && !toEquipData->IsKnife(false, pItemInLoadoutStaticData->m_pszItemTypeName) && !toEquipData->IsGlove(false, pItemInLoadoutStaticData->m_pszItemTypeName)) {
        Vars.full_update = true;
        p_inventory->SOUpdated(p_inventory->GetOwner(), (CSharedObject*)pItemInLoadoutSOCData, eSOCacheEvent_Incremental);
        return;
    }
    else  if (toEquipData->IsGlove(false, pItemInLoadoutStaticData->m_pszItemTypeName)) {
        Vars.should_update = true;
        added_gloves.item_id = pItemViewToEquip->m_iItemID();
        added_gloves.item_high_id = pItemViewToEquip->m_iItemIDHigh();
        added_gloves.item_low_id = pItemViewToEquip->m_iItemIDLow();
        added_gloves.item_def_id = pItemViewToEquip->m_iItemDefinitionIndex();
        p_inventory->SOUpdated(p_inventory->GetOwner(), (CSharedObject*)pItemInLoadoutSOCData, eSOCacheEvent_Incremental);
        return;
    }
    else if (toEquipData->IsKnife(false, pItemInLoadoutStaticData->m_pszItemTypeName)) {
        Vars.full_update = true;
        p_inventory->SOUpdated(p_inventory->GetOwner(), (CSharedObject*)pItemInLoadoutSOCData, eSOCacheEvent_Incremental);
        return;
    }
}

void skin_changer::OnSetModel(C_BaseModelEntity* pEntity, const char*& model) {
    if (!I::Engine->IsConnected() || !I::Engine->IsInGame())
        return;

    CCSPlayerController* p_local_player_controller = CCSPlayerController::GetLocalPlayerController();
    if (!p_local_player_controller) return;

    C_CSPlayerPawn* p_local_pawn = I::GameResourceService->pGameEntitySystem->Get<C_CSPlayerPawn>(p_local_player_controller->GetPawnHandle());
    if (!p_local_pawn) return;

    if (p_local_pawn->GetHealth() <= 0)
        return;

    if (!pEntity || !pEntity->IsViewModel()) return;

    C_BaseViewModel* p_view_model = (C_BaseViewModel*)pEntity;
    if (!p_view_model)
        return;
    CCSPlayerInventory* p_inventory = CCSPlayerInventory::GetInstance();
    if (!p_inventory) return;

    const uint64_t steamID = p_inventory->GetOwner().m_id;

    C_CSWeaponBase* p_weapon = I::GameResourceService->pGameEntitySystem->Get<C_CSWeaponBase>(p_view_model->m_hWeapon());
    if (!p_weapon) return;

    if (!p_weapon || !p_weapon->IsWeapon() ||
        p_weapon->GetOriginalOwnerXuid() != steamID)
        return;

    CAttributeManager* p_attribute_container = &p_weapon->m_AttributeManager();
    if (!p_attribute_container) return;

    C_EconItemView* p_weapon_item_view = &p_attribute_container->m_Item();
    if (!p_weapon_item_view) return;

    CEconItemDefinition* p_weapon_definition = p_weapon_item_view->GetStaticData();
    if (!p_weapon_definition) return;

    C_EconItemView* p_weapon_in_loadout_item_view = p_inventory->GetItemInLoadout(
        p_weapon->m_iOriginalTeamNumber(), p_weapon_definition->GetLoadoutSlot());
    if (!p_weapon_in_loadout_item_view) return;

    auto it = std::find(g_vec_added_items_ids.cbegin(), g_vec_added_items_ids.cend(),
        p_weapon_in_loadout_item_view->m_iItemID());
    if (it == g_vec_added_items_ids.cend()) return;

    CEconItemDefinition* p_weapon_in_loadout_definition =
        p_weapon_in_loadout_item_view->GetStaticData();

    if (!p_weapon_in_loadout_definition) return;

    model = p_weapon_in_loadout_definition->GetModelName();
}

void skin_changer::AddEconItemToList(CEconItem* pItem) {
    g_vec_added_items_ids.emplace_back(pItem->m_ulID);
}

void skin_changer::Shutdown() {
    CCSPlayerInventory* p_inventory = CCSPlayerInventory::GetInstance();
    if (!p_inventory) return;

    for (uint64_t id : g_vec_added_items_ids) {
        p_inventory->RemoveEconItem(p_inventory->GetSOCDataForItem(id));
    }
}

void skin_changer::OnRoundReset(IGameEvent* pEvent) {
    if (!pEvent || g_vec_added_items_ids.empty()) return;

    const char* event_name = pEvent->GetName();
    if (!event_name) return;

    CCSPlayerController* p_local_player_controller = CCSPlayerController::GetLocalPlayerController();

    if (!p_local_player_controller)
        return;

    C_CSPlayerPawn* p_local_pawn = I::GameResourceService->pGameEntitySystem->Get<C_CSPlayerPawn>(p_local_player_controller->GetPawnHandle());
    if (!p_local_pawn) return;

}

void skin_changer::OnPreFireEvent(IGameEvent* pEvent) {
    if (!pEvent) return;

    const char* event_name = pEvent->GetName();
    if (!event_name) return;

    const auto pControllerWhoKilled = pEvent->get_player_controller("attacker");
    if (pControllerWhoKilled == nullptr)
        return;

    const auto pControllerWhoDied = pEvent->get_player_controller("userid");
    if (pControllerWhoDied == nullptr)
        return;

    if (pControllerWhoKilled->GetIdentity()->GetIndex() == pControllerWhoDied->GetIdentity()->GetIndex())
        return;

    CCSPlayerController* p_local_player_controller = CCSPlayerController::GetLocalPlayerController();

    if (!p_local_player_controller || pControllerWhoKilled->GetIdentity()->GetIndex() != p_local_player_controller->GetIdentity()->GetIndex())
        return;

    C_CSPlayerPawn* p_local_pawn = I::GameResourceService->pGameEntitySystem->Get<C_CSPlayerPawn>(p_local_player_controller->GetPawnHandle());
    if (!p_local_pawn) return;

    CPlayer_WeaponServices* p_weapon_services = p_local_pawn->GetWeaponServices();
    if (!p_weapon_services) return;

    C_CSWeaponBase* p_active_weapon = p_local_pawn->ActiveWeapon();
    if (!p_active_weapon) return;

    CAttributeManager* p_attribute_container = &p_active_weapon->m_AttributeManager();
    if (!p_attribute_container) return;


    C_EconItemView* p_weapon_item_view = &p_attribute_container->m_Item();
    if (!p_weapon_item_view) return;

    CEconItemDefinition* p_weapon_definition = p_weapon_item_view->GetStaticData();
    if (!p_weapon_definition || !p_weapon_definition->IsKnife(true, p_weapon_definition->m_pszItemTypeName)) return;
    const std::string_view token_name = CS_XOR("weapon");
    CUtlStringToken token(token_name.data());

    pEvent->SetString(token, p_weapon_definition->GetSimpleWeaponName());
}
