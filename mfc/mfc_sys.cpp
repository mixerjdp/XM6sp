//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC Sub-Window (System) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "log.h"
#include "device.h"
#include "vm.h"
#include "schedule.h"
#include "event.h"
#include "render.h"
#include "mfc_sub.h"
#include "mfc_frm.h"
#include "mfc_draw.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_res.h"
#include "mfc_cpu.h"
#include "mfc_sys.h"

//===========================================================================
//
//	Log Window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CLogWnd::CLogWnd() : CSubListWnd()
{
	// Window parameter definition
	m_dwID = MAKEID('L', 'O', 'G', 'L');
	::GetMsg(IDS_SWND_LOG, m_strCaption);
	m_nWidth = 64;
	m_nHeight = 8;

	// No focus
	m_nFocus = -1;

	// Log
	m_pLog = NULL;
}

//---------------------------------------------------------------------------
//
//	Message Map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CLogWnd, CSubListWnd)
	ON_WM_CREATE()
	ON_WM_DRAWITEM()
	ON_NOTIFY(NM_DBLCLK, 0, OnDblClick)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CLogWnd::OnCreate(LPCREATESTRUCT lpcs)
{
 ASSERT(this);

 // Base class
 if (CSubListWnd::OnCreate(lpcs) != 0) {
  return -1;
 }

 // Load messages
 ::GetMsg(IDS_SWND_LOG_DETAIL, m_strDetail);
 ::GetMsg(IDS_SWND_LOG_NORMAL, m_strNormal);
 ::GetMsg(IDS_SWND_LOG_WARNING, m_strWarning);

 // Get log
 ASSERT(!m_pLog);
 m_pLog = &(::GetVM()->log);
 ASSERT(m_pLog);

 return 0;
}

//---------------------------------------------------------------------------
//
//	Double click
//
//---------------------------------------------------------------------------
void CLogWnd::OnDblClick(NMHDR* /*pNotifyStruct*/, LRESULT* /*result*/)
{
 Log::logdata_t logdata;
 BOOL bSuccess;
 CDrawView *pView;
 CDisasmWnd *pDisWnd;

 ASSERT(this);
 ASSERT_VALID(this);

 // Return if no focus
 if (m_nFocus < 0) {
  return;
 }

 // Get log data and allocate working buffer
 bSuccess = m_pLog->GetData(m_nFocus, &logdata);
 if (!bSuccess) {
  // Buffer doesn't need freeing
  return;
 }

 // Set address in address window
 pView = (CDrawView*)GetParent();
 pDisWnd = (CDisasmWnd*)pView->SearchSWnd(MAKEID('D', 'I', 'S', 'A'));
 if (pDisWnd) {
  pDisWnd->SetAddr(logdata.pc);
 }

 // Free buffer allocated by GetData()
 delete[] logdata.string;
}

//---------------------------------------------------------------------------
//
//	Item draw
//
//---------------------------------------------------------------------------
void CLogWnd::OnDrawItem(int nID, LPDRAWITEMSTRUCT lpDIS)
{
 CDC dc;
 CDC mDC;
 CBitmap Bitmap;
 CBitmap *pBitmap;
 CFont *pFont;
 CRect rectItem(&lpDIS->rcItem);
 CRect rectClient;
 CString strText;
 Log::logdata_t logdata;
 CSize size;
 int nItem;
 int nWidth;
#if !defined(UNICODE)
 wchar_t szWide[100];
#endif	// UNICODE

 ASSERT(this);
 ASSERT(nID == 0);
 ASSERT(lpDIS);
 ASSERT_VALID(this);

 // Attach DC
 VERIFY(dc.Attach(lpDIS->hDC));

 // If disabled draw blank
 if (!m_bEnable) {
  dc.FillSolidRect(&rectItem, ::GetSysColor(COLOR_WINDOW));
  dc.Detach();
  return;
 }

 // Get log data
 if (!m_pLog->GetData(lpDIS->itemID, &logdata)) {
  // If none display blank
  dc.FillSolidRect(&rectItem, ::GetSysColor(COLOR_WINDOW));
  dc.Detach();
  return;
 }

 // Create compatible bitmap
 VERIFY(mDC.CreateCompatibleDC(&dc));
 VERIFY(Bitmap.CreateCompatibleBitmap(&dc, rectItem.Width(), rectItem.Height()));
 pBitmap = mDC.SelectObject(&Bitmap);
 ASSERT(pBitmap);

 // Select font
 pFont = mDC.SelectObject(m_ListCtrl.GetFont());
 ASSERT(pFont);

 // Set background color and determine highlight
 rectClient.left = 0;
 rectClient.right = rectItem.Width();
 rectClient.top = 0;
 rectClient.bottom = rectItem.Height();
 if (lpDIS->itemState & ODS_FOCUS) {
  // Remember focus number
  m_nFocus = lpDIS->itemID;
  mDC.FillSolidRect(&rectClient, GetSysColor(COLOR_HIGHLIGHT));
  mDC.SetTextColor(::GetSysColor(COLOR_HIGHLIGHTTEXT));
  mDC.SetBkColor(::GetSysColor(COLOR_HIGHLIGHT));
 }
 else {
  // No focus
  mDC.FillSolidRect(&rectClient, ::GetSysColor(COLOR_WINDOW));
  mDC.SetBkColor(::GetSysColor(COLOR_WINDOW));
 }

 // Display sub-items by column
 for (nItem=0; nItem<6; nItem++) {
  // Create coordinates
  nWidth = m_ListCtrl.GetColumnWidth(nItem);
  rectClient.right = rectClient.left + nWidth;

  // Get text
  TextSub(nItem, &logdata, strText);

  // Color settings
  if (!(lpDIS->itemState & ODS_FOCUS)) {
   mDC.SetTextColor(RGB(0, 0, 0));
   if (nItem == 3) {
    switch (logdata.level) {
     // Detail (gray)
     case Log::Detail:
      mDC.SetTextColor(RGB(192, 192, 192));
      break;
     // Warning (red)
     case Log::Warning:
      mDC.SetTextColor(RGB(255, 0, 0));
      break;
    }
   }
  }

  // Display
  switch (nItem) {
   // No. (right align)
   case 0:
   // Time (right align)
   case 1:
    mDC.DrawText(strText, &rectClient, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
    break;

   // PC (center align)
   case 2:
   // Level (center align)
   case 3:
   // Device (center align)
   case 4:
    mDC.DrawText(strText, &rectClient, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    break;

   // Message (left align)
   case 5:
#if !defined(UNICODE)
    // If not Japanese, display VM's Shift-JIS as-is
    if (::IsJapanese()) {
     // Japanese
     mDC.DrawText(strText, &rectClient, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
     break;
    }

    // CP932 support
    if (::Support932()) {
     // CP932 support (UNICODE is supported)
     memset(&szWide[0], 0, sizeof(szWide));
     if (_tcslen(strText) < 0x100) {
      // Convert Shift-JIS to UNICODE
      ::MultiByteToWideChar(932, MB_PRECOMPOSED, (LPCSTR)(LPCTSTR)strText, -1, szWide, 100);

      // Display as UNICODE
      ::DrawTextWide(mDC.m_hDC, szWide, -1, rectClient, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
     }
     break;
    }

    // Cannot display
    if (::IsWinNT()) {
     strText = _T("CP932(Japanese Shift-JIS) is required");
    }
    else {
     strText = _T("(Japanese Text)");
    }
#endif	// !UNICODE
    mDC.DrawText(strText, &rectClient, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    break;

   // Others (should not happen)
   default:
    ASSERT(FALSE);
  }

  // Update rectangle
  rectClient.left += nWidth;
 }

 // Complete display
 VERIFY(dc.BitBlt(rectItem.left, rectItem.top, rectItem.Width(), rectItem.Height(),
   &mDC, 0, 0, SRCCOPY));
 VERIFY(mDC.SelectObject(pFont));
 VERIFY(mDC.SelectObject(pBitmap));
 VERIFY(Bitmap.DeleteObject());
 VERIFY(mDC.DeleteDC());
 dc.Detach();

 // Free working buffer
 delete[] logdata.string;
 nID = 0;
}

//---------------------------------------------------------------------------
//
//	List control initialization
//
//---------------------------------------------------------------------------
void FASTCALL CLogWnd::InitList()
{
 CString strHeader;

 ASSERT(this);
 ASSERT_VALID(this);

 // Column 0 (Number)
 ::GetMsg(IDS_SWND_LOG_NUMBER, strHeader);
 m_ListCtrl.InsertColumn(0, strHeader, LVCFMT_RIGHT, m_tmWidth * 7, 0);

 // Column 1 (Time)
 ::GetMsg(IDS_SWND_LOG_TIME, strHeader);
 m_ListCtrl.InsertColumn(1, strHeader, LVCFMT_RIGHT, m_tmWidth * 10, 1);

 // Column 2 (PC)
 ::GetMsg(IDS_SWND_LOG_PC, strHeader);
 m_ListCtrl.InsertColumn(2, strHeader, LVCFMT_CENTER, m_tmWidth * 8, 2);

 // Column 3 (Level)
 ::GetMsg(IDS_SWND_LOG_LEVEL, strHeader);
 m_ListCtrl.InsertColumn(3, strHeader, LVCFMT_CENTER, m_tmWidth * 6, 3);

 // Column 4 (Device)
 ::GetMsg(IDS_SWND_LOG_DEVICE, strHeader);
 m_ListCtrl.InsertColumn(4, strHeader, LVCFMT_CENTER, m_tmWidth * 8, 4);

 // Column 5 (Message)
 ::GetMsg(IDS_SWND_LOG_MSG, strHeader);
 m_ListCtrl.InsertColumn(5, strHeader, LVCFMT_LEFT, m_tmWidth * 32, 5);

 // Delete all
 m_ListCtrl.DeleteAllItems();
}

//---------------------------------------------------------------------------
//
//	Refresh
//
//---------------------------------------------------------------------------
void FASTCALL CLogWnd::Refresh()
{
 // Since message thread updates via Update, do nothing
}

//---------------------------------------------------------------------------
//
//	Message thread update
//
//---------------------------------------------------------------------------
void FASTCALL CLogWnd::Update()
{
 int nItem;
 int nLog;
 BOOL bRun;
 CString strEmpty;

 ASSERT(this);
 ASSERT_VALID(this);

 // If disabled do nothing
 if (!m_bEnable) {
  return;
 }

 // Get current count and increase items
 nItem = m_ListCtrl.GetItemCount();
 nLog = m_pLog->GetNum();

 // Reduce
 if (nItem > nLog) {
  m_ListCtrl.DeleteAllItems();
  nItem = 0;
 }

 // Increase
 if (nItem < nLog) {
  // Dummy string for insertion
  strEmpty.Empty();

  // If 1024 items or more, suspend VM and add items
  if ((nLog - nItem) >= 0x400) {
   // Suspend VM because bulk addition takes time
   bRun = m_pSch->IsEnable();
   m_pSch->Enable(FALSE);

   // Add
   while (nItem < nLog) {
    m_ListCtrl.InsertItem(nItem, strEmpty);
    nItem++;
   }

   // Resume VM
   m_pSch->Enable(bRun);
   m_pSch->Reset();
  }
  else {
   // Since less than 1024 items, add without VM suspension
   while (nItem < nLog) {
    m_ListCtrl.InsertItem(nItem, strEmpty);
    nItem++;
   }
  }
 }

 // Drawing update request is not needed since it only updates the list
}

//---------------------------------------------------------------------------
//
//	Text format
//
//---------------------------------------------------------------------------
void FASTCALL CLogWnd::TextSub(int nType, Log::logdata_t *pLogData, CString& string)
{
 DWORD dwTime;
 CString strTemp;
#if defined(UNICODE)
 wchar_t szWide[100];
#endif	// UNICODE

 ASSERT(this);
 ASSERT((nType >= 0) && (nType <= 5));
 ASSERT(pLogData);

 switch (nType) {
  // Number
  case 0:
   string.Format("%d", pLogData->number);
   return;

  // Time (s.ms.us)
  case 1:
   dwTime = pLogData->time;
   dwTime /= 2;
   strTemp.Format("%d.", dwTime / (1000 * 1000));
   string = strTemp;
   dwTime %= (1000 * 1000);
   strTemp.Format("%03d.", dwTime / 1000);
   string += strTemp;
   dwTime %= 1000;
   strTemp.Format("%03d", dwTime);
   string += strTemp;
   return;

  // PC
  case 2:
   string.Format("$%06X", pLogData->pc);
   return;

  // Level
  case 3:
   switch (pLogData->level) {
    // Detail
    case Log::Detail:
     string = m_strDetail;
     break;
    // Normal
    case Log::Normal:
     string = m_strNormal;
     break;
    // Warning
    case Log::Warning:
     string = m_strWarning;
     break;
    // Others (should not happen)
    default:
     ASSERT(FALSE);
     string.Empty();
     break;
   }
   return;

  // Device
  case 4:
   string.Format(_T("%c%c%c%c"),
    (BYTE)(pLogData->id >> 24),
    (BYTE)(pLogData->id >> 16),
    (BYTE)(pLogData->id >> 8),
    (BYTE)(pLogData->id));
   return;

  // Message
  case 5:
#if defined(UNICODE)
   // Convert from CP932 if supported
   if (::Support932()) {
    // Convert Shift-JIS to UNICODE
    ::MultiByteToWideChar(932, MB_PRECOMPOSED, (LPCSTR)pLogData->string, -1, szWide, 100);

    // Replace
    string = szWide;
   }
   else {
    if (::IsWinNT()) {
     // UNICODE supported but CP932 not available
     string = _T("CP932(Japanese Shift-JIS) is required");
    }
    else {
     // Since not UNICODE, display on Win9x
     string = _T("(Japanese Text)");
    }
   }
#else
   // Since not UNICODE, string is multibyte
   string = pLogData->string;
#endif	// UNICODE
   return;

  // Others (should not happen)
  default:
   break;
 }

 // Should not happen normally
 ASSERT(FALSE);
}

//===========================================================================
//
//	Scheduler Window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSchedulerWnd::CSchedulerWnd()
{
 // Window parameter definition
 m_dwID = MAKEID('S', 'C', 'H', 'E');
 ::GetMsg(IDS_SWND_SCHEDULER, m_strCaption);
 m_nWidth = 72;
 m_nHeight = 23;

 // Get scheduler
 m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
 ASSERT(m_pScheduler);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CSchedulerWnd::Setup()
{
 int x;
 int y;
 int nIndex;
 DWORD dwRate;
 Event *pEvent;
 Device *pDevice;
 DWORD dwID;
 CString strText;
 Scheduler::scheduler_t sch;

 ASSERT(this);
 ASSERT(m_pSch);
 ASSERT(m_pScheduler);

 // Get scheduler structure
 m_pScheduler->GetScheduler(&sch);

 // Clear
 Clear();
 x = 44;

 // Left side
 y = 0;

 // Passed time
 SetString(0, y, _T("Passed Time"));
 strText.Format(_T("%12dms"), sch.total / 2000);
 SetString(15, y, strText);
 y++;

 // Execution time
 SetString(0, y, _T("Execution Time"));
 strText.Format(_T("%6d.%1dus"), sch.one / 2, (sch.one & 1) * 5);
 SetString(19, y, strText);
 y++;

 // Frame rate
 dwRate = m_pSch->GetFrameRate();
 SetString(0, y, _T("Frame Rate"));
 strText.Format(_T("%2d.%1dfps"), dwRate / 10, (dwRate % 10));
 SetString(22, y, strText);

 // Right side
 y = 0;

 // CPU usage (execution speed)
 SetString(x, y, _T("MPU Speed"));
 strText.Format(_T("%d.%02dMHz"), sch.speed / 100, sch.speed % 100);
 if (sch.speed >= 1000) {
  SetString(x + 20, y, strText);
 }
 else {
  SetString(x + 21, y, strText);
 }
 y++;

 // CPU usage (over cycle)
 SetString(x, y, _T("MPU Over Cycle"));
 strText.Format(_T("%5u"), sch.cycle);
 SetString(x + 23, y, strText);
 y++;

 // CPU usage (over time)
 SetString(x, y, _T("MPU Over Time"));
 strText.Format(_T("%3d.%1dus"), sch.time / 2, (sch.time & 1) * 5);
 SetString(x + 21, y, strText);
 y++;

 // Event list header
 y++;
 SetString(0, y, _T("No."));
 SetString(6, y, _T("Remain"));
 SetString(20, y, _T("Time"));
 SetString(34, y, _T("User"));
 SetString(42, y, _T("Flag"));
 SetString(48, y, _T("Device"));
 SetString(56, y, _T("Description"));
 y++;

 // Event loop
 pEvent = m_pScheduler->GetFirstEvent();
 nIndex = 0;
 while (pEvent) {
  // Basic parameters
  strText.Format(_T("%2d %5d.%04dms (%5d.%04dms) $%08X "),
   nIndex + 1,
   pEvent->GetRemain() / 2000,
   (pEvent->GetRemain() % 2000) * 5,
   pEvent->GetTime() / 2000,
   (pEvent->GetTime() % 2000) * 5,
   pEvent->GetUser());
  SetString(0, y, strText);

  // Enable flag
  if (pEvent->GetTime() != 0) {
   SetString(42, y, _T("Enable"));
  }

  // Device name
  pDevice = pEvent->GetDevice();
  ASSERT(pDevice);
  dwID = pDevice->GetID();
  strText.Format(_T("%c%c%c%c"),
     (BYTE)(dwID >> 24),
     (BYTE)(dwID >> 16),
     (BYTE)(dwID >> 8),
     (BYTE)dwID);
  SetString(49, y, strText);

  // Description
  SetString(54, y, pEvent->GetDesc());

  // Next event
  pEvent = pEvent->GetNextEvent();
  nIndex++;
  y++;
 }
}

//---------------------------------------------------------------------------
//
//	Message thread update
//
//---------------------------------------------------------------------------
void FASTCALL CSchedulerWnd::Update()
{
 int nNum;

 ASSERT(this);

 // Get scheduler event count and resize
 nNum = m_pScheduler->GetEventNum();
 ReSize(m_nWidth, nNum + 5);
}

//===========================================================================
//
//	Device Window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDeviceWnd::CDeviceWnd()
{
 // Window parameter definition
 m_dwID = MAKEID('D', 'E', 'V', 'I');
 ::GetMsg(IDS_SWND_DEVICE, m_strCaption);

 m_nWidth = 57;
 m_nHeight = 32;
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CDeviceWnd::Setup()
{
 int nDevice;
 const Device *pDevice;
 const MemDevice *pMemDev;
 CString strText;
 CString strTemp;
 DWORD dwID;
 DWORD dwMemID;
 BOOL bMem;

 ASSERT(this);

 // Clear
 Clear();

 // Get first device and create header ID
 pDevice = ::GetVM()->GetFirstDevice();
 ASSERT(pDevice);
 dwMemID = MAKEID('M', 'E', 'M', ' ');
 bMem = FALSE;

 SetString(0, 0, _T("No. Device Object"));
 SetString(23, 0, _T("Address Range"));
 SetString(41, 0, _T("Description"));

 // Loop
 nDevice = 0;
 while (pDevice) {
  // Number
  strText.Format(_T("%2d  "), nDevice + 1);

  // ID
  dwID = pDevice->GetID();
  strTemp.Format(_T("%c%c%c%c"),
     (BYTE)(dwID >> 24),
     (BYTE)(dwID >> 16),
     (BYTE)(dwID >> 8),
     (BYTE)(dwID));
  strText += strTemp;

  // Pointer
  strTemp.Format(_T("  %08x  "), pDevice);
  strText += strTemp;

  // Memory flag
  if (dwID == dwMemID) {
   bMem = TRUE;
  }

  // Address range
  if (bMem) {
   pMemDev = (MemDevice *)pDevice;
   strTemp.Format(_T("$%06X - $%06X"),
     pMemDev->GetFirstAddr(), pMemDev->GetLastAddr());
  }
  else {
   strTemp = _T("(NO MEMORY)");
  }
  strText += strTemp;

  // Set
  SetString(0, nDevice + 1, strText);

  // Description
  strText = pDevice->GetDesc();
  SetString(39, nDevice + 1, strText);

  // Next
  nDevice++;
  pDevice = pDevice->GetNextDevice();
 }
}

#endif	// _WIN32