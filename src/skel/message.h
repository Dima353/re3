#ifndef MESSAGE_H
#define MESSAGE_H

#include <array>
#include <cstdint>

#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>

#ifndef ALIGN
#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#endif // ALIGN

struct DisplayBuffer {
	void *data;
	SceGxmSyncObject *sync;
	SceGxmColorSurface surf;
	SceUID uid;
}; // struct DisplayBuffer

class GxmDrawer {
public:
	GxmDrawer();
	~GxmDrawer();

	void gxmSwap();
	void clearScreenBuffer() const;
	void updateSceDialog() const;

	static void *dramAlloc(std::uint32_t size, SceUID *uid);
	static void gxmVsyncCb(const void *callbackData);

private:
	static constexpr int DISPLAY_WIDTH{960};
	static constexpr int DISPLAY_HEIGHT{544};
	static constexpr int DISPLAY_STRIDE_IN_PIXELS{1024};
	static constexpr int DISPLAY_BUFFER_COUNT{2};
	static constexpr int DISPLAY_MAX_PENDING_SWAPS{1};

	std::uint32_t backBufferIndex_{}, frontBufferIndex_{};
	std::array<DisplayBuffer, DISPLAY_BUFFER_COUNT> displayBuf_{};
}; // class GxmDrawer

void showMsgDialog(const char *message);
int fileExists(const char *path);

#endif //MESSAGE_H
