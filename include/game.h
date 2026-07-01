#ifndef GAME_H
#define GAME_H

#include "common.h"

/** @brief Game-code per-frame VSync handler (dispatched for RENDER_GAME). */
void vsyncGameHandler(void);

/** @brief Main game state-machine loop, driven by g_vsyncRate. */
void gameStateLoop(void);

#endif /* GAME_H */
