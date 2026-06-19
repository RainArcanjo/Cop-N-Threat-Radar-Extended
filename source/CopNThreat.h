#pragma once
#include "BlipRenderer.h"
#include "ThreatManager.h"
#include "CopBlips.h"
#include <memory>

class CopNThreat {
private:
    std::unique_ptr<BlipRenderer> m_pRenderer;
    std::unique_ptr<ThreatManager> m_pThreatManager;
    std::unique_ptr<CopBlips> m_pCopBlips;

    static void ReloadSettingsCheat();

public:
    CopNThreat();
    void Init();
    void Shutdown();
    void OnDrawBlips();
};