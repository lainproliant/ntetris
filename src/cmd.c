#include "cmd.h"
#include "player.h"
#include "packet.h"
#include "macros.h"
#include "room.h"

#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __sun
#include "strtonum.h"
#include <mtmalloc.h>
#elif defined(__linux__)
#include <bsd/stdlib.h>
#include <bsd/string.h>
#endif

void gh_freeplayer(gpointer k, gpointer v, gpointer d)
{
    player_t *p = (player_t*)v;
    destroyPlayer(p);
}

/* Function to feed to g_hash_table_foreach */
void gh_kickPlayer(gpointer k, gpointer v, gpointer d)
{
    sendKickPacket((player_t*)v, (const char*)d, g_server->listenSock);
    destroyPlayer((player_t*)v);
}

void killPlayers(uv_timer_t *h)
{
    uv_rwlock_wrlock(g_server->playerTableLock);
    g_hash_table_foreach(g_server->playersById, (GHFunc)gh_freeplayer, NULL);
    g_hash_table_remove_all(g_server->playersById);
    g_hash_table_remove_all(g_server->playersByNames);
    uv_rwlock_wrunlock(g_server->playerTableLock);
}

void graceful_shutdown(const char *reason)
{
    const char *kickReason = NULL;
    const char *defaultReason = "Server going down";

    /* If empty or NULL string (empty if strsep used) */
    kickReason = ((reason) && strlen(reason)) ? reason : defaultReason;

    g_hash_table_foreach_steal(g_server->playersById,
                              (GHRFunc)gh_kickPlayer, 
                              (gpointer)kickReason);

    PRINT("Taking server down with reason: %s\n", kickReason);
    /* Do other cleanup stuff here */
    uv_loop_close(uv_default_loop());
    exit(0);
}

void parse_cmd(const char *cmd)
{
    const char *stn_err_str= NULL;
    const char *pNameOrId = NULL;
    const char *reason = NULL;
    const char *srvcmd = strsep((char**)&cmd, " \n");
    unsigned int id;

    if (!strncmp(srvcmd, "lsplayers", 9)) {
        printPlayers();
    } else if (!strncmp(srvcmd, "lsrooms", 7)) {
        printRooms();
    } else if (!strncmp(srvcmd, "kickname", 8)) {
        pNameOrId = strsep((char**)&cmd, "\n");
        kickPlayerByName(pNameOrId);
    } else if (!strncmp(srvcmd, "kickidreason", 11)) {
        pNameOrId = strsep((char**)&cmd, " ");

        if (pNameOrId == NULL || strlen(pNameOrId) == 0) {
            WARN("Syntax: kickidreason <userid> <reason>");
            return;
        }

        reason = strsep((char**)&cmd, "\n");

        if (reason == NULL || strlen(reason) == 0) {
            WARN("Syntax: kickidreason <userid> <reason>");
            return;
        }

        id = strtonum(pNameOrId, 0, UINT32_MAX, &stn_err_str);

        if (stn_err_str) {
            WARNING("Can't parse uid %s: %s", pNameOrId, stn_err_str);
        } else {
            kickPlayerById(id, reason);
        }
    } else if (!strncmp(srvcmd, "kickall", 6)) {
        killPlayers(NULL);
    } else if (!strncmp(srvcmd, "kickid", 6)) {
        pNameOrId = strsep((char**)&cmd, "\n");

        if (pNameOrId == NULL || strlen(pNameOrId) == 0) {
            WARN("Syntax: kickid <userid>");
            return;
        }

        id = strtonum(pNameOrId, 0, UINT32_MAX, &stn_err_str);

        if (stn_err_str) {
            WARNING("Can't parse uid %s: %s", pNameOrId, stn_err_str);
        } else {
            kickPlayerById(id, NULL);
        }
    } else if (!strncmp(srvcmd, "quit", 5)) {
        reason = strsep((char**)&cmd, "\n");
        graceful_shutdown(reason); 
    } else {
        WARNING("command %s not recognized", srvcmd);
    }
}
