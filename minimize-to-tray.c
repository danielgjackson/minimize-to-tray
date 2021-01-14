// Minimize a window to the taskbar notification area (system tray)
// Dan Jackson, 2020.

#define _WIN32_WINNT 0x0600
#include <windows.h>

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include <tchar.h>
#include <rpc.h>

#define USE_GUID
//#define SETFOCUS

// Notify icons
#include <shellapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "shell32.lib")
#endif

// commctrl v6 for LoadIconMetric()
#include <commctrl.h>
#ifdef _MSC_VER
// Moved to external .manifest file linked through .rc file
//#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#endif

// Defines
#define WMAPP_NOTIFYCALLBACK (WM_APP + 1)
#define TIMER_ID 1
#define IDM_RESTORE	101
#define IDM_EXIT	102
#define IDM_CLOSE	103

// Hacky global state
HINSTANCE ghInstance = NULL;
HWND ghWndMain = NULL;
#ifdef USE_GUID
  GUID guidMinimizeIcon = {0};
#else
  UINT idMinimizeIcon = 0;
#endif
HWINEVENTHOOK ghWehDestroy = NULL;
HWINEVENTHOOK ghWehMinimizeStart = NULL;
HWND ghWndTracked = NULL;
BOOL gbHasNotifyIcon = FALSE;
BOOL gbNotify = TRUE;
BOOL gbSubsystemWindows = FALSE;
BOOL gbHasConsole = FALSE;
BOOL gbStartMinimized = TRUE;
BOOL gbForcePoll = FALSE;

// Redirect standard I/O to a console
static BOOL RedirectIOToConsole(BOOL tryAttach, BOOL createIfRequired)
{
	BOOL hasConsole = FALSE;
	if (tryAttach && AttachConsole(ATTACH_PARENT_PROCESS))
	{
		hasConsole = TRUE;
	}
	if (createIfRequired && !hasConsole)
	{
		AllocConsole();
		AttachConsole(GetCurrentProcessId());
		hasConsole = TRUE;
	}
	if (hasConsole)
	{
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}
	return hasConsole;
}

/*
static void PrintLastError()
{ 
	LPVOID lpMsgBuf; 
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, (LPTSTR)&lpMsgBuf, 0, NULL); 
	_ftprintf(stderr, (TCHAR *)lpMsgBuf);
	#ifdef _DEBUG
	OutputDebugString((TCHAR *)lpMsgBuf); 
	#endif
	LocalFree(lpMsgBuf); 
}
*/

// TODO: Does not get icons for "modern apps" (UWP etc): try GetPackageFullName/Package Query API ?
HICON GetWindowIcon(HWND hWnd)
{
	HICON hIcon = (HICON)SendMessage(hWnd, WM_GETICON, ICON_SMALL2, 0);
	if (hIcon == NULL) hIcon = (HICON)SendMessage(hWnd, WM_GETICON, ICON_SMALL, 0);
	if (hIcon == NULL) hIcon = (HICON)SendMessage(hWnd, WM_GETICON, ICON_BIG, 0);
	if (hIcon == NULL) hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICON);
	if (hIcon == NULL) hIcon = (HICON)GetClassLongPtr(hWnd, GCLP_HICONSM);
	if (hIcon == NULL) LoadIconMetric(ghInstance, L"MAINICON", LIM_SMALL, &hIcon);	// MAKEINTRESOURCE(IDI_APPLICATION)
	return hIcon;
}

NOTIFYICONDATA nid = {0};

void DeleteNotificationIcon(void)
{
	if (gbHasNotifyIcon)
	{
_tprintf(TEXT("ICON: Delete\n"));
		memset(&nid, 0, sizeof(nid));
		nid.cbSize = sizeof(nid);
		nid.hWnd = ghWndMain;
		nid.uFlags = 0;
		nid.dwStateMask = NIS_HIDDEN;
		_tcscpy(nid.szInfoTitle, TEXT(""));
		_tcscpy(nid.szInfo, TEXT(""));
#ifdef USE_GUID
		nid.uFlags |= NIF_GUID;
		nid.guidItem = guidMinimizeIcon;
#else
		memset(&nid.guidItem, 0, sizeof(nid.guidItem));
		nid.uID = idMinimizeIcon;
#endif
		Shell_NotifyIcon(NIM_DELETE, &nid);
		gbHasNotifyIcon = FALSE;
	}
}

BOOL AddNotificationIcon(HWND hwnd)
{
_tprintf(TEXT("ICON: Add\n"));
#ifndef USE_GUID
_tprintf(TEXT("ICON: 0x%08x\n"), idMinimizeIcon);
#endif
	DeleteNotificationIcon();

// TODO: Remove this local shadowing of the global notify icon state once stopped notification showing when icon is removed.
NOTIFYICONDATA nid = {0};
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = hwnd;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
	nid.uFlags |= NIS_HIDDEN;
	nid.dwStateMask = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP | NIS_HIDDEN;
	if (gbNotify)
	{
		nid.uFlags |= NIF_INFO | NIF_REALTIME;
		nid.dwStateMask |= NIF_INFO | NIF_REALTIME;
		_tcscpy(nid.szInfoTitle, TEXT("Minimize To Tray"));
		_tcscpy(nid.szInfo, TEXT("The application is being minimized to the Notification Area."));
		nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
	}
#ifdef USE_GUID
	nid.uFlags |= NIF_GUID;
	nid.guidItem = guidMinimizeIcon;
#else
	memset(&nid.guidItem, 0, sizeof(nid.guidItem));
	nid.uID = idMinimizeIcon;
#endif
	nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
	nid.hIcon = GetWindowIcon(ghWndTracked);
	GetWindowText(ghWndTracked, nid.szTip, sizeof(nid.szTip)/sizeof(nid.szTip[0]));
	Shell_NotifyIcon(NIM_ADD, &nid);
	nid.uVersion = NOTIFYICON_VERSION_4;
	Shell_NotifyIcon(NIM_SETVERSION, &nid);
	gbHasNotifyIcon = TRUE;
	return gbHasNotifyIcon;
}

void ShowNotificationIcon(HWND hwnd)
{
_tprintf(TEXT("ICON: Show\n"));
	if (!gbHasNotifyIcon)
	{
		AddNotificationIcon(hwnd);
	}
	nid.uFlags &= ~NIS_HIDDEN;
	nid.dwStateMask = NIS_HIDDEN;
	Shell_NotifyIcon(NIM_MODIFY, &nid);
_tprintf(TEXT("ICON: ...shown\n"));
}

void HideNotificationIcon(void)
{
_tprintf(TEXT("ICON: Hide\n"));
#ifndef USE_GUID
_tprintf(TEXT("ICON: 0x%08x\n"), idMinimizeIcon);
#endif
	if (gbHasNotifyIcon)
	{
		nid.uFlags |= NIS_HIDDEN;
		nid.dwStateMask = NIS_HIDDEN;
		Shell_NotifyIcon(NIM_MODIFY, &nid);
_tprintf(TEXT("ICON: ...hidden\n"));
// [dgj] temporary
#ifdef USE_GUID
DeleteNotificationIcon();
#endif
	}
}

void ManageMinimize(void)
{
	if (IsWindow(ghWndTracked))
	{
		_tprintf(TEXT("COMMAND: Minimize...\n"));
		// Minimize
		ShowWindow(ghWndTracked, SW_MINIMIZE);	// SW_FORCEMINIMIZE
		// Hide
		ShowWindow(ghWndTracked, SW_HIDE);
	}
	ShowNotificationIcon(ghWndMain);
}

// Restore tracked window and remove notification icon
void ManageRestore(bool managerTerminating)
{
	if (IsWindow(ghWndTracked))
	{
		_tprintf(TEXT("COMMAND: Restore...\n"));
		// Un-hide
		ShowWindow(ghWndTracked, SW_SHOW);

		// If this process is terminating, restore the window but leave the state as-is (e.g. minimized)
		if (!managerTerminating)
		{
			if (IsIconic(ghWndTracked))
			{
				// Restore
				ShowWindow(ghWndTracked, SW_SHOWNORMAL);
			}
			SetForegroundWindow(ghWndTracked);
		}
	}
	HideNotificationIcon();
	if (managerTerminating)
	{
		DeleteNotificationIcon();
	}
}

// Fallback polling if fails to install hook
void PollUpdate(HWND hWnd)
{
	_tprintf(TEXT("."));
	if (!IsWindow(ghWndTracked))
	{
		_tprintf(TEXT("POLL: Close detected.\n"));
		ManageRestore(false);	// removes icon
		PostMessage(hWnd, WM_CLOSE, 0, 0);	// closes application
		return;
	}
	// Not hidden
	if (IsWindowVisible(ghWndTracked))
	{
		if (IsIconic(ghWndTracked))
		{
			_tprintf(TEXT("POLL: Minimize detected.\n"));
			ManageMinimize();
			return;
		}
	}
}

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime)
{
	if (hWnd == ghWndTracked && event == EVENT_OBJECT_DESTROY && idObject == OBJID_WINDOW && idChild == INDEXID_CONTAINER)
	{
		_tprintf(TEXT("HOOK: Close detected.\n"));
		ManageRestore(false);	// removes icon
		PostMessage(ghWndMain, WM_CLOSE, 0, 0);	// closes application
		return;
	}
	else if (hWnd == ghWndTracked && event == EVENT_SYSTEM_MINIMIZESTART && idObject == OBJID_WINDOW && idChild == INDEXID_CONTAINER)
	{
		_tprintf(TEXT("HOOK: Minimize detected.\n"));
		ManageMinimize();
		return;
	}
}

// State for finding a window
typedef struct
{
	const TCHAR *title;
	HWND hWndFound;
	BOOL bSuffix;
	BOOL bPrefix;
	const TCHAR *matchingTitle;
} find_state_t;

// Callback helper to match on a window title
static BOOL CALLBACK enumFuncFindTitle(HWND hWnd, LPARAM lparam)
{
	find_state_t *findState = (find_state_t *)lparam;
	if (IsWindowVisible(hWnd))
	{
		// Window's title text
		int length = GetWindowTextLength(hWnd);
		TCHAR *windowTitle = (TCHAR *)malloc((length + 1) * sizeof(TCHAR));
		GetWindowText(hWnd, windowTitle, length + 1);
		
		// Loop through all matching substrings
		BOOL matched = FALSE;
		for (TCHAR *match, *search = windowTitle; (match = _tcsstr(search, findState->title)) != NULL; search++)
		{
			// Matching complete string only
			if (!findState->bSuffix && !findState->bPrefix)
			{
				matched |= match == windowTitle && _tcslen(findState->title) == _tcslen(windowTitle);
			}
			// Matching as a prefix
			if (!findState->bSuffix && findState->bPrefix)
			{
				matched |= match == windowTitle;
			}
			// Matching as a suffix
			if (findState->bSuffix && !findState->bPrefix)
			{
				matched |= _tcslen(findState->title) + (match - windowTitle) >= _tcslen(windowTitle);
			}
			// Matching as any substring 
			if (findState->bSuffix && findState->bPrefix)
			{
				matched |= true;
			}
		}
		
		// Adjust matching windows
		if (matched)
		{
			findState->matchingTitle = windowTitle;
			findState->hWndFound = hWnd;
			//return FALSE;		// TODO: Possible to sort by time opened?
		}

		free(windowTitle);
	}
	return TRUE;
}

// This is NOT a good way to generate a GUID and large parts would collide if called at the same time.
// TODO: Use UuidCreate((UUID *)guid) from rpc.h
void CreateGuid(GUID *guid)
{
	unsigned int seed;
	LARGE_INTEGER counter = {0};
	if (QueryPerformanceCounter(&counter))
	{
		seed = counter.LowPart;
	}
	else
	{
		seed = time(NULL);
	}
	srand(seed);
	guid->Data1 = (unsigned long)(intptr_t)ghWndTracked;	// Long
	guid->Data2 = (unsigned short)rand();		// short
	guid->Data3 = (unsigned short)rand();		// short
	guid->Data4[0] = (unsigned char)rand();
	guid->Data4[1] = (unsigned char)rand();
	guid->Data4[2] = (unsigned char)rand();
	guid->Data4[3] = (unsigned char)rand();
	guid->Data4[4] = (unsigned char)rand();
	guid->Data4[5] = (unsigned char)rand();
	guid->Data4[6] = (unsigned char)rand();
	guid->Data4[7] = (unsigned char)rand();
}

void ShowContextMenu(HWND hwnd, POINT pt)
{
	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, IDM_RESTORE, TEXT("&Restore"));
	AppendMenu(hMenu, MF_STRING, IDM_CLOSE, TEXT("&Close"));
	AppendMenu(hMenu, MF_SEPARATOR, 0, TEXT("Separator"));
	AppendMenu(hMenu, MF_STRING, IDM_EXIT, TEXT("&Stop Hiding"));
	SetMenuDefaultItem(hMenu, IDM_RESTORE, FALSE);
	SetForegroundWindow(hwnd);
	UINT uFlags = TPM_RIGHTBUTTON;
	if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
	{
		uFlags |= TPM_RIGHTALIGN;
	}
	else
	{
		uFlags |= TPM_LEFTALIGN;
	}
	TrackPopupMenuEx(hMenu, uFlags, pt.x, pt.y, hwnd, NULL);
	DestroyMenu(hMenu);
}

void Startup(HWND hWnd)
{
	ghWndMain = hWnd;

	_tprintf(TEXT("Startup...\n"));
	if (gbStartMinimized)
	{
		ManageMinimize();
	}

	DWORD dwProcessId = 0;
	DWORD dwThreadId = GetWindowThreadProcessId(ghWndTracked, &dwProcessId);
	if (dwThreadId)
	{
		DWORD filterProcess = 0; // dwProcessId
		DWORD filterThread = 0; // dwThreadId
		ghWehDestroy = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, NULL, WinEventProc, filterProcess, filterThread, WINEVENT_OUTOFCONTEXT);
		if (ghWehDestroy == NULL)
		{
			_tprintf(TEXT("WARNING: SetWinEventHook(EVENT_OBJECT_DESTROY) failed.\n"));
		}
		ghWehMinimizeStart = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZESTART, NULL, WinEventProc, filterProcess, filterThread, WINEVENT_OUTOFCONTEXT);
		if (ghWehMinimizeStart == NULL)
		{
			_tprintf(TEXT("WARNING: SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART) failed.\n"));
		}
	}
	else
	{
		_tprintf(TEXT("WARNING: GetWindowThreadProcessId() failed.\n"));
	}

	if (gbForcePoll || ghWehDestroy == NULL || ghWehMinimizeStart == NULL)
	{
		if (!gbForcePoll) _tprintf(TEXT("WARNING: No hooks installed, falling back to polling method for this window.\n"));
		SetTimer(hWnd, TIMER_ID, 2 * 1000, NULL);
	}
	
	// Do one poll (e.g. if currently minimized and we're not forcing it and we're not polling it)
	PollUpdate(hWnd);
}

void Shutdown(void)
{
	_tprintf(TEXT("Shutdown()...\n"));
	if (ghWehDestroy) { UnhookWinEvent(ghWehDestroy); ghWehDestroy = NULL; }
	if (ghWehMinimizeStart) { UnhookWinEvent(ghWehMinimizeStart); ghWehMinimizeStart = NULL; }
	_tprintf(TEXT("...END: Shutdown()\n"));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		Startup(hwnd);
		break;

	case WM_COMMAND:
		{
			int const wmId = LOWORD(wParam);
#ifdef SETFOCUS
if (gbHasNotifyIcon && (wmId == IDM_RESTORE || wmId == IDM_EXIT || wmId == IDM_CLOSE)) Shell_NotifyIcon(NIM_SETFOCUS, &nid);
#endif
			switch (wmId)
			{
			case IDM_RESTORE:	// Restore the tracked window
				ManageRestore(false);
				break;
			case IDM_EXIT:	// Stop this program and leave the tracked window alone
				ManageRestore(true);
				PostMessage(hwnd, WM_CLOSE, 0, 0);
				break;
			case IDM_CLOSE:	// Close tracked window (if it does close, we will exit too)
				ManageRestore(false);
			   	PostMessage(ghWndTracked, WM_CLOSE, 0, 0);
				break;
			default:
				return DefWindowProc(hwnd, message, wParam, lParam);
			}
		}
		break;

	case WMAPP_NOTIFYCALLBACK:
		switch (LOWORD(lParam))
		{
		case NIN_SELECT:
#ifdef SETFOCUS
if (gbHasNotifyIcon) Shell_NotifyIcon(NIM_SETFOCUS, &nid);
#endif
			ManageRestore(false);	// Restore the tracked window
			break;

		case WM_RBUTTONUP:		// If not using NOTIFYICON_VERSION_4 ?
		case WM_CONTEXTMENU:
			{
				POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
				ShowContextMenu(hwnd, pt);
			}
			break;
		}
		break;

	case WM_CLOSE:
_tprintf(TEXT("WM_CLOSE...\n"));
		ManageRestore(true);	// close while we still have a window so DeleteNotificationIcon works consistently
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
_tprintf(TEXT("WM_DESTROY...\n"));
		Shutdown();
		PostQuitMessage(0);
		break;

	case WM_TIMER:
		if (wParam == TIMER_ID)
		{
			PollUpdate(hwnd);
		}
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}

void done(void)
{
	ManageRestore(true);
	_tprintf(TEXT("DONE.\n"));
}

void finish(void)
{
	_tprintf(TEXT("END: Application ended.\n"));
	done();
}

LONG WINAPI UnhandledException(EXCEPTION_POINTERS *exceptionInfo)
{
	_tprintf(TEXT("END: Unhandled exception.\n"));
	done();
	return EXCEPTION_CONTINUE_SEARCH; // EXCEPTION_EXECUTE_HANDLER;
}

void SignalHandler(int signal)
{
	_tprintf(TEXT("END: Received signal: %d\n"), signal);
	//if (signal == SIGABRT)
	done();
}

BOOL WINAPI consoleHandler(DWORD signal)
{
	_tprintf(TEXT("END: Received console control event: %u\n"), (unsigned int)signal);
	//if (signal == CTRL_C_EVENT)
	done();
	return FALSE;
}

int run(int argc, TCHAR *argv[], HINSTANCE hInstance, BOOL hasConsole)
{
	ghInstance = hInstance;
	gbHasConsole = hasConsole;

#if 0
	_tprintf(TEXT("DEBUG: hasConsole=%s\n"), hasConsole ? TEXT("true") : TEXT("false"));
	_tprintf(TEXT("DEBUG: subsystemWindows=%s\n"), gbSubsystemWindows ? TEXT("true") : TEXT("false"));
#endif
	
	if (gbHasConsole)
	{
		SetConsoleCtrlHandler(consoleHandler, TRUE);
	}
	SetUnhandledExceptionFilter(UnhandledException);
	atexit(finish);
	signal(SIGABRT, SignalHandler);

	ghInstance = hInstance;

	BOOL bShowHelp = FALSE;
	find_state_t findState = {0};
	int positional = 0;
	int errors = 0;

	for (int i = 0; i < argc; i++)  
	{
		if (_tcsicmp(argv[i], TEXT("/?")) == 0) { bShowHelp = true; }
		else if (_tcsicmp(argv[i], TEXT("/HELP")) == 0) { bShowHelp = true; }
		else if (_tcsicmp(argv[i], TEXT("--help")) == 0) { bShowHelp = true; }
		else if (_tcsicmp(argv[i], TEXT("/MIN")) == 0) { gbStartMinimized = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/NOMIN")) == 0) { gbStartMinimized = false; }
		else if (_tcsicmp(argv[i], TEXT("/POLL")) == 0) { gbForcePoll = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/NOPOLL")) == 0) { gbForcePoll = FALSE; }
		else if (_tcsicmp(argv[i], TEXT("/NOTIFY")) == 0) { gbNotify = TRUE; }
		else if (_tcsicmp(argv[i], TEXT("/NONOTIFY")) == 0) { gbNotify = FALSE; }
		
		else if (argv[i][0] == '/') 
		{
			_ftprintf(stderr, TEXT("ERROR: Unexpected parameter: %s\n"), argv[i]);
			errors++;
		} 
		else 
		{
			if (positional == 0)
			{
				size_t len = _tcslen(argv[i]);
				if (len > 0 && argv[i][len - 1] == L'*')
				{
					findState.bPrefix = TRUE;
					argv[i][len - 1] = '\0';
				}

				findState.title = argv[i];

				if (findState.title[0] == L'*')
				{
					findState.bSuffix = TRUE;
					findState.title++;
				}
			} 
			else 
			{
				_ftprintf(stderr, TEXT("ERROR: Unexpected positional parameter: %s\n"), argv[i]);
				errors++;
			}
			positional++;
		}
	}


	if (errors)
	{
		_ftprintf(stderr, TEXT("ERROR: %d parameter error(s).\n"), errors);
		bShowHelp = true;
	}

	if (findState.title == NULL || _tcslen(findState.title) == 0)
	{
		_ftprintf(stderr, TEXT("ERROR: Window title not specified.\n"));
		bShowHelp = true;
	}

	if (bShowHelp) 
	{
		TCHAR *msg = TEXT(
		  "minimize-to-tray  Dan Jackson, 2020.\n"
		  "\n"
		  "Usage: [/NOMIN|/MIN] [/POLL] \"<window-title>\"\n"
		  "\n"
		  "<window-title> can begin and end with '*' to match as a substring; begin with '*' to match as a suffix; end with '*' to match as a prefix; or otherwise must match exactly.\n"
		  "\n"
		);
		// [/CONSOLE:<ATTACH|CREATE|ATTACH-CREATE>]*  (* only as first parameter)
		if (gbHasConsole)
		{
			_tprintf(msg);
		}
		else
		{
			MessageBox(NULL, msg, TEXT("minimize-to-tray"), MB_OK | MB_ICONERROR);
		}
		return -1;
	}


	// Find window
	EnumWindows(enumFuncFindTitle, (LPARAM)&findState);
	ghWndTracked = findState.hWndFound;

	if (ghWndTracked == NULL)
	{
		TCHAR *msg = TEXT("ERROR: No windows found matching title.\n");
		if (gbHasConsole)
		{
			_ftprintf(stderr, msg);
		}
		else
		{
			MessageBox(NULL, msg, TEXT("minimize-to-tray"), MB_OK | MB_ICONERROR);
		}
		return 1;
	}

	const TCHAR szWindowClass[] = TEXT("MinimizeToTray");
	WNDCLASSEX wcex = {sizeof(wcex)};
	wcex.style		  = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.hInstance	  = ghInstance;
	wcex.hIcon		  = LoadIcon(ghInstance, TEXT("MAINICON"));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = NULL;
	wcex.lpszClassName  = szWindowClass;
	RegisterClassEx(&wcex);

	// 
#ifdef USE_GUID
	CreateGuid(&guidMinimizeIcon);
#else
	idMinimizeIcon = (UINT)ghWndTracked;
#endif

	TCHAR szTitle[100] = TEXT("MinimizeToTray");
	HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, 250, 200, NULL, NULL, ghInstance, NULL);
	ghWndMain = hWnd;
	if (!hWnd) { return -1; }


	// Start tracking application window
	_tprintf(TEXT("TRACKING: %s\n"), findState.matchingTitle);

	ShowWindow(hWnd, SW_HIDE); // nCmdShow
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	ManageRestore(true);
	
	return 0;
}

// Entry point when compiled with console subsystem
int _tmain(int argc, TCHAR *argv[]) 
{
	HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
	return run(argc - 1, argv + 1, hInstance, TRUE);
}

// Entry point when compiled with Windows subsystem
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	gbSubsystemWindows = TRUE;

	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(lpCmdLine, &argc);

	int argOffset = 0;

	BOOL bConsoleAttach = FALSE;
	BOOL bConsoleCreate = FALSE;
	BOOL hasConsole = FALSE;
	for (; argc > 0; argc--, argOffset++)
	{
		if (_tcsicmp(argv[argOffset], TEXT("/CONSOLE:ATTACH")) == 0)
		{
			bConsoleAttach = TRUE;
		}
		else if (_tcsicmp(argv[argOffset], TEXT("/CONSOLE:CREATE")) == 0)
		{
			bConsoleCreate = TRUE;
		}
		else if (_tcsicmp(argv[argOffset], TEXT("/CONSOLE:ATTACH-CREATE")) == 0)
		{
			bConsoleAttach = TRUE;
			bConsoleCreate = TRUE;
		}
		else if (_tcsicmp(argv[argOffset], TEXT("/CONSOLE:DEBUG")) == 0)	// Use existing console
		{
			// stdout to stderr
			_dup2(2, 1);
			hasConsole = TRUE;
		}
		else	// No more prefix arguments
		{
			//_ftprintf(stderr, TEXT("NOTE: Stopped finding prefix parameters at: %s\n"), argv[argOffset]);
			break;
		}
	}
	if (!hasConsole) {
		hasConsole = RedirectIOToConsole(bConsoleAttach, bConsoleCreate);
	}

	return run(argc, argv + argOffset, hInstance, hasConsole);
}
