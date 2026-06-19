#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <CRGBA.h>
#include <CPed.h>
#include <CVehicle.h>
#include <CTask.h>
#include <CPlayerPed.h>
#include <CEntity.h>
#include <CTaskComplex.h>
#include <CTaskComplexEnterCar.h>

#include "Settings.h"
#include "BlipRenderer.h"

class NonCopPeds {
public:
    NonCopPeds();
    void Initialise(const Settings& cfg, const BlipRenderer& renderer);
    void OnSettingsChanged(const Settings& cfg);
    void Shutdown();

    // ciclo de jogo
    void Update(); // coleta contexto (carjacks próximos, limpeza etc.)
    void Draw();   // desenha os blips de ameaça

private:
    // cfg + renderer
    const Settings* m_cfg = nullptr; // não possuímos
    const BlipRenderer* m_rdr = nullptr; // não possuímos

    // estado de ameaça
    struct ThreatInfo {
        bool  isThreat = false;
        bool  wasAttacking = false;
        bool  isVehicleThreat = false;
        bool  hasBeenDraggedOutCar = false;
        bool  isTryingToCarjack = false;
        float lastDistance = 999.0f;
    };

    // OPT: cache de peds visíveis (preenchido em Update e reutilizado)
    std::vector<CPed*> m_visiblePeds;

    // mapa de ameaça
    std::unordered_map<CPed*, ThreatInfo> m_threatMap;

    // OPT: refs de vítimas rastreadas com estrutura O(1) (evita vector::find)
    std::unordered_set<int> m_trackedRefs;

    // transientes para detecção de shuffle/enter
    bool      gPrevShuffle = false;
    CVehicle* gPrevEnterVeh = nullptr;

    // ---------- helpers ----------
    static int   PedToRef(CPed* p);
    static CPed* RefToPed(int ref);

    void CleanupVictims();
    void TrackVictim(CPed* ped);
    bool IsVictimTracked(CPed* ped) const;

    // leitura de tasks
    CTask* FindInChain(CTask* t, int type);
    CTask* FindTaskDeepByType(CPed* ped, int type);

    // carjack/arrasto
    void CaptureDraggedVictimsNearPlayer(CPlayerPed* player, float radius);
    void CaptureBikeJackedVictimsNearPlayer(CPlayerPed* player, float radius);
    void CaptureCarjackVictims(CPlayerPed* player);

    // avaliação
    bool     IsVehicleHitByPlayer(CPed* ped);
    CEntity* GetPedKillTargetFromTasks(CPed* ped);

    bool IsPedFleeing(CPed* ped);
    bool IsBikeJackPairNow(CPed* victim, CPlayerPed* player);
    bool IsPassengerSideEntryAsDriver(const CTaskComplexEnterCar* enter);
    CTaskComplexEnterCar* GetEnterCarTask(CPed* ped);
    bool IsCarDragPairNow(CPed* victim, CPlayerPed* player);
    CVehicle* GetVehicleGoingToEnter(CPlayerPed* player);
    CPed* GetDraggedOutPed(CPlayerPed* player);
};
