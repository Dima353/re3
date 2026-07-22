#include <cstdlib>

#include <psp2/display.h>
#include <psp2/message_dialog.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/clib.h>

#include "message.h"

GxmDrawer::GxmDrawer() {
    constexpr SceGxmInitializeParams params{
            0, DISPLAY_MAX_PENDING_SWAPS, gxmVsyncCb, sizeof(void *),
            SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE};
    sceGxmInitialize(&params);


    for (auto &buf: displayBuf_) {
        buf.data = dramAlloc(4 * DISPLAY_STRIDE_IN_PIXELS * DISPLAY_HEIGHT,
                            &buf.uid);
        sceGxmColorSurfaceInit(&buf.surf, SCE_GXM_COLOR_FORMAT_A8B8G8R8,
                                SCE_GXM_COLOR_SURFACE_LINEAR,
                                SCE_GXM_COLOR_SURFACE_SCALE_NONE,
                                SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
                                DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                DISPLAY_STRIDE_IN_PIXELS, buf.data);
        sceGxmSyncObjectCreate(&buf.sync);
    }
}

GxmDrawer::~GxmDrawer() {
    sceGxmTerminate();

    for (const auto &buf: displayBuf_) {
        sceGxmSyncObjectDestroy(buf.sync);
        sceKernelFreeMemBlock(buf.uid);
    }
}

void GxmDrawer::gxmSwap() {
    sceGxmPadHeartbeat(&displayBuf_[backBufferIndex_].surf,
                        displayBuf_[backBufferIndex_].sync);
    sceGxmDisplayQueueAddEntry(displayBuf_[frontBufferIndex_].sync,
                                displayBuf_[backBufferIndex_].sync,
                                &displayBuf_[backBufferIndex_].data);
    frontBufferIndex_ = backBufferIndex_;
    backBufferIndex_ = (backBufferIndex_ + 1) % DISPLAY_BUFFER_COUNT;
}

void GxmDrawer::clearScreenBuffer() const {
    sceClibMemset(displayBuf_[backBufferIndex_].data,
                    static_cast<int>(0xFF000000),
                    DISPLAY_HEIGHT * DISPLAY_STRIDE_IN_PIXELS * 4);
}

void GxmDrawer::updateSceDialog() const {
    const SceCommonDialogUpdateParam updateParam{
            {nullptr, displayBuf_[backBufferIndex_].data,
                static_cast<SceGxmColorSurfaceType>(0),
                static_cast<SceGxmColorFormat>(0), DISPLAY_WIDTH,
                DISPLAY_HEIGHT, DISPLAY_STRIDE_IN_PIXELS},
            displayBuf_[backBufferIndex_].sync};
    sceCommonDialogUpdate(&updateParam);
}

void *GxmDrawer::dramAlloc(const std::uint32_t size, SceUID *uid) {
    void *mem;
    *uid = sceKernelAllocMemBlock("gpu_mem",
                                    SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                    ALIGN(size, 256 * 1024), nullptr);
    sceKernelGetMemBlockBase(*uid, &mem);
    sceGxmMapMemory(mem, ALIGN(size, 256 * 1024),
                    static_cast<SceGxmMemoryAttribFlags>(
                            SCE_GXM_MEMORY_ATTRIB_READ |
                            SCE_GXM_MEMORY_ATTRIB_WRITE));
    return mem;
}

void GxmDrawer::gxmVsyncCb(const void *callbackData) {
    const SceDisplayFrameBuf fb{
            sizeof(SceDisplayFrameBuf),
            *static_cast<void **>(const_cast<void *>(callbackData)),
            DISPLAY_STRIDE_IN_PIXELS,
            0,
            DISPLAY_WIDTH,
            DISPLAY_HEIGHT};
    sceDisplaySetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME);
}

void showMsgDialog(const char *message) {
	GxmDrawer gxmDrawer;

	SceMsgDialogUserMessageParam msgParam;
	sceClibMemset(&msgParam, 0, sizeof(SceMsgDialogUserMessageParam));
	msgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	msgParam.msg = reinterpret_cast<const SceChar8 *>(message);

	SceMsgDialogParam param;
	sceMsgDialogParamInit(&param);
	param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	param.userMsgParam = &msgParam;

	constexpr SceCommonDialogConfigParam confParam{};
	sceCommonDialogSetConfigParam(&confParam);
	sceMsgDialogInit(&param);

	while (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
		gxmDrawer.clearScreenBuffer();
		gxmDrawer.updateSceDialog();

		gxmDrawer.gxmSwap();
		sceDisplayWaitVblankStart();
	}
	sceMsgDialogTerm();
}

int fileExists(const char *path) {
	SceIoStat stat;
	return sceIoGetstat(path, &stat) >= 0;
}
