  //---------------------------------------------------------------------------
  //
  //	EMULADOR X68000 "XM6"
  //
  //	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
  //	[ Componente MFC ]
  //
  //---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_com_h)
#define mfc_com_h

  //===========================================================================
  //
  //	Componente
  //
  //===========================================================================
class CComponent : public CObject
{
public:
 	 // Funciones basicas
	CComponent(CFrmWnd *pFrmWnd);
 										 // Constructor
	virtual ~CComponent();
 										 // Destructor
	virtual BOOL FASTCALL Init();
 										 // Inicializacion
	virtual void FASTCALL Cleanup();
 										 // Limpieza
	virtual void FASTCALL Enable(BOOL bEnable)	{ m_bEnable = bEnable; }
 										 // Control de operacion
	BOOL FASTCALL IsEnable() const		{ return m_bEnable; }
 										 // Obtener estado de operacion
	virtual BOOL FASTCALL Save(Fileio *pFio, int nVer);
 										 // Guardar
	virtual BOOL FASTCALL Load(Fileio *pFio, int nVer);
 										 // Cargar
	virtual void FASTCALL ApplyCfg(const Config *pConfig);
 										 // Aplicar configuracion
#if !defined(NDEBUG)
		void AssertValid() const;
 										 // Diagnostico
 #endif	 // NDEBUG

 	 // Propiedades
	DWORD FASTCALL GetID() const		{ return m_dwID; }
 										 // Obtener ID
	void FASTCALL GetDesc(CString& strDesc) const;
 										 // Obtener nombre

 	 // Gestion de componentes
	CComponent* FASTCALL SearchComponent(DWORD dwID);
 										 // Buscar componente
	void FASTCALL AddComponent(CComponent *pNewComponent);
 										 // Anadir componente
	CComponent* FASTCALL GetPrevComponent() const	{ return m_pPrev; }
 										 // Obtener el componente anterior
	CComponent* FASTCALL GetNextComponent() const	{ return m_pNext; }
 										 // Obtener el componente siguiente

protected:
	CFrmWnd *m_pFrmWnd;
 										 // Ventana de marco
	DWORD m_dwID;
 										 // ID de componente
	BOOL m_bEnable;
 										 // Flag de activado
	CString m_strDesc;
 										 // Nombre
	CComponent *m_pPrev;
 										 // Componente anterior
	CComponent *m_pNext;
 										 // Componente siguiente
};

 #endif	 // mfc_com_h
 #endif	 // _WIN32
