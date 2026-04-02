//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
//	[MFC component]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_com_h)
#define mfc_com_h

//===========================================================================
//
//	Component
//
//===========================================================================
class CComponent : public CObject
{
public:
	// Basic functions
	CComponent(CFrmWnd *pFrmWnd);
										// Constructor
	virtual ~CComponent();
										// Destructor
	virtual BOOL FASTCALL Init();
										// Initialization
	virtual void FASTCALL Cleanup();
										// Cleanup
	virtual void FASTCALL Enable(BOOL bEnable)	{ m_bEnable = bEnable; }
										// Enable control
	BOOL FASTCALL IsEnable() const		{ return m_bEnable; }
										// Check whether the component is enabled
	virtual BOOL FASTCALL Save(Fileio *pFio, int nVer);
										// Save
	virtual BOOL FASTCALL Load(Fileio *pFio, int nVer);
										// Load
	virtual void FASTCALL ApplyCfg(const Config *pConfig);
										// Apply configuration
#if !defined(NDEBUG)
		void AssertValid() const;
										// Diagnostics
#endif	// NDEBUG

	// Properties
	DWORD FASTCALL GetID() const		{ return m_dwID; }
										// Get the ID
	void FASTCALL GetDesc(CString& strDesc) const;
										// Get the name

	// Component management
	CComponent* FASTCALL SearchComponent(DWORD dwID);
										// Search for a component
	void FASTCALL AddComponent(CComponent *pNewComponent);
										// Add a component
	CComponent* FASTCALL GetPrevComponent() const	{ return m_pPrev; }
										// Get the previous component
	CComponent* FASTCALL GetNextComponent() const	{ return m_pNext; }
										// Get the next component

protected:
	CFrmWnd *m_pFrmWnd;
										// Frame window
	DWORD m_dwID;
										// Component ID
	BOOL m_bEnable;
										// Enabled flag
	CString m_strDesc;
										// Name
	CComponent *m_pPrev;
										// Previous component
	CComponent *m_pNext;
										// Next component
};

#endif	// mfc_com_h
#endif	// _WIN32
