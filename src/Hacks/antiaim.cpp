#include "antiaim.h"

#include "aimbot.h"
#include "../settings.h"
#include "../Hooks/hooks.h"
#include "../Utils/math.h"
#include "../Utils/entity.h"
#include "../interfaces.h"

bool Settings::AntiAim::enabled = false;
AntiAimType Settings::AntiAim::type = AntiAimType::RAGE;
float Settings::AntiAim::yaw = 180.0f;

bool Settings::AntiAim::States::enabled = false;

float Settings::AntiAim::States::Stand::angle = 180.0f;
// float Settings::AntiAim::States::Walk::angle = 180.0f;
float Settings::AntiAim::States::Run::angle = 180.0f;
float Settings::AntiAim::States::Air::angle = 180.0f;

AntiAimType Settings::AntiAim::States::Stand::type = AntiAimType::RAGE;
// AntiAimType Settings::AntiAim::States::Walk::type = AntiAimType::RAGE;
AntiAimType Settings::AntiAim::States::Run::type = AntiAimType::RAGE;
AntiAimType Settings::AntiAim::States::Air::type = AntiAimType::RAGE;

ButtonCode_t Settings::AntiAim::left = ButtonCode_t::KEY_X;
ButtonCode_t Settings::AntiAim::right = ButtonCode_t::KEY_C;

bool Settings::AntiAim::AutoDisable::knifeHeld = false;

bool Settings::AntiAim::LBYBreaker::enabled = false;
bool Settings::AntiAim::LBYBreaker::custom = false;
float Settings::AntiAim::LBYBreaker::offset = 180.0f;

float AntiAim::GetMaxDelta(CCSGOAnimState *animState) {

    float speedFraction = std::max(0.0f, std::min(animState->feetShuffleSpeed, 1.0f));

    float speedFactor = std::max(0.0f, std::min(1.0f, animState->feetShuffleSpeed2));

    float unk1 = ((animState->runningAccelProgress * -0.30000001) - 0.19999999) * speedFraction;
    float unk2 = unk1 + 1.0f;
    float delta;

    if (animState->duckProgress > 0)
    {
        unk2 += ((animState->duckProgress * speedFactor) * (0.5f - unk2));// - 1.f
    }

    delta = *(float*)((uintptr_t)animState + 0x3A4) * unk2;

    return delta - 0.5f;
}

static void DoAntiAim(AntiAimType type, QAngle& angle, bool bSend, CCSGOAnimState* animState, bool directionSwitch, C_BasePlayer* localplayer, CUserCmd* cmd)
{
    if (Settings::AntiAim::States::enabled)
    {
        if (localplayer->GetVelocity().Length() <= 0.0f)
            type = Settings::AntiAim::States::Stand::type;
        else if (!(localplayer->GetFlags() & FL_ONGROUND))
            type = Settings::AntiAim::States::Air::type;
        /* Doesn't work
        else if (cmd->buttons & IN_WALK)
            type = Settings::AntiAim::States::Walk::type;
        */
        else
            type = Settings::AntiAim::States::Run::type;
    }
    else
        type = Settings::AntiAim::type;

	float maxDelta = AntiAim::GetMaxDelta(animState);

    switch (type)
    {
    case AntiAimType::RAGE: {
        static bool yFlip = false;

        angle.x = 89.0f;
        if (yFlip)
            angle.y += directionSwitch ? maxDelta / 2 : -maxDelta / 2;
        else
            angle.y += directionSwitch ? -maxDelta / 2 + 180.0f : maxDelta / 2 + 180.0f;

        if (!bSend)
        {
            if (yFlip)
                angle.y += directionSwitch ? -maxDelta : maxDelta;
            else
                angle.y += directionSwitch ? maxDelta : -maxDelta;
        }
        else
            yFlip = !yFlip;

    } break;
    
    case AntiAimType::LEGIT: {
        if (!bSend)
            angle.y += directionSwitch ? maxDelta : -maxDelta;
    } break;

    case AntiAimType::CUSTOM: {
        angle.x = 89.0f;
        
        if (Settings::AntiAim::States::enabled)
        {
            if (localplayer->GetVelocity().Length() <= 0.0f)
                angle.y += Settings::AntiAim::States::Stand::angle;
            else if (!(localplayer->GetFlags() & FL_ONGROUND))
                angle.y += Settings::AntiAim::States::Air::angle;
            /* Doesn't work
            else if (cmd->buttons & IN_WALK)
                angle.y += Settings::AntiAim::States::Walk::angle;
            */
            else
                angle.y += Settings::AntiAim::States::Run::angle;
        }
        else
            angle.y += Settings::AntiAim::yaw;

        if (!bSend)
            angle.y += directionSwitch ? maxDelta : -maxDelta;
    }
    
    default:
        break;
    }
}

void AntiAim::CreateMove(CUserCmd* cmd)
{
    if (!Settings::AntiAim::enabled)
        return;

    if (Settings::Aimbot::AimStep::enabled && Aimbot::aimStepInProgress)
        return;

    QAngle oldAngle = cmd->viewangles;
    float oldForward = cmd->forwardmove;
    float oldSideMove = cmd->sidemove;

    QAngle angle = cmd->viewangles;

    C_BasePlayer* localplayer = (C_BasePlayer*) entityList->GetClientEntity(engine->GetLocalPlayer());
    if (!localplayer || !localplayer->GetAlive())
        return;

    C_BaseCombatWeapon* activeWeapon = (C_BaseCombatWeapon*) entityList->GetClientEntityFromHandle(localplayer->GetActiveWeapon());
    if (!activeWeapon)
        return;

    if (activeWeapon->GetCSWpnData()->GetWeaponType() == CSWeaponType::WEAPONTYPE_GRENADE)
    {
        C_BaseCSGrenade* csGrenade = (C_BaseCSGrenade*) activeWeapon;

        if (csGrenade->GetThrowTime() > 0.f)
            return;
    }

    if (cmd->buttons & IN_USE || cmd->buttons & IN_ATTACK || (cmd->buttons & IN_ATTACK2 && *activeWeapon->GetItemDefinitionIndex() == ItemDefinitionIndex::WEAPON_REVOLVER))
        return;

    if (localplayer->GetMoveType() == MOVETYPE_LADDER || localplayer->GetMoveType() == MOVETYPE_NOCLIP)
        return;

    if (Settings::AntiAim::AutoDisable::knifeHeld && localplayer->GetAlive() && activeWeapon->GetCSWpnData()->GetWeaponType() == CSWeaponType::WEAPONTYPE_KNIFE)
        return;

    static bool bSend = true;
    bSend = !bSend;

    bool should_clamp = true;
    bool needToFlick = false;
    static bool lbyBreak = false;
    static float lastCheck;
    static float nextUpdate = FLT_MAX;
    float vel2D = localplayer->GetVelocity().Length2D();//localplayer->GetAnimState()->verticalVelocity + localplayer->GetAnimState()->horizontalVelocity;
    CCSGOAnimState* animState = localplayer->GetAnimState();

    if (Settings::AntiAim::LBYBreaker::enabled)
    {
        if (vel2D >= 0.1f || !(localplayer->GetFlags() & FL_ONGROUND) || localplayer->GetFlags() & FL_FROZEN)
        {
            lbyBreak = false;
            lastCheck = globalVars->curtime;
            nextUpdate = globalVars->curtime + 0.22;
        }
        else if (!lbyBreak && (globalVars->curtime - lastCheck) > 0.22 || lbyBreak && (globalVars->curtime - lastCheck) > 1.1)
        {
            lbyBreak = true;
            lastCheck = globalVars->curtime;
            nextUpdate = globalVars->curtime + 1.1;
            needToFlick = true;
        }
    }

    if ((nextUpdate - globalVars->interval_per_tick) >= globalVars->curtime && nextUpdate <= globalVars->curtime)
        CreateMove::sendPacket = false;

    static bool directionSwitch = false;

    if (inputSystem->IsButtonDown(Settings::AntiAim::left))
		directionSwitch = true;
	else if (inputSystem->IsButtonDown(Settings::AntiAim::right))
		directionSwitch = false;

    static AntiAimType type;

    if (needToFlick)
    {
        CreateMove::sendPacket = false;
        
        if (!Settings::AntiAim::LBYBreaker::custom)
            angle.y += directionSwitch ? Settings::AntiAim::LBYBreaker::offset : -Settings::AntiAim::LBYBreaker::offset;
        else
        {
            static float maxDelta = AntiAim::GetMaxDelta(animState);

            if (type != AntiAimType::RAGE)
                angle.y += directionSwitch ? -maxDelta : maxDelta;
            else
                angle.y += directionSwitch ? -90.0f : 90.0f;
        }
        
    }
    else
    {
    	DoAntiAim(type, angle, bSend, animState, directionSwitch, localplayer, cmd);
    }

    if (should_clamp)
    {
        Math::NormalizeAngles(angle);
        Math::ClampAngles(angle);
    }

    if (!needToFlick)
        CreateMove::sendPacket = bSend;

    cmd->viewangles = angle;

    Math::CorrectMovement(oldAngle, cmd, oldForward, oldSideMove);
}
