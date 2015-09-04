#include <string.h>
#include <stdlib.h>
#include "room.h"
#include "rand.h"
#include "player.h"

room_t *createRoom(msg_create_room *m, unsigned int id)
{
    room_t *r = calloc(1, sizeof(room_t));
    r->name = calloc(m->roomNameLen, sizeof(char));

    strlcpy(r->name, m->roomNameAndPass, m->roomNameLen); 

    if (m->passLen > 0) {
        r->password = calloc(m->passLen, sizeof(char));
        strlcpy(r->password, m->roomNameAndPass + m->roomNameLen, m->passLen); 
    }

    return r;
}

unsigned int genRoomId(GHashTable *roomsById)
{
    unsigned int retVal;

    do {
        retVal = getRand();
    } while (g_hash_table_lookup(roomsById, GUINT_TO_POINTER(retVal)));

    return retVal;
}

void kickPlayerFromRoom(gpointer p, gpointer msg)
{
    player_t *player = (player_t*)p;
    const char *kmsg = (const char*)msg;
    sendKickPacket(p, kmsg, vanillaSock);

    destroyPlayer(p);
}

void destroyRoom(room_t *r, const char *optionalMsg)
{
    g_slist_foreach(r->players, (GFunc)kickPlayerFromRoom, 
                    (gpointer)optionalMsg);
    g_slist_free(r->players);

    free(r->name);
    free(r->password);
    free(r); 
}
