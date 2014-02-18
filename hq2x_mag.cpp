/*
  http://www.microsoft.com/msj/archive/S274.aspx


	- there is some bug related to the remove cleartype algo. for some reason most of the time it's completely juked up. no idea.

*/

#include "stdafx.h"
#include "hq2x_mag.h"
#include "animbitmap.h"
#include "registry.h"
#include <shellapi.h>

struct Settings
{
  DWORD Magnification;// 1, 2, 3, 4, 6, 8, 9, 12, 16

  bool TimedRefresh;//
  DWORD TimedRefreshInterval;//
  bool RefreshOnKeyboard;//
  bool RefreshOnMouse;//

  bool TrackMouse;//
  bool TrackFocus;
  bool TrackText;

  bool AlwaysOnTop;//
  bool Paused;//
  bool ShowCursor;//
  bool ShowUpdateLight;//
  bool ShowMagnification;
  bool ShowTitleBar;//

	bool removeClearType;
	bool useHQ2x;

  WINDOWPLACEMENT WindowPlacement;

  void LoadSettings()
  {
		AnimBitmap<16>::RgbPixel p = AnimBitmap<16>::MakeRgbPixel32(255,255,255);
		BYTE r = AnimBitmap<16>::R(p);
		BYTE g = AnimBitmap<16>::G(p);
		BYTE b = AnimBitmap<16>::B(p);

    RegistryKey k;
    if(k.Open(false, false, "HKCU\\Software\\Hq2x Magnifier"))
    {
      k["Magnification"].GetDWORD(Magnification);
      k["TimedRefresh"].GetBool(TimedRefresh);
      k["TimedRefreshInterval"].GetDWORD(TimedRefreshInterval);
      k["RefreshOnKeyboard"].GetBool(RefreshOnKeyboard);
      k["RefreshOnMouse"].GetBool(RefreshOnMouse);
      k["TrackMouse"].GetBool(TrackMouse);
      k["TrackFocus"].GetBool(TrackFocus);
      k["TrackText"].GetBool(TrackText);
      k["AlwaysOnTop"].GetBool(AlwaysOnTop);
      k["Paused"].GetBool(Paused);
      k["ShowCursor"].GetBool(ShowCursor);
      k["ShowUpdateLight"].GetBool(ShowUpdateLight);
      k["ShowMagnification"].GetBool(ShowMagnification);
      k["ShowTitleBar"].GetBool(ShowTitleBar);
      k["removeClearType"].GetBool(removeClearType);
      k["useHQ2x"].GetBool(useHQ2x);
      k["WindowPlacement"].GetBinary2(&WindowPlacement, sizeof(WindowPlacement));
    }
  }

  void SaveSettings()
  {
    RegistryKey k(true, true, "HKCU\\Software\\Hq2x Magnifier");
    k["Magnification"].SetDWORD(Magnification);
    k["TimedRefresh"].SetBool(TimedRefresh);
    k["TimedRefreshInterval"].SetDWORD(TimedRefreshInterval);
    k["RefreshOnKeyboard"].SetBool(RefreshOnKeyboard);
    k["RefreshOnMouse"].SetBool(RefreshOnMouse);
    k["TrackMouse"].SetBool(TrackMouse);
    k["TrackFocus"].SetBool(TrackFocus);
    k["TrackText"].SetBool(TrackText);
    k["AlwaysOnTop"].SetBool(AlwaysOnTop);
    k["Paused"].SetBool(Paused);
    k["ShowCursor"].SetBool(ShowCursor);
    k["ShowUpdateLight"].SetBool(ShowUpdateLight);
    k["ShowMagnification"].SetBool(ShowMagnification);
    k["ShowTitleBar"].SetBool(ShowTitleBar);
    k["removeClearType"].SetBool(removeClearType);
    k["useHQ2x"].SetBool(useHQ2x);
    k["WindowPlacement"].SetBinary(&WindowPlacement, sizeof(WindowPlacement));
  }

  void LoadDefaults()
  {

		AnimBitmap<16>::RgbPixel p = AnimBitmap<16>::ScaleToIntensity(AnimBitmap<16>::MakeRgbPixel32(1,1,1), 2);


    Magnification = 3;

    TimedRefresh = true;
    TimedRefreshInterval = 500;
    RefreshOnKeyboard = true;
    RefreshOnMouse = true;

    TrackMouse = true;
    TrackFocus = false;
    TrackText = false;

    AlwaysOnTop = true;
    Paused = false;
    ShowCursor = true;
    ShowUpdateLight = true;
    ShowMagnification = true;
    ShowTitleBar = true;
    removeClearType = true;
    useHQ2x = true;

		WindowPlacement.length = sizeof(WindowPlacement);
		WindowPlacement.flags = 0;
		WindowPlacement.showCmd = SW_SHOWNORMAL;
		WindowPlacement.ptMaxPosition.x = -32000;
		WindowPlacement.ptMaxPosition.y = -32000;
		WindowPlacement.ptMinPosition.x = -1;
		WindowPlacement.ptMinPosition.y = -1;
		WindowPlacement.rcNormalPosition.left = 0;
		WindowPlacement.rcNormalPosition.top = 0;
		WindowPlacement.rcNormalPosition.right = 300;
		WindowPlacement.rcNormalPosition.bottom = 300;
  }
};


HINSTANCE g_hInstance;
HWND g_hWnd;
POINT g_p;
HHOOK g_hook;
HHOOK g_KeyHook;
AnimBitmap<16>* g_src;
AnimBitmap<32>* g_dst;
AnimBitmap<16>* g_temp;
AnimBitmap<32>* g_offscreen;

Settings* g_Settings;

bool g_Dragging;
POINT g_DragOriginCursor;
RECT g_DragOriginWindow;
bool g_bUpdateLightOn;

void RefreshMainDisplay()
{
  PostMessage(g_hWnd, WM_APP, 0,0);
}

void ResetTimer()
{
  KillTimer(g_hWnd, 0);
  if(g_Settings->TimedRefresh)
  {
    SetTimer(g_hWnd, 0, g_Settings->TimedRefreshInterval, 0);
  }
}

void ResetAlwaysOnTop()
{
  SetWindowPos(g_hWnd, g_Settings->AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

void RefreshTitleBar()
{
  DWORD dwStyle = WS_BORDER | WS_SYSMENU | WS_THICKFRAME;
  DWORD dwExStyle = WS_EX_TOOLWINDOW;
  if(g_Settings->ShowTitleBar)
  {
    dwStyle = WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_THICKFRAME;
    dwExStyle = WS_EX_TOOLWINDOW;
  }
  SetWindowLong(g_hWnd, GWL_STYLE, (LONG)dwStyle);
  SetWindowLong(g_hWnd, GWL_EXSTYLE, (LONG)dwExStyle);
  SetWindowPos(g_hWnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
  SetWindowPlacement(g_hWnd, &g_Settings->WindowPlacement);
}


INT_PTR CALLBACK CustomTimeProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg)
  {
  case WM_COMMAND:
    switch(LOWORD(wParam))
    {
    case IDOK:
      {
        DWORD dw = GetDlgItemInt(hWnd, IDC_MS, 0, TRUE);
        // validation
        if((dw < 3000000) || (dw > 0))
        {
          g_Settings->TimedRefreshInterval = dw;
          g_Settings->TimedRefresh = true;
          ResetTimer();
        }
        EndDialog(hWnd, 0);
        return TRUE;
      }
    case IDCANCEL:
      EndDialog(hWnd, 0);
      return TRUE;
    }
    return TRUE;
  case WM_INITDIALOG:
    SetDlgItemInt(hWnd, IDC_MS, g_Settings->TimedRefreshInterval, FALSE);
    SendDlgItemMessage(hWnd, IDC_MS, EM_SETSEL, 0, -1);
    SetFocus(GetDlgItem(hWnd, IDC_MS));
    return FALSE;
  }
  return FALSE;
}

void OnContextMenuSelect(int i)
{
  switch(i)
  {
  case ID_SIZE_1X:
    g_Settings->Magnification = 1;
    break;
  case ID_SIZE_2X:
    g_Settings->Magnification = 2;
    break;
  case ID_SIZE_3X:
    g_Settings->Magnification = 3;
    break;
  case ID_SIZE_4X:
    g_Settings->Magnification = 4;
    break;
  case ID_SIZE_6X:
    g_Settings->Magnification = 6;
    break;
  case ID_SIZE_8X:
    g_Settings->Magnification = 8;
    break;
  case ID_SIZE_9X:
    g_Settings->Magnification = 9;
    break;
  case ID_SIZE_12X:
    g_Settings->Magnification = 12;
    break;
  case ID_SIZE_16X:
    g_Settings->Magnification = 16;
    break;
  case ID_TIMEDREFRESHINTERVAL_OFF:
    g_Settings->TimedRefresh = !g_Settings->TimedRefresh;
    ResetTimer();
    break;
  case ID_INTERVAL_100MS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 100;
    ResetTimer();
    break;
  case ID_INTERVAL_250MS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 250;
    ResetTimer();
    break;
  case ID_INTERVAL_500MS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 500;
    ResetTimer();
    break;
  case ID_INTERVAL_750MS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 750;
    ResetTimer();
    break;
  case ID_INTERVAL_1000MS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 1000;
    ResetTimer();
    break;
  case ID_INTERVAL_2SECONDS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 2000;
    ResetTimer();
    break;
  case ID_INTERVAL_3SECONDS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 3000;
    ResetTimer();
    break;
  case ID_INTERVAL_4SECONDS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 4000;
    ResetTimer();
    break;
  case ID_INTERVAL_5SECONDS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 5000;
    ResetTimer();
    break;
  case ID_INTERVAL_7SECONSD:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 7000;
    ResetTimer();
    break;
  case ID_INTERVAL_10SECONDS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 10000;
    ResetTimer();
    break;
  case ID_INTERVAL_15SECONDS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 15000;
    ResetTimer();
    break;
  case ID_INTERVAL_20SECONDS:
    g_Settings->TimedRefresh = true;
    g_Settings->TimedRefreshInterval = 20000;
    ResetTimer();
    break;
  case ID_X_ALWAYSONTOP:
    g_Settings->AlwaysOnTop = !g_Settings->AlwaysOnTop;
    ResetAlwaysOnTop();
    break;
  case ID_X_PAUSE:
    g_Settings->Paused = !g_Settings->Paused;
    break;
  case ID_X_REFRESHONKEYBOARDEVENTS:
    g_Settings->RefreshOnKeyboard = !g_Settings->RefreshOnKeyboard;
    break;
  case ID_X_REFRESHONMOUSEEVENTS:
    g_Settings->RefreshOnMouse = !g_Settings->RefreshOnMouse;
    break;
  case ID_INTERVAL_CUSTOM:
    // NEED TO SHOW DIALOG
    DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_CUSTOMTIME), g_hWnd, CustomTimeProc, 1);
    break;
  case ID_X_EXIT:
    PostMessage(g_hWnd, WM_CLOSE, 0, 0);
    break;
  case ID_X_SHOWCURSOR:
    g_Settings->ShowCursor = !g_Settings->ShowCursor;
    break;
  case ID_X_SHOWUPDATELIGHT:
    g_Settings->ShowUpdateLight = !g_Settings->ShowUpdateLight;
    break;
  case ID_X_SHOWMAGNIFICATION:
    g_Settings->ShowMagnification = !g_Settings->ShowMagnification;
    break;
  case ID_X_SHOWTITLEBAR:
    g_Settings->ShowTitleBar = !g_Settings->ShowTitleBar;
    RefreshTitleBar();
    break;
  case ID_X_REMOVECLEARTYPEEFFECTS:
		g_Settings->removeClearType = !g_Settings->removeClearType;
    RefreshTitleBar();
    break;
  case ID_X_FOLLOWKEYBOARDFOCUS:
    g_Settings->TrackFocus = !g_Settings->TrackFocus;
    break;
  case ID_X_FOLLOWMOUSECURSOR:
    g_Settings->TrackMouse = !g_Settings->TrackMouse;
    break;
  case ID_X_FOLLOWKEYBOARDACTIVITY:
    g_Settings->TrackText = !g_Settings->TrackText;
    break;
  }

  return;
}


void CheckMenuItem2(HMENU h, bool Check, UINT id)
{
  CheckMenuItem(h, id, MF_BYCOMMAND | (Check ? MF_CHECKED : MF_UNCHECKED));
}

void CheckTimedIntervalDealie(HMENU h, long delay, UINT id, bool& DidWeUseIt)
{
  bool bCheck = g_Settings->TimedRefresh && (g_Settings->TimedRefreshInterval == delay);
  CheckMenuItem2(h, bCheck, id);
  if(bCheck) DidWeUseIt = true;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) 
	{
  case WM_CREATE:
    {
      SetWindowPlacement(hWnd, &g_Settings->WindowPlacement);
      return 0;
    }
  case WM_CAPTURECHANGED:
    {
      // cancel the move
      if(g_Dragging)
      {
        MoveWindow(hWnd, g_DragOriginWindow.left, g_DragOriginWindow.top, g_DragOriginWindow.right - g_DragOriginWindow.left, g_DragOriginWindow.bottom - g_DragOriginWindow.top, TRUE);
        g_Dragging = false;
      }
      return 0;
    }
  case WM_LBUTTONUP:
    {
      if(g_Dragging)
      {
        // commit the move
        g_Dragging = false;
        ReleaseCapture();
      }
      return 0;
    }
  case WM_MOUSEMOVE:
    {
      if(g_Dragging)
      {
        // move the window
        POINT p;
        GetCursorPos(&p);
        int dx = p.x - g_DragOriginCursor.x;
        int dy = p.y - g_DragOriginCursor.y;
        MoveWindow(hWnd,
          g_DragOriginWindow.left + dx,
          g_DragOriginWindow.top + dy,
          g_DragOriginWindow.right - g_DragOriginWindow.left,
          g_DragOriginWindow.bottom - g_DragOriginWindow.top,
          TRUE);
      }
      return 0;
    }
  case WM_LBUTTONDOWN:
    {
      // begin size move
      //GetCursorPos(&g_DragOriginCursor);
      g_DragOriginCursor.x = LOWORD(lParam);
      g_DragOriginCursor.y = HIWORD(lParam);
      ClientToScreen(hWnd, &g_DragOriginCursor);
      GetWindowRect(hWnd, &g_DragOriginWindow);
      g_Dragging = true;
      SetCapture(hWnd);
      return 0;
    }
  case WM_CONTEXTMENU:
    {
      HMENU h = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_CONTEXT));
      POINT p;
      GetCursorPos(&p);
      HMENU hSub = GetSubMenu(h, 0);

      // init the menu checkboxes and stuff.
      CheckMenuItem2(hSub, g_Settings->RefreshOnKeyboard, ID_X_REFRESHONKEYBOARDEVENTS);
      CheckMenuItem2(hSub, g_Settings->RefreshOnMouse, ID_X_REFRESHONMOUSEEVENTS);

      CheckMenuItem2(hSub, g_Settings->TrackMouse, ID_X_FOLLOWMOUSECURSOR);
      CheckMenuItem2(hSub, g_Settings->TrackFocus, ID_X_FOLLOWKEYBOARDFOCUS);
      CheckMenuItem2(hSub, g_Settings->TrackText, ID_X_FOLLOWKEYBOARDACTIVITY);

      CheckMenuItem2(hSub, g_Settings->AlwaysOnTop, ID_X_ALWAYSONTOP);
      CheckMenuItem2(hSub, g_Settings->Paused, ID_X_PAUSE);
      CheckMenuItem2(hSub, g_Settings->ShowCursor, ID_X_SHOWCURSOR);
      CheckMenuItem2(hSub, g_Settings->ShowUpdateLight, ID_X_SHOWUPDATELIGHT);
      CheckMenuItem2(hSub, g_Settings->ShowMagnification, ID_X_SHOWMAGNIFICATION);
      CheckMenuItem2(hSub, g_Settings->ShowTitleBar, ID_X_SHOWTITLEBAR);
			CheckMenuItem2(hSub, g_Settings->removeClearType, ID_X_REMOVECLEARTYPEEFFECTS);

      //HMENU hMagMenu = GetSubMenu(hSub, 0);
      CheckMenuItem2(hSub, g_Settings->Magnification == 1, ID_SIZE_1X);
      CheckMenuItem2(hSub, g_Settings->Magnification == 2, ID_SIZE_2X);
      CheckMenuItem2(hSub, g_Settings->Magnification == 3, ID_SIZE_3X);
      CheckMenuItem2(hSub, g_Settings->Magnification == 4, ID_SIZE_4X);
      CheckMenuItem2(hSub, g_Settings->Magnification == 6, ID_SIZE_6X);
      CheckMenuItem2(hSub, g_Settings->Magnification == 8, ID_SIZE_8X);
      CheckMenuItem2(hSub, g_Settings->Magnification == 9, ID_SIZE_9X);
      CheckMenuItem2(hSub, g_Settings->Magnification == 12, ID_SIZE_12X);
      CheckMenuItem2(hSub, g_Settings->Magnification == 16, ID_SIZE_16X);

      //HMENU hRefreshMenu = GetSubMenu(hSub, 1);
      CheckMenuItem2(hSub, !g_Settings->TimedRefresh, ID_TIMEDREFRESHINTERVAL_OFF);
      bool bUsed;
      CheckTimedIntervalDealie(hSub, 100, ID_INTERVAL_100MS, bUsed);
      CheckTimedIntervalDealie(hSub, 250, ID_INTERVAL_250MS, bUsed);
      CheckTimedIntervalDealie(hSub, 500, ID_INTERVAL_500MS, bUsed);
      CheckTimedIntervalDealie(hSub, 750, ID_INTERVAL_750MS, bUsed);
      CheckTimedIntervalDealie(hSub, 1000, ID_INTERVAL_1000MS, bUsed);
      CheckTimedIntervalDealie(hSub, 2000, ID_INTERVAL_2SECONDS, bUsed);
      CheckTimedIntervalDealie(hSub, 3000, ID_INTERVAL_3SECONDS, bUsed);
      CheckTimedIntervalDealie(hSub, 4000, ID_INTERVAL_4SECONDS, bUsed);
      CheckTimedIntervalDealie(hSub, 5000, ID_INTERVAL_5SECONDS, bUsed);
      CheckTimedIntervalDealie(hSub, 7000, ID_INTERVAL_7SECONSD, bUsed);
      CheckTimedIntervalDealie(hSub, 10000, ID_INTERVAL_10SECONDS, bUsed);
      CheckTimedIntervalDealie(hSub, 15000, ID_INTERVAL_15SECONDS, bUsed);
      CheckTimedIntervalDealie(hSub, 20000, ID_INTERVAL_20SECONDS, bUsed);
      CheckMenuItem2(hSub, !bUsed && g_Settings->TimedRefresh, ID_INTERVAL_CUSTOM);

      BOOL b = TrackPopupMenuEx(hSub, TPM_NONOTIFY | TPM_RETURNCMD, p.x, p.y, hWnd, 0);
      if(b)
      {
        OnContextMenuSelect(b);
      }

      DestroyMenu(h);
      return 0;
    }
  //case WM_GETMINMAXINFO:
  //  {
  //    MINMAXINFO* p = (MINMAXINFO*)lParam;
  //    p->ptMinTrackSize.x = 50;
  //    p->ptMinTrackSize.y = 50;
  //    return 0;
  //  }
  case WM_CHAR:
    {
      switch(wParam)
      {
      case 27:// ESC key
        if(g_Dragging)
        {
          MoveWindow(hWnd, g_DragOriginWindow.left, g_DragOriginWindow.top, g_DragOriginWindow.right - g_DragOriginWindow.left, g_DragOriginWindow.bottom - g_DragOriginWindow.top, TRUE);
          g_Dragging = false;
          ReleaseCapture();
        }
        break;
      case '1':
      case '2':
      case '3':
      case '4':
      case '6':
      case '8':
      case '9':
        g_Settings->Magnification = (long)(wParam - '0');
        PostMessage(g_hWnd, WM_APP, 0,0);
        break;
      case '@':
        g_Settings->Magnification = 12;
        PostMessage(g_hWnd, WM_APP, 0,0);
        break;
      case '^':
        g_Settings->Magnification = 16;
        PostMessage(g_hWnd, WM_APP, 0,0);
        break;
      case 'p':
      case 'P':
        g_Settings->Paused = !g_Settings->Paused;
        PostMessage(g_hWnd, WM_APP, 0,0);
        break;
      case 'a':
      case 'A':
        g_Settings->AlwaysOnTop = !g_Settings->AlwaysOnTop;
        SetWindowPos(g_hWnd, g_Settings->AlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        PostMessage(g_hWnd, WM_APP, 0,0);
        break;
			case 'c':
      case 'C':
				g_Settings->removeClearType = !g_Settings->removeClearType;
        PostMessage(g_hWnd, WM_APP, 0,0);
        break;
			case 'r':
      case 'R':
				g_Settings->useHQ2x = !g_Settings->useHQ2x;
        PostMessage(g_hWnd, WM_APP, 0,0);
        break;
      }
      return 0;
    }
	case WM_PAINT:
    {
	    PAINTSTRUCT ps;
	    HDC hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      PostMessage(g_hWnd, WM_APP, 0,0);
		  return 0;
    }
  case WM_TIMER:
  case WM_APP:
    {
			OutputDebugString("WM_TIMER---------------------------------------------\r\n");
      RECT rc;
      GetClientRect(hWnd, &rc);

      g_offscreen->SetSize(rc.right, rc.bottom);
      g_offscreen->Fill(0);

      if(!g_Settings->Paused)
      {
        // set source size
        g_src->SetSize(rc.right / g_Settings->Magnification, rc.bottom / g_Settings->Magnification);
	      g_src->Fill(0);// necessary for bleed

        // capture screen
        HDC dcScreen = CreateDC(TEXT("DISPLAY"), 0, 0, 0);
        g_src->BlitFrom(dcScreen, g_p.x - ((rc.right / g_Settings->Magnification) / 2), g_p.y - ((rc.bottom / g_Settings->Magnification) / 2), rc.right / g_Settings->Magnification, rc.bottom / g_Settings->Magnification, 0, 0);
        DeleteDC(dcScreen);
				OutputDebugString("Captuing screen\r\n");

				if(g_Settings->removeClearType)
					g_src->RemoveClearType();

        if(g_Settings->ShowCursor)
        {
          CURSORINFO ci = {0};
          ci.cbSize = sizeof(ci);
          if(GetCursorInfo(&ci))
          {
            ICONINFO ii = {0};
            GetIconInfo(ci.hCursor, &ii);
            DrawIcon(g_src->m_offscreen, (g_src->GetWidth() / 2) - ii.xHotspot, (g_src->GetHeight() / 2) - ii.yHotspot, ci.hCursor);
            DeleteObject(ii.hbmColor);
            DeleteObject(ii.hbmMask);
          }
        }

				if(!g_Settings->useHQ2x)
				{
					g_dst->SetSize(g_src->GetWidth() * g_Settings->Magnification, g_src->GetHeight() * g_Settings->Magnification);
					g_src->StretchBlit(*g_dst, 0, 0, g_src->GetWidth() * g_Settings->Magnification, g_src->GetHeight() * g_Settings->Magnification, 0, 0, g_src->GetWidth(), g_src->GetHeight());
					OutputDebugString("StretchBlit\r\n");
				}
				else
				{
					switch(g_Settings->Magnification)
					{
					case 1:
						g_src->Blit(*g_dst, 0, 0);
						break;
					case 3:
						g_src->hq3x(*g_dst);
						break;
					case 4:
						g_src->hq4x(*g_dst);
						break;
					case 6:
						{
							g_src->hq3x(*g_dst);
							g_temp->SetSize(g_dst->GetWidth(), g_dst->GetHeight());
							g_dst->Blit(*g_temp, 0, 0);
							g_temp->hq2x(*g_dst);
							break;
						}
					case 8:
						{
							g_src->hq4x(*g_dst);
							g_temp->SetSize(g_dst->GetWidth(), g_dst->GetHeight());
							g_dst->Blit(*g_temp, 0, 0);
							g_temp->hq2x(*g_dst);
							break;
						}
					case 9:
						{
							g_src->hq3x(*g_dst);
							g_temp->SetSize(g_dst->GetWidth(), g_dst->GetHeight());
							g_dst->Blit(*g_temp, 0, 0);
							g_temp->hq3x(*g_dst);
							break;
						}
					case 12:
						{
							g_src->hq3x(*g_dst);
							g_temp->SetSize(g_dst->GetWidth(), g_dst->GetHeight());
							g_dst->Blit(*g_temp, 0, 0);
							g_temp->hq4x(*g_dst);
							break;
						}
					case 16:
						{
							g_src->hq4x(*g_dst);
							g_temp->SetSize(g_dst->GetWidth(), g_dst->GetHeight());
							g_dst->Blit(*g_temp, 0, 0);
							g_temp->hq4x(*g_dst);
							break;
						}
					default:
						OutputDebugString("hq2x\r\n");
						g_src->hq2x(*g_dst);
						break;
					}
				}
        g_dst->Blit(*g_offscreen, 0, 0);
      }// if !paused
      else
      {
        // if we are paused, try to draw the existing dest thing.
        g_dst->Blit(*g_offscreen, 0, 0);
      }

      if(g_Settings->ShowUpdateLight)
      {
        if(g_bUpdateLightOn)
        {
          long w = g_offscreen->GetWidth();
          long h = g_offscreen->GetHeight();
          g_offscreen->Rect(w - min(8, h), 0, w, min(8, h), RGB(0,0,255));
        }
        g_bUpdateLightOn = !g_bUpdateLightOn;
      }

      if(g_Settings->ShowMagnification)
      {
        char s[100];
        wsprintf(s, "%lux", g_Settings->Magnification);
        g_offscreen->_DrawText(s, 0, 0);
      }

      // blit offscreen to screen
      HDC dc = GetDC(hWnd);
      g_offscreen->Blit(dc, 0, 0);
      ReleaseDC(hWnd, dc);

      return 0;
    }
  case WM_SIZE:
	case WM_WINDOWPOSCHANGED:
		{
			g_Settings->WindowPlacement.length = sizeof(g_Settings->WindowPlacement);
			GetWindowPlacement(hWnd, &g_Settings->WindowPlacement);
			break;
		}
  case WM_ERASEBKGND:
    return FALSE;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}


LRESULT CALLBACK LowLevelMouseProc( int nCode, WPARAM wParam, LPARAM lParam )
{
  if(g_Settings->TrackMouse)
  {
    MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;
    g_p.x = p->pt.x;
    g_p.y = p->pt.y;
  }
  if(g_Settings->RefreshOnMouse)
  {
    PostMessage(g_hWnd, WM_APP, 0,0);
  }
  return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam )
{
  if(g_Settings->RefreshOnKeyboard)
  {
    PostMessage(g_hWnd, WM_APP, 0,0);
  }
  return CallNextHookEx(g_KeyHook, nCode, wParam, lParam);
}


//#ifdef _DEBUG
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
//#else
//int WinMainCRTStartup()
//{
  //HINSTANCE hInstance = GetModuleHandle(0);
//#endif
  WNDCLASSEX wcex = {0};
	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.hInstance		= hInstance;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= 0;
	wcex.lpszClassName	= "aoeu";
	RegisterClassEx(&wcex);

  AnimBitmap<16> src;
  AnimBitmap<16> temp;
  AnimBitmap<32> dest;
  AnimBitmap<32> offscreen;
  Settings s;

  g_Settings = &s;
  g_Settings->LoadDefaults();
  g_Settings->LoadSettings();
  g_src = &src;
  g_dst = &dest;
  g_temp = &temp;
  g_offscreen = &offscreen;

  g_hInstance = hInstance; // Store instance handle in our global variable
  g_hWnd = CreateWindowEx(WS_EX_TOOLWINDOW, "aoeu", "hq2x magnifier.",
    WS_BORDER | WS_SYSMENU | WS_THICKFRAME,
    CW_USEDEFAULT, 0, 200, 200, NULL, NULL, hInstance, NULL);
  ShowWindow(g_hWnd, SW_NORMAL);

  InitLUTs();

  g_Dragging = false;
  g_bUpdateLightOn = false;

  ResetTimer();
  ResetAlwaysOnTop();
  RefreshTitleBar();

  //APPBARDATA ad = {0};
  //ad.cbSize = sizeof(ad);
  //ad.hWnd = g_hWnd;
  //ad.uCallbackMessage = WM_APP + 2;
  //UINT_PTR x = SHAppBarMessage(ABM_NEW, &ad);

  //SetRect(&ad.rc, 0, 0, GetSystemMetrics(SM_CXSCREEN), 100);
  //ad.uEdge=ABE_TOP;
  //x = SHAppBarMessage(ABM_QUERYPOS, &ad);
  //x = SHAppBarMessage(ABM_SETPOS, &ad);

  //MoveWindow(ad.hWnd, ad.rc.left, ad.rc.top, 
  //    ad.rc.right - ad.rc.left, 
  //    ad.rc.bottom - ad.rc.top, TRUE); 

  g_KeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
  g_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

  g_Settings->SaveSettings();

  //x = SHAppBarMessage(ABM_REMOVE, &ad);

  UnhookWindowsHookEx(g_hook);
  UnhookWindowsHookEx(g_KeyHook);
  UninitLUTs();

	return (int) msg.wParam;
}

