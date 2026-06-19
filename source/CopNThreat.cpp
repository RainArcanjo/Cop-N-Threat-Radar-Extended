#include <plugin.h>
#include <CMenuManager.h>
#include <CHud.h>
#include <CTimer.h>
#include <CRadar.h>
#include <CFont.h>

#include "Settings.h"
#include "CopBlips.h"
#include "NonCopPeds.h"
#include "BlipRenderer.h"

using namespace plugin;

namespace {
    static unsigned int s_keyPressTime = 0;
    BlipRenderer gBlipRenderer;
    CopBlips     gCopBlips;
    NonCopPeds   gNonCopPeds;

    class CopNThreat {
    public:
        CopNThreat() {
            Events::initRwEvent += [] {
                gSettings.Read();
                gBlipRenderer.Initialise(gSettings);
                gCopBlips.Initialise(gSettings, gBlipRenderer);
                gNonCopPeds.Initialise(gSettings, gBlipRenderer);
            };

            Events::shutdownRwEvent += [] {
                gBlipRenderer.Shutdown();
                gCopBlips.Shutdown();
                gNonCopPeds.Shutdown();
            };

            Events::gameProcessEvent += [] {
                if (plugin::KeyPressed(VK_F11) && CTimer::m_snTimeInMillisecondsPauseMode - s_keyPressTime > 200) {
                    if (!gSettings.m_bEnabled) {
                        s_keyPressTime = CTimer::m_snTimeInMillisecondsPauseMode;
                        return;
					}

                    gSettings.Read();
					gBlipRenderer.Reload(gSettings);
					gCopBlips.OnSettingsChanged(gSettings);
					gNonCopPeds.OnSettingsChanged(gSettings);
                    s_keyPressTime = CTimer::m_snTimeInMillisecondsPauseMode;

                    CHud::SetHelpMessage("CopNThreat: Settings Reloaded", true, false, false);
                }

                if (!gSettings.m_bEnabled) return;
                gNonCopPeds.Update();
            };

            Events::drawBlipsEvent += [] {
                if (!gSettings.m_bEnabled) return;
                if (FrontEndMenuManager.m_bDrawRadarOrMap != 0 && !gSettings.m_bShowOnPauseMap) return;

                CWanted* wanted = FindPlayerWanted();
                const bool hasWanted = wanted && wanted->m_nWantedLevel > 0;

                if (gSettings.m_bPedThreatEnabled) {
                    gNonCopPeds.Draw();
                }

                if (hasWanted && gSettings.m_bCopRadarEnabled) {
                    gCopBlips.DrawCopPeds();
                    gCopBlips.DrawCopVehicles();
                }
            };
        }
    } gApp; // instancia única
} // namespace
