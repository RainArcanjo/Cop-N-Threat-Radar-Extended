#include <plugin.h>
#include <CGeneral.h>
#include "Settings.h"

using namespace plugin;

Settings gSettings;

void Settings::Read() {
    config_file ini(PLUGIN_PATH("CopNThreat.ini"));

    m_bEnabled = ini["Enabled"].asBool(true);
    m_bCopRadarEnabled = ini["CopRadarEnabled"].asBool(true);
    m_bPedThreatEnabled = ini["PedThreatEnabled"].asBool(true);
	m_bShowOnPauseMap = ini["ShowOnPauseMap"].asBool(true);

    m_fCopPedSize = ini["CopPedSize"].asFloat(14.0f);
    m_fCopVehicleSize = ini["CopVehicleSize"].asFloat(24.0f);
    m_fHeliSize = ini["HeliSize"].asFloat(28.0f);
    m_fKillTargetSize = ini["KillTargetSize"].asFloat(24.0f);
    m_fCarTargetSize = ini["CarTargetSize"].asFloat(22.0f);

    m_CopBlinkColor1 = ini["CopBlinkColor1"].asRGBA({ 200, 40, 40, 255 });
    m_CopBlinkColor2 = ini["CopBlinkColor2"].asRGBA({ 40, 90, 200, 255 });
    m_KillTargetColor = ini["KillTargetColor"].asRGBA({ 255, 60, 60, 255 });
    m_CarTargetColor = ini["CarTargetColor"].asRGBA({ 255, 165, 0, 255 });

    m_nCopPedBlinkMs = ini["CopPedBlinkMs"].asInt(900);
    m_nCopVehBlinkMs = ini["CopVehBlinkMs"].asInt(800);
    m_nBlinkJitterMs = ini["BlinkJitterMs"].asInt(120);
}