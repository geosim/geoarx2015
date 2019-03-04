/**********************************************************
Name: GS_ADE.CPP

Module description: Funzioni sviluppate su concetti ADE
            
Author: Roberto Poltini & Paolo De Sole

(c) Copyright 1995-2015 by IREN ACQUA GAS  S.p.A.

              
Modification history:
              
Notes and restrictions on use: 

**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/

#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rxdefs.h"
#include "adslib.h"
#include "MapProj.h"
#include "MapArxApi.h"
#include "MapTemplate.h"
#include "rxmfcapi.h"
#include <MapBoundary.h>
#include <MapQuery.h>
#include "acedCmdNF.h" // per acedCommandS e acedCmdS

#include "..\gs_def.h" // definizioni globali
#include <adeads.h>
#include "adsdlg.h"   

#include "gs_error.h"   
#include "gs_list.h"   
#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_ase.h"       // gestione dei link grafica/database
#include "gs_dwg.h"       // gestione disegni
#include "gs_netw.h"
#include "gs_dbref.h"
#include "gs_class.h"     // prototipi funzioni gestione classi
#include "gs_prjct.h" 
#include "gs_graph.h"
#include "gs_init.h" 
#include "gs_area.h" 
#include "gs_filtr.h"
#include "gs_query.h"
#include "gs_ade.h"


/*********************************************************/
/* FUNCTIONS */
/*********************************************************/


long gsc_getAttachedDwgs(void)
{
   AcMapSession    *mapApi;
   AcMapProject    *pProj;
   AcMapDrawingSet *pDSet;
   long            qty = 0;

   mapApi = AcMapGetSession();
   if (mapApi != NULL)
      if (mapApi->GetProject(pProj) == Adesk::kTrue)
         if (pProj->GetDrawingSet(pDSet) == Adesk::kTrue)
            qty = pDSet->CountDrawings();
	
   return qty;
}


/*********************************************************/
/*.doc gsc_ade_qrygettype                     <external> */
/*+
  Questa funzione legge la modalità della query ADE corrente.
  Parametri:  
  C_STRING &qryType;          (default = NULL)
  ade_boolean *multiLine;     (default = NULL)
  C_STRING &templ;            (default = NULL)
  C_STRING &fileName;         (default = NULL)
                       
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_ade_qrygettype(C_STRING *qryType, ade_boolean *multiLine, C_STRING *templ, 
	                    C_STRING *fileName)
{
   AcMapSession      *pMapSession = NULL;
   AcMapProject      *pProj;
   AcMapQuery        *pCurrentQuery;
   AcMap::EQueryType kType;
   int               res = GS_GOOD;

   // Get the Map Session Object
   if ((pMapSession = AcMapGetSession()) == NULL) return GS_BAD;
   // Get the currently active project 
   if(pMapSession->GetProject(pProj) != Adesk::kTrue) return GS_BAD;
   // Create a new query object 
   if (pProj->CreateQuery(pCurrentQuery, Adesk::kFalse) != AcMap::kOk) return GS_BAD;
   if (pCurrentQuery->LoadFromCurrent() != AcMap::kOk)
      { delete pCurrentQuery; return GS_BAD; }
   if (pCurrentQuery->GetMode(kType) != AcMap::kOk)
      { delete pCurrentQuery; return GS_BAD; }

   switch (kType)
   {
      case AcMap::kQueryDraw:
         if (qryType) *qryType = _T("DRAW");
         break;
      case AcMap::kQueryPreview:
         if (qryType) *qryType = _T("PREVIEW");
         break;
      case AcMap::kQueryReport:
      {
         if (qryType) *qryType = _T("REPORT");

         if (multiLine || templ || fileName)
         {
            AcMapReportTemplate *pTempl;

            if (pCurrentQuery->GetReportTemplate(pTempl) != AcMap::kOk)
               { res = GS_BAD; break; }

            if (multiLine)
            {
               Adesk::Boolean bIsEnabled;

               pTempl->IsReportNestedEnabled(bIsEnabled);
               *multiLine = (bIsEnabled == Adesk::kTrue) ? TRUE : FALSE;
            }

            if (templ)
            {
		         int               count = pTempl->CountLines();
		         AcMapTemplateLine *pLine = NULL;
					AcMapExpression   *pExpr;
					const TCHAR        *pExprStr;

               templ->clear();
			      for (int i = 0; i < count; i++)
			      {
				      // be sure pLine is initialized to NULL to allocate the object at the first call
				      if (pTempl->GetLine(pLine, i) == AcMap::kOk)
					      // get the associated expression object
					      if (pLine->GetExpression(pExpr) == AcMap::kOk)
					      {
						      pExpr->GetExpressionString(pExprStr);
                        if (i > 0) (*templ) += _T(',');
						      (*templ) += pExprStr;
						      delete pExpr;
					      }
			      }
            	if (pLine) delete pLine;
            }

            if (fileName)
            {
               const TCHAR *pStr;

               pTempl->GetFileName(pStr);
               *fileName = pStr;
            }

            delete pTempl;
         }
         break;
      }
      default:
         res = GS_BAD;
   }

   delete pCurrentQuery;

   return res;
}


/*********************************************************/
/*.doc gsc_getADECurrRptFile                  <external> */
/*+
  Questa funzione legge il nome del file report che è
  settato nella query ADE corrente.
  Parametri:  
  C_STRING &ReportFile;    Path completa del file di report
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getADECurrRptFile(C_STRING &ReportFile)
{
   const TCHAR          *pStr;
   AcMapSession        *pMapSession = NULL;
   AcMapProject        *pProj;
   AcMapQuery          *pCurrentQuery;
   AcMapReportTemplate *pTempl;

   // Get the Map Session Object
   if ((pMapSession = AcMapGetSession()) == NULL) return GS_BAD;
   // Get the currently active project 
   if(pMapSession->GetProject(pProj) != Adesk::kTrue) return GS_BAD;
   // Create a new query object 
   if (pProj->CreateQuery(pCurrentQuery, Adesk::kFalse) != AcMap::kOk) return GS_BAD;
   if (pCurrentQuery->LoadFromCurrent() != AcMap::kOk)
      { delete pCurrentQuery; return GS_BAD; }
   if (pCurrentQuery->GetReportTemplate(pTempl) != AcMap::kOk)
      { delete pCurrentQuery; return GS_BAD; }
   pTempl->GetFileName(pStr);
   ReportFile = pStr;

   delete pTempl;
   delete pCurrentQuery;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_setADECurrRptFile                  <external> */
/*+
  Questa funzione setta il nome del file report per la
  query ADE corrente.
  Parametri:  
  C_STRING &ReportFile;    Path completa del file di report
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_setADECurrRptFile(C_STRING &ReportFile)
{
   C_STRING    qryType, fileName, Templ;
   ade_boolean multiLine;

   if (gsc_ade_qrygettype(&qryType, &multiLine, &Templ) == GS_BAD)
      return GS_BAD;

   if (ade_qrysettype(qryType.get_name(), multiLine, Templ.get_name(), 
                      ReportFile.get_name()) != RTNORM)
      return GS_BAD;

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//////////////////  C_MAP_ENV  INIZIO  ////////////////////////////////////
//-----------------------------------------------------------------------//


C_MAP_ENV::C_MAP_ENV()
{
   RestoreLastActiveDwgsOnStartup = NULL;
   ActivateDwgsOnAttach = NULL;
   DontAddObjectsToSaveSet = NULL;
   MarkObjectsForEditingWithoutPrompting = NULL;
   LogFileActive = NULL;
   LogFileName = NULL;
   LogMessageLevel = NULL;
   QueryFileDirectory = NULL;
   CaseSensitiveMatch = NULL;
   SaveCurrQueryInSession = NULL;
   MkSelSetWithQryObj = NULL;
   DefaultJoinOperator = NULL;
   ColorForAdd = NULL;
   ColorForRemove = NULL;
   BlockLocnForQuery = NULL;
   TextLocnForQuery = NULL;
   ShowBlockAsInsPt = NULL;
   RedefineBlockDefinitions = NULL;
   RedefineLayerDefinitions = NULL;
   RedefineTextStyleDefinitions = NULL;
   RemoveUnusedGroups = NULL;
   EraseSavedBackObjects = NULL;
   RemoveLockAfterSave = NULL;
   CreateHistoryFileOfChanges = NULL;
   CreateBackupFileOfSourceDwg = NULL;
   // commentato per baco MAP2
   // NoOfSQLConditionsInHistory = NULL;
   AdjustSizesAndScalesForChangesInUnits = NULL;
   AdjustRotationsForMapDistortions = NULL;
   AdjustSizesAndScalesForMapDistortions = NULL;
   AdjustElevations = NULL;
   AdjustZeroRotationObjects = NULL;

   // System preferences
   ForceUserLogin = NULL;
   EnableObjectLocking = NULL;
   NumberofOpenDwgs = NULL;
}


C_MAP_ENV::~C_MAP_ENV() 
{
   ReleaseCurrentAdeEnvironment();
}


/*********************************************************/
/*.doc void C_MAP_ENV::FreezeCurrentAdeEnvironment(void) <internal> /*
/*+
  Congela lo stato delle variabili d'ambiente ADE
  
-*/  
/*********************************************************/
void C_MAP_ENV::FreezeCurrentAdeEnvironment(void)
{
   ReleaseCurrentAdeEnvironment(); // rilascio rb

   RestoreLastActiveDwgsOnStartup = ade_prefgetval(_T("RestoreLastActiveDwgsOnStartup"));
   ActivateDwgsOnAttach = ade_prefgetval(_T("ActivateDwgsOnAttach"));
   DontAddObjectsToSaveSet = ade_prefgetval(_T("DontAddObjectsToSaveSet"));
   MarkObjectsForEditingWithoutPrompting = ade_prefgetval(_T("MarkObjectsForEditingWithoutPrompting"));
   LogFileActive = ade_prefgetval(_T("LogFileActive"));
   LogFileName = ade_prefgetval(_T("LogFileName"));
   LogMessageLevel = ade_prefgetval(_T("LogMessageLevel"));
   QueryFileDirectory = ade_prefgetval(_T("QueryFileDirectory"));
   CaseSensitiveMatch = ade_prefgetval(_T("CaseSensitiveMatch"));
   SaveCurrQueryInSession = ade_prefgetval(_T("SaveCurrQueryInSession"));
   MkSelSetWithQryObj = ade_prefgetval(_T("MkSelSetWithQryObj"));
   DefaultJoinOperator = ade_prefgetval(_T("DefaultJoinOperator"));
   ColorForAdd = ade_prefgetval(_T("ColorForAdd"));
   ColorForRemove = ade_prefgetval(_T("ColorForRemove"));
   BlockLocnForQuery = ade_prefgetval(_T("BlockLocnForQuery"));
   TextLocnForQuery = ade_prefgetval(_T("TextLocnForQuery"));
   ShowBlockAsInsPt = ade_prefgetval(_T("ShowBlockAsInsPt"));
   RedefineBlockDefinitions = ade_prefgetval(_T("RedefineBlockDefinitions"));
   RedefineLayerDefinitions = ade_prefgetval(_T("RedefineLayerDefinitions"));
   RedefineTextStyleDefinitions = ade_prefgetval(_T("RedefineTextStyleDefinitions"));
   RemoveUnusedGroups = ade_prefgetval(_T("RemoveUnusedGroups"));
   EraseSavedBackObjects = ade_prefgetval(_T("EraseSavedBackObjects"));
   RemoveLockAfterSave = ade_prefgetval(_T("RemoveLockAfterSave"));
   CreateHistoryFileOfChanges = ade_prefgetval(_T("CreateHistoryFileOfChanges"));
   CreateBackupFileOfSourceDwg = ade_prefgetval(_T("CreateBackupFileOfSourceDwg"));
   // commentato per baco MAP2
   // NoOfSQLConditionsInHistory = ade_prefgetval("NoOfSQLConditionsInHistory");
   AdjustSizesAndScalesForChangesInUnits = ade_prefgetval(_T("AdjustSizesAndScalesForChangesInUnits"));
   AdjustRotationsForMapDistortions = ade_prefgetval(_T("AdjustRotationsForMapDistortions"));
   AdjustSizesAndScalesForMapDistortions = ade_prefgetval(_T("AdjustSizesAndScalesForMapDistortions"));
   AdjustElevations = ade_prefgetval(_T("AdjustElevations"));
   AdjustZeroRotationObjects = ade_prefgetval(_T("AdjustZeroRotationObjects"));

   // System preferences
   ForceUserLogin = ade_prefgetval(_T("ForceUserLogin"));
   EnableObjectLocking = ade_prefgetval(_T("EnableObjectLocking"));
   NumberofOpenDwgs = ade_prefgetval(_T("NumberofOpenDwgs"));
}


/*********************************************************/
/*.doc void C_MAP_ENV::FreezeCurrentAdeEnvironment(void) <internal> /*
/*+
  Ripristino lo stato delle variabili d'ambiente ADE
  
-*/  
/*********************************************************/
void C_MAP_ENV::RestoreLastAdeEnvironment(void)
{
   ade_prefsetval(_T("RestoreLastActiveDwgsOnStartup"), RestoreLastActiveDwgsOnStartup);
   ade_prefsetval(_T("ActivateDwgsOnAttach"), ActivateDwgsOnAttach);
   ade_prefsetval(_T("DontAddObjectsToSaveSet"), DontAddObjectsToSaveSet);
   ade_prefsetval(_T("MarkObjectsForEditingWithoutPrompting"), MarkObjectsForEditingWithoutPrompting);
   ade_prefsetval(_T("LogFileActive"), LogFileActive);
   ade_prefsetval(_T("LogFileName"), LogFileName);
   ade_prefsetval(_T("LogMessageLevel"), LogMessageLevel);
   ade_prefsetval(_T("QueryFileDirectory"), QueryFileDirectory);
   ade_prefsetval(_T("CaseSensitiveMatch"), CaseSensitiveMatch);
   ade_prefsetval(_T("SaveCurrQueryInSession"), SaveCurrQueryInSession);
   ade_prefsetval(_T("MkSelSetWithQryObj"), MkSelSetWithQryObj);
   ade_prefsetval(_T("DefaultJoinOperator"), DefaultJoinOperator);
   ade_prefsetval(_T("ColorForAdd"), ColorForAdd);
   ade_prefsetval(_T("ColorForRemove"), ColorForRemove);
   ade_prefsetval(_T("BlockLocnForQuery"), BlockLocnForQuery);
   ade_prefsetval(_T("TextLocnForQuery"), TextLocnForQuery);
   ade_prefsetval(_T("ShowBlockAsInsPt"), ShowBlockAsInsPt);
   ade_prefsetval(_T("RedefineBlockDefinitions"), RedefineBlockDefinitions);
   ade_prefsetval(_T("RedefineLayerDefinitions"), RedefineLayerDefinitions);
   ade_prefsetval(_T("RedefineTextStyleDefinitions"), RedefineTextStyleDefinitions);
   ade_prefsetval(_T("RemoveUnusedGroups"), RemoveUnusedGroups);
   ade_prefsetval(_T("EraseSavedBackObjects"), EraseSavedBackObjects);
   ade_prefsetval(_T("RemoveLockAfterSave"), RemoveLockAfterSave);
   ade_prefsetval(_T("CreateHistoryFileOfChanges"), CreateHistoryFileOfChanges);
   ade_prefsetval(_T("CreateBackupFileOfSourceDwg"), CreateBackupFileOfSourceDwg);
   // commentato per baco MAP2
   // ade_prefsetval(_T("NoOfSQLConditionsInHistory", NoOfSQLConditionsInHistory);
   ade_prefsetval(_T("AdjustSizesAndScalesForChangesInUnits"), AdjustSizesAndScalesForChangesInUnits);
   ade_prefsetval(_T("AdjustRotationsForMapDistortions"), AdjustRotationsForMapDistortions);
   ade_prefsetval(_T("AdjustSizesAndScalesForMapDistortions"), AdjustSizesAndScalesForMapDistortions);
   ade_prefsetval(_T("AdjustElevations"), AdjustElevations);
   ade_prefsetval(_T("AdjustZeroRotationObjects"), AdjustZeroRotationObjects);

   // System preferences
   ade_prefsetval(_T("ForceUserLogin"), ForceUserLogin);
   // Questa proprietà non è modificabile se ci sono DWG attaccati e attivi
   if (gsc_getAttachedDwgs() == 0)
      ade_prefsetval(_T("EnableObjectLocking"), EnableObjectLocking);
   ade_prefsetval(_T("NumberofOpenDwgs"), NumberofOpenDwgs);
}


/*********************************************************/
/*.doc C_MAP_ENV::SetAdeEnvVariable           <internal> /*
/*+
  Configura una variabile d'ambiente ADE.
  Parametri:
  TCHAR *AdeVar;      Nome variabile
  int AdeVarValue;   Valore intero
  
  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_MAP_ENV::SetAdeEnvVariable(TCHAR *AdeVar, int AdeVarValue)
{
   C_RB_LIST TempVar;
   
   if ((TempVar << acutBuildList(RTSHORT, AdeVarValue, 0)) == NULL) return GS_BAD;

   if (ade_prefsetval(AdeVar, TempVar.get_head())!=RTNORM)
      { GS_ERR_COD = eGSUnknown; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_MAP_ENV::SetAdeEnvVariable           <internal> /*
/*+
  Configura una variabile d'ambiente ADE.
  Parametri:
  TCHAR *AdeVar;     Nome variabile
  TCHAR AdeVarValue; Valore stringa
  
  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_MAP_ENV::SetAdeEnvVariable(TCHAR *AdeVar, TCHAR *AdeVarValue)
{
   C_RB_LIST TempVar;
   
   if ((TempVar << acutBuildList(RTSTR, AdeVarValue, 0)) == NULL) return GS_BAD;

   if(ade_prefsetval(AdeVar, TempVar.get_head())!=RTNORM)
      { GS_ERR_COD = eGSUnknown; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_MAP_ENV::SetAdeEnvVariable           <internal> /*
/*+
  Configura una variabile d'ambiente ADE.
  Parametri:
  TCHAR *AdeVar;           Nome variabile
  EAsiBoolean AdeVarValue; Valore booleano
  
  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_MAP_ENV::SetAdeEnvVariable(TCHAR *AdeVar, EAsiBoolean AdeVarValue)
{
   C_RB_LIST TempVar;
   
   if(AdeVarValue==kAsiTrue || AdeVarValue==kAsiGood)
   {
      if ((TempVar << acutBuildList(RTT, 0)) == NULL) return GS_BAD;
   }
   else
      if ((TempVar << acutBuildList(RTNIL, 0)) == NULL) return GS_BAD;

   if (ade_prefsetval(AdeVar, TempVar.get_head())!=RTNORM)
      { GS_ERR_COD = eGSUnknown; return GS_BAD; }

   return GS_GOOD;
}


void  C_MAP_ENV::ReleaseCurrentAdeEnvironment(void)
{
   if (RestoreLastActiveDwgsOnStartup) acutRelRb(RestoreLastActiveDwgsOnStartup);
   if (ActivateDwgsOnAttach) acutRelRb(ActivateDwgsOnAttach);
   if (DontAddObjectsToSaveSet) acutRelRb(DontAddObjectsToSaveSet);
   if (MarkObjectsForEditingWithoutPrompting) acutRelRb(MarkObjectsForEditingWithoutPrompting);
   if (LogFileActive) acutRelRb(LogFileActive);
   if (LogFileName) acutRelRb(LogFileName);
   if (LogMessageLevel) acutRelRb(LogMessageLevel);
   if (QueryFileDirectory) acutRelRb(QueryFileDirectory);
   if (CaseSensitiveMatch) acutRelRb(CaseSensitiveMatch);
   if (SaveCurrQueryInSession) acutRelRb(SaveCurrQueryInSession);
   if (MkSelSetWithQryObj) acutRelRb(MkSelSetWithQryObj);
   if (DefaultJoinOperator) acutRelRb(DefaultJoinOperator);
   if (ColorForAdd) acutRelRb(ColorForAdd);
   if (ColorForRemove) acutRelRb(ColorForRemove);
   if (BlockLocnForQuery) acutRelRb(BlockLocnForQuery);
   if (TextLocnForQuery) acutRelRb(TextLocnForQuery);
   if (ShowBlockAsInsPt) acutRelRb(ShowBlockAsInsPt);
   if (RedefineBlockDefinitions) acutRelRb(RedefineBlockDefinitions);
   if (RedefineLayerDefinitions) acutRelRb(RedefineLayerDefinitions);
   if (RedefineTextStyleDefinitions) acutRelRb(RedefineTextStyleDefinitions);
   if (RemoveUnusedGroups) acutRelRb(RemoveUnusedGroups);
   if (EraseSavedBackObjects) acutRelRb(EraseSavedBackObjects);
   if (RemoveLockAfterSave) acutRelRb(RemoveLockAfterSave);
   if (CreateHistoryFileOfChanges) acutRelRb(CreateHistoryFileOfChanges);
   if (CreateBackupFileOfSourceDwg) acutRelRb(CreateBackupFileOfSourceDwg);
   // commentato per baco MAP2
   // if(NoOfSQLConditionsInHistory) acutRelRb(NoOfSQLConditionsInHistory);
   if (AdjustSizesAndScalesForChangesInUnits) acutRelRb(AdjustSizesAndScalesForChangesInUnits);
   if (AdjustRotationsForMapDistortions) acutRelRb(AdjustRotationsForMapDistortions);
   if (AdjustSizesAndScalesForMapDistortions) acutRelRb(AdjustSizesAndScalesForMapDistortions);
   if (AdjustElevations) acutRelRb(AdjustElevations);
   if (AdjustZeroRotationObjects) acutRelRb(AdjustZeroRotationObjects);
   
   // System preferences
   if (ForceUserLogin) acutRelRb(ForceUserLogin);
   if (EnableObjectLocking) acutRelRb(EnableObjectLocking);
   if (NumberofOpenDwgs) acutRelRb(NumberofOpenDwgs);
};


/*********************************************************/
/*.doc C_MAP_ENV::SetEnv4GEOsim
/*+
  Setta l'ambiente ADE per lavorare in una sessione di GEOsim.
-*/  
/*********************************************************/
void C_MAP_ENV::SetEnv4GEOsim(void)
{
   struct resbuf rb;

   // Setto la modalità di quotatura
   rb.restype = RTSHORT;
   DimAssoc = rb.resval.rint = GEOsimAppl::GLOBALVARS.get_DimAssoc();
   acedSetVar(_T("DIMASSOC"), &rb);

   // Non aggiungere automaticamente oggetti al gruppo di selezione di MAP
   SetAdeEnvVariable(_T("DontAddObjectsToSaveSet"), kAsiTrue);
   SetAdeEnvVariable(_T("MarkObjectsForEditingWithoutPrompting"), kAsiFalse);   

   // Non si può settare questa opzione se ci sono DWG attaccati
   if (gsc_getAttachedDwgs() == 0)
      SetAdeEnvVariable(_T("EnableObjectLocking"), kAsiTrue);

   // Non cancellare gli oggetti dopo il salvataggio
   SetAdeEnvVariable(_T("EraseSavedBackObjects"), kAsiFalse);
   // Rimuovere il lock agli oggetti dopo il salvataggio
   SetAdeEnvVariable(_T("RemoveLockAfterSave"), kAsiTrue);
   // Visualizza dolo messaggi di errore gravi
   SetAdeEnvVariable(_T("LogMessageLevel"), 0);
}


/*********************************************************/
/*.doc C_MAP_ENV::SetEnv4GEOsimSave
/*+
  Setta l'ambiente ADE per la fase di salvataggio di GEOsim.
-*/  
/*********************************************************/
void C_MAP_ENV::SetEnv4GEOsimSave(void)
{
   SetAdeEnvVariable(_T("RedefineBlockDefinitions"), kAsiFalse);
   SetAdeEnvVariable(_T("DontAddObjectsToSaveSet"), kAsiFalse);
   SetAdeEnvVariable(_T("MarkObjectsForEditingWithoutPrompting"), kAsiTrue);
   
   // Non si può settare questa opzione se ci sono DWG attaccati
   if (gsc_getAttachedDwgs() == 0)
      SetAdeEnvVariable(_T("EnableObjectLocking"), kAsiTrue);

   SetAdeEnvVariable(_T("EraseSavedBackObjects"), kAsiFalse);
   SetAdeEnvVariable(_T("RemoveLockAfterSave"), kAsiTrue);
   SetAdeEnvVariable(_T("LogMessageLevel"), 0);
}


//-----------------------------------------------------------------------//
///////////////       C_MAP_ENV  FINE      ////////////////////////////////
//-----------------------------------------------------------------------//


//****************************************************************
// funzioni che temporaneamente sostituiscono le chiamate originali
// alla libreria di ADE (perchè sono bacate)
// inizio funzioni da cancellare 
//****************************************************************


///////////////////////////////////////////////////////////////////////////////
// ade_qlqrygetid:
///////////////////////////////////////////////////////////////////////////////
int gs_ade_qlqrygetid(void)
{
   presbuf arg = acedGetArgs();
   ade_id  QryId;

   acedRetNil();

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if ((QryId = gsc_ade_qlqrygetid(arg->resval.rstring)) != ADE_NULLID) 
      acedRetReal(QryId);

   return RTNORM;
}
ade_id gsc_ade_qlqrygetid(TCHAR *qryName)
{
   C_RB_LIST QueryCategoryList;

   if ((QueryCategoryList << ade_qllistctgy()) == NULL) return ADE_NULLID;
   return ade_qlqrygetid(qryName);
}


///////////////////////////////////////////////////////////////////////////////
// ade_qlloadqry:
///////////////////////////////////////////////////////////////////////////////
int gs_ade_qlloadqry(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil();

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_ade_qlloadqry(arg->resval.rstring) != RTERROR) acedRetT();

   return RTNORM;
}
int gsc_ade_qlloadqry(TCHAR *qryName)
{
   ade_id qryId = gsc_ade_qlqrygetid(qryName);

   if (qryId != ADE_NULLID)
      return ade_qlloadqry(qryId);
   else
      return RTERROR;
}


///////////////////////////////////////////////////////////////////////////////
// gsc_ExistCurrentAdeQry:
///////////////////////////////////////////////////////////////////////////////
int gsc_ExistCurrentAdeQry(void)
{
   AcMapSession *mapApi = NULL;
	AcMapProject *pProj = NULL;
   AcMapQuery   *pQuery = NULL;
   int          Result = GS_BAD;

   if (!(mapApi = AcMapGetSession())) return GS_BAD; // sessione di MAP
   if (mapApi->GetProject(pProj) != Adesk::kTrue) return GS_BAD; // progetto MAP
   if (pProj->CreateQuery(pQuery) != AcMap::kOk) return GS_BAD;

   if (pQuery->LoadFromCurrent() == AcMap::kOk)
      if (pQuery->IsDefined()) Result = GS_GOOD;

   delete pQuery;

   return Result;
}


///////////////////////////////////////////////////////////////////////////////
// gs_ade_qrylist e gsc_ade_qrylist
///////////////////////////////////////////////////////////////////////////////
int gs_ade_qrylist(void)
{
   C_RB_LIST List;

   // la funzione, esposta al lisp, è usata per evitare messaggi non desiderati 
   // di MAP nel caso in cui non ci sia una qry corrente
   List << gsc_ade_qrylist();
   if (List.IsEmpty()) acedRetNil();
   else List.LspRetList();
   
   return RTNORM;
}
presbuf gsc_ade_qrylist(void)
{
   if (gsc_ExistCurrentAdeQry() == GS_BAD) return NULL;
   return ade_qrylist();
}


///////////////////////////////////////////////////////////////////////////////
// ade_qldelquery:
///////////////////////////////////////////////////////////////////////////////
int gs_ade_qldelquery(void)
{
   presbuf arg = acedGetArgs();

   acedRetNil();

   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (arg->restype == RTSTR)
   {
      if (gsc_ade_qldelquery(arg->resval.rstring) != RTERROR) acedRetT();
   }
   else
   {
      ade_id qryId;

      if (gsc_rb2Dbl(arg, &qryId) == GS_GOOD)
         if (gsc_ade_qldelquery(qryId) != RTERROR) acedRetT();
   }

   return RTNORM;
}
int gsc_ade_qldelquery(ade_id qryId)
{
   if (qryId != ADE_NULLID)
      return ade_qldelquery(qryId);
   else
      return RTNORM;
}
int gsc_ade_qldelquery(TCHAR *qryName)
{
   return gsc_ade_qldelquery(gsc_ade_qlqrygetid(qryName));
}

presbuf gsc_ade_qrygetdwgandhandle(ads_name ename)
{
   int     OldCmdDia;
   presbuf p;

   // forzo cmdddia a 0 per evitare messaggi di avviso in finestre indesiderate
   gsc_set_cmddia(0, &OldCmdDia);
   // leggo handle nel disegno originale 
   p = ade_qrygetdwgandhandle(ename);
   // risetto cmddia al valore precedente
   gsc_set_cmddia(OldCmdDia);
   
   // ade_errclear(); // questa funzione è molto lenta - roby

   return p;
}

int gsc_ade_userset(const TCHAR *userName, const TCHAR *pswd)
{
   // fa la login verificando prima l'esistenza dell'utente
   // per evitare messaggi di avviso in finestre indesiderate
   C_RB_LIST UserList;

   if ((UserList << ade_userlist()) == NULL)
      { GS_ERR_COD = eGSInvAdeUser; return RTERROR; }
   if (UserList.SearchName(userName, GS_BAD) == NULL)
      { GS_ERR_COD = eGSInvAdeUser; return RTERROR; }
   if (ade_userset(userName, pswd) != RTNORM)
      { GS_ERR_COD = eGSInvAdeUser; return RTERROR; }

   return RTNORM;
}


//****************************************************************
// fine funzioni da cancellare 
// inizio funzioni da mantenere
//****************************************************************


/*********************************************************/
/*.doc gsc_editlockobjs()                        
/*+
  Questa funzione il lock degli oggetti inserendoli nel gruppo di selezione
  del salvataggio di ADE.
  Parametri:
  C_SELSET &SelSet; 
  oppure
  ads_name selSet;
                    
  Restituisce il numero di oggetti inseriti in caso di successo altrimento ADE_REALFAIL.
-*/                                             
/*********************************************************/
ads_real gsc_editlockobjs(ads_name SelSet)
{
   C_SELSET dummy;

   dummy << SelSet;
   dummy.ReleaseAllAtDistruction(GS_BAD);
   return gsc_editlockobjs(dummy);
}
ads_real gsc_editlockobjs(C_SELSET &SelSet)
{
   AcDbObjectIdArray ObjIds;

   if (SelSet.get_AcDbObjectIdArray(ObjIds) == GS_BAD) return ADE_REALFAIL;
   return gsc_editlockobjs(ObjIds);
}
ads_real gsc_editlockobjs(AcDbObjectIdArray &ObjIds)
{
   // Anche usando le funzioni a basso livello Autocad si ferma se trova 
   // che un altro utente ha salvato e quindi modificato il dwg e fa la domanda
   // se si vuole continuare. Con la funzione AddObjects è impossibile rispondere _Y.
   AcMapSession *pMapSession = NULL;
   AcMapProject *pProj;
   AcMapSaveSet *pSaveSet;

   // Get the Map Session Object
   if ((pMapSession = AcMapGetSession()) == NULL) return ADE_REALFAIL;
   // Get the currently active project 
   if (pMapSession->GetProject(pProj) != Adesk::kTrue) return ADE_REALFAIL;
   // Get the currently sveset
   if (pProj->GetSaveSet(pSaveSet) != Adesk::kTrue) return ADE_REALFAIL;

   // Add objects
   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
   int tot = pSaveSet->AddObjects(ObjIds);
   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;

   if (ObjIds.length() != tot) { gsc_printAdeErr(); return ADE_REALFAIL; }

   return tot;
}


/*********************************************************/
/*.doc gsc_adeSelObjs()                        
/*+
  Questa funzione esegue il lock degli oggetti inserendoli 
  nel gruppo di selezione del salvataggio di ADE tramite il comando
  ADESELOBJS che può gestire interruzioni tipo la domanda
  "il disegno è stato modificato da un altro utente, vuoi continuare ?".
  Parametri:
  C_SELSET &SelSet;
                    
  Restituisce GS_GOO in caso di successo altrimento GS_BAD.
-*/                                             
/*********************************************************/
int gsc_adeSelObjs(C_SELSET &SelSet)
{
   C_REAL_LIST DwgIdList;
   C_RB_LIST   cmdRbParams;
   ads_name    dummySS;

   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave

   SelSet.get_selection(dummySS);
   cmdRbParams << acutBuildList(RTSTR, _T("_S"), RTPICKS, dummySS, RTSTR, GS_EMPTYSTR, 0);

   gsc_getDwgsShouldBeRequeried(SelSet, DwgIdList);

   for (int i = 0; i < DwgIdList.get_count(); i++)
      cmdRbParams += _T("_Y");

   // ADESELOBJS _S (gruppo di selezione) gruppo "" e se ci sono dwg da ricaricare (cambiati da altri utenti)
   // il comando si ferma e chiede di ricaricarli e rispondo  "_Y" per ogni dwg      
   int res = gsc_callCmd(_T("_.ADESELOBJS"), cmdRbParams.get_head());

   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;

   return (res == RTNORM) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_getDwgIdList
/*+
  Questa funzione ricava la lista degli id dei disegni attaccati da cui
  sono stati estratti gli oggetti contenuti nel gruppo di selezione SelSet.
  Parametri:
  C_SELSET &SelSet;        gruppo di selezione (input)
  C_REAL_LIST &DwgIdList;  lista degli id dei disegni (output)
                    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/*********************************************************/
int gsc_getDwgIdList(C_SELSET &SelSet, C_REAL_LIST &DwgIdList)
{
   AcMapSession         *mapApi;
   AcMapProject         *pProj;
   AcMapId              Id;
   AcDbObjectIdArray    ObjIds;
   AcDbObjectId         ObjId;

   // Ottengo un vettore di AcDbObjectId
   if (SelSet.get_AcDbObjectIdArray(ObjIds) == GS_BAD) return GS_BAD;
   
   if ((mapApi = AcMapGetSession()) == NULL) return GS_BAD;
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return GS_BAD;

   for (long i = 0; i < ObjIds.length(); ++i) 
      // se l'oggetto arriva da un dwg attaccato non ancora trovato
      if (pProj->GetSourceDrawingId(Id,  ObjIds[i]) == AcMap::kOk &&
          DwgIdList.search_key((double) Id) == NULL)
         if (DwgIdList.add_tail_double((double) Id) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getDwgsShouldBeRequeried
/*+
  Questa funzione ricava la lista degli id dei disegni attaccati da cui
  sono stati estratti gli oggetti contenuti nel gruppo di selezione SelSet
  che dovrebbero essere ricaricati.
  Parametri:
  C_SELSET &SelSet;        gruppo di selezione (input)
  C_REAL_LIST &DwgIdList;  lista degli id dei disegni (output)
                    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/*********************************************************/
int gsc_getDwgsShouldBeRequeried(C_SELSET &SelSet, C_REAL_LIST &DwgIdList)
{
   AcMapSession         *mapApi;
   AcMapProject         *pProj;
   AcMapDrawingSet      *pDSet;
   AcMapAttachedDrawing *pDwg = NULL;
   C_REAL_LIST          OriginDwgIdList;
   C_REAL              *pDwgId;

   if (gsc_getDwgIdList(SelSet, OriginDwgIdList) == GS_BAD) return GS_BAD;
   
   mapApi = AcMapGetSession();
   if (!mapApi) return GS_BAD;
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return GS_BAD;
   if (pProj->GetDrawingSet(pDSet) == Adesk::kFalse) return GS_GOOD;

   pDwgId = (C_REAL *) OriginDwgIdList.get_head();
   while (pDwgId)
   {
      // se il dwg è stato modificato
      if (pDSet->GetDrawing(pDwg, (AcMapId) pDwgId->get_key_double()) == AcMap::kOk)
	  {
		  if (pDwg->IsUpdatedByAnotherMapUser())
            if (DwgIdList.search_key(pDwgId->get_key_double()) == NULL)
               if (DwgIdList.add_tail_double(pDwgId->get_key_double()) == GS_BAD)
				{ delete pDwg; return GS_BAD; }
	  }

      pDwgId = (C_REAL *) OriginDwgIdList.get_next();
   }

   if (pDwg) delete pDwg;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_dsdetach()                        
/*+
  Questa funzione esegue il detach del dwg al quale e' stato fatto 
  l'attach di ADE.
  Parametri:
  ade_id dwgId;   Identificatore del disegno attaccato
  int retest;     se MORETESTS -> in caso di errore riprova n volte 
                  con i tempi di attesa impostati poi ritorna GS_BAD,
                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                  (default = MORETESTS)
                    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/*********************************************************/
int gsc_dsdetach(ade_id dwgId, int retest)
{
   ade_errclear();
   if (retest == MORETESTS)
   {
      int tentativi = 1;
      
      do
      {
         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            if (ade_dsdetach(dwgId) == RTNORM) return GS_GOOD;
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         if (gsc_dderroradmin(_T("ADE")) != GS_GOOD) // l'utente vuole abbandonare
            { GS_ERR_COD = eGSDwgCannotDetach; return GS_BAD; }
      }
      while (1);
   }
   else
      if (ade_dsdetach(dwgId) != RTNORM)
      {
         #if defined(GSDEBUG) // se versione per debugging
            gsc_printAdeErr();
         #endif
         GS_ERR_COD = eGSDwgCannotDetach; 
         return GS_BAD;
      }
      
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getAdeId                        
/*+
  Questa funzione ottiene l'identificatore ade di un disegno attaccato.
  Parametri:
  AcMapAttachedDrawing *pMapAttachedDwg;  Puntatore a disegno attaccato
-*/                                             
/*********************************************************/
ade_id gsc_getAdeId(AcMapAttachedDrawing *pMapAttachedDwg)
{
	ACHAR  *pAliasPath = NULL;
   ade_id Result;

   if (!pMapAttachedDwg) return ADE_NULLID;
	if (pMapAttachedDwg->GetAliasPath(pAliasPath) != AcMap::kOk) return ADE_NULLID;
	Result = ade_dwggetid(pAliasPath);
   if (pAliasPath) free(pAliasPath);
   
   return Result;
}


/*********************************************************/
/*.doc gsc_dwgactivate                        
/*+
  Questa funzione esegue il detach dei dwg ai quali e' stato fatto 
  l'attach di ADE con filtro opzionale sui soli dwg di una classe.
  Parametri:
  ade_id dwgId;   Identificatore del disegno attaccato
  int retest;     se MORETESTS -> in caso di errore riprova n volte 
                  con i tempi di attesa impostati poi ritorna GS_BAD,
                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                  (default = MORETESTS)
                    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/*********************************************************/
int gsc_dwgactivate(ade_id dwgId, int retest)
{
   ade_errclear();
   if (retest == MORETESTS)
   {
      int tentativi = 1;
      
      do
      {
         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            if (ade_dwgactivate(dwgId) == RTNORM) return GS_GOOD;
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         if (gsc_dderroradmin(_T("ADE")) != GS_GOOD) // l'utente vuole abbandonare
            { GS_ERR_COD = eGSDwgCannotActivate; return GS_BAD; }
      }
      while (1);
   }
   else
      if (ade_dwgactivate(dwgId) != RTNORM)
      {
         #if defined(GSDEBUG) // se versione per debugging
            gsc_printAdeErr();
         #endif
         GS_ERR_COD = eGSDwgCannotActivate; 
         return GS_BAD;
      }
      
   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) gsc_dsattach()                        
/*+
  Questa funzione esegue l'attach del dwg.
  Parametri:
  TCHAR *dwgName; Path completa stile ade(es. "COMPUTER1:\DIR\A.DWG")
  int retest;     se MORETESTS -> in caso di errore riprova n volte 
                  con i tempi di attesa impostati poi ritorna GS_BAD,
                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                  (default = MORETESTS)
                    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/*********************************************************/
ade_id gsc_dsattach(TCHAR *dwgName, int retest)
{
   ade_id result = ADE_NULLID;

   ade_errclear();
   if (retest == MORETESTS)
   {
      int tentativi = 1;
      
      do
      {
         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            if ((result = ade_dsattach(dwgName)) != ADE_NULLID) return result;
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         if (gsc_dderroradmin(_T("ADE")) != GS_GOOD) // l'utente vuole abbandonare
            { GS_ERR_COD = eGSDwgCannotAttach; return ADE_NULLID; }
      }
      while (1);
   }
   else
      if ((result = ade_dsattach(dwgName)) == ADE_NULLID)
      {
         #if defined(GSDEBUG) // se versione per debugging
            gsc_printAdeErr();
         #endif
         GS_ERR_COD = eGSDwgCannotAttach; 
         return ADE_NULLID;
      }
      
   return result;
}


/*********************************************************/
/*.doc gs_attachExtractDetach()                        
/*+
  Questa comando esegue l'attach, estrazione di tutto e detach di un dwg.
-*/                                             
/*********************************************************/
void gsAttachExtractDetach(void)
{
   C_STRING dwgName;

   // "GEOsim - Seleziona file"
   if (gsc_GetFileD(gsc_msg(645), NULL, _T("dwg"), UI_LOADFILE_FLAGS, dwgName) != RTNORM)
      return;

   GEOsimAppl::CMDLIST.StartCmd();

   if (gsc_attachExtractDetach(dwgName) != GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gsc_attachExtractDetach()                        
/*+
  Questa funzione esegue l'attach, estrazione di tutto e detach di un dwg.
  Parametri:
  C_STRING &dwgName; Path completa stile ade(es. "COMPUTER1:\DIR\A.DWG")
  int retest;     se MORETESTS -> in caso di errore riprova n volte 
                  con i tempi di attesa impostati poi ritorna GS_BAD,
                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                  (default = MORETESTS)
                    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/*********************************************************/
int gsc_attachExtractDetach(C_STRING &dwg, int retest)
{
   C_STRING  newPath;
   ade_id    dwgId = ADE_NULLID;
   C_RB_LIST extractCond;

   // disattivo tutti i dwg attaccati
   if (gsc_AllDwgDeactivate() == GS_BAD) return GS_BAD;

   do
   {
	   // effettuo l'attach e l'activate del sinottico 
	   if (gsc_ADEdrv2nethost(dwg.get_name(), newPath) == GS_BAD) break;
	   if ((dwgId = gsc_dsattach(newPath.get_name())) != ADE_NULLID)
		   if (gsc_dwgactivate(dwgId) != GS_GOOD) break;

	   // imposto condizione spaziale con ALL    // settare errore
	   if ((extractCond << acutBuildList(RTLB, RTSTR, ALL_SPATIAL_COND, RTLE, 0)) == NULL) break;
	   if (ade_qrydefine(GS_EMPTYSTR, GS_EMPTYSTR, GS_EMPTYSTR, _T("Location"),
                        extractCond.get_head(), GS_EMPTYSTR) == ADE_NULLID)
		   { GS_ERR_COD = eGSQryCondNotDef; break; }

	   // setto la modalità di estrazione
	   if (ade_qrysettype(_T("draw"), FALSE, GS_EMPTYSTR, GS_EMPTYSTR) != RTNORM) break;

	   // effettuo l'estrazione del disegno (sinottico)
	   ade_qryexecute();
   }
   while (0);

   if (dwgId != ADE_NULLID)
      gsc_dsdetach(dwgId, retest);

   // attivo tutti i dwg attaccati
   if (gsc_AllDwgActivate() == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_saveobjs                           <external> /*
/*+
  Salva gli oggetti nei disegni di provenienza.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_saveobjs()
{
   resbuf param;

   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   param.restype     = RTSHORT;
   param.resval.rint = 1;
   param.rbnext      = NULL;

   // pulisco lo stack degli errori ADE per evitare che MAP visualizzi la lista
   // di errori ADE precedenti
   ade_errclear();

   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave

   // salvo le componenti grafiche nei disegni di partenza
   if (ade_saveobjs(&param) == RTERROR)
      acutPrintf(gsc_msg(260)); // "\nNessun oggetto modificato."
   else            
      acutPrintf(gsc_msg(261)); // "\nSalvataggio oggetti modificati terminato."

   GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;

   if (ade_errqty() > 0) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_savetodwg <external> /*
/*+
  Salva gli oggetti editati nel disegno indicato.
  Parametri:
  C_SELSET &SelSet;  Gruppo di selezione
  ade_id dwgId;      identificatore del disegno attaccato
  int retest;        se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)

  oppure:

  ads_name selSet;   gruppo di selezione
  ade_id dwgId;      identificatore del disegno attaccato
  int retest;        se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_savetodwg(C_SELSET &SelSet, AcMapAttachedDrawing *pMapAttachedDwg, int retest)
{
   ads_name ss;
   SelSet.get_selection(ss);
   
   return gsc_savetodwg(ss, pMapAttachedDwg, retest);
}
int gsc_savetodwg(ads_name selSet, AcMapAttachedDrawing *pMapAttachedDwg, int retest)
{
   ade_errclear();
   if (retest == MORETESTS)
   {
      int tentativi = 1;
      
      do
      {
         do
         {  // se non è il primo tentativo
            if (tentativi > 1) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime()); // attesa tra un tentativo e l'altro
            GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
            if (ade_savetodwg(selSet, gsc_getAdeId(pMapAttachedDwg)) == RTNORM)
               { GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false; return GS_GOOD; }
            GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
            tentativi++;
         }
         while (tentativi <= GEOsimAppl::GLOBALVARS.get_NumTest());
      
         if (gsc_dderroradmin(_T("ADE")) != GS_GOOD) // l'utente vuole abbandonare
            { GS_ERR_COD = eGSCannotSaveEntIntoDwg; return GS_BAD; }
      }
      while (1);
   }
   else
   {
      GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = true; // usato nell'evento beginSave
      if (ade_savetodwg(selSet, gsc_getAdeId(pMapAttachedDwg)) != RTNORM)
      {
         GEOsimAppl::CMDLIST.CmdRunningFromInternalCall = false;
         #if defined(GSDEBUG) // se versione per debugging
            gsc_printAdeErr();
         #endif
         GS_ERR_COD = eGSCannotSaveEntIntoDwg; 
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_AllDwgDetach()                        
/*+
  Questa funzione esegue il detach dei dwg ai quali e' stato fatto 
  l'attach di ADE con filtro opzionale sui soli dwg di una classe.
  Parametri:
  int prj;        codice progetto  (default = 0)
  int cls;        codice classe    (default = 0)
                    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/*********************************************************/
int gsc_AllDwgDetach(int prj, int cls)
{
   presbuf   p, DwgName = NULL;
   C_RB_LIST list_dwg;
   ade_id    dwg_id;
   C_STRING  str_num, name, PrefixName;
   int       result = GS_GOOD;

   // lista dei dwg 'attaccati'
   if ((p = ade_dslist(ADE_NULLID, ADE_FALSE)) == NULL) return GS_GOOD;

   list_dwg << p;
   if (prj != 0 && cls != 0)
   {
      TCHAR *cod36prj = NULL, *cod36cls = NULL;

      if (gsc_long2base36(&cod36prj, prj, 2) == GS_BAD) return GS_BAD;
      if (gsc_long2base36(&cod36cls, cls, 3) == GS_BAD) 
         { free(cod36prj); return GS_BAD; }
      PrefixName = cod36prj;
      PrefixName += cod36cls;
      free(cod36prj);
      free(cod36cls);
   }

   p = list_dwg.get_head();
   while (p)
   {
      dwg_id = (ade_id) p->resval.rreal;
      if (prj != 0 && cls != 0)
      {
         if ((DwgName = ade_dwggetsetting(dwg_id, _T("dwgname"))) == NULL)
            { result = GS_BAD; GS_ERR_COD = eGSDwgCannotDetach; break; }
         gsc_splitpath(DwgName->resval.rstring, NULL, NULL, &name);
         name.set_chr('\0', 5);
         acutRelRb(DwgName); DwgName = NULL;

         // no case-senstive
         if (name.comp(PrefixName, FALSE) == 0)
            if (gsc_dsdetach(dwg_id) == GS_BAD) { result = GS_BAD; break; }
      }
      else
         if (gsc_dsdetach(dwg_id) == GS_BAD) { result = GS_BAD; break; }

      p = list_dwg.get_next();
   }
   if (DwgName) acutRelRb(DwgName);

   return result;
}


/***********************************************************/
/*.doc (new 2) gsc_AllDwgDeactivate()                 
/*+
   Questa funzione esegue la disattivazione di tutti i dwg attaccati alla
   sessione corrente. Se viene specificato un progetto e una classe (> 0) allora
   tutti i dwg della classe saranno ignorati dalla funzione.
   Parametri:
   int ExceptionPrj;     codice progetto  (default = 0)
   int ExceptionCls;     codice classe    (default = 0)

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/*************************************************************/
int gsc_AllDwgDeactivate(int ExceptionPrj, C_INT_LIST *ExceptionClsList)
{
   C_RB_LIST  list_dwg;
   C_DWG_LIST ExceptionDwgList;
   C_DWG      *pDwg;
   C_STRING   DwgName;
   C_INT      *pExceptionCls;

   // inizializzo la lista dei dwg della classe
   if (ExceptionPrj > 0 && ExceptionClsList && ExceptionClsList->get_count() > 0)
   {
      C_DWG_LIST ClassDwgList;

      pExceptionCls = (C_INT *) ExceptionClsList->get_head();
      while (pExceptionCls)
      {
         if (ClassDwgList.load(ExceptionPrj, pExceptionCls->get_key()) == GS_BAD) return GS_BAD;
         // converto i nomi in formato ADE
         pDwg = (C_DWG *) ClassDwgList.get_head();
         while (pDwg)
         {
            if (DwgName.paste(pDwg->get_ADEAliasPath()) == NULL) return GS_BAD;
            if (pDwg->set_name(DwgName.get_name()) == GS_BAD) return GS_BAD;
            pDwg = (C_DWG *) ClassDwgList.get_next();
         }
         ExceptionDwgList.paste_tail(ClassDwgList);

         pExceptionCls = (C_INT *) ExceptionClsList->get_next();
      }
   }

   // lista dei dwg 'attaccati'
   if ((list_dwg << ade_dslist(ADE_NULLID, ADE_FALSE)) != NULL)
   {
      presbuf  p, DwgName;
      ade_id   dwg_id;

      p = list_dwg.get_head();
      while (p)
      {
         dwg_id = (ade_id) p->resval.rreal;
         if (ade_dwgisactive(dwg_id) == ADE_TRUE)
         {
            if (ExceptionPrj <= 0 || !ExceptionClsList || ExceptionClsList->get_count() <= 0)
            {
               if (ade_dwgdeactivate(dwg_id) != RTNORM)
                  { GS_ERR_COD = eGSDwgCannotDeactivate; return GS_BAD; }
            }
            else // salto quelli della classe
            {
               if ((DwgName = ade_dwggetsetting(dwg_id, _T("dwgname"))) == NULL)
                  { GS_ERR_COD = eGSDwgCannotDeactivate; return GS_BAD; }
               // cerca nome (case insensitive)
               if (ExceptionDwgList.search_name(DwgName->resval.rstring, FALSE) == NULL)
                  if (ade_dwgdeactivate(dwg_id) != RTNORM)
                  {
                     acutRelRb(DwgName);
                     GS_ERR_COD = eGSDwgCannotDeactivate;
                     #if defined(GSDEBUG) // se versione per debugging
                        gsc_printAdeErr();
                     #endif
                     return GS_BAD;
                  }
               acutRelRb(DwgName);
            }
         }

         p = list_dwg.get_next();
      } 
   }

   return GS_GOOD;
}
int gsc_AllDwgDeactivate(int ExceptionPrj, int ExceptionCls)
{
   if (ExceptionPrj > 0 && ExceptionCls > 0)
   {
      C_INT_LIST ExceptionClsList;
      C_INT      *pExceptionCls = NULL;

      if ((pExceptionCls = new C_INT(ExceptionCls)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      ExceptionClsList.add_head(pExceptionCls);

      return gsc_AllDwgDeactivate(ExceptionPrj, &ExceptionClsList);
   }
   else
      return gsc_AllDwgDeactivate();
}


/**********************************************************/
/*.doc (new 2) gsc_AllDwgActivate()                         
*+
   Questa funzione esegue l'attivazione di tutti i dwg attaccati alla
   sessione corrente.

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**********************************************************/
int gsc_AllDwgActivate(void)
{
   C_RB_LIST list_dwg;

   // lista dei dwg 'attaccati'
   if ((list_dwg << ade_dslist(ADE_NULLID, ADE_FALSE)) != NULL)
   {
      presbuf p;
      ade_id  dwg_id;

      p = list_dwg.get_head();
      while (p)
      {
         dwg_id = (ade_id) p->resval.rreal;
         if (gsc_dwgactivate(dwg_id) != GS_GOOD) return GS_BAD;

         p = list_dwg.get_next();
      } 
   }

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_load_spatial_qry <internal> */
/*+
  Questa funzione carica una condizione spaziale da file e la imposta 
  nell'oggetto AcMapQuery.
  Parametri:
  const TCHAR *SrcQryFile;  Opzionale, Se = NULL verrà usata la query corrente,
                           altrimenti verrà caricata una query dal file indicato
                           ponendola come qry corrente
  AcMapQuery &Query;

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_load_spatial_qry(const TCHAR *SrcQryFile, AcMapQuery &Query)
{
   FILE                   *file;
   C_STR_LIST             SpatialSelList;
   C_STR                  *pSpatialSel;
   C_STRING               JoinOp, Boundary, SelMode;
   bool                   Inverse;
   C_RB_LIST              CoordList;
   AcMapQueryBranch       Branch;
   AcMapLocationCondition Cond;
   presbuf                pRbPoint;

   // Apro il file in sola lettura NON in modo UNICODE (i file .QRY sono ancora in ANSI)
   if ((file = gsc_fopen(SrcQryFile, _T("r"))) == NULL)
      return GS_BAD;
   // Leggo la condizione spaziale
   if (gsc_SpatialSel_Load(file, SpatialSelList) == GS_BAD)
      { gsc_fclose(file); return GS_BAD; }
   gsc_fclose(file);

   pSpatialSel = (C_STR *) SpatialSelList.get_head();
   while (pSpatialSel)
   {
      // La scompongo (es. Boundary = "window", SelMode = "crossing")
      if (gsc_SplitSpatialSel(pSpatialSel, JoinOp, &Inverse, Boundary, SelMode, CoordList) == GS_BAD)
         return GS_BAD;

      Cond.SetJoinOperator((JoinOp.comp("AND", 0) == 0) ? AcMap::kOperatorAnd : AcMap::kOperatorOr);

      Cond.SetIsNot(Inverse);

      if (Boundary.comp(ALL_SPATIAL_COND, FALSE) == 0) // tutto
		   // set ALL condition
		   Cond.SetBoundary(&AcMapAllBoundary()) ;
      else if (Boundary.comp(POINT_SPATIAL_COND, FALSE) == 0) // punto
      {
         if (!(pRbPoint = CoordList.get_head()) || pRbPoint->restype != RTPOINT)
            return GS_BAD;
         AcGePoint3d pp(pRbPoint->resval.rpoint[X], pRbPoint->resval.rpoint[Y], 0.0) ;
	      AcMapPointBoundary Boundary(pp);

		   // initialize POINT condition
		   Cond.SetBoundary(&Boundary);
		   // the only type of location type can be applied to the POINT condition
		   Cond.SetLocationType(AcMap::kLocationCrossing);
      }
	   else if (Boundary.comp(CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
      {
         if (!(pRbPoint = CoordList.get_head()) || pRbPoint->restype != RTPOINT)
            return GS_BAD;
         AcGePoint3d pp(pRbPoint->resval.rpoint[X], pRbPoint->resval.rpoint[Y], 0.0) ;
         if (!(pRbPoint = pRbPoint->rbnext) || pRbPoint->restype != RTREAL)
            return GS_BAD;
         AcMapCircleBoundary Boundary(pp, pRbPoint->resval.rreal);

		   // initialize CIRCLE condition
         Cond.SetBoundary(&Boundary);
         if (SelMode.comp(_T("crossing"), FALSE) == 0)
			   Cond.SetLocationType(AcMap::kLocationCrossing);
         else
			   Cond.SetLocationType(AcMap::kLocationInside);
      }
	   else if (Boundary.comp(FENCE_SPATIAL_COND, FALSE) == 0 || // fence
               Boundary.comp(BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // bufferfence
      {
         AcGePoint3dArray pp;

         pRbPoint = CoordList.get_head();
         while (pRbPoint)
         {
            if (pRbPoint->restype != RTPOINT) return GS_BAD;
			   pp.append(AcGePoint3d(pRbPoint->resval.rpoint[X], pRbPoint->resval.rpoint[Y], 0.0));
            pRbPoint = CoordList.get_next();
         }
   	   AcMapFenceBoundary Boundary(pp);

		   // initialize FENCE condition
         Cond.SetBoundary(&Boundary);
		   // the only type of location type can be applied to the FENCE condition
		   Cond.SetLocationType(AcMap::kLocationCrossing) ;
	   }
	   else if (Boundary.comp(WINDOW_SPATIAL_COND, FALSE) == 0) // window
      {
         if (!(pRbPoint = CoordList.get_head()) || pRbPoint->restype != RTPOINT)
            return GS_BAD;
         AcGePoint3d p1(pRbPoint->resval.rpoint[X], pRbPoint->resval.rpoint[Y], 0.0);
         if (!(pRbPoint = CoordList.get_next()) || pRbPoint->restype != RTPOINT)
            return GS_BAD;
         AcGePoint3d p2(pRbPoint->resval.rpoint[X], pRbPoint->resval.rpoint[Y], 0.0);
   	   AcMapWindowBoundary Boundary(p1, p2);

		   // initialize WINDOW condition
         Cond.SetBoundary(&Boundary);
         if (SelMode.comp(_T("crossing"), FALSE) == 0)
			   Cond.SetLocationType(AcMap::kLocationCrossing);
         else
			   Cond.SetLocationType(AcMap::kLocationInside);
      }
	   else if (Boundary.comp(POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
      {
         AcGePoint3dArray pp;

         pRbPoint = CoordList.get_head();
         while (pRbPoint)
         {
            if (pRbPoint->restype == RTPOINT) // inserisco solo i punti
			      pp.append(AcGePoint3d(pRbPoint->resval.rpoint[X], pRbPoint->resval.rpoint[Y], 0.0));
            pRbPoint = CoordList.get_next();
         }
   	   AcMapPolygonBoundary Boundary(pp);

		   // initialize POLYGON condition
         Cond.SetBoundary(&Boundary);
         if (SelMode.comp(_T("crossing"), FALSE) == 0)
			   Cond.SetLocationType(AcMap::kLocationCrossing);
         else
			   Cond.SetLocationType(AcMap::kLocationInside);
      }
      else
         return GS_BAD;

   	Branch.AppendOperand(&Cond);
   
      pSpatialSel = (C_STR *) SpatialSelList.get_next();
   }

   if (Query.Define(&Branch) != AcMap::kOk) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_save_qry <internal> */
/*+
  Questa funzione riceve categoria e nome della query da salvare:
  prima di salvare la query verifica se ne esiste una con identico nome 
  e la cancella. 
  Parametri:
  TCHAR* cat;              Opzionale, categoria; default = "estrazione"
  TCHAR* name;             Opzionale, nome query; default = "spaz_estr"
  bool OnlyCoordinates;    Opzionale, Flag per salvare solo le coordinate 
                           della query senza le proprietà di 
                           alterazione grafica; default = FALSE
  const TCHAR *SrcQryFile; Opzionale, Se = NULL verrà usata la query corrente,
                           altrimenti verrà caricata una query dal file indicato

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_save_qry(TCHAR *cat, TCHAR *name, bool OnlyCoordinates, 
                 const TCHAR *SrcQryFile)
{
   AcMapSession *pMapSession = NULL;
   AcMapProject *pProj;
   AcMapQuery   *pQuery;
   int          iOptions = AcMap::kSaveLocationCoordinates |  
                           AcMap::kSavePropertyAlteration;

   if (OnlyCoordinates) iOptions = AcMap::kSaveLocationCoordinates; // Solo Coordinate (senza property alteration = 8)

   // Get the Map Session Object
   if ((pMapSession = AcMapGetSession()) == NULL) return GS_BAD;
   // Get the currently active project 
   if (pMapSession->GetProject(pProj) != Adesk::kTrue) return GS_BAD;
   // Create a new query
   if (pProj->CreateQuery(pQuery, Adesk::kFalse) != AcMap::kOk) return GS_BAD;

   if (SrcQryFile) // caricamento query da file
   {
      if (gsc_load_spatial_qry(SrcQryFile, *pQuery) == GS_BAD)
         { delete pQuery; return GS_BAD; }
   }
   else
   {
      // Carico la query con i valori della query corrente
      if (pQuery->LoadFromCurrent() != AcMap::kOk)
         { delete pQuery; return GS_BAD; }
   }

   // Salvo la query internamente
   if (pQuery->Save(cat, name, _T("Query per estrazione"), NULL, iOptions) != AcMap::kOk)
      { delete pQuery; return GS_BAD; }
   delete pQuery;

   if (ade_qryclear() != RTNORM) return GS_BAD;

   return GS_GOOD;   
}


/*********************************************************/
/*.doc gsc_getCurrQryExtents                  <internal> */
/*+
  Questa funzione ricava una finestra contenente la zona
  impostata dalla query di ADE corrente.
  Parametri:
  double *Xmin;
  double *Ymin;
  double *Xmax;
  double *Ymax;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getCurrQryExtents(C_RECT_LIST &RectList)
{
   int       i = 1;
   C_RB_LIST QryCondIDList, QryCond;
   presbuf   p;
   double    Xmin, Ymin, Xmax, Ymax;
   C_RECT    *pRect;

   RectList.remove_all();

   // se esiste una sola query spaziale definita
   if ((QryCondIDList << gsc_ade_qrylist()) == NULL) return GS_BAD;

   while ((p = QryCondIDList.getptr_at(i++)) != NULL)
   {
      QryCond << ade_qrygetcond((ade_id) p->resval.rreal);
      p = QryCond.getptr_at(4); // CondType
      if (gsc_strcmp(p->resval.rstring, _T("location"), FALSE) == 0)
      {
         p = QryCond.getptr_at(1); // JoinOperator
         // Se l'operatore è "and" non sono in grado di stabilire l'estensione
         // (es. intersezione tra un cerchio e un poligono)
         if (gsc_strcmp(p->resval.rstring, _T("and"), FALSE) == 0) return GS_BAD;
         if (gsc_getQryCondExtents(QryCond, &Xmin, &Ymin, &Xmax, &Ymax) == GS_BAD) return GS_BAD;
         if ((pRect = new C_RECT(Xmin, Ymin, Xmax, Ymax)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         
         RectList.add_tail(pRect);
      }
   }

   return (RectList.get_count() > 0) ? GS_GOOD : GS_BAD;
}
int gsc_getCurrQryExtents(double *Xmin, double *Ymin, double *Xmax, double *Ymax)
{
   C_RECT_LIST RectList;
   C_RECT      *pRect;

   if (gsc_getCurrQryExtents(RectList) == GS_BAD) return GS_BAD;
   pRect = (C_RECT *) RectList.get_head();
   *Xmin = pRect->Left();
   *Ymin = pRect->Bottom();
   *Xmax = pRect->Right();
   *Ymax = pRect->Top();
   pRect = (C_RECT *) RectList.get_next();
   while (pRect)
   {
      if (*Xmin > pRect->Left()) *Xmin = pRect->Left();
      if (*Ymin > pRect->Bottom()) *Ymin = pRect->Bottom();
      if (*Xmax < pRect->Right()) *Xmax = pRect->Right();
      if (*Ymax < pRect->Top()) *Ymax = pRect->Top();

      pRect = (C_RECT *) RectList.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getQryCondExtents                      <internal> */
/*+
  Questa funzione ricava una finestra contenente la zona impostata
  da una sola condizione di Qry indicata dal parametro <QryCond>.
  (per ora solo se si tratta di estrazione di tipo "window", "circle",
   "polyline", "fence", "polygon", "point")
  Parametri:
  C_RB_LIST &QryCond; Condizione spaziale
  double *Xmin;
  double *Ymin;
  double *Xmax;
  double *Ymax;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getQryCondExtents(C_RB_LIST &QryCond, double *Xmin, double *Ymin, double *Xmax, double *Ymax)
{
   presbuf p;
   int     Result = GS_BAD;
   
   // non ammetto l'operatore NOT
   if ((p = QryCond.getptr_at(3)) == NULL || p->restype != RTSTR) return GS_BAD;
   if (gsc_strcmp(p->resval.rstring, _T("not"), FALSE) == 0) return GS_BAD;

   // Deve esserci una sola condizione spaziale
   if ((p = QryCond.getptr_at(6)) == NULL || p->restype != RTSTR) return GS_BAD;

   // controllo il tipo di contorno
   if (gsc_strcmp(p->resval.rstring, WINDOW_SPATIAL_COND, FALSE) == 0) // window
   {
      if ((p = QryCond.getptr_at(8)) != NULL && 
          (p->restype == RTPOINT || p->restype == RT3DPOINT))
      {
         *Xmin = p->resval.rpoint[X];
         *Ymin = p->resval.rpoint[Y];

         if ((p = QryCond.getptr_at(9)) != NULL && 
             (p->restype == RTPOINT || p->restype == RT3DPOINT))
         {
            *Xmax = p->resval.rpoint[X];
            *Ymax = p->resval.rpoint[Y];

            Result = GS_GOOD;
         }
      }
   }
   else if (gsc_strcmp(p->resval.rstring, CIRCLE_SPATIAL_COND, FALSE) == 0) // cerchio
   {
      if ((p = QryCond.getptr_at(8)) != NULL && 
          (p->restype == RTPOINT || p->restype == RT3DPOINT))
      {
         *Xmin = p->resval.rpoint[X]; // coordinata X centro cerchio
         *Ymin = p->resval.rpoint[Y]; // coordinata Y centro cerchio

         if ((p = QryCond.getptr_at(9)) != NULL && p->restype == RTREAL)
         {  // raggio
            *Xmax = *Xmin + p->resval.rreal;
            *Ymax = *Ymin + p->resval.rreal;
            *Xmin = *Xmin - p->resval.rreal;
            *Ymin = *Ymin - p->resval.rreal;

            Result = GS_GOOD;
         }
      }
   }
   else if (gsc_strcmp(p->resval.rstring, POLYLINE_SPATIAL_COND, FALSE) == 0 ||  // polyline
            gsc_strcmp(p->resval.rstring, FENCE_SPATIAL_COND, FALSE) == 0 ||     // fence
            gsc_strcmp(p->resval.rstring, POLYGON_SPATIAL_COND, FALSE) == 0)     // polygon
   {
      ads_point corner1, corner2;

      if (gsc_get_AdeQryPolyFence_window(QryCond, corner1, corner2) == GS_BAD)
         return GS_BAD;
      
      *Xmin = corner1[X];
      *Ymin = corner1[Y];
      *Xmax = corner2[X];
      *Ymax = corner2[Y];

      Result = GS_GOOD;
   }
   else if (gsc_strcmp(p->resval.rstring, POINT_SPATIAL_COND, FALSE) == 0) // punto
   {
      if ((p = QryCond.getptr_at(7)) != NULL && 
          (p->restype == RTPOINT || p->restype == RT3DPOINT))
      {
         *Xmax = *Xmin = p->resval.rpoint[X];
         *Ymax = *Ymin = p->resval.rpoint[Y];
      }

      Result = GS_GOOD;
   }
 
   if (Result == GS_GOOD)
   {
      // mi assicuro che Xmin sia minore di Xmax e Ymin sia minore di Ymax
      if (*Xmin > *Xmax)
      {
         double dummy = *Xmin;
         *Xmin = *Xmax;
         *Xmax = dummy;
      }
      if (*Ymin > *Ymax)
      {
         double dummy = *Ymin;
         *Ymin = *Ymax;
         *Ymax = dummy;
      }
   }

   return Result;
}


/***************************************************************************/
/*.doc gsc_find_qry <external> */
/*+
  Questa funzione serve per trovare una ade_query nota la lista delle
  categorie, il nome della categoria, il nome della query e una modalità di 
  operatività sulla query.
  C_CLASS * pCls  : puntatore alla classe;
  presbuf cat_list: lista delle categorie;
  TCHAR* cat_qry:    nome della categoria che si vuole cercare;
  TCHAR* name_query: nome della query;
  int mod:          modalità di chiamata; i valori di mod sono:
                    - 2 carica la query;
                    - 1 deleta le query che non interessano;
                    - 0 non fa nulla.

  La funzione restituisce il valore del flag flg (1 = esiste la query 0 = non esiste).
-*/  
/***************************************************************************/
int gsc_find_qry(C_CLASS *pCls, presbuf cat_list, TCHAR *cat_qry, 
                 TCHAR *name_query, int mod)
{
   presbuf    cat_info_qry, cat_info_name, qry_name, q_list = NULL;
   C_RB_LIST  listqry;
   int        flg = 0, flag = 0;
   TCHAR      *point = NULL, sql_name[11] = GS_EMPTYSTR;
   ade_id     qry_id = 0;

   while (cat_list != NULL)
   {
      cat_info_name = ade_qlgetctgyinfo((ade_id) cat_list->resval.rreal, _T("name")); 

      //carico i nomi delle categorie
      while (cat_info_name != NULL)
      {
         if (gsc_strcmp(cat_info_name->resval.rstring, cat_qry) == 0)        
         {  // cerco la categoria che mi serve
            cat_info_qry = ade_qlgetctgyinfo((ade_id) cat_list->resval.rreal, _T("qrylist"));
            //carico la lista delle queries della categoria
            while (cat_info_qry != NULL)
            {
               qry_id = (ade_id) cat_info_qry->resval.rreal;
               if ((qry_name = ade_qlgetqryinfo(qry_id, _T("name"))) == NULL) return GS_BAD;

               //cerco la query che mi interessa
               if (gsc_strcmp(qry_name->resval.rstring, name_query) == 0)
               {
                  flag = flg = 1;
                  if (mod == 2) 
                  { // carico la query
                     if (ade_qlloadqry(qry_id) != RTNORM) return GS_BAD;
                     break;
                  }
                  if (mod == 1)   
                  {  // creo la query finale per la classe selezionata
                     // pulisce FAS, coord. spaz. e SQL
                     if (ade_qryclear() != RTNORM) return GS_BAD;
                     wcscpy(sql_name, _T("sqlu"));
                     wcscat(sql_name, name_query + 4);

                     // cerco se esiste una query di SQL per la classe e la carico
                     if (gsc_ade_qlloadqry(sql_name) != RTERROR)
                     {
                        if ((q_list = gsc_ade_qrylist()) != NULL)
                        {  // cerco la condizione di SQL e la compongo
                           if ((listqry << ade_qrygetcond((ade_id) q_list->resval.rreal))	== NULL) 
							         return GS_BAD;
                        }
                     }
                     // se non è una query di tipo SQL la carico
                     if (name_query[0] != _T('s'))
                     {
                        if (ade_qlloadqry(qry_id) != RTNORM) return GS_BAD;
                        // aggiungo la condizione di SQL se esiste
                        if (q_list != NULL)
                        {
                           for (int i=0; i<4; i++) listqry.remove_head();
                           listqry.remove_tail();
                           if (ade_qrydefine(GS_EMPTYSTR, GS_EMPTYSTR , GS_EMPTYSTR, _T("SQL"),
                                             listqry.get_head(), _T(")")) == ADE_NULLID)
                              { GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
                        }
                     }

                     if (q_list || name_query[0] != _T('s'))
                     {
                        if (name_query[0] == _T('f')) name_query[3] = _T('f');
                        else
                        {
                           wcscpy(name_query, _T("fasf"));
                           wcscat(name_query, qry_name->resval.rstring+4);
                        }
                        if (gsc_save_qry(_T("estrazione"), name_query) == GS_BAD) return GS_BAD;
                        acutRelRb(q_list);
                     }
                  }
               }
               // cancello la query se non serve più
               if (gsc_strcmp((qry_name->resval.rstring) + 4, name_query + 4) == 0 && flag == 0
                   && mod == 1 && qry_name->resval.rstring[0] != _T('s'))
                  if (ade_qldelquery(qry_id) != RTNORM) return GS_BAD;
               flag = 0;
               cat_info_qry = cat_info_qry->rbnext;
           }
        }
        if (flg == 1) break; 
        cat_info_name = cat_info_name->rbnext;
     }
     if (flg == 1) break;
     cat_list = cat_list->rbnext ;
   }

   return flg;
}




/*********************************************************/
/*.doc gsc_IsClassWithFASOrSQL                <external> */
/*+
  Questa funzione ritorna GS_GOOD se la classe ha almeno una condizione di
  FAS o di SQL attiva per la prossima estrazione.
  Parametri:
  int prj:     codice progetto
  int cls:     codice classe
  int sub:     sottoclasse
  int *type;   tipo di impostazione effettuata sulla classe
               se = GRAPHICAL se è variata la FAS
               se = RECORD si estrae ricercando per condizione SQL
               se = ALL entrambe le 2 precedenti

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_IsClassWithFASOrSQL(int cod_prj, int cod_cls, int cod_sub, int *type)
{
   C_RB_LIST list_cat, ListCatInfo, ListCatQry, List1, List2;
   presbuf   cat_info_qry, cat_info_name, qry_name, cat, qry_cond;
   TCHAR     qry_cat[] = _T("estrazione");
   C_STRING  n_query;
   ade_id    qry_id = 0, sql_id = 0;

   *type = -1;
   //carico la query relativa alla classe
   if ((list_cat << ade_qllistctgy()) == NULL) return GS_BAD;

   n_query = _T("fasf");
   n_query += cod_cls;
   n_query += _T("-");
   n_query += cod_sub;

   cat = list_cat.get_head();
   while (cat)
   {
      ListCatInfo << ade_qlgetctgyinfo((ade_id) cat->resval.rreal, _T("name"));
      cat_info_name = ListCatInfo.get_head();
      while (cat_info_name)
      {
         if (gsc_strcmp(cat_info_name->resval.rstring, qry_cat) == 0)
         //cerco la categoria
         { 
            ListCatQry << ade_qlgetctgyinfo((ade_id) cat->resval.rreal, _T("qrylist"));
            cat_info_qry = ListCatQry.get_head();
            //carico la lista query della categoria
            while (cat_info_qry)
            {
               qry_id = (ade_id) cat_info_qry->resval.rreal;
               if ((qry_name = ade_qlgetqryinfo(qry_id, _T("name"))) == NULL) return GS_BAD;
               //cerco la query che mi interessa
               if (gsc_strcmp(qry_name->resval.rstring, n_query.get_name()) == 0)
               {
                  // carico la query
                  if (ade_qlloadqry(qry_id) == RTERROR) return GS_BAD;
                  if ((qry_cond = gsc_ade_qrylist()) != NULL) sql_id = qry_cond->resval.rreal; 
                  // se non ci sono condizioni SQL e FAS
                  List1 << ade_qrygetcond(sql_id);
                  List2 << ade_altplist();
                  
                  if (List1.get_head() || List2.get_head())
                  {
                     if (List1.get_head()) *type = RECORD;
                     else if (List2.get_head()) *type = GRAPHICAL;
                     else *type = ALL;
                     return GS_GOOD;
                  }
               }
               cat_info_qry = ListCatQry.get_next();
            }
         }
         cat_info_name = ListCatInfo.get_next();
      }
      cat = list_cat.get_next();
   }

   return GS_BAD;
}


/***************************************************************************/
/*.doc gsc_GetAreaFromReport                                    <internal> */
/*+
  Questa funzione legge un'area contenente le coordinate scritte in un file
  di report generato da ADE.
  Parametri:
  const TCHAR *Path;    Path del file report
  ads_point  pt1;       angolo basso-sinistra
  ads_point  pt2;       angolo alto-destra

  La funzione restituisce GS_GOOD in caso di succeso, altrimenti GS_BAD.
-*/  
/***************************************************************************/
int gsc_GetAreaFromReport(const TCHAR *Path, ads_point corner1, ads_point corner2)
{
   ads_point pt1, pt2;
   FILE      *file;
   TCHAR     *str, *end;
   double    value;
   C_STRING  buffer, DecSep, ListSep, StrValue;
   int       i;

   if (gsc_getLocaleDecimalSep(DecSep) == GS_BAD || gsc_getLocaleListSep(ListSep) == GS_BAD)
      return GS_BAD;

   // verifico che il separatore decimale sia diverso dal separatore di lista 
   if (gsc_strcmp(DecSep.get_name(), ListSep.get_name()) == 0)
		{ GS_ERR_COD = eGSNoValidListSep; return GS_BAD; }
 
   bool Unicode = false;
   if ((file = gsc_fopen(Path, _T("r"), ONETEST, &Unicode)) == NULL) return GS_BAD;

   if (gsc_readline(file, buffer, Unicode) == WEOF) { gsc_fclose(file); return GS_BAD; }
   {
      str = buffer.get_name();

      for (i = 0; i < 3; i++)
      {
         if (!str || wcslen(str) == 0)
            { GS_ERR_COD = eGSBadLocationQry; gsc_fclose(file); return GS_BAD; }

         if ((end = gsc_strstr(str, ListSep.get_name())))
         {
            end[0]   = _T('\0');
            StrValue = str;
            str      = end + ListSep.len();
         }
         else
         {
            StrValue = str;
            str      = NULL;
         }

         StrValue.strtran(DecSep.get_name(), _T("."));
         pt1[i] = _wtof(StrValue.get_name());
      }

      if (str && wcslen(str) > 0)
      {
         for (i = 0; i < 3; i++)
         {
            if (!str || wcslen(str) == 0)
               { GS_ERR_COD = eGSBadLocationQry; gsc_fclose(file); return GS_BAD; }

            if ((end = gsc_strstr(str, ListSep.get_name())))
            {
               end[0]   = _T('\0');
               StrValue = str;
               str      = end + ListSep.len();
            }
            else
            {
               StrValue = str;
               str      = NULL;
            }

            StrValue.strtran(DecSep.get_name(), _T("."));
            pt2[i] = _wtof(StrValue.get_name());
         }
      
         corner1[X] = gsc_min(pt1[X], pt2[X]);
         corner1[Y] = gsc_min(pt1[Y], pt2[Y]);
         corner1[Z] = gsc_min(pt1[Z], pt2[Z]);

         corner2[X] = gsc_max(pt1[X], pt2[X]);
         corner2[Y] = gsc_max(pt1[Y], pt2[Y]);
         corner2[Z] = gsc_max(pt1[Z], pt2[Z]);

      }
      else
      {
         ads_point_set(pt1, corner1);
         ads_point_set(corner1, corner2);
      }

      while (gsc_readline(file, buffer, Unicode) != WEOF)
      {
         str = buffer.get_name();

         for (i = 0; i < 3; i++)
         {
            if (!str || wcslen(str) == 0)
               { GS_ERR_COD = eGSBadLocationQry; gsc_fclose(file); return GS_BAD; }

            if ((end = gsc_strstr(str, ListSep.get_name())))
            {
               end[0]   = _T('\0');
               StrValue = str;
               str      = end + ListSep.len();
            }
            else
            {
               StrValue = str;
               str      = NULL;
            }

            StrValue.strtran(DecSep.get_name(), _T("."));
            pt1[i] = _wtof(StrValue.get_name());
         }

         if (str && wcslen(str) > 0)
         {
            for (i = 0; i < 3; i++)
            {
               if (!str || wcslen(str) == 0)
                  { GS_ERR_COD = eGSBadLocationQry; gsc_fclose(file); return GS_BAD; }

               if ((end = gsc_strstr(str, ListSep.get_name())))
               {
                  end[0]   = _T('\0');
                  StrValue = str;
                  str      = end + ListSep.len();
               }
               else
               {
                  StrValue = str;
                  str      = NULL;
               }

               StrValue.strtran(DecSep.get_name(), _T("."));
               pt2[i] = _wtof(StrValue.get_name());
            }
         
            value = gsc_min(pt1[X], pt2[X]);
            if (corner1[X] > value) corner1[X] = value;
            value = gsc_min(pt1[Y], pt2[Y]);
            if (corner1[Y] > value) corner1[Y] = value;
            value = gsc_min(pt1[Z], pt2[Z]);
            if (corner1[Z] > value) corner1[Z] = value;

            value = gsc_max(pt1[X], pt2[X]);
            if (corner2[X] < value) corner2[X] = value;
            value = gsc_max(pt1[Y], pt2[Y]);
            if (corner2[Y] < value) corner2[Y] = value;
            value = gsc_max(pt1[Z], pt2[Z]);
            if (corner2[Z] < value) corner2[Z] = value;
         }
         else
         {
            if (corner1[X] > pt1[X]) corner1[X] = pt1[X];
            if (corner1[Y] > pt1[Y]) corner1[Y] = pt1[Y];
            if (corner1[Z] > pt1[Z]) corner1[Z] = pt1[Z];

            if (corner2[X] < pt1[X]) corner2[X] = pt1[X];
            if (corner2[Y] < pt1[Y]) corner2[Y] = pt1[Y];
            if (corner2[Z] < pt1[Z]) corner2[Z] = pt1[Z];
         }
      }
      gsc_fclose(file);
   }
   
   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_printAdeErr(internal) */
/*+
    Questa funzione stampa gli errori nello stack di ADE
-*/
/****************************************************************************/
void gsc_printAdeErr(void) 
{
   presbuf pLista, pMsg, pDiag;
   int     iCount, ii, jj = 0;

   if ((pLista = gsc_get_ADEdiag()) == NULL || (iCount = gsc_length(pLista)) <= 0)
      { if (pLista) acutRelRb(pLista); return; }

   // Print number of conditions in diagnostics area
   acutPrintf(_T("\nNumber of errors: %d"), iCount);
   // Print position where error was detected
   acutPrintf(_T("\n#     Message"));

   while ((pMsg = gsc_nth(jj++, pLista)) != NULL)
   {
      acutPrintf(_T("\n%s"), gsc_nth(0, pMsg)->resval.rstring);  
      if ((pMsg = gsc_nth(1, pMsg)) != NULL)
      {
         ii = 0;
         // Print diagnostics parameters
         while ((pDiag = gsc_nth(ii++, pMsg)) != NULL)
            acutPrintf(_T("\n%s"), pDiag->resval.rstring);  
      }
   }

   acutRelRb(pLista);
}


/*****************************************************************************/
/*.doc gsc_get_ADEdiag                                           */
/*+	   
   Restituisce una lista di resbuf contenente le informazioni diagnostiche
   nel seguente formato:
   ((<message 1> (<param 1><param 2>...))(<message 2> (<param 1><param 2>...)) ...)

   Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*****************************************************************************/
presbuf gsc_get_ADEdiag(void)
{
   int       iCount, jj;
   TCHAR     message[512];
   C_RB_LIST list;

   if (list << acutBuildList(RTLB, 0) == NULL) return NULL;

   // Number of conditions in diagnostics area
   if ((iCount = ade_errqty()) > 0)
   {
      for (jj = 0; jj < iCount; jj++)
      {
         // Store messages
         swprintf(message, 512, _T("%-5d %-12s"), jj + 1, ade_errmsg(jj));
         if ((list += acutBuildList(RTLB, RTSTR, message, RTLB, 0)) == NULL)
            return NULL;
         swprintf(message, 512, _T("%s ADE %d"), gsc_msg(613), ade_errtype(jj));    // Tipo errore:
         if ((list += acutBuildList(RTSTR, message, 0)) == NULL)
            return NULL;
         swprintf(message, 512, _T("%s %d"), gsc_msg(614), ade_errcode(jj)); // Codice errore:
         if ((list += acutBuildList(RTSTR, message, 0)) == NULL)
            return NULL;
         swprintf(message, 512, _T("%s %s"), gsc_msg(615), ade_errmsg(jj));  // Messaggio:
         if ((list += acutBuildList(RTSTR, message, 0)) == NULL)
            return NULL;

         if ((list += acutBuildList(RTLE, RTLE, 0)) == NULL) return NULL;
      }
   }
   if ((list += acutBuildList(RTLE, 0)) == NULL) return NULL;

   list.ReleaseAllAtDistruction(GS_BAD);
   
   return list.get_head();
}


/***************************************************************************/
/*.doc gsc_altpdefine                                                      */
/*+	   
  Questa funzione definisce delle alterazioni grafiche nella query ADE
  corrente leggendo i valori dalla FAS e considerando solo quelli indicati
  da flag_set (vedi enum GraphSettingsEnum).
  Parametri:
  C_FAS &FAS;        Caratteristiche grafiche
  long  flag_set;    Flag a bit per sapere quali proprietà gestire nella FAS

  Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/**************************************************************************/
int gsc_altpdefine(C_FAS &FAS, long flag_set)
{
   resbuf   *pRb;
   C_STRING StrNum;

   if (flag_set & GSBlockNameSetting)
   {
      pRb = gsc_str2rb(FAS.block);
      if (ade_altpdefine(_T("blockname"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSLayerSetting)
   {
      pRb = gsc_str2rb(FAS.layer);
      if (ade_altpdefine(_T("layer"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSTextStyleSetting)
   {
      pRb = gsc_str2rb(FAS.style);
      if (ade_altpdefine(_T("style"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSBlockScaleSetting)
   {
      StrNum = FAS.block_scale; // converto il valore in stringa
      pRb = acutBuildList(RTSTR, StrNum.get_name(), 0);
      if (ade_altpdefine(_T("scale"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSLineTypeSetting)
   {
      pRb = gsc_str2rb(FAS.line);
      if (ade_altpdefine(_T("linetype"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSWidthSetting)
   {
      StrNum = FAS.width; // converto il valore in stringa
      pRb = acutBuildList(RTSTR, StrNum.get_name(), 0);
      if (ade_altpdefine(_T("width"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSColorSetting)
   {
      FAS.color.getString(StrNum); // converto il valore in stringa
      pRb = acutBuildList(RTSTR, StrNum.get_name(), 0);
      if (ade_altpdefine(_T("color"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSTextHeightSetting)
   {
      StrNum = FAS.h_text; // converto il valore in stringa
      pRb = acutBuildList(RTSTR, StrNum.get_name(), 0);
      if (ade_altpdefine(_T("height"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSElevationSetting)
   {
      StrNum = FAS.elevation; // converto il valore in stringa
      pRb = acutBuildList(RTSTR, StrNum.get_name(), 0);
      if (ade_altpdefine(_T("elevation"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   if (flag_set & GSThicknessSetting)
   {
      StrNum = FAS.thickness; // converto il valore in stringa
      pRb = acutBuildList(RTSTR, StrNum.get_name(), 0);
      if (ade_altpdefine(_T("thickness"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   // Riempimenti
   if (flag_set & GSHatchNameSetting &&
       flag_set & GSHatchLayerSetting &&
       flag_set & GSHatchColorSetting &&
       flag_set & GSHatchScaleSetting &&
       flag_set & GSHatchRotationSetting)
   {
      C_STRING StrScale, StrRotation;

      StrScale    = FAS.hatch_scale;    // converto il valore in stringa
      StrRotation = FAS.hatch_rotation; // converto il valore in stringa
      FAS.hatch_color.getString(StrNum); // converto il valore in stringa
      pRb = acutBuildList(RTLB,
                                RTLB, 
                                RTSTR, _T("pattern"), 
                                RTSTR, FAS.hatch,
                                RTDOTE,
                                RTLB, 
                                RTSTR, _T("scale"), 
                                RTSTR, StrScale.get_name(),
                                RTDOTE,
                                RTLB, 
                                RTSTR, _T("rotation"), 
                                RTSTR, StrRotation.get_name(),
                                RTDOTE,
                                RTLB, 
                                RTSTR, _T("layer"), 
                                RTSTR, FAS.hatch_layer,
                                RTDOTE,
                                RTLB, 
                                RTSTR, _T("color"), 
                                RTSTR, StrNum.get_name(),
                                RTDOTE,
                          RTLE, 0);
      if (ade_altpdefine(_T("hatch"), pRb) == ADE_NULLID)
         { acutRelRb(pRb); GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
      acutRelRb(pRb);
   }

   return GS_GOOD;
}


/********************************************************************/
/*.doc gsc_getGraphSettingsFromCurrentADEQry <external> */
/*+
  Questa funzione riceve un presbuf ed rileva settando il vettore di resbuf
  le ultime condizioni impostate dall'utente nella sessione di lavoro.
  Parametri:
  C_FAS &FAS;     Caratteristiche grafiche
  int *flag_set;  flag-impostazioni fas.

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/********************************************************************/
int gsc_getGraphSettingsFromCurrentADEQry(C_FAS &FAS, long *flag_set)
{
   presbuf l_def, l_prop, AlterProp;

   // Lista delle proprietà grafiche alterate dalla query ADE corrente
   l_def = ade_altplist();
   *flag_set = GSNoneSetting;

   while (l_def != NULL)
   {  // rilevo una ad una le prorietà alterate
      if ((AlterProp = ade_altpgetprop((ade_id) l_def->resval.rreal)) == NULL) return GS_BAD;               
      l_prop = AlterProp;
      
      switch (l_prop->restype)
      {
         case RTSTR: 
            if (gsc_strcmp(l_prop->resval.rstring, _T("blockName"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;
               gsc_strcpy(FAS.block, l_prop->resval.rstring, MAX_LEN_BLOCKNAME);
               *flag_set = *flag_set + GSBlockNameSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("layer"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;
               gsc_strcpy(FAS.layer, l_prop->resval.rstring, MAX_LEN_LAYERNAME);
               *flag_set = *flag_set + GSLayerSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("style"), GS_BAD) == 0) 
            {
               l_prop = l_prop->rbnext;
               gsc_strcpy(FAS.style, l_prop->resval.rstring, MAX_LEN_TEXTSTYLENAME);
               *flag_set = *flag_set + GSTextStyleSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("scale"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;
               gsc_rb2Dbl(l_prop, &FAS.block_scale);
               *flag_set = *flag_set + GSBlockScaleSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("linetype"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;
               gsc_strcpy(FAS.line, l_prop->resval.rstring, MAX_LEN_LINETYPENAME);
               *flag_set = *flag_set + GSLineTypeSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("width"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;
               gsc_rb2Dbl(l_prop, &FAS.width);
               *flag_set = *flag_set + GSWidthSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("color"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;

               FAS.color.setResbuf(l_prop);
               *flag_set = *flag_set + GSColorSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("height"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;
               gsc_rb2Dbl(l_prop, &FAS.h_text);
               *flag_set = *flag_set + GSTextHeightSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("elevation"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;
               gsc_rb2Dbl(l_prop, &FAS.elevation);
               *flag_set = *flag_set + GSElevationSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("thickness"), GS_BAD) == 0)
            {
               l_prop = l_prop->rbnext;
               gsc_rb2Dbl(l_prop, &FAS.thickness);
               *flag_set = *flag_set + GSThicknessSetting;
               break;
            }
            else
            if (gsc_strcmp(l_prop->resval.rstring, _T("hatch"), GS_BAD) == 0)
            {
               for (int i = 0; i < 3; i++) l_prop = l_prop->rbnext;
               while (l_prop)
               {
                  if (gsc_strcmp(l_prop->resval.rstring, _T("Pattern"), GS_BAD) == 0)  
                  {
                     l_prop = l_prop->rbnext; // DOT
                     l_prop = l_prop->rbnext;
                     gsc_strcpy(FAS.hatch, l_prop->resval.rstring, MAX_LEN_HATCHNAME);
                     *flag_set = *flag_set + GSHatchNameSetting;
                  }
                  else
                  if (gsc_strcmp(l_prop->resval.rstring, _T("layer"), GS_BAD) == 0)
                  {
                     l_prop = l_prop->rbnext; // DOT
                     l_prop = l_prop->rbnext;
                     gsc_strcpy(FAS.hatch_layer, l_prop->resval.rstring, MAX_LEN_LAYERNAME);
                     *flag_set = *flag_set + GSHatchLayerSetting;
                     break;
                  }
                  else
                  if (gsc_strcmp(l_prop->resval.rstring, _T("color"), GS_BAD) == 0)
                  {
                     l_prop = l_prop->rbnext; // DOT
                     l_prop = l_prop->rbnext;

                     FAS.hatch_color.setResbuf(l_prop);
                     *flag_set = *flag_set + GSHatchColorSetting;
                     break;
                  }
                  else
                  if (gsc_strcmp(l_prop->resval.rstring, _T("scale"), GS_BAD) == 0)
                  {
                     l_prop = l_prop->rbnext; // DOT
                     l_prop = l_prop->rbnext;
                     gsc_rb2Dbl(l_prop, &FAS.hatch_scale);
                     *flag_set = *flag_set + GSHatchScaleSetting;
                     break;
                  }
                  else
                  if (gsc_strcmp(l_prop->resval.rstring, _T("rotation"), GS_BAD) == 0)
                  {
                     l_prop = l_prop->rbnext; // DOT
                     l_prop = l_prop->rbnext;
                     gsc_rb2Dbl(l_prop, &FAS.hatch_rotation);
                     *flag_set = *flag_set + GSHatchRotationSetting;
                     break;
                  }

                  for (int i = 0; i < 3; i++) l_prop = l_prop->rbnext;
               }
            }
            break;

         case RTLB: case RTLE: case RTDOTE: case RTSHORT: case RTREAL: break;
      } 
      acutRelRb(AlterProp);
      l_def = l_def->rbnext;
   }
   acutRelRb(l_def);

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_ClsSQLQryDel                                   <external> */
/*+
  La funzione cancella una condizione SQL da file.
  Parametri:
  int cls;               codice classe
  int sub;               codice sotto-classe
  C_STRING &File;        Path del file (default = NULL); se nullo viene
                         utilizzato "GEOSIM.INI" nella sessione corrente.
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gs_ClsSQLQryDel(void)
{
   presbuf  arg;
   int      cls, sub = 0;

   acedRetNil();

   if (!(arg = acedGetArgs())) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_rb2Int(arg, &cls) != GS_GOOD) // codice classe
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((arg = arg->rbnext))
   {
      if (gsc_rb2Int(arg, &sub) == GS_BAD) // codice sottoclasse
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }    
   }
   if (gsc_ClsSQLQryDel(cls, sub) != GS_GOOD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}
int gsc_ClsSQLQryDel(int cls, int sub, C_STRING *File)
{
   C_STRING entry("ExtractSQLCond_Class"), Path;

   entry += cls;
   if (sub > 0)
   {
      entry += _T("-");
      entry += sub;
   }

   if (File) Path = File->get_name();
   else
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      // memorizzo la scelta in GEOSIM.INI
      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   if (gsc_path_exist(Path) == GS_GOOD &&
       gsc_delete_entry(Path.get_name(), GS_INI_LABEL, entry.get_name()) == GS_BAD)
      if (GS_ERR_COD == eGSSezNotFound || GS_ERR_COD == eGSEntryNotFound)
         return GS_GOOD;
      else return GS_BAD;

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_ClsSQLQryLoad                                  <external> */
/*+
  La funzione carica una condizione SQL da file.
  Parametri:
  int cls;               codice classe
  int sub;               codice sotto-classe
  C_STRING &SQL;         Condizione SQL
  C_STRING &File;        Path del file (default = NULL); se nullo viene
                         utilizzato "GEOSIM.INI" (vedi "gsc_setPathInfoToINI")
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_ClsSQLQryLoad(int cls, int sub, C_STRING &SQL, C_STRING *File)
{
   C_STRING entry("ExtractSQLCond_Class"), Path;

   if (File) Path = File->get_name();
   else
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      // memorizzo la scelta in GEOSIM.INI
      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   entry += cls;
   if (sub > 0)
   {
      entry += _T("-");
      entry += sub;
   }

   if (gsc_path_exist(Path) == GS_BAD ||
      gsc_get_profile(Path, GS_INI_LABEL, entry.get_name(), SQL) == GS_BAD)
      return GS_BAD;
   else return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_ClsSQLQrySave                                  <external> */
/*+
  La funzione salva una condizione SQL in un file.
  Parametri:
  int cls;               codice classe
  int sub;               codice sotto-classe
  C_STRING &SQL;         Condizione SQL
  C_STRING &File;        Path del file (default = NULL); se nullo viene
                         utilizzato "GEOSIM.INI" (vedi "gsc_setPathInfoToINI")
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gs_ClsSQLQrySave(void)
{
   presbuf  arg;
   int      cls, sub = 0;
   C_STRING SQL;

   acedRetNil();

   if (!(arg = acedGetArgs())) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_rb2Int(arg, &cls) != GS_GOOD) // codice classe
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!(arg = arg->rbnext)) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype != RTSTR)
   {
      if (gsc_rb2Int(arg, &sub) == GS_BAD) // codice sottoclasse
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }    
      if (!(arg = arg->rbnext)) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   }
   SQL = arg->resval.rstring; // condizione SQL
   if (gsc_ClsSQLQrySave(cls, sub, SQL) != GS_GOOD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}
int gsc_ClsSQLQrySave(int cls, int sub, C_STRING &SQL, C_STRING *File)
{
   C_STRING entry("ExtractSQLCond_Class"), Path;

   if (File) Path = File->get_name();
   else
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      // memorizzo la scelta in GEOSIM.INI
      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   entry += cls;
   if (sub > 0)
   {
      entry += _T("-");
      entry += sub;
   }

   return gsc_set_profile(Path.get_name(), GS_INI_LABEL, entry.get_name(), SQL.get_name());
}


/*********************************************************************/
/*.doc gsc_ClsAlterPropQryDel                             <external> */
/*+
  La funzione cancella una lista di alterazione di proprietà grafiche da file.
  Parametri:
  int cls;                    codice classe
  C_STRING &File;             Path del file (default = NULL); se nullo viene
                              utilizzato "GEOSIM.INI" nella sessione corrente.
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_ClsAlterPropQryDel(int cls, int sub, C_STRING *File)
{
   C_STRING entry(_T("ExtractAlterProp_Class")), Path;

   entry += cls;

   if (File) Path = File->get_name();
   else
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      // memorizzo la scelta in GEOSIM.INI
      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   if (gsc_path_exist(Path) == GS_GOOD &&
       gsc_delete_entry(Path.get_name(), GS_INI_LABEL, entry.get_name()) == GS_BAD)
      if (GS_ERR_COD == eGSSezNotFound || GS_ERR_COD == eGSEntryNotFound)
         return GS_GOOD;
      else return GS_BAD;

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_ClsAlterPropQryLoad                            <external> */
/*+
  La funzione carica una lista di alterazione di proprietà grafiche da file.
  Parametri:
  int      cls;       Codice classe
  int      sub;       Codice sottoclasse
  C_FAS    &fas;      Lista delle proprietà
  long     *flag_set; Flag a bit per sapere quali proprietà gestire nella FAS
  C_STRING &File;     Path del file (default = NULL); se nullo viene
                      utilizzato "GEOSIM.INI" nella sessione corrente.
  
  La funzione restituisce GS_GOOD se sono impostate delle alterazioni grafiche altrimenti GS_BAD.
-*/  
/*********************************************************************/
int gsc_ClsAlterPropQryLoad(int cls, int sub, C_FAS &fas, long *flag_set, C_STRING *File)
{
   C_STRING sez(_T("ExtractAlterProp_Class")), Path;
   C_STRING Buffer;

   sez += cls;
   sez += _T("_");
   sez += sub;

   if (File) Path = File->get_name();
   else
   {
      if (!GS_CURRENT_WRK_SESSION) return GS_BAD;
      // memorizzo la scelta in GEOSIM.INI
      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;
   }
  
   if (gsc_path_exist(Path) == GS_BAD) return GS_BAD;
   
   if (fas.load(Path, sez.get_name()) == GS_BAD) return GS_BAD;
  
   if (gsc_get_profile(Path, sez.get_name(), _T("Flag"), Buffer) == GS_BAD)
      return GS_BAD;
   *flag_set = Buffer.tol();
   
   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_ClsAlterPropQrySave                            <external> */
/*+
  La funzione salva una lista di alterazione di proprietà grafiche in un file.
  Parametri:
  int      cls;      Codice classe
  int      sub;      Codice sottoclasse
  C_FAS    &fas;     Lista delle proprietà
  long     flag_set; Flag a bit per sapere quali proprietà gestire nella FAS
  C_STRING &File;    Path del file (default = NULL); se nullo viene
                     utilizzato "GEOSIM.INI" nella sessione corrente.
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_ClsAlterPropQrySave(int cls, int sub, C_FAS &fas, long flag_set, C_STRING *File)
{
   C_STRING sez(_T("ExtractAlterProp_Class")), Path;

   sez += cls;
   sez += _T("_");
   sez += sub;

   if (File) Path = *File;
   else
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      // memorizzo la scelta in GEOSIM.INI
      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   if (fas.ToFile(Path, sez.get_name()) == GS_BAD) return GS_BAD;
   
   return gsc_set_profile(Path, sez.get_name(), _T("Flag"), flag_set);
}


/*********************************************************/
/*.doc gsc_ClsODQryDefine                     <external> */
/*+
  Questa funzione definisce una query per estrarre gli oggetti identificati 
  dalla lista C_STR_LIST passata come parametro (utilizza il codice dell'entità).
  Vengono definite un massimo di MAX_OD_COND condizioni partendo da
  la identificatore contenuto dal cursore della lista "KeyList".
  Se al termine della funzione il cursore è nullo vuol dire che non ci sono
  altri identificatori da definire, altrimenti si dovrà richiamare questa 
  funzione per definire i restanti identificatori.
  Parametri:
  int cls;              Codice classe
  int sub;              Codice sotto-classe
  C_STR_LIST &KeyList;  Lista dei codici chiave degli oggetti da estrarre
  const TCHAR *JoinOp;   Operatore (default = "and")

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ClsODQryDefine(int cls, int sub, C_STR_LIST &KeyList, const TCHAR *JoinOp)
{
   C_STR      *pKey;
	C_STRING   ODTblFldName, JOp, bgGroups, endGroups;
   bool       First = TRUE;
   C_RB_LIST  QryCond;
   C_DWG_LIST DwgList;
   int        i = 0, MAX_OD_COND = 1000;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (KeyList.get_count() == 0) return GS_GOOD;
   
   // inizializzo e attivo la lista dei dwg della classe
   if (DwgList.load(GS_CURRENT_WRK_SESSION->get_PrjId(), cls) == GS_BAD ||
       DwgList.activate() == GS_BAD)
      return GS_BAD;

   gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), cls, sub, ODTblFldName);
   ODTblFldName += _T(".ID");

   pKey = (C_STR *) KeyList.get_cursor();
	// scorro la lista delle classi e degli oggetti che voglio estarre 
	while (pKey && (i++ < MAX_OD_COND))
	{
      // Costruisco la condition query da passare ad ade_qrydefine()
      QryCond << acutBuildList(RTLB,
                               RTSTR, _T("objdata"),
                               RTSTR, ODTblFldName.get_name(),
                               RTSTR, _T("="), RTSTR, pKey->get_name(),
                               RTLE, 0);
      pKey = (C_STR *) KeyList.get_next();

      if (First) // se prima condizione
      {
         JOp      = JoinOp;
         bgGroups = _T("("); 
         First    = FALSE;
      }
      else 
      {
         JOp      = _T("or");
         bgGroups = GS_EMPTYSTR;
      }

      endGroups = (!pKey || i >= MAX_OD_COND) ? _T(")"): GS_EMPTYSTR; // ultima condizione

      // Imposto la condizione di estrazione SQL/DATA
      if (ade_qrydefine(JOp.get_name(),
                        bgGroups.get_name(),
                        GS_EMPTYSTR, _T("Data"), QryCond.get_head(), 
                        endGroups.get_name()) == ADE_NULLID)
         { GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
	}

	return GS_GOOD;
}
