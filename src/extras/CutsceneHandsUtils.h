#pragma once

#include "config.h"
#ifdef CUTSCENE_HANDS

// Small RenderWare helpers for the cutscene-hands port. The original mod's Utils.h
// reimplemented these with hardcoded retail-VC addresses and RW-SDK struct field
// names; none of that exists here, so everything is redone on top of librw (via the
// fakerw layer, which already provides the native RpHAnim*/RpClump*/RwStream* API).

#include "rwcore.h"
#include "rpworld.h"
#include "rphanim.h"
#include "rpskin.h"

// Load a .dff clump from a loose file. Returns nil on any failure (caller decides).
static RpClump *CutsceneHands_LoadClump(const char *name)
{
	RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, name);
	if(stream == nil)
		return nil;
	RpClump *out = nil;
	if(RwStreamFindChunk(stream, rwID_CLUMP, nil, nil))
		out = RpClumpStreamRead(stream);
	RwStreamClose(stream, nil);
	return out;
}

// Load a RenderWare .anm (HAnim animation) from a loose file. nil on failure.
static RpHAnimAnimation *CutsceneHands_LoadAnm(const char *name)
{
	RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, name);
	if(stream == nil)
		return nil;
	RpHAnimAnimation *out = nil;
	if(RwStreamFindChunk(stream, rwID_HANIMANIMATION, nil, nil))
		out = RpHAnimAnimationStreamRead(stream);
	RwStreamClose(stream, nil);
	return out;
}

static RpAtomic *CutsceneHands_GetHierCB(RpAtomic *atomic, void *data)
{
	*(RpHAnimHierarchy**)data = RpSkinAtomicGetHAnimHierarchy(atomic);
	return nil;
}

// First skinned atomic's HAnim hierarchy in a clump.
static RpHAnimHierarchy *CutsceneHands_GetHierarchy(RpClump *clump)
{
	RpHAnimHierarchy *hier = nil;
	RpClumpForAllAtomics(clump, CutsceneHands_GetHierCB, &hier);
	return hier;
}

#endif	// CUTSCENE_HANDS
