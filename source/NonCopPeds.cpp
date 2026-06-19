#include "NonCopPeds.h"
#include "Settings.h"
#include "BlipRenderer.h"
#include "CopBlips.h"

#include <plugin.h>
#include <CPools.h>
#include <CPed.h>
#include <CVehicle.h>
#include <CPlayerPed.h>
#include <CRadar.h>
#include <CWorld.h>
#include <CTimer.h>
#include <CGeneral.h>
#include <CMenuManager.h>
#include <eTaskType.h>
#include <eModelID.h>
#include <eEventType.h>

using namespace plugin;

constexpr int PRIMARY_SLOTS = 5;
constexpr int SECONDARY_SLOTS = 6;

NonCopPeds::NonCopPeds() = default;

void NonCopPeds::Initialise(const Settings& cfg, const BlipRenderer& r) {
    m_cfg = &cfg;
    m_rdr = &r;
    m_visiblePeds.reserve(128);
    m_trackedRefs.reserve(64);
    m_threatMap.reserve(256);
}
void NonCopPeds::OnSettingsChanged(const Settings& cfg) { m_cfg = &cfg; }

void NonCopPeds::Shutdown() {
    m_threatMap.clear();
    m_trackedRefs.clear();
    m_visiblePeds.clear();
    m_cfg = nullptr;
    m_rdr = nullptr;
}

// ---- pool/refs ----
int NonCopPeds::PedToRef(CPed* ped) { return ped ? CPools::GetPedRef(ped) : -1; }
CPed* NonCopPeds::RefToPed(int ref) { return ref != -1 ? CPools::GetPed(ref) : nullptr; }

void NonCopPeds::CleanupVictims() {
    // remove refs que apontam para nullptr
    for (auto it = m_trackedRefs.begin(); it != m_trackedRefs.end();) {
        if (!RefToPed(*it)) it = m_trackedRefs.erase(it);
        else ++it;
    }
}

void NonCopPeds::TrackVictim(CPed* ped) {
    if (!ped || ped->IsPlayer() || !ped->IsAlive()) return;
    const int ref = PedToRef(ped);
    if (ref != -1) m_trackedRefs.insert(ref);
}

bool NonCopPeds::IsVictimTracked(CPed* ped) const {
    if (!ped) return false;
    const int ref = const_cast<NonCopPeds*>(this)->PedToRef(ped);
    if (ref == -1) return false;
    return m_trackedRefs.find(ref) != m_trackedRefs.end();
}

// ---- leitura de tasks ----
CTask* NonCopPeds::FindInChain(CTask* t, int type) {
    while (t) {
        if (t->GetId() == type) return t;
        t = t->GetSubTask();
    }
    return nullptr;
}

CTask* NonCopPeds::FindTaskDeepByType(CPed* ped, int type) {
    if (!ped || !ped->m_pIntelligence) return nullptr;
    auto& tm = ped->m_pIntelligence->m_TaskMgr;

    // OPT: busca nos encadeamentos ativos (uma chamada por slot seria ideal,
    // mas aqui mantemos abordagem segura)
    if (CTask* t = FindInChain(tm.GetActiveTask(), type)) return t;

    for (int i = 0; i < SECONDARY_SLOTS; ++i)
        if (CTask* t = FindInChain(tm.GetTaskSecondary(i), type)) return t;

    return nullptr;
}

// ler alvo da task por offset simples
static CEntity* ReadTaskTargetByOffset(CTask* t, int off) {
    if (!t || !off) return nullptr;
    auto** slot = reinterpret_cast<CEntity**>((uintptr_t)t + off);
    CEntity* e = *slot;
    if (!e || e == (CEntity*)-1) return nullptr;
    if (e->m_nType < ENTITY_TYPE_NOTHING || e->m_nType > ENTITY_TYPE_OBJECT) return nullptr;
    return e;
}

CEntity* NonCopPeds::GetPedKillTargetFromTasks(CPed* ped) {
    if (!ped) return nullptr;

    const int types[] = {
        TASK_COMPLEX_KILL_PED_ON_FOOT,
        TASK_COMPLEX_ARREST_PED,
        TASK_SIMPLE_ARREST_PED,
        TASK_SIMPLE_KILL_PED_WITH_CAR,
        TASK_SIMPLE_HURT_PED_WITH_CAR,
        TASK_KILL_PED_FROM_BOAT,
        TASK_COMPLEX_KILL_CRIMINAL,
        TASK_COMPLEX_HIT_PED_WITH_CAR,
    };

    for (int ttype : types) {
        if (CTask* t = FindTaskDeepByType(ped, ttype)) {
            int off = 0;
            switch (ttype) {
            case TASK_COMPLEX_ARREST_PED:           off = 4;  break;
            case TASK_SIMPLE_ARREST_PED:            off = 8;  break;
            case TASK_COMPLEX_KILL_PED_ON_FOOT:     off = 16; break;
            case TASK_SIMPLE_KILL_PED_WITH_CAR:     off = 8;  break;
            case TASK_SIMPLE_HURT_PED_WITH_CAR:     off = 8;  break;
            case TASK_KILL_PED_FROM_BOAT:           off = 12; break;
            case TASK_COMPLEX_KILL_CRIMINAL:        off = 12; break;
            case TASK_COMPLEX_HIT_PED_WITH_CAR:     off = 12; break;
            }
            if (CEntity* e = ReadTaskTargetByOffset(t, off)) return e;
        }
    }
    return nullptr;
}

// ---- avaliação de contexto ----
bool NonCopPeds::IsVehicleHitByPlayer(CPed* ped) {
    if (!ped || !ped->m_pVehicle) return false;
    CEntity* damageSource = ped->m_pVehicle->m_pLastDamageEntity;
    return damageSource && damageSource == FindPlayerVehicle();
}

bool NonCopPeds::IsPedFleeing(CPed* ped) {
    if (!ped) return false;

    const int flee[] = {
        TASK_COMPLEX_SMART_FLEE_POINT,
        TASK_COMPLEX_SMART_FLEE_ENTITY,
        TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE,
        TASK_COMPLEX_FLEE_ENTITY,
        TASK_COMPLEX_FLEE_POINT,
        TASK_COMPLEX_FLEE_ANY_MEANS,
        TASK_COMPLEX_LEAVE_CAR_AND_FLEE,
        TASK_SMART_FLEE_ENTITY_WALKING,
        TASK_GROUP_FLEE_THREAT,
        TASK_SIMPLE_HANDS_UP,
    };

    for (int t : flee)
        if (FindTaskDeepByType(ped, t)) return true;

    const CVector pedPosition = ped->GetPosition();
    const CVector playerPosition = FindPlayerPed()->GetPosition();
    const float dx = pedPosition.x - playerPosition.x;
    const float dy = pedPosition.y - playerPosition.y;
    return (dx * dx + dy * dy) > (50.0f * 50.0f);
}

bool NonCopPeds::IsCarDragPairNow(CPed* victim, CPlayerPed* player) {
    if (!victim || !player) return false;
    if (!FindTaskDeepByType(victim, TASK_SIMPLE_CAR_SLOW_BE_DRAGGED_OUT)) return false;

    return FindTaskDeepByType(player, TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT) ||
        FindTaskDeepByType(player, TASK_SIMPLE_CAR_GET_IN) ||
        FindTaskDeepByType(player, TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE) ||
        FindTaskDeepByType(player, TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE) ||
        FindTaskDeepByType(player, TASK_COMPLEX_STEAL_CAR);
}

CTaskComplexEnterCar* NonCopPeds::GetEnterCarTask(CPed* ped) {
    if (!ped || !ped->m_pIntelligence) return nullptr;
    if (CTask* t = FindTaskDeepByType(ped, TASK_COMPLEX_ENTER_CAR_AS_DRIVER))
        return reinterpret_cast<CTaskComplexEnterCar*>(t);
    if (CTask* t = FindTaskDeepByType(ped, TASK_COMPLEX_ENTER_CAR_AS_PASSENGER))
        return reinterpret_cast<CTaskComplexEnterCar*>(t);
    return nullptr;
}

CVehicle* NonCopPeds::GetVehicleGoingToEnter(CPlayerPed* player) {
    if (auto* enter = GetEnterCarTask(player)) return enter->m_pTargetVehicle;
    return nullptr;
}

CPed* NonCopPeds::GetDraggedOutPed(CPlayerPed* player) {
    if (auto* enter = GetEnterCarTask(player)) return enter->m_pDraggedPed;
    return nullptr;
}

bool NonCopPeds::IsPassengerSideEntryAsDriver(const CTaskComplexEnterCar* enter) {
    if (!enter || !enter->m_bAsDriver) return false;
    return enter->m_nTargetDoor != TARGET_DOOR_FRONT_LEFT;
}

// ---- contexto carjack/arrasto ----
// OPT: iteramos sobre cache m_visiblePeds em vez do pool
void NonCopPeds::CaptureDraggedVictimsNearPlayer(CPlayerPed* player, float radius) {
    if (!player) return;
    const CVector playerPosition = player->GetPosition();
    const float r2 = radius * radius;

    for (CPed* ped : m_visiblePeds) {
        if (!ped || ped->IsPlayer() || !ped->IsAlive()) continue;
        if (IsCarDragPairNow(ped, player)) {
            const CVector pos = ped->GetPosition();
            const float dx = pos.x - playerPosition.x, dy = pos.y - playerPosition.y;
            if (dx * dx + dy * dy <= r2) TrackVictim(ped);
        }
    }
}

bool NonCopPeds::IsBikeJackPairNow(CPed* victim, CPlayerPed* player) {
    if (!victim || !player) return false;
    if (!FindTaskDeepByType(victim, TASK_SIMPLE_BIKE_JACKED)) return false;

    return FindTaskDeepByType(player, TASK_SIMPLE_CAR_GET_IN) ||
        FindTaskDeepByType(player, TASK_COMPLEX_STEAL_CAR) ||
        FindTaskDeepByType(player, TASK_COMPLEX_ENTER_CAR_AS_DRIVER) ||
        FindTaskDeepByType(player, TASK_COMPLEX_ENTER_CAR_AS_PASSENGER);
}

void NonCopPeds::CaptureBikeJackedVictimsNearPlayer(CPlayerPed* player, float radius) {
    if (!player) return;
    const CVector playerPosition = player->GetPosition();
    const float r2 = radius * radius;

    for (CPed* ped : m_visiblePeds) {
        if (!ped || ped->IsPlayer() || !ped->IsAlive()) continue;
        if (IsBikeJackPairNow(ped, player)) {
            const CVector pos = ped->GetPosition();
            const float dx = pos.x - playerPosition.x, dy = pos.y - playerPosition.y;
            if (dx * dx + dy * dy <= r2) TrackVictim(ped);
        }
    }
}

void NonCopPeds::CaptureCarjackVictims(CPlayerPed* player) {
    auto* enter = GetEnterCarTask(player);
    CVehicle* targetVeh = enter ? enter->m_pTargetVehicle : nullptr;
    const bool hasEnter = (enter != nullptr);

    const bool shuffleNow = hasEnter && FindTaskDeepByType(player, TASK_SIMPLE_CAR_SHUFFLE);
    const bool shuffleEdge = hasEnter && shuffleNow && !gPrevShuffle;
    const bool newTarget = hasEnter && targetVeh && (targetVeh != gPrevEnterVeh);
    const bool passSideAsDriver = hasEnter && IsPassengerSideEntryAsDriver(enter);

    if (newTarget || shuffleEdge || passSideAsDriver) {
        if (CPed* dragged = enter ? enter->m_pDraggedPed : nullptr) TrackVictim(dragged);

        if (targetVeh) {
            if (targetVeh->m_pDriver && !targetVeh->m_pDriver->IsPlayer() && targetVeh->m_pDriver->IsAlive())
                TrackVictim(targetVeh->m_pDriver);

            if (targetVeh->m_apPassengers) {
                for (int i = 0; i < 8; ++i) {
                    if (CPed* pass = targetVeh->m_apPassengers[i]) {
                        if (!pass->IsPlayer() && pass->IsAlive()) TrackVictim(pass);
                    }
                }
            }
        }
    }

    gPrevShuffle = hasEnter ? shuffleNow : false;
    gPrevEnterVeh = hasEnter ? targetVeh : nullptr;
}

// ================= frame hooks =================
void NonCopPeds::Update() {
    CPlayerPed* player = FindPlayerPed();
    if (!m_cfg || !m_cfg->m_bEnabled || !player) return;

    // OPT: captura uma vez os peds relevantes do frame
    m_visiblePeds.clear();
    m_visiblePeds.reserve(CPools::ms_pPedPool->m_nSize);

    for (int i = CPools::ms_pPedPool->m_nSize; i; --i) {
        CPed* p = CPools::ms_pPedPool->GetAt(i - 1);
        if (!p || !p->IsAlive() || p->IsPlayer()) continue;
        if (CopBlips::IsCopModel(p->m_nModelIndex)) continue; // só não-policiais aqui
        m_visiblePeds.push_back(p);
    }

    // Capturas que antes percorriam o pool agora usam m_visiblePeds
    CaptureDraggedVictimsNearPlayer(player, 14.0f);
    CaptureBikeJackedVictimsNearPlayer(player, 14.0f);
    CaptureCarjackVictims(player);
    CleanupVictims();
}

void NonCopPeds::Draw() {
    if (!m_cfg || !m_rdr) return;
    CPlayerPed* player = FindPlayerPed();
    if (!player) return;

    // OPT: limpeza de ameaça mais barata (sem vetor temporário)
    for (auto it = m_threatMap.begin(); it != m_threatMap.end();) {
        CPed* ped = it->first;
        bool alive = false;

        // verificação leve se o ponteiro ainda é válido
        if (ped && ped->IsAlive()) {
            // confirma existência no pool atual
            for (int i = CPools::ms_pPedPool->m_nSize; i; --i) {
                if (CPed* p = CPools::ms_pPedPool->GetAt(i - 1); p == ped) { alive = true; break; }
            }
        }

        if (!alive) it = m_threatMap.erase(it);
        else ++it;
    }

    // OPT: iterar cache de não-cops
    for (CPed* ped : m_visiblePeds) {
        ThreatInfo& th = m_threatMap[ped];
        bool hostile = false;

        if (CEntity* tgt = GetPedKillTargetFromTasks(ped)) {
            if (tgt == (CEntity*)player) {
                th.isThreat = true;
                th.wasAttacking = true;
                hostile = true;
            }
        }

        if (IsVehicleHitByPlayer(ped) && ped->m_pIntelligence->IsRespondingToEvent(EVENT_VEHICLE_DAMAGE_COLLISION) && !IsPedFleeing(ped)) {
            th.isThreat = true;
            th.isVehicleThreat = true;
        }

        if (IsVictimTracked(ped)) {
            th.hasBeenDraggedOutCar = true;
        }

        if (ped->m_pIntelligence->IsRespondingToEvent(EVENT_DRAGGED_OUT_CAR) && !IsPedFleeing(ped) && th.hasBeenDraggedOutCar && ped->m_pIntelligence->IsPedGoingForCarDoor()) {
            th.isThreat = true;
            th.isTryingToCarjack = true;
        }

        if (th.isThreat) {
            // decide sprite/cor
            BlipRenderer::Id sid = BlipRenderer::Id::KillTarget;
            CRGBA col = m_cfg->m_KillTargetColor;
            float size = m_cfg->m_fKillTargetSize;

            if (th.isVehicleThreat || th.isTryingToCarjack) {
                sid = BlipRenderer::Id::CarTarget;
                col = m_cfg->m_CarTargetColor;
                size = m_cfg->m_fCarTargetSize;
            }

            m_rdr->DrawSprite(sid, ped->GetPosition(), size, 0.0f, col, /*rotate*/false);

            // desligamento
            const bool tooFarOrFlee = (th.lastDistance > 50.0f) || IsPedFleeing(ped);
            const bool carjackEnded = th.isTryingToCarjack &&
                !ped->m_pIntelligence->IsRespondingToEvent(EVENT_DRAGGED_OUT_CAR);
            const bool vehThreatEnded = th.isVehicleThreat &&
                !ped->m_pIntelligence->IsRespondingToEvent(EVENT_VEHICLE_DAMAGE_COLLISION);

            if (tooFarOrFlee || carjackEnded || vehThreatEnded) {
                th = ThreatInfo{};
            }

            th.lastDistance = (ped->GetPosition() - player->GetPosition()).Magnitude();
        }
    }
}
