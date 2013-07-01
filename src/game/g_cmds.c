/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2006 Tim Angus

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "g_local.h"

/*
==================
G_SanitiseString

Remove case and control characters from a player name
==================
*/
void G_SanitiseString( char *in, char *out, int len )
{
  qboolean skip = qtrue;
  int spaces = 0;

  while( *in && len > 0 )
  {
    // strip leading white space
    if( *in == ' ' )
    {
      if( skip )
      {
        in++;
        continue;
      }
      spaces++;
    }
    else
    {
      spaces = 0;
      skip = qfalse;
    }
    
    if( Q_IsColorString( in ) )
    {
      in += 2;    // skip color code
      continue;
    }

    if( *in < 32 )
    {
      in++;
      continue;
    }

    *out++ = tolower( *in++ );
    len--;
  }
  out -= spaces; 
  *out = 0;
}

/*
==================
G_ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int G_ClientNumberFromString( gentity_t *to, char *s )
{
  gclient_t *cl;
  int       idnum;
  char      s2[ MAX_STRING_CHARS ];
  char      n2[ MAX_STRING_CHARS ];

  // numeric values are just slot numbers
  if( s[ 0 ] >= '0' && s[ 0 ] <= '9' )
  {
    idnum = atoi( s );

    if( idnum < 0 || idnum >= level.maxclients )
      return -1;

    cl = &level.clients[ idnum ];

    if( cl->pers.connected == CON_DISCONNECTED )
      return -1;

    return idnum;
  }

  // check for a name match
  G_SanitiseString( s, s2, sizeof( s2 ) );

  for( idnum = 0, cl = level.clients; idnum < level.maxclients; idnum++, cl++ )
  {
    if( cl->pers.connected == CON_DISCONNECTED )
      continue;

    G_SanitiseString( cl->pers.netname, n2, sizeof( n2 ) );

    if( !strcmp( n2, s2 ) )
      return idnum;
  }

  return -1;
}


/*
==================
G_MatchOnePlayer

This is a companion function to G_ClientNumbersFromString()

returns qtrue if the int array plist only has one client id, false otherwise
In the case of false, err will be populated with an error message.
==================
*/
qboolean G_MatchOnePlayer( int *plist, char *err, int len )
{
  gclient_t *cl;
  int *p;
  char line[ MAX_NAME_LENGTH + 10 ] = {""};

  err[ 0 ] = '\0';
  if( plist[ 0 ] == -1 )
  {
    Q_strcat( err, len, "no connected player by that name or slot #" );
    return qfalse;
  }
  if( plist[ 1 ] != -1 )
  {
    Q_strcat( err, len, "more than one player name matches. "
            "be more specific or use the slot #:\n" );
    for( p = plist; *p != -1; p++ )
    {
      cl = &level.clients[ *p ];
      if( cl->pers.connected == CON_CONNECTED )
      {
        Com_sprintf( line, sizeof( line ), "%2i - %s^7\n",
          *p, cl->pers.netname );
        if( strlen( err ) + strlen( line ) > len )
          break;
        Q_strcat( err, len, line );
      }
    }
    return qfalse;
  }
  return qtrue;
}

/*
==================
G_ClientNumbersFromString

Sets plist to an array of integers that represent client numbers that have
names that are a partial match for s.

Returns number of matching clientids up to MAX_CLIENTS.
==================
*/
int G_ClientNumbersFromString( char *s, int *plist)
{
  gclient_t *p;
  int i, found = 0;
  char n2[ MAX_NAME_LENGTH ] = {""};
  char s2[ MAX_NAME_LENGTH ] = {""};
  int max = MAX_CLIENTS;

  // if a number is provided, it might be a slot #
  for( i = 0; s[ i ] && isdigit( s[ i ] ); i++ );
  if( !s[ i ] )
  {
    i = atoi( s );
    if( i >= 0 && i < level.maxclients )
    {
      p = &level.clients[ i ];
      if( p->pers.connected != CON_DISCONNECTED )
      {
        *plist = i;
        return 1;
      }
    }
    // we must assume that if only a number is provided, it is a clientNum
    *plist = -1;
    return 0;
  }

  // now look for name matches
  G_SanitiseString( s, s2, sizeof( s2 ) );
  if( strlen( s2 ) < 1 )
    return 0;
  for( i = 0; i < level.maxclients && found <= max; i++ )
  {
    p = &level.clients[ i ];
    if( p->pers.connected == CON_DISCONNECTED )
    {
      continue;
    }
    G_SanitiseString( p->pers.netname, n2, sizeof( n2 ) );
    if( strstr( n2, s2 ) )
    {
      *plist++ = i;
      found++;
    }
  }
  *plist = -1;
  return found;
}

/*
==================
ScoreboardMessage

==================
*/
void ScoreboardMessage( gentity_t *ent )
{
  char      entry[ 1024 ];
  char      string[ 1400 ];
  int       stringlength;
  int       i, j;
  gclient_t *cl;
  int       numSorted;
  weapon_t  weapon = WP_NONE;
  upgrade_t upgrade = UP_NONE;

  // send the latest information on all clients
  string[ 0 ] = 0;
  stringlength = 0;

  numSorted = level.numConnectedClients;

  for( i = 0; i < numSorted; i++ )
  {
    int   ping;

    cl = &level.clients[ level.sortedClients[ i ] ];

    if( cl->pers.connected == CON_CONNECTING )
      ping = -1;
    else if( cl->sess.spectatorState == SPECTATOR_FOLLOW )
      ping = cl->pers.ping < 999 ? cl->pers.ping : 999;
    else
      ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

    //If (loop) client is a spectator, they have nothing, so indicate such. 
    //Only send the client requesting the scoreboard the weapon/upgrades information for members of their team. If they are not on a team, send it all.
    if( cl->sess.sessionTeam != TEAM_SPECTATOR && 
      (ent->client->pers.teamSelection == PTE_NONE || cl->pers.teamSelection == ent->client->pers.teamSelection ) )
    {
      weapon = cl->ps.weapon;

      if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, cl->ps.stats ) )
        upgrade = UP_BATTLESUIT;
      else if( BG_InventoryContainsUpgrade( UP_JETPACK, cl->ps.stats ) )
        upgrade = UP_JETPACK;
      else if( BG_InventoryContainsUpgrade( UP_BATTPACK, cl->ps.stats ) )
        upgrade = UP_BATTPACK;
      else if( BG_InventoryContainsUpgrade( UP_HELMET, cl->ps.stats ) )
        upgrade = UP_HELMET;
      else if( BG_InventoryContainsUpgrade( UP_LIGHTARMOUR, cl->ps.stats ) )
        upgrade = UP_LIGHTARMOUR;
      else
        upgrade = UP_NONE;
    }
    else
    {
      weapon = WP_NONE;
      upgrade = UP_NONE;
    }

    Com_sprintf( entry, sizeof( entry ),
      " %d %d %d %d %d %d", level.sortedClients[ i ], cl->pers.score, ping, 
      ( level.time - cl->pers.enterTime ) / 60000, weapon, upgrade );

    j = strlen( entry );

    if( stringlength + j > 1024 )
      break;

    strcpy( string + stringlength, entry );
    stringlength += j;
  }

  trap_SendServerCommand( ent-g_entities, va( "scores %i %i %i%s", i,
    level.alienKills, level.humanKills, string ) );
}


/*
==================
ConcatArgs
==================
*/
char *ConcatArgs( int start )
{
  int         i, c, tlen;
  static char line[ MAX_STRING_CHARS ];
  int         len;
  char        arg[ MAX_STRING_CHARS ];

  len = 0;
  c = trap_Argc( );

  for( i = start; i < c; i++ )
  {
    trap_Argv( i, arg, sizeof( arg ) );
    tlen = strlen( arg );

    if( len + tlen >= MAX_STRING_CHARS - 1 )
      break;

    memcpy( line + len, arg, tlen );
    len += tlen;

    if( len == MAX_STRING_CHARS - 1 )
      break;

    if( i != c - 1 )
    {
      line[ len ] = ' ';
      len++;
    }
  }

  line[ len ] = 0;

  return line;
}

/*
==================
G_Flood_Limited

Determine whether a user is flood limited, and adjust their flood demerits
==================
*/

qboolean G_Flood_Limited( gentity_t *ent )
{
  int millisSinceLastCommand;
  int maximumDemerits;

  // This shouldn't be called if g_floodMinTime isn't set, but handle it anyway.
  if( !g_floodMinTime.integer )
    return qfalse;
  
  if( level.paused ) //Doesn't work when game is paused, so disable
    return qfalse;
  
  // Do not limit admins with no censor/flood flag
  if( G_admin_permission( ent, ADMF_NOCENSORFLOOD ) )
   return qfalse;
  
  millisSinceLastCommand = level.time - ent->client->pers.lastFloodTime;
  if( millisSinceLastCommand < g_floodMinTime.integer )
    ent->client->pers.floodDemerits += ( g_floodMinTime.integer - millisSinceLastCommand );
  else
  {
    ent->client->pers.floodDemerits -= ( millisSinceLastCommand - g_floodMinTime.integer );
    if( ent->client->pers.floodDemerits < 0 )
      ent->client->pers.floodDemerits = 0;
  }

  ent->client->pers.lastFloodTime = level.time;

  // If g_floodMaxDemerits == 0, then we go against g_floodMinTime^2.
  
  if( !g_floodMaxDemerits.integer )
     maximumDemerits = g_floodMinTime.integer * g_floodMinTime.integer / 1000;
  else
     maximumDemerits = g_floodMaxDemerits.integer;

  if( ent->client->pers.floodDemerits > maximumDemerits )
     return qtrue;

  return qfalse;
}
  

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f( gentity_t *ent )
{
  char  *msg;

 if( !g_devmapNoGod.integer )
 {
  ent->flags ^= FL_GODMODE;

  if( !( ent->flags & FL_GODMODE ) )
    msg = "godmode OFF\n";
  else
    msg = "godmode ON\n";
 }
 else
 {
  msg = "Godmode has been disabled.\n";
 }

  trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent )
{
  char  *msg;

 if( !g_devmapNoGod.integer )
 {
  ent->flags ^= FL_NOTARGET;

  if( !( ent->flags & FL_NOTARGET ) )
    msg = "notarget OFF\n";
  else
    msg = "notarget ON\n";
 }
 else
 {
  msg = "Godmode has been disabled.\n";
 }

  trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent )
{
  char  *msg;

 if( !g_devmapNoGod.integer )
 {
  if( ent->client->noclip )
    msg = "noclip OFF\n";
  else
    msg = "noclip ON\n";

  ent->client->noclip = !ent->client->noclip;
 } 
 else
 {
  msg = "Godmode has been disabled.\n";
 }

  trap_SendServerCommand( ent - g_entities, va( "print \"%s\"", msg ) );
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent )
{
  BeginIntermission( );
  trap_SendServerCommand( ent - g_entities, "clientLevelShot" );
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent )
{
  if( ent->client->ps.stats[ STAT_STATE ] & SS_INFESTING )
    return;

  if( ent->client->ps.stats[ STAT_STATE ] & SS_HOVELING )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Leave the hovel first (use your destroy key)\n\"" );
    return;
  }

  if( g_cheats.integer )
  {
    ent->flags &= ~FL_GODMODE;
    ent->client->ps.stats[ STAT_HEALTH ] = ent->health = 0;
    player_die( ent, ent, ent, 100000, MOD_SUICIDE );
  }
  ent->suicideTime = level.time;
}

/*
==================
G_LeaveTeam
==================
*/
void G_LeaveTeam( gentity_t *self )
{
  pTeam_t   team = self->client->pers.teamSelection;
  gentity_t *ent;
  int       i;

  if( team == PTE_ALIENS )
    G_RemoveFromSpawnQueue( &level.alienSpawnQueue, self->client->ps.clientNum );
  else if( team == PTE_HUMANS )
    G_RemoveFromSpawnQueue( &level.humanSpawnQueue, self->client->ps.clientNum );
  else
  {
    if( self->client->sess.spectatorState == SPECTATOR_FOLLOW )
    {
      G_StopFollowing( self );
    }
    return;
  }
  
  // Cancel pending suicides
  self->suicideTime = 0;

  // stop any following clients
  G_StopFromFollowing( self );

  for( i = 0; i < level.num_entities; i++ )
  {
    ent = &g_entities[ i ];
    if( !ent->inuse )
      continue;

    // clean up projectiles
    if( ent->s.eType == ET_MISSILE && ent->r.ownerNum == self->s.number )
      G_FreeEntity( ent );
    if( ent->client && ent->client->pers.connected == CON_CONNECTED )
    {
      // cure poison
      if( ent->client->ps.stats[ STAT_STATE ] & SS_POISONCLOUDED &&
          ent->client->lastPoisonCloudedClient == self )
        ent->client->ps.stats[ STAT_STATE ] &= ~SS_POISONCLOUDED;
      if( ent->client->ps.stats[ STAT_STATE ] & SS_POISONED &&
          ent->client->lastPoisonClient == self )
        ent->client->ps.stats[ STAT_STATE ] &= ~SS_POISONED;
    }
  }
}

/*
=================
G_ChangeTeam
=================
*/
void G_ChangeTeam( gentity_t *ent, pTeam_t newTeam )
{
  pTeam_t oldTeam = ent->client->pers.teamSelection;
  qboolean isFixingImbalance=qfalse;
 
  if( oldTeam == newTeam )
    return;

  G_LeaveTeam( ent );
  ent->client->pers.teamSelection = newTeam;

  // G_LeaveTeam() calls G_StopFollowing() which sets spec mode to free. 
  // Undo that in this case, or else people can freespec while in the spawn queue on their new team
  if( newTeam != PTE_NONE )
  {
    ent->client->sess.spectatorState = SPECTATOR_LOCKED;
  }
  
  
  if ( ( level.numAlienClients - level.numHumanClients > 2 && oldTeam==PTE_ALIENS && newTeam == PTE_HUMANS && level.numHumanSpawns>0 ) ||
       ( level.numHumanClients - level.numAlienClients > 2 && oldTeam==PTE_HUMANS && newTeam == PTE_ALIENS  && level.numAlienSpawns>0 ) ) 
  {
    isFixingImbalance=qtrue;
  }

  // under certain circumstances, clients can keep their kills and credits
  // when switching teams
  if( G_admin_permission( ent, ADMF_TEAMCHANGEFREE ) ||
    ( g_teamImbalanceWarnings.integer && isFixingImbalance ) ||
    ( ( oldTeam == PTE_HUMANS || oldTeam == PTE_ALIENS )
    && ( level.time - ent->client->pers.teamChangeTime ) > 60000 ) )
  {
    if( oldTeam == PTE_ALIENS )
      ent->client->pers.credit *= (float)FREEKILL_HUMAN / FREEKILL_ALIEN;
    else if( newTeam == PTE_ALIENS )
      ent->client->pers.credit *= (float)FREEKILL_ALIEN / FREEKILL_HUMAN;
  }
  else
  {
    ent->client->pers.credit = 0;
    ent->client->pers.score = 0;
  }
  
  ent->client->ps.persistant[ PERS_KILLED ] = 0;
  ent->client->pers.statscounters.kills = 0;
  ent->client->pers.statscounters.structskilled = 0;
  ent->client->pers.statscounters.assists = 0;
  ent->client->pers.statscounters.repairspoisons = 0;
  ent->client->pers.statscounters.headshots = 0;
  ent->client->pers.statscounters.hits = 0;
  ent->client->pers.statscounters.hitslocational = 0;
  ent->client->pers.statscounters.deaths = 0;
  ent->client->pers.statscounters.feeds = 0;
  ent->client->pers.statscounters.suicides = 0;
  ent->client->pers.statscounters.teamkills = 0;
  ent->client->pers.statscounters.dmgdone = 0;
  ent->client->pers.statscounters.structdmgdone = 0;
  ent->client->pers.statscounters.ffdmgdone = 0;
  ent->client->pers.statscounters.structsbuilt = 0;
  ent->client->pers.statscounters.timealive = 0;
  ent->client->pers.statscounters.timeinbase = 0;
  ent->client->pers.statscounters.dretchbasytime = 0;
  ent->client->pers.statscounters.jetpackusewallwalkusetime = 0;

  // ROTAX
  ent->client->pers.statscounters.tremball_goalie = 0;

  ent->client->pers.classSelection = PCL_NONE;
  ClientSpawn( ent, NULL, NULL, NULL );

  ent->client->pers.joinedATeam = qtrue;
  ent->client->pers.teamChangeTime = level.time;

  //update ClientInfo
  ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );
  G_CheckDBProtection( );
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent )
{
  pTeam_t team;
  pTeam_t oldteam = ent->client->pers.teamSelection;
  char    s[ MAX_TOKEN_CHARS ];
  char buf[ MAX_STRING_CHARS ];
  qboolean force = G_admin_permission(ent, ADMF_FORCETEAMCHANGE);
  int     aliens = level.numAlienClients;
  int     humans = level.numHumanClients;

  // stop team join spam
  if( level.time - ent->client->pers.teamChangeTime < 1000 )
    return;

  if( oldteam == PTE_ALIENS )
    aliens--;
  else if( oldteam == PTE_HUMANS )
    humans--;

  // do warm up
  if( g_doWarmup.integer && g_warmupMode.integer == 1 &&
      level.time - level.startTime < g_warmup.integer * 1000 )
  {
    trap_SendServerCommand( ent - g_entities, va( "print \"team: you can't join"
      " a team during warm up (%d seconds remaining)\n\"",
      g_warmup.integer - ( level.time - level.startTime ) / 1000 ) );
    return;
  }

  trap_Argv( 1, s, sizeof( s ) );

  if( !strlen( s ) )
  {
    trap_SendServerCommand( ent-g_entities, va("print \"team: %i\n\"",
      oldteam ) );
    return;
  }

  if( Q_stricmpn( s, "spec", 4 ) ){
    if(G_admin_level(ent)<g_minLevelToJoinTeam.integer){
        trap_SendServerCommand( ent-g_entities,"print \"Sorry, but your admin level is only permitted to spectate.\n\"" ); 
        return;
    }
  }
  
  if( !Q_stricmpn( s, "spec", 4 ) )
  {
    team = PTE_NONE;
    ent->client->pers.statscounters.tremball_team = 0; // ROTAX
  }
  else if( !force && ent->client->pers.teamSelection == PTE_NONE &&
           g_maxGameClients.integer && level.numPlayingClients >=
           g_maxGameClients.integer )
  {
    trap_SendServerCommand( ent - g_entities, va( "print \"The maximum number "
      "of playing clients has been reached (g_maxGameClients = %i)\n\"",
      g_maxGameClients.integer ) );
    return;
  }
  else if( !Q_stricmpn( s, "alien", 5 ) )
  {
    if( g_forceAutoSelect.integer && !G_admin_permission(ent, ADMF_FORCETEAMCHANGE) )
    {
      trap_SendServerCommand( ent-g_entities, "print \"You can only join teams using autoselect\n\"" );
      return;
    }

    if( level.alienTeamLocked && !force )
    {
      trap_SendServerCommand( ent-g_entities,
        va( "print \"Alien team has been ^1LOCKED\n\"" ) );
      return; 
    }
    else if( level.humanTeamLocked )
    {
      // if only one team has been locked, let people join the other
      // regardless of balance
      force = qtrue;
    }

    if( !force && g_teamForceBalance.integer && aliens > humans )
    {
      G_TriggerMenu( ent - g_entities, MN_A_TEAMFULL );
      return;
    }
    

    team = PTE_ALIENS;
    ent->client->pers.statscounters.tremball_team = 1; // ROTAX
  }
  else if( !Q_stricmpn( s, "human", 5 ) )
  {
    if( g_forceAutoSelect.integer && !G_admin_permission(ent, ADMF_FORCETEAMCHANGE) )
    {
      trap_SendServerCommand( ent-g_entities, "print \"You can only join teams using autoselect\n\"" );
      return;
    }

    if( level.humanTeamLocked && !force )
    {
      trap_SendServerCommand( ent-g_entities,
        va( "print \"Human team has been ^1LOCKED\n\"" ) );
      return; 
    }
    else if( level.alienTeamLocked )
    {
      // if only one team has been locked, let people join the other
      // regardless of balance
      force = qtrue;
    }

    if( !force && g_teamForceBalance.integer && humans > aliens )
    {
      G_TriggerMenu( ent - g_entities, MN_H_TEAMFULL );
      return;
    }

    team = PTE_ALIENS;// ROTAX
    ent->client->pers.statscounters.tremball_team = 2; // ROTAX
  }
  else if( !Q_stricmp( s, "auto" ) )
  {
    if( level.humanTeamLocked && level.alienTeamLocked )
    {
      team = PTE_NONE;
      ent->client->pers.statscounters.tremball_team = 0; // ROTAX
    }
    else if( humans > aliens )
    {
      team = PTE_ALIENS;
      ent->client->pers.statscounters.tremball_team = 1; // ROTAX
    }
    else if( humans < aliens )
    {
      team = PTE_ALIENS;// ROTAX
      ent->client->pers.statscounters.tremball_team = 2; // ROTAX
    }
    else
    {
      team = PTE_ALIENS + ( rand( ) % 2 );
      ent->client->pers.statscounters.tremball_team = team + 1; // ROTAX
    }

    if( team == PTE_ALIENS && level.alienTeamLocked )
    {
      team = PTE_ALIENS;// ROTAX
      ent->client->pers.statscounters.tremball_team = 2; // ROTAX
    }
    else if( team == PTE_HUMANS && level.humanTeamLocked )
    {
      team = PTE_ALIENS;
      ent->client->pers.statscounters.tremball_team = 1; // ROTAX
    }
  }
  else
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"Unknown team: %s\n\"", s ) );
    return;
  }

  // stop team join spam
  if( oldteam == team )
    return;

   if (team != PTE_NONE)
   {
     char  namebuff[32];
 
     Q_strncpyz (namebuff, ent->client->pers.netname, sizeof(namebuff));
     Q_CleanStr (namebuff);
 
     if (!namebuff[0] || !Q_stricmp (namebuff, "UnnamedPlayer"))
     {
       trap_SendServerCommand( ent-g_entities, va( "print \"Please set your player name before joining a team. Press ESC and use the Options / Game menu  or use /name in the console\n\"") );
       return;
     }
   }
 

  G_ChangeTeam( ent, team );
   
   

   if( team == PTE_ALIENS ) {
     if ( oldteam == PTE_HUMANS )
       Com_sprintf( buf, sizeof( buf ), "%s^7 abandoned humans and joined the aliens.", ent->client->pers.netname );
     else
       Com_sprintf( buf, sizeof( buf ), "%s^7 joined the aliens.", ent->client->pers.netname );
   }
   else if( team == PTE_HUMANS ) {
     if ( oldteam == PTE_ALIENS )
       Com_sprintf( buf, sizeof( buf ), "%s^7 abandoned the aliens and joined the humans.", ent->client->pers.netname );
     else
       Com_sprintf( buf, sizeof( buf ), "%s^7 joined the humans.", ent->client->pers.netname );
   }
   else if( team == PTE_NONE ) {
     if ( oldteam == PTE_HUMANS )
       Com_sprintf( buf, sizeof( buf ), "%s^7 left the humans.", ent->client->pers.netname );
     else
       Com_sprintf( buf, sizeof( buf ), "%s^7 left the aliens.", ent->client->pers.netname );

      ent->client->pers.statscounters.tremball_team = 0; // ROTAX
   }
   trap_SendServerCommand( -1, va( "print \"%s\n\"", buf ) );
   G_LogOnlyPrintf("ClientTeam: %s\n",buf);
}


/*
==================
G_Say
==================
*/
static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, const char *prefix )
{
  qboolean ignore = qfalse;
  qboolean specAllChat = qfalse;

  if( !other )
    return;

  if( !other->inuse )
    return;

  if( !other->client )
    return;

  if( other->client->pers.connected != CON_CONNECTED )
    return;

  if( ( mode == SAY_TEAM || mode == SAY_ACTION_T ) && !OnSameTeam( ent, other ) )
  {
    if( other->client->pers.teamSelection != PTE_NONE )
      return;

    specAllChat = G_admin_permission( other, ADMF_SPEC_ALLCHAT );
    if( !specAllChat )
      return;

    // specs with ADMF_SPEC_ALLCHAT flag can see team chat
  }
  
  if( mode == SAY_ADMINS && !G_admin_permission( other, ADMF_ADMINCHAT) )
     return;

  if( BG_ClientListTest( &other->client->sess.ignoreList, ent-g_entities ) )
    ignore = qtrue;
  
  trap_SendServerCommand( other-g_entities, va( "%s \"%s%s%s%c%c%s\"",
    ( mode == SAY_TEAM || mode == SAY_ACTION_T ) ? "tchat" : "chat",
    ( ignore ) ? "[skipnotify]" : "",
    ( specAllChat ) ? prefix : "",
    name, Q_COLOR_ESCAPE, color, message ) );
}

#define EC    "\x19"

void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText )
{
  int         j;
  gentity_t   *other;
  int         color;
  const char  *prefix;
  char        name[ 64 ];
  // don't let text be too long for malicious reasons
  char        text[ MAX_SAY_TEXT ];
  char        location[ 64 ];

  // Bail if the text is blank.
  if( ! chatText[0] )
     return;

  // Flood limit.  If they're talking too fast, determine that and return.
  if( g_floodMinTime.integer )
    if ( G_Flood_Limited( ent ) )
    {
      trap_SendServerCommand( ent-g_entities, "print \"Your chat is flood-limited; wait before chatting again\n\"" );
      return;
    }
       
  if (g_chatTeamPrefix.integer && ent && ent->client )
  {
    switch( ent->client->pers.teamSelection)
    {
      default:
      case PTE_NONE:
        prefix = "[S] ";
        break;

      case PTE_ALIENS:
        prefix = "[A] ";
        break;

      case PTE_HUMANS:
        prefix = "[H] ";
    }
  }
  else
    prefix = "";

  switch( mode )
  {
    default:
    case SAY_ALL:
      G_LogPrintf( "say: %s^7: %s^7\n", ent->client->pers.netname, chatText );
      Com_sprintf( name, sizeof( name ), "%s%s%c%c"EC": ", prefix,
                   ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      color = COLOR_GREEN;
      break;

    case SAY_TEAM:
      G_LogPrintf( "sayteam: %s%s^7: %s^7\n", prefix, ent->client->pers.netname, chatText );
      if( Team_GetLocationMsg( ent, location, sizeof( location ) ) )
        Com_sprintf( name, sizeof( name ), EC"(%s%c%c"EC") (%s)"EC": ",
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
      else
        Com_sprintf( name, sizeof( name ), EC"(%s%c%c"EC")"EC": ",
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      
      if( ent->client->pers.teamSelection == PTE_NONE )
        color = COLOR_YELLOW;
      else
        color = COLOR_CYAN;
      break;

    case SAY_TELL:
      if( target && OnSameTeam( target, ent ) &&
          Team_GetLocationMsg( ent, location, sizeof( location ) ) )
        Com_sprintf( name, sizeof( name ), EC"[%s%c%c"EC"] (%s)"EC": ",
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
      else
        Com_sprintf( name, sizeof( name ), EC"[%s%c%c"EC"]"EC": ",
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      color = COLOR_MAGENTA;
      break;
      
    case SAY_ACTION:
      G_LogPrintf( "action: %s^7: %s^7\n", ent->client->pers.netname, chatText );
      Com_sprintf( name, sizeof( name ), "^2%s^7%s%s%c%c"EC" ", g_actionPrefix.string, prefix,
                   ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      color = COLOR_WHITE;
      break;

    case SAY_ACTION_T:
      G_LogPrintf( "actionteam: %s%s^7: %s^7\n", prefix, ent->client->pers.netname, chatText );
      if( Team_GetLocationMsg( ent, location, sizeof( location ) ) )
        Com_sprintf( name, sizeof( name ), EC"^5%s^7%s%c%c"EC"(%s)"EC" ", g_actionPrefix.string, 
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE, location );
      else
        Com_sprintf( name, sizeof( name ), EC"^5%s^7%s%c%c"EC""EC" ", g_actionPrefix.string, 
          ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
      color = COLOR_WHITE;
      break;
      
      case SAY_ADMINS:
        if( G_admin_permission( ent, ADMF_ADMINCHAT ) ) //Differentiate between inter-admin chatter and user-admin alerts
        {
         G_LogPrintf( "say_admins: [ADMIN]%s^7: %s^7\n", ( ent ) ? ent->client->pers.netname : "console", chatText );
         Com_sprintf( name, sizeof( name ), "%s[ADMIN]%s%c%c"EC": ", prefix,
                    ( ent ) ? ent->client->pers.netname : "console", Q_COLOR_ESCAPE, COLOR_WHITE );
         color = COLOR_MAGENTA;
        }
        else
        {
          G_LogPrintf( "say_admins: [PLAYER]%s^7: %s^7\n", ent->client->pers.netname, chatText );
          Com_sprintf( name, sizeof( name ), "%s[PLAYER]%s%c%c"EC": ", prefix,
            ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
          color = COLOR_MAGENTA;
        }
        break;
  }
  
  if( mode!=SAY_TEAM && ent && ent->client && ent->client->pers.teamSelection == PTE_NONE && G_admin_level(ent)<g_minLevelToSpecMM1.integer )
  {
    trap_SendServerCommand( ent-g_entities,va( "print \"Sorry, but your admin level may only use teamchat while spectating.\n\"") ); 
    return;
  }

  Com_sprintf( text, sizeof( text ), "%s^7", chatText );

  if( target )
  {
    G_SayTo( ent, target, mode, color, name, text, prefix );
    return;
  }
  


  // Ugly hax: if adminsayfilter is off, do the SAY first to prevent text from going out of order
  if( !g_adminSayFilter.integer )
  {
    // send it to all the apropriate clients
    for( j = 0; j < level.maxclients; j++ )
    {
      other = &g_entities[ j ];
      G_SayTo( ent, other, mode, color, name, text, prefix );
    }
  }
   
   if( g_adminParseSay.integer && ( mode== SAY_ALL || mode == SAY_TEAM ) )
   {
     if( G_admin_cmd_check ( ent, qtrue ) && g_adminSayFilter.integer ) 
     {
       return;
     }
   }

  // if it's on, do it here, where it won't happen if it was an admin command
  if( g_adminSayFilter.integer )
  {
    // send it to all the apropriate clients
    for( j = 0; j < level.maxclients; j++ )
    {
      other = &g_entities[ j ];
      G_SayTo( ent, other, mode, color, name, text, prefix );
    }
  }
  

}

static void Cmd_SayArea_f( gentity_t *ent )
{
  int    entityList[ MAX_GENTITIES ];
  int    num, i;
  int    color = COLOR_BLUE;
  const char  *prefix;
  vec3_t range = { HELMET_RANGE, HELMET_RANGE, HELMET_RANGE };
  vec3_t mins, maxs;
  char   *msg = ConcatArgs( 1 );
  char   name[ 64 ];
  
   if( g_floodMinTime.integer )
   if ( G_Flood_Limited( ent ) )
   {
    trap_SendServerCommand( ent-g_entities, "print \"Your chat is flood-limited; wait before chatting again\n\"" );
    return;
   }
  
  if (g_chatTeamPrefix.integer)
  {
    switch( ent->client->pers.teamSelection)
    {
      default:
      case PTE_NONE:
        prefix = "[S] ";
        break;

      case PTE_ALIENS:
        prefix = "[A] ";
        break;

      case PTE_HUMANS:
        prefix = "[H] ";
    }
  }
  else
    prefix = "";

  G_LogPrintf( "sayarea: %s%s^7: %s\n", prefix, ent->client->pers.netname, msg );
  Com_sprintf( name, sizeof( name ), EC"<%s%c%c"EC"> ",
    ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );

  VectorAdd( ent->s.origin, range, maxs );
  VectorSubtract( ent->s.origin, range, mins );

  num = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );
  for( i = 0; i < num; i++ )
    G_SayTo( ent, &g_entities[ entityList[ i ] ], SAY_TEAM, color, name, msg, prefix );
  
  //Send to ADMF_SPEC_ALLCHAT candidates
  for( i = 0; i < level.maxclients; i++ )
  {
    if( (&g_entities[ i ])->client->pers.teamSelection == PTE_NONE  &&
        G_admin_permission( &g_entities[ i ], ADMF_SPEC_ALLCHAT ) )
    {
      G_SayTo( ent, &g_entities[ i ], SAY_TEAM, color, name, msg, prefix );   
    }
  }
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent )
{
  char    *p;
  char    *args;
  int     mode = SAY_ALL;
  int     skipargs = 0;

  args = G_SayConcatArgs( 0 );
  if( Q_stricmpn( args, "say_team ", 9 ) == 0 )
    mode = SAY_TEAM;
  if( Q_stricmpn( args, "say_admins ", 11 ) == 0 || Q_stricmpn( args, "a ", 2 ) == 0)
    mode = SAY_ADMINS;

  // support parsing /m out of say text since some people have a hard
  // time figuring out what the console is.
  if( !Q_stricmpn( args, "say /m ", 7 ) ||
      !Q_stricmpn( args, "say_team /m ", 12 ) || 
      !Q_stricmpn( args, "say /mt ", 8 ) || 
      !Q_stricmpn( args, "say_team /mt ", 13 ) )
  {
    G_PrivateMessage( ent );
    return;
  }
  
   
   if( !Q_stricmpn( args, "say /a ", 7) ||
       !Q_stricmpn( args, "say_team /a ", 12) ||
       !Q_stricmpn( args, "say /say_admins ", 16) ||
       !Q_stricmpn( args, "say_team /say_admins ", 21) )
   {
       mode = SAY_ADMINS;
       skipargs=1;
   }
   
   if( mode == SAY_ADMINS)  
   if(!G_admin_permission( ent, ADMF_ADMINCHAT ) )
   {
     if( !g_publicSayadmins.integer )
     {
      ADMP( "Sorry, but public use of say_admins has been disabled.\n" );
      return;
     }
     else
     {
       ADMP( "Your message has been sent to any available admins and to the server logs.\n" );
     }
   }
   

  if(!Q_stricmpn( args, "say /me ", 8 ) )
  {
   if( g_actionPrefix.string[0] ) 
   { 
    mode = SAY_ACTION;
    skipargs=1;
   } else return;
  }
  else if(!Q_stricmpn( args, "say_team /me ", 13 ) )
  {
   if( g_actionPrefix.string[0] ) 
   { 
    mode = SAY_ACTION_T;
    skipargs=1;
   } else return;
  }
  else if( !Q_stricmpn( args, "me ", 3 ) )
  {
   if( g_actionPrefix.string[0] ) 
   { 
    mode = SAY_ACTION;
   } else return;
  }
  else if( !Q_stricmpn( args, "me_team ", 8 ) )
  {
   if( g_actionPrefix.string[0] ) 
   { 
    mode = SAY_ACTION_T;
   } else return;
  }

  if( trap_Argc( ) < 2 )
    return;

  p = G_SayConcatArgs( 1 + skipargs );

  G_Say( ent, NULL, mode, p );
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent )
{
  int     targetNum;
  gentity_t *target;
  char    *p;
  char    arg[MAX_TOKEN_CHARS];

  if( trap_Argc( ) < 2 )
    return;

  trap_Argv( 1, arg, sizeof( arg ) );
  targetNum = atoi( arg );

  if( targetNum < 0 || targetNum >= level.maxclients )
    return;

  target = &g_entities[ targetNum ];
  if( !target || !target->inuse || !target->client )
    return;

  p = ConcatArgs( 2 );

  G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p );
  G_Say( ent, target, SAY_TELL, p );
  // don't tell to the player self if it was already directed to this player
  // also don't send the chat back to a bot
  if( ent != target )
    G_Say( ent, ent, SAY_TELL, p );
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent )
{
  trap_SendServerCommand( ent-g_entities, va( "print \"%s\n\"", vtos( ent->s.origin ) ) );
}


static qboolean map_is_votable( const char *map )
{
  char maps[ MAX_CVAR_VALUE_STRING ];
  char *token, *token_p;

  if( !g_votableMaps.string[ 0 ] )
    return qtrue;

  Q_strncpyz( maps, g_votableMaps.string, sizeof( maps ) );
  token_p = maps;
  while( *( token = COM_Parse( &token_p ) ) )
  {
    if( !Q_stricmp( token, map ) )
      return qtrue;
  }

  return qfalse;
}

/*
==================
Cmd_CallVote_f
==================
*/
void Cmd_CallVote_f( gentity_t *ent )
{
  int   i;
  char  arg1[ MAX_STRING_TOKENS ];
  char  arg2[ MAX_STRING_TOKENS ];
  int   clientNum = -1;
  char  name[ MAX_NETNAME ];
  char *arg1plus;
  char *arg2plus;
  char nullstring[] = "";
  char  message[ MAX_STRING_CHARS ];
  char targetname[ MAX_NAME_LENGTH] = "";
  char reason[ MAX_STRING_CHARS ] = "";
  char *ptr = NULL;

  arg1plus = G_SayConcatArgs( 1 );
  arg2plus = G_SayConcatArgs( 2 );

  if( !g_allowVote.integer )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here\n\"" );
    return;
  }
  
  // Flood limit.  If they're talking too fast, determine that and return.
  if( g_floodMinTime.integer )
    if ( G_Flood_Limited( ent ) )
    {
      trap_SendServerCommand( ent-g_entities, "print \"Your /callvote attempt is flood-limited; wait before chatting again\n\"" );
      return;
    }

  if( g_voteMinTime.integer
    && ent->client->pers.firstConnect 
    && level.time - ent->client->pers.enterTime < g_voteMinTime.integer * 1000
    && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
    && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"You must wait %d seconds after connecting before calling a vote\n\"",
      g_voteMinTime.integer ) );
    return;
  }

  if( level.voteTime )
  {
    trap_SendServerCommand( ent-g_entities, "print \"A vote is already in progress\n\"" );
    return;
  }

  if( g_voteLimit.integer > 0
    && ent->client->pers.voteCount >= g_voteLimit.integer
    && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"You have already called the maximum number of votes (%d)\n\"",
      g_voteLimit.integer ) );
    return;
  }
  
  if( ent->client->pers.muted )
  {
    trap_SendServerCommand( ent - g_entities,
      "print \"You are muted and cannot call votes\n\"" );
    return;
  }

  // make sure it is a valid command to vote on
  trap_Argv( 1, arg1, sizeof( arg1 ) );
  trap_Argv( 2, arg2, sizeof( arg2 ) );

  if( strchr( arg1plus, ';' ) )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
    return;
  }

  // if there is still a vote to be executed
  if( level.voteExecuteTime )
  {
    if( !Q_stricmp( level.voteString, "map_restart" ) )
    {
      G_admin_maplog_result( "r" );
    }
    else if( !Q_stricmpn( level.voteString, "map", 3 ) )
    {
      G_admin_maplog_result( "m" );
    }

    level.voteExecuteTime = 0;
    trap_SendConsoleCommand( EXEC_APPEND, va( "%s\n", level.voteString ) );
  }
  
  level.votePassThreshold=50;
  
  ptr = strstr(arg1plus, " -");
  if( ptr )
  {
    *ptr = '\0';
    ptr+=2; 

    if( *ptr == 'r' || *ptr=='R' )
    {
      ptr++;
      while( *ptr == ' ' )
        ptr++;
      strcpy(reason, ptr);
    }
    else
    {
      trap_SendServerCommand( ent-g_entities, "print \"callvote: Warning: invalid argument specified \n\"" );
    }
  }

  // detect clientNum for partial name match votes
  if( !Q_stricmp( arg1, "kick" ) ||
    !Q_stricmp( arg1, "mute" ) ||
    !Q_stricmp( arg1, "unmute" ) )
  {
    int clientNums[ MAX_CLIENTS ] = { -1 };
    int numMatches=0;
    char err[ MAX_STRING_CHARS ] = "";
    
    Q_strncpyz(targetname, arg2plus, sizeof(targetname));
    ptr = strstr(targetname, " -");
    if( ptr )
      *ptr = '\0';
    
    if( g_requireVoteReasons.integer && !G_admin_permission( ent, ADMF_UNACCOUNTABLE ) && !Q_stricmp( arg1, "kick" ) && reason[ 0 ]=='\0' )
    {
       trap_SendServerCommand( ent-g_entities, "print \"callvote: You must specify a reason. Use /callvote kick [player] -r [reason] \n\"" );
       return;
    }
    
    if( !targetname[ 0 ] )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: no target\n\"" );
      return;
    }

    numMatches = G_ClientNumbersFromString( targetname, clientNums );
    if( numMatches == 1 )
    {
      // there was only one partial name match
      clientNum = clientNums[ 0 ]; 
    }
    else
    {
      // look for an exact name match (sets clientNum to -1 if it fails) 
      clientNum = G_ClientNumberFromString( ent, targetname );
    }
    
    if( clientNum==-1  && numMatches > 1 ) 
    {
      G_MatchOnePlayer( clientNums, err, sizeof( err ) );
      ADMP( va( "^3callvote: ^7%s\n", err ) );
      return;
    }
    
    if( clientNum != -1 &&
      level.clients[ clientNum ].pers.connected == CON_DISCONNECTED )
    {
      clientNum = -1;
    }

    if( clientNum != -1 )
    {
      Q_strncpyz( name, level.clients[ clientNum ].pers.netname,
        sizeof( name ) );
      Q_CleanStr( name );
      if ( G_admin_permission ( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
      {
    char reasonprint[ MAX_STRING_CHARS ] = "";

    if( reason[ 0 ] != '\0' )
      Com_sprintf(reasonprint, sizeof(reasonprint), "With reason: %s", reason);

        Com_sprintf( message, sizeof( message ), "%s^7 attempted /callvote %s %s on immune admin %s^7 %s^7",
          ent->client->pers.netname, arg1, targetname, g_entities[ clientNum ].client->pers.netname, reasonprint );
      }
    }
    else
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: invalid player\n\"" );
      return;
    }
  }
 
  if( !Q_stricmp( arg1, "kick" ) )
  {
    if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: admin is immune from vote kick\n\"" );
      G_AdminsPrintf("%s\n",message);
      return;
    }
    if( ( g_entities[clientNum].r.svFlags & SVF_BOT ) )
    {     
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: you can't kick bots\n\"" );
      return;
    }

    // use ip in case this player disconnects before the vote ends
    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "!ban %s \"%s\" vote kick", level.clients[ clientNum ].pers.ip,
      g_adminTempBan.string );
    if ( reason[0]!='\0' )
      Q_strcat( level.voteString, sizeof( level.voteDisplayString ), va( "(%s^7)", reason ) );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
      "Kick player \'%s\'", name );
  }
  else if( !Q_stricmp( arg1, "mute" ) )
  {
    if( level.clients[ clientNum ].pers.muted )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: player is already muted\n\"" );
      return;
    }

    if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: admin is immune from vote mute\n\"" );
      G_AdminsPrintf("%s\n",message);
      return;
    }
    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "!mute %i", clientNum );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
      "Mute player \'%s\'", name );
  }
  else if( !Q_stricmp( arg1, "unmute" ) )
  {
    if( !level.clients[ clientNum ].pers.muted )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: player is not currently muted\n\"" );
      return;
    }
    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "!unmute %i", clientNum );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
      "Un-Mute player \'%s\'", name );
  }
  else if( !Q_stricmp( arg1, "map_restart" ) )
  {
    if( g_mapvoteMaxTime.integer 
      && (( level.time - level.startTime ) >= g_mapvoteMaxTime.integer * 1000 )
      && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
      && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
    {
       trap_SendServerCommand( ent-g_entities, va(
         "print \"You cannot call for a restart after %d seconds\n\"",
         g_mapvoteMaxTime.integer ) );
       return;
    }
    Com_sprintf( level.voteString, sizeof( level.voteString ), "%s", arg1 );
    Com_sprintf( level.voteDisplayString,
        sizeof( level.voteDisplayString ), "Restart current map" );
    level.votePassThreshold = g_mapVotesPercent.integer;
  }
  else if( !Q_stricmp( arg1, "map" ) )
  {
    if( g_mapvoteMaxTime.integer 
      && (( level.time - level.startTime ) >= g_mapvoteMaxTime.integer * 1000 )
      && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
      && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
    {
       trap_SendServerCommand( ent-g_entities, va(
         "print \"You cannot call for a mapchange after %d seconds\n\"",
         g_mapvoteMaxTime.integer ) );
       return;
    }
  
    if( !G_MapExists( arg2 ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "'maps/%s.bsp' could not be found on the server\n\"", arg2 ) );
      return;
    }

    if( !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) && !map_is_votable( arg2 ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "Only admins may call a vote for map: %s\n\"", arg2 ) );
      return;
    }

    Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
    Com_sprintf( level.voteDisplayString,
        sizeof( level.voteDisplayString ), "Change to map '%s'", arg2 );
    level.votePassThreshold = g_mapVotesPercent.integer;
  }
  else if( !Q_stricmp( arg1, "nextmap" ) )
  {
    if( G_MapExists( g_nextMap.string ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "the next map is already set to '%s^7'\n\"", g_nextMap.string ) );
      return;
    }

    if( !arg2[ 0 ] )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: you must specify a map\n\"" );
      return;
    }

    if( !G_MapExists( arg2 ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "'maps/%s^7.bsp' could not be found on the server\n\"", arg2 ) );
      return;
    }

    if( !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) && !map_is_votable( arg2 ) )
    {
      trap_SendServerCommand( ent - g_entities, va( "print \"callvote: "
        "Only admins may call a vote for map: %s\n\"", arg2 ) );
      return;
    }

    Com_sprintf( level.voteString, sizeof( level.voteString ),
      "set g_nextMap %s", arg2 );
    Com_sprintf( level.voteDisplayString,
        sizeof( level.voteDisplayString ), "Set the next map to '%s^7'", arg2 );
    level.votePassThreshold = g_mapVotesPercent.integer;
  }
  else if( !Q_stricmp( arg1, "draw" ) )
  {
    Com_sprintf( level.voteString, sizeof( level.voteString ), "evacuation" );
    Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ),
        "End match in a draw" );
    level.votePassThreshold = g_mapVotesPercent.integer;
  }
   else if( !Q_stricmp( arg1, "poll" ) )
    {
      if( arg2plus[ 0 ] == '\0' )
      {
        trap_SendServerCommand( ent-g_entities, "print \"callvote: You forgot to specify what people should vote on.\n\"" );
        return;
      }
      Com_sprintf( level.voteString, sizeof( level.voteString ), nullstring);
      Com_sprintf( level.voteDisplayString,
          sizeof( level.voteDisplayString ), "[Poll] \'%s\'", arg2plus );
   }
  else
  {
    trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
    trap_SendServerCommand( ent-g_entities, "print \"Valid vote commands are: "
      "map, map_restart, draw, nextmap, kick, mute, unmute, poll\n" );
    return;
  }
  
  if( level.votePassThreshold!=50 )
  {
    Q_strcat( level.voteDisplayString, sizeof( level.voteDisplayString ), va( " (Needs > %d percent)", level.votePassThreshold ) );
  }
  
  if ( reason[0]!='\0' )
    Q_strcat( level.voteDisplayString, sizeof( level.voteDisplayString ), va( " Reason: '%s^7'", reason ) );
  

  trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE
         " called a vote: %s" S_COLOR_WHITE "\n\"", ent->client->pers.netname, level.voteDisplayString ) );
  
  G_LogPrintf("Vote: %s^7 called a vote: %s^7\n", ent->client->pers.netname, level.voteDisplayString );
  
  Q_strcat( level.voteDisplayString, sizeof( level.voteDisplayString ), va( " Called by: '%s^7'", ent->client->pers.netname ) );

  ent->client->pers.voteCount++;

  // start the voting, the caller autoamtically votes yes
  level.voteTime = level.time;
  level.voteNo = 0;

  for( i = 0 ; i < level.maxclients ; i++ )
    level.clients[i].ps.eFlags &= ~EF_VOTED;

  if( !Q_stricmp( arg1, "poll" ) )
  {
    level.voteYes = 0;
  }
  else
  {
   level.voteYes = 1;
   ent->client->ps.eFlags |= EF_VOTED;
  }

  trap_SetConfigstring( CS_VOTE_TIME, va( "%i", level.voteTime ) );
  trap_SetConfigstring( CS_VOTE_STRING, level.voteDisplayString );
  trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
  trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
}


/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent )
{
  char msg[ 64 ];

  if( !level.voteTime )
  { 
    if( ent->client->pers.teamSelection != PTE_NONE )
    {
      // If there is a teamvote going on but no global vote, forward this vote on as a teamvote
      // (ugly hack for 1.1 cgames + noobs who can't figure out how to use any command that isn't bound by default)
      int     cs_offset = 0;
      if( ent->client->pers.teamSelection == PTE_ALIENS )
        cs_offset = 1;
    
      if( level.teamVoteTime[ cs_offset ] )
      {
         if( !(ent->client->ps.eFlags & EF_TEAMVOTED ) )
        {
          Cmd_TeamVote_f(ent); 
          return;
        }
      }
    }
    trap_SendServerCommand( ent-g_entities, "print \"No vote in progress\n\"" );
    return;
  }

  if( ent->client->ps.eFlags & EF_VOTED )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Vote already cast\n\"" );
    return;
  }

  trap_SendServerCommand( ent-g_entities, "print \"Vote cast\n\"" );

  ent->client->ps.eFlags |= EF_VOTED;

  trap_Argv( 1, msg, sizeof( msg ) );

  if( msg[ 0 ] == 'y' || msg[ 1 ] == 'Y' || msg[ 1 ] == '1' )
  {
    level.voteYes++;
    trap_SetConfigstring( CS_VOTE_YES, va( "%i", level.voteYes ) );
  }
  else
  {
    level.voteNo++;
    trap_SetConfigstring( CS_VOTE_NO, va( "%i", level.voteNo ) );
  }

  // a majority will be determined in G_CheckVote, which will also account
  // for players entering or leaving
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent )
{
  int   i, team, cs_offset = 0;
  char  arg1[ MAX_STRING_TOKENS ];
  char  arg2[ MAX_STRING_TOKENS ];
  int   clientNum = -1;
  char  name[ MAX_NETNAME ];
  char nullstring[] = "";
  char  message[ MAX_STRING_CHARS ];
  char targetname[ MAX_NAME_LENGTH] = "";
  char reason[ MAX_STRING_CHARS ] = "";
  char *arg1plus;
  char *arg2plus;
  char *ptr = NULL;
  int numVoters = 0;

  arg1plus = G_SayConcatArgs( 1 );
  arg2plus = G_SayConcatArgs( 2 );
  
  team = ent->client->pers.teamSelection;

  if( team == PTE_ALIENS )
    cs_offset = 1;

  if(team==PTE_ALIENS)
    numVoters = level.numAlienClients;
  else if(team==PTE_HUMANS)
    numVoters = level.numHumanClients;

  if( !g_allowVote.integer )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Voting not allowed here\n\"" );
    return;
  }

  if( level.teamVoteTime[ cs_offset ] )
  {
    trap_SendServerCommand( ent-g_entities, "print \"A team vote is already in progress\n\"" );
    return;
  }

  if( g_voteLimit.integer > 0
    && ent->client->pers.voteCount >= g_voteLimit.integer 
    && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"You have already called the maximum number of votes (%d)\n\"",
      g_voteLimit.integer ) );
    return;
  }
  
  if( ent->client->pers.muted )
  {
    trap_SendServerCommand( ent - g_entities,
      "print \"You are muted and cannot call teamvotes\n\"" );
    return;
  }

  if( g_voteMinTime.integer
    && ent->client->pers.firstConnect 
    && level.time - ent->client->pers.enterTime < g_voteMinTime.integer * 1000
    && !G_admin_permission( ent, ADMF_NO_VOTE_LIMIT ) 
    && (level.numPlayingClients > 0 && level.numConnectedClients>1) )
  {
    trap_SendServerCommand( ent-g_entities, va(
      "print \"You must wait %d seconds after connecting before calling a vote\n\"",
      g_voteMinTime.integer ) );
    return;
  }

  // make sure it is a valid command to vote on
  trap_Argv( 1, arg1, sizeof( arg1 ) );
  trap_Argv( 2, arg2, sizeof( arg2 ) );

  if( strchr( arg1plus, ';' ) )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Invalid team vote string\n\"" );
    return;
  }
  
  ptr = strstr(arg1plus, " -");
  if( ptr )
  {
    *ptr = '\0';
    ptr+=2; 

    if( *ptr == 'r' || *ptr=='R' )
    {
      ptr++;
      while( *ptr == ' ' )
        ptr++;
      strcpy(reason, ptr);
    }
    else
    {
      trap_SendServerCommand( ent-g_entities, "print \"callteamvote: Warning: invalid argument specified \n\"" );
    }
  }
  
  // detect clientNum for partial name match votes // ROTAX
  if( !Q_stricmp( arg1, "kick" ) ||
    !Q_stricmp( arg1, "goalie" ) )
  {
    int clientNums[ MAX_CLIENTS ] = { -1 };
    int numMatches=0;
    char err[ MAX_STRING_CHARS ];
    
    Q_strncpyz(targetname, arg2plus, sizeof(targetname));
    ptr = strstr(targetname, " -");
    if( ptr )
      *ptr = '\0';
    
    if( g_requireVoteReasons.integer && !G_admin_permission( ent, ADMF_UNACCOUNTABLE ) && !Q_stricmp( arg1, "kick" ) && reason[ 0 ]=='\0' )
    {
       trap_SendServerCommand( ent-g_entities, "print \"callvote: You must specify a reason. Use /callteamvote kick [player] -r [reason] \n\"" );
       return;
    }
    

    if( !arg2[ 0 ] )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: no target\n\"" );
      return;
    }

    numMatches = G_ClientNumbersFromString( targetname, clientNums );
    if( numMatches == 1 )
    {
      // there was only one partial name match
      clientNum = clientNums[ 0 ]; 
    }
    else
    {
      // look for an exact name match (sets clientNum to -1 if it fails) 
      clientNum = G_ClientNumberFromString( ent, targetname );
    }
    
    if( clientNum==-1  && numMatches > 1 ) 
    {
      G_MatchOnePlayer( clientNums, err, sizeof( err ) );
      ADMP( va( "^3callteamvote: ^7%s\n", err ) );
      return;
    }

    // make sure this player is on the same team
    if( clientNum != -1 && level.clients[ clientNum ].pers.teamSelection !=
      team )
    {
      clientNum = -1;
    }
      
    if( clientNum != -1 &&
      level.clients[ clientNum ].pers.connected == CON_DISCONNECTED )
    {
      clientNum = -1;
    }

    if( clientNum != -1 )
    {
      Q_strncpyz( name, level.clients[ clientNum ].pers.netname,
        sizeof( name ) );
      Q_CleanStr( name );
      if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
      {
       char reasonprint[ MAX_STRING_CHARS ] = "";
       if( reason[ 0 ] != '\0' )
        Com_sprintf(reasonprint, sizeof(reasonprint), "With reason: %s", reason);

        Com_sprintf( message, sizeof( message ), "%s^7 attempted /callteamvote %s %s on immune admin %s^7 %s^7",
          ent->client->pers.netname, arg1, arg2, g_entities[ clientNum ].client->pers.netname, reasonprint );
      }
    }
    else
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: invalid player\n\"" );
      return;
    }
  }

  if( !Q_stricmp( arg1, "kick" ) )
  {
    if( G_admin_permission( &g_entities[ clientNum ], ADMF_IMMUNITY ) )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: admin is immune from vote kick\n\"" );
      G_AdminsPrintf("%s\n",message);
      return;
    }
    if( ( g_entities[clientNum].r.svFlags & SVF_BOT ) )
    {     
      trap_SendServerCommand( ent-g_entities,
        "print \"callvote: you can't kick bots\n\"" );
      return;
    }


    // use ip in case this player disconnects before the vote ends
    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ),
      "!ban %s \"%s\" team vote kick", level.clients[ clientNum ].pers.ip,
      g_adminTempBan.string );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "Kick player '%s'", name );
  }

  else if( !Q_stricmp( arg1, "goalie" ) ) // ROTAX
  {
    if( level.clients[ clientNum ].pers.statscounters.tremball_goalie == 1 )
    {
      trap_SendServerCommand( ent-g_entities,
        va("print \"callteamvote: Player %s is goalie already.\n\"", name) );
      return;
    }

    if( level.clients[ clientNum ].pers.teamSelection == PTE_NONE || level.clients[ clientNum ].ps.stats[ STAT_PCLASS ] == PCL_ALIEN_LEVEL0)
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: Goalie can be only player from red or blue team.\n\"" );
      return;
    }

    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ), "!goalie %i", clientNum );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "'%s' want be goalie", name );
  }
  else if( !Q_stricmp( arg1, "admitdefeat" ) )
  {
    if( numVoters <=1 )
    {
      trap_SendServerCommand( ent-g_entities,
        "print \"callteamvote: You cannot admitdefeat by yourself. Use /callvote draw.\n\"" );
      return;
    }

    Com_sprintf( level.teamVoteString[ cs_offset ],
      sizeof( level.teamVoteString[ cs_offset ] ), "admitdefeat %i", team );
    Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
        sizeof( level.teamVoteDisplayString[ cs_offset ] ),
        "Admit Defeat" );
  }
   else if( !Q_stricmp( arg1, "poll" ) )
   {
     if( arg2plus[ 0 ] == '\0' )
     {
       trap_SendServerCommand( ent-g_entities, "print \"callteamvote: You forgot to specify what people should vote on.\n\"" );
       return;
     }
     Com_sprintf( level.teamVoteString[ cs_offset ], sizeof( level.teamVoteString[ cs_offset ] ), nullstring );
     Com_sprintf( level.teamVoteDisplayString[ cs_offset ],
         sizeof( level.voteDisplayString ), "[Poll] \'%s\'", arg2plus );
   }
  else
  {
    trap_SendServerCommand( ent-g_entities, "print \"Invalid vote string\n\"" );
    trap_SendServerCommand( ent-g_entities,
       "print \"Valid team vote commands are: "
       "kick, poll, and admitdefeat\n\"" );
    return;
  }
  ent->client->pers.voteCount++;
  
  if ( reason[0]!='\0' )
    Q_strcat( level.teamVoteDisplayString[ cs_offset ], sizeof( level.teamVoteDisplayString[ cs_offset ] ), va( " Reason: '%s'^7", reason ) );

  for( i = 0 ; i < level.maxclients ; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_DISCONNECTED )
      continue;

    if( level.clients[ i ].ps.stats[ STAT_PTEAM ] == team )
    {
      trap_SendServerCommand( i, va("print \"%s " S_COLOR_WHITE
            "called a team vote: %s^7 \n\"", ent->client->pers.netname, level.teamVoteDisplayString[ cs_offset ] ) );
    }
    else if( G_admin_permission( &g_entities[ i ], ADMF_ADMINCHAT ) && 
             ( !Q_stricmp( arg1, "kick" ) || 
             level.clients[ i ].pers.teamSelection == PTE_NONE ) )
    {
      trap_SendServerCommand( i, va("print \"^6[Admins]^7 %s " S_COLOR_WHITE
            "called a team vote: %s^7 \n\"", ent->client->pers.netname, level.teamVoteDisplayString[ cs_offset ] ) );
    }
  }
  
  if(team==PTE_ALIENS)
    G_LogPrintf("Teamvote: %s^7 called a teamvote (aliens): %s^7\n", ent->client->pers.netname, level.teamVoteDisplayString[ cs_offset ] );
  else if(team==PTE_HUMANS)
    G_LogPrintf("Teamvote: %s^7 called a teamvote (humans): %s^7\n", ent->client->pers.netname, level.teamVoteDisplayString[ cs_offset ] );
  
  Q_strcat( level.teamVoteDisplayString[ cs_offset ], sizeof( level.teamVoteDisplayString[ cs_offset ] ), va( " Called by: '%s^7'", ent->client->pers.netname ) );

  // start the voting, the caller autoamtically votes yes
  level.teamVoteTime[ cs_offset ] = level.time;
  level.teamVoteNo[ cs_offset ] = 0;

  for( i = 0 ; i < level.maxclients ; i++ )
  {
    if( level.clients[ i ].ps.stats[ STAT_PTEAM ] == team )
      level.clients[ i ].ps.eFlags &= ~EF_TEAMVOTED;
  }

  if( !Q_stricmp( arg1, "poll" ) )
  {
    level.teamVoteYes[ cs_offset ] = 0;
  }
  else
  {
   level.teamVoteYes[ cs_offset ] = 1;
   ent->client->ps.eFlags |= EF_TEAMVOTED;
  }

  trap_SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va( "%i", level.teamVoteTime[ cs_offset ] ) );
  trap_SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteDisplayString[ cs_offset ] );
  trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va( "%i", level.teamVoteYes[ cs_offset ] ) );
  trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va( "%i", level.teamVoteNo[ cs_offset ] ) );
}


/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent )
{
  int     cs_offset = 0;
  char    msg[ 64 ];

  if( ent->client->pers.teamSelection == PTE_ALIENS )
    cs_offset = 1;

  if( !level.teamVoteTime[ cs_offset ] )
  {
    trap_SendServerCommand( ent-g_entities, "print \"No team vote in progress\n\"" );
    return;
  }

  if( ent->client->ps.eFlags & EF_TEAMVOTED )
  {
    trap_SendServerCommand( ent-g_entities, "print \"Team vote already cast\n\"" );
    return;
  }

  trap_SendServerCommand( ent-g_entities, "print \"Team vote cast\n\"" );

  ent->client->ps.eFlags |= EF_TEAMVOTED;

  trap_Argv( 1, msg, sizeof( msg ) );

  if( msg[ 0 ] == 'y' || msg[ 1 ] == 'Y' || msg[ 1 ] == '1' )
  {
    level.teamVoteYes[ cs_offset ]++;
    trap_SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va( "%i", level.teamVoteYes[ cs_offset ] ) );
  }
  else
  {
    level.teamVoteNo[ cs_offset ]++;
    trap_SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va( "%i", level.teamVoteNo[ cs_offset ] ) );
  }

  // a majority will be determined in TeamCheckVote, which will also account
  // for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent )
{
  vec3_t  origin, angles;
  char    buffer[ MAX_TOKEN_CHARS ];
  int     i;

  if( trap_Argc( ) != 5 )
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"usage: setviewpos x y z yaw\n\"" ) );
    return;
  }

  VectorClear( angles );

  for( i = 0 ; i < 3 ; i++ )
  {
    trap_Argv( i + 1, buffer, sizeof( buffer ) );
    origin[ i ] = atof( buffer );
  }

  trap_Argv( 4, buffer, sizeof( buffer ) );
  angles[ YAW ] = atof( buffer );

  TeleportPlayer( ent, origin, angles );
}


/*
=================
Cmd_Class_f
=================
*/
void Cmd_Class_f( gentity_t *ent )
{
  char      s[ MAX_TOKEN_CHARS ];
  int       clientNum;

  clientNum = ent->client - level.clients;
  trap_Argv( 1, s, sizeof( s ) );

  if( ent->client->sess.sessionTeam == TEAM_SPECTATOR )
  {
    if( ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
      G_StopFollowing( ent );

    if( ent->client->pers.teamSelection == PTE_ALIENS )
    {
      if( G_PushSpawnQueue( &level.alienSpawnQueue, clientNum ) )
      {
        if( ent->client->pers.statscounters.tremball_team == 1 ) // RED
        {
          if( ent->client->pers.statscounters.tremball_goalie == 1 )
          {
            ent->client->pers.classSelection = PCL_ALIEN_LEVEL4;
            ent->client->ps.stats[ STAT_PCLASS ] = PCL_ALIEN_LEVEL4;
          }
          else
          {
            ent->client->pers.classSelection = PCL_ALIEN_LEVEL3;
            ent->client->ps.stats[ STAT_PCLASS ] = PCL_ALIEN_LEVEL3;
          }
        }
        if( ent->client->pers.statscounters.tremball_team == 2 ) // BLUE
        {
          if( ent->client->pers.statscounters.tremball_goalie == 1 )
          {
            ent->client->pers.classSelection = PCL_ALIEN_LEVEL4;
            ent->client->ps.stats[ STAT_PCLASS ] = PCL_ALIEN_LEVEL4;
          }
          else
          {
            ent->client->pers.classSelection = PCL_ALIEN_LEVEL3_UPG;
            ent->client->ps.stats[ STAT_PCLASS ] = PCL_ALIEN_LEVEL3_UPG;
          }
        }
      }
    }
    return;
  }

  if( ent->health <= 0 )
    return;
}


/*
=================
Cmd_Boost_f
=================
*/
void Cmd_Boost_f( gentity_t *ent )
{
  return;
}


/*
=================
Cmd_MyStats_f
=================
*/
void Cmd_MyStats_f( gentity_t *ent )
{

   if(!ent) return;


   if( !level.intermissiontime && ent->client->pers.statscounters.timeLastViewed && (level.time - ent->client->pers.statscounters.timeLastViewed) <60000 ) 
   {   
     ADMP( "You may only check your stats once per minute and during intermission.\n");
     return;
   }
   
   if( !g_myStats.integer )
   {
    ADMP( "myStats has been disabled\n");
    return;
   }
   
   ADMP( G_statsString( &ent->client->pers.statscounters, &ent->client->pers.teamSelection ) );
   ent->client->pers.statscounters.timeLastViewed = level.time;
  
  return;
}

char *G_statsString( statsCounters_t *sc, pTeam_t *pt )
{
  char *s;
  
  int percentNearBase=0;
  int percentJetpackWallwalk=0;
  int percentHeadshots=0;
  double avgTimeAlive=0;
  int avgTimeAliveMins = 0;
  int avgTimeAliveSecs = 0;

  if( sc->timealive )
   percentNearBase = (int)(100 *  (float) sc->timeinbase / ((float) (sc->timealive ) ) );

  if( sc->timealive && sc->deaths )
  {
    avgTimeAlive = sc->timealive / sc->deaths;
  }

  avgTimeAliveMins = (int) (avgTimeAlive / 60.0f);
  avgTimeAliveSecs = (int) (avgTimeAlive - (60.0f * avgTimeAliveMins));
  
  if( *pt == PTE_ALIENS )
  {
    if( sc->dretchbasytime > 0 )
     percentJetpackWallwalk = (int)(100 *  (float) sc->jetpackusewallwalkusetime / ((float) ( sc->dretchbasytime) ) );
    
    if( sc->hitslocational )
      percentHeadshots = (int)(100 * (float) sc->headshots / ((float) (sc->hitslocational) ) );
    
    s = va( "^3Kills:^7 %3i ^3StructKills:^7 %3i ^3Assists:^7 %3i^7 ^3Poisons:^7 %3i ^3Headshots:^7 %3i (%3i)\n^3Deaths:^7 %3i ^3Feeds:^7 %3i ^3Suicides:^7 %3i ^3TKs:^7 %3i ^3Avg Lifespan:^7 %4d:%02d\n^3Damage to:^7 ^3Enemies:^7 %5i ^3Structs:^7 %5i ^3Friendlies:^7 %3i \n^3Structs Built:^7 %3i ^3Time Near Base:^7 %3i ^3Time wallwalking:^7 %3i\n",
     sc->kills,
     sc->structskilled,
     sc->assists,
     sc->repairspoisons,
     sc->headshots,
     percentHeadshots,
     sc->deaths,
     sc->feeds,
     sc->suicides,
     sc->teamkills,
     avgTimeAliveMins,
     avgTimeAliveSecs,
     sc->dmgdone,
     sc->structdmgdone,
     sc->ffdmgdone,
     sc->structsbuilt,
     percentNearBase,
     percentJetpackWallwalk
         );
  }
  else if( *pt == PTE_HUMANS )
  {
    if( sc->timealive )
     percentJetpackWallwalk = (int)(100 *  (float) sc->jetpackusewallwalkusetime / ((float) ( sc->timealive ) ) );
    s = va( "^3Kills:^7 %3i ^3StructKills:^7 %3i ^3Assists:^7 %3i \n^3Deaths:^7 %3i ^3Feeds:^7 %3i ^3Suicides:^7 %3i ^3TKs:^7 %3i ^3Avg Lifespan:^7 %4d:%02d\n^3Damage to:^7 ^3Enemies:^7 %5i ^3Structs:^7 %5i ^3Friendlies:^7 %3i \n^3Structs Built:^7 %3i ^3Repairs:^7 %4i ^3Time Near Base:^7 %3i ^3Time Jetpacking:^7 %3i\n",
     sc->kills,
     sc->structskilled,
     sc->assists,
     sc->deaths,
     sc->feeds,
     sc->suicides,
     sc->teamkills,
     avgTimeAliveMins,
     avgTimeAliveSecs,
     sc->dmgdone,
     sc->structdmgdone,
     sc->ffdmgdone,
     sc->structsbuilt,
     sc->repairspoisons,
     percentNearBase,
     percentJetpackWallwalk
         );
  }
  else s="No stats available\n";

  return s;
}



/*
=================
G_StopFromFollowing

stops any other clients from following this one
called when a player leaves a team or dies
=================
*/
void G_StopFromFollowing( gentity_t *ent )
{
  int i;

  for( i = 0; i < level.maxclients; i++ )
  {
    if( level.clients[ i ].sess.spectatorState == SPECTATOR_FOLLOW &&
        level.clients[ i ].sess.spectatorClient == ent-g_entities )
    {
      if( !G_FollowNewClient( &g_entities[ i ], 1 ) )
        G_StopFollowing( &g_entities[ i ] );
    }
  }
}

/*
=================
G_StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void G_StopFollowing( gentity_t *ent )
{
  ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;
  ent->client->sess.sessionTeam = TEAM_SPECTATOR;
  ent->client->ps.stats[ STAT_PTEAM ] = ent->client->pers.teamSelection;

  if( ent->client->pers.teamSelection == PTE_NONE )
  {
    ent->client->sess.spectatorState = SPECTATOR_FREE;
    ent->client->ps.pm_type = PM_SPECTATOR;
    ent->client->ps.stats[ STAT_HEALTH ] = 100; // hacky server-side fix to prevent cgame from viewlocking a freespec
  }
  else
  {
    vec3_t   spawn_origin, spawn_angles;

    ent->client->sess.spectatorState = SPECTATOR_LOCKED;
    if( ent->client->pers.teamSelection == PTE_ALIENS )
      G_SelectAlienLockSpawnPoint( spawn_origin, spawn_angles );
    else if( ent->client->pers.teamSelection == PTE_HUMANS )
      G_SelectHumanLockSpawnPoint( spawn_origin, spawn_angles );
    G_SetOrigin( ent, spawn_origin );
    VectorCopy( spawn_origin, ent->client->ps.origin );
    G_SetClientViewAngle( ent, spawn_angles );
  }
  ent->client->sess.spectatorClient = -1;
  ent->client->ps.pm_flags &= ~PMF_FOLLOW;

  // Prevent spawning with bsuit in rare case
  if( BG_InventoryContainsUpgrade( UP_BATTLESUIT, ent->client->ps.stats ) )
    BG_RemoveUpgradeFromInventory( UP_BATTLESUIT, ent->client->ps.stats );

  ent->client->ps.stats[ STAT_STATE ] &= ~SS_WALLCLIMBING;
  ent->client->ps.stats[ STAT_STATE ] &= ~SS_WALLCLIMBINGCEILING;
  ent->client->ps.eFlags &= ~EF_WALLCLIMB;
  ent->client->ps.viewangles[ PITCH ] = 0.0f;

  ent->client->ps.clientNum = ent - g_entities;

  CalculateRanks( );
}

/*
=================
G_FollowNewClient

This was a really nice, elegant function. Then I fucked it up.
=================
*/
qboolean G_FollowNewClient( gentity_t *ent, int dir )
{
  int       clientnum = ent->client->sess.spectatorClient;
  int       original = clientnum;
  qboolean  selectAny = qfalse;

  if( dir > 1 )
    dir = 1;
  else if( dir < -1 )
    dir = -1;
  else if( dir == 0 )
    return qtrue;

  if( ent->client->sess.sessionTeam != TEAM_SPECTATOR )
    return qfalse;

  // select any if no target exists
  if( clientnum < 0 || clientnum >= level.maxclients )
  {
    clientnum = original = 0;
    selectAny = qtrue;
  }

  do
  {
    clientnum += dir;

    if( clientnum >= level.maxclients )
      clientnum = 0;

    if( clientnum < 0 )
      clientnum = level.maxclients - 1;

    // avoid selecting existing follow target
    if( clientnum == original && !selectAny )
      continue; //effectively break;

    // can't follow self
    if( &level.clients[ clientnum ] == ent->client )
      continue;

    // can only follow connected clients
    if( level.clients[ clientnum ].pers.connected != CON_CONNECTED )
      continue;

    // can't follow another spectator
     if( level.clients[ clientnum ].pers.teamSelection == PTE_NONE )
       continue;
     
      // can only follow teammates when dead and on a team
     if( ent->client->pers.teamSelection != PTE_NONE && 
         ( level.clients[ clientnum ].pers.teamSelection != 
           ent->client->pers.teamSelection ) )
       continue;
     
     // cannot follow a teammate who is following you
     if( level.clients[ clientnum ].sess.spectatorState == SPECTATOR_FOLLOW && 
         ( level.clients[ clientnum ].sess.spectatorClient == ent->s.number ) )
       continue;

    // this is good, we can use it
    ent->client->sess.spectatorClient = clientnum;
    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    return qtrue;

  } while( clientnum != original );

  return qfalse;
}

/*
=================
G_ToggleFollow
=================
*/
void G_ToggleFollow( gentity_t *ent )
{
  if( ent->client->sess.spectatorState == SPECTATOR_FOLLOW )
    G_StopFollowing( ent );
  else
    G_FollowNewClient( ent, 1 );
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent )
{
  int   i;
  int   pids[ MAX_CLIENTS ];
  char  arg[ MAX_TOKEN_CHARS ];

  if( ent->client->sess.sessionTeam != TEAM_SPECTATOR )
  {
    trap_SendServerCommand( ent - g_entities, "print \"follow: You cannot follow unless you are dead or on the spectators.\n\"" );
    return;
  }

  if( trap_Argc( ) != 2 )
  {
    G_ToggleFollow( ent );
  }
  else
  {
    trap_Argv( 1, arg, sizeof( arg ) );
    if( G_ClientNumbersFromString( arg, pids ) == 1 )
    {
      i = pids[ 0 ];
    }
    else
    {
      i = G_ClientNumberFromString( ent, arg );

      if( i == -1 )
      {
        trap_SendServerCommand( ent - g_entities,
          "print \"follow: invalid player\n\"" );
        return;
      }
    }

    // can't follow self
    if( &level.clients[ i ] == ent->client )
    {
      trap_SendServerCommand( ent - g_entities, "print \"follow: You cannot follow yourself.\n\"" );
      return;
    }

    // can't follow another spectator
    if( level.clients[ i ].pers.teamSelection == PTE_NONE)
    {
      trap_SendServerCommand( ent - g_entities, "print \"follow: You cannot follow another spectator.\n\"" );
      return;
    }

    // can only follow teammates when dead and on a team
    if( ent->client->pers.teamSelection != PTE_NONE && 
        ( level.clients[ i ].pers.teamSelection != 
          ent->client->pers.teamSelection ) )
    {
      trap_SendServerCommand( ent - g_entities, "print \"follow: You can only follow teammates, and only when you are dead.\n\"" );
      return;
    }

    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    ent->client->sess.spectatorClient = i;
  }
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent )
{
  char args[ 11 ];
  int  dir = 1;

  trap_Argv( 0, args, sizeof( args ) );
  if( Q_stricmp( args, "followprev" ) == 0 )
    dir = -1;

  // won't work unless spectating
   if( ent->client->sess.sessionTeam != TEAM_SPECTATOR )
     return;
   if( ent->client->sess.spectatorState == SPECTATOR_NOT )
     return;
  G_FollowNewClient( ent, dir );
}

/*
=================
Cmd_PTRCVerify_f

Check a PTR code is valid
=================
*/
void Cmd_PTRCVerify_f( gentity_t *ent )
{
  connectionRecord_t  *connection;
  char                s[ MAX_TOKEN_CHARS ] = { 0 };
  int                 code;

  if( ent->client->pers.connection )
    return;

  trap_Argv( 1, s, sizeof( s ) );

  if( !strlen( s ) )
    return;

  code = atoi( s );

  connection = G_FindConnectionForCode( code );
  if( connection && connection->clientNum == -1 )
  {
    // valid code
    if( connection->clientTeam != PTE_NONE )
      trap_SendServerCommand( ent->client->ps.clientNum, "ptrcconfirm" );

    // restore mapping
    ent->client->pers.connection = connection;
    connection->clientNum = ent->client->ps.clientNum;
  }
  else
  {
    // invalid code -- generate a new one
    connection = G_GenerateNewConnection( ent->client );

    if( connection )
    {
      trap_SendServerCommand( ent->client->ps.clientNum,
        va( "ptrcissue %d", connection->ptrCode ) );
    }
  }
}

/*
=================
Cmd_PTRCRestore_f

Restore against a PTR code
=================
*/
void Cmd_PTRCRestore_f( gentity_t *ent )
{
  char                s[ MAX_TOKEN_CHARS ] = { 0 };
  int                 code;
  connectionRecord_t  *connection;

  if( ent->client->pers.joinedATeam )
  {
    trap_SendServerCommand( ent - g_entities,
      "print \"You cannot use a PTR code after joining a team\n\"" );
    return;
  }

  trap_Argv( 1, s, sizeof( s ) );

  if( !strlen( s ) )
    return;

  code = atoi( s );

  connection = ent->client->pers.connection;
  if( connection && connection->ptrCode == code )
  {
    // set the correct team
    G_ChangeTeam( ent, connection->clientTeam );

    // set the correct credit etc.
    ent->client->ps.persistant[ PERS_CREDIT ] = 0;
    G_AddCreditToClient( ent->client, connection->clientCredit, qtrue );
    ent->client->pers.score = connection->clientScore;
    ent->client->pers.enterTime = connection->clientEnterTime;
  }
  else
  {
    trap_SendServerCommand( ent - g_entities,
      va( "print \"\"%d\" is not a valid PTR code\n\"", code ) );
  }
}

static void Cmd_Ignore_f( gentity_t *ent )
{
  int pids[ MAX_CLIENTS ];
  char name[ MAX_NAME_LENGTH ];
  char cmd[ 9 ];
  int matches = 0;
  int i;
  qboolean ignore = qfalse;

  trap_Argv( 0, cmd, sizeof( cmd ) );
  if( Q_stricmp( cmd, "ignore" ) == 0 )
    ignore = qtrue;

  if( trap_Argc() < 2 )
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
      "%s: usage \\%s [clientNum | partial name match]\n\"", cmd, cmd ) );
    return;
  }

  Q_strncpyz( name, ConcatArgs( 1 ), sizeof( name ) );
  matches = G_ClientNumbersFromString( name, pids ); 
  if( matches < 1 )
  {
    trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
      "%s: no clients match the name '%s'\n\"", cmd, name ) );
    return;
  }

  for( i = 0; i < matches; i++ )
  {
    if( ignore )
    {
      if( !BG_ClientListTest( &ent->client->sess.ignoreList, pids[ i ] ) )
      {
        BG_ClientListAdd( &ent->client->sess.ignoreList, pids[ i ] );
        ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );
        trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "ignore: added %s^7 to your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
      else
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "ignore: %s^7 is already on your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
    }
    else
    {
      if( BG_ClientListTest( &ent->client->sess.ignoreList, pids[ i ] ) )
      {
        BG_ClientListRemove( &ent->client->sess.ignoreList, pids[ i ] );
        ClientUserinfoChanged( ent->client->ps.clientNum, qfalse );
        trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "unignore: removed %s^7 from your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
      else
      {
        trap_SendServerCommand( ent-g_entities, va( "print \"[skipnotify]"
          "unignore: %s^7 is not on your ignore list\n\"",
          level.clients[ pids[ i ] ].pers.netname ) );
      }
    }
  }
}


commands_t cmds[ ] = {
  // normal commands
  { "team", 0, Cmd_Team_f },
  { "vote", 0, Cmd_Vote_f },
  { "ignore", 0, Cmd_Ignore_f },
  { "unignore", 0, Cmd_Ignore_f },

  // communication commands
  { "tell", CMD_MESSAGE, Cmd_Tell_f },
  { "callvote", CMD_MESSAGE, Cmd_CallVote_f },
  { "callteamvote", CMD_MESSAGE|CMD_TEAM, Cmd_CallTeamVote_f },
  { "say_area", CMD_MESSAGE|CMD_TEAM, Cmd_SayArea_f },
  // can be used even during intermission
  { "say", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "say_team", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "say_admins", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "a", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "m", CMD_MESSAGE|CMD_INTERMISSION, G_PrivateMessage },
  { "mt", CMD_MESSAGE|CMD_INTERMISSION, G_PrivateMessage },
  { "me", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },
  { "me_team", CMD_MESSAGE|CMD_INTERMISSION, Cmd_Say_f },

  { "score", CMD_INTERMISSION, ScoreboardMessage },
  { "mystats", CMD_TEAM|CMD_INTERMISSION, Cmd_MyStats_f },

  // cheats
  { "god", CMD_CHEAT|CMD_TEAM|CMD_LIVING, Cmd_God_f },
  { "notarget", CMD_CHEAT|CMD_TEAM|CMD_LIVING, Cmd_Notarget_f },
  { "noclip", CMD_CHEAT|CMD_TEAM|CMD_LIVING, Cmd_Noclip_f },
  { "levelshot", CMD_CHEAT, Cmd_LevelShot_f },
  { "setviewpos", CMD_CHEAT, Cmd_SetViewpos_f },

  { "kill", CMD_TEAM|CMD_LIVING, Cmd_Kill_f },

  // game commands
  { "ptrcverify", CMD_NOTEAM, Cmd_PTRCVerify_f },
  { "ptrcrestore", CMD_NOTEAM, Cmd_PTRCRestore_f },

  { "follow", 0, Cmd_Follow_f },
  { "follownext", 0, Cmd_FollowCycle_f },
  { "followprev", 0, Cmd_FollowCycle_f },

  { "where", CMD_TEAM, Cmd_Where_f },
  { "teamvote", CMD_TEAM, Cmd_TeamVote_f },
  { "class", CMD_TEAM, Cmd_Class_f },

  { "boost", 0, Cmd_Boost_f },
};
static int numCmds = sizeof( cmds ) / sizeof( cmds[ 0 ] );

/*
=================
ClientCommand
=================
*/
void ClientCommand( int clientNum )
{
  gentity_t *ent;
  char      cmd[ MAX_TOKEN_CHARS ];
  int       i;

  ent = g_entities + clientNum;
  if( !ent->client )
    return;   // not fully in game yet

  trap_Argv( 0, cmd, sizeof( cmd ) );

  for( i = 0; i < numCmds; i++ )
  {
    if( Q_stricmp( cmd, cmds[ i ].cmdName ) == 0 )
      break;
  }

  if( i == numCmds )
  {
    if( !G_admin_cmd_check( ent, qfalse ) )
      trap_SendServerCommand( clientNum,
        va( "print \"Unknown command %s\n\"", cmd ) );
    return;
  }

  // do tests here to reduce the amount of repeated code

  if( !( cmds[ i ].cmdFlags & CMD_INTERMISSION ) && ( level.intermissiontime || level.paused ) )
    return;

  if( cmds[ i ].cmdFlags & CMD_CHEAT && !g_cheats.integer )
  {
    trap_SendServerCommand( clientNum,
      "print \"Cheats are not enabled on this server\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_MESSAGE && ent->client->pers.muted )
  {
    trap_SendServerCommand( clientNum,
      "print \"You are muted and cannot use message commands.\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_TEAM &&
      ent->client->pers.teamSelection == PTE_NONE )
  {
    trap_SendServerCommand( clientNum, "print \"Join a team first\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_NOTEAM &&
      ent->client->pers.teamSelection != PTE_NONE )
  {
    trap_SendServerCommand( clientNum,
      "print \"Cannot use this command when on a team\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_ALIEN &&
      ent->client->pers.teamSelection != PTE_ALIENS )
  {
    trap_SendServerCommand( clientNum,
      "print \"Must be alien to use this command\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_HUMAN &&
      ent->client->pers.teamSelection != PTE_HUMANS )
  {
    trap_SendServerCommand( clientNum,
      "print \"Must be human to use this command\n\"" );
    return;
  }

  if( cmds[ i ].cmdFlags & CMD_LIVING &&
    ( ent->client->ps.stats[ STAT_HEALTH ] <= 0 ||
      ent->client->sess.sessionTeam == TEAM_SPECTATOR ) )
  {
    trap_SendServerCommand( clientNum,
      "print \"Must be living to use this command\n\"" );
    return;
  }

  cmds[ i ].cmdHandler( ent );
}

int G_SayArgc( void )
{
  int c = 0;
  char *s;

  s = ConcatArgs( 0 );
  while( 1 )
  {
    while( *s == ' ' )
      s++;
    if( !*s )
      break;
    c++;
    while( *s && *s != ' ' )
      s++;
  }
  return c;
}

qboolean G_SayArgv( int n, char *buffer, int bufferLength )
{
  int bc = 0;
  int c = 0;
  char *s;

  if( bufferLength < 1 )
    return qfalse;
  if( n < 0 )
    return qfalse;
  s = ConcatArgs( 0 );
  while( c < n )
  {
    while( *s == ' ' )
      s++;
    if( !*s )
      break;
    c++;
    while( *s && *s != ' ' )
      s++;
  }
  if( c < n )
    return qfalse;
  while( *s == ' ' )
    s++;
  if( !*s )
    return qfalse;
  //memccpy( buffer, s, ' ', bufferLength );
  while( bc < bufferLength - 1 && *s && *s != ' ' )
    buffer[ bc++ ] = *s++;
  buffer[ bc ] = 0;
  return qtrue;
}

char *G_SayConcatArgs( int start )
{
  char *s;
  int c = 0;

  s = ConcatArgs( 0 );
  while( c < start )
  {
    while( *s == ' ' )
      s++;
    if( !*s )
      break;
    c++;
    while( *s && *s != ' ' )
      s++;
  }
  while( *s == ' ' )
    s++;
  return s;
}

void G_DecolorString( char *in, char *out )
{   
  while( *in ) {
    if( *in == 27 || *in == '^' ) {
      in++;
      if( *in )
        in++;
      continue;
    }
    *out++ = *in++;
  }
  *out = '\0';
}

void G_ParseEscapedString( char *buffer )
{
  int i = 0;
  int j = 0;

  while( buffer[i] )
  {
    if(!buffer[i]) break;

    if(buffer[i] == '\\')
    {
      if(buffer[i + 1] == '\\')
        buffer[j] = buffer[++i];
      else if(buffer[i + 1] == 'n')
      {
        buffer[j] = '\n';
        i++;
      }
      else
        buffer[j] = buffer[i];
    }
    else
      buffer[j] = buffer[i];

    i++;
    j++;
  }
  buffer[j] = 0;
}

void G_WordWrap( char *buffer, int maxwidth )
{
  char out[ MAX_STRING_CHARS ];
  int i = 0;
  int j = 0;
  int k;
  int linecount = 0;
  int currentcolor = 7;

  while ( buffer[ j ]!='\0' )
  {
     if( i == ( MAX_STRING_CHARS - 1 ) )
       break;

     //If it's the start of a new line, copy over the color code,
     //but not if we already did it, or if the text at the start of the next line is also a color code
     if( linecount == 0 && i>2 && out[ i-2 ] != Q_COLOR_ESCAPE && out[ i-1 ] != Q_COLOR_ESCAPE )
     {
       out[ i ] = Q_COLOR_ESCAPE;
       out[ i + 1 ] = '0' + currentcolor; 
       i+=2;
       continue;
     }

     if( linecount < maxwidth )
     {
       out[ i ] = buffer[ j ];
       if( out[ i ] == '\n' ) 
       {
         linecount = 0;
       }
       else if( Q_IsColorString( &buffer[j] ) )
       {
         currentcolor = buffer[j+1] - '0';
       }
       else
         linecount++;
       
       //If we're at a space and getting close to a line break, look ahead and make sure that there isn't already a \n or a closer space coming. If not, break here.
      if( out[ i ] == ' ' && linecount >= (maxwidth - 10 ) ) 
      {
        qboolean foundbreak = qfalse;
        for( k = i+1; k < maxwidth; k++ )
        {
          if( !buffer[ k ] )
            continue;
          if( buffer[ k ] == '\n' || buffer[ k ] == ' ' )
            foundbreak = qtrue;
        }
        if( !foundbreak )
        {
          out [ i ] = '\n';
          linecount = 0;
        }
      }
       
      i++;
      j++;
     }
     else
     {
       out[ i ] = '\n';
       i++;
       linecount = 0;
     }
  }
  out[ i ] = '\0';


  strcpy( buffer, out );
}

void G_PrivateMessage( gentity_t *ent )
{
  int pids[ MAX_CLIENTS ];
  int ignoreids[ MAX_CLIENTS ];
  char name[ MAX_NAME_LENGTH ];
  char cmd[ 12 ];
  char str[ MAX_STRING_CHARS ];
  char *msg;
  char color;
  int pcount, matches, ignored = 0;
  int i;
  int skipargs = 0;
  qboolean teamonly = qfalse;
  gentity_t *tmpent;

  if( !g_privateMessages.integer && ent )
  {
    ADMP( "Sorry, but private messages have been disabled\n" );
    return;
  }
  
  if( g_floodMinTime.integer )
   if ( G_Flood_Limited( ent ) )
   {
    trap_SendServerCommand( ent-g_entities, "print \"Your chat is flood-limited; wait before chatting again\n\"" );
    return;
   }

  G_SayArgv( 0, cmd, sizeof( cmd ) );
  if( !Q_stricmp( cmd, "say" ) || !Q_stricmp( cmd, "say_team" ) )
  {
    skipargs = 1;
    G_SayArgv( 1, cmd, sizeof( cmd ) );
  }
  if( G_SayArgc( ) < 3+skipargs )
  {
    ADMP( va( "usage: %s [name|slot#] [message]\n", cmd ) );
    return;
  }

  if( !Q_stricmp( cmd, "mt" ) || !Q_stricmp( cmd, "/mt" ) )
    teamonly = qtrue;

  G_SayArgv( 1+skipargs, name, sizeof( name ) );
  msg = G_SayConcatArgs( 2+skipargs );
  pcount = G_ClientNumbersFromString( name, pids );

  if( ent )
  {
    int count = 0;

    for( i=0; i < pcount; i++ )
    {
      tmpent = &g_entities[ pids[ i ] ];

      if( teamonly && !OnSameTeam( ent, tmpent ) )
        continue;

      if( BG_ClientListTest( &tmpent->client->sess.ignoreList,
        ent-g_entities ) )
      {
        ignoreids[ ignored++ ] = pids[ i ];
        continue;
      }

      pids[ count ] = pids[ i ];
      count++;
    }
    matches = count;
  }
  else
  {
    matches = pcount;
  }

  color = teamonly ? COLOR_CYAN : COLOR_YELLOW;

  if( !Q_stricmp( name, "console" ) )
  {
    ADMP( va( "^%cPrivate message: ^7%s\n", color, msg ) );
    ADMP( va( "^%csent to Console.\n", color ) );

    G_LogPrintf( "privmsg: %s^7: Console: ^6%s^7\n",
      ( ent ) ? ent->client->pers.netname : "Console", msg );

    return;
  }

  Q_strncpyz( str,
    va( "^%csent to %i player%s: ^7", color, matches,
      ( matches == 1 ) ? "" : "s" ),
    sizeof( str ) );

  for( i=0; i < matches; i++ )
  {
    tmpent = &g_entities[ pids[ i ] ];

    if( i > 0 )
      Q_strcat( str, sizeof( str ), "^7, " );
    Q_strcat( str, sizeof( str ), tmpent->client->pers.netname );
    trap_SendServerCommand( pids[ i ], va(
      "chat \"%s^%c -> ^7%s^7: (%d recipients): ^%c%s^7\" %li",
      ( ent ) ? ent->client->pers.netname : "console",
      color,
      name,
      matches,
      color,
      msg,
      ent ? ent-g_entities : -1 ) );

    trap_SendServerCommand( pids[ i ], va( 
      "cp \"^%cprivate message from ^7%s^7\"", color,
      ( ent ) ? ent->client->pers.netname : "console" ) );
  }

  if( !matches )
    ADMP( va( "^3No player matching ^7\'%s^7\' ^3to send message to.\n",
      name ) );
  else
  {
    if( ent )
      ADMP( va( "^%cPrivate message: ^7%s\n", color, msg ) );

    ADMP( va( "%s\n", str ) );

    G_LogPrintf( "%s: %s^7: %s^7: %s\n",
      ( teamonly ) ? "tprivmsg" : "privmsg",
      ( ent ) ? ent->client->pers.netname : "console",
      name, msg );
  }

  if( ignored )
  {
    Q_strncpyz( str, va( "^%cignored by %i player%s: ^7", color, ignored,
      ( ignored == 1 ) ? "" : "s" ), sizeof( str ) );
    for( i=0; i < ignored; i++ )
    {
      tmpent = &g_entities[ ignoreids[ i ] ];
      if( i > 0 )
        Q_strcat( str, sizeof( str ), "^7, " );
      Q_strcat( str, sizeof( str ), tmpent->client->pers.netname );
    }
    ADMP( va( "%s\n", str ) );
  }
}


void G_CP( gentity_t *ent )
 { 
   int i;
   char buffer[MAX_STRING_CHARS];
   char prefixes[MAX_STRING_CHARS] = "";
   char wrappedtext[ MAX_STRING_CHARS ] = "";
   char *ptr;
   char *text;
   qboolean sendAliens = qtrue;
   qboolean sendHumans = qtrue;
   qboolean sendSpecs = qtrue;
   Q_strncpyz( buffer, ConcatArgs( 1 ), sizeof( buffer ) );
   G_ParseEscapedString( buffer );

   if( strstr( buffer, "!cp" ) )
   {
     ptr = buffer;
     while( *ptr != '!' )
       ptr++;
     ptr+=4;
     
     Q_strncpyz( buffer, ptr, sizeof(buffer) );
   }

   text = buffer;

   ptr = buffer;
   while( *ptr == ' ' )
     ptr++;
   if( *ptr == '-' )
   {
      sendAliens = qfalse;
      sendHumans = qfalse;
      sendSpecs = qfalse;
      Q_strcat( prefixes, sizeof( prefixes ), " " );
      ptr++;

      while( *ptr && *ptr != ' ' )
      {
        if( !sendAliens && ( *ptr == 'a' || *ptr == 'A' ) )
        {
          sendAliens = qtrue;
          Q_strcat( prefixes, sizeof( prefixes ), "[A]" );
        }
        if( !sendHumans && ( *ptr == 'h' || *ptr == 'H' ) )
        {
          sendHumans = qtrue;
          Q_strcat( prefixes, sizeof( prefixes ), "[H]" );
        }
        if( !sendSpecs && ( *ptr == 's' || *ptr == 'S' ) )
        {
          sendSpecs = qtrue;
          Q_strcat( prefixes, sizeof( prefixes ), "[S]" );
        }
        ptr++;
      }
      if( *ptr ) text = ptr+1;
      else text = ptr;
   }
  
  strcpy( wrappedtext, text );

  if( strlen( text ) == 0 ) return;

  G_WordWrap( wrappedtext, 50 );

  for( i = 0; i < level.maxclients; i++ )
  {
    if( level.clients[ i ].pers.connected == CON_DISCONNECTED )
      continue;

    if( ( !sendAliens && level.clients[ i ].pers.teamSelection == PTE_ALIENS ) ||
         ( !sendHumans && level.clients[ i ].pers.teamSelection == PTE_HUMANS ) ||
         ( !sendSpecs && level.clients[ i ].pers.teamSelection == PTE_NONE ) )
    {
      if( G_admin_permission( &g_entities[ i ], ADMF_ADMINCHAT ) )
      {
        trap_SendServerCommand( i, va("print \"^6[Admins]^7 CP to other team%s: %s \n\"", prefixes, text ) );
      }
      continue;
    }

      trap_SendServerCommand( i, va( "cp \"%s\"", wrappedtext ) );
      trap_SendServerCommand( i, va( "print \"%s^7 CP%s: %s\n\"", ( ent ? G_admin_adminPrintName( ent ) : "console" ), prefixes, text ) );
    }

     G_Printf( "cp: %s\n", ConcatArgs( 1 ) );
 }

