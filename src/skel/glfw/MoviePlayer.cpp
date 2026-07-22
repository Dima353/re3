// Nintendo Switch / GLFW movie player. Ported from the SDL2 build; only the
// windowing-system glue differs (GLFW proc address / buffer swap / timer).
//
// The PS Vita gets its own backend further down, built on SceAvPlayer, behind the
// same interface. Windows D3D9 builds play the intro through the win.cpp skeleton
// (DirectShow), and the remaining GLFW targets never played it at all - the MPEG
// states there have always just fallen through. Those get the stub at the bottom of
// this file, which keeps glfw.cpp's intro state machine linkable without dragging
// pl_mpeg and OpenAL into their builds.
#if defined __SWITCH__ && defined RW_GL3 && !defined LIBRW_SDL2 && defined AUDIO_OAL

#include "common.h"

// GLFW_INCLUDE_NONE: don't let GLFW pull its own GL headers - librw already
// provides GL symbols through glad, and GLFWwindow is needed by crossplatform.h.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "MoviePlayer.h"
#include "crossplatform.h"
#include "skeleton.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PL_MPEG_IMPLEMENTATION
#include "vendor/pl_mpeg/pl_mpeg.h"

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef long GLsizeiptr;
typedef long GLintptr;
typedef char GLchar;
typedef unsigned int GLbitfield;

typedef void (*PFN_glGenTextures)(GLsizei n, GLuint *textures);
typedef void (*PFN_glBindTexture)(GLenum target, GLuint texture);
typedef void (*PFN_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
	GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (*PFN_glTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef GLuint (*PFN_glCreateShader)(GLenum type);
typedef void (*PFN_glShaderSource)(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
typedef void (*PFN_glCompileShader)(GLuint shader);
typedef void (*PFN_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
typedef void (*PFN_glGetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef GLuint (*PFN_glCreateProgram)(void);
typedef void (*PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void (*PFN_glLinkProgram)(GLuint program);
typedef void (*PFN_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
typedef void (*PFN_glGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
typedef void (*PFN_glUseProgram)(GLuint program);
typedef GLint (*PFN_glGetAttribLocation)(GLuint program, const GLchar *name);
typedef GLint (*PFN_glGetUniformLocation)(GLuint program, const GLchar *name);
typedef void (*PFN_glUniform1i)(GLint location, GLint v0);
typedef void (*PFN_glGenBuffers)(GLsizei n, GLuint *buffers);
typedef void (*PFN_glBindBuffer)(GLenum target, GLuint buffer);
typedef void (*PFN_glBufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (*PFN_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (*PFN_glEnableVertexAttribArray)(GLuint index);
typedef void (*PFN_glDisableVertexAttribArray)(GLuint index);
typedef void (*PFN_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void (*PFN_glActiveTexture)(GLenum texture);
typedef void (*PFN_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (*PFN_glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (*PFN_glClear)(GLbitfield mask);
typedef void (*PFN_glDisable)(GLenum cap);
typedef void (*PFN_glGetIntegerv)(GLenum pname, GLint *data);
typedef const unsigned char *(*PFN_glGetString)(GLenum name);
typedef void (*PFN_glGenVertexArrays)(GLsizei n, GLuint *arrays);
typedef void (*PFN_glBindVertexArray)(GLuint array);
typedef void (*PFN_glEnable)(GLenum cap);
typedef unsigned char (*PFN_glIsEnabled)(GLenum cap);

namespace
{
	PFN_glGenTextures glGenTextures_ = nullptr;
	PFN_glBindTexture glBindTexture_ = nullptr;
	PFN_glTexImage2D glTexImage2D_ = nullptr;
	PFN_glTexParameteri glTexParameteri_ = nullptr;
	PFN_glCreateShader glCreateShader_ = nullptr;
	PFN_glShaderSource glShaderSource_ = nullptr;
	PFN_glCompileShader glCompileShader_ = nullptr;
	PFN_glGetShaderiv glGetShaderiv_ = nullptr;
	PFN_glGetShaderInfoLog glGetShaderInfoLog_ = nullptr;
	PFN_glCreateProgram glCreateProgram_ = nullptr;
	PFN_glAttachShader glAttachShader_ = nullptr;
	PFN_glLinkProgram glLinkProgram_ = nullptr;
	PFN_glGetProgramiv glGetProgramiv_ = nullptr;
	PFN_glGetProgramInfoLog glGetProgramInfoLog_ = nullptr;
	PFN_glUseProgram glUseProgram_ = nullptr;
	PFN_glGetAttribLocation glGetAttribLocation_ = nullptr;
	PFN_glGetUniformLocation glGetUniformLocation_ = nullptr;
	PFN_glUniform1i glUniform1i_ = nullptr;
	PFN_glGenBuffers glGenBuffers_ = nullptr;
	PFN_glBindBuffer glBindBuffer_ = nullptr;
	PFN_glBufferData glBufferData_ = nullptr;
	PFN_glVertexAttribPointer glVertexAttribPointer_ = nullptr;
	PFN_glEnableVertexAttribArray glEnableVertexAttribArray_ = nullptr;
	PFN_glDisableVertexAttribArray glDisableVertexAttribArray_ = nullptr;
	PFN_glDrawArrays glDrawArrays_ = nullptr;
	PFN_glActiveTexture glActiveTexture_ = nullptr;
	PFN_glViewport glViewport_ = nullptr;
	PFN_glClearColor glClearColor_ = nullptr;
	PFN_glClear glClear_ = nullptr;
	PFN_glDisable glDisable_ = nullptr;
	PFN_glGetIntegerv glGetIntegerv_ = nullptr;
	PFN_glGetString glGetString_ = nullptr;
	// Vertex Array Object entry points. Optional: absent on plain GLES2, but
	// required to draw on a desktop GL core profile (which switch-mesa gives us).
	PFN_glGenVertexArrays glGenVertexArrays_ = nullptr;
	PFN_glBindVertexArray glBindVertexArray_ = nullptr;
	PFN_glEnable glEnable_ = nullptr;
	PFN_glIsEnabled glIsEnabled_ = nullptr;
	bool g_glLoaded = false;

	template<typename T>
	bool LoadOneGL(const char *name, T &fn)
	{
		fn = (T)glfwGetProcAddress(name);
		return fn != nullptr;
	}

	bool LoadGL()
	{
		if (g_glLoaded)
			return true;
		bool ok = true;
		ok &= LoadOneGL("glGenTextures", glGenTextures_);
		ok &= LoadOneGL("glBindTexture", glBindTexture_);
		ok &= LoadOneGL("glTexImage2D", glTexImage2D_);
		ok &= LoadOneGL("glTexParameteri", glTexParameteri_);
		ok &= LoadOneGL("glCreateShader", glCreateShader_);
		ok &= LoadOneGL("glShaderSource", glShaderSource_);
		ok &= LoadOneGL("glCompileShader", glCompileShader_);
		ok &= LoadOneGL("glGetShaderiv", glGetShaderiv_);
		ok &= LoadOneGL("glGetShaderInfoLog", glGetShaderInfoLog_);
		ok &= LoadOneGL("glCreateProgram", glCreateProgram_);
		ok &= LoadOneGL("glAttachShader", glAttachShader_);
		ok &= LoadOneGL("glLinkProgram", glLinkProgram_);
		ok &= LoadOneGL("glGetProgramiv", glGetProgramiv_);
		ok &= LoadOneGL("glGetProgramInfoLog", glGetProgramInfoLog_);
		ok &= LoadOneGL("glUseProgram", glUseProgram_);
		ok &= LoadOneGL("glGetAttribLocation", glGetAttribLocation_);
		ok &= LoadOneGL("glGetUniformLocation", glGetUniformLocation_);
		ok &= LoadOneGL("glUniform1i", glUniform1i_);
		ok &= LoadOneGL("glGenBuffers", glGenBuffers_);
		ok &= LoadOneGL("glBindBuffer", glBindBuffer_);
		ok &= LoadOneGL("glBufferData", glBufferData_);
		ok &= LoadOneGL("glVertexAttribPointer", glVertexAttribPointer_);
		ok &= LoadOneGL("glEnableVertexAttribArray", glEnableVertexAttribArray_);
		ok &= LoadOneGL("glDisableVertexAttribArray", glDisableVertexAttribArray_);
		ok &= LoadOneGL("glDrawArrays", glDrawArrays_);
		ok &= LoadOneGL("glActiveTexture", glActiveTexture_);
		ok &= LoadOneGL("glViewport", glViewport_);
		ok &= LoadOneGL("glClearColor", glClearColor_);
		ok &= LoadOneGL("glClear", glClear_);
		ok &= LoadOneGL("glDisable", glDisable_);
		ok &= LoadOneGL("glGetIntegerv", glGetIntegerv_);
		ok &= LoadOneGL("glGetString", glGetString_);
		ok &= LoadOneGL("glEnable", glEnable_);
		ok &= LoadOneGL("glIsEnabled", glIsEnabled_);
		// VAO funcs are optional (GLES2 has none) - don't fail LoadGL if missing.
		LoadOneGL("glGenVertexArrays", glGenVertexArrays_);
		LoadOneGL("glBindVertexArray", glBindVertexArray_);
		g_glLoaded = ok;
		if (!g_glLoaded)
			printf("MoviePlayer: failed to resolve one or more GL functions via glfwGetProcAddress\n");
		return g_glLoaded;
	}
}

#define GL_TEXTURE_2D         0x0DE1
#define GL_TEXTURE0           0x84C0
#define GL_RGBA               0x1908
#define GL_UNSIGNED_BYTE      0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR             0x2601
#define GL_VERTEX_SHADER      0x8B31
#define GL_FRAGMENT_SHADER    0x8B30
#define GL_COMPILE_STATUS     0x8B81
#define GL_LINK_STATUS        0x8B82
#define GL_ARRAY_BUFFER       0x8892
#define GL_STATIC_DRAW        0x88E4
#define GL_FLOAT              0x1406
#define GL_FALSE              0
#define GL_TRUE               1
#define GL_TRIANGLE_STRIP     0x0005
#define GL_DEPTH_TEST         0x0B71
#define GL_CULL_FACE          0x0B44
#define GL_BLEND              0x0BE2
#define GL_COLOR_BUFFER_BIT   0x4000
#define GL_VIEWPORT           0x0BA2
#define GL_VERSION            0x1F02
#define GL_CURRENT_PROGRAM       0x8B8D
#define GL_ARRAY_BUFFER_BINDING  0x8894
#define GL_VERTEX_ARRAY_BINDING  0x85B5
#define GL_ACTIVE_TEXTURE        0x84E0
#define GL_TEXTURE_BINDING_2D    0x8069

namespace
{
	enum class State { Inactive, Active, Stopping };
	State g_state = State::Inactive;

	plm_t *g_plm = nullptr;
	uint8_t *g_rgbaBuffer = nullptr;
	int g_videoWidth = 0;
	int g_videoHeight = 0;
	double g_frameDuration = 1.0 / 30.0; // seconds per video frame, from plm_get_framerate()

	ALCdevice *g_alDevice = nullptr;
	ALCcontext *g_alContext = nullptr;
	ALuint g_alSource = 0;
	static const int kNumAudioBuffers = 4;
	ALuint g_alBuffers[kNumAudioBuffers] = { 0, 0, 0, 0 };
	bool g_audioReady = false;
	int g_audioSampleRate = 48000;
	double g_audioFrameDuration = (double)PLM_AUDIO_SAMPLES_PER_FRAME / 48000.0; // seconds per audio frame
	double g_audioAccumulator = 0.0;
	int g_nextFreeBuffer = 0;

	double g_lastTime = 0.0; // seconds, from glfwGetTime()
	double g_accumulator = 0.0;

	bool InitAudio()
	{
		g_alDevice = alcOpenDevice(nullptr); // default device
		if (!g_alDevice) {
			printf("MoviePlayer: alcOpenDevice failed, continuing without audio\n");
			return false;
		}

		g_alContext = alcCreateContext(g_alDevice, nullptr);
		if (!g_alContext) {
			printf("MoviePlayer: alcCreateContext failed, continuing without audio\n");
			alcCloseDevice(g_alDevice);
			g_alDevice = nullptr;
			return false;
		}

		alcMakeContextCurrent(g_alContext);

		alGenSources(1, &g_alSource);
		alGenBuffers(kNumAudioBuffers, g_alBuffers);

		g_nextFreeBuffer = 0;
		g_audioReady = true;
		return true;
	}

	void CloseAudio()
	{
		if (!g_audioReady)
			return;

		if (g_alSource) {
			alSourceStop(g_alSource);
			ALint queued = 0;
			alGetSourcei(g_alSource, AL_BUFFERS_QUEUED, &queued);
			while (queued-- > 0) {
				ALuint buf;
				alSourceUnqueueBuffers(g_alSource, 1, &buf);
			}
			alDeleteSources(1, &g_alSource);
			g_alSource = 0;
		}

		alDeleteBuffers(kNumAudioBuffers, g_alBuffers);
		for (int i = 0; i < kNumAudioBuffers; i++)
			g_alBuffers[i] = 0;

		alcMakeContextCurrent(nullptr);
		if (g_alContext) { alcDestroyContext(g_alContext); g_alContext = nullptr; }
		if (g_alDevice) { alcCloseDevice(g_alDevice); g_alDevice = nullptr; }

		g_audioReady = false;
	}

	void QueueAudioFrame(plm_samples_t *samples)
	{
		if (!g_audioReady)
			return;

		static int16_t pcm[PLM_AUDIO_SAMPLES_PER_FRAME * 2];
		for (int i = 0; i < PLM_AUDIO_SAMPLES_PER_FRAME * 2; i++) {
			float s = samples->interleaved[i];
			if (s > 1.0f) s = 1.0f;
			if (s < -1.0f) s = -1.0f;
			pcm[i] = (int16_t)(s * 32767.0f);
		}

		ALuint buffer = 0;
		ALint processed = 0;
		alGetSourcei(g_alSource, AL_BUFFERS_PROCESSED, &processed);
		if (processed > 0) {
			alSourceUnqueueBuffers(g_alSource, 1, &buffer);
		} else {
			if (g_nextFreeBuffer >= kNumAudioBuffers)
				return;
			buffer = g_alBuffers[g_nextFreeBuffer++];
		}

		alBufferData(buffer, AL_FORMAT_STEREO16, pcm, sizeof(pcm), (ALsizei)g_audioSampleRate);
		alSourceQueueBuffers(g_alSource, 1, &buffer);

		ALint state = 0;
		alGetSourcei(g_alSource, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING)
			alSourcePlay(g_alSource);
	}

	void CloseMovie()
	{
		if (g_plm) { plm_destroy(g_plm); g_plm = nullptr; }
		if (g_rgbaBuffer) { free(g_rgbaBuffer); g_rgbaBuffer = nullptr; }
		CloseAudio();
		g_videoWidth = 0;
		g_videoHeight = 0;
		g_state = State::Inactive;
	}

	// Two shader variants. librw hands us either a desktop GL core context
	// (switch-mesa gives GL 3.3 core) or a GLES context, and the two speak
	// different GLSL dialects. Without an explicit #version the source defaults
	// to GLSL 110, which a strict core-profile driver rejects - that is why the
	// movie played with sound but no picture. Pick the dialect at runtime.
	const char *kVertexShaderGL =
		"#version 150\n"
		"in vec2 aPos;\n"
		"in vec2 aTexCoord;\n"
		"out vec2 vTexCoord;\n"
		"void main() {\n"
		"    vTexCoord = aTexCoord;\n"
		"    gl_Position = vec4(aPos, 0.0, 1.0);\n"
		"}\n";

	const char *kFragmentShaderGL =
		"#version 150\n"
		"in vec2 vTexCoord;\n"
		"out vec4 FragColor;\n"
		"uniform sampler2D uTexture;\n"
		"void main() {\n"
		"    FragColor = texture(uTexture, vTexCoord);\n"
		"}\n";

	const char *kVertexShaderES =
		"#version 100\n"
		"attribute vec2 aPos;\n"
		"attribute vec2 aTexCoord;\n"
		"varying vec2 vTexCoord;\n"
		"void main() {\n"
		"    vTexCoord = aTexCoord;\n"
		"    gl_Position = vec4(aPos, 0.0, 1.0);\n"
		"}\n";

	const char *kFragmentShaderES =
		"#version 100\n"
		"precision mediump float;\n"
		"varying vec2 vTexCoord;\n"
		"uniform sampler2D uTexture;\n"
		"void main() {\n"
		"    gl_FragColor = texture2D(uTexture, vTexCoord);\n"
		"}\n";

	GLuint g_program = 0;
	GLuint g_vbo = 0;
	GLuint g_vao = 0;
	GLuint g_texture = 0;
	GLint g_aPosLoc = -1;
	GLint g_aTexCoordLoc = -1;
	GLint g_uTextureLoc = -1;
	bool g_shaderReady = false;

	GLuint CompileShader(GLenum type, const char *src)
	{
		GLuint shader = glCreateShader_(type);
		glShaderSource_(shader, 1, &src, nullptr);
		glCompileShader_(shader);
		GLint status = 0;
		glGetShaderiv_(shader, GL_COMPILE_STATUS, &status);
		if (status == GL_FALSE) {
			char log[512];
			glGetShaderInfoLog_(shader, sizeof(log), nullptr, log);
			printf("MoviePlayer: shader compile error: %s\n", log);
		}
		return shader;
	}

	bool InitShader()
	{
		if (g_shaderReady)
			return true;

		const char *ver = glGetString_ ? (const char *)glGetString_(GL_VERSION) : nullptr;
		bool isGLES = ver && strstr(ver, "OpenGL ES") != nullptr;

		GLuint vs = CompileShader(GL_VERTEX_SHADER, isGLES ? kVertexShaderES : kVertexShaderGL);
		GLuint fs = CompileShader(GL_FRAGMENT_SHADER, isGLES ? kFragmentShaderES : kFragmentShaderGL);

		g_program = glCreateProgram_();
		glAttachShader_(g_program, vs);
		glAttachShader_(g_program, fs);
		glLinkProgram_(g_program);

		GLint linkStatus = 0;
		glGetProgramiv_(g_program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus == GL_FALSE) {
			char log[512];
			glGetProgramInfoLog_(g_program, sizeof(log), nullptr, log);
			printf("MoviePlayer: shader link error: %s\n", log);
			return false;
		}

		g_aPosLoc = glGetAttribLocation_(g_program, "aPos");
		g_aTexCoordLoc = glGetAttribLocation_(g_program, "aTexCoord");
		g_uTextureLoc = glGetUniformLocation_(g_program, "uTexture");

		const GLfloat verts[] = {
			// x,    y,     u,    v
			-1.0f, -1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 1.0f,
			-1.0f,  1.0f,  0.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 0.0f,
		};

		// A core-profile context can't draw without a bound VAO, and binding our
		// own also stops us from trampling the VAO librw left bound.
		if (glGenVertexArrays_ && glBindVertexArray_)
			glGenVertexArrays_(1, &g_vao);

		glGenBuffers_(1, &g_vbo);
		glBindBuffer_(GL_ARRAY_BUFFER, g_vbo);
		glBufferData_(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

		g_shaderReady = true;
		return true;
	}

	void DrawCurrentFrame()
	{
		if (!LoadGL() || g_rgbaBuffer == nullptr)
			return;

		// Save every GL state we are about to touch. librw keeps a render-state
		// cache that mirrors what it last set; the current GL state IS that cache.
		// If we leave GL changed, librw skips the "redundant" re-set and renders
		// nothing after the movie (menu invisible, last frame frozen). Restoring
		// GL to what we found keeps librw's cache valid.
		GLint sProgram = 0, sArrayBuf = 0, sVao = 0, sActiveTex = 0, sTexBind = 0;
		GLint sViewport[4] = { 0, 0, 0, 0 };
		glGetIntegerv_(GL_CURRENT_PROGRAM, &sProgram);
		glGetIntegerv_(GL_ARRAY_BUFFER_BINDING, &sArrayBuf);
		if (glBindVertexArray_)
			glGetIntegerv_(GL_VERTEX_ARRAY_BINDING, &sVao);
		glGetIntegerv_(GL_ACTIVE_TEXTURE, &sActiveTex);
		glGetIntegerv_(GL_VIEWPORT, sViewport);
		unsigned char sDepth = glIsEnabled_(GL_DEPTH_TEST);
		unsigned char sCull  = glIsEnabled_(GL_CULL_FACE);
		unsigned char sBlend = glIsEnabled_(GL_BLEND);

		glActiveTexture_(GL_TEXTURE0);
		glGetIntegerv_(GL_TEXTURE_BINDING_2D, &sTexBind);

		if (g_texture == 0)
			glGenTextures_(1, &g_texture);

		glBindTexture_(GL_TEXTURE_2D, g_texture);
		glTexImage2D_(GL_TEXTURE_2D, 0, GL_RGBA, g_videoWidth, g_videoHeight,
			0, GL_RGBA, GL_UNSIGNED_BYTE, g_rgbaBuffer);
		glTexParameteri_(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri_(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (!InitShader()) {
			// Restore the texture binding we disturbed before bailing.
			glBindTexture_(GL_TEXTURE_2D, (GLuint)sTexBind);
			glActiveTexture_((GLenum)sActiveTex);
			return;
		}

		// Size the viewport from the actual framebuffer. At this early boot stage
		// the cached GL_VIEWPORT may still be 0x0 (nothing has rendered yet).
		int fbWidth = 0, fbHeight = 0;
		glfwGetFramebufferSize(PSGLOBAL(window), &fbWidth, &fbHeight);
		glViewport_(0, 0, fbWidth, fbHeight);

		glDisable_(GL_DEPTH_TEST);
		glDisable_(GL_CULL_FACE);
		glDisable_(GL_BLEND);

		glClearColor_(0.0f, 0.0f, 0.0f, 1.0f);
		glClear_(GL_COLOR_BUFFER_BIT);

		glUseProgram_(g_program);

		if (glBindVertexArray_)
			glBindVertexArray_(g_vao);

		glBindTexture_(GL_TEXTURE_2D, g_texture);
		glUniform1i_(g_uTextureLoc, 0);

		glBindBuffer_(GL_ARRAY_BUFFER, g_vbo);

		glEnableVertexAttribArray_((GLuint)g_aPosLoc);
		glVertexAttribPointer_((GLuint)g_aPosLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)0);

		glEnableVertexAttribArray_((GLuint)g_aTexCoordLoc);
		glVertexAttribPointer_((GLuint)g_aTexCoordLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const void *)(2 * sizeof(GLfloat)));

		glDrawArrays_(GL_TRIANGLE_STRIP, 0, 4);

		glDisableVertexAttribArray_((GLuint)g_aPosLoc);
		glDisableVertexAttribArray_((GLuint)g_aTexCoordLoc);

		glfwSwapBuffers(PSGLOBAL(window));

		// Restore everything so librw's state cache stays in sync.
		if (glBindVertexArray_)
			glBindVertexArray_((GLuint)sVao);
		glUseProgram_((GLuint)sProgram);
		glBindBuffer_(GL_ARRAY_BUFFER, (GLuint)sArrayBuf);
		glBindTexture_(GL_TEXTURE_2D, (GLuint)sTexBind);
		glActiveTexture_((GLenum)sActiveTex);
		glViewport_(sViewport[0], sViewport[1], sViewport[2], sViewport[3]);
		if (sDepth) glEnable_(GL_DEPTH_TEST);
		if (sCull)  glEnable_(GL_CULL_FACE);
		if (sBlend) glEnable_(GL_BLEND);
	}
}

void MoviePlayer::Play(const char *path)
{
	if (g_state != State::Inactive)
		CloseMovie();

	char *realPath = casepath(path);
	const char *openPath = realPath ? realPath : path;

	g_plm = plm_create_with_filename(openPath);
	if (realPath)
		free(realPath);

	if (!g_plm) {
		printf("MoviePlayer: failed to open %s\n", path);
		return;
	}

	if (!plm_has_headers(g_plm)) {
		printf("MoviePlayer: %s has no valid MPEG-PS headers\n", path);
		plm_destroy(g_plm);
		g_plm = nullptr;
		return;
	}

	g_videoWidth = plm_get_width(g_plm);
	g_videoHeight = plm_get_height(g_plm);

	if (g_videoWidth <= 0 || g_videoHeight <= 0) {
		printf("MoviePlayer: %s has no video stream\n", path);
		plm_destroy(g_plm);
		g_plm = nullptr;
		return;
	}

	g_rgbaBuffer = (uint8_t *)malloc((size_t)g_videoWidth * (size_t)g_videoHeight * 4);
	g_frameDuration = 1.0 / plm_get_framerate(g_plm);

	g_audioSampleRate = plm_get_samplerate(g_plm);
	if (g_audioSampleRate > 0 && InitAudio()) {
		plm_set_audio_enabled(g_plm, TRUE);
		plm_set_audio_stream(g_plm, 0);
		g_audioFrameDuration = (double)PLM_AUDIO_SAMPLES_PER_FRAME / (double)g_audioSampleRate;
		g_audioAccumulator = 0.0;
	} else {
		plm_set_audio_enabled(g_plm, FALSE);
	}

	printf("MoviePlayer: playing %s (%dx%d, %.2f fps, duration %.2fs, audio=%s)\n",
		path, g_videoWidth, g_videoHeight, plm_get_framerate(g_plm), plm_get_duration(g_plm),
		g_audioReady ? "yes" : "no");

	g_lastTime = glfwGetTime();
	g_accumulator = 0.0;
	g_state = State::Active;
}

bool MoviePlayer::IsActive()
{
	return g_state != State::Inactive;
}

void MoviePlayer::Stop()
{
	if (g_state == State::Active)
		g_state = State::Stopping;
}

void MoviePlayer::Draw()
{
	if (g_state == State::Inactive)
		return;

	if (g_state == State::Stopping) {
		CloseMovie();
		return;
	}

	double now = glfwGetTime();
	double elapsed = now - g_lastTime;
	g_lastTime = now;
	g_accumulator += elapsed;

	const double kMaxAccumulator = g_frameDuration * 4.0;
	if (g_accumulator > kMaxAccumulator)
		g_accumulator = kMaxAccumulator;

	const int kMaxFramesPerCall = 4;
	int framesDecoded = 0;
	while (g_accumulator >= g_frameDuration && framesDecoded < kMaxFramesPerCall) {
		plm_frame_t *frame = plm_decode_video(g_plm);
		if (frame == nullptr)
			break;

		plm_frame_to_rgba(frame, g_rgbaBuffer, g_videoWidth * 4);
		DrawCurrentFrame();

		g_accumulator -= g_frameDuration;
		framesDecoded++;
	}

	if (g_audioReady) {
		g_audioAccumulator += elapsed;
		const double kMaxAudioAccumulator = g_audioFrameDuration * 8.0;
		if (g_audioAccumulator > kMaxAudioAccumulator)
			g_audioAccumulator = kMaxAudioAccumulator;

		const int kMaxAudioFramesPerCall = 8;
		int audioFramesDecoded = 0;
		while (g_audioAccumulator >= g_audioFrameDuration && audioFramesDecoded < kMaxAudioFramesPerCall) {
			plm_samples_t *samples = plm_decode_audio(g_plm);
			if (samples == nullptr)
				break;
			QueueAudioFrame(samples);
			g_audioAccumulator -= g_audioFrameDuration;
			audioFramesDecoded++;
		}
	}

	if (plm_has_ended(g_plm)) {
		printf("MoviePlayer: movie finished\n");
		g_state = State::Stopping;
	}
}

#elif defined PSP2

// PS Vita backend. Same interface, different everything else: decoding is done by
// SceAvPlayer rather than pl_mpeg, the frames arrive as GXM textures that are wrapped
// into GL ones, and audio is pumped by a dedicated thread instead of OpenAL.
// Lifted out of glfw.cpp, where it used to sit inline alongside the intro state machine.

#include "common.h"
#include <malloc.h>	// memalign, for the SceAvPlayer allocator callbacks
#include <vitasdk.h>
#include <vitaGL.h>
#include "crossplatform.h"
#include "MoviePlayer.h"

#define FB_ALIGNMENT 0x40000
#define ALIGN_MEM(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

SceAvPlayerHandle movie_player;

GLuint movie_frame[2];
uint8_t movie_frame_idx = 0;
SceGxmTexture *movie_tex[2];
GLuint movie_fs;
GLuint movie_vs;
GLuint movie_prog;

SceUID audio_thid;
int audio_new;
int audio_port;
int audio_len;
int audio_freq;
int audio_mode;

enum {
	PLAYER_INACTIVE,
	PLAYER_ACTIVE,
	PLAYER_STOP,
};

int player_state = PLAYER_INACTIVE;

float movie_pos[8] = {
  -1.0f, 1.0f,
  -1.0f, -1.0f,
   1.0f, 1.0f,
   1.0f, -1.0f
};

float movie_texcoord[8] = {
  0.0f, 0.0f,
  0.0f, 1.0f,
  1.0f, 0.0f,
  1.0f, 1.0f
};

void *mem_alloc(void *p, uint32_t align, uint32_t size) {
	return memalign(align, size);
}

void mem_free(void *p, void *ptr) {
	free(ptr);
}

void *gpu_alloc(void *p, uint32_t align, uint32_t size) {
	if (align < FB_ALIGNMENT) {
		align = FB_ALIGNMENT;
	}
	size = ALIGN_MEM(size, align);
	return vglAlloc(size, VGL_MEM_SLOW);
}

void gpu_free(void *p, void *ptr) {
	glFinish();
	vglFree(ptr);
}

void movie_player_audio_init(void) {
	audio_port = -1;
	for (int i = 0; i < 8; i++) {
		if (sceAudioOutGetConfig(i, SCE_AUDIO_OUT_CONFIG_TYPE_LEN) >= 0) {
			audio_port = i;
			break;
		}
	}

	if (audio_port == -1) {
		audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
		audio_new = 1;
	} else {
		audio_len = sceAudioOutGetConfig(audio_port, SCE_AUDIO_OUT_CONFIG_TYPE_LEN);
		audio_freq = sceAudioOutGetConfig(audio_port, SCE_AUDIO_OUT_CONFIG_TYPE_FREQ);
		audio_mode = sceAudioOutGetConfig(audio_port, SCE_AUDIO_OUT_CONFIG_TYPE_MODE);
		audio_new = 0;
	}
}

void movie_player_audio_shutdown(void) {
  if (audio_new) {
    sceAudioOutReleasePort(audio_port);
  } else {
    sceAudioOutSetConfig(audio_port, audio_len, audio_freq, (SceAudioOutMode)audio_mode);
  }
}

int movie_player_audio_thread(SceSize args, void *argp) {
  SceAvPlayerFrameInfo frame;
  memset(&frame, 0, sizeof(SceAvPlayerFrameInfo));

  while (player_state == PLAYER_ACTIVE && sceAvPlayerIsActive(movie_player)) {
    if (sceAvPlayerGetAudioData(movie_player, &frame)) {
      sceAudioOutSetConfig(audio_port, 1024, frame.details.audio.sampleRate, frame.details.audio.channelCount == 1 ? SCE_AUDIO_OUT_MODE_MONO : SCE_AUDIO_OUT_MODE_STEREO);
      sceAudioOutOutput(audio_port, frame.pData);
    } else {
      sceKernelDelayThread(1000);
    }
  }

  return sceKernelExitDeleteThread(0);
}

void movie_player_draw(void) {
	if (player_state == PLAYER_ACTIVE) {
		if (sceAvPlayerIsActive(movie_player)) {
			SceAvPlayerFrameInfo frame;
			if (sceAvPlayerGetVideoData(movie_player, &frame)) {
				movie_frame_idx = (movie_frame_idx + 1) % 2;
				sceGxmTextureInitLinear(
					movie_tex[movie_frame_idx],
					frame.pData,
					SCE_GXM_TEXTURE_FORMAT_YVU420P2_CSC1,
					frame.details.video.width,
					frame.details.video.height, 0);
				sceGxmTextureSetMinFilter(movie_tex[movie_frame_idx], SCE_GXM_TEXTURE_FILTER_LINEAR);
				sceGxmTextureSetMagFilter(movie_tex[movie_frame_idx], SCE_GXM_TEXTURE_FILTER_LINEAR);
				glUseProgram(movie_prog);
				glBindTexture(GL_TEXTURE_2D, movie_frame[movie_frame_idx]);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
				glEnableVertexAttribArray(0);
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &movie_pos[0]);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, &movie_texcoord[0]);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				vglSwapBuffers(GL_FALSE);
			}
		} else {
			player_state = PLAYER_STOP;
		}
	}

	if (player_state == PLAYER_STOP) {
		sceAvPlayerStop(movie_player);
		sceKernelWaitThreadEnd(audio_thid, NULL, NULL);
		sceAvPlayerClose(movie_player);
		movie_player_audio_shutdown();
		player_state = PLAYER_INACTIVE;
		glClear(GL_COLOR_BUFFER_BIT);
		vglSwapBuffers(GL_FALSE);
	}
}

int movie_player_inited = 0;
void movie_player_init() {
	if (movie_player_inited)
		return;
	
	sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);

	glGenTextures(2, movie_frame);
	for (int i = 0; i < 2; i++) {
		glBindTexture(GL_TEXTURE_2D, movie_frame[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 960, 544, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		movie_tex[i] = vglGetGxmTexture(GL_TEXTURE_2D);
		vglFree(vglGetTexDataPointer(GL_TEXTURE_2D));
	}

	// The precompiled GXP blobs in shaders/movie_[vf].h can no longer be fed to
	// glShaderBinary: it now expects a vitaGL-serialized blob, so unserialize_shader
	// reads the "GXP\0" magic as a uniform count and walks off into garbage.
	// Compile the original Cg source (kept as a comment in those headers) instead.
	static const char *movie_v_src =
		"void main(\n"
		"	float2 inPos,\n"
		"	float2 inTex,\n"
		"	float4 out pos : POSITION,\n"
		"	float2 out texcoord : TEXCOORD0\n"
		") {\n"
		"	texcoord = inTex;\n"
		"	pos = float4(inPos, 1.f, 1.f);\n"
		"}\n";
	static const char *movie_f_src =
		"float4 main(\n"
		"	float2 texcoord : TEXCOORD0,\n"
		"	uniform sampler2D tex\n"
		") {\n"
		"	return tex2D(tex, texcoord);\n"
		"}\n";

	movie_vs = glCreateShader(GL_CG_VERTEX_SHADER_EXT);
	glShaderSource(movie_vs, 1, &movie_v_src, NULL);
	glCompileShader(movie_vs);

	movie_fs = glCreateShader(GL_CG_FRAGMENT_SHADER_EXT);
	glShaderSource(movie_fs, 1, &movie_f_src, NULL);
	glCompileShader(movie_fs);

	movie_prog = glCreateProgram();
	glAttachShader(movie_prog, movie_vs);
	glAttachShader(movie_prog, movie_fs);
	glBindAttribLocation(movie_prog, 0, "inPos");
	glBindAttribLocation(movie_prog, 1, "inTex");
	glLinkProgram(movie_prog);
	
	movie_player_inited = 1;
}

void movie_player_play(const char *file) {
	SceIoStat st;
	if (sceIoGetstat(file, &st) < 0)
		return;
	
	movie_player_init();
	movie_player_audio_init();
	
	SceAvPlayerInitData playerInit;
	sceClibMemset(&playerInit, 0, sizeof(SceAvPlayerInitData));

	playerInit.memoryReplacement.allocate = mem_alloc;
	playerInit.memoryReplacement.deallocate = mem_free;
	playerInit.memoryReplacement.allocateTexture = gpu_alloc;
	playerInit.memoryReplacement.deallocateTexture = gpu_free;

	playerInit.basePriority = 0xA0;
	playerInit.numOutputVideoFrameBuffers = 2;
	playerInit.autoStart = GL_TRUE;

	movie_player = sceAvPlayerInit(&playerInit);

	sceAvPlayerAddSource(movie_player, file);

	audio_thid = sceKernelCreateThread("movie_audio_thread", movie_player_audio_thread, 0x10000100 - 10, 0x4000, 0, 0, NULL);
	sceKernelStartThread(audio_thid, 0, NULL);

	player_state = PLAYER_ACTIVE;
}

namespace MoviePlayer
{
	void Play(const char *path)
	{
		// The shared intro code asks for pl_mpeg's .mpg by a path relative to the
		// data directory; SceAvPlayer wants .mp4 and an absolute one.
		char rel[256];
		strncpy(rel, path, sizeof(rel) - 1);
		rel[sizeof(rel) - 1] = '\0';
		char *ext = strrchr(rel, '.');
		if (ext != nil)
			strcpy(ext, ".mp4");

		char full[PATH_MAX];
		_realpath(rel, full);

		// This build has always shipped one set of intro videos, so the localised
		// title reel the shared code asks for on French and German is not there.
		// Fall back to the plain one rather than silently dropping the intro.
		SceIoStat st;
		if (sceIoGetstat(full, &st) < 0) {
			char *ger = strstr(rel, "GER.mp4");
			if (ger != nil) {
				strcpy(ger, ".mp4");
				_realpath(rel, full);
			}
		}

		movie_player_play(full);
	}

	void Draw() { movie_player_draw(); }

	bool IsActive() { return player_state != PLAYER_INACTIVE; }

	void Stop()
	{
		if (player_state == PLAYER_ACTIVE)
			player_state = PLAYER_STOP;	// movie_player_draw does the teardown
	}
}

#else // !(RW_GL3 && !LIBRW_SDL2)

#include "MoviePlayer.h"

namespace MoviePlayer
{
	void Play(const char *) {}
	void Draw() {}
	bool IsActive() { return false; }
	void Stop() {}
}

#endif // RW_GL3 && !LIBRW_SDL2
