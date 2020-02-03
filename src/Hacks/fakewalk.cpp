#include "fakewalk.h"

#include "../settings.h"
#include "../interfaces.h"
#include "../Hooks/hooks.h"

bool Settings::FakeWalk::enabled = false;
ButtonCode_t Settings::FakeWalk::key = ButtonCode_t::KEY_X;

void FakeWalk::CreateMove(CUserCmd* cmd)
{
    C_BasePlayer* localplayer = (C_BasePlayer*) entityList->GetClientEntity(engine->GetLocalPlayer());
	if (!localplayer || !localplayer->GetAlive())
		return;

    static int choked = 0;

    if (!Settings::FakeWalk::enabled)
        return;

    if (!inputSystem->IsButtonDown(Settings::FakeWalk::key))
        return;

    choked = choked > 3 ? 0 : choked + 1;
    cmd->forwardmove = choked < 1 || choked > 2 ? 0 : cmd->forwardmove;
    cmd->sidemove = choked < 1 || choked > 2 ? 0 : cmd->sidemove;
    CreateMove::sendPacket2 = choked < 1;
}