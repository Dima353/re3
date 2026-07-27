#include "common.h"

#ifdef CUTSCENE_HANDS

#include "CutsceneHands.h"
#include "CutsceneMgr.h"
#include "Script.h"
#include "Streaming.h"
#include "ModelInfo.h"
#include "TxdStore.h"
#include "FileMgr.h"
#include "Timer.h"
#include "Object.h"
#include "CutsceneObject.h"
#include "NodeName.h"
#include "rphanim.h"
#include "rpskin.h"
#include "XmlLibrary.h"
#include "CutsceneHandsUtils.h"
#ifdef EXTENDED_PIPELINES
#include "custompipes.h"
#endif

#include <array>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <strings.h>

#ifndef stricmp
#define stricmp strcasecmp
#endif

#define NO_HANDS_STREAMING	// hands are loaded straight from the anim folder (no streaming slot)

bool CutsceneHands::ms_enabled = false;

// The former XboxHandsVC plugin class, now a file-local implementation. Its address
// hooks are gone; the public CutsceneHands:: methods at the bottom drive it, and the
// RenderWare skin/HAnim calls are mapped onto librw (see CutsceneHandsUtils.h).
namespace {

class XboxHandsVC {
public:
    enum Definitions {
        CUTSCENE_BUFFER_SIZE = 64000,
        NUM_INSTANCES = 32,
        MODEL_CSHANDS = 639,
        SPECIAL_CHAR_OFFSET = 108,
    };

    enum BoneTag {
        BONE_root = 0,
        BONE_pelvis = 1,
        BONE_spine = 2,
        BONE_spine1 = 3,
        BONE_neck = 4,
        BONE_head = 5,
        BONE_l_clavicle = 31,
        BONE_l_upperarm = 32,
        BONE_l_forearm = 33,
        BONE_l_hand = 34,
        BONE_l_finger = 35,
        BONE_r_clavicle = 21,
        BONE_r_upperarm = 22,
        BONE_r_forearm = 23,
        BONE_r_hand = 24,
        BONE_r_finger = 25,
        BONE_l_thigh = 41,
        BONE_l_calf = 42,
        BONE_l_foot = 43,
        BONE_r_thigh = 51,
        BONE_r_calf = 52,
        BONE_r_foot = 53,
    };

    struct HandAnimInfo {
        char* m_aAnimFile;
        RpHAnimAnimation* m_pAnim;
    };

    struct AtomicByNameData {
        char name[256];
        RpAtomic* atomic;
    };

    struct XMLConfigure;
    struct HandState;

    static inline constexpr bool m_PrefsHighpolyModels = true;
    static inline XmlLibrary::Parser* g_parser = nullptr;
    static inline XmlLibrary::Element* g_element = nullptr;

    struct CutsceneHand {
        static inline HandAnimInfo m_Animation[2][2] = {
            {
                { "anim/SLhandWM.anm", nullptr },
                { "anim/SRhandWM.anm", nullptr }
            },
            {
                { "anim/SLhandWF.anm", nullptr },
                { "anim/SRhandWF.anm", nullptr }
            }
        };

        static inline const char* m_AtomicName[2][2][2] = {
            {
                {
                    { "SLhandPalmCM" },
                    { "SRhandPalmCM" }
                },

                {
                    { "SLhandPalmWM" },
                    { "SRhandPalmWM" }
                }
            },

            {
                {
                    { "SLhandPalmCF" },
                    { "SRhandPalmCF" }
                },

                {
                    { "SLhandPalmWF" },
                    { "SRhandPalmWF" }
                }
            }
        };

        static inline const char* m_PropAtomicName[2][2] = {
            {
                { "" },
                { "" }
            },

            {
                { "SLhandPalmGM" },
                { "SRhandPalmGM" }
            }
        };

        enum Gender {
            GENDER_MALE = 0,
            GENDER_FEMALE,
        };

        enum Race {
            RACE_BLACK = 0,
            RACE_WHITE,
        };

        enum Stature {
            STATURE_0 = 0,
            STATURE_1
        };

        enum Prop {
            PROP_NONE = 0,
        };

        enum Handedness {
            HANDEDNESS_LEFT = 0,
            HANDEDNESS_RIGHT,
            HANDEDNESS_BOTH,
            HANDEDNESS_UNDEFINED,
        };

        struct HandInstance {
            RpAtomic* m_pAtomic;
            RwFrame* m_pFrame;
            bool m_bHasHand;
            CRGBA m_Color;
            uint8_t pad[3];
            CVector m_vecPos;
            RpHAnimAnimation* m_pAnim;
            float m_fAnimTime;
            char m_aName[15];
            int8_t m_PrintName;
            int32_t m_Gender;
            int32_t m_Race;
            int32_t m_Stature;
            int32_t m_Prop;
            CVector m_vecScale;

            void Initialise() {
                m_fAnimTime = 0.0f;

                m_vecScale.x = 1.0f;
                m_vecScale.y = 1.0f;
                m_vecScale.z = 1.0f;

                m_pAtomic = nullptr;

                m_pFrame = nullptr;

                m_bHasHand = true;

                m_Color.r = 255;
                m_Color.g = 255;
                m_Color.b = 255;
                m_Color.a = 255;

                m_aName[0] = '\0';

                m_PrintName = 0;

                m_Gender = GENDER_MALE;
                m_Race = RACE_WHITE;
                m_Prop = PROP_NONE;
                m_Stature = STATURE_0;
            }
        };

        VALIDATE_SIZE(HandInstance, 0x50);

        static void Init() {
            if (m_PrefsHighpolyModels) {
                InitXML();
                InitialiseCharacters();
                m_Animation[0][0].m_pAnim = CutsceneHands_LoadAnm(m_Animation[0][0].m_aAnimFile);
                m_Animation[0][1].m_pAnim = CutsceneHands_LoadAnm(m_Animation[0][1].m_aAnimFile);
                m_Animation[1][0].m_pAnim = CutsceneHands_LoadAnm(m_Animation[1][0].m_aAnimFile);
                m_Animation[1][1].m_pAnim = CutsceneHands_LoadAnm(m_Animation[1][1].m_aAnimFile);
            }
        }

        static void InitXML() {
            if (m_PrefsHighpolyModels && !g_parser) {
                uint8_t* buff = new uint8_t[CUTSCENE_BUFFER_SIZE];

                if (buff) {
                    int32_t ret = CFileMgr::LoadFile("data/CutsceneHands.xml", buff, CUTSCENE_BUFFER_SIZE, "r");
                    if (ret < 0) {
                        delete[] buff;
                        return;
                    }

                    g_parser = new XmlLibrary::Parser();
                    g_element = &g_parser->Parse((const char*)buff, CUTSCENE_BUFFER_SIZE);

                    delete[] buff;
                }
            }
        }

        static void InitialiseCharacters() {
            for (auto& inst : Instances) {
                inst[0].Initialise();
                inst[1].Initialise();
            }
        }

        static void CleanUp() {
            if (m_PrefsHighpolyModels) {
                for (auto& anim : m_Animation) {
                    if (anim[0].m_pAnim) {
                        RpHAnimAnimationDestroy(anim[0].m_pAnim);
                        anim[0].m_pAnim = nullptr;
                    }

                    if (anim[1].m_pAnim) {
                        RpHAnimAnimationDestroy(anim[1].m_pAnim);
                        anim[1].m_pAnim = nullptr;
                    }
                }
            }

            if (g_parser) {
                delete g_parser;
                g_parser = nullptr;
            }

            for (int32_t i = 0; i < NUM_INSTANCES; i++)
                Unload(i);
        }

        static RpAtomic* FindHandAtomicByName(RpAtomic* atomic, void* data) {
            const char* nodeName = GetFrameNodeName(RpAtomicGetFrame(atomic));

            if (!strcmp(((AtomicByNameData*)data)->name, nodeName)) {
                ((AtomicByNameData*)data)->atomic = atomic;
                return nullptr;
            }
            return atomic;
        }

        static void SetHandAnimation(int32_t index, Handedness hand) {
            auto inst = &Instances[index][hand];
            inst->m_pAnim = m_Animation[inst->m_Gender][hand].m_pAnim;
        }

        static void SetMaterialColour(RpMaterial* mat, RwRGBA const& col) {
            mat->color = col;
        }

        static bool IsCutsceneRunning(const char* name) {
            if (!stricmp(CCutsceneMgr::GetCutsceneName(), name))
                return true;

            if (CTheScripts::NumberOfIntroTextLinesThisFrame) {
                for (CRunningScript* sc = CTheScripts::pActiveScripts; sc; sc = sc->GetNext()) {
                    if (!stricmp(sc->m_abScriptName, name))
                        return true;
                }
            }

            return false;
        }

        static bool IsSpecialCharacter(int32_t index) {
            return (uint32_t)(index - 1) <= 20;
        }

        static int32_t GetModelIndexFromName(const char* name) {
            int32_t i = 0;
            for (auto& inst : Instances) {
                if (!stricmp(inst[HANDEDNESS_LEFT].m_aName, name)
                    && !stricmp(inst[HANDEDNESS_RIGHT].m_aName, name)) {
                    return i;
                }
                i++;
            }

            return -1;
        }

        static void ConfigureForCharacter(int32_t index) {
            if (!m_PrefsHighpolyModels)
                return;

            XMLConfigure conf(*g_element, index);
            conf.Evaluate();

            HandState& leftHand = conf.GetHand(HANDEDNESS_LEFT);
            HandState& rightHand = conf.GetHand(HANDEDNESS_RIGHT);

            SetColour(index, HANDEDNESS_LEFT, leftHand);
            SetColour(index, HANDEDNESS_RIGHT, rightHand);

            SetAnimationTime(index, HANDEDNESS_LEFT, leftHand);
            SetAnimationTime(index, HANDEDNESS_RIGHT, rightHand);

            SetModel(index, HANDEDNESS_LEFT, leftHand);
            SetModel(index, HANDEDNESS_RIGHT, rightHand);

            SetScale(index, HANDEDNESS_LEFT, leftHand);
            SetScale(index, HANDEDNESS_RIGHT, rightHand);

            SetPosition(index, HANDEDNESS_LEFT, leftHand);
            SetPosition(index, HANDEDNESS_RIGHT, rightHand);
        }

        static void SetColour(int32_t index, Handedness hand, HandState& state) {
            auto inst = &Instances[index][hand];
            inst->m_Color = state.m_Color;
        }

        static void SetAnimationTime(int32_t index, Handedness hand, HandState& state) {
            auto inst = &Instances[index][hand];

            if (state.m_bIsAnimStatic)
                inst->m_fAnimTime = state.m_fAnimTime;
            else if (!CTimer::m_CodePause)
                inst->m_fAnimTime = CCutsceneMgr::GetCutsceneTimeInMilleseconds() / 1000.0f;
        }

        static void SetModel(int32_t index, Handedness hand, HandState& state) {
            auto inst = &Instances[index][hand];

            inst->m_bHasHand = !state.m_Model.empty();

            if (inst->m_bHasHand) {
                state.m_Prop = (Prop)!stricmp(state.m_Model.c_str(), XMLConfigure::HAND_MODEL_GLOVE);
                SetSkinnedModel(index, hand, state.m_Gender, state.m_Race, state.m_Stature, state.m_Prop);
            }
        }

        static void SetScale(int32_t index, Handedness hand, HandState& state) {
            auto inst = &Instances[index][hand];
            inst->m_vecScale = state.m_vecScale;
        }

        static void SetPosition(int32_t index, Handedness hand, HandState& state) {
            auto inst = &Instances[index][hand];
            inst->m_vecPos = state.m_vecPos;
        }

        static int32_t GetIdForCharacter() {
            return MODEL_CSHANDS;
        }

        static void Destroy(int32_t index, Handedness hand) {
            if (index <= 20) {
#ifndef NO_HANDS_STREAMING
                int32_t modelIndex = GetIdForCharacter();
                CStreaming::SetMissionDoesntRequireModel(modelIndex);
                CStreaming::SetModelIsDeletable(modelIndex);
                CStreaming::SetModelTxdIsDeletable(modelIndex);
#endif

                auto inst = &Instances[index][hand];
                if (inst->m_pAtomic) {
                    RpAtomicDestroy(inst->m_pAtomic);
                    inst->m_pAtomic = nullptr;
                }

                if (inst->m_pFrame) {
                    RwFrameDestroyHierarchy(inst->m_pFrame);
                    inst->m_pFrame = nullptr;
                }
            }
        }

        static void DeleteRwObject(int32_t index, Handedness hand) {
            Destroy(index, hand);
        }

        static void SetSkinnedModel(int32_t index, Handedness hand, Gender gender, Race race, Stature stature, Prop prop) {
            auto inst = &Instances[index][hand];
            if (inst->m_pAtomic) {
                if (inst->m_Gender == gender &&
                    inst->m_Race == race &&
                    inst->m_Stature == stature &&
                    inst->m_Prop == prop)
                    return;

                DeleteRwObject(index, hand);
                inst->m_pAtomic = nullptr;
            }

            inst->m_Gender = gender;
            inst->m_Race = race;
            inst->m_Prop = prop;
            inst->m_Stature = stature;

            int32_t modelIndex = GetIdForCharacter();

            if (modelIndex < 0)
                return;

#ifdef NO_HANDS_STREAMING
            RpClump* clump = m_pHandsClump;
#else
            RpClump* clump = ((CClumpModelInfo*)CModelInfo::ms_modelInfoPtrs[modelIndex])->m_pClump;
#endif

            if (clump) {
                AtomicByNameData data;
                data.atomic = nullptr;
                data.name[0] = '\0';

                if (prop)
                    strcpy(data.name, m_PropAtomicName[prop][hand]);
                else
                    strcpy(data.name, m_AtomicName[gender][race][hand]);

                RpClumpForAllAtomics(clump, FindHandAtomicByName, &data);
                if(data.atomic == nil) return;	// nothing to clone -> avoid NULL deref

                inst->m_pAtomic = RpAtomicClone(data.atomic);

                RpHAnimHierarchy* hier = RpHAnimFrameGetHierarchy(RpAtomicGetFrame(data.atomic));
                // The hand hierarchy is posed in object-local space and placed in the world via the
                // atomic frame's LTM (see PositionNew). librw's skin pipeline only honours that when
                // LOCALSPACEMATRICES is set; without it the pipeline treats the bone matrices as
                // world-space and factors out the atomic LTM, conjugating the local finger pose by the
                // wrist's world translation and flinging the hand off-screen (invisible hands).
                RpHAnimHierarchy* hierNew = rw::HAnimHierarchy::create(hier->numNodes, nil, nil,
                    hier->flags | rw::HAnimHierarchy::LOCALSPACEMATRICES, 36);

                for (int32_t i = 0; i < hierNew->numNodes; i++) {
                    hierNew->nodeInfo[i].frame = nil;
                    hierNew->nodeInfo[i].flags = hier->nodeInfo[i].flags;
                    hierNew->nodeInfo[i].index = hier->nodeInfo[i].index;
                    hierNew->nodeInfo[i].id = hier->nodeInfo[i].id;
                }

                auto frame = RwFrameCreate();
                RpAtomicSetFrame(inst->m_pAtomic, frame);
                RpHAnimFrameSetHierarchy(frame, hierNew);
                RpSkinAtomicSetHAnimHierarchy(inst->m_pAtomic, hierNew);

                auto parentFrame = RwFrameCreate();
                RwFrameAddChild(parentFrame, frame);
                inst->m_pFrame = parentFrame;

                InitialiseHandAnimation(index, hand);

#ifdef EXTENDED_PIPELINES
                // The ped body renders through the neo rim-light skin pipe (PedModelInfo attaches it
                // to every ped clump). Give the hand the same pipe so its lighting tracks the body
                // instead of the vanilla skin pipe (otherwise the hand over-responds to time-of-day
                // ambient/directional - lighter by day, darker by night).
                CustomPipes::AttachRimPipe(inst->m_pAtomic);
#endif

#ifdef DEBUG
                PrintCharacterNames(index, 1);
#endif
            }
        }

        static void InitialiseHandAnimation(int32_t index, Handedness hand) {
            auto inst = &Instances[index][hand];
            RpHAnimHierarchy* hier = RpHAnimFrameGetHierarchy(RpAtomicGetFrame(inst->m_pAtomic));
            RpSkinAtomicSetHAnimHierarchy(inst->m_pAtomic, hier);
            SetHandAnimation(index, hand);
            RpHAnimHierarchySetCurrentAnim(hier, inst->m_pAnim);
            RpHAnimHierarchySetCurrentAnimTime(hier, 0.0f);
        }

        static void PrintCharacterNames(int32_t index, int8_t on) {
#ifdef DEBUG
            HandInstance* inst = Instances[index].data();
            inst[0].m_PrintName = on;
            inst[1].m_PrintName = on;
#endif
        }

        static RwMatrix* GetBoneMatrix(RpClump* clump, int32_t bone) {
            RpHAnimHierarchy* hier = CutsceneHands_GetHierarchy(clump);
            int32_t index = RpHAnimIDGetIndex(hier, bone);

            if (index < 0)
                return nullptr;
            else
                return &RpHAnimHierarchyGetMatrixArray(hier)[index];
        }

        static void CopyBoneMatrix(RpClump* clump, int32_t bone, RwMatrix* out) {
            RwMatrix* mat = GetBoneMatrix(clump, bone);
            if (mat) {
                out->right = mat->right;
                out->flags = mat->flags;
                out->up = mat->up;
                out->pad1 = mat->pad1;
                out->at = mat->at;
                out->pad2 = mat->pad2;
                out->pos = mat->pos;
                out->pad3 = mat->pad3;
            }
        }

        static void PositionNew(RpClump* clump, int32_t index, Handedness hand) {
            int32_t id = GetIdForCharacter();
            if (id < 0)
                return;

            auto inst = &Instances[index][hand];

            if (!inst->m_pAtomic)
                return;

            if (!inst->m_pFrame)
                return;

            RwFrame* frame = inst->m_pFrame;

            CopyBoneMatrix(clump, hand == HANDEDNESS_LEFT ? BONE_l_hand : BONE_r_hand, &frame->matrix);
            RwMatrixUpdate(&frame->matrix);
            RwFrameUpdateObjects(frame);

            CVector pos = inst->m_vecPos;
            CVector scale = inst->m_vecScale;

            RwMatrix mat;
            CopyBoneMatrix(clump, hand == HANDEDNESS_LEFT ? BONE_l_forearm : BONE_r_forearm, &mat);
            mat.pos.x = 0.0f;
            mat.pos.y = 0.0f;
            mat.pos.z = 0.0f;

            RwMatrixUpdate(&mat);

            RwV3dTransformVectors((RwV3d*)&pos, (RwV3d*)&pos, 1, &mat);

            RwMatrixTranslate(&frame->matrix, (RwV3d*)&pos, rwCOMBINEPOSTCONCAT);
            RwMatrixUpdate(&frame->matrix);

            RwMatrixScale(&frame->matrix, (RwV3d*)&scale, rwCOMBINEPRECONCAT);
            RwMatrixUpdate(&frame->matrix);

            RwFrameUpdateObjects(frame);
        }

        static void UpdateAnimation(RpClump* clump, int32_t index, Handedness hand) {
            auto inst = &Instances[index][hand];

            if (!inst->m_pAtomic)
                return;

            auto hier = RpSkinAtomicGetHAnimHierarchy(inst->m_pAtomic);

            // Anim loop workaround
            float time = inst->m_fAnimTime;
            float duration = hier->interpolator->currentAnim->duration + 3.0f;
            float currentTime = hier->interpolator->currentTime;

            if (duration > 0.0f) {
                currentTime = std::fmod(time, duration);
            }
            else {
                currentTime = 0.0f;
            }
            if (hier->interpolator->currentTime >= hier->interpolator->currentAnim->duration) {
                RpHAnimHierarchySetCurrentAnim(hier, inst->m_pAnim);
            }
            //

            RpHAnimHierarchySetCurrentAnimTime(hier, currentTime);
            RpHAnimHierarchyUpdateMatrices(hier);
        }

        static void UpdateSkin(RpClump* clump, int32_t index, Handedness hand) {
            int32_t id = GetIdForCharacter();
            if (id >= 0 && Instances[index][hand].m_bHasHand) {
                PositionNew(clump, index, hand);
                UpdateAnimation(clump, index, hand);
            }
        }

        // Hides the low-poly hand under the HD replacement: zero-scale the wrist bone, then collapse
        // every bone below the wrist (fingers/fingertips, whatever their ids) onto it.
        //
        // Skinned peds keep their skeleton as a FLAT bone array (no RwFrame tree), with parent/child
        // links encoded only by the hierarchy's push/pop node flags. So we reconstruct each bone's
        // parent the same way updateMatrices walks it, then fold the wrist's whole sub-tree. Walking
        // the real topology - instead of the old hard-coded finger ids (36/26/54..63) - makes this
        // work on any character's skeleton (fixes 'double hands' where finger bones weren't collapsed).
        static void AtrophyHand(RpClump* clump, int32_t boneTag) {
            RpHAnimHierarchy* hier = CutsceneHands_GetHierarchy(clump);
            if (!hier)
                return;

            int32_t handIdx = RpHAnimIDGetIndex(hier, boneTag);
            if (handIdx < 0)
                return;

            int32_t numNodes = hier->numNodes;
            if (numNodes > 64)
                numNodes = 64;

            int32_t parentOf[64];
            int32_t stack[64], sp = 0, curParent = -1;
            for (int32_t i = 0; i < numNodes; i++) {
                parentOf[i] = curParent;
                if (hier->nodeInfo[i].flags & rw::HAnimHierarchy::PUSH)
                    stack[sp++] = curParent;
                curParent = i;
                if ((hier->nodeInfo[i].flags & rw::HAnimHierarchy::POP) && sp > 0)
                    curParent = stack[--sp];
            }

            RwMatrix* mats = RpHAnimHierarchyGetMatrixArray(hier);
            RwV3d zero = { 0.0f, 0.0f, 0.0f };
            RwMatrixScale(&mats[handIdx], &zero, rwCOMBINEPRECONCAT);
            RwMatrixUpdate(&mats[handIdx]);
            RwMatrix collapsed = mats[handIdx];

            for (int32_t i = handIdx + 1; i < numNodes; i++) {
                int32_t a = i;
                while (a >= 0 && a != handIdx)
                    a = parentOf[a];
                if (a == handIdx)
                    mats[i] = collapsed;
            }
        }

        static void Replace(CObject* obj, Handedness hand) {
            int32_t index = obj->GetModelIndex() - SPECIAL_CHAR_OFFSET;
            bool isSpecial = IsSpecialCharacter(index);

            if (!isSpecial)
                return;

            UpdateSkin(obj->GetClump(), index, hand);

            RpClump* clump = obj->GetClump();
            if (hand == HANDEDNESS_LEFT)
                AtrophyHand(clump, BONE_l_hand);
            else
                AtrophyHand(clump, BONE_r_hand);
        }

        static void PreRender(CObject* obj) {
            if (!m_PrefsHighpolyModels)
                return;

            int32_t index = obj->GetModelIndex() - SPECIAL_CHAR_OFFSET;

            bool isSpecialChar = IsSpecialCharacter(index);
            if (!isSpecialChar)
                return;

            ConfigureForCharacter(index);

            bool hasLeftHand = Instances[index][HANDEDNESS_LEFT].m_bHasHand;
            bool hasRightHand = Instances[index][HANDEDNESS_RIGHT].m_bHasHand;

            if (hasLeftHand)
                Replace(obj, HANDEDNESS_LEFT);

            if (hasRightHand)
                Replace(obj, HANDEDNESS_RIGHT);
        }

        static void Render(int32_t index, Handedness hand) {
            if (!m_PrefsHighpolyModels)
                return;

            bool isSpecialChar = IsSpecialCharacter(index);
            if (!isSpecialChar)
                return;

            auto inst = &Instances[index][hand];
            bool hasHands = inst->m_bHasHand;

            if (!hasHands)
                return;

            if (!inst->m_pAtomic)
                return;

            RpMaterialSetColor(RpGeometryGetMaterial(RpAtomicGetGeometry(inst->m_pAtomic), 0), (RwRGBA*)&inst->m_Color);
            RpAtomicRender(inst->m_pAtomic);
        }

        static void Create(int32_t index, Handedness hand) {
            auto inst = &Instances[index][hand];
            SetSkinnedModel(index,
                            hand,
                            (Gender)inst->m_Gender,
                            (Race)inst->m_Race,
                            (Stature)inst->m_Stature,
                            (Prop)inst->m_Prop);
        }

        static void HasLoaded(int32_t index) {
            if (!m_PrefsHighpolyModels)
                return;

#ifndef NO_HANDS_STREAMING
            if (CStreaming::ms_aInfoForModel[CutsceneHand::GetIdForCharacter()].m_nLoadState != LOADSTATE_LOADED)
                return;
#endif

            if (!Instances[index][HANDEDNESS_LEFT].m_pAtomic)
                CutsceneHand::Create(index, CutsceneHand::HANDEDNESS_LEFT);

            if (!Instances[index][HANDEDNESS_RIGHT].m_pAtomic)
                CutsceneHand::Create(index, CutsceneHand::HANDEDNESS_RIGHT);
        }

        static void Unload(int32_t index) {
            if (!m_PrefsHighpolyModels)
                return;

            Destroy(index, HANDEDNESS_LEFT);
            Destroy(index, HANDEDNESS_RIGHT);
        }

        static int32_t ReplaceHandsOnCharacterWithThisAnim(char* anim) {
            return 1;
        }

        static int32_t SetAtomicColour(RpAtomic*, void*) {
            return 0;
        }

        static void Request(int32_t index, const char* name) {
            if (!m_PrefsHighpolyModels)
                return;

            int32_t id = GetIdForCharacter();
            if (id < 0)
                return;

#ifndef NO_HANDS_STREAMING
            CStreaming::RequestModel(id, 6);
#endif

            Instances[index][0].m_pAtomic = nullptr;
            Instances[index][1].m_pAtomic = nullptr;

            strncpy(Instances[index][0].m_aName, name, 15);
            strncpy(Instances[index][1].m_aName, name, 15);
        }
    };

    struct HandState {
        std::string m_Model;
        float m_fAnimStart;
        float m_fAnimEnd;
        float m_fAnimTime;
        bool m_bIsAnimStatic;
        float m_fAnimCutsceneStart;
        float m_fAnimCutsceneEnd;
        CVector m_vecScale;
        CVector m_vecRotate;
        CVector m_vecPos;
        CRGBA m_Color;

        CutsceneHand::Gender m_Gender;
        CutsceneHand::Race m_Race;
        CutsceneHand::Prop m_Prop;
        CutsceneHand::Stature m_Stature;

        void Init() {
            m_Model = "";

            m_fAnimStart = 0.0f;
            m_fAnimEnd = 0.0f;
            m_fAnimTime = 0.0f;
            m_bIsAnimStatic = false;
            m_fAnimCutsceneStart = 0.0f;
            m_fAnimCutsceneEnd = 0.0f;

            m_vecScale.x = 1.0f;
            m_vecScale.y = 1.0f;
            m_vecScale.z = 1.0f;

            m_vecRotate.x = 0.0f;
            m_vecRotate.y = 0.0f;
            m_vecRotate.z = 0.0f;

            m_vecPos.x = 0.0f;
            m_vecPos.y = 0.0f;
            m_vecPos.z = 0.0f;

            m_Color.r = 255;
            m_Color.g = 255;
            m_Color.b = 255;
            m_Color.a = 255;

            m_Gender = CutsceneHand::GENDER_MALE;
            m_Race = CutsceneHand::RACE_WHITE;

            m_Stature = CutsceneHand::STATURE_1;
            m_Prop = CutsceneHand::PROP_NONE;
        }

        HandState& operator=(const HandState& right) {
            m_Model = right.m_Model;
            m_fAnimStart = right.m_fAnimStart;
            m_fAnimEnd = right.m_fAnimEnd;
            m_fAnimTime = right.m_fAnimTime;
            m_bIsAnimStatic = right.m_bIsAnimStatic;
            m_fAnimCutsceneStart = right.m_fAnimCutsceneStart;
            m_fAnimCutsceneEnd = right.m_fAnimCutsceneEnd;
            m_vecScale.x = right.m_vecScale.x;
            m_vecScale.y = right.m_vecScale.y;
            m_vecScale.z = right.m_vecScale.z;
            m_vecRotate.x = right.m_vecRotate.x;
            m_vecRotate.y = right.m_vecRotate.y;
            m_vecRotate.z = right.m_vecRotate.z;
            m_vecPos.x = right.m_vecPos.x;
            m_vecPos.y = right.m_vecPos.y;
            m_vecPos.z = right.m_vecPos.z;
            m_Color.r = right.m_Color.r;
            m_Color.g = right.m_Color.g;
            m_Color.b = right.m_Color.b;
            m_Color.a = right.m_Color.a;
            m_Race = right.m_Race;
            m_Gender = right.m_Gender;
            m_Prop = right.m_Prop;
            m_Stature = right.m_Stature;
            return *this;
        }
    };

    struct XMLConfigure {
        static inline constexpr char* XML_ATTRIBUTE_ID = "ID";
        static inline constexpr char* XML_ATTRIBUTE_NAME = "NAME";
        static inline constexpr char* XML_ATTRIBUTE_SIDE = "SIDE";
        static inline constexpr char* XML_ATTRIBUTE_LEFT = "LEFT";
        static inline constexpr char* XML_ATTRIBUTE_RIGHT = "RIGHT";
        static inline constexpr char* XML_ATTRIBUTE_BOTH = "BOTH";
        static inline constexpr char* XML_ATTRIBUTE_MODEL = "MODEL";
        static inline constexpr char* XML_ATTRIBUTE_GENDER = "GENDER";
        static inline constexpr char* XML_ATTRIBUTE_MALE = "MALE";
        static inline constexpr char* XML_ATTRIBUTE_FEMALE = "FEMALE";
        static inline constexpr char* XML_ATTRIBUTE_RACE = "RACE";
        static inline constexpr char* XML_ATTRIBUTE_BLACK = "BLACK";
        static inline constexpr char* XML_ATTRIBUTE_WHITE = "WHITE";
        static inline constexpr char* XML_ATTRIBUTE_SCALE_X = "SCALEX";
        static inline constexpr char* XML_ATTRIBUTE_SCALE_Y = "SCALEY";
        static inline constexpr char* XML_ATTRIBUTE_SCALE_Z = "SCALEZ";
        static inline constexpr char* XML_ATTRIBUTE_ROTATE_X = "ROTATEX";
        static inline constexpr char* XML_ATTRIBUTE_ROTATE_Y = "ROTATEY";
        static inline constexpr char* XML_ATTRIBUTE_ROTATE_Z = "ROTATEZ";
        static inline constexpr char* XML_ATTRIBUTE_POSITION_X = "POSITIONX";
        static inline constexpr char* XML_ATTRIBUTE_POSITION_Y = "POSITIONY";
        static inline constexpr char* XML_ATTRIBUTE_POSITION_Z = "POSITIONZ";
        static inline constexpr char* XML_ATTRIBUTE_RED = "RED";
        static inline constexpr char* XML_ATTRIBUTE_GREEN = "GREEN";
        static inline constexpr char* XML_ATTRIBUTE_BLUE = "BLUE";
        static inline constexpr char* XML_ATTRIBUTE_CUTSCENE_START = "CUTSCENE_START";
        static inline constexpr char* XML_ATTRIBUTE_CUTSCENE_END = "CUTSCENE_END";
        static inline constexpr char* XML_ATTRIBUTE_ANIMATION_START = "START";
        static inline constexpr char* XML_ATTRIBUTE_ANIMATION_END = "END";
        static inline constexpr char* XML_ATTRIBUTE_ANIMATION_TIME = "TIME";
        static inline constexpr char* XML_ATTRIBUTE_ANIMATION_TYPE = "TYPE";
        static inline constexpr char* XML_ATTRIBUTE_ANIMATION_STATIC = "STATIC";
        static inline constexpr char* XML_ATTRIBUTE_ANIMATION_LOOP = "LOOP";
        static inline constexpr char* XML_TAG_ROOT = "GTA_XML";
        static inline constexpr char* XML_TAG_CUTSCENE = "CUTSCENE";
        static inline constexpr char* XML_TAG_CHARACTER = "CHARACTER";
        static inline constexpr char* XML_TAG_HAND = "HAND";
        static inline constexpr char* XML_TAG_ANIMATION = "ANIMATION";
        static inline constexpr char* HAND_MODEL_ORIGINAL = "NON-SKINNED";
        static inline constexpr char* HAND_MODEL_GLOVE = "SKINNED-GLOVE";

        XmlLibrary::Element* m_xmlRoot;
        float m_fCutsceneTime;
        int m_nModelIndex;
        std::string m_CutsceneName;
        XmlLibrary::Element* m_curElemet;
        CutsceneHand::Handedness m_HandSide;
        HandState _handstate18;
        HandState _handstate74;
        HandState m_RightHand;
        HandState m_LeftHand;

        bool IsCutsceneRunning(const std::string& name) {
            return CutsceneHand::IsCutsceneRunning(name.c_str());
        }

        void Init() {
            m_CutsceneName = "";
            m_HandSide = CutsceneHand::HANDEDNESS_BOTH;
            m_curElemet = m_xmlRoot;

            _handstate18.Init();
            _handstate74.Init();
            m_RightHand.Init();
            m_LeftHand.Init();
        }

        XMLConfigure(XmlLibrary::Element& root, int32_t player) {
            m_xmlRoot = &root;
            m_fCutsceneTime = CCutsceneMgr::GetCutsceneTimeInMilleseconds();
            m_nModelIndex = player;

            _handstate18.Init();
            _handstate74.Init();
            m_RightHand.Init();
            m_LeftHand.Init();

            Init();
        }

        void Animation() {
            const XmlLibrary::AttributeVector* attributes = m_curElemet->GetAttributes();

            AnimationType(attributes);
            AnimationTime(attributes);
            AnimationCutsceneStart(attributes);
            AnimationCutsceneEnd(attributes);
            AnimationStart(attributes);
            AnimationEnd(attributes);
        }

        void Cutscene() {
            const XmlLibrary::AttributeVector* attributes = m_curElemet->GetAttributes();

            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsName);

                if (it != attributes->end()) {
                    m_CutsceneName = (*it)->Value();
                }
            }

            if (m_CutsceneName.empty() || IsCutsceneRunning(m_CutsceneName))
                ProcessChildren();
        }

        void Process() {
            if (m_curElemet && !m_curElemet->IsNull()) {
                if (!stricmp(m_curElemet->GetName().c_str(), XML_TAG_ROOT)) {
                    ProcessChildren();
                }
                else if (!stricmp(m_curElemet->GetName().c_str(), XML_TAG_CUTSCENE)) {
                    Cutscene();
                }
                else if (!stricmp(m_curElemet->GetName().c_str(), XML_TAG_CHARACTER)) {
                    Character();
                }
                else if (!stricmp(m_curElemet->GetName().c_str(), XML_TAG_HAND)) {
                    Hand();
                }
                else if (!stricmp(m_curElemet->GetName().c_str(), XML_TAG_ANIMATION)) {
                    Animation();
                }
            }
        }

        void ProcessChildren() {
            const XmlLibrary::ElementVector* childs = m_curElemet->GetChilds();

            if (childs) {
                for (XmlLibrary::ElementVector::const_iterator it = childs->begin(); it != childs->end(); ++it) {
                    m_curElemet = *it;
                    Process();
                }
            }

        }

        void Hand() {
            const XmlLibrary::AttributeVector* attributes = m_curElemet->GetAttributes();

            CutsceneHand::Handedness savedHandSide;
            HandState saved18;
            HandState saved74;

            savedHandSide = m_HandSide;

            saved18 = _handstate18;
            saved74 = _handstate74;

            HandSide(attributes);
            HandModel(attributes);
            HandGender(attributes);
            HandRace(attributes);
            HandScaleX(attributes);
            HandScaleY(attributes);
            HandScaleZ(attributes);
            HandRotateX(attributes);
            HandRotateY(attributes);
            HandRotateZ(attributes);
            HandPositionX(attributes);
            HandPositionY(attributes);
            HandPositionZ(attributes);
            HandColourRed(attributes);
            HandColourGreen(attributes);
            HandColourBlue(attributes);

            ProcessChildren();

            m_HandSide = savedHandSide;

            _handstate18 = saved18;
            _handstate74 = saved74;
        }

        void Character() {
            const XmlLibrary::AttributeVector* attributes = m_curElemet->GetAttributes();

            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsName);

                if (it != attributes->end()) {
                    int32_t mi = CutsceneHand::GetModelIndexFromName((*it)->Value().c_str());

                    if (mi != -1 && mi != -3 && mi != m_nModelIndex)
                        return;
                }

            }

            ProcessChildren();
        }

        void Evaluate() {
            Init();
            Process();
        }

        HandState& GetHand(CutsceneHand::Handedness hand) {
            if (hand == CutsceneHand::HANDEDNESS_RIGHT)
                return m_RightHand;
            else
                return m_LeftHand;
        }

        void HandSide(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsSide);

                if (it != attributes->end()) {
                    if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_RIGHT))
                        m_HandSide = CutsceneHand::HANDEDNESS_RIGHT;
                    else if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_LEFT))
                        m_HandSide = CutsceneHand::HANDEDNESS_LEFT;
                    else if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_BOTH))
                        m_HandSide = CutsceneHand::HANDEDNESS_BOTH;
                    else
                        m_HandSide = CutsceneHand::HANDEDNESS_UNDEFINED;
                }
            }
        }

        void HandModel(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsModel);

                if (it != attributes->end()) {
                    std::string modelname = (*it)->Value();
                    if (!stricmp((*it)->Value().c_str(), HAND_MODEL_ORIGINAL))
                        modelname = "";

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_Model = modelname;
                        m_RightHand.m_Model = modelname;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_Model = modelname;
                        m_LeftHand.m_Model = modelname;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_Model = modelname;
                        m_RightHand.m_Model = modelname;

                        _handstate74.m_Model = modelname;
                        m_LeftHand.m_Model = modelname;
                    }
                }
            }
        }

        void HandGender(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsGender);

                if (it != attributes->end()) {
                    CutsceneHand::Gender gender = CutsceneHand::GENDER_MALE;

                    if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_MALE))
                        gender = CutsceneHand::GENDER_MALE;
                    else if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_FEMALE))
                        gender = CutsceneHand::GENDER_FEMALE;

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_Gender = gender;
                        m_RightHand.m_Gender = gender;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_Gender = gender;
                        m_LeftHand.m_Gender = gender;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_Gender = gender;
                        m_RightHand.m_Gender = gender;

                        _handstate74.m_Gender = gender;
                        m_LeftHand.m_Gender = gender;
                    }
                }
            }
        }

        void HandRace(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsRace);

                if (it != attributes->end()) {
                    CutsceneHand::Race race = CutsceneHand::RACE_WHITE;

                    if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_BLACK))
                        race = CutsceneHand::RACE_BLACK;
                    else if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_WHITE))
                        race = CutsceneHand::RACE_WHITE;

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_Race = race;
                        m_RightHand.m_Race = race;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_Race = race;
                        m_LeftHand.m_Race = race;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_Race = race;
                        m_RightHand.m_Race = race;

                        _handstate74.m_Race = race;
                        m_LeftHand.m_Race = race;
                    }
                }
            }
        }

        void HandScaleX(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsScaleX);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecScale.x = val;
                        m_RightHand.m_vecScale.x = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecScale.x = val;
                        m_LeftHand.m_vecScale.x = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecScale.x = val;
                        m_RightHand.m_vecScale.x = val;

                        _handstate74.m_vecScale.x = val;
                        m_LeftHand.m_vecScale.x = val;
                    }
                }
            }
        }

        void HandScaleY(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsScaleY);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecScale.y = val;
                        m_RightHand.m_vecScale.y = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecScale.y = val;
                        m_LeftHand.m_vecScale.y = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecScale.y = val;
                        m_RightHand.m_vecScale.y = val;

                        _handstate74.m_vecScale.y = val;
                        m_LeftHand.m_vecScale.y = val;
                    }
                }
            }
        }

        void HandScaleZ(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsScaleZ);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecScale.z = val;
                        m_RightHand.m_vecScale.z = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecScale.z = val;
                        m_LeftHand.m_vecScale.z = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecScale.z = val;
                        m_RightHand.m_vecScale.z = val;

                        _handstate74.m_vecScale.z = val;
                        m_LeftHand.m_vecScale.z = val;
                    }
                }
            }
        }

        void HandRotateX(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsRotateX);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecRotate.x = val;
                        m_RightHand.m_vecRotate.x = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecRotate.x = val;
                        m_LeftHand.m_vecRotate.x = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecRotate.x = val;
                        m_RightHand.m_vecRotate.x = val;

                        _handstate74.m_vecRotate.x = val;
                        m_LeftHand.m_vecRotate.x = val;
                    }
                }
            }
        }

        void HandRotateY(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsRotateY);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecRotate.y = val;
                        m_RightHand.m_vecRotate.y = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecRotate.y = val;
                        m_LeftHand.m_vecRotate.y = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecRotate.y = val;
                        m_RightHand.m_vecRotate.y = val;

                        _handstate74.m_vecRotate.y = val;
                        m_LeftHand.m_vecRotate.y = val;
                    }
                }
            }
        }

        void HandRotateZ(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsRotateZ);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecRotate.z = val;
                        m_RightHand.m_vecRotate.z = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecRotate.z = val;
                        m_LeftHand.m_vecRotate.z = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecRotate.z = val;
                        m_RightHand.m_vecRotate.z = val;

                        _handstate74.m_vecRotate.z = val;
                        m_LeftHand.m_vecRotate.z = val;
                    }
                }
            }
        }

        void HandPositionX(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsPositionX);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecPos.x = val;
                        m_RightHand.m_vecPos.x = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecPos.x = val;
                        m_LeftHand.m_vecPos.x = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecPos.x = val;
                        m_RightHand.m_vecPos.x = val;

                        _handstate74.m_vecPos.x = val;
                        m_LeftHand.m_vecPos.x = val;
                    }
                }
            }
        }

        void HandPositionY(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsPositionY);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecPos.y = val;
                        m_RightHand.m_vecPos.y = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecPos.y = val;
                        m_LeftHand.m_vecPos.y = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecPos.y = val;
                        m_RightHand.m_vecPos.y = val;

                        _handstate74.m_vecPos.y = val;
                        m_LeftHand.m_vecPos.y = val;
                    }
                }
            }
        }

        void HandPositionZ(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsPositionZ);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_vecPos.z = val;
                        m_RightHand.m_vecPos.z = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_vecPos.z = val;
                        m_LeftHand.m_vecPos.z = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_vecPos.z = val;
                        m_RightHand.m_vecPos.z = val;

                        _handstate74.m_vecPos.z = val;
                        m_LeftHand.m_vecPos.z = val;
                    }
                }
            }
        }

        void HandColourRed(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsRed);

                if (it != attributes->end()) {
                    int val;
                    sscanf((*it)->Value().c_str(), "%d", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_Color.r = val;
                        m_RightHand.m_Color.r = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_Color.r = val;
                        m_LeftHand.m_Color.r = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_Color.r = val;
                        m_RightHand.m_Color.r = val;

                        _handstate74.m_Color.r = val;
                        m_LeftHand.m_Color.r = val;
                    }
                }
            }
        }

        void HandColourGreen(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsGreen);

                if (it != attributes->end()) {
                    int val;
                    sscanf((*it)->Value().c_str(), "%d", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_Color.g = val;
                        m_RightHand.m_Color.g = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_Color.g = val;
                        m_LeftHand.m_Color.g = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_Color.g = val;
                        m_RightHand.m_Color.g = val;

                        _handstate74.m_Color.g = val;
                        m_LeftHand.m_Color.g = val;
                    }
                }
            }
        }

        void HandColourBlue(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsBlue);

                if (it != attributes->end()) {
                    int val;
                    sscanf((*it)->Value().c_str(), "%d", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_Color.b = val;
                        m_RightHand.m_Color.b = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_Color.b = val;
                        m_LeftHand.m_Color.b = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_Color.b = val;
                        m_RightHand.m_Color.b = val;

                        _handstate74.m_Color.b = val;
                        m_LeftHand.m_Color.b = val;
                    }
                }
            }
        }


        void AnimationCutsceneStart(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsCutsceneStart);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    val *= 100.0f;

                    if (m_fCutsceneTime > val) {
                        if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                            _handstate18.m_fAnimCutsceneStart = val;
                            m_RightHand.m_fAnimCutsceneStart = val;
                        }
                        else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                            _handstate74.m_fAnimCutsceneStart = val;
                            m_LeftHand.m_fAnimCutsceneStart = val;
                        }
                        else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                            _handstate18.m_fAnimCutsceneStart = val;
                            m_RightHand.m_fAnimCutsceneStart = val;

                            _handstate74.m_fAnimCutsceneStart = val;
                            m_LeftHand.m_fAnimCutsceneStart = val;
                        }
                    }
                }
            }
        }

        void AnimationCutsceneEnd(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsCutsceneEnd);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    val *= 100.0f;

                    if (m_fCutsceneTime > val) {
                        if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                            _handstate18.m_fAnimCutsceneEnd = val;
                            m_RightHand.m_fAnimCutsceneEnd = val;
                        }
                        else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                            _handstate74.m_fAnimCutsceneEnd = val;
                            m_LeftHand.m_fAnimCutsceneEnd = val;
                        }
                        else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                            _handstate18.m_fAnimCutsceneEnd = val;
                            m_RightHand.m_fAnimCutsceneEnd = val;

                            _handstate74.m_fAnimCutsceneEnd = val;
                            m_LeftHand.m_fAnimCutsceneEnd = val;
                        }
                    }
                }
            }
        }

        void AnimationStart(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsAnimationStart);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    val *= 100.0f;

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_fAnimStart = val;
                        m_RightHand.m_fAnimStart = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_fAnimStart = val;
                        m_LeftHand.m_fAnimStart = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_fAnimStart = val;
                        m_RightHand.m_fAnimStart = val;

                        _handstate74.m_fAnimStart = val;
                        m_LeftHand.m_fAnimStart = val;
                    }
                }
            }
        }

        void AnimationEnd(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsAnimationEnd);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    val *= 100.0f;

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_fAnimEnd = val;
                        m_RightHand.m_fAnimEnd = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_fAnimEnd = val;
                        m_LeftHand.m_fAnimEnd = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_fAnimEnd = val;
                        m_RightHand.m_fAnimEnd = val;

                        _handstate74.m_fAnimEnd = val;
                        m_LeftHand.m_fAnimEnd = val;
                    }
                }
            }
        }

        void AnimationType(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsAnimationType);

                if (it != attributes->end()) {
                    bool bIsStatic = false;

                    if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_ANIMATION_STATIC))
                        bIsStatic = true;
                    else if (!stricmp((*it)->Value().c_str(), XML_ATTRIBUTE_ANIMATION_LOOP))
                        bIsStatic = false;

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_bIsAnimStatic = bIsStatic;
                        m_RightHand.m_bIsAnimStatic = bIsStatic;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_bIsAnimStatic = bIsStatic;
                        m_LeftHand.m_bIsAnimStatic = bIsStatic;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_bIsAnimStatic = bIsStatic;
                        m_RightHand.m_bIsAnimStatic = bIsStatic;

                        _handstate74.m_bIsAnimStatic = bIsStatic;
                        m_LeftHand.m_bIsAnimStatic = bIsStatic;
                    }
                }
            }
        }

        void AnimationTime(const XmlLibrary::AttributeVector* attributes) {
            if (attributes) {
                XmlLibrary::AttributeVector::const_iterator it = std::find_if(attributes->begin(), attributes->end(), AttributeIsAnimationTime);

                if (it != attributes->end()) {
                    float val;
                    sscanf((*it)->Value().c_str(), "%f", &val);

                    if (m_HandSide == CutsceneHand::HANDEDNESS_RIGHT) {
                        _handstate18.m_fAnimTime = val;
                        m_RightHand.m_fAnimTime = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_LEFT) {
                        _handstate74.m_fAnimTime = val;
                        m_LeftHand.m_fAnimTime = val;
                    }
                    else if (m_HandSide == CutsceneHand::HANDEDNESS_BOTH) {
                        _handstate18.m_fAnimTime = val;
                        m_RightHand.m_fAnimTime = val;

                        _handstate74.m_fAnimTime = val;
                        m_LeftHand.m_fAnimTime = val;
                    }
                }
            }
        }

        static bool AttributeIsId(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_ID);
        }

        static bool AttributeIsName(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_NAME);
        }

        static bool AttributeIsModel(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_MODEL);
        }

        static bool AttributeIsRace(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_RACE);
        }

        static bool AttributeIsGender(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_GENDER);
        }

        static bool AttributeIsSide(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_SIDE);
        }

        static bool AttributeIsScaleX(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_SCALE_X);
        }

        static bool AttributeIsScaleY(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_SCALE_Y);
        }

        static bool AttributeIsScaleZ(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_SCALE_Z);
        }

        static bool AttributeIsRotateX(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_ROTATE_X);
        }

        static bool AttributeIsRotateY(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_ROTATE_Y);
        }

        static bool AttributeIsRotateZ(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_ROTATE_Z);
        }

        static bool AttributeIsPositionX(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_POSITION_X);
        }

        static bool AttributeIsPositionY(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_POSITION_Y);
        }

        static bool AttributeIsPositionZ(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_POSITION_Z);
        }

        static bool AttributeIsRed(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_RED);
        }

        static bool AttributeIsGreen(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_GREEN);
        }

        static bool AttributeIsBlue(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_BLUE);
        }

        static bool AttributeIsCutsceneStart(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_CUTSCENE_START);
        }

        static bool AttributeIsCutsceneEnd(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_CUTSCENE_END);
        }

        static bool AttributeIsAnimationTime(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_ANIMATION_TIME);
        }

        static bool AttributeIsAnimationType(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_ANIMATION_TYPE);
        }

        static bool AttributeIsAnimationStart(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_ANIMATION_START);
        }

        static bool AttributeIsAnimationEnd(XmlLibrary::Attribute* const& attribute) {
            return !stricmp(attribute->Name().c_str(), XML_ATTRIBUTE_ANIMATION_END);
        }
    };

    static inline std::array<std::array<CutsceneHand::HandInstance, 2>, NUM_INSTANCES> Instances;

#ifdef NO_HANDS_STREAMING
    static inline RpClump* m_pHandsClump = nullptr;
#endif

};	// class XboxHandsVC

}	// anonymous namespace


// ---------------------------------------------------------------------------
// Public interface. Every entry point is gated on ms_enabled, which Initialise()
// only sets once ALL assets loaded -- a missing file just leaves the feature off.
// ---------------------------------------------------------------------------

void
CutsceneHands::Initialise(void)
{
	if(XboxHandsVC::m_pHandsClump != nil)	// already initialised (new-game re-entry)
		return;

	ms_enabled = false;

	// Hand textures.
	int slot = CTxdStore::FindTxdSlot("hands");
	if(slot == -1)
		slot = CTxdStore::AddTxdSlot("hands");
	if(!CTxdStore::LoadTxd(slot, "anim/CSHands.txd")){
		debug("CutsceneHands: anim/CSHands.txd missing - HD hands disabled\n");
		return;
	}
	CTxdStore::AddRef(slot);
	CTxdStore::SetCurrentTxd(slot);

	// Hand model (holds every palm atomic).
	XboxHandsVC::m_pHandsClump = CutsceneHands_LoadClump("anim/CSHands.dff");
	if(XboxHandsVC::m_pHandsClump == nil){
		debug("CutsceneHands: anim/CSHands.dff missing - HD hands disabled\n");
		return;
	}

	// XML config + the four .anm animations.
	XboxHandsVC::CutsceneHand::Init();
	if(XboxHandsVC::g_parser == nil || XboxHandsVC::g_parser->HadError()){
		debug("CutsceneHands: data/CutsceneHands.xml missing or malformed - HD hands disabled\n");
		return;
	}
	for(int g = 0; g < 2; g++)
		for(int h = 0; h < 2; h++)
			if(XboxHandsVC::CutsceneHand::m_Animation[g][h].m_pAnim == nil){
				debug("CutsceneHands: an anim/*.anm file failed to load - HD hands disabled\n");
				return;
			}

	ms_enabled = true;
	debug("CutsceneHands: HD Xbox cutscene hands enabled\n");
}

void
CutsceneHands::Shutdown(void)
{
	XboxHandsVC::CutsceneHand::CleanUp();

	int slot = CTxdStore::FindTxdSlot("hands");
	if(slot != -1)
		CTxdStore::RemoveTxdSlot(slot);

	if(XboxHandsVC::m_pHandsClump){
		RpClumpDestroy(XboxHandsVC::m_pHandsClump);
		XboxHandsVC::m_pHandsClump = nil;
	}
	ms_enabled = false;
}

void
CutsceneHands::InitXML(void)
{
	if(!ms_enabled) return;
	XboxHandsVC::CutsceneHand::InitXML();
}

void
CutsceneHands::RequestSpecialChar(int slot, const char *name)
{
	if(!ms_enabled) return;
	XboxHandsVC::CutsceneHand::Request(slot + 1, name);
}

void
CutsceneHands::SpecialCharLoaded(int id)
{
	if(!ms_enabled) return;
	XboxHandsVC::CutsceneHand::HasLoaded(id + 1);
}

void
CutsceneHands::UnloadSpecialChar(int id)
{
	if(!ms_enabled) return;
	XboxHandsVC::CutsceneHand::Unload(id + 1);
}

void
CutsceneHands::PreRender(CObject *obj)
{
	if(!ms_enabled) return;
	XboxHandsVC::CutsceneHand::PreRender(obj);
}

void
CutsceneHands::Render(CObject *obj)
{
	if(!ms_enabled) return;
	int index = obj->GetModelIndex() - XboxHandsVC::SPECIAL_CHAR_OFFSET;
	if(XboxHandsVC::CutsceneHand::IsSpecialCharacter(index)){
		XboxHandsVC::CutsceneHand::Render(index, XboxHandsVC::CutsceneHand::HANDEDNESS_LEFT);
		XboxHandsVC::CutsceneHand::Render(index, XboxHandsVC::CutsceneHand::HANDEDNESS_RIGHT);
	}
}

#endif	// CUTSCENE_HANDS
