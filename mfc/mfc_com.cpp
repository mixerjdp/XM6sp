//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
//	[MFC component]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "mfc_frm.h"
#include "mfc_com.h"

//===========================================================================
//
//	Component
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CComponent::CComponent(CFrmWnd *pFrmWnd)
{
	// Store the frame window
	ASSERT(pFrmWnd);
	m_pFrmWnd = pFrmWnd;

	// Initialize the working area
	m_pPrev = NULL;
	m_pNext = NULL;
	m_dwID = 0;
	m_strDesc.Empty();
	m_bEnable = FALSE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
CComponent::~CComponent()
{
	// Keep it disabled; destruction does not mean much here
	m_bEnable = FALSE;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CComponent::Init()
{
	ASSERT(this);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CComponent::Cleanup()
{
	ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL CComponent::Save(Fileio* /*pFio*/, int /*nVer*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL CComponent::Load(Fileio* /*pFio*/, int /*nVer*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CComponent::ApplyCfg(const Config* /*pConfig*/)
{
	ASSERT(this);
	ASSERT_VALID(this);
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Diagnostics
//
//---------------------------------------------------------------------------
void CComponent::AssertValid() const
{
	ASSERT(this);

	// Base class
	CObject::AssertValid();

	ASSERT(m_pFrmWnd);
	ASSERT(m_dwID != 0);
	ASSERT(m_strDesc.GetLength() > 0);
	ASSERT(m_pPrev || m_pNext);
	ASSERT(!m_pPrev || (m_pPrev->GetNextComponent() == (CComponent*)this));
	ASSERT(!m_pNext || (m_pNext->GetPrevComponent() == (CComponent*)this));
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Get the name
//
//---------------------------------------------------------------------------
void FASTCALL CComponent::GetDesc(CString& strDesc) const
{
	ASSERT(this);
	ASSERT_VALID(this);

	strDesc = m_strDesc;
}

//---------------------------------------------------------------------------
//
//	Search for a component
//
//---------------------------------------------------------------------------
CComponent* FASTCALL CComponent::SearchComponent(DWORD dwID)
{
	CComponent *pComponent;

	ASSERT(this);

	// Get the first component
	pComponent = this;
	while (pComponent->m_pPrev) {
		ASSERT(pComponent == pComponent->m_pPrev->m_pNext);
		pComponent = pComponent->m_pPrev;
	}

	// Search by ID
	while (pComponent) {
		if (pComponent->GetID() == dwID) {
			return pComponent;
		}

		// Next
		pComponent = pComponent->m_pNext;
	}

	// Not found
	return NULL;
}

//---------------------------------------------------------------------------
//
//	Add a new component
//
//---------------------------------------------------------------------------
void FASTCALL CComponent::AddComponent(CComponent *pNewComponent)
{
	CComponent *pComponent;

	ASSERT(this);
	ASSERT(pNewComponent);
	ASSERT(!pNewComponent->m_pPrev);
	ASSERT(!pNewComponent->m_pNext);

	// Get the first component
	pComponent = this;

	// Find the last component
	while (pComponent->m_pNext) {
		pComponent = pComponent->m_pNext;
	}

	// Link the components in both directions
	pComponent->m_pNext = pNewComponent;
	pNewComponent->m_pPrev = pComponent;
}

#endif	// _WIN32
