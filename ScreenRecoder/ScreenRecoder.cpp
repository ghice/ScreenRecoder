//ScreenRecoder.cpp: 定义应用程序的入口点。
//

#include "stdafx.h"
#include "ScreenRecoder.h"
#include <Shlwapi.h> 
#include <shellapi.h>
#include "StringHlp.h"
#include "AdHookApi.h"
#include "../ScreenRecoder.Core/IScreenAudioRecord_C.h"
#include <list>
#include <MMSystem.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "winmm.lib")

LPWSTR *szArgList;
int argCount;
HINSTANCE hInst;
MonoDomain *domain;
MonoAssembly *assembly;
MonoImage* image;
WCHAR startDir[MAX_PATH];
CHAR startDirA[MAX_PATH];
WCHAR inipath[MAX_PATH];
WCHAR defexportdir[MAX_PATH];
WCHAR defexportdir2[MAX_PATH];
CAdHookApi hook;
bool startNewWhenExit = false;
std::list<int> regedKey;

void myExitProcess(UINT exitcode)
{
	if (IsDebuggerPresent())
		DebugBreak();
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,  _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR    lpCmdLine, _In_ int nCmdShow)
{
	hInst = hInstance;
	int rs = -1;

	szArgList = CommandLineToArgvW(GetCommandLine(), &argCount);
	if (szArgList == NULL)
	{
		AppDisplayErr((LPWSTR)L"Unable to parse command line");
		return rs;
	}
	wcscpy_s(startDir, szArgList[0]);
	PathRemoveFileSpec(startDir);

	char*s = UnicodeToAnsi(startDir);
	strcpy_s(startDirA, s);
	StrDel(s);

	LoadLibrary(L"MonoPosixHelper.dll");

	hook.Add(L"kernel32.dll", "ExitProcess", (void*)myExitProcess);
	hook.BeginAll();

	rs = LoadApp();

	hook.EndAll();
	LocalFree(szArgList);
	return rs;
}

#define CMD_CASE(cmd) if(wcscmp(szArgList[1], cmd)==0)

int LoadApp()
{
	int rs = -1;
	bool needLoadApp = false;
	if (argCount == 1)
		needLoadApp = true;
	else
	{
		CMD_CASE(L"-test") {

		}
		CMD_CASE(L"-test2") {

		}
	}

	if (needLoadApp) {
		if (AppLoadMono())
		{
			AppLoadMonoICalls();
			rs = AppRun();
			AppFreeMono();
		}
		else AppDisplayErr((LPWSTR)L"加载主程序失败！");
	}
	return rs;
}

void BMonoPrintCallback(const char *string, mono_bool is_stdout)
{
	if (IsDebuggerPresent()) {
		OutputDebugStringA(string);
		OutputDebugStringA("\n");
	}
}
void BMonoPrintErrCallback(const char *string, mono_bool is_stdout)
{
	BMonoPrintCallback(string, is_stdout);
	if (MessageBoxA(NULL, string, "Mono Error", MB_ICONEXCLAMATION | MB_YESNO) == IDYES) {
		ExitProcess(-1);
	}
}
void BMonoLogCallback(const char *log_domain, const char *log_level, const char *message, mono_bool fatal, void *user_data)
{
	bool exit = false;
	if (fatal || strcmp(log_level, "error") == 0)
		if (MessageBoxA(NULL, message, "Mono Error", MB_ICONEXCLAMATION | MB_YESNO) == IDYES)
			exit = true;
	if (IsDebuggerPresent())
		OutputDebugStringA(FormatString("%s![%s] %s\n", log_domain, log_level, message).c_str());
	if (fatal || strcmp(log_level, "error") == 0 && exit) {
		ExitProcess(-1);
	}
}

B_CAPI(void) Test() {
	AppDisplayErr((LPWSTR)L"B_CAPI(void) Test()");
}
B_CAPI(void) WindowMove(HWND hWnd)
{
	ReleaseCapture();
	SendMessage(hWnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}
B_CAPI(void) WindowShow(HWND hWnd)
{
	if (!IsWindowVisible(hWnd))
		ShowWindow(hWnd, SW_SHOW);
}
B_CAPI(void) WindowHide(HWND hWnd)
{
	if (IsWindowVisible(hWnd))
		ShowWindow(hWnd, SW_HIDE);
}
MonoString* GetInIPath()
{
	MonoString* rs = NULL;
	WCHAR path[MAX_PATH];
	wcscpy_s(path, startDir);
	wcscat_s(path, L"\\ScreenRecoder.ini");
	char*utf8str = UnicodeToUtf8(path);
	rs = mono_string_new(domain, utf8str);
	StrDel(utf8str);
	return rs;
}
LPWSTR GetInIPath2()
{

	wcscpy_s(inipath, startDir);
	wcscat_s(inipath, L"\\ScreenRecoder.ini");
	return inipath;
}

B_CAPI(MonoDomain *) AppGetMonoDomain()
{
	return domain;
}
B_CAPI(MonoAssembly *) AppGetMonoAssembly()
{
	return assembly;
}
B_CAPI(MonoImage *) AppGetMonoImage()
{
	return image;
}
int AppDisplayErr(LPWSTR str, int mb)
{
	return MessageBox(NULL, L"屏幕录像 - 错误", str, mb);
}
BOOL AppLoadMono()
{
	DIRMAKER(monopath, L"\\mono\\lib");
	UTF8MAKER(monopath2, monopath);
	DIRMAKER(monoetcpath, L"\\mono\\etc");
	UTF8MAKER(monoetcpath2, monoetcpath);
	mono_set_dirs(monopath2, monoetcpath2);
	CHARDEL(monopath2);
	CHARDEL(monoetcpath2);

	//if (IsDebuggerPresent())
	//	mono_trace_set_level_string("debug");
	//else 
	mono_trace_set_level_string("warning");
	mono_trace_set_log_handler(BMonoLogCallback, NULL);
	mono_trace_set_print_handler(BMonoPrintCallback);
	mono_trace_set_printerr_handler(BMonoPrintCallback);

	DIRMAKER(csharppatth, L"\\ScreenRecoder.App.dll");
	UTF8MAKER(csharppatth2, csharppatth);
	if (_waccess(csharppatth, 0) == 0)
	{
		//获取应用域
		domain = mono_jit_init(csharppatth2);
		if (domain) {
			//加载程序集
			assembly = mono_domain_assembly_open(domain, csharppatth2);
			if (assembly) {
				image = mono_assembly_get_image(assembly);
				if (image)return 1;
			}
			else {
				wstring s(L"加载 "); s += csharppatth; s += L" 时发生错误。";
				MessageBox(0, s.c_str(), L"系统错误", MB_ICONERROR);
			}
		}
		else {
			wstring s(L"加载程序时发生错误。");
			MessageBox(0, s.c_str(), L"系统错误", MB_ICONERROR);
		}
	}
	else {
		wstring s(L"加载 "); s += csharppatth; s += L" 时发生错误。\n找不到文件";
		MessageBox(0, s.c_str(), L"系统错误", MB_ICONERROR);
	}
	CHARDEL(csharppatth2);
	CHARDEL(monopath2);
	return 0;
}
B_CAPI(void) AppFreeMono()
{
	if (domain)
		mono_jit_cleanup(domain);
}
BOOL AppLoadMonoICalls()
{
	mono_add_internal_call("ScreenRecoder.App.API::GetInIPath", &GetInIPath);
	mono_add_internal_call("ScreenRecoder.App.API::Test", &Test);
	mono_add_internal_call("ScreenRecoder.App.API::WindowMove", &WindowMove);
	mono_add_internal_call("ScreenRecoder.App.API::WindowShow", &WindowShow);
	mono_add_internal_call("ScreenRecoder.App.API::WindowHide", &WindowHide);
	mono_add_internal_call("ScreenRecoder.App.API::DrawRect", &AppDrawRect);
	mono_add_internal_call("ScreenRecoder.App.API::ShowAnim", &AppShowAnim);
	mono_add_internal_call("ScreenRecoder.App.API::HideAnim", &AppHideAnim);
	mono_add_internal_call("ScreenRecoder.App.API::WindowSetForeground", &AppTop);
	mono_add_internal_call("ScreenRecoder.App.API::OpenFile", &AppOpenFile);
	mono_add_internal_call("ScreenRecoder.App.API::StartNewWhenExit", &AppStartNewWhenExit);
	mono_add_internal_call("ScreenRecoder.App.API::RegisterHotKey", &AppRegisterHotKey);
	mono_add_internal_call("ScreenRecoder.App.API::UnRegisterAllHotKey", &AppUnRegisterAllHotKey);
	mono_add_internal_call("ScreenRecoder.App.API::WindowIsVisible", &WindowIsVisible);
	mono_add_internal_call("ScreenRecoder.App.API::GetDefExportDir", &GetDefExportDir);
	

	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderDestroy", &RecoderDestroy);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderCreate", &RecoderCreate);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderSetUpdateCallback", &RecoderSetUpdateCallback);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderPauseButton", &RecoderPauseButton);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderStopButton", &RecoderStopButton);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderStartButton", &RecoderStartButton);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderSetNextOutFileName", &RecoderSetNextOutFileName);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderSetCaptureFrameRate", &RecoderSetCaptureFrameRate);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderSetCaptureRect", &RecoderSetCaptureRect);
	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderSetOutFileDir", &RecoderSetOutFileDir);
	
	mono_add_internal_call("ScreenRecoder.App.Recorder::get_IsRecording", &RecoderIsRecording);
	mono_add_internal_call("ScreenRecoder.App.Recorder::get_IsInterrupt", &RecoderIsInterrupt);
	mono_add_internal_call("ScreenRecoder.App.Recorder::get_Duration", &RecoderGetRecSec);
	mono_add_internal_call("ScreenRecoder.App.Recorder::get_LastError", &RecoderGetLastError);

	mono_add_internal_call("ScreenRecoder.App.Recorder::get_State", &RecoderGetState);
	mono_add_internal_call("ScreenRecoder.App.Recorder::get_RecordSound", &RecoderGetRecSound);
	mono_add_internal_call("ScreenRecoder.App.Recorder::set_RecordSound", &RecoderSetRecSound);
	mono_add_internal_call("ScreenRecoder.App.Recorder::get_RecordMouse", &RecoderGetRecMic);
	mono_add_internal_call("ScreenRecoder.App.Recorder::set_RecordMouse", &RecoderSetRecMic);
	mono_add_internal_call("ScreenRecoder.App.Recorder::get_RecordQuality", &RecoderGetRecQua);
	mono_add_internal_call("ScreenRecoder.App.Recorder::set_RecordQuality", &RecoderSetRecQua);
	mono_add_internal_call("ScreenRecoder.App.Recorder::get_RecordFormat", &RecoderGetRecFormat);
	mono_add_internal_call("ScreenRecoder.App.Recorder::set_RecordFormat", &RecoderSetRecFormat);

	mono_add_internal_call("ScreenRecoder.App.Recorder::RecoderGetLastOutFileName", &RecoderGetLastOutFileName);
	
	
	return TRUE;
}
int AppRun()
{
	AppStartInit();

	HANDLE hMutex = CreateMutex(NULL, TRUE, L"ScreenRecoderApp");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		WCHAR buf[32];
		GetPrivateProfileStringW(L"AppSetting", L"LastWindow", L"0", buf, 32, GetInIPath2());
		HWND hWnd = (HWND)_wtol(buf);
		if (IsWindow(hWnd))
			SetForegroundWindow(hWnd);
	}
	else {
		MonoClass* program_class = mono_class_from_name(image, "ScreenRecoder.App", "Program");
		MonoMethodDesc* methoddesc = mono_method_desc_new("Program::Main", false);
		MonoMethod* method = mono_method_desc_search_in_class(methoddesc, program_class);
		MonoObject* pararm[1];
		if (argCount > 1)
		{
			pararm[0] = (MonoObject*)mono_array_new(domain, mono_class_from_name(mono_get_corlib(), "System", "String"), argCount - 1);
			for (int i = 1; i < argCount; i++) {
				char*utf8str = UnicodeToUtf8(szArgList[i]);
				mono_array_set((MonoArray*)pararm[0], MonoString*, i - 1, mono_string_new(domain, utf8str));
				StrDel(utf8str);
			}
		}
		else pararm[0] = (MonoObject*)mono_array_new(domain, mono_class_from_name(mono_get_corlib(), "System", "String"), 0);
		mono_runtime_invoke(method, NULL, (void**)&pararm, NULL);
		AppExitRealloc();
	}
	if (hMutex)
		ReleaseMutex(hMutex);
	if (startNewWhenExit)
		ShellExecute(NULL, L"open", szArgList[0], NULL, NULL, 5);
	return 1;
}

HBRUSH hRedBrush;

void AppStartInit()
{
	hRedBrush = CreateSolidBrush(RGB(255, 0, 0));

	WCHAR defexportdir[MAX_PATH];
	memset(defexportdir, 0, sizeof(defexportdir));
	wcscpy_s(defexportdir, startDir);
	wcscat_s(defexportdir, L"\\");
	wcscat_s(defexportdir, L"output");
	wcscpy_s(defexportdir2, defexportdir);
	if (_waccess(defexportdir, 0) != 0) {
		_wmkdir(defexportdir);
	}
}
void AppExitRealloc()
{
	DeleteObject(hRedBrush);
}
void AppDrawRect(HDC hdc, int w, int h)
{
	HGDIOBJ old = SelectObject(hdc, hRedBrush);

	Rectangle(hdc, 16, 16, w - 4, h - 4);

	Rectangle(hdc, 0, 0, (w - 50), (h - 10));
	Rectangle(hdc, 0, 0, (w - 10), (h - 50));

	Rectangle(hdc, w - 50, 0, 0, (h - 10));
	Rectangle(hdc, w - 10, 0, (w - 10), (h - 50));

	Rectangle(hdc, w - 10, 0, 0, 0);
	Rectangle(hdc, w - 50, h - 10, 0, 0);

	Rectangle(hdc, 0, h - 50, w - 10, 0);
	Rectangle(hdc, 0, h - 10, w - 50, 0);

	SelectObject(hdc, old);
}
void AppShowAnim(HWND hWnd)
{
	AnimateWindow(hWnd, 1000, AW_SLIDE | AW_VER_POSITIVE | AW_ACTIVATE);
}
void AppHideAnim(HWND hWnd)
{
	AnimateWindow(hWnd, 1000, AW_CENTER | AW_HIDE);
}
BOOL WindowIsVisible(HWND hWnd)
{
	return  IsWindowVisible(hWnd);
}

void AppTop(HWND hWnd)
{
	SetForegroundWindow(hWnd);
}
void AppOpenFile(MonoString* file)
{
	LPWSTR w = (LPWSTR)mono_string_to_utf16(file);
	ShellExecute(NULL, L"open", w, NULL, NULL, 5);
	mono_free(w);
}
void AppStartNewWhenExit()
{
	startNewWhenExit = true;
}
BOOL AppRegisterHotKey(HWND hWnd, int i, int k1, int k2, int k3)
{
	
	bool shift = false, alt = false, ctrl = false;
	UINT mk = 0, key = 0;

	if (k1 != 0) {
		if (k1 == 65536 && !shift) {
			mk |= MOD_SHIFT;
			shift = true;
		}
		else if (k1 == 262144 && !alt) {
			mk |= MOD_ALT;
			alt = true;
		}
		else if (k1 == 131072 && !ctrl) {
			mk |= MOD_CONTROL;
			ctrl = true;
		}
		else key = k1;
	}

	if (k2 != 0) {
		if (k2 == 65536 && !shift) {
			mk |= MOD_SHIFT;
			shift = true;
		}
		else if (k2 == 262144 && !alt) {
			mk |= MOD_ALT;
			alt = true;
		}
		else if (k2 == 131072 && !ctrl) {
			mk |= MOD_CONTROL;
			ctrl = true;
		}
		else key = k2;
	}

	if (k3 != 0) {
		if (k3 == 65536 && !shift) {
			mk |= MOD_SHIFT;
			shift = true;
		}
		else if (k3 == 262144 && !alt) {
			mk |= MOD_ALT;
			alt = true;
		}
		else if (k3 == 131072 && !ctrl) {
			mk |= MOD_CONTROL;
			ctrl = true;
		}
		else key = k3;
	}

	return RegisterHotKey(hWnd, 0x8886 + i, mk, key);
}
void AppUnRegisterAllHotKey(HWND hWnd)
{
	list<int>::iterator it;
	for (it = regedKey.begin(); it != regedKey.end(); it++)
	{
		int item = *it;
		UnregisterHotKey(hWnd, item);
	}
}
MonoString*GetDefExportDir()
{
	MonoString* rs = NULL;
	char*utf8str = UnicodeToUtf8(defexportdir2);
	rs = mono_string_new(domain, utf8str);
	StrDel(utf8str);
	return rs;
}

bool record_started_ = false;
bool record_interrupt_ = false;
int64_t start_capture_time_ = 0;
int64_t duration_ = 0;
void* m_pRecorder = NULL;
UpdateCallback m_UpdateCallback;



int capture_left, capture_top, capture_width, capture_height;
int capture_frame_rate = 15;
bool capture_recorder_speaker = true;
bool capture_recorder_mic = true;
int capture_recorder_quality = 1;
int capture_recorder_format = 0;
char*capture_out_file = (char*)"";
char capture_last_out_file[MAX_PATH];
char capture_out_dir[MAX_PATH];
const wchar_t*capture_last_error = NULL;

void recorder_log_cb(MediaFileRecorder::SDK_LOG_LEVEL level, const wchar_t* msg)
{
#if _DEBUG
	if (level >= MediaFileRecorder::LOG_INFO)
	{
		OutputDebugString(msg);
		OutputDebugString(L"\n");
	}
#else
	if (level > MediaFileRecorder::LOG_INFO)
	{
		OutputDebugString(msg);
		OutputDebugString(L"\n");
    }
#endif
	if (level >= MediaFileRecorder::LOG_ERROR)
	{
		capture_last_error = msg;
	}
}

LONG RecoderGetRecSec() {
	return static_cast<LONG>(duration_ / 1000);
}
BOOL RecoderIsRecording() {
	return record_started_;
}
BOOL RecoderIsInterrupt() {
	return record_interrupt_;
}
void RecoderDestroy() {
	MR_DestroyScreenAudioRecorder(m_pRecorder);
}
void RecoderCreate() {
	m_pRecorder = MR_CreateScreenAudioRecorder();
	MR_SetLogCallBack(recorder_log_cb);
}
RECORD_STATE RecoderGetState() {
	int n = MR_GetRecordState(m_pRecorder);
	return (RECORD_STATE)n;
}
void RecoderSetUpdateCallback(UpdateCallback updateCallback)
{
	m_UpdateCallback = updateCallback;
}
void RecoderPauseButton()
{
	if (record_started_)
	{
		if (!record_interrupt_)
		{
			MR_SuspendRecord(m_pRecorder);
			record_interrupt_ = true;
			if (m_UpdateCallback) m_UpdateCallback(UpdateCallbackID::OnPause, NULL);

			duration_ += timeGetTime() - start_capture_time_;
			char log[128] = { 0 };
			_snprintf_s(log, 128, "duration: %lld \n", duration_);
			OutputDebugStringA(log);
		}
		else
		{
			MR_ResumeRecord(m_pRecorder);
			start_capture_time_ = timeGetTime();
			record_interrupt_ = false;
			if (m_UpdateCallback) m_UpdateCallback(UpdateCallbackID::OnContinue, NULL);
		}
	}
}
void RecoderStopButton()
{
	if (record_started_) {
		record_started_ = false;
		MR_StopRecord(m_pRecorder);
		if (m_UpdateCallback) m_UpdateCallback(UpdateCallbackID::OnStop, NULL);

		if (!record_interrupt_)
			duration_ += timeGetTime() - start_capture_time_;

		char log[128] = { 0 };
		_snprintf_s(log, 128, "duration: %lld \n", duration_);
		OutputDebugStringA(log);
	}
}
BOOL RecoderStartButton()
{
	if (!record_started_)
	{
		MediaFileRecorder::RECT grab_rect;
		grab_rect.left = capture_left;
		grab_rect.top = capture_top;
		grab_rect.right = capture_left + capture_width;
		grab_rect.bottom = capture_top + capture_height;

		MediaFileRecorder::RECORD_INFO record_info;

		if (strlen(capture_out_file) == 0) {
			SYSTEMTIME sys;
			GetLocalTime(&sys);
			WCHAR path[MAX_PATH];
			if (strcmp(capture_out_dir, "") == 0)
				wsprintf(path, L"%02d%02d%02d%02d.mp4", sys.wDay, sys.wHour, sys.wMinute, sys.wSecond);
			else {
				LPWSTR w = AnsiToUnicode(capture_out_dir);
				wsprintf(path, L"%s\\%02d%02d%02d%02d.mp4", w, sys.wDay, sys.wHour, sys.wMinute, sys.wSecond);
				StrDel(w);
			}
			char*s = UnicodeToUtf8(path);
			strcpy_s(capture_last_out_file, s);
			strcpy_s(record_info.file_name, s);
			StrDel(s);
		}
		else strcpy_s(record_info.file_name, capture_out_file);

		record_info.video_capture_rect = grab_rect;
		record_info.video_dst_width = capture_width;
		record_info.video_dst_height = capture_height;
		record_info.video_frame_rate = capture_frame_rate;
		record_info.quality = (MediaFileRecorder::VIDEO_QUALITY)capture_recorder_quality;
		record_info.format = (MediaFileRecorder::VIDEO_FORMAT)capture_recorder_format;
		record_info.is_record_speaker = capture_recorder_speaker;
		record_info.is_record_video = true;
		record_info.is_record_mic = capture_recorder_mic;

		int ret = MR_SetRecordInfo(m_pRecorder, record_info);
		ret = MR_StartRecord(m_pRecorder);
		if (ret == 0)
		{
			record_started_ = true;
			if (m_UpdateCallback) m_UpdateCallback(UpdateCallbackID::OnStart, NULL);
			start_capture_time_ = timeGetTime();
			duration_ = 0;
			return 1;
		}
	}
	return 0;
}
void RecoderSetNextOutFileName(MonoString* file)
{
	if (mono_string_length(file) == 0)
		capture_out_file = (char*)"";
	else {
		LPWSTR w = (LPWSTR)mono_string_to_utf16(file);
		char*str = UnicodeToAnsi(w);
		capture_out_file = str;
		StrDel(str);	
		mono_free(w);
	}
}
void RecoderSetOutFileDir(MonoString* dir)
{
	if (mono_string_length(dir) == 0)
		strcpy_s(capture_out_dir, "");
	else {
		LPWSTR w = (LPWSTR)mono_string_to_utf16(dir);
		char*str = UnicodeToAnsi(w);
		strcpy_s(capture_out_dir, str);
		StrDel(str);
		mono_free(w);
	}
}
MonoString* RecoderGetLastOutFileName()
{
	MonoString* rs = NULL;
	WCHAR path[MAX_PATH];
	memset(path, 0, sizeof(path));
	if (strcmp(capture_out_dir, "") == 0) {
		wcscpy_s(path, startDir);
		wcscat_s(path, L"\\");
	}
	else {
		//WCHAR*c2 = AnsiToUnicode(capture_out_dir);
		//wcscpy_s(path, c2);
		//wcscat_s(path, L"\\");
		//StrDel(c2);
	}

	WCHAR*c3 = AnsiToUnicode(capture_last_out_file);
	wcscat_s(path, c3);
	StrDel(c3);

	char*utf8str = UnicodeToUtf8(path);
	rs = mono_string_new(domain, utf8str);
	StrDel(utf8str);
	return rs;
}
MonoString* RecoderGetLastError()
{
	MonoString* rs = NULL;
	char*utf8str = UnicodeToUtf8(capture_last_error);
	rs = mono_string_new(domain, utf8str);
	StrDel(utf8str);
	return rs;
}
void RecoderSetCaptureFrameRate(int rate)
{
	capture_frame_rate = rate;
}
void RecoderSetCaptureRect(int x, int y, int w, int h)
{
	if (x == 0 && y == 0 && w == 0 && h == 0)
	{
		w = GetSystemMetrics(SM_CXFULLSCREEN);
		h = GetSystemMetrics(SM_CYFULLSCREEN);
	}
	capture_left=x; 
	capture_top=y;
	capture_width=w;
	capture_height=h;
}
void RecoderSetRecMic(BOOL rmic)
{
	capture_recorder_mic = rmic;
}
BOOL RecoderGetRecMic()
{
	return capture_recorder_mic;
}
void RecoderSetRecSound(BOOL rmic)
{
	capture_recorder_speaker = rmic;
}
BOOL RecoderGetRecSound()
{
	return capture_recorder_speaker;
}
void RecoderSetRecQua(int rmic)
{
	capture_recorder_quality = rmic;
}
int RecoderGetRecQua()
{
	return capture_recorder_quality;
}
void RecoderSetRecFormat(int format)
{
	capture_recorder_format = format;
}
int RecoderGetRecFormat()
{
	return capture_recorder_format;
}