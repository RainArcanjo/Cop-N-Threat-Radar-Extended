#pragma once
#include <plugin.h>
#include <CRGBA.h>

class Settings {
public:
    // Main
    bool m_bEnabled;
    bool m_bPedThreatEnabled;
    bool m_bCopRadarEnabled;
    bool m_bShowOnPauseMap;

    // Blip Sizes
    float m_fCopPedSize;
    float m_fCopVehicleSize;
    float m_fHeliSize;
    float m_fKillTargetSize;
    float m_fCarTargetSize;

    // Blip Colors
    CRGBA m_CopBlinkColor1;
    CRGBA m_CopBlinkColor2;
    CRGBA m_KillTargetColor;
    CRGBA m_CarTargetColor;

    // Misc
    int m_nCopPedBlinkMs;
    int m_nCopVehBlinkMs;
    int m_nBlinkJitterMs;

public:
    void Read();
};

extern Settings gSettings;