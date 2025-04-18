#include "aoe_loot.h"
#include "ScriptMgr.h"
#include "World.h"
#include "LootMgr.h"
#include "ServerScript.h"
#include "WorldSession.h"
#include "WorldPacket.h" 
#include "Player.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "ChatCommandArgs.h"
#include "WorldObjectScript.h"
#include "Creature.h"
#include "Config.h"
#include "Log.h"
#include "Map.h"
#include <fmt/format.h>
#include "Corpse.h"

using namespace Acore::ChatCommands;
using namespace WorldPackets;

bool AoeLootServer::CanPacketReceive(WorldSession* session, WorldPacket& packet)
{
    if (packet.GetOpcode() == CMSG_LOOT)
    {
        Player* player = session->GetPlayer();
        if (player)
        {
            // Trigger AOE loot when a player attempts to loot a corpse
            ChatHandler handler(player->GetSession());
            handler.ParseCommands(".startaoeloot");
        }
    }
    return true;
}

ChatCommandTable AoeLootCommandScript::GetCommands() const
{
    static ChatCommandTable playerAoeLootCommandTable =
    {
        { "startaoeloot", HandleStartAoeLootCommand, SEC_PLAYER, Console::No }
    };
    return playerAoeLootCommandTable;
}

// Handle gold looting without distance checks
bool AoeLootCommandScript::ProcessLootMoney(Player* player, Creature* creature)
{
    if (!player || !creature)
        return false;
        
    Loot* loot = &creature->loot;
    if (!loot || loot->gold == 0)
        return false;
        
    uint32 goldAmount = loot->gold;
    bool shareMoney = true;  // Share by default for creature corpses
    
    if (shareMoney && player->GetGroup())
    {
        Group* group = player->GetGroup();
        std::vector<Player*> playersNear;
        
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (member)
                playersNear.push_back(member);
        }
        
        uint32 goldPerPlayer = uint32((loot->gold) / (playersNear.size()));
        
        for (Player* groupMember : playersNear)
        {
            groupMember->ModifyMoney(goldPerPlayer);
            groupMember->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, goldPerPlayer);
            
            WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
            data << uint32(goldPerPlayer);
            data << uint8(playersNear.size() > 1 ? 0 : 1);  // 0 is "Your share is..." and 1 is "You loot..."
            groupMember->GetSession()->SendPacket(&data);
        }
    }
    else
    {
        // No group - give all gold to the player
        player->ModifyMoney(loot->gold);
        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, loot->gold);
        
        WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
        data << uint32(loot->gold);
        data << uint8(1);   // "You loot..."
        player->GetSession()->SendPacket(&data);
    }
    
    // Mark the money as looted
    loot->gold = 0;
    loot->NotifyMoneyRemoved();
    
    return true;
}

bool AoeLootCommandScript::ProcessLootSlot(Player* player, ObjectGuid lguid, uint8 lootSlot)
{
    if (!player)
        return false;

    Loot* loot = nullptr;
    float aoeDistance = sConfigMgr->GetOption<float>("AOELoot.Range", 55.0f);

    // Get the loot object based on the GUID type
    if (lguid.IsGameObject())
    {
        GameObject* go = player->GetMap()->GetGameObject(lguid);
        
        if (!go || ((go->GetOwnerGUID() != player->GetGUID() && 
            go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) && 
            !go->IsWithinDistInMap(player, aoeDistance)))
        {
            return false;
        }
        
        loot = &go->loot;
    }
    else if (lguid.IsItem())
    {
        Item* pItem = player->GetItemByGuid(lguid);
        if (!pItem)
            return false;
        
        loot = &pItem->loot;
    }
    else if (lguid.IsCorpse())
    {
        Corpse* bones = ObjectAccessor::GetCorpse(*player, lguid);
        if (!bones)
            return false;
        
        loot = &bones->loot;
    }
    else
    {
        Creature* creature = player->GetMap()->GetCreature(lguid);
        
        if (creature && creature->IsAlive())
        {
            bool isPickpocketing = creature->IsAlive() && 
                                 (player->IsClass(CLASS_ROGUE, CLASS_CONTEXT_ABILITY) && 
                                  creature->loot.loot_type == LOOT_PICKPOCKETING);
                                  
            bool lootAllowed = creature->IsAlive() == isPickpocketing;
            
            if (!lootAllowed || !creature->IsWithinDistInMap(player, INTERACTION_DISTANCE))
                return false;
        }
        
        loot = &creature->loot;
    }
    
    if (!loot)
        return false;
    
    // Check if the item is already looted
    QuestItem* qitem = nullptr;
    QuestItem* ffaitem = nullptr;
    QuestItem* conditem = nullptr;
    LootItem* item = loot->LootItemInSlot(lootSlot, player, &qitem, &ffaitem, &conditem);
    
    if (!item)
    {
        if (sConfigMgr->GetOption<bool>("AOELoot.Debug", false))
            LOG_DEBUG("module.aoe_loot", "No valid loot item found in slot {}", lootSlot);
            
        return false;
    }
    
    // Store the loot item in the player's inventory
    InventoryResult msg;
    player->StoreLootItem(lootSlot, loot, msg);
    
    if (msg == EQUIP_ERR_OK)
    {
        // Successfully looted the item
        loot->items[lootSlot].is_looted = true;
        loot->unlootedCount--;
        
        if (sConfigMgr->GetOption<bool>("AOELoot.Debug", false))
            LOG_DEBUG("module.aoe_loot", "Successfully looted item (ID: {}) x{} from {}", item->itemid, static_cast<uint32_t>(item->count), lguid.ToString());
    }
    
    // Handle mail fallback for items
    if (msg != EQUIP_ERR_OK && lguid.IsItem() && loot->loot_type != LOOT_CORPSE)
    {
        item->is_looted = true;
        loot->NotifyItemRemoved(item->itemIndex);
        loot->unlootedCount--;
        
        player->SendItemRetrievalMail(item->itemid, item->count);
    }
    
    return true;
}

bool AoeLootCommandScript::HandleStartAoeLootCommand(ChatHandler* handler, Optional<std::string> /*args*/)
{
    if (!sConfigMgr->GetOption<bool>("AOELoot.Enable", true))
        return true;

    Player* player = handler->GetSession()->GetPlayer();
    if (!player)
        return true;

    float range = sConfigMgr->GetOption<float>("AOELoot.Range", 55.0);
    bool debugMode = sConfigMgr->GetOption<bool>("AOELoot.Debug", false);

    std::list<Creature*> nearbyCorpses;
    player->GetDeadCreatureListInGrid(nearbyCorpses, range);

    if (debugMode)
    {
        LOG_DEBUG("module.aoe_loot", "AOE Loot: Found {} nearby corpses within range {}.", nearbyCorpses.size(), range);
    }

    // Filter valid corpses
    std::list<Creature*> validCorpses;
    for (auto* creature : nearbyCorpses)
    {
        if (!player || !creature)
            continue;

        // Check if creature is valid for looting by this player
        if (!creature->hasLootRecipient() || !creature->isTappedBy(player))
            continue;

        // Skip if corpse is not lootable
        if (!creature->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE))
        {
            if (debugMode)
                LOG_DEBUG("module.aoe_loot", "AOE Loot: Skipping creature {} - not lootable", creature->GetGUID().ToString());
            continue;
        }

        // Additional permission check for instances
        if (player->GetMap()->Instanceable() && !player->isAllowedToLoot(creature))
        {
            if (debugMode)
                LOG_DEBUG("module.aoe_loot", "AOE Loot: Skipping creature {} - not your loot", creature->GetGUID().ToString());
            continue;
        }
        
        validCorpses.push_back(creature);
    }

    if (debugMode)
    {
        LOG_DEBUG("module.aoe_loot", "AOE Loot: Found {} valid nearby corpses within range {}.", validCorpses.size(), range);
    }

    // Process all valid corpses
    for (auto* creature : validCorpses)
    {
        Loot* loot = &creature->loot;
        if (!loot)
            continue;

        player->SetLootGUID(creature->GetGUID());

        // Process quest items
        if (!loot->quest_items.empty())
        {
            for (uint8 i = 0; i < loot->quest_items.size(); ++i)
            {
                uint8 lootSlot = loot->items.size() + i;
                ProcessLootSlot(player, creature->GetGUID(), lootSlot);
                
                if (debugMode)
                    LOG_DEBUG("module.aoe_loot", "AOE Loot: looted quest item in slot {}", lootSlot);
            }
        }
    
        // Process regular items
        for (uint8 lootSlot = 0; lootSlot < loot->items.size(); ++lootSlot)
        {
            player->SetLootGUID(creature->GetGUID());
            ProcessLootSlot(player, creature->GetGUID(), lootSlot);

            if (debugMode && lootSlot < loot->items.size())
            {
                LOG_DEBUG("module.aoe_loot", "AOE Loot: looted item from slot {} (ID: {}) from {}", lootSlot, loot->items[lootSlot].itemid, creature->GetGUID().ToString());
            }
        }

        // Handle money
        if (loot->gold > 0)
        {
            uint32 goldAmount = loot->gold;
            ProcessLootMoney(player, creature);
            
            if (debugMode)
            {
                LOG_DEBUG("module.aoe_loot", "AOE Loot: Looted {} copper from {}", goldAmount, creature->GetGUID().ToString());
            }
        }

        // Release loot if corpse is fully looted
        if (loot->isLooted())
        {
            WorldPacket releaseData(CMSG_LOOT_RELEASE, 8);
            releaseData << creature->GetGUID();
            player->GetSession()->HandleLootReleaseOpcode(releaseData);
        }
        player->SetLootGUID(ObjectGuid::Empty);
    }
    return true;
}

// Display login message to player
void AoeLootPlayer::OnPlayerLogin(Player* player)
{
    if (sConfigMgr->GetOption<bool>("AOELoot.Enable", true) && 
        sConfigMgr->GetOption<bool>("AOELoot.Message", true))
    {
        ChatHandler(player->GetSession()).PSendSysMessage(AOE_ACORE_STRING_MESSAGE);
    }
}

// Add script registrations
void AddSC_AoeLoot()
{
    new AoeLootPlayer();
    new AoeLootServer();
    new AoeLootCommandScript();
}
