#include "lagcomp.h"

#include "../Utils/math.h"
#include "../settings.h"
#include "../interfaces.h"
#include <algorithm>

bool Settings::LagComp::enabled = false;

std::vector<LagComp::BacktrackTick> LagComp::ticks;

float GetLerpTime()
{
    int ud_rate = cvar->FindVar("cl_updaterate")->GetInt();
    ConVar *min_ud_rate = cvar->FindVar("sv_minupdaterate");
    ConVar *max_ud_rate = cvar->FindVar("sv_maxupdaterate");

    if (min_ud_rate && max_ud_rate)
        ud_rate = max_ud_rate->GetInt();

    float ratio = cvar->FindVar("cl_interp_ratio")->GetFloat();

    if (ratio == 0)
        ratio = 1.0f;

    float lerp = cvar->FindVar("cl_interp")->GetFloat();
    ConVar *c_min_ratio = cvar->FindVar("sv_client_min_interp_ratio");
    ConVar *c_max_ratio = cvar->FindVar("sv_client_max_interp_ratio");

    if (c_min_ratio && c_max_ratio && c_min_ratio->GetFloat() != 1)
        ratio = std::clamp(ratio, c_min_ratio->GetFloat(), c_max_ratio->GetFloat());

    return std::max(lerp, (ratio / ud_rate));
}

bool IsTickValid( float time ) // polak's invalid tick remover
{
    float correct = 0;

    // static INetChannelInfo* netInfo;

    // correct += netInfo->GetLatency(FLOW_OUTGOING);
    // correct += netInfo->GetLatency(FLOW_INCOMING);

    correct += GetLerpTime();
    correct = std::clamp( correct, 0.f, cvar->FindVar("sv_maxunlag")->GetFloat());
 
    float deltaTime = correct - ( globalVars->curtime - time );
 
    return fabsf(deltaTime) < 0.2f;
}

void RemoveBadRecords(std::vector<LagComp::BacktrackTick>& records)
{
    auto& m_LagRecords = records; // Should use rbegin but can't erase
    for (auto lag_record = m_LagRecords.begin(); lag_record != m_LagRecords.end(); lag_record++)
    {
        if (!IsTickValid(lag_record->SimulationTime))
        {
            m_LagRecords.erase(lag_record);
            if (!m_LagRecords.empty())
                lag_record = m_LagRecords.begin();
            else break;
        }
    }
}

void LagComp::FrameStageNotify(ClientFrameStage_t stage)
{
    if (!Settings::LagComp::enabled)
        return;

    if (stage == ClientFrameStage_t::FRAME_NET_UPDATE_POSTDATAUPDATE_START)
    {
        LagComp::ticks.insert(LagComp::ticks.begin(), {globalVars->tickcount, globalVars->curtime});
        auto& cur = LagComp::ticks[0];
    
        RemoveBadRecords(LagComp::ticks);

        for (int i = 1; i < engine->GetMaxClients(); ++i)
        {
            C_BasePlayer* entity = (C_BasePlayer*) entityList->GetClientEntity(i);
            C_BasePlayer* localplayer = (C_BasePlayer*) entityList->GetClientEntity(engine->GetLocalPlayer());

            if (!entity ||
                entity == localplayer ||
                entity->GetDormant() ||
                !entity->GetAlive() ||
                Entity::IsTeamMate(entity, localplayer) ||
                entity->GetImmune())
                continue;

            LagComp::BacktrackRecord record = {entity, entity->GetBonePosition(BONE_HEAD), entity->GetVecOrigin()};

            // *(int*)((uintptr_t)record.entity + 0xA30) = globalVars->framecount; getting some weird stretch of model pls send halp
            // *(int*)((uintptr_t)record.entity + 0xA28) = 0;

            if(entity->SetupBones(record.boneMatrix, 128, 0x0007FF00, globalVars->curtime))
                cur.records.emplace_back(record);
        }
    }
}

/* This will be moved to aimbot.cpp
void LagComp::CreateMove(CUserCmd* cmd)
{
    if (!Settings::LagComp::enabled)
        return;

    C_BasePlayer* localplayer = (C_BasePlayer*) entityList->GetClientEntity(engine->GetLocalPlayer());

    if (!localplayer || !localplayer->GetAlive())
        return;

    float serverTime = localplayer->GetTickBase() * globalVars->interval_per_tick;
    C_BaseCombatWeapon* weapon = (C_BaseCombatWeapon*) entityList->GetClientEntityFromHandle(localplayer->GetActiveWeapon());

    QAngle myAngle;
    engine->GetViewAngles(myAngle);
    QAngle myAngle_rcs = myAngle + *localplayer->GetAimPunchAngle();

    if (cmd->buttons & IN_ATTACK && weapon->GetNextPrimaryAttack() <= serverTime)
    {
        float fov = 7.f;
        int tickcount = 0;
        bool hasTarget = false;

        for (auto& tick : LagComp::ticks)
        {
            for (auto& record : tick.records)
            {

                float tmpFOV = Math::GetFov(myAngle_rcs, Math::CalcAngle(localplayer->GetEyePosition(), record.head));

                if (tmpFOV < fov)
                {
                    fov = tmpFOV;
                    tickcount = tick.tickcount;
                    hasTarget = true;
                }
            }
        }

        if (hasTarget)
        {
            cmd->tick_count = tickcount;
        }
    }
}
*/