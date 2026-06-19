// BlipRenderer.h
#pragma once
#include <array>
#include <CSprite2d.h>
#include <CVector.h>
#include <CVector2D.h>
#include <CRGBA.h>
#include "Settings.h"

#ifndef SCREEN_COORD
#define SCREEN_COORD(x)          (x)
#define SCREEN_COORD_MAX_X       (RsGlobal.maximumWidth)
#define SCREEN_COORD_MAX_Y       (RsGlobal.maximumHeight)
#endif

#if defined(M_PI)
#undef M_PI
#endif
inline constexpr float M_PI = 3.14159265358979323846f;

class BlipRenderer {
public:
    enum class Id : int {
        CopLevel, CopHigher, CopLower, Heli, CopVehicle, KillTarget, CarTarget,
        COUNT
    };

    // ciclo de vida
    void Initialise(const Settings& cfg);  // carrega sprites da pasta do INI
    void Reload(const Settings& cfg);      // recarrega (cheat/ini)
    void Shutdown();                       // opcional

    // desenho de alto nível (mundo -> tela -> draw)
    // rotate=false usa draw retangular; rotate=true usa 4 vértices com rotação
    void DrawSprite(Id id, const CVector& world, float size,
        float angle, const CRGBA& col, bool rotate) const;

    // (opcional) acesso bruto ao sprite
    CSprite2d& Get(Id id) { return m_sprites[(int)id]; }
    const CSprite2d& Get(Id id) const { return m_sprites[(int)id]; }

    // utilitário exposto (p/ casos especiais)
    void WorldToBlipScreen(const CVector& world, CVector2D& screen) const;

private:
    std::array<CSprite2d, (int)Id::COUNT> m_sprites;

    // helpers internos
    static bool IsDrawingPauseMap();

    static void DrawSpriteAtScreen(CSprite2d& s, float x, float y, float w, float h, const CRGBA& col);
    static void DrawSpriteRotated(CSprite2d& s, float cx, float cy, float w, float h, float angle, const CRGBA& col);
};
