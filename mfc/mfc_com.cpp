  //---------------------------------------------------------------------------
  //
  //	EMULADOR X68000 "XM6"
  //
  //	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
  //	[ Componente MFC ]
  //
  //---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "mfc_frm.h"
#include "mfc_com.h"

  //===========================================================================
  //
  //	Componente
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CComponent::CComponent(CFrmWnd *pFrmWnd)
{
 	 // Memorizar la ventana de marco
	ASSERT(pFrmWnd);
	m_pFrmWnd = pFrmWnd;

 	 // Inicializacion del area de trabajo
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
 	 // Desactivado (no tiene mucho significado)
	m_bEnable = FALSE;
}

  //---------------------------------------------------------------------------
  //
  //	Inicializacion
  //
  //---------------------------------------------------------------------------
BOOL FASTCALL CComponent::Init()
{
	ASSERT(this);
	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Limpieza
  //
  //---------------------------------------------------------------------------
void FASTCALL CComponent::Cleanup()
{
	ASSERT(this);
}

  //---------------------------------------------------------------------------
  //
  //	Guardar
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
  //	Cargar
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
  //	Aplicar configuracion
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
  //	Diagnostico
  //
  //---------------------------------------------------------------------------
void CComponent::AssertValid() const
{
	ASSERT(this);

 	 // Clase base
	CObject::AssertValid();

	ASSERT(m_pFrmWnd);
	ASSERT(m_dwID != 0);
	ASSERT(m_strDesc.GetLength() > 0);
	ASSERT(m_pPrev || m_pNext);
	ASSERT(!m_pPrev || (m_pPrev->GetNextComponent() == (CComponent*)this));
	ASSERT(!m_pNext || (m_pNext->GetPrevComponent() == (CComponent*)this));
}
 #endif	 // NDEBUG

  //---------------------------------------------------------------------------
  //
  //	Obtener nombre
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
  //	Buscar componente
  //
  //---------------------------------------------------------------------------
CComponent* FASTCALL CComponent::SearchComponent(DWORD dwID)
{
	CComponent *pComponent;

	ASSERT(this);

 	 // Obtener el primer componente
	pComponent = this;
	while (pComponent->m_pPrev) {
		ASSERT(pComponent == pComponent->m_pPrev->m_pNext);
		pComponent = pComponent->m_pPrev;
	}

 	 // Buscar ID
	while (pComponent) {
		if (pComponent->GetID() == dwID) {
			return pComponent;
		}

 		 // Siguiente
		pComponent = pComponent->m_pNext;
	}

 	 // No se encontro
	return NULL;
}

  //---------------------------------------------------------------------------
  //
  //	Anadir nuevo componente
  //
  //---------------------------------------------------------------------------
void FASTCALL CComponent::AddComponent(CComponent *pNewComponent)
{
	CComponent *pComponent;

	ASSERT(this);
	ASSERT(pNewComponent);
	ASSERT(!pNewComponent->m_pPrev);
	ASSERT(!pNewComponent->m_pNext);

 	 // Obtener el primer componente
	pComponent = this;

 	 // Buscar el ultimo componente
	while (pComponent->m_pNext) {
		pComponent = pComponent->m_pNext;
	}

 	 // Conexion mediante enlace bidireccional
	pComponent->m_pNext = pNewComponent;
	pNewComponent->m_pPrev = pComponent;
}

 #endif	 // _WIN32
