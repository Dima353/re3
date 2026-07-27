#pragma once

#include "config.h"

#ifdef CUTSCENE_HANDS

// HD cutscene hands, backported from the Xbox version of GTA:VC.
// Ported from the "XboxHandsVC" plugin-sdk mod (XboxHandsVC.asi) into native
// re3/reVC source: the plugin's address hooks become direct calls from
// CCutsceneObject / CCutsceneMgr / CStreaming, and its RenderWare skin/HAnim
// calls are mapped onto librw.
//
// SAFETY: the feature is fully opt-in on the assets. Initialise() only flips
// ms_enabled to true when EVERY required asset loads (anim/CSHands.txd,
// anim/CSHands.dff, the four anim/*.anm files, data/CutsceneHands.xml). If any
// is missing or fails to parse, ms_enabled stays false, a note is logged, and
// every entry point below becomes a no-op. The game then runs exactly as stock
// -- the original mod called exit(1) on a missing model; we never do that.

class CObject;

class CutsceneHands
{
public:
	static bool ms_enabled;		// true only after all assets loaded successfully

	// Lifecycle (call once RenderWare + streaming/TxdStore are up, and at shutdown)
	static void Initialise(void);	// loads txd/dff/anm/xml; sets ms_enabled
	static void Shutdown(void);

	// CCutsceneMgr::LoadCutsceneData -> (re)parse the per-cutscene config
	static void InitXML(void);

	// Special-character streaming (CStreaming request/loaded/remove for cutscene actors)
	static void RequestSpecialChar(int slot, const char *name);
	static void SpecialCharLoaded(int id);
	static void UnloadSpecialChar(int id);

	// CCutsceneObject render hooks
	static void PreRender(CObject *obj);	// attach + pose the HD hands, atrophy stock hand bones
	static void Render(CObject *obj);		// draw the HD hand atomics for this actor
};

#endif	// CUTSCENE_HANDS
