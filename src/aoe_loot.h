#ifndef MODULE_AOELOOT_H
#define MODULE_AOELOOT_H

#include "ScriptMgr.h"
#include "Config.h"
#include "ServerScript.h"
#include "Chat.h"
#include "Player.h"
#include "Item.h"
#include "ScriptedGossip.h"
#include "ChatCommand.h"       
#include "ChatCommandArgs.h" 
#include "AccountMgr.h"
#include <vector> 
#include <list>
#include <map>
#include <ObjectGuid.h>

using namespace Acore::ChatCommands;


// AoeLootManager Class >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> //

class AoeLootManager : public ServerScript
{

public:
    AoeLootManager() : ServerScript("AoeLootManager") {}
    
    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override;
};

// AoeLootManager Class End. >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> //


// AoeLootPlayer Class >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> //

class AoeLootPlayer : public PlayerScript
{
public:
    AoeLootPlayer() : PlayerScript("AoeLootPlayer") {}

    void OnPlayerLogin(Player* player) override;
    void OnPlayerLogout(Player* player) override;
};

// AoeLootPlayer Class End. >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> //


// AoeLootCommandScript Class >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> //

class AoeLootCommandScript : public CommandScript
{
public:
    AoeLootCommandScript() : CommandScript("AoeLootCommandScript") {}
    ChatCommandTable GetCommands() const override;

    // Command handlers
    static bool HandleAoeLootOnCommand(ChatHandler* handler, Optional<std::string> args);
    static bool HandleAoeLootOffCommand(ChatHandler* handler, Optional<std::string> args);
    static bool HandleStartAoeLootCommand(ChatHandler* handler, Optional<std::string> args);
    static bool HandleAoeLootToggleCommand(ChatHandler* handler, Optional<std::string> args);
    static bool HandleAoeLootDebugOnCommand(ChatHandler* handler, Optional<std::string> args);
    static bool HandleAoeLootDebugOffCommand(ChatHandler* handler, Optional<std::string> args);
    static bool HandleAoeLootDebugToggleCommand(ChatHandler* handler, Optional<std::string> args);
    
    // Core loot processing functions
    static bool ProcessLootSlot(Player* player, ObjectGuid lguid, uint8 lootSlot);
    static bool ProcessLootMoney(Player* player, Creature* creature);
    static void ProcessLootRelease(ObjectGuid lguid, Player* player, Loot* loot);

    // Helper functions
    static void DebugMessage(Player* player, const std::string& message);
    static std::vector<Player*> GetGroupMembers(Player* player);
    static void ProcessQuestItems(Player* player, ObjectGuid lguid, Loot* loot);
    static std::pair<Loot*, bool> GetLootObject(Player* player, ObjectGuid lguid);
    static std::vector<Creature*> GetValidCorpses(Player* player, float range);
    static void ProcessCreatureLoot(Player* player, Creature* creature);
    static bool IsValidLootTarget(Player* player, Creature* creature);

    // Getters and setters for player AOE loot settings
    static bool GetPlayerAoeLootEnabled(uint64 guid);
    static bool GetPlayerAoeLootDebug(uint64 guid);
    static void SetPlayerAoeLootEnabled(uint64 guid, bool mode);
    static void SetPlayerAoeLootDebug(uint64 guid, bool mode);
    static void RemovePlayerLootEnabled(uint64 guid);
    static void RemovePlayerLootDebug(uint64 guid);
    static bool hasPlayerAoeLootEnabled(uint64 guid);
    static bool hasPlayerAoeLootDebug(uint64 guid);

private:
    static std::map<uint64, bool> playerAoeLootEnabled;
    static std::map<uint64, bool> playerAoeLootDebug;

};

// AoeLootCommandScript Class End. >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> //


void AddSC_AoeLoot();

#endif //MODULE_AOELOOT_H
