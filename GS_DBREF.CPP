/**********************************************************
Name: GS_DBREF

Module description: File contenente le funzioni per la gestione dei
                    riferimenti alle tabelle interne in Geosim.
            
Author: Poltini Roberto

(c) Copyright 1995-2015 by IREN ACQUA GAS SpA

Modification history:
              
Notes and restrictions on use: 
**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include "adeads.h"
#include <MapOdColumn.h>
#include <MapArxApi.h>
#include <MapODDefinition.h>
#include <MapStringArray.h>
#include "MapODRecord.h"

#include "gs_init.h"
#include "gs_utily.h"
#include "gs_resbf.h"     // (gsc_rbsearch)
#include "gs_error.h"     // codici errori
#include "gs_dbref.h"     // prototipi funzioni gestione riferimenti a db
#include "gs_class.h"
#include "gs_ade.h"       // per gestire errori ade
#include "gs_graph.h"     // per gsc_set_cmddia
#include "gs_area.h"      // per GS_CURRENT_WRK_SESSION


/*********************************************************/
/* FUNCTIONS */
/*********************************************************/


///////////////////////////////////////////////////////////////////////////////
// Object Data Table Data - INIZIO
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_getODTableName <internal> /*
/*+
  Questa funzione, dato un codice di progetto e di classe restituisce il nome della
  tabella interna.
  Parametri:
  int prj;                codice progetto
  int cls;                codice classe
  int sub;                codice sotto-classe
  C_STRING &ODTableName;  nome tabella (output)
  
-*/  
/*********************************************************/
void gsc_getODTableName(int prj, int cls, int sub, C_STRING &ODTableName)
{
   ODTableName = _T("PRJ");
   ODTableName += prj;
   ODTableName += _T("CLS");
   ODTableName += cls;
   if (sub > 0)
   {
      ODTableName += _T("SUB");
      ODTableName += sub;
   }
}


/*********************************************************/
/*.doc gsc_getODTable                         <external> /*
/*+
  Questa funzione, dato un codice di progetto e di classe restituisce la
  tabella interna dati oggetto che deve già esistere.
  Parametri:
  int prj;               codice progetto
  int cls;               codice classe
  int sub;               codice sotto-classe
  
  Ritorna il puntatore a una AcMapODTable (tabella OD) (da rilasciare a cura
  del chiamante) in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
AcMapODTable *gsc_getODTable(int prj, int cls, int sub)
{
   C_STRING ODTableName;
   
   gsc_getODTableName(prj, cls, sub, ODTableName);

   return gsc_getODTable(ODTableName);
}
AcMapODTable *gsc_getODTable(C_STRING &ODTableName)
{
   AcMapSession     *mapApi  = NULL;
   AcMapProject     *pProj   = NULL;
   AcMapODContainer *pODCont = NULL;
   AcMapODTable     *pTable = NULL;

   if (!(mapApi = AcMapGetSession())) return NULL;
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return NULL;
   if (pProj->GetODContainer(pODCont) == Adesk::kFalse) return NULL;
   if (pODCont->GetODTable(pTable, ODTableName.get_name()) != Acad::eOk) return NULL;

   return pTable;
}


/*********************************************************/
/*.doc gsc_setODTable                         <external> /*
/*+
  Questa funzione, dato un codice di progetto e di classe restituisce il nome della
  tabella interna associata creandola se non esiste.
  Parametri:
  int prj;                 codice progetto
  int cls;                 codice classe
  int sub;                 codice sotto-classe
  C_STRING *pODTableName;  nome tabella (output) (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_setODTable(C_STRING &ODTableName)
{
   // Poichè la funzione ade_odtablelist legge anche in tutti i disegni attaccati
   // questo genera problemi in estrazioni concorrenti
   // la riga è commentata, provo comunque a creare la tabella intrappolando
   // un enventuale errore se la tabella già esistesse.

   // leggo la lista delle tabelle interne già esistenti nella sessione di lavoro
   //tablename_list << ade_odtablelist();
   //if (gsc_rbsearch(ODTableName.get_name(), tablename_list.get_head()) == NULL)

   // verifico se la tabella è già esistente
   if (GEOsimAppl::OD_TABLENAME_LIST.search_name(ODTableName.get_name(), FALSE) == NULL)
   {
      C_RB_LIST tabledef;

      if ((tabledef << acutBuildList(RTLB,
                                       RTLB,
                                          RTSTR, _T("tablename"),
                                          RTSTR, ODTableName.get_name(),
                                       RTDOTE,
                                       RTLB,
                                          RTSTR, _T("tabledesc"),
                                          RTSTR, GS_EMPTYSTR,
                                       RTDOTE,
                                       RTLB,
                                          RTSTR, _T("columns"),
                                          RTLB,
                                             RTLB,
                                                RTSTR, _T("colname"),
                                                RTSTR, _T("ID"),
                                             RTDOTE,
                                             RTLB,
                                                RTSTR, _T("coldesc"),
                                                RTSTR, GS_EMPTYSTR,
                                             RTDOTE,
                                             RTLB,
                                                RTSTR, _T("coltype"),
                                                RTSTR, _T("character"),
                                             RTDOTE,
                                             RTLB,
                                                RTSTR, _T("defaultval"),
                                                RTSTR, GS_EMPTYSTR,
                                             RTDOTE,
                                          RTLE,
                                       RTLE,
                                    RTLE, 0)) == NULL)
         return GS_BAD;
      ade_errclear();
      if (ade_oddefinetab(tabledef.get_head()) != RTNORM)
      {
         int AdeErr = ade_errcode(0);
         
         // 1312 = kAdeIrdDuplicateTableName
         // Object Data Table with specified name already exists. 
         // ADEDEFDATA - all Object Data table names must be unique. 
         // Duplicate names are not allowed.
         // Se già esiste non è un problema
         if (AdeErr != 1312)
            { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
      }

      // Aggiungo la tabelle definita in GEOsimAppl::OD_TABLENAME_LIST
      C_STR *p = new C_STR(ODTableName.get_name());
      GEOsimAppl::OD_TABLENAME_LIST.add_tail(p);
   }   
   return GS_GOOD;
}
int gsc_setODTable(int prj, int cls, int sub, C_STRING *pODTableName)
{
   C_RB_LIST tablename_list;
   C_STRING  ODTableName;

   // ricavo il nome della tabella interna 
   gsc_getODTableName(prj, cls, sub, ODTableName);

   if (gsc_setODTable(ODTableName) == GS_BAD) return GS_BAD;
   
   if (pODTableName) pODTableName->set_name(ODTableName.get_name());

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_GetObjectODRecordIterator          <external> /*
/*+
  Questa funzione, prepara una struttura che server per velocizzare 
  le funzioni "gsc_setID2ODTable", "gsc_delID2ODTable",
  "gsc_getIDfromODTable".
  Dopo aver ottenuto il contenitore degli object data per il progetto
  MAP della sessione di MAP corrente, ricava il puntatore a un
  AcMapODRecordIterator.
  Parametri:
  AcMapODRecordIterator *&pIter;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria da rilasciare con "delete"
-*/  
/*********************************************************/
int gsc_GetObjectODRecordIterator(AcMapODRecordIterator *&pIter)
{
   AcMapSession     *mapApi;
   AcMapProject     *pProj;
   AcMapODContainer *pODCont;

   if (!(mapApi = AcMapGetSession())) return GS_BAD;
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return GS_BAD;
   if (pProj->GetODContainer(pODCont) == Adesk::kFalse) return GS_BAD;
   if (pODCont->GetObjectODRecordIterator(pIter) != AcMap::kOk) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_setID2ODTable                      <internal> /*
/*+
  Questa funzione, è di supporto usata alle "gsc_setID2ODTable".
  Parametri:
  ads_name ent;               entità a cui assegnare l'identificatore
  C_STRING &ODTableName;      nome tabella 
  C_STRING &ID;               identificatore
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_setID2ODTable(AcDbObject *pObj, AcMapODTable *pTable, C_STRING &ID,
                      AcMapODRecordIterator *pIter)
{
   AcMapODTableRecord Record;
   AcMapValue         Value = ID.get_name(); 

   // leggo l'identificatore dell'entità
   if (pIter->Init(pObj, AcMap::kOpenForWrite, Adesk::kTrue) != AcMap::kOk)
      { gsc_printAdeErr(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   for (; pIter->IsDone() == Adesk::kFalse; pIter->Next())
      if (pIter->GetRecord(Record) == AcMap::kOk && 
          gsc_strcmp(pTable->Name(), Record.ODTableName()) == 0)
      {
         // cambio identificatore nell'entità
         Record.Value(0) = Value; 
         if (pIter->UpdateRecord(Record) != AcMap::kOk)
            { gsc_printAdeErr(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

         return GS_GOOD;
      }

   // se non ha l'identificatore aggiungo un record all'entità
   pTable->InitRecord(Record);
   Record.Value(0) = Value; 
   if (pTable->AddRecord(Record, pObj) != AcMap::kOk)
      { gsc_printAdeErr(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   return GS_GOOD;
}
int gsc_setID2ODTable(ads_name ent, C_STRING &ODTableName, C_STRING &ID)
{
   AcDbObjectId          objId;
   AcDbObject            *pObj;
   AcMapODTable          *pTable = NULL; 
   AcMapODRecordIterator *pIter  = NULL;

   if ((pTable = gsc_getODTable(ODTableName)) == NULL) return GS_BAD;
   if (gsc_GetObjectODRecordIterator(pIter) == GS_BAD) 
      { delete pTable; return GS_BAD; }

   if (acdbGetObjectId(objId, ent) != Acad::eOk)
      { delete pIter; delete pTable; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite, true) != Acad::eOk) 
      { delete pIter; delete pTable; GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_setID2ODTable(pObj, pTable, ID, pIter) == GS_BAD)
      { pObj->close(); delete pIter; delete pTable; return GS_BAD; }
   
   pObj->close();
   delete pIter;
   delete pTable;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_setID2ODTable <external> /*
/*+
  Questa funzione, usata per gli oggetti grafici di classi senza collegamento 
  a DB, associa un identificatore all'entità <ent>. Se l'entità ha
  gia un record della tabella interna <tablename> allora viene settato il valore
  del campo con il nuovo identificatore, altrimenti viene creato un nuovo record
  e settato l'identificatore. L'identificatore è composto 
  dall'handle (nel dwg di provenienza) + "-" +  nome del dwg di provenienza.
  N.B. l'oggetto deve essere già stato salvato nel dwg.
  Parametri:
  ads_name ent;                  entità a cui assegnare l'identificatore
  C_STRING &ODTableName;         nome tabella 
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_setID2ODTable(ads_name ent, C_STRING &ODTableName)
{
   C_RB_LIST pResult;
   ade_id    dwg_id;
   C_STRING  identifier_str, name;

   // leggo handle nel disegno originale 
   if ((pResult << gsc_ade_qrygetdwgandhandle(ent)) == NULL)
      { GS_ERR_COD = eGSCannotSaveEntIntoDwg; return GS_BAD; }
   // memorizzo Id del dwg
   dwg_id = (ade_id) pResult.get_head()->resval.rreal;
   // memorizzo Handle
   identifier_str = pResult.getptr_at(2)->resval.rstring;
   identifier_str += _T("-");
   // memorizzo nome del file
   if ((pResult << ade_dwggetsetting(dwg_id, _T("DWGNAME"))) == NULL)
      { GS_ERR_COD = eGSCannotLoadDwgList; return GS_BAD; }
   gsc_splitpath(pResult.get_head()->resval.rstring, NULL, NULL, &name);
   identifier_str += name;

   return gsc_setID2ODTable(ent, ODTableName, identifier_str);
}


/*********************************************************/
/*.doc gsc_setID2ODTable <external> /*
/*+
  Questa funzione, usata per gli oggetti grafici di classi con collegamento 
  a DB, inserisce un identificatore numerico all'entità <ent>.
  Se l'entità ha già un record della tabella interna <tablename> allora 
  viene settato il valore del campo con il nuovo identificatore, 
  altrimenti viene creato un nuovo record e settato l'identificatore.
  Parametri:
  ads_name ent;                  entità a cui assegnare l'identificatore
  C_STRING &ODTableName;         nome tabella 
  long     ID;                   Codice identificativo dell'entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_setID2ODTable(const TCHAR *handle, C_STRING &ODTableName, long ID)
{
   ads_name ent;

   if (acdbHandEnt(handle, ent) != RTNORM) return GS_BAD;

   return gsc_setID2ODTable(ent, ODTableName, ID);
}
int gsc_setID2ODTable(ads_name ent, C_STRING &ODTableName, long ID)
{
   C_STRING identifier_str;

   identifier_str = ID;

   return gsc_setID2ODTable(ent, ODTableName, identifier_str);
}


/*********************************************************/
/*.doc gsc_delID2ODTable <external> /*
/*+
  Questa funzione, cancella un identificatore all'entità <ent>.
  Parametri:
  ads_name ent;           entità a cui assegnare l'identificatore
  C_STRING &ODTableName;  nome tabella 
  AcMapODRecordIterator *pIter;  Struttura opzionale per velocizzare la
                                 funzione (input, default = NULL). Vedi
                                 "gsc_GetObjectODRecordIterator"
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_delID2ODTable(AcDbObjectId &eId, C_STRING &ODTableName,
                      AcMapODRecordIterator *pIter)
{
   AcMapODTableRecord    record;
   AcMapODRecordIterator *pInternalIter;

   // leggo l'identificatore dell'entità
   if (pIter) pInternalIter = pIter;
   else
      if (gsc_GetObjectODRecordIterator(pInternalIter) == GS_BAD) return GS_BAD;

   if (pInternalIter->Init(eId, AcMap::kOpenForWrite, Adesk::kTrue) != AcMap::kOk)
   {
      if (!pIter) delete pInternalIter;
      GS_ERR_COD = eGSInvGraphObjct;
      return GS_BAD;
   }

   for (; pInternalIter->IsDone() == Adesk::kFalse; pInternalIter->Next())
      if (pInternalIter->GetRecord(record) == AcMap::kOk && 
          ODTableName.comp(record.ODTableName()) == 0)
      {           
         if (pInternalIter->RemoveRecord() != AcMap::kOk) 
            { if (!pIter) delete pInternalIter; return GS_BAD; }
         break;
      }

   if (!pIter) delete pInternalIter;

   return GS_GOOD;
}
int gsc_delID2ODTable(ads_name ent, C_STRING &ODTableName,
                      AcMapODRecordIterator *pIter)
{
   AcDbObjectId eId;

   if (acdbGetObjectId(eId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return gsc_delID2ODTable(eId, ODTableName, pIter);
}


/*********************************************************/
/*.doc gsc_getIDfromODTable                   <external> /*
/*+
  Questa funzione, usata per gli oggetti grafici di classi senza collegamento 
  a DB, legge l'identificatore dell'entità <ent> da OD. L'identificatore è
  composto dall'handle (nel dwg di provenienza) + "-" +  nome del dwg di provenienza.
  N.B. l'oggetto deve essere già stato salvato nel dwg.
  Parametri:
  ads_name ent;                  oggetto grafico
  C_STRING &ODTableName;         nome tabella (input)
  C_STRING &ID;                  valore ID (output)
  AcMapODRecordIterator *pIter;  Struttura opzionale per velocizzare la
                                 funzione (input, default = NULL). Vedi
                                 "gsc_GetObjectODRecordIterator"
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getIDfromODTable(ads_name ent, C_STRING &ODTableName, C_STRING &ID,
                         AcMapODRecordIterator *pIter)
{
   AcDbObjectId           eId;
   AcMapODTableRecord    record;
   AcMapODRecordIterator *pInternalIter;

   ID.clear();

   if (acdbGetObjectId(eId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   // leggo l'identificatore dell'entità
   if (pIter) pInternalIter = pIter;
   else
      if (gsc_GetObjectODRecordIterator(pInternalIter) == GS_BAD) return GS_BAD;

   if (pInternalIter->Init(eId, AcMap::kOpenForRead, Adesk::kTrue) != AcMap::kOk)
   {
      if (!pIter) delete pInternalIter;
      GS_ERR_COD = eGSInvGraphObjct;
      return GS_BAD;
   }

   for (; pInternalIter->IsDone() == Adesk::kFalse; pInternalIter->Next())
      if (pInternalIter->GetRecord(record) == AcMap::kOk && 
          ODTableName.comp(record.ODTableName()) == 0)
      {           
         ID = (const TCHAR *) record.Value(0);
         break;
      }

   if (!pIter) delete pInternalIter;

   if (ID.get_name() == NULL)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   else
      return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getIDfromODTable <external> /*
/*+
  Questa funzione, usata per gli oggetti grafici di classi con collegamento 
  a DB, legge l'identificatore numerico dell'entità <ent> da OD.
  Parametri:
  ads_name ent;                  oggetto grafico
  C_STRING &ODTableName;         nome tabella (input)
  long     *ID;                  valore ID (output)
  AcMapODRecordIterator *pIter;  Struttura opzionale per velocizzare la
                                 funzione (input, default = NULL). Vedi
                                 "gsc_GetObjectODRecordIterator"
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getIDfromODTable(ads_name ent, C_STRING &ODTableName, long *ID,
                         AcMapODRecordIterator *pIter)
{
   AcDbObjectId           eId;
   AcMapODTableRecord    record;
   AcMapODRecordIterator *pInternalIter;
   int                   res = GS_BAD;

   if (acdbGetObjectId(eId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   // leggo l'identificatore dell'entità
   if (pIter) pInternalIter = pIter;
   else
      if (gsc_GetObjectODRecordIterator(pInternalIter) == GS_BAD) return GS_BAD;

   if (pInternalIter->Init(eId, AcMap::kOpenForRead, Adesk::kTrue) != AcMap::kOk)
   {
      if (!pIter) delete pInternalIter;
      GS_ERR_COD = eGSInvGraphObjct;
      return GS_BAD;
   }

   for (; pInternalIter->IsDone() == Adesk::kFalse; pInternalIter->Next())
      if (pInternalIter->GetRecord(record) == AcMap::kOk && 
          ODTableName.comp(record.ODTableName()) == 0)
      {           
         *ID = _wtol((const ACHAR *) record.Value(0));
         res = GS_GOOD;
         break;
      }

   if (!pIter) delete pInternalIter;
   if (res == GS_BAD) GS_ERR_COD = eGSInvalidKey;

   return res;
}


/*********************************************************/
/*.doc gsc_renODTable                         <external> /*
/*+
  Questa funzione, rinomina la tabella OD utilizzata per un oggetto grafico.
  Parametri:
  ads_name ent;                  oggetto grafico
  C_STRING &OldODTableName;      nome vecchio
  C_STRING &NewODTableName;      nome nuovo
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_renODTable(ads_name ent, C_STRING &OldODTableName, C_STRING &NewODTableName)
{
   C_STRING ID;

   if (gsc_getIDfromODTable(ent, OldODTableName, ID) == GS_BAD) return GS_BAD;
   if (gsc_delID2ODTable(ent, OldODTableName) == GS_BAD) return GS_BAD;
   return gsc_setID2ODTable(ent, NewODTableName, ID);
}


/*********************************************************/
/*.doc gsc_getODTables                        <external> /*
/*+
  Questa funzione legge quali sono le tabelle OD collegate 
  ad un gruppo di selezione o ad un singolo oggetto grafico.
  Parametri:
  C_SELSET &SelSet;              Gruppo di selezione
  C_STR_LIST &ODTables;          Lista nomi tabelle OD (input)

  oppure

  ads_name ent;                  oggetto grafico
  C_STR_LIST &ODTables;          Lista nomi tabelle OD (input)
  AcMapODRecordIterator *pIter;  Struttura opzionale per velocizzare la
                                 funzione (input, default = NULL). Vedi
                                 "gsc_GetObjectODRecordIterator"
  bool IncludeGEOsimOD;          Opzionale (default false), per includere
                                 o scartare le tabelle OD gestite da GEOsim
                                 (quelle il cui nome inizia per 
                                 "PRJ" + <progetto corrente> + "CLS"
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_getODTablesfromSS(void)
{
   presbuf    arg = NULL;
   C_SELSET   ss;
   C_STR_LIST ODList;
   bool       IncludeGEOsimOD = false;

   acedRetNil();
   arg = acedGetArgs();
   
   // Gruppo di selezione da esaminare 
   if (!arg || arg->restype != RTPICKS)  
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ss << arg->resval.rlname;
   ss.ReleaseAllAtDistruction(GS_BAD);
   // secondo parametro opzionale
   if ((arg = arg->rbnext) && arg->restype == RTT) IncludeGEOsimOD = true;

   if (gsc_getODTables(ss, ODList, IncludeGEOsimOD, GS_GOOD) == GS_BAD) return RTERROR;
   
   if (ODList.get_count() > 0)
   {
      C_RB_LIST ret;

      // Ordino la lista da restituire
      ODList.sort_name();
      // Restituisco la lista
      if ((ret << ODList.to_rb()) == NULL) return RTERROR;
      ret.LspRetList();
   }

   return RTNORM;
}
int gsc_getODTables(C_SELSET &SelSet, C_STR_LIST &ODTables, bool IncludeGEOsimOD, int CounterToVideo)
{
   AcMapODRecordIterator *pIter;
   ads_name              ent;
   long                  i = 0;
   C_STR_LIST            EntODTables;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1087)); // "Lettura collegamenti a dati oggetto"

   ODTables.remove_all();
   if (gsc_GetObjectODRecordIterator(pIter) == GS_BAD) return GS_BAD;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(SelSet.length());

   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      if (gsc_getODTables(ent, EntODTables, pIter, IncludeGEOsimOD) == GS_GOOD)
         ODTables.add_tail_unique(EntODTables);
   }
   delete pIter;
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return GS_GOOD;
}
int gsc_getODTables(ads_name ent, C_STR_LIST &ODTables,
                    AcMapODRecordIterator *pIter, bool IncludeGEOsimOD)
{
   AcDbObjectId eId;

   if (acdbGetObjectId(eId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   return gsc_getODTables(eId, ODTables, pIter, IncludeGEOsimOD);
}
int gsc_getODTables(AcDbObjectId &eId, C_STR_LIST &ODTables,
                    AcMapODRecordIterator *pIter, bool IncludeGEOsimOD)
{
   AcMapODTableRecord    record;
   AcMapODRecordIterator *pInternalIter;
   AcMap::EErrCode       err;
   C_STRING              GSPrefixTableName, ODTableName;

   ODTables.remove_all();

   // leggo l'identificatore dell'entità
   if (pIter) pInternalIter = pIter;
   else
      if (gsc_GetObjectODRecordIterator(pInternalIter) == GS_BAD) return GS_BAD;

   if (pInternalIter->Init(eId, AcMap::kOpenForRead, Adesk::kTrue) != AcMap::kOk)
   {
      if (!pIter) delete pInternalIter;
      GS_ERR_COD = eGSInvGraphObjct;
      return GS_BAD;
   }

   if (!IncludeGEOsimOD && GS_CURRENT_WRK_SESSION)
   {
      GSPrefixTableName = _T("PRJ");
      GSPrefixTableName += GS_CURRENT_WRK_SESSION->get_PrjId();
      GSPrefixTableName += _T("CLS");
   }

   for (; pInternalIter->IsDone() == Adesk::kFalse; pInternalIter->Next())
   {
      // Se esiste una tabella OD e non è ancora stata registrata nella lista
      if ((err = pInternalIter->GetRecord(record)) == AcMap::kOk)
      {
         ODTableName = record.ODTableName();
         // Se devo fare il controllo sul nome della tabella OD
         if (GSPrefixTableName.get_name())
            // scarto quelle che iniziano per GSPrefixTableName
            if (ODTableName.at(GSPrefixTableName.get_name()) == ODTableName.get_name())
               continue;

         ODTables.add_tail_unique(ODTableName.get_name());
      }
   }
   if (!pIter) delete pInternalIter;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getODTables                        <external> /*
/*+
  Questa funzione legge quali sono le tabelle OD nella sessione corrente.
  Parametri:
  C_STR_LIST &ODTables;          Lista nomi tabelle OD (input)
  bool IncludeGEOsimOD;          Opzionale (default false), per includere
                                 o scartare le tabelle OD gestite da GEOsim
                                 (quelle il cui nome inizia per 
                                 "PRJ" + <progetto corrente> + "CLS"
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_getODTables(void)
{
   presbuf    arg = NULL;
   C_STR_LIST ODList;
   bool       IncludeGEOsimOD = false;

   acedRetNil();
   arg = acedGetArgs();
   
   // Flag che indica se includere o meno le tabelle OD di GEOsim
   if (arg && arg->restype == RTT) IncludeGEOsimOD = true;

   if (gsc_getODTables(ODList, IncludeGEOsimOD) == GS_BAD) return RTERROR;
   
   if (ODList.get_count() > 0)
   {
      C_RB_LIST ret;

      // Ordino la lista da restituire
      ODList.sort_name();
      // Restituisco la lista
      if ((ret << ODList.to_rb()) == NULL) return RTERROR;
      ret.LspRetList();
   }

   return RTNORM;
}
int gsc_getODTables(C_STR_LIST &ODTables, bool IncludeGEOsimOD)
{
   AcMapSession     *mapApi;
   AcMapProject     *pProj;
   AcMapODContainer *pODCont;
   AcMapStringArray TableNames;
   C_STR            *pODTable;
   C_STRING         GSPrefixTableName, ODTableName;

   if (!(mapApi = AcMapGetSession())) return NULL;
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return NULL;
   if (pProj->GetODContainer(pODCont) == Adesk::kFalse) return NULL;
   if (pODCont->GetODTableNames(TableNames) != Acad::eOk) return NULL;

   if (!IncludeGEOsimOD && GS_CURRENT_WRK_SESSION)
   {
      GSPrefixTableName = _T("PRJ");
      GSPrefixTableName += GS_CURRENT_WRK_SESSION->get_PrjId();
      GSPrefixTableName += _T("CLS");
   }

   int TableCount = pODCont->CountODTables();
   for (int i = 0; i < TableCount; i++)
   {
      ODTableName = TableNames[i];
      // Se devo fare il controllo sul nome della tabella OD
      if (GSPrefixTableName.get_name())
         // scarto quelle che iniziano per GSPrefixTableName
         if (ODTableName.at(GSPrefixTableName.get_name()) == ODTableName.get_name())
            continue;

      if (!ODTables.search_name(ODTableName.get_name()))
      {
         if ((pODTable = new C_STR(ODTableName.get_name())) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         ODTables.add_tail(pODTable);
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getRbFromODValue                   <external> /*
/*+
  Questa funzione trasforma un valore OD in un resbuf.
  Parametri:
  AcMapValue &ODValue; 
  
  Restituisce un resbuf in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria.
-*/  
/*********************************************************/
presbuf gsc_getRbFromODValue(const AcMapValue &ODValue)
{
   presbuf p = NULL;

   switch (ODValue.Type()) 
   { 
      case AcMap::kCharacter:
         p = acutBuildList(RTSTR, (const TCHAR *) ODValue, 0);
         break;
      case AcMap::kInteger:
         // faccio il cast a long perchè contrariamente a quello che pensavo
         // si possono memorizzare valori > 32000
         p = acutBuildList(RTLONG, (long) ODValue, 0);
         break;
      case AcMap::kReal:
         p = acutBuildList(RTREAL, (double) ODValue, 0);
         break;
      case AcMap::kPoint:
      {
         AcGePoint3d dummyPt((const AcGePoint3d&) ODValue);
         ads_point   pt;

         ads_point_set_from_AcGePoint3d(dummyPt, pt);
         p = acutBuildList(RT3DPOINT, pt, 0);
         break;
      }
      default:
         p = acutBuildList(RTNIL, 0);
   }

   return p;
}


/*********************************************************/
/*.doc gsc_getValuesFromODTable               <external> /*
/*+
  Questa funzione legge i valori dei campi della tabella OD dell'entità <ent>.
  Parametri:
  ads_name ent;                  oggetto grafico
  C_STRING &ODTableName;         nome tabella (input)
  C_RB_LIST &ColValues;          Lista valori ((<nome campo><valore>)...) (out)
  AcMapODRecordIterator *pIter;  Struttura opzionale per velocizzare la
                                 funzione (input, default = NULL). Vedi
                                 "gsc_GetObjectODRecordIterator"
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getValuesFromODTable(ads_name ent, C_STRING &ODTableName, C_RB_LIST &ColValues,
                             AcMapODRecordIterator *pIter)
{
   AcMapSession          *mapApi;
   AcMapProject          *pProj;
   AcMapODContainer      *pODCont;
   AcMapODTable          *pTable = NULL; 
   AcDbObjectId          eId;
   AcMapODTableRecord    record;
   AcMapODRecordIterator *pInternalIter;
   int                   Result = GS_GOOD;

   ColValues.remove_all();
   if (!(mapApi = AcMapGetSession())) return GS_BAD;
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return GS_BAD;
   if (pProj->GetODContainer(pODCont) == Adesk::kFalse) return GS_BAD;
   if (pODCont->GetODTable(pTable, ODTableName.get_name()) != Acad::eOk) return GS_BAD;

   if (acdbGetObjectId(eId, ent) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   // leggo l'identificatore dell'entità
   if (pIter) pInternalIter = pIter;
   else
      if (gsc_GetObjectODRecordIterator(pInternalIter) == GS_BAD) return GS_BAD;

   if (pInternalIter->Init(eId, AcMap::kOpenForRead, Adesk::kTrue) != AcMap::kOk)
   {
      if (!pIter) delete pInternalIter;
      GS_ERR_COD = eGSInvGraphObjct;
      return GS_BAD;
   }

   for (; pInternalIter->IsDone() == Adesk::kFalse; pInternalIter->Next())
      if (pInternalIter->GetRecord(record) == AcMap::kOk && 
          ODTableName.comp(record.ODTableName()) == 0)
      {
         AcMapODTableDefinition  TableDef = pTable->Definition(); 
         AcMapODColumnDefinition Column; 

         ColValues += acutBuildList(RTLB, 0);
         for (int i = 0; i < record.Count(); i++)
         {
            AcMapValue &val = record.Value(i); 

            if (TableDef.GetColumn(Column, i) != AcMap::kOk) 
               { ColValues.remove_all(); Result = GS_BAD; break; }
            ColValues += acutBuildList(RTLB, RTSTR, Column.Name(), 0);
            ColValues += gsc_getRbFromODValue(val);
            ColValues += acutBuildList(RTLE, 0);
         }

         ColValues += acutBuildList(RTLE, 0);        
         break;
      }

   if (!pIter) delete pInternalIter;

   return Result;
}


/*********************************************************/
/*.doc gsc_getStruFromODTable                 <external> */
/*+
  Questa funzione legge la struttura di una tabella dati oggetto 
  nel seguente formato:
  ((<name><type><def><descr>) ...)
  Parametri:
  const TCHAR *ODTableName;      Nome Tabella OD

  Restituisce la struttura della tabella in caso di successo 
  altrimenti restituisce NULL.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
presbuf gsc_getStruFromODTable(const TCHAR *ODTableName)
{
   AcMapSession            *mapApi;
   AcMapProject            *pProj;
   AcMapODContainer        *pODCont;
   AcMapODTable            *pTable = NULL;
   AcMapODColumnDefinition ColumnDef;
   C_RB_LIST	            Stru;
   int                     i = 0;
   C_STRING                SQL_Descr;

   if (!(mapApi = AcMapGetSession())) return NULL;
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return NULL;
   if (pProj->GetODContainer(pODCont) == Adesk::kFalse) return NULL;
   if (pODCont->GetODTable(pTable, ODTableName) != Acad::eOk) return NULL;

   if ((Stru += acutBuildList(RTLB, 0)) == NULL) return NULL;

   for (i = 0; i < pTable->Definition().Count(); i++)
      if (pTable->Definition().GetColumn(ColumnDef, i) == Acad::eOk)
      {
         if ((Stru += acutBuildList(RTLB, 
                                    RTSTR, ColumnDef.Name(),
                                    RTSHORT, ColumnDef.Type(),
                                    0)) == NULL)
            return NULL;
         if ((Stru += gsc_getRbFromODValue(ColumnDef.DefaultValue())) == NULL)
            return NULL;
         if ((Stru += acutBuildList(RTSTR, ColumnDef.Description(), 
                                    RTLE, 0)) == NULL)
            return NULL;
      }

   if ((Stru += acutBuildList(RTLE, 0)) == NULL) return NULL;
   Stru.ReleaseAllAtDistruction(GS_BAD);

   return Stru.get_head();
}



/*********************************************************/
/*.doc gs_RenameODField                       <external> */
/*+
  Questa funzione LISP rinomina un campo di una tabella dati oggetto.
  Parametri:
  (<Table name> <Old field name> <new field name>)
  
  Restituisce T in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_RenameODField(void)
{
   presbuf arg = acedGetArgs();
   TCHAR   *pODTableName, *pOldODFieldName, *pNewODFieldName;

   acedRetNil();
   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   pODTableName = arg->resval.rstring;

   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   pOldODFieldName = arg->resval.rstring;

   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   pNewODFieldName = arg->resval.rstring;

   if (gsc_renameODField(pODTableName, pOldODFieldName, pNewODFieldName) == GS_BAD)
      return RTERROR;

   acedRetT();
   return RTNORM;
}

/*********************************************************/
/*.doc gsc_renameODField                      <external> */
/*+
  Questa funzione cambia il nome di un campo della tabella dati oggetto.
  Parametri:
  const TCHAR *ODTableName;      Nome Tabella OD
  const TCHAR *OldODFieldName;   Nome vecchio campo della tabella OD
  const TCHAR *NewODFieldName;   Nome nuovo campo della tabella OD

  Restituisce lGS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_renameODField(const TCHAR *ODTableName, const TCHAR *OldODFieldName, const TCHAR *NewODFieldName)
{
   AcDbDatabase                 *pDatabase = acdbHostApplicationServices()->workingDatabase();;
   AcMapSession                 *mapApi;
   AcMapProject                 *pProj;
   AcMapODContainer             *pODCont;
   AcMapODTable                 *pTable = NULL;
   AcMapODTableDefinition       TableDef;
   AcMapODColumnDefinition      ColumnDef;
   int                          i = 0, OldFieldPos, NewFieldPos;
   AcMapODRecordIterator        *pODIterator;
   AcMapODTableRecord           ODRecord;
   AcDbBlockTable               *pBlockTable;
   AcDbBlockTableRecord         *pBlockTableRecord;
   AcDbBlockTableRecordIterator *pIterator;
   AcDbObjectId                 objId;

   if (!ODTableName || !OldODFieldName || !NewODFieldName)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (!(mapApi = AcMapGetSession())) return GS_BAD;
   if (mapApi->GetProject(pProj) == Adesk::kFalse) return GS_BAD;
   if (pProj->GetODContainer(pODCont) == Adesk::kFalse) return GS_BAD;
   if (pODCont->GetODTable(pTable, ODTableName) != AcMap::kOk) return GS_BAD;
   TableDef = pTable->Definition();
   // trovo il vecchio campo
   OldFieldPos = TableDef.FindColumn(OldODFieldName);
   if (TableDef.GetColumn(ColumnDef, OldFieldPos) != AcMap::kOk)
      { GS_ERR_COD = eGSInvODFieldName; return GS_BAD; }
   if (ColumnDef.SetName(NewODFieldName) != AcMap::kOk)
      { GS_ERR_COD = eGSInvODFieldName; return GS_BAD; }
   // Inserisco nuovo campo
   NewFieldPos = OldFieldPos + 1;
   if (TableDef.AddColumn(ColumnDef, NewFieldPos) != AcMap::kOk)
      { GS_ERR_COD = eGSInvODFieldName; return GS_BAD; }
   if (pODCont->UpdateODTable(ODTableName, TableDef) != AcMap::kOk) return GS_BAD;

   if (pDatabase->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) return GS_BAD;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForRead) != Acad::eOk)
      { pBlockTable->close(); return GS_BAD; }
   pBlockTable->close();
   if (pBlockTableRecord->newIterator(pIterator) != Acad::eOk) 
      { pBlockTableRecord->close(); return GS_BAD; }

   if (pODCont->GetObjectODRecordIterator(pODIterator) != AcMap::kOk)
      { pBlockTableRecord->close(); return GS_BAD; }

   // Ciclo su tutti gli oggetti
   for (pIterator->start(); !pIterator->done(); pIterator->step())
      if (pIterator->getEntityId(objId) == Acad::eOk)
         if (pODIterator->Init(objId, AcMap::kOpenForWrite, Adesk::kTrue) == AcMap::kOk)
            // Per tutti i record della tabella OD
            for (; pODIterator->IsDone() == Adesk::kFalse; pODIterator->Next())
            {
               if (pODIterator->GetRecord(ODRecord) == AcMap::kOk)
                  if (gsc_strcmp(ODTableName, ODRecord.ODTableName()) == 0)
                     // aggiorno il record già esistente
                     {                      
                        ODRecord.Value(NewFieldPos) = ODRecord.Value(OldFieldPos);
                        if (pODIterator->UpdateRecord(ODRecord) != AcMap::kOk)
                           { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
                     }
            }

   pBlockTableRecord->close();
   delete pODIterator;
   delete pIterator;

   if (TableDef.RemoveColumn(OldFieldPos) != AcMap::kOk)
      { GS_ERR_COD = eGSInvODFieldName; return GS_BAD; }
   if (pODCont->UpdateODTable(ODTableName, TableDef) != AcMap::kOk) return GS_BAD;

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// Object Data Table Data - FINE
// Extended Entity Data - INIZIO
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_setID2EED                          <external> /*
/*+
  Questa funzione, usata per gli oggetti grafici di classi con collegamento 
  a DB, inserisce un identificatore numerico all'entità <ent>.
  Se l'entità ha già un valore nella EED allora 
  viene settato il valore con il nuovo identificatore, 
  altrimenti viene creata una nuova EED settato l'identificatore.
  Parametri:
  ads_name ent;                  entità a cui assegnare l'identificatore
  long     ID;                   Codice identificativo dell'entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_setID2EED(const TCHAR *handle, long ID)
{
   ads_name ent;

   if (acdbHandEnt(handle, ent) != RTNORM) return GS_BAD;

   return gsc_setID2EED(ent, ID);
}
int gsc_setID2EED(ads_name ent, long ID)
{
   C_EED eed;
   eed.load(ent);
   eed.gs_id = ID;
   return eed.save(ent, GS_BAD);
}


/*********************************************************/
/*.doc gs_PurgeODTables                       <external> */
/*+
  Questa funzione LISP cancella le tabelle dati oggetto che non sono usate.
  Parametri:
  ([dwg])   opzionale, path completa di un file dwg
  
  Restituisce T in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_PurgeODTables(void)
{
   presbuf arg = acedGetArgs();
   int     result;

   if (arg && arg->restype == RTSTR)
      result = gsc_PurgeODTables(arg->resval.rstring);
   else
      result = gsc_PurgeODTables();

   if (result == GS_GOOD)
   {
      acedRetT();
      return RTNORM;
   }
   else
   {
      acedRetNil();
      return RTERROR;
   }
}


/*********************************************************/
/*.doc gsc_PurgeODTables                      <external> */
/*+
  Questa funzione LISP cancella le tabelle dati oggetto che non sono usate.
  Parametri:
  const TCHAR *DwgPath; Opzionale, Nome del file DWG da elaborare
                        Se = NULL si usa il disegno corrente (default = NULL)
  
  Restituisce T in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gsc_PurgeODTables(const TCHAR *DwgPath)
{
   AcMapSession     *mapApi  = NULL;
   AcMapProject     *pProj   = NULL;
   AcMapODContainer *pODCont = NULL;
   AcMapStringArray ODList;
   C_STR_LIST       ODTableNameList, EntODTableNameList;
   C_STR            *pODTableName;
   C_STRING         tmp_path, ext;
   AcDbDatabase     extDatabase(Adesk::kFalse), *pDatabase;
   int              result = GS_GOOD, i;
   long             nPurged = 0;

   AcDbBlockTable               *pBlockTable;
   AcDbBlockTableRecord         *pBlockTableRecord;
   AcDbBlockTableRecordIterator *pIterator;
   AcMapODRecordIterator        *pODIterator;
   AcDbObjectId                 objId;
   bool                         Stop = false;

   if (!(mapApi = AcMapGetSession())) return GS_BAD;

   if (gsc_strlen(DwgPath) > 0)
   {
      tmp_path = DwgPath;
      // Controlla Correttezza Path
      if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD; 
      gsc_splitpath(tmp_path, NULL, NULL, NULL, &ext);
      if (ext.len() == 0) // se non ha estensione la aggiungo
         tmp_path += _T(".DWG");

      // leggo il dwg
      // _SH_DENYWR = open for read/write and allow no sharing
      if (extDatabase.readDwgFile(tmp_path.get_name(), _SH_DENYWR) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }

      if (mapApi->GetProjectForDb(pProj, &extDatabase) == Adesk::kFalse) return GS_BAD;
      pDatabase = &extDatabase;
   }
   else
   {
      if (mapApi->GetProject(pProj) == Adesk::kFalse) return GS_BAD;
      pDatabase = acdbHostApplicationServices()->workingDatabase();
   }

   if (pProj->GetODContainer(pODCont) == Adesk::kFalse) return GS_BAD;
   if (pODCont->GetODTableNames(ODList) != AcMap::kOk) return GS_BAD;

   if (ODList.Length() == 0) return GS_GOOD;

	for (i = 0; i < ODList.Length(); i++)
		ODTableNameList.add_tail_str(ODList.At(i));

   if (pDatabase->getBlockTable(pBlockTable, AcDb::kForRead) != Acad::eOk) return GS_BAD;
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pBlockTableRecord, AcDb::kForRead) != Acad::eOk)
      { pBlockTable->close(); return GS_BAD; }
   pBlockTable->close();
   if (pBlockTableRecord->newIterator(pIterator) != Acad::eOk) 
      { pBlockTableRecord->close(); return GS_BAD; }

   if (pODCont->GetObjectODRecordIterator(pODIterator) != AcMap::kOk)
      { pBlockTableRecord->close(); return GS_BAD; }

   // Ciclo su tutti gli oggetti
   for (pIterator->start(); !pIterator->done(); pIterator->step())
      if (pIterator->getEntityId(objId) == Acad::eOk)
         if (gsc_getODTables(objId, EntODTableNameList, pODIterator, true) == GS_GOOD)
         {
            pODTableName = (C_STR *) EntODTableNameList.get_head();
            while (pODTableName)
            {
               pODTableName = (C_STR *) ODTableNameList.search_name(pODTableName->get_name());
               if (pODTableName)
               {
                  ODTableNameList.remove(pODTableName);
                  if (ODTableNameList.get_count() == 0) // tutte le tabelle OD sono usate
                     { Stop = true; break; }
               }

               pODTableName = (C_STR *) EntODTableNameList.get_next();
            }
         }

   pBlockTableRecord->close();
   delete pODIterator;
   delete pIterator;

   pODTableName = (C_STR *) ODTableNameList.get_head();
   while (pODTableName)
   {
      // cancello la tabella OD
      if (pODCont->RemoveODTable(pODTableName->get_name()) != AcMap::kOk)
      {
          TCHAR Msg[MAX_LEN_MSG];
         swprintf(Msg, MAX_LEN_MSG, _T("OD Table named <%s> not erased."), pODTableName->get_name());
         gsc_write_log(Msg);
         result = GS_BAD;
      }
      else
         nPurged++;

      pODTableName = (C_STR *) ODTableNameList.get_next();
   }

   if (gsc_strlen(DwgPath) > 0 && nPurged > 0)
      if (extDatabase.saveAs(tmp_path.get_name()) != Acad::eOk)
         return GS_BAD;

   acutPrintf(gsc_msg(447), nPurged); // "\nEliminati %ld tabella/e dati oggetto dal disegno."

   return result;
}


/*********************************************************/
/*.doc gsc_oddeletetab                        <external> */
/*+
  Questa funzione cancella la tabella OD specificata settando
  prima la variabile CMDDIA = 0 per evitare messaggi a video
  e ripristinandola a fine comando.
  Parametri:
  const TCHAR *TableName;      Nome tabella
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_oddeletetab(const TCHAR *TableName)
{
   int              OldCmdDia, result = GS_BAD;
   AcMapSession     *mapApi = NULL;
   AcMapProject     *pProj = NULL;
   AcMapODContainer *pODCont = NULL;
   AcMapODTable     *pODTable = NULL;

   // cancello la tabella OD
   gsc_set_cmddia(0, &OldCmdDia);

   do
   {
      if (!(mapApi = AcMapGetSession())) break;
      if (mapApi->GetProject(pProj) == Adesk::kFalse) break;
      if (pProj->GetODContainer(pODCont) == Adesk::kFalse) break;
      if (pODCont->RemoveODTable(TableName) == Adesk::kFalse) break;
      result = GS_GOOD;
   }
   while (0);

   gsc_set_cmddia(OldCmdDia); // Reset CMDDIA

   return GS_GOOD;
}


/*************************************************************************/
/*.doc gsc_createODTable                                      <internal> */
/*+
  Funzione che crea una tabella dati oggetto con gli attributi di una
  C_ATTRIB_LIST.
  Parametri:
  C_STRING &ODTableName;      Nome della tabella dati oggetto
  C_ATTRIB_LIST *pAttribList; Puntatore C_ATTRIB_LIST già inizializztaa tramite 
                              "init_ADOType" se il parametro AllCharDataType = false
  bool AllCharDataType;       Flag, Se true tutti i tipi di dato saranno considerati 
                              char altrimenti verranno creati i campi con i tipi 
                              corrispondenti al DB (default = false)

  Restituisce il puntatore alla tabella OD in caso di successo altrimenti restituisce NULL.
  N.B. Alloca memoria.
-*/  
/*************************************************************************/
AcMapODTable *gsc_createODTable(C_STRING &ODTableName, C_LIST *_AttribList, bool AllCharDataType) // roby shape
{
   C_RB_LIST TableDef;
   C_ATTRIB_LIST *pAttribList = (C_ATTRIB_LIST *) _AttribList;
   C_ATTRIB  *pAttrib;
   C_STRING  ColName, ColDesc;

   if (ODTableName.len() == 0 || ODTableName.len() > MAX_LEN_ODTABLE_NAME)
      { GS_ERR_COD = eGSInvODTabName; return NULL; }
   if (pAttribList == NULL) { GS_ERR_COD = eGSInvalidArg; return NULL; }

   if ((TableDef << acutBuildList(RTLB,
                                    RTLB,
                                       RTSTR, _T("tablename"),
                                       RTSTR, ODTableName.get_name(),
                                    RTDOTE,
                                    RTLB,
                                       RTSTR, _T("tabledesc"),
                                       RTSTR, GS_EMPTYSTR,
                                    RTDOTE,
                                    RTLB,
                                       RTSTR, _T("columns"), 0)) == NULL)
      return NULL;

   pAttrib = (C_ATTRIB *) pAttribList->get_head();
   while (pAttrib)
   {
      ColName = (pAttrib->name.len() == 0) ? GS_EMPTYSTR : pAttrib->name.get_name(); // roby shape
      ColDesc = (pAttrib->Descr.len() == 0) ? GS_EMPTYSTR : pAttrib->Descr.get_name();

      if ((TableDef += acutBuildList(RTLB,
                                        RTLB,
                                           RTSTR, _T("colname"),
                                           RTSTR, ColName.get_name(),
                                        RTDOTE,
                                        RTLB,
                                           RTSTR, _T("coldesc"),
                                           RTSTR, ColDesc.get_name(),
                                        RTDOTE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      
      if (AllCharDataType || gsc_DBIsNumeric(pAttrib->ADOType) != GS_GOOD)
      {
         if ((TableDef += acutBuildList(RTLB,
                                           RTSTR, _T("coltype"),
                                           RTSTR, _T("character"),
                                        RTDOTE,
                                        RTLB,
                                           RTSTR, _T("defaultval"),
                                           RTSTR, GS_EMPTYSTR,
                                        RTDOTE,
                                        RTLE, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
      }
      else
         if (gsc_DBIsNumericWithDecimals(pAttrib->ADOType) == GS_GOOD)
         {
            if ((TableDef += acutBuildList(RTLB,
                                                RTSTR, _T("coltype"),
                                                RTSTR, _T("real"),
                                             RTDOTE,
                                             RTLB,
                                                RTSTR, _T("defaultval"),
                                                RTREAL, 0.0,
                                             RTDOTE,
                                             RTLE, 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return NULL; }
         }
         else
            if ((TableDef += acutBuildList(RTLB,
                                                RTSTR, _T("coltype"),
                                                RTSTR, _T("integer"),
                                             RTDOTE,
                                             RTLB,
                                                RTSTR, _T("defaultval"),
                                                RTREAL, 0.0,
                                             RTDOTE,
                                             RTLE, 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return NULL; }

      pAttrib = ( C_ATTRIB *) pAttrib->get_next();
   }

   if ((TableDef += acutBuildList(RTLE, RTLE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

   ade_errclear();
   if (ade_oddefinetab(TableDef.get_head()) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return NULL; }

   // Aggiungo la tabelle definita in GEOsimAppl::OD_TABLENAME_LIST
   GEOsimAppl::OD_TABLENAME_LIST.add_tail_unique(ODTableName.get_name());

   return gsc_getODTable(ODTableName);
}


/*************************************************************************/
/*.doc gsc_setODRecordValue                                   <internal> */
/*+
  Funzione che setta il valore di un campo di un record OD leggendolo da un resbuf.
  Parametri:
  AcMapValue &Value;    Valore di un record di una tabella OD
  presbuf pRb;           Valore in formato resbuf

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
void gsc_setODRecordValue(AcMapValue &Value, presbuf pRb)
{
   switch (Value.Type())
   {
      case AcMap::kInteger:
      {
         int dummy;

         if (gsc_rb2Int(pRb, &dummy) == GS_GOOD) Value = dummy;
         else Value = 0;

         break;
      }
      case AcMap::kReal:
      {
         double dummy;

         if (gsc_rb2Dbl(pRb, &dummy) == GS_GOOD) Value = dummy;
         else Value = 0;

         break;
      }
      case AcMap::kCharacter:
         if (pRb->restype == RTSTR) Value = pRb->resval.rstring;
         else Value = _T("");
         
         break;
		case AcMap::kPoint:
      {
         AcGePoint3d dummy(0, 0, 0);
         ads_point   Pt;

         if (gsc_rb2Pt(pRb, Pt) == GS_GOOD)
            AcGePoint3d_set_from_ads_point(Pt, dummy);
         Value = dummy;
         break;
      }
   }
}


/*************************************************************************/
/*.doc gsc_setODRecord                                        <internal> */
/*+
  Funzione che aggiunge o modifica un record di dati oggetto di una
  tabella nota con i valori dei campi della classe.
  Parametri:
  ads_name ent;                  Oggetto grafico
  C_RB_LIST &ColValues;          Lista di valori da scrivere
  AcMapODTable *pTable;          Tabella dati oggetto
  AcMapODRecordIterator *pIter;  Struttura opzionale per velocizzare la
                                 funzione (input, default = NULL). Vedi
                                 "gsc_GetObjectODRecordIterator"

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************/
int gsc_setODRecord(ads_name ent, C_RB_LIST &ColValues,
                    AcMapODTable *pTable, AcMapODRecordIterator *pIter)
{
   AcDbObjectId       objId;
   AcDbObject         *pObj = NULL; 
   AcMapODTableRecord Record;
   int                i;
   presbuf            pRb;

   if (!(pRb = ColValues.get_head())) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (acdbGetObjectId(objId, ent) != Acad::eOk) 
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (acdbOpenObject(pObj, objId, AcDb::kForWrite, true) != Acad::eOk) 
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   AcMap::EErrCode err;

   // leggo l'identificatore dell'entità
   if ((err = pIter->Init(pObj, AcMap::kOpenForWrite, Adesk::kTrue)) != AcMap::kOk)
      { pObj->close(); GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   for (; pIter->IsDone() == Adesk::kFalse; pIter->Next())
      if (pIter->GetRecord(Record) == AcMap::kOk && 
          gsc_strcmp(pTable->Name(), Record.ODTableName()) == 0)
      {
         // aggiorno il record già esistente
         for (i = 0; i < Record.Count(); i++)
         {
            // <aperta tonda> poi <nome colonna> poi <valore colonna>
            if (!(pRb = pRb->rbnext) || !(pRb = pRb->rbnext) || !(pRb = pRb->rbnext))
               { pObj->close(); GS_ERR_COD = eGSInvalidArg; return GS_BAD; } 
            
            gsc_setODRecordValue(Record.Value(i), pRb);

            // <chiusa tonda>
            if (!(pRb = pRb->rbnext))
               { pObj->close(); GS_ERR_COD = eGSInvalidArg; return GS_BAD; } 
         }

         if (pIter->UpdateRecord(Record) != AcMap::kOk)
            { pObj->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

         if (pObj) pObj->close();
         return GS_GOOD;
      }

   // se non ha il record lo aggiungo
   pTable->InitRecord(Record);
   for (i = 0; i < Record.Count(); i++)
   {
      // <aperta tonda> poi <nome colonna> poi <valore colonna>
      if (!(pRb = pRb->rbnext) || !(pRb = pRb->rbnext) || !(pRb = pRb->rbnext))
         { pObj->close(); GS_ERR_COD = eGSInvalidArg; return GS_BAD; } 
      
      gsc_setODRecordValue(Record.Value(i), pRb);

      // <chiusa tonda>
      if (!(pRb = pRb->rbnext))
         { pObj->close(); GS_ERR_COD = eGSInvalidArg; return GS_BAD; } 
   }

   if (pTable->AddRecord(Record, pObj) != AcMap::kOk)
      { pObj->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   pObj->close();

   return GS_GOOD;
}