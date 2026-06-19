#pragma once
#include <CSprite2d.h>
#include <CVector.h>
#include <CVector2D.h>
#include <CRGBA.h>
#include <unordered_map>
#include <CPed.h>
#include <CVehicle.h>
#include "Settings.h"
#include "BlipRenderer.h"

using namespace plugin;

class CopBlips {
public:
    enum class SpriteId { CopLevel, CopHigher, CopLower, Heli, CopVehicle, KillTarget, CarTarget };

    void Initialise(const Settings& cfg, const BlipRenderer& blipRenderer);
    void Shutdown();
    void OnSettingsChanged(const Settings& cfg);

    void DrawCopPeds();       // peds policiais (a pé)
    void DrawCopVehicles();   // heli/barco/viaturas

    // consultas
    static bool IsCopModel(int model);

private:
    // ----- estado -----
    const Settings* m_cfg = nullptr;      // snapshot (não possuímos)
    const BlipRenderer* m_rdr = nullptr;    // renderer (não possuímos)

    // OPT: mapa de vivos em veículos especiais (reduzido por frame)
    std::unordered_map<CVehicle*, int> m_aliveCopsOnVeh;

    // --------- helpers ---------
    // hash barato (suficiente p/ pisca assíncrono)
    static inline uint32_t HashSeedFast(const void* p) noexcept {
        return (uint32_t)((uintptr_t)p * 2654435761u);
    }

    static inline CRGBA BlinkSeededJitter(const CRGBA& a, const CRGBA& b,
        int baseMs, int jitterMs, uint32_t seed) noexcept;

    static inline float SpinAngleRPM(float rpm, float phase = 0.0f) noexcept;

    enum class HeightClass : uint8_t { Lower, Level, Higher };
    static inline HeightClass ClassifyHeight(const CVector& e, const CVector& p) noexcept;

    static inline bool  IsSpecialVehicle(const CVehicle* v) noexcept;
    static inline bool  IsCopPedAlive(const CPed* p) noexcept;
};
