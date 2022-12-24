/*
  The "zombie" plugin is made by mrapple

  This plugin is distributted under the GNU LGPL v. 3 (see below or lgpl.txt)

  Please note that versions of the zombie plugin are periodically pushed out
  The version you see may not be the actual one running on the server

  Special thanks go to the following:

  trepan - individual bzdb vars, bonus points, misc help
  murielle - team switching
  ukeleleguy - original concept
  BulletCatcher - misc help
  Anxuiz - misc help
  http:patorjk.com - ascii art

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program.  If not, see
  <http://www.gnu.org/licenses/>.
*/

/*
  2.4.x Credits:
  allejo - bz_Toolkit for team switching and other misc useful features.
  Zehra - Updating of plug-in to 2.4.x 
*/

/*
@CHANGELOG:
* Updates:
- Ported main plug-in to 2.4.x
- Use bzToolKit for team switching
- Ported much of the slash command code to utility functions
- Made timer a utility function
- Fixed team kills giving bounties
- Free player record added.
- Reduced timer countdown by 7 seconds
- Updated utility function to better handle it.
- Attempt to fix earlier attempt at handling scores better.
- Teamkills/self-kills don't cause score drop anymore.
- Player sprees aren't reset by team kills or self kills.

*/


#include "bzfsAPI.h"
#include "bztoolkit/bzToolkitAPI.h"
#include "StateDatabase.h"
#include "../src/bzfs/bzfs.h"

bool isAdmin(int playerID)
{
  bz_BasePlayerRecord* player = bz_getPlayerByIndex(playerID);
  bool admin = (player->hasPerm("ban") || player->hasPerm("shortBan"));
  bz_freePlayerRecord(player);
  return admin;
}

int getPlayerByCallsign(bz_ApiString callsign)
{
  bz_APIIntList* players = bz_newIntList();
  bz_getPlayerIndexList(players);
  int id = -1;
  bool found = false;
  for(unsigned int i = 0; i < players->size(); i++)
  {
    bz_BasePlayerRecord* player = bz_getPlayerByIndex(players->get(i));
    if(player->callsign == callsign)
    {
      id = (int)i;
      found = true;
    }
    bz_freePlayerRecord(player);
    if(found)
      break;
  }
  bz_deleteIntList(players);
  return id;
}

void setVar(int playerID, const std::string& key, const std::string& value)
{
  void *bufStart = getDirectMessageBuffer();
  void *buf = nboPackUShort(bufStart, 1);
  buf = nboPackUByte (buf, key.length());
  buf = nboPackString(buf, key.c_str(), key.length());
  buf = nboPackUByte (buf, value.length());
  buf = nboPackString(buf, value.c_str(), value.length());
  const int len = (char*)buf - (char*)bufStart;
  directMessage(playerID, MsgSetVar, len, bufStart);
}

void setZombie(int playerID)
{
  setVar(playerID,"_tankSpeed","35");
  setVar(playerID,"_reloadTime","0.0000001");
  setVar(playerID,"_radarLimit","400");
  setVar(playerID,"_tankAngVel","0.785398");
  bz_givePlayerFlag(playerID, "SR", false);
  bztk_changeTeam(playerID, eGreenTeam);
}

void setHuman(int playerID)
{
  setVar(playerID,"_tankSpeed","25");
  setVar(playerID,"_reloadTime","3");
  setVar(playerID,"_radarLimit","100");
  setVar(playerID,"_tankAngVel","0.9");
  bz_removePlayerFlag(playerID);
  bztk_changeTeam(playerID, eRedTeam);
}

void setRogue(int playerID)
{
  setVar(playerID,"_tankSpeed","0.00000001");
  setVar(playerID,"_reloadTime","0.00000001");
  setVar(playerID,"_radarLimit","0");
  setVar(playerID,"_tankAngVel","0.00000001");
  bz_removePlayerFlag(playerID);
  bztk_changeTeam(playerID, eRogueTeam);
}

void setObserver(int playerID)
{
  setVar(playerID,"_tankSpeed","25");
  setVar(playerID,"_reloadTime","3.5");
  setVar(playerID,"_radarLimit","800");
  setVar(playerID,"_tankAngVel","0.785398");
  bz_removePlayerFlag(playerID);
  bztk_changeTeam(playerID, eObservers);
}

void dontMove(int playerID)
{
  setVar(playerID,"_tankSpeed","0.00000001");
  setVar(playerID,"_reloadTime","0.00000001");
  setVar(playerID,"_radarLimit","0");
  setVar(playerID,"_tankAngVel","0.00000001");
  bz_removePlayerFlag(playerID);
}

// Don't risk Segfaults...
std::string getCallsign(int player) {
  //Get callsign.
  bz_BasePlayerRecord *updateData;
  updateData  = bz_getPlayerByIndex(player);
  std::string callsign="(UNKNOWN)";
  if(updateData) {
    callsign = updateData->callsign.c_str();//get the callsign. Replaces UNKNOWN if they have a callsign
  }
  bz_freePlayerRecord(updateData);
  return callsign;
}

// Ported to functions (start)
/*void timerCountdown(int count) {
  std::string message;
  if ((count >= 1) && (count <= 12)) {
    bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "%d", count);
  } else {
    if ((count >= 0) && (count <= 14)) {
      if (count == 13) {
        message=std::string("13 seconds until game start");
      } else if (count == 14) {
        message=std::string("Last chance to join! If you are an observer, type /join to join now!");
      } else {
        message=std::string("Go!");
      }
      bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,message.c_str());
    }
  }
}*/

// Timer countdown works well, 
// we should maybe add a conditional to prevent some errors of startval being set to small numbers.
void timerCountdown(int count, int startVal) {
  std::string message;
  if ((count >= 1) && (count <= (startVal - 2))) {
    bz_sendTextMessagef(BZ_SERVER,BZ_ALLUSERS, "%d", count);
  } else {
    if ((count >= 0) && (count <= startVal)) {
      if (count == (startVal - 1)) {
        message=std::string(bz_format("%d seconds until game start", count));
      } else if (count == startVal) {
        message=std::string("Last chance to join! If you are an observer, type /join to join now!");
      } else {
        message=std::string("Go!");
      }
      bz_sendTextMessage(BZ_SERVER,BZ_ALLUSERS,message.c_str());
    }
  }
}


void scoreBounties(int kills, int killer) {
  // Is our killer on a spree?
  if ((kills == 3) || (kills == 5) || (kills == 10)) {
    std::string callsign = getCallsign(killer);
    if(kills == 3) {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s is on a rampage and was awarded 3 bonus points!", 
        callsign.c_str());//bz_incrementPlayerWins(killer, 3);
    } else if(kills == 5) {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s is on a killing spree and was awarded 5 bonus points!", 
        callsign.c_str());//bz_incrementPlayerWins(killer, 5);
    } else { //if(kills == 10) {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s is continuing the domination and was awarded 10 bonus points!",
        callsign.c_str());//bz_incrementPlayerWins(killer, 10);
    }
    bz_incrementPlayerWins(killer, kills);
  }
}

// probably should be renamed...
bool gameStart(int player, bool gameInSession, int countdown) {
    std::string callsign;
    callsign = getCallsign(player);
    bool startGame = false;
    if(gameInSession) {
      bz_sendTextMessagef(BZ_SERVER, player, 
        "%s, you can not start a countdown when there is already a game in progress", callsign.c_str());
      bz_debugMessagef(1, 
        "%s has attempted to start a countdown when a game is already in progress", callsign.c_str());
    } else {
      if(bz_getTeamCount(eRogueTeam) > 1) {
        if(countdown == 0) {
          startGame = true;
        } else {
          bz_sendTextMessagef(BZ_SERVER, player, 
            "%s, you can not start a countdown when a countdown has been started", callsign.c_str());
          bz_debugMessagef(1, 
            "%s has attempted to start a countdown when a countdown was already running", callsign.c_str());
        }
      } else {
        bz_sendTextMessagef(BZ_SERVER, player, 
          "%s, you can not start a countdown with less then two players", callsign.c_str());
        bz_debugMessagef(1, 
        "%s has attempted to start a countdown with less then two players", callsign.c_str());
      }
    }
    return startGame;
}

bool gameStop(int player, bool gameInSession, int countdown) {
    bool stopGame = false;
    if(isAdmin(player)) {
      if(gameInSession || countdown > 0) {
        stopGame = true;
      } else {
        bz_sendTextMessagef(BZ_SERVER, player, "%s, there is no game in progress", getCallsign(player).c_str());
      }
    }
    return stopGame;
}

// We do this to better handle scoring. (Players don't lose points due to server kills.)
//@TODO figure out why this does not work...
/*void killPlayer(int player) {
    if (bz_killPlayer(player, false, BZ_SERVER)) {
        if (bz_getPlayerLosses(player) >= 1) {
            bz_incrementPlayerLosses(player, -1);
        }
    }
}*/
void killPlayer(int player) {
  //Make sure that the player to be killed is not already dead.
  bz_BasePlayerRecord *playerData;
  playerData = bz_getPlayerByIndex(player);
  if(playerData){
    if(playerData->spawned){
      bz_killPlayer(player, false, BZ_SERVER);
      int loss = bz_getPlayerLosses(player);
      if (loss >= 1) {
        bz_incrementPlayerLosses(player, -1);
      }
    }
  }
  bz_freePlayerRecord(playerData);
}

void stopOrRestartKill(void) {
  // Kill all players and make them not move
  bz_APIIntList* playerlist=bz_newIntList();
  bz_getPlayerIndexList(playerlist);

  for ( unsigned int i=0; i < playerlist->size(); i++) {
    bz_BasePlayerRecord *updateData;
    updateData = bz_getPlayerByIndex(playerlist->get(i));

    if(updateData->team != eObservers) {
      dontMove(updateData->playerID);
      setRogue(updateData->playerID);
      //bz_killPlayer(updateData->playerID, false, -1);
      killPlayer(updateData->playerID);
    }

    bz_freePlayerRecord(updateData);
  }
  bz_deleteIntList(playerlist);
}


// ported functions (end)

class Zombie : public bz_Plugin, bz_CustomSlashCommandHandler
{
public:
  const char* Name(){return "Zombie";}
  void Init ( const char* commandLine );
  void Event(bz_EventData *eventData );
  void Cleanup ( void );
  bool SlashCommand ( int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params );
  bool gameInProgress = false;
  bool teamDecider = false;
  bool updateNow = false;
  bool dontUpdateNow = false;

  int timerStartValue = 8;
  int timer;
  int num;

  double lastsecond;
  double lastsec;

  std::map<int, bz_eTeamType> teamtracker;
  std::map<int, bz_eTeamType> hometeam;
  std::map<int, int> scoretracker;

  bz_eTeamType winningTeam = eNoTeam;
};

BZ_PLUGIN(Zombie)

void Zombie::Init (const char* commandLine) {
  bz_registerCustomSlashCommand ("start", this);
  bz_registerCustomSlashCommand ("stop", this);
  bz_registerCustomSlashCommand ("switch", this);
  bz_registerCustomSlashCommand ("restart", this);
  bz_registerCustomSlashCommand ("join", this);
  bz_registerCustomSlashCommand ("DEBUG", this);
  bz_registerCustomSlashCommand ("givepoints", this);
  
  Register(bz_ePlayerDieEvent);
  Register(bz_ePlayerSpawnEvent);
  Register(bz_ePlayerJoinEvent);
  Register(bz_eGetPlayerSpawnPosEvent);
  Register(bz_eFlagDroppedEvent);
  Register(bz_eTickEvent);
  Register(bz_eGetAutoTeamEvent);
  Register(bz_ePlayerPartEvent);

  bz_debugMessage(4,"zombie plugin loaded");
}

void Zombie::Cleanup (void) {
  bz_removeCustomSlashCommand ("start");
  bz_removeCustomSlashCommand ("stop");
  bz_removeCustomSlashCommand ("switch");
  bz_removeCustomSlashCommand ("restart");
  bz_removeCustomSlashCommand ("join");
  bz_removeCustomSlashCommand ("DEBUG");
  bz_removeCustomSlashCommand ("givepoints");

  Flush();

  bz_debugMessage(4,"zombie plugin unloaded");
}

bool Zombie::SlashCommand ( int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList* params ) {
// Admin permission level commands were hidden to non-admins.
  if(command == "start") {
    bool start = gameStart(playerID, gameInProgress, timer);
    // ^handles game start conditionals.
    if (start == true) {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has started a new game of zombie", getCallsign(playerID).c_str());
      timer = timerStartValue;//timer = 15;
      bz_debugMessagef(1, "%s has started a new game of zombie", getCallsign(playerID).c_str());
    }
    return true;
  }
  else if(command == "stop") {
    bool stopGame = gameStop(playerID, gameInProgress, timer);
    if (stopGame == true) {
      bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has ended the current game of zombie", getCallsign(playerID).c_str());
      bz_debugMessagef(1, "%s ended the current game of zombie", getCallsign(playerID).c_str());
        
      gameInProgress = false;
      timer = 0;
      stopOrRestartKill();
    }
    return true;
  }
  else if(command == "switch")
  {
    bz_APIStringList params;
    params.tokenize(message.c_str(), " ", 0, true);
    
    if(isAdmin(playerID))
    {
      if(params.size() >= 2)
      {
        int otherPlayerID = getPlayerByCallsign(params.get(0));
        if(otherPlayerID != -1)
        {
          bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
          if(params.get(1) == "red" || params.get(1) == "green" || params.get(1) == "rogue" || params.get(1) == "observer")
          {
            //bz_killPlayer(otherPlayerID, false, -1);
            killPlayer(otherPlayerID);

            if(params.get(1) == "red")
              teamtracker[otherPlayerID] = eRedTeam;

            if(params.get(1) == "green")
              teamtracker[otherPlayerID] = eGreenTeam;

            if(params.get(1) == "rogue")
              teamtracker[otherPlayerID] = eRogueTeam;

            if(params.get(1) == "observer")
              teamtracker[otherPlayerID] = eObservers;
            
            updateNow = true;
            bz_sendTextMessagef(BZ_SERVER, eAdministrators, "%s switched %s to the %s team", player->callsign.c_str(), params.get(0).c_str(), params.get(1).c_str());
            bz_freePlayerRecord(player);
          }
          else
          {
            bz_sendTextMessagef(BZ_SERVER, playerID, "Not a valid team: %s", params.get(1).c_str());
          }
        }
        else
        {
          bz_sendTextMessagef(BZ_SERVER, playerID, "No Such Player: %s", params.get(0).c_str());
        }
      }
      else
      {
        bz_sendTextMessage(BZ_SERVER, playerID, "Usage: /switch <player> <team>");
      }
    }
    else
    {
      bz_sendTextMessage(BZ_SERVER, playerID, "You do not have permission to switch teams");
    }
    
    return true;
  }
  else if(command == "restart")
  {
    // TODO: Add a restart command
    // The stop and start commands should be made into functions
    // so that they can easily be called here and above.
    // (No duplication of code)
  }
  else if(command == "join")
  {
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
    if(player->team == eObservers)
    {
      if(!gameInProgress)
      {
        teamtracker[playerID] = eRogueTeam;
        dontMove(playerID);
        setRogue(playerID);
        bz_sendTextMessagef(BZ_SERVER, playerID, "The game will start shortly. Good luck %s!", player->callsign.c_str());
      }
      else
      {
        bz_sendTextMessagef(BZ_SERVER, playerID, "%s, a game is in progress. Please wait until it finishes before joining.", player->callsign.c_str());
      }
    }
    else
    {
      bz_sendTextMessagef(BZ_SERVER, playerID, "%s, you must be an observer to use this command.", player->callsign.c_str());
    }
    bz_freePlayerRecord(player);
    return true;
  }
  else if(command == "DEBUG")
  {
    if(isAdmin(playerID))
    {
      // Server Statistics
      bz_sendTextMessage(BZ_SERVER, playerID, "--------( Server Stats )--------");
      bz_sendTextMessagef(BZ_SERVER, playerID, "Game running: %s",gameInProgress ? "Yes":"No");
      bz_sendTextMessagef(BZ_SERVER, playerID, "Countdown in progress: %s",timer>0 ? "Yes":"No");
      bz_sendTextMessagef(BZ_SERVER, playerID, "Countdown time: %d",timer);
      bz_sendTextMessagef(BZ_SERVER, playerID, "Countdown last second: %d",lastsecond);
      
      // Player Statistics
      bz_sendTextMessage(BZ_SERVER, playerID, "--------( Player Stats )--------");
      bz_sendTextMessagef(BZ_SERVER, playerID, "Rogue players: %d",bz_getTeamCount(eRogueTeam));
      bz_sendTextMessagef(BZ_SERVER, playerID, "Green players: %d",bz_getTeamCount(eGreenTeam));
      bz_sendTextMessagef(BZ_SERVER, playerID, "Red players: %d",bz_getTeamCount(eRedTeam));
      bz_sendTextMessagef(BZ_SERVER, playerID, "Observers: %d",bz_getTeamCount(eObservers));
      
      // Team Tracker Statistics
      bz_sendTextMessage(BZ_SERVER, playerID, "--------( TeamTracker Stats )--------");
      bz_APIIntList* playerlist=bz_newIntList();
      bz_getPlayerIndexList(playerlist);
      for ( unsigned int i=0; i < playerlist->size(); i++)
      {
        bz_BasePlayerRecord *updateData = bz_getPlayerByIndex(playerlist->get(i));

        switch(teamtracker[updateData->playerID])
        {
          default:
            break;
          case eGreenTeam:
            bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - green",updateData->playerID,updateData->callsign.c_str());
            break;
          case eRedTeam:
            bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - red",updateData->playerID,updateData->callsign.c_str());
            break;
          case eRogueTeam:
            bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - rogue",updateData->playerID,updateData->callsign.c_str());
            break;
          case eObservers:
            bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - observer",updateData->playerID,updateData->callsign.c_str());
            break;
        }
        bz_freePlayerRecord(updateData);
      }
      bz_deleteIntList(playerlist);
      
      // Score Tracker Statistics
      bz_sendTextMessage(BZ_SERVER, playerID, "--------( ScoreTracker Stats )--------");
      bz_APIIntList* scoretrack=bz_newIntList();
      bz_getPlayerIndexList(scoretrack);
      for ( unsigned int i=0; i < playerlist->size(); i++)
      {
        bz_BasePlayerRecord *updateData = bz_getPlayerByIndex(playerlist->get(i));
        bz_sendTextMessagef(BZ_SERVER, playerID, "%d - %s - %d",updateData->playerID,updateData->callsign.c_str(),scoretracker[updateData->playerID]);
        bz_freePlayerRecord(updateData);
      }
      bz_deleteIntList(scoretrack);
    return true;
    } else {
      return false;
    }
  }
  else if(command == "givepoints")
  {
    bz_BasePlayerRecord *player = bz_getPlayerByIndex(playerID);
    bz_APIStringList params;
    params.tokenize(message.c_str(), " ", 0, true);
    if(isAdmin(playerID))
    {
      if(params.size() == 2)
      {
        int otherPlayerID = getPlayerByCallsign(params.get(0));
        int pointsToGive = atoi(params.get(1).c_str());
        if(otherPlayerID != -1)
        {
          bz_incrementPlayerWins(otherPlayerID, pointsToGive);
          bz_sendTextMessagef(BZ_SERVER, playerID, "You gave %s %d point%s", params.get(0).c_str(), pointsToGive, num == 1 ? "":"s");
        }
        else
        {
          bz_sendTextMessagef(BZ_SERVER, playerID, "%s, \"%s\" is not a valid player", player->callsign.c_str(), params.get(0).c_str());
        }
      }
      else
      {    
        bz_sendTextMessage(BZ_SERVER, playerID, "Usage: /givepoints <player> <points>");
      }
    }
    bz_freePlayerRecord(player);
    return true;
  }
  return false;
}

void Zombie::Event(bz_EventData *eventData ){
  switch (eventData->eventType) {
    case bz_ePlayerJoinEvent: {
      bz_PlayerJoinPartEventData_V1* joindata = (bz_PlayerJoinPartEventData_V1*)eventData;
      int playerID = joindata->playerID;
      std::string callsign = joindata->record->callsign;
      bz_debugMessagef(1,"ZOMBIEDEBUG: %s joining from %f", callsign.c_str(), joindata->record->ipAddress.c_str());
      scoretracker[playerID] = 0;

      if(gameInProgress || joindata->record->team == eObservers) {
        teamtracker[playerID] = eObservers;
        hometeam[playerID] = eObservers;
        bz_debugMessagef(1,"ZOMBIEDEBUG: %s joined to the observer team", callsign.c_str());

        if (gameInProgress) {
          bz_sendTextMessage(BZ_SERVER, playerID, 
            "There is currently a game in progress. Wait until the game is done, then type /join");
        } else {
          bz_sendTextMessage(BZ_SERVER, playerID, "The game has not started. If you would like to join, type /join.");
        }
      } else {
        teamtracker[playerID] = eRogueTeam;
        hometeam[playerID] = eRogueTeam;
        bz_debugMessagef(1,"ZOMBIEDEBUG: %s joined to the rogue team", callsign.c_str());
        dontMove(playerID);
        bz_sendTextMessage(BZ_SERVER, playerID, 
          "To start the game, wait untill there are at least two players, then type /start");
      }
      // MAKE SURE WE DONT UPDATE ANYTHING!
      updateNow = false;
      dontUpdateNow = true;
    }break;

    case bz_ePlayerPartEvent: {
      bz_PlayerJoinPartEventData_V1* partData = (bz_PlayerJoinPartEventData_V1*)eventData;
      int player = partData->playerID;
      teamtracker[player] = eNoTeam;
      hometeam[player] = eNoTeam;
      scoretracker[player] = 0; 
    }break;

      case bz_ePlayerDieEvent: {
      bz_PlayerDieEventData_V2* diedata = (bz_PlayerDieEventData_V2*)eventData;
      // Someone who is actually playing
      if(diedata->killerID > 200 || diedata->killerID == BZ_SERVER)
        break;
      
      // For some reason, it seems like we got a lot of duplicate code:
      int playerID = diedata->playerID;
      int killerID = diedata->killerID;
      bz_eTeamType team = diedata->team;
      bz_eTeamType killerTeam = diedata->killerTeam;
      std::string player = getCallsign(playerID);
      std::string killer = getCallsign(killerID);
      
      //@TODO see if there is a better way to do this.
      // Note: Yes, we probably can merge it into a single statement or two.
      if(team == eRedTeam && killerTeam == eGreenTeam && gameInProgress)
      {
        bz_sendTextMessage(BZ_SERVER, playerID, "You are now a zombie -- Go kill those humans!");
        teamtracker[playerID] = eGreenTeam;
        bz_debugMessagef(1, "ZOMBIEDEBUG: Player: %s || Killer: %s || Player team: red || Killer team: green",
          player.c_str(), killer.c_str());
      }
      if(team == eGreenTeam && killerTeam == eRedTeam && gameInProgress)
      {
        bz_sendTextMessage(BZ_SERVER, playerID, "You are now a human -- Go kill those zombies!");
        teamtracker[playerID] = eRedTeam;
        bz_debugMessagef(1, "ZOMBIEDEBUG: Player: %s || Killer: %s || Player team: green || Killer team: red",
         player.c_str(), killer.c_str());
      }
      if(bz_getTeamCount(eGreenTeam) == 1 || bz_getTeamCount(eRedTeam) == 1)
      {
        if(bz_getTeamCount(eGreenTeam) == 1 && killerTeam == eRedTeam && team == eGreenTeam)
          winningTeam = eRedTeam;
        if(bz_getTeamCount(eRedTeam) == 1 && killerTeam == eGreenTeam && team == eRedTeam)
          winningTeam = eGreenTeam;
        // Lol wut?
        if(bz_getTeamCount(eRedTeam) == 1 && bz_getTeamCount(eGreenTeam) == 1)
          winningTeam = eNoTeam; 
      }
      // Seems it could be a function.
      if (killerTeam != team) { // Should prevent all bugs in this case.
        scoretracker[killerID]++;
        int killscore = scoretracker[killerID];
        // Handle bounty and bonus points, if any.
        scoreBounties(killscore, killerID);

        // If the person who was killed had points, give the killer a few bonus points.
        if(scoretracker[playerID] > 2) {
          bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s ended %s's reign and was awarded 5 bonus points!",
            killer.c_str(), player.c_str());
          bz_incrementPlayerWins(killerID, 5);
        }
        scoretracker[playerID] = 0;
      } 
      
      //@TODO also add additional handling to prevent negative scores.
      // Or should this be handled in other plug-ins which do score handling?
      if (killerTeam == team) {
          if (playerID != killerID) {
              bz_incrementPlayerTKs(killerID, -1);
              //bz_incrementPlayerLosses(killerID, -1);
              bz_incrementPlayerLosses(playerID, -1);
          } else {
              bz_incrementPlayerLosses(playerID, -1);
          }
      }
            
      /*if (killerTeam == team) {
        if (playerID != killerID) {
          printf("First pass\n");
          int kTK = bz_getPlayerTKs(killerID);
          int pTK = bz_getPlayerTKs(playerID);
          printf("Start: kTK: %d pTK: %d\n", kTK, pTK);  
          int kL  = bz_getPlayerLosses(killerID);
          int pL  = bz_getPlayerLosses(playerID);
          if (kTK > 0) {
            bz_incrementPlayerTKs(killerID, -1);
          }
          if (pTK > 0) {
            bz_incrementPlayerTKs(playerID, -1);
          }
          printf("End: kTK: %d pTK: %d\n", bz_getPlayerTKs(killerID), bz_getPlayerTKs(playerID));
          
          printf("Start: kL: %d pL: %d\n", kL, pL);   
          if (kL > 0) {
            bz_incrementPlayerLosses(killerID, -1);
          }
          if (pL > 0) {
            bz_incrementPlayerLosses(playerID, -1);
          }
          printf("End: kL: %d pL: %d\n", bz_getPlayerLosses(killerID), bz_getPlayerLosses(playerID));  
        } else {
          int pL  = bz_getPlayerLosses(playerID);
          if (pL > 0) {
            bz_incrementPlayerLosses(playerID, -1);
          }
          printf("pL %d losses modded: %d\n", pL, bz_getPlayerLosses(playerID));
        }
      
      }*/
      updateNow = true;
        
    }break;
     
    case bz_eTickEvent:
    {
      bz_TickEventData_V1* tickdata = (bz_TickEventData_V1*)eventData;
      //@TODO use timing mech from one of my other plug-ins for better timing.
      if(timer != 0)
      {
        if(tickdata->eventTime-1>lastsecond)
        {
          lastsecond = tickdata->eventTime;
          timer -= 1;
          timerCountdown(timer, timerStartValue);//timerCountdown(timer);
        }
        
        if(timer < 1)
        {
          bz_debugMessage(1,"ZOMBIEDEBUG: Timer hit zero. Starting team decision making.");
          timer = 0;
          bz_APIIntList* playerlist = bz_newIntList();
          bz_getPlayerIndexList(playerlist);
          if(teamDecider)
          {
            num = 0;
            teamDecider = false;
          }
          else
          {
            num = 1;
            teamDecider = true;
          }
          for ( unsigned int i=0; i < playerlist->size(); i++)
          {
            bz_BasePlayerRecord *updateData;
            updateData = bz_getPlayerByIndex(playerlist->get(i));
            if(updateData->team == eRogueTeam)
            {
              // Check if the number is odd or even
              if (num % 2 == 0)
              {
                teamtracker[updateData->playerID] = eGreenTeam;
                setZombie(updateData->playerID);
                hometeam[updateData->playerID] = eGreenTeam;
              }
              else
              {
                teamtracker[updateData->playerID] = eRedTeam;
                setHuman(updateData->playerID);
                hometeam[updateData->playerID] = eRedTeam;
              }
              bz_debugMessagef(1,"ZOMBIEDEBUG: %s confirmed as a player. Decided team: ",updateData->callsign.c_str(), num % 2 == 0 ? "green":"red");
              num++;
            }
            //bz_killPlayer(updateData->playerID, false, -1);
            killPlayer(updateData->playerID);
            bz_freePlayerRecord(updateData);
          }
          bz_deleteIntList(playerlist);
          winningTeam = eNoTeam;
          gameInProgress = true;
          bz_debugMessage(1,"ZOMBIEDEBUG: Loop done. Game in progress.");
        }
      }
      //^Note: This can be done better, we don't need to do it this way.
      if((bz_getTeamCount(eRedTeam) == 0 || bz_getTeamCount(eGreenTeam) == 0) && gameInProgress)
      {
        gameInProgress = false;
        bz_debugMessage(1,"ZOMBIEDEBUG: Team won, ending game...");
        bz_resetTeamScores();
        bz_APIIntList* playerlist = bz_newIntList();
        bz_getPlayerIndexList(playerlist);
        for ( unsigned int i=0; i < playerlist->size(); i++)
        {
          bz_BasePlayerRecord *updateData;
          updateData = bz_getPlayerByIndex(playerlist->get(i));
          
          if(updateData->team != eObservers)
          {
            dontMove(updateData->playerID);
            setRogue(updateData->playerID);
            teamtracker[updateData->playerID] = eRogueTeam;
            //bz_killPlayer(updateData->playerID, false, -1);
            killPlayer(updateData->playerID);
            bz_debugMessagef(1, "ZOMBIEDEBUG: %s was in the game. Killed and set as a rogue.", updateData->callsign.c_str());
          }
          
          bz_freePlayerRecord(updateData);
        }
        
        bz_deleteIntList(playerlist);
        
        // It might look wierd here because there are double "\"
        if(winningTeam == eGreenTeam)
        {
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "----------------------------------------------------------------------------------------------------------");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, " /$$$$$$$$                         /$$       /$$                           /$$      /$$ /$$            /$$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "|_____ $$                         | $$      |__/                          | $$  /$ | $$|__/           | $$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "     /$$/   /$$$$$$  /$$$$$$/$$$$ | $$$$$$$  /$$  /$$$$$$   /$$$$$$$      | $$ /$$$| $$ /$$ /$$$$$$$  | $$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "    /$$/   /$$__  $$| $$_  $$_  $$| $$__  $$| $$ /$$__  $$ /$$_____/      | $$/$$ $$ $$| $$| $$__  $$ | $$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "   /$$/   | $$  \\ $$| $$ \\ $$ \\ $$| $$  \\ $$| $$| $$$$$$$$|  $$$$$$       | $$$$_  $$$$| $$| $$  \\ $$ |__/");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "  /$$/    | $$  | $$| $$ | $$ | $$| $$  | $$| $$| $$_____/ \\____  $$      | $$$/ \\  $$$| $$| $$  | $$     ");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, " /$$$$$$$$|  $$$$$$/| $$ | $$ | $$| $$$$$$$/| $$|  $$$$$$$ /$$$$$$$/      | $$/   \\  $$| $$| $$  | $$  /$$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "|________/ \\______/ |__/ |__/ |__/|_______/ |__/ \\_______/|_______/       |__/     \\__/|__/|__/  |__/ |__/");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "----------------------------------------------------------------------------------------------------------");
          bz_debugMessage(1, "ZOMBIEDEBUG: Zombies won the game");
        }
        if (winningTeam == eRedTeam)
        {
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "------------------------------------------------------------------------------------------------------");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, " /$$   /$$                                                             /$$      /$$ /$$            /$$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$  | $$                                                            | $$  /$ | $$|__/           | $$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$  | $$ /$$   /$$ /$$$$$$/$$$$   /$$$$$$  /$$$$$$$   /$$$$$$$      | $$ /$$$| $$ /$$ /$$$$$$$  | $$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$$$$$$$| $$  | $$| $$_  $$_  $$ |____  $$| $$__  $$ /$$_____/      | $$/$$ $$ $$| $$| $$__  $$ | $$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$__  $$| $$  | $$| $$ \\ $$ \\ $$  /$$$$$$$| $$  \\ $$|  $$$$$$       | $$$$_  $$$$| $$| $$  \\ $$ |__/");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$  | $$| $$  | $$| $$ | $$ | $$ /$$__  $$| $$  | $$ \\____  $$      | $$$/ \\  $$$| $$| $$  | $$     ");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "| $$  | $$|  $$$$$$/| $$ | $$ | $$|  $$$$$$$| $$  | $$ /$$$$$$$/      | $$/   \\  $$| $$| $$  | $$  /$$");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "|__/  |__/ \\______/ |__/ |__/ |__/ \\_______/|__/  |__/|_______/       |__/     \\__/|__/|__/  |__/ |__/");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "------------------------------------------------------------------------------------------------------");                bz_debugMessage(1, "ZOMBIEDEBUG: Humans won the game");
        }
        if(bz_getTeamCount(eRogueTeam) > 1 || bz_getTeamCount(eGreenTeam) > 1 || bz_getTeamCount(eRedTeam) > 1)
        {
          bz_debugMessage(1, "ZOMBIEDEBUG: It seems we have enough players, starting another game.");
          bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "The server has automatically started a new game");
          timer = timerStartValue;//timer = 15;
        }
        winningTeam = eNoTeam;
      }
      
      // Make sure we're only checking every two seconds or when we need to update
      // Make sure a game is running
      if((tickdata->eventTime-2>lastsec || updateNow) && gameInProgress)
      {
        // If contradicted, then we're going to update next time
        if(dontUpdateNow)
        {
          dontUpdateNow = false;
        }
        else
        {
          lastsec = tickdata->eventTime;
          bz_APIIntList* playerlist=bz_newIntList();
          bz_getPlayerIndexList(playerlist);
          for ( unsigned int i=0; i < playerlist->size(); i++)
          {
            bz_BasePlayerRecord *updateData = bz_getPlayerByIndex(playerlist->get(i));
            
            if(updateData->team != teamtracker[updateData->playerID])
            {
              bz_debugMessagef(1, "ZOMBIEDEBUG: %s's team was not in sync with the team tracker! Updating now...", updateData->callsign.c_str());
              switch(teamtracker[updateData->playerID])
              {
                default:
                  break;
                case eGreenTeam:
                  setZombie(updateData->playerID);
                  break;
                case eRedTeam:
                  setHuman(updateData->playerID);
                  break;
                case eRogueTeam:
                  setRogue(updateData->playerID);
                  break;
                case eObservers:
                {
                  //bz_killPlayer(updateData->playerID, false, -1);
                  killPlayer(updateData->playerID);
                  setObserver(updateData->playerID);
                }
                  break;
              }
              bz_debugMessagef(1, "ZOMBIEDEBUG: Team updated for %s", updateData->callsign.c_str());
            }
            bz_freePlayerRecord(updateData);
          }
          bz_deleteIntList(playerlist);
          updateNow = false;
        }
      }
      
    }break;

    case bz_ePlayerSpawnEvent:{
      bz_PlayerSpawnEventData_V1* spawndata = (bz_PlayerSpawnEventData_V1*)eventData;
      if(spawndata->team == eGreenTeam && gameInProgress) {
        bz_givePlayerFlag(spawndata->playerID, "SR", true);
        bz_debugMessagef(1, "ZOMBIEDEBUG: %s spawned and was given a SR flag", getCallsign(spawndata->playerID).c_str());
      }
    }break;

    case bz_eFlagDroppedEvent: {
      bz_FlagDroppedEventData_V1* flagdropdata = (bz_FlagDroppedEventData_V1*)eventData;
      bz_givePlayerFlag(flagdropdata->playerID, flagdropdata->flagType, true);
    }break;

    case bz_eGetAutoTeamEvent:
    {
      bz_GetAutoTeamEventData_V1 *autoteamdata = (bz_GetAutoTeamEventData_V1*)eventData;
      int player = autoteamdata->playerID;
      std::string callsign = getCallsign(player);
      autoteamdata->handled = true;
      if(gameInProgress || autoteamdata->team == eObservers || timer == 1)
      {
        autoteamdata->team = eObservers;
        teamtracker[player] = eObservers;
        hometeam[player] = eObservers;
        bz_debugMessagef(1,"ZOMBIEDEBUG: %s was set as an observer via autoteam", callsign.c_str());
      }
      else
      {
        teamtracker[player] = eRogueTeam;
        hometeam[player] = eRogueTeam;
        autoteamdata->team = eRogueTeam;
        bz_debugMessagef(1,"ZOMBIEDEBUG: %s was set as a rogue via autoteam", callsign.c_str());
      }
      // MAKE SURE WE DONT UPDATE ANYTHING!
      updateNow = false;
      dontUpdateNow = true;
    }break;

    default:{ 
    }break;
  }
}

// Local Variables: ***
// mode:C++ ***
// tab-width: 8 ***
// c-basic-offset: 2 ***
// indent-tabs-mode: t ***
// End: ***
// ex: shiftwidth=2 tabstop=8
