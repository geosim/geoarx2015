// (C) Copyright 2002-2012 by Autodesk, Inc. 
//
// Permission to use, copy, modify, and distribute this software in
// object code form for any purpose and without fee is hereby granted, 
// provided that the above copyright notice appears in all copies and 
// that both that copyright notice and the limited warranty and
// restricted rights notice below appear in all supporting 
// documentation.
//
// AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS. 
// AUTODESK SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTY OF
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE.  AUTODESK, INC. 
// DOES NOT WARRANT THAT THE OPERATION OF THE PROGRAM WILL BE
// UNINTERRUPTED OR ERROR FREE.
//
// Use, duplication, or disclosure by the U.S. Government is subject to 
// restrictions set forth in FAR 52.227-19 (Commercial Computer
// Software - Restricted Rights) and DFAR 252.227-7013(c)(1)(ii)
// (Rights in Technical Data and Computer Software), as applicable.
//

//-----------------------------------------------------------------------------
//----- acrxEntryPoint.cpp
//-----------------------------------------------------------------------------
#include "StdAfx.h"
#include "resource.h"

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include "gs_list.h"
#include "gs_init.h"
#include "gs_error.h"
#include "gs_utily.h"


/*************************************************************************/
/*.doc DbReactorOn(internal) */
/*
    This function is called to add a reactor to the database to monitor 
    changes.
*/
/*************************************************************************/
void DbReactorOn()
{
   if (!GSDBReactor) GSDBReactor = new C_GSDBReactor;
      
   if (acdbHostApplicationServices()->workingDatabase())
      acdbHostApplicationServices()->workingDatabase()->addReactor(GSDBReactor);
}


/*************************************************************************/
/*.doc DbReactorOff(internal) */
/*
    This function is called to remove a reactor to the database to monitor 
    changes.
*/
/*************************************************************************/
void DbReactorOff()
{
   if (acdbHostApplicationServices()->workingDatabase() && GSDBReactor)
   {
      acdbHostApplicationServices()->workingDatabase()->removeReactor(GSDBReactor);
      delete GSDBReactor;
      GSDBReactor = NULL;
   }
}


//-----------------------------------------------------------------------------
#define szRDS _RXST("")

//-----------------------------------------------------------------------------
//----- ObjectARX EntryPoint
class CgeoarxApp : public AcRxArxApp
{
public:
	CgeoarxApp () : AcRxArxApp () {}

	virtual AcRx::AppRetCode On_kInitAppMsg (void *pkt)
   {
		// TODO: Load dependencies here
		
		// You *must* call On_kInitAppMsg here
		AcRx::AppRetCode retCode =AcRxArxApp::On_kInitAppMsg (pkt) ;
		
      if (GEOsimAppl::init() == GS_BAD) gsc_print_error();

      // Finestra di presentazione da chiamare dopo GEOsimAppl::init()
      gsc_DisplaySplash();

      DbReactorOn();
      if (!GSEditorReactor)
      { 
         C_GSEditorReactor::rxInit();
         acrxBuildClassHierarchy();
         GSEditorReactor = new C_GSEditorReactor; 
      }

      ::CoInitialize(NULL); // inizializza collegamento COM per OLE-DB

      TCHAR    OldTitle[100], *p;
      C_STRING NewTitle;

      GetWindowText(adsw_acadMainWnd(), OldTitle, 100);
      NewTitle = OldTitle;
      if ((p = NewTitle.at(_T('['))) != NULL) *p = _T('\0');
      NewTitle += _T("GEOsim (");
      NewTitle += gsc_msg(130); // Versione attuale di GEOsim
      NewTitle += _T(")");
      SetWindowText(adsw_acadMainWnd(), NewTitle.get_name()); 

      // profiling
      gsc_clear_profiling();

		return (retCode) ;
	}

	virtual AcRx::AppRetCode On_kUnloadAppMsg (void *pkt)
   {
      GEOsimAppl::LastAppMsgCode = AcRx::kUnloadAppMsg;

      // Questa funzione termina GEOsim.
      if (GEOsimAppl::terminate() == GS_BAD) gsc_print_error();

      DbReactorOff();      
      if (GSEditorReactor) delete GSEditorReactor;

		// You *must* call On_kUnloadAppMsg here
		AcRx::AppRetCode retCode =AcRxArxApp::On_kUnloadAppMsg (pkt) ;

      CoUninitialize(); // termina collegamento COM per OLE-DB

		return (retCode) ;
	}

	virtual void RegisterServerComponents () {
	}

   virtual AcRx::AppRetCode On_kLoadDwgMsg (void *pkt)
   {
      GEOsimAppl::LastAppMsgCode = AcRx::kLoadDwgMsg;
		AcRx::AppRetCode retCode = AcRxArxApp::On_kLoadDwgMsg (pkt) ;

      //// se c'era una sessione di GEOsim aperta la chiudo
      if (GS_CURRENT_WRK_SESSION) gsc_ExitCurrSession(GS_GOOD);
      GEOsimAppl::CMDLIST.funcLoad();
      GEOsimAppl::CMDLIST.GEOsimCmdsLoad();
      // New AcDbDatabase, so if reactor exist attach it
      if (GSDBReactor != NULL) DbReactorOn();

      return retCode;
   }

   virtual AcRx::AppRetCode On_kUnloadDwgMsg (void *pkt)
   {
      GEOsimAppl::LastAppMsgCode = AcRx::kUnloadDwgMsg;
		AcRx::AppRetCode retCode = AcRxArxApp::On_kUnloadDwgMsg (pkt) ;

      GEOsimAppl::CMDLIST.funcUnload();
      GEOsimAppl::OD_TABLENAME_LIST.remove_all(); // Lista delle tabelle OD correntemente definite

      return retCode;
   }

   virtual AcRx::AppRetCode On_kInvkSubrMsg (void *pkt)
   {
      GEOsimAppl::LastAppMsgCode = AcRx::kInvkSubrMsg;
      GEOsimAppl::CMDLIST.doFunc();
      
      return AcRx::kRetOK;
   }

	// The ACED_ARXCOMMAND_ENTRY_AUTO macro can be applied to any static member 
	// function of the CgeoarxApp class.
	// The function should take no arguments and return nothing.
	//
	// NOTE: ACED_ARXCOMMAND_ENTRY_AUTO has overloads where you can provide resourceid and
	// have arguments to define context and command mechanism.
	
	// ACED_ARXCOMMAND_ENTRY_AUTO(classname, group, globCmd, locCmd, cmdFlags, UIContext)
	// ACED_ARXCOMMAND_ENTRYBYID_AUTO(classname, group, globCmd, locCmdId, cmdFlags, UIContext)
	// only differs that it creates a localized name using a string in the resource file
	//   locCmdId - resource ID for localized command

	// Modal Command with localized name
	// ACED_ARXCOMMAND_ENTRY_AUTO(CgeoarxApp, MyGroup, MyCommand, MyCommandLocal, ACRX_CMD_MODAL)
	static void MyGroupMyCommand () {
		// Put your command code here

	}

	// Modal Command with pickfirst selection
	// ACED_ARXCOMMAND_ENTRY_AUTO(CgeoarxApp, MyGroup, MyPickFirst, MyPickFirstLocal, ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET)
	static void MyGroupMyPickFirst () {
		ads_name result ;
		int iRet =acedSSGet (ACRX_T("_I"), NULL, NULL, NULL, result) ;
		if ( iRet == RTNORM )
		{
			// There are selected entities
			// Put your command using pickfirst set code here
		}
		else
		{
			// There are no selected entities
			// Put your command code here
		}
	}

	// Application Session Command with localized name
	// ACED_ARXCOMMAND_ENTRY_AUTO(CgeoarxApp, MyGroup, MySessionCmd, MySessionCmdLocal, ACRX_CMD_MODAL | ACRX_CMD_SESSION)
	static void MyGroupMySessionCmd () {
		// Put your command code here
	}

	// The ACED_ADSFUNCTION_ENTRY_AUTO / ACED_ADSCOMMAND_ENTRY_AUTO macros can be applied to any static member 
	// function of the CgeoarxApp class.
	// The function may or may not take arguments and have to return RTNORM, RTERROR, RTCAN, RTFAIL, RTREJ to AutoCAD, but use
	// acedRetNil, acedRetT, acedRetVoid, acedRetInt, acedRetReal, acedRetStr, acedRetPoint, acedRetName, acedRetList, acedRetVal to return
	// a value to the Lisp interpreter.
	//
	// NOTE: ACED_ADSFUNCTION_ENTRY_AUTO / ACED_ADSCOMMAND_ENTRY_AUTO has overloads where you can provide resourceid.
	
	//- ACED_ADSFUNCTION_ENTRY_AUTO(classname, name, regFunc) - this example
	//- ACED_ADSSYMBOL_ENTRYBYID_AUTO(classname, name, nameId, regFunc) - only differs that it creates a localized name using a string in the resource file
	//- ACED_ADSCOMMAND_ENTRY_AUTO(classname, name, regFunc) - a Lisp command (prefix C:)
	//- ACED_ADSCOMMAND_ENTRYBYID_AUTO(classname, name, nameId, regFunc) - only differs that it creates a localized name using a string in the resource file

	// Lisp Function is similar to ARX Command but it creates a lisp 
	// callable function. Many return types are supported not just string
	// or integer.
	// ACED_ADSFUNCTION_ENTRY_AUTO(CgeoarxApp, MyLispFunction, false)
	static int ads_MyLispFunction () {
		//struct resbuf *args =acedGetArgs () ;
		
		// Put your command code here

		//acutRelRb (args) ;
		
		// Return a value to the AutoCAD Lisp Interpreter
		// acedRetNil, acedRetT, acedRetVoid, acedRetInt, acedRetReal, acedRetStr, acedRetPoint, acedRetName, acedRetList, acedRetVal

		return (RTNORM) ;
	}
	
} ;

//-----------------------------------------------------------------------------
IMPLEMENT_ARX_ENTRYPOINT(CgeoarxApp)

ACED_ARXCOMMAND_ENTRY_AUTO(CgeoarxApp, MyGroup, MyCommand, MyCommandLocal, ACRX_CMD_MODAL, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CgeoarxApp, MyGroup, MyPickFirst, MyPickFirstLocal, ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET, NULL)
ACED_ARXCOMMAND_ENTRY_AUTO(CgeoarxApp, MyGroup, MySessionCmd, MySessionCmdLocal, ACRX_CMD_MODAL | ACRX_CMD_SESSION, NULL)
ACED_ADSSYMBOL_ENTRY_AUTO(CgeoarxApp, MyLispFunction, false)

