#pragma once
#include "resource.h"
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/utils/mono-logger.h>

int LoadApp();
B_CAPI(MonoDomain *) AppGetMonoDomain();
B_CAPI(MonoAssembly *) AppGetMonoAssembly();
B_CAPI(MonoImage *) AppGetMonoImage();
int AppDisplayErr(LPWSTR str, int mb = MB_OK);
BOOL AppLoadMono();
B_CAPI(void) AppFreeMono();
BOOL AppLoadMonoICalls();

int AppRun();

void AppStartInit();
void AppExitRealloc();
void AppShowAnim(HWND hWnd);
void AppHideAnim(HWND hWnd);
BOOL WindowIsVisible(HWND hWnd);
void AppTop(HWND hWnd);
void AppOpenFile(MonoString * file);
void AppStartNewWhenExit();
void AppDrawRect(HDC hdc, int w, int h);

enum UpdateCallbackID
{
	OnStart,
	OnPause,
	OnContinue,
	OnStop,
};
enum RECORD_STATE
{
	NOT_BEGIN,
	RECORDING,
	SUSPENDED,
};

RECORD_STATE RecoderGetState();

typedef void(_cdecl*UpdateCallback)(UpdateCallbackID id, void*data);

BOOL AppRegisterHotKey(HWND hWnd, int i, int k1, int k2, int k3);

void AppUnRegisterAllHotKey(HWND hWnd);


LONG RecoderGetRecSec();
BOOL RecoderIsRecording();
BOOL RecoderIsInterrupt();
void RecoderDestroy();
void RecoderCreate();
void RecoderSetUpdateCallback(UpdateCallback updateCallback);
void RecoderPauseButton();
void RecoderStopButton();
BOOL RecoderStartButton();
void RecoderSetNextOutFileName(MonoString* file);
void RecoderSetOutFileDir(MonoString * dir);
MonoString* RecoderGetLastOutFileName();
MonoString * RecoderGetLastError();
void RecoderSetCaptureFrameRate(int rate);
void RecoderSetCaptureRect(int x, int y, int w, int h);
void RecoderSetRecMic(BOOL rmic);
BOOL RecoderGetRecMic();
void RecoderSetRecSound(BOOL rmic);
BOOL RecoderGetRecSound();
void RecoderSetRecQua(int rmic);
int RecoderGetRecQua();
void RecoderSetRecFormat(int format);
int RecoderGetRecFormat();
MonoString * GetDefExportDir();
