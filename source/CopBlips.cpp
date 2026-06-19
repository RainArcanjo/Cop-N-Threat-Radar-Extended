#include <plugin.h>
#include <CRadar.h>
#include <CPools.h>
#include <CPed.h>
#include <CTimer.h>
#include <CMenuManager.h>
#include <CGeneral.h>
#include <CWorld.h>
#include <eModelID.h>

#include "CopBlips.h"
#include "Settings.h"

using namespace plugin;

void CopBlips::Initialise(const Settings& cfg, const BlipRenderer& r) {
    m_cfg = &cfg;
    m_rdr = &r;
    m_aliveCopsOnVeh.reserve(16); // OPT: evita realocações
}

void CopBlips::Shutdown() {
    m_cfg = nullptr;
    m_rdr = nullptr;
    m_aliveCopsOnVeh.clear();
    m_aliveCopsOnVeh.rehash(0);
}

void CopBlips::OnSettingsChanged(const Settings& cfg) { m_cfg = &cfg; }

bool CopBlips::IsCopModel(int m) {
    switch (m) {
    case MODEL_LAPD1:
    case MODEL_SFPD1:
    case MODEL_LVPD1:
    case MODEL_CSHER:
    case MODEL_LAPDM1:
    case MODEL_SWAT:
    case MODEL_FBI:
    case MODEL_ARMY:
    case MODEL_DSHER:
        return true;
    default: return false;
    }
}

inline bool CopBlips::IsSpecialVehicle(const CVehicle* v) noexcept {
    if (!v) return false;
    const int mid = v->m_nModelIndex;
    return mid == MODEL_PREDATOR || mid == MODEL_POLMAV;
}

inline uint32_t ms_now() noexcept { return (uint32_t)CTimer::m_snTimeInMillisecondsPauseMode; }

inline CRGBA CopBlips::BlinkSeededJitter(const CRGBA& a, const CRGBA& b,
    int baseMs, int jitterMs, uint32_t seed) noexcept {
    int period = baseMs + (int)(seed % (2 * jitterMs + 1)) - jitterMs;

    if (period < 120) {
        period = 120;
    }

    const int t = (int)((ms_now() + (int)(seed % period)) % period);

    return (t < period / 2) ? a : b;
}

inline float CopBlips::SpinAngleRPM(float rpm, float phase) noexcept {
    const float tms = (float)ms_now();
    const float rad = (rpm * (2.0f * (float)M_PI) * tms) / 60000.0f + phase;
    // normaliza rapidamente sem chamar fmod (um pouco mais barato em 32 bits)
    return rad - floorf(rad / (2.0f * (float)M_PI)) * (2.0f * (float)M_PI);
}

inline CopBlips::HeightClass CopBlips::ClassifyHeight(const CVector& e, const CVector& p) noexcept {
    const float dz = e.z - p.z;
    return dz > 4.0f ? HeightClass::Higher : (dz < -2.0f ? HeightClass::Lower : HeightClass::Level);
}

inline bool CopBlips::IsCopPedAlive(const CPed* p) noexcept {
    return p && IsCopModel(p->m_nModelIndex) && p->m_fHealth > 0.0f &&
        p->m_ePedState != PEDSTATE_DIE && p->m_ePedState != PEDSTATE_DEAD;
}

// ---------------- draws -----------------

void CopBlips::DrawCopPeds() {
    if (!m_cfg || !m_rdr) return;

    m_aliveCopsOnVeh.clear();
    m_aliveCopsOnVeh.reserve(16);

    const CVector playerPos = FindPlayerCentreOfWorld_NoInteriorShift(0);

    // OPT: percorre pool 1x; conta cops vivos em veículos especiais e desenha cops a pé
    const int n = CPools::ms_pPedPool->m_nSize;
    for (int i = n; i; --i) {
        CPed* ped = CPools::ms_pPedPool->GetAt(i - 1);
        if (!IsCopPedAlive(ped) || ped->IsPlayer()) continue;

        if (ped->bInVehicle) {
            if (IsSpecialVehicle(ped->m_pVehicle)) {
                // conta quantos cops vivos estão neste veículo (para PREDATOR)
                m_aliveCopsOnVeh[ped->m_pVehicle] += 1;
            }
            continue; // não desenha ped a pé se está em veículo
        }

        // Escolha do sprite por altura relativa ao player
        const HeightClass hc = ClassifyHeight(ped->GetPosition(), playerPos);
        const BlipRenderer::Id spriteId =
            (hc == HeightClass::Higher) ? BlipRenderer::Id::CopHigher :
            (hc == HeightClass::Lower) ? BlipRenderer::Id::CopLower :
            BlipRenderer::Id::CopLevel;

        const CRGBA col = BlinkSeededJitter(m_cfg->m_CopBlinkColor1, m_cfg->m_CopBlinkColor2,
            m_cfg->m_nCopPedBlinkMs, m_cfg->m_nBlinkJitterMs,
            HashSeedFast(ped));

        // Desenha (sem rotação para pedestre)
        m_rdr->DrawSprite(
            spriteId,
            ped->GetPosition(),
            m_cfg->m_fCopPedSize,
            /*angle*/0.0f,
            col,
            /*rotate*/false
        );
    }
}

void CopBlips::DrawCopVehicles() {
    if (!m_cfg || !m_rdr) return;

    // OPT: Ângulo de rotação calculado 1x por frame e reaproveitado
    const float ang = SpinAngleRPM(180.0f) + (float)M_PI;

    const int n = CPools::ms_pVehiclePool->m_nSize;
    for (int i = n; i; --i) {
        CVehicle* veh = CPools::ms_pVehiclePool->GetAt(i - 1);
        if (!veh || veh->m_fHealth <= 0.0f) continue;
        if (veh == FindPlayerVehicle(-1, 0)) continue;

        const CRGBA col = BlinkSeededJitter(
            m_cfg->m_CopBlinkColor1, m_cfg->m_CopBlinkColor2,
            m_cfg->m_nCopVehBlinkMs, m_cfg->m_nBlinkJitterMs, HashSeedFast(veh)
        );

        // Helicóptero da polícia
        if (veh->m_nModelIndex == MODEL_POLMAV) {
            m_rdr->DrawSprite(
                BlipRenderer::Id::Heli,
                veh->GetPosition(),
                m_cfg->m_fCopVehicleSize,
                ang, col, /*rotate*/true
            );
            continue;
        }

        // PREDATOR: só se ainda houver policial vivo no barco (contado no DrawCopPeds)
        if (veh->m_nModelIndex == MODEL_PREDATOR) {
            const auto it = m_aliveCopsOnVeh.find(veh);
            const int alive = (it == m_aliveCopsOnVeh.end()) ? 0 : it->second;
            if (alive <= 0) continue;

            m_rdr->DrawSprite(
                BlipRenderer::Id::CopVehicle,
                veh->GetPosition(),
                m_cfg->m_fCopVehicleSize * 0.86f,
                ang, col, /*rotate*/true
            );
            continue;
        }

        // Viaturas comuns com agente a bordo
        if (veh->bCreatedAsPoliceVehicle && (veh->m_pDriver || veh->m_nNumPassengers > 0)) {
            m_rdr->DrawSprite(
                BlipRenderer::Id::CopVehicle,
                veh->GetPosition(),
                m_cfg->m_fCopVehicleSize,
                ang, col, /*rotate*/true
            );
        }
    }
}
