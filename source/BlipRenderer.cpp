#include <plugin.h>
#include <CRadar.h>
#include <CMenuManager.h>
#include <SpriteLoader.h>

#include "BlipRenderer.h"

using namespace plugin;

// usamos o loader global que você já tem no projeto
SpriteLoader spriteLoader = {};

namespace {
    static const char* const kSpriteNames[(int)BlipRenderer::Id::COUNT] = {
        "radar_cop_level",
        "radar_cop_higher",
        "radar_cop_lower",
        "radar_cop_heli",
        "radar_cop_vehicle",
        "radar_kill_target",
        "radar_car_target"
    };
}


bool BlipRenderer::IsDrawingPauseMap() {
    return FrontEndMenuManager.m_bDrawRadarOrMap != 0;
}

// carrega todos os sprites declarados no enum
static void LoadAllInto(BlipRenderer& r, const Settings& cfg) {
    spriteLoader.LoadAllSpritesFromFolder(PLUGIN_PATH("CopNThreat\\sprites"));

    for (int i = 0; i < (int)BlipRenderer::Id::COUNT; ++i) {
        r.Get((BlipRenderer::Id)i) = spriteLoader.GetSprite(kSpriteNames[i]);
    }
}

void BlipRenderer::Initialise(const Settings& cfg) { LoadAllInto(*this, cfg); }
void BlipRenderer::Reload(const Settings& cfg) {
    // guarda os atuais para fallback, caso falte algo no reload
    auto old = m_sprites;

    // se o seu SpriteLoader tiver .Clear(), pode usar
     spriteLoader.Clear();

    // recarrega a pasta definida no INI
    spriteLoader.LoadAllSpritesFromFolder(PLUGIN_PATH("CopNThreat\\sprites"));

    // rebind dos sprites
    for (int i = 0; i < (int)Id::COUNT; ++i) {
        CSprite2d spr = spriteLoader.GetSprite(kSpriteNames[i]);
        // se carregou ok, substitui; senão mantém o antigo
        if (spr.m_pTexture) {
            m_sprites[i] = spr;
        }
        else {
            m_sprites[i] = old[i];
        }
    }
}
void BlipRenderer::Shutdown() { spriteLoader.Clear(); }

void BlipRenderer::WorldToBlipScreen(const CVector& world, CVector2D& screen) const {
    CVector2D rs;
    CRadar::TransformRealWorldPointToRadarSpace(rs, { world.x, world.y });

    if (!IsDrawingPauseMap()) {
        CRadar::LimitRadarPoint(rs);
        CRadar::TransformRadarPointToScreenSpace(screen, rs);
        return;
    }
    // caminho do pause map (igual ao sample do GPS/Collectibles)
    CRadar::LimitRadarPoint(rs);
    CRadar::TransformRadarPointToScreenSpace(screen, rs);
    screen.x *= (float)RsGlobal.maximumWidth / 640.0f;
    screen.y *= (float)RsGlobal.maximumHeight / 448.0f;
    CRadar::LimitToMap(&screen.x, &screen.y);
}

// --- low-level draws ---
void BlipRenderer::DrawSpriteAtScreen(CSprite2d& s, float x, float y, float w, float h, const CRGBA& col) {
    if (!s.m_pTexture) return;
    const CRect r(x - w * 0.5f, y - h * 0.5f, x + w * 0.5f, y + h * 0.5f);
    s.Draw(r, col);
}

void BlipRenderer::DrawSpriteRotated(CSprite2d& s, float cx, float cy, float w, float h, float angle, const CRGBA& col) {
    if (!s.m_pTexture) return;

    void* oldCull = nullptr; RwRenderStateGet(rwRENDERSTATECULLMODE, &oldCull);
    void* oldVA = nullptr; RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &oldVA);
    RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);

    const float hw = w * 0.5f, hh = h * 0.5f, c = cosf(angle), sN = sinf(angle);
    CVector2D tl{ cx + (-hw) * c - (-hh) * sN, cy + (-hw) * sN + (-hh) * c };
    CVector2D tr{ cx + (+hw) * c - (-hh) * sN, cy + (+hw) * sN + (-hh) * c };
    CVector2D bl{ cx + (-hw) * c - (+hh) * sN, cy + (-hw) * sN + (+hh) * c };
    CVector2D br{ cx + (+hw) * c - (+hh) * sN, cy + (+hw) * sN + (+hh) * c };
    s.Draw(tl.x, tl.y, tr.x, tr.y, bl.x, bl.y, br.x, br.y, col);

    RwRenderStateSet(rwRENDERSTATECULLMODE, oldCull);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, oldVA);
}

// --- alto nível ---
void BlipRenderer::DrawSprite(Id id, const CVector& world, float size,
    float angle, const CRGBA& col, bool rotate) const
{
    const CSprite2d& s = m_sprites[(int)id];
    if (!s.m_pTexture) return;

    CVector2D scr; WorldToBlipScreen(world, scr);
    const float w = SCREEN_COORD(size), h = SCREEN_COORD(size);

    if (!rotate) DrawSpriteAtScreen(const_cast<CSprite2d&>(s), scr.x, scr.y, w, h, col);
    else         DrawSpriteRotated(const_cast<CSprite2d&>(s), scr.x, scr.y, w, h, angle, col);
}
