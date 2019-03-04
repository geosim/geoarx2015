/**********************************************************
Name: GS_LOCK.CPP
                                   
Module description: File funzioni di base per la gestione
                    del lock agli oggetti. 
            
Author: Paolo De Sole & Poltini Roberto

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.a.

**********************************************************/


/**************************************************************************/
/*  INCLUDE  */
/**************************************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "rxdefs.h"   
#include "adslib.h"   
#include <adeads.h>
#include "adsdlg.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"   
#include "gs_list.h"   
#include "gs_utily.h"
#include "gs_init.h"   
#include "gs_netw.h"   
#include "gs_resbf.h"
#include "gs_ase.h"
#include "gs_class.h"
#include "gs_graph.h"
#include "gs_cmd.h"
#include "gs_prjct.h"   
#include "gs_dbref.h"
#include "gs_area.h"   
#include "gs_ade.h"   
#include "gs_lock.h"

/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


int gsc_getCls2UnlockList(C_PROJECT *pPrj,C_WRK_SESSION *pSession, C_INT_LIST &Cls2UnlockList);
int get_cls_sub_id(ads_name entita, int *cls, int *sub,
                   TCHAR id[MAX_LEN_HANDLE + DWG_FILE_NAME_LEN - 1]);


/*********************************************************/
/*.doc gsc_PrepareSelLockWhereKey             <external> */
/*+	   
  Questa funzione restituisce il comando SQL per la ricerca di lock
  di una entità di una classe.
  Parametri:
  C_DBCONNECTION *pConn;                  Puntatore connessione OLE-DB
  C_STRING       &TableRef;               Riferimento completo alla tabella
  _CommandPtr    &pCmd;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_PrepareSelLockWhereKey(C_DBCONNECTION *pConn, C_STRING &TableRef,
                               _CommandPtr &pCmd)
{
   C_STRING      statement;
   _ParameterPtr pParam;

   // interrogo il database con la select where "codice database"
   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE ID=? AND CLASS_ID=? AND SUB_CL_ID=?");

   if (gsc_PrepareCmd(pConn->get_Connection(), statement, pCmd) == GS_BAD)
      return GS_BAD;

   if (gsc_CreateDBParameter(pParam, _T("ID"), adWChar, MAX_LEN_HANDLE + DWG_FILE_NAME_LEN - 2) == GS_BAD) return GS_BAD;
   pCmd->Parameters->Append(pParam);
   
   if (gsc_CreateDBParameter(pParam, _T("CLASS_ID"), adSmallInt) == GS_BAD) return GS_BAD;
   pCmd->Parameters->Append(pParam);

   if (gsc_CreateDBParameter(pParam, _T("SUB_CL_ID"), adSmallInt) == GS_BAD) return GS_BAD;
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/***************************************************************/
/* Inizio funzioni della classe C_LOCK                         */
/***************************************************************/

// Costruttore della classe C_LOCK
C_LOCK::C_LOCK()
{
   ads_name_clear(Object); // Entity name
   cls = 0;                // Codice della classe dell' oggetto
   sub = 0;                // sottocodice della classe
   wcscpy(id, GS_EMPTYSTR);     // Codice dell' entità
   Session = 0;            // sessione di lavoro
   Status = FREE;          // Stato del lock
}
C_LOCK::C_LOCK(ads_name _ent)
{
   set_ent(_ent);
   Session = 0;            // sessione di lavoro
   Status = FREE;          // Stato del lock
}


void C_LOCK::set_ClsSubId(int ClsCode, int SubCode, const TCHAR *IdCode)
{
   cls = ClsCode;
   sub = SubCode;
   gsc_strcpy(id, IdCode, MAX_LEN_HANDLE + DWG_FILE_NAME_LEN - 1);
}


void C_LOCK::set_ClsSubId(int ClsCode, int SubCode, long IdCode)
{
   cls = ClsCode;
   sub = SubCode;
   swprintf(id, _T("%ld"), IdCode);
}


int C_LOCK::set_ent(ads_name _ent)
{
   ads_name_set(_ent, Object);
   Session = 0;
   DateTime.clear();
   return get_cls_sub_id(Object, &cls, &sub, id);
}

int C_LOCK::get_cls(void)  { return cls; }    
int C_LOCK::get_sub(void)  { return sub; }    
int C_LOCK::get_status(void) { return Status; }    
long C_LOCK::get_session(void) { return Session; }    
TCHAR *C_LOCK::get_DateTime(void) { return DateTime.get_name(); } 


/**************************************************************/
/*.doc int C_LOCK::FindForId                                  */
/*+                                                                       
  Funzione di ricerca del record di lock.
  Devono essere inizializzati i membri id, cls, sub. 
  Parametri:
  _RecordsetPtr &pRs;         Recordset di lettura
  bool *IsRecordsetCloseable; Flag che ritorna se il record set deve essere chiuso
                              o se è da lascaire aperto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/**************************************************************/
int C_LOCK::FindForId(_RecordsetPtr &pRs, bool *IsRecordsetCloseable)
{
   if ((pRs = GS_CURRENT_WRK_SESSION->get_pPrj()->get_RsSelectLock()) != NULL) 
   {  
      SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 3 elementi
      SAFEARRAYBOUND rgsabound;
      _variant_t     var, KeyForSeek;
      long           ix;

      rgsabound.lLbound   = 0;
      rgsabound.cElements = 3;
      psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

      // ID
      ix  = 0;
      gsc_set_variant_t(var, id);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE CLASSE
      ix  = 1;
      gsc_set_variant_t(var, (short) cls);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE SOTTOCLASSE
      ix  = 2;
      gsc_set_variant_t(var, (short) sub);
      SafeArrayPutElement(psa, &ix, &var);

      KeyForSeek.vt = VT_ARRAY|VT_VARIANT;
      KeyForSeek.parray = psa;  
      
      // Uso tecnica dei recordset molto più veloce
      if (gsc_DBSetIndexRs(pRs, _T("GS_LOCK_ON_ID_CLASS_ID_SUB_CL_ID")) == GS_BAD)
         return GS_BAD;

      *IsRecordsetCloseable = false;

      return gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ);
   }
   else
   {
      _CommandPtr pCmd;

      // Ricavo il comando che esegue la selezione di link per handle
      if ((pCmd = GS_CURRENT_WRK_SESSION->get_pPrj()->get_CmdSelectLockWhereKey()) == NULL) return GS_BAD;
      // Imposto i vari parametri 
      if (gsc_SetDBParam(pCmd, 0, id) == GS_BAD ||
          gsc_SetDBParam(pCmd, 1, cls) == GS_BAD ||
          gsc_SetDBParam(pCmd, 2, sub) == GS_BAD)
         return GS_BAD;

      *IsRecordsetCloseable = true;

      // Esegue l'istruzione SQL
      return gsc_ExeCmd(pCmd, pRs, adOpenDynamic, adLockOptimistic);
   }
}


/**************************************************************/
/*.doc int C_LOCK::get_info                                   */
/*+                                                                       
  Funzione di lettura delle informazioni di un lock.
  Devono essere inizializzati i membri id, cls, sub. Verranno letti i
  valori di State, Session, DateTime.
  Parametri:
  int *Found;   Flag che indica se è stato trovato il lock (= GS_GOOD) oppure
                oppure se non è stato trovato (= GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/**************************************************************/
int C_LOCK::get_info(int *Found)
{
   static C_RB_LIST ColValues;
   static presbuf   pRbOwner;
   static presbuf   pRbLock;
   static presbuf   pRbDateTime;
   _RecordsetPtr    pRs;
   bool             IsRecordsetCloseable;

   Status = FREE;

   if (sub > 0) // si tratta di simulazione
   {
      C_WRK_SESSION_LIST lista_aree;
      C_WRK_SESSION      *p_area;
      C_AREACLASS_LIST   lista_classi;
      C_AREACLASS        *pClass;

      if (lista_aree.rarea(GS_CURRENT_WRK_SESSION->get_PrjId()) == GS_BAD) return GS_BAD;
      p_area = (C_WRK_SESSION *) lista_aree.get_head();
      while (p_area)
      { 
         // se nella sessione di lavoro è stata estratta la classe
         if (p_area->get_pClsCodeList()->search_key(cls) != NULL)
            // leggo le informazioni dettagliate delle classi già estratte nella sessione
            if (p_area->get_class_list(lista_classi) == GS_BAD) return GS_BAD;
            if ((pClass = (C_AREACLASS *) lista_classi.search_key(cls)) != NULL &&
                pClass->get_level() == GSUpdateableData)
            {
               Status   = TEMPORARY_LOCKED;
               Session  = p_area->get_id();
               DateTime = p_area->get_CreationDateTime();
               *Found   = GS_GOOD;

               return GS_GOOD;
            }
         p_area = (C_WRK_SESSION *) lista_aree.get_next();
      }

      *Found = GS_BAD;

      return GS_GOOD;
   }

   // se non ha id si tratta di una entità GEOsim nuova (spaghetti) oppure
   // se codice negativo si tratta di una entità GEOsim nuova (con database)
   if (wcslen(id) == 0 || _wtoi(id) < 0)
   {
      Status = NEW;
      DateTime.clear();
      Session = GS_CURRENT_WRK_SESSION->get_id();
      *Found = GS_GOOD;

      return GS_GOOD;
   }

   if (FindForId(pRs, &IsRecordsetCloseable) == GS_BAD) return GS_BAD;

   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      *Found = GS_BAD; 
      if (IsRecordsetCloseable) gsc_DBCloseRs(pRs);
      return GS_GOOD; 
   }

   // Se lista resbuf di lettura non è inizializzata 
   if (!ColValues.get_head()) 
   {  // inizializzazione lista resbuf di lettura
      if (gsc_InitDBReadRow(pRs, ColValues) == NULL)
      {
         if (IsRecordsetCloseable) gsc_DBCloseRs(pRs);
         return GS_BAD;
      }
      pRbLock     = ColValues.CdrAssoc(_T("ACTIVE"));
      pRbOwner    = ColValues.CdrAssoc(_T("OWNER"));
      pRbDateTime = ColValues.CdrAssoc(_T("DATETIME"));
   }

   // Lettura record
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
   {
      if (IsRecordsetCloseable) gsc_DBCloseRs(pRs);
      return GS_BAD;
   }

   if (IsRecordsetCloseable) gsc_DBCloseRs(pRs);

   if (gsc_rb2Int(pRbLock, &Status) == GS_BAD) return GS_BAD;

   // se lo stato è TEMPORARY_LOCKED o PERMANENT_LOCKED allora l'oggetto è bloccato
   if (Status == TEMPORARY_LOCKED || Status == PERMANENT_LOCKED)
   {
      if (pRbDateTime->restype == RTSTR) DateTime = pRbDateTime->resval.rstring;
      else DateTime.clear();
      if (gsc_rb2Lng(pRbOwner, &Session) == GS_BAD) return GS_BAD; 
   }
   else
   {
      DateTime.clear();
      Session = 0;
   }

   *Found = GS_GOOD;

   return GS_GOOD;
}


/**************************************************************/
/*.doc is_locked                                              */
/*+                                                                       
  Funzione che verifica se un oggetto è bloccato. 
  Parametri:
  ads_name entity;      Oggetto da verificare.
  int      *result;     Risultato del controllo: GS_GOOD se era bloccato altrimenti GS_BAD
  int      OnlyByOther; Flag, se = GS_GOOD controlla che l'entità sia bloccata solo
                        da altre sessioni di lavoro altrimenti anche da quella corrente
                        (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. vengono inizializzati i membri <Object>, <cls>, <sub>, <id> (solo se non sottoclasse)
-*/  
/**************************************************************/
int C_LOCK::is_locked(ads_name entita, int *result, int OnlyByOther)
{
   // leggo le informazioni GEOsim dall'oggetto grafico (<id> solo se non sottoclasse)
   if (set_ent(entita) == GS_BAD) return GS_BAD;

   return is_locked(result, OnlyByOther);
}


/**************************************************************/
/*.doc is_locked                                              */
/*+                                                                       
  Funzione che verifica se un oggetto è bloccato.
  In caso affermativo viene inizializzata il codice della sessione proprietaria.
  N.B.: Devono già essere inizializzati i membri <cls>, <sub>, <id>.

  Parametri:
  int      *result;     Risultato del controllo: GS_GOOD se era bloccato altrimenti GS_BAD
  int      OnlyByOther; Flag, se = GS_GOOD controlla che l'entità sia bloccata solo
                        da altre sessioni di lavoro altrimenti anche da quella corrente
                        (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/**************************************************************/
int C_LOCK::is_locked(int *result, int OnlyByOther)
{
   int Found;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (get_info(&Found) == GS_BAD) return GS_BAD;

   *result = GS_BAD;
   // se lo stato è TEMPORARY_LOCKED o PERMANENT_LOCKED allora l'oggetto è bloccato
   if (Status == TEMPORARY_LOCKED || Status == PERMANENT_LOCKED)
   {
      // se vale solo quando è bloccata da altre sessioni
      if (OnlyByOther == GS_GOOD) 
      {
         if (Session != GS_CURRENT_WRK_SESSION->get_id()) *result = GS_GOOD;
      }
      else
         *result = GS_GOOD;
   }

   return GS_GOOD;
}


/**************************************************************/
/*.doc WhoHasIt                                              */
/*+                                                                       
  Funzione che legge i dati relativi al lock dell'oggetto.
  Parametri:
  ads_name entity       Oggetto da verificare.
  int      *result;     Risultato del controllo (FREE, NEW, LOCKED)
  C_STRING *handenti;   Handle entità           (default = NULL)
  C_STRING *filename;   Nome dwg di provenienza (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 

  N.B. vengono comunque inizializzati i membri <Object>, <cls>, <sub>,
-*/  
/**************************************************************/
int C_LOCK::WhoHasIt(ads_name entity, int *result, C_STRING *handenti, C_STRING *filename)
{
   int Found;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // leggo le informazioni GEOsim dall'oggetto grafico (<id> solo se non sottoclasse)
   if (set_ent(entity) == GS_BAD) return GS_BAD;

   if (get_info(&Found) == GS_BAD) return GS_BAD;

   if (!Found)
   {
      if (filename) filename->clear();
      if (handenti) handenti->clear();
   }
   else
      switch (Status)
      {
         case TEMPORARY_LOCKED:
         case PERMANENT_LOCKED:
         {
            C_CLASS *pClass;

            // ricavo il puntatore della classe. 
            pClass = GS_CURRENT_WRK_SESSION->find_class(get_cls(), get_sub());

            // Se la classe usa i DWG
            if (pClass->ptr_GphInfo()->getDataSourceType() == GSDwgGphDataSource)
            {
               C_RB_LIST RbList;

               if ((RbList << gsc_ade_qrygetdwgandhandle(entity)) != NULL)
               {
                  if (handenti) 
                     // memorizzo Handle
                     handenti->set_name(RbList.get_head()->rbnext->resval.rstring);

                  if (filename)
                  {
                     ade_id dwg_id = (ade_id) RbList.get_head()->resval.rreal; // Id del dwg

                     // memorizzo nome del file
                     if ((RbList << ade_dwggetsetting(dwg_id, _T("DWGNAME"))) == NULL)
                        { GS_ERR_COD = eGSDwgCannotActivate; return GS_BAD; }
                     if (RbList.get_head()->restype == RTSTR && RbList.get_head()->resval.rstring)
                     {
                        C_STRING tmp_path;

                        tmp_path = RbList.get_head()->resval.rstring;
                        // Converto path con alias
                        if (gsc_nethost2drive(tmp_path) == GS_BAD) return GS_BAD;
                        filename->set_name(tmp_path.get_name());
                     }
                     else
                        filename->set_name(GS_EMPTYSTR);
                   }
               }
            }
            else // la geometria è in tabelle DB
            {
               C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pClass->ptr_GphInfo();

               if (handenti)
               {
                  TCHAR        Handle[MAX_LEN_HANDLE];
                  C_BSTR_REAL  *pHandleId;

                  // ricavo l'handle
                  if (gsc_enthand(entity, Handle) == GS_GOOD &&
                     // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
                     // è un oggetto estratto
                      (pHandleId = (C_BSTR_REAL *) pGphInfo->HandleId_LinkTree.search(Handle)))
                     // memorizzo codice geometrico
                     handenti->set_name(pHandleId->get_key());
                  else
                     handenti->clear();
               }

               if (filename)
                  filename->set_name(pGphInfo->TableRef.get_name());
               else
                  filename->clear();
            }
            break;
         }
         case NEW:
            if (filename) filename->set_name(gsc_msg(591)); // "nessuno (entità nuova)"
            if (handenti) handenti->clear();
            break;
      }
   *result = Status;

   return GS_GOOD;
}


/**************************************************************/
/*.doc set_locked(ads_name obj)                               */
/*+                                                                       
  Funzione che blocca un oggetto in GS_LOCK.
  Parametri:
  ads_name obj; Oggetto da bloccare.
  int *result;  flag, se GS_GOOD la funzione ha bloccato l'oggetto altrimenti
                l'oggetto era bloccato da qualcun'altro
  int mode;     se TEMPORARY_LOCKED allora il blocco ha la durata della sessione
                se PERMANENT_LOCKED allora il blocco permane finchè esistono sessioni
                di lavoro (solo l'ultima rilascia tutti i LOCK permanenti)
                default = TEMPORARY_LOCKED

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/**************************************************************/
int C_LOCK::set_locked(ads_name obj, int *result, int mode)
{
   // leggo le informazioni GEOsim dall'oggetto grafico (<id> solo se non sottoclasse)
   if (get_cls_sub_id(obj, &cls, &sub, id) == GS_BAD) return GS_BAD;
   ads_name_set(obj, Object);
   
   return set_locked(result, mode);
}


/**************************************************************/
/*.doc set_locked                                             */
/*+                                                                       
  Funzione che blocca un oggetto in GS_LOCK.
  N.B.: Devono già essere inizializzati i membri <cls>, <sub>, <id>.

  Parametri:
  int *result;  flag, se GS_GOOD la funzione ha bloccato l'oggetto altrimenti
                l'oggetto era bloccato da qualcun'altro
  int mode;     se TEMPORARY_LOCKED allora il blocco ha la durata della sessione
                se PERMANENT_LOCKED allora il blocco permane finchè esistono sessioni
                                    di lavoro (solo l'ultima rilascia tutti i LOCK permanenti)
                default = TEMPORARY_LOCKED

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/**************************************************************/
int C_LOCK::set_locked(int *result, int mode)
{
   static C_RB_LIST ColValues;
   static presbuf   pRbOwner;
   static presbuf   pRbLock;
   static presbuf   pRbDateTime;
   static presbuf   pRbCls;
   static presbuf   pRbSub;
   static presbuf   pRbId;
   _RecordsetPtr    pRs;

   *result = GS_BAD;

   // verifico abilitazione della classe
   if (GS_CURRENT_WRK_SESSION->get_level() != GSUpdateableData)
      { GS_ERR_COD = eGSReadOnlySession; return GS_BAD; }

   if (sub > 0) // si tratta di simulazione
   {
      C_SUB *pSub;

      if (!(pSub = (C_SUB *) GS_CURRENT_WRK_SESSION->find_class(cls, sub))) return GS_BAD;
      if (pSub->ptr_id()->abilit != GSUpdateableData) // la classe viene bloccata interamente
         GS_ERR_COD = eGSObjectIsLockedByAnotherUser;
      else
         *result = GS_GOOD;

      return GS_GOOD;
   }

   // se non ha id si tratta di una entità GEOsim nuova (spaghetti) oppure
   // se codice negativo si tratta di una entità GEOsim nuova (con database)
   if (wcslen(id) == 0 || _wtoi(id) < 0)
   {  // se l'oggetto è nuovo non deve essere bloccato
      *result = GS_GOOD;
      return GS_GOOD;
   }

   // setto la data e l'ora di lock
   gsc_current_DateTime(DateTime);

   if (ColValues.get_head() == NULL)
   {
      // Costruisco il record da inserire
      if ((ColValues << acutBuildList(RTLB,
                                      RTLB, RTSTR, _T("ACTIVE"),    RTSHORT, mode, RTLE,
                                      RTLB, RTSTR, _T("OWNER"),     RTLONG,  GS_CURRENT_WRK_SESSION->get_id(), RTLE,
                                      RTLB, RTSTR, _T("CLASS_ID"),  RTSHORT, cls, RTLE,
                                      RTLB, RTSTR, _T("SUB_CL_ID"), RTSHORT, sub, RTLE,
                                      RTLB, RTSTR, _T("ID"),        RTSTR,   id, RTLE,
                                      RTLB, RTSTR, _T("DATETIME"),  RTSTR,   (DateTime.len() == 0) ? GS_EMPTYSTR : DateTime.get_name(), RTLE,
                                      RTLE, 0)) == NULL)
         return GS_BAD;
      pRbLock     = ColValues.CdrAssoc(_T("ACTIVE"));
      pRbOwner    = ColValues.CdrAssoc(_T("OWNER"));
      pRbCls      = ColValues.CdrAssoc(_T("CLASS_ID"));
      pRbSub      = ColValues.CdrAssoc(_T("SUB_CL_ID"));
      pRbId       = ColValues.CdrAssoc(_T("ID"));
      pRbDateTime = ColValues.CdrAssoc(_T("DATETIME"));
   }
   else
   {
      gsc_RbSubst(pRbLock, mode);
      gsc_RbSubst(pRbOwner, GS_CURRENT_WRK_SESSION->get_id());
      gsc_RbSubst(pRbCls, cls);
      gsc_RbSubst(pRbSub, sub);
      gsc_RbSubst(pRbId, id);
      gsc_RbSubst(pRbDateTime, (DateTime.len() == 0) ? GS_EMPTYSTR : DateTime.get_name());
   }

   if ((pRs = GS_CURRENT_WRK_SESSION->get_pPrj()->get_RsInsertLock()) == NULL) return GS_BAD;
   // Un solo tentativo senza stampare eventuali messaggi di errore
   if (gsc_DBInsRow(pRs, ColValues, ONETEST, GS_BAD) == GS_BAD)
   {
      int Found;

      // se l'errore non era dovuto perchè il lock esisteva già
      if (GS_ERR_COD != eGSIntConstr) return GS_BAD;
      GS_ERR_COD = eGSNoError;

      // Lettura info lock
      if (get_info(&Found) == GS_BAD || Found == GS_BAD) return GS_BAD;
      // se è bloccato da me stesso
      if (GS_CURRENT_WRK_SESSION->get_id() == Session) *result = GS_GOOD;

      return GS_GOOD;
   }

   *result = GS_GOOD;

   return GS_GOOD;
}


/**************************************************************/
/*.doc set_unlocked(ads_name obj)                             */
/*+                                                                       
  Funzione che sblocca un oggetto in GS_LOCK.
  Parametri:
  ads_name entity      Oggetto da sbloccare.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/**************************************************************/
int C_LOCK::set_unlocked(ads_name obj)
{
   _CommandPtr   pCmd;
   _RecordsetPtr pRs;
   int           Res = GS_BAD;

   // leggo le informazioni GEOsim dall'oggetto grafico (<id> solo se non sottoclasse)
   if (get_cls_sub_id(obj, &cls, &sub, id) == GS_BAD) return GS_BAD;
   ads_name_set(Object, obj);

   // Ricavo il comando che esegue la selezione di un lock
   if ((pCmd = GS_CURRENT_WRK_SESSION->get_pPrj()->get_CmdSelectLockWhereKey()) == NULL) return GS_BAD;
   // Imposto i vari parametri 
   if (gsc_SetDBParam(pCmd, 0, id) == GS_BAD ||
       gsc_SetDBParam(pCmd, 1, cls) == GS_BAD ||
       gsc_SetDBParam(pCmd, 2, sub) == GS_BAD)
      return GS_BAD;
   // Esegue l'istruzione SQL
   if (gsc_ExeCmd(pCmd, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD) return GS_BAD;
   
   if (gsc_isEOF(pRs) == GS_BAD)
      // Cancella la riga
      Res = gsc_DBDelRow(pRs);
   gsc_DBCloseRs(pRs);
   
   return Res;
}


/***************************************************************/
/* Fine funzioni della classe C_LOCK                           */
/***************************************************************/


/*************************************************************/
/*.doc (new 2) gsc_unlockallObjs(long numArea) <external> */
/*+	   
   Funzione che rilascia tutti gli oggetti bloccati.
   Se si tratta dell'ultima sessione di lavoro verrano rilasciati tutti i lock
   temporanei e permanenti altrimenti solo i lock temporanei della sessione.
   Parametri: 
   C_PROJECT     *pPrj;       puntatore a progetto
   C_WRK_SESSION *pSession;   puntatore alla sessione di lavoro per ricerca in GS_LOCK

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************/
int gsc_unlockallObjs(C_PROJECT *pPrj, C_WRK_SESSION *pSession)
{
   C_DBCONNECTION *pConn;
   C_STRING       statement, TableRef;
   int            isLast = TRUE, result;
   C_WRK_SESSION_LIST SessionList;
   C_WRK_SESSION      *csr_SessionList; 

   if (!pPrj) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   result = GS_BAD;
   do
   {
      // ricavo la lista delle sessioni
      if (SessionList.rarea(pPrj->get_key()) == GS_BAD) break;
      
      csr_SessionList = (C_WRK_SESSION *) SessionList.get_head();
      while (csr_SessionList)
      {  // se è un'altra sessione
         if (csr_SessionList->get_id() != pSession->get_id())
            // e questa NON è appena creata in attesa della prima estrazione
            if (csr_SessionList->get_status() != WRK_SESSION_WAIT)
               { isLast = FALSE; break; } // ci sono altre sessione che stanno lavorando

         csr_SessionList = (C_WRK_SESSION *) SessionList.get_next();
      }

      if (pPrj->getLocksTabInfo(&pConn, &TableRef) == GS_BAD) break; 

      statement = _T("DELETE FROM ");
      statement += TableRef;

      if (!isLast) 
      {
         C_INT_LIST Cls2UnlockList;
         C_INT      *pCls2Unlock;

         // se esistono altre sessioni di lavoro rilascio solo i lock temporanei
         // della sessione...
         statement += _T(" WHERE OWNER=");
         statement += pSession->get_id();
         statement += _T(" AND (ACTIVE=");
         statement += TEMPORARY_LOCKED;

         // ricavo la lista delle classi i cui lock permanenti sono rilasciabili 
         if (gsc_getCls2UnlockList(pPrj, pSession, Cls2UnlockList) == GS_BAD) break;

         // ... e rilascio anche i lock permanenti su classi non estratte in
         // modifica da altre sessioni di lavoro
         pCls2Unlock = (C_INT *) Cls2UnlockList.get_head();
         if (pCls2Unlock)
         {
            statement += _T(" OR CLASS_ID IN (");

            while (pCls2Unlock)
            {
               statement += pCls2Unlock->get_key();
               pCls2Unlock = (C_INT *) Cls2UnlockList.get_next();
               if (pCls2Unlock) statement += _T(",");
            }
            statement += _T(")");
         }

         statement += _T(")");
      }

      // eseguo istruzione SQL
      if (gsc_ExeCmd(pConn->get_Connection(), statement) == GS_BAD) break;

      if (!isLast) // se esistono altre sessioni di lavoro setto il codice
      {            // sessione = -1 solo per i lock permanenti della sessione
         statement = _T("UPDATE ");
         statement += TableRef;
         statement += _T(" SET OWNER=-1 WHERE ACTIVE=");
         statement += PERMANENT_LOCKED;
         statement += _T(" AND OWNER=");
         statement += pSession->get_id();

         // eseguo istruzione SQL
         if (gsc_ExeCmd(pConn->get_Connection(), statement) == GS_BAD) break;
      }

      result = GS_GOOD;
   }
   while (0);

   return result;
}


/********************************************************************/
/*.doc (new 2) gsc_getCls2UnlockList                                */
/*+	   
  Funzione che ritorna una lista di codici di classi che sono state
  estratte nella sessione e sono modificabili e che NON sono state 
  estratte in altre sessioni in modalità modifica. In questo modo è
  possibile sapere quali lock permanenti si possono comunque rilasciare.
  Parametri:
  C_PROJECT     *pPrj;            puntatore a progetto
  C_WRK_SESSION *pSession;        puntatore alla sessione di lavoro per ricerca in GS_LOCK
  C_INT_LIST    &Cls2UnlockList;  Lista dei codici delle classi risultato della ricerca
    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/********************************************************************/
int gsc_getCls2UnlockList(C_PROJECT *pPrj, C_WRK_SESSION *pSession, C_INT_LIST &Cls2UnlockList)
{
   C_AREACLASS_LIST curr_class_list, class_list;
   C_AREACLASS      *p_class, *p_class2;
   C_WRK_SESSION_LIST      SessionList;
   C_WRK_SESSION           *csr_SessionList;
   C_INT            *pCls2Unlock;

   Cls2UnlockList.remove_all();

   // ricavo le classi di questa sessione che sono modificabili ed estratte
   if (pSession->get_class_list(curr_class_list) == GS_BAD) return GS_BAD;

   p_class = (C_AREACLASS *) curr_class_list.get_head();
   while (p_class)
   {  // se modificabile ed estratta
      if (p_class->get_level() == GSUpdateableData && 
         (p_class->get_sel() == EXTRACTED || p_class->get_sel() == EXTR_SEL))
         p_class = (C_AREACLASS *) curr_class_list.get_next();
	   else // la cancello
      {
         curr_class_list.remove_at();
         p_class = (C_AREACLASS *) curr_class_list.get_cursor();
      }
   }

   // ricavo la lista delle sessioni
   if (SessionList.rarea(pPrj->get_key()) == GS_BAD) return GS_BAD;
   
   csr_SessionList = (C_WRK_SESSION *) SessionList.get_head();
   while (csr_SessionList)
   {  // se non si tratta della sessione corrente
      if (csr_SessionList->get_id() != pSession->get_id())
      {
         // ricavo le classi estratte della sessione
         if (csr_SessionList->get_class_list(class_list) == GS_BAD)
         {  // se non si può sapere quali classi ci sono nella sessione
            // considero che le abbia estratte tutte
            curr_class_list.remove_all();
            break;
         }

         // per ogni classe estratta e modificabile della sessione corrente verifico
         // se ne esiste una con le stesse caratteristiche nelle altre sessioni
         p_class = (C_AREACLASS *) curr_class_list.get_head();
         while (p_class)
            // se modificabile ed estratta
            if ((p_class2 = (C_AREACLASS *) class_list.search_key(p_class->get_key())) != NULL &&
                p_class2->get_level() == GSUpdateableData && 
                (p_class2->get_sel() == EXTRACTED || p_class2->get_sel() == EXTR_SEL))
            {
               curr_class_list.remove_at(); // la tolgo
               p_class = (C_AREACLASS *) curr_class_list.get_cursor();
            }
            else
               p_class = (C_AREACLASS *) curr_class_list.get_next();
      }
      // sessione successiva
      csr_SessionList = (C_WRK_SESSION *) SessionList.get_next();
   }

   // dalla lista <curr_class_list> ricavo la lista delle classi con oggetti
   // rilasciabili da lock permanenti
   p_class = (C_AREACLASS *) curr_class_list.get_head();
   while (p_class)
   {   
      if ((pCls2Unlock = new (C_INT)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      pCls2Unlock->set_key(p_class->get_key());
      Cls2UnlockList.add_tail(pCls2Unlock);

      p_class = (C_AREACLASS *) curr_class_list.get_next();
   }

   return GS_GOOD;
}


/********************************************************************/
/*.doc (new 2) gsc_setSelSetLocked(ads_name GrSelection) <external> */
/*+	   
  Funzione che blocca in GS_LOCK tutti gli oggetti del gruppo di  
  selezione passato come parametro, se uno di questi oggetti è
  risulta già bloccato viene cancellato dal gruppo di selezione.

  Parametri:
  ads_name GrSelection;   Gruppo di selezione contenente gli oggetti 
                          da bloccare.
  int mode;     se TEMPORARY_LOCKED allora il blocco ha la durata della sessione
                se PERMANENT_LOCKED allora il blocco permane finchè esistono sessioni
                di lavoro (solo l'ultima rilascia tutti i LOCK permanenti)
                default = TEMPORARY_LOCKED
  int CounterToVideo;     flag, se = GS_GOOD stampa a video il numero di entità
                          che si stanno elaborando (default = GS_BAD)
    
  Restituisce il numero di entità bloccate in caso di successo altrimenti
  restituisce -1.
-*/  
/********************************************************************/
long gsc_setSelSetLocked(ads_name GrSelection, int mode, int CounterToVideo)
{
   long     Count = 0, LockedCount = 0;
   int      res;
   C_LOCK   oggetto;
   ads_name ent;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1069)); // "Bloccaggio entità"

   if (CounterToVideo == GS_GOOD)
   {
      long SelSetLen = 0;
      acedSSLength(GrSelection, &SelSetLen);
      StatusBarProgressMeter.Init(SelSetLen);
   }

   while (acedSSName(GrSelection, Count++, ent) == RTNORM)
   {
      // se res = GS_GOOD la funzione ha bloccato l'oggetto altrimenti
      // l'oggetto era bloccato da qualcun'altro
      if (oggetto.set_locked(ent, &res, mode) == GS_BAD) return GS_BAD;
      // se res = GS_GOOD la funzione ha bloccato l'oggetto altrimenti
      // l'oggetto era bloccato da qualcun'altro
      if (res == GS_GOOD) LockedCount++;

      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(Count);
   }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return Count - LockedCount;
}



/********************************************************************/
/*.doc (new 2) gsddwhohasit                              <external> */
/*+	   
  Comando che ritorna i dati relativi al proprietario di un oggetto GEOsim.
-*/  
/********************************************************************/
void gsddwhohasit(void)
{
	int       ret;
   long      WrkSessionId;
	ads_name	 Ent;
	ads_point Point;
	C_STRING  DateTime, file, msg, ClassName;
   TCHAR     HeaderStr[MAX_LEN_CLASSNAME + LEN_KEY_ATTR + LEN_KEY_ATTR + 100];
   C_ID      ClassIdentif;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }   

   acutPrintf(gsc_msg(210)); // "\nSeleziona entità: "
   acedInitGet(RSG_NONULL, GS_EMPTYSTR);
   while ((ret = acedEntSel(GS_EMPTYSTR, Ent, Point)) == RTERROR);
   if (ret == RTREJ) { GS_ERR_COD = eGSAdsCommandErr; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   if (ret == RTCAN) return GEOsimAppl::CMDLIST.CancelCmd();

 	if (gsc_whohasit(Ent, &ClassIdentif, &WrkSessionId, &DateTime, NULL, &file) == GS_BAD)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   if (ClassIdentif.sub_code == 0)
   {
      ClassName = ClassIdentif.name;
      // "L'entità della classe %s (codice %d)"
      swprintf(HeaderStr, MAX_LEN_CLASSNAME + LEN_KEY_ATTR + LEN_KEY_ATTR + 100,
               gsc_msg(593), ClassName.get_name(), ClassIdentif.code);
   }
   else
   {
      C_CLASS *pMotherClass = GS_CURRENT_WRK_SESSION->find_class(ClassIdentif.code);

      ClassName = pMotherClass->ptr_id()->name;
      ClassName += _T(" - ");
      ClassName += ClassIdentif.name;
      // "L'entità della classe %s (codice %d, sottocodice %d)"
      swprintf(HeaderStr, gsc_msg(557), ClassName.get_name(), ClassIdentif.code,
               ClassIdentif.sub_code);
   }

   msg = HeaderStr;

   if (WrkSessionId != 0)
   {  // " è stata bloccata"
      msg += gsc_msg(620);
      
      if (WrkSessionId > 0)
      {
         msg += gsc_msg(592); // " dalla sessione n°."
         msg += WrkSessionId;
      }
      if (DateTime.len() > 0)
      {
         msg += gsc_msg(558); // " il "
         msg += DateTime;
      }
      if (file.len() > 0)
      {
         // ". Disegno di provenienza: "
         msg += gsc_msg(560);
         msg += file.get_name();
      }
      gsc_ddalert(msg.get_name());
   }
	else
   {  // " non è bloccata."
      msg += gsc_msg(561);
      gsc_ddalert(msg.get_name()); 
   }

   return GEOsimAppl::CMDLIST.EndCmd();
}


/********************************************************************/
/*.doc (new 2) gsc_whohasit(ads_name GrSelection) <external> */
/*+	   
  Funzione che ritorna i dati relativi al proprietario di un oggetto GEOsim.
  Parametri:
  ads_name entity;         Oggetto grafico
  C_ID     *ClassIdentif;  Struttura identificativa della classe di GEOsim (default = NULL)
  long     *nArea;         Codice area proprietaria (default = NULL)
  C_STRING *DateTimeLock;  Data e ora di lock (default = NULL)
  C_STRING *handenti,      Handle entità nel disegno di origine (default = NULL)
  C_STRING *filename       Nome dei file .DWG di origine (default = NULL)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/********************************************************************/
int gsc_whohasit(ads_name entity, C_ID *ClassIdentif, long *nArea, C_STRING *DateTimeLock,
                 C_STRING *handenti, C_STRING *filename)
{
	C_LOCK  oggetto;
	int     result;
   C_CLASS *pClass;

   // verifico se l'oggetto è libero
   if (oggetto.WhoHasIt(entity, &result, handenti, filename) == GS_BAD) return GS_BAD;
   
   if (ClassIdentif)
   {
      // ricavo il puntatore della classe. 
      pClass = GS_CURRENT_WRK_SESSION->find_class(oggetto.get_cls(), oggetto.get_sub());
      pClass->ptr_id()->copy(ClassIdentif);
   }

   if (result == NEW) // oggetto nuovo
   {
      if (GS_CURRENT_WRK_SESSION == NULL) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      if (nArea) *nArea = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : 0;
      if (DateTimeLock) DateTimeLock->clear();	
   }
   else
   {  // se l'oggetto è bloccato da me stesso result = FREE ma oggetto 
      // viene comunque inizializzato
      if (nArea) *nArea = oggetto.get_session();
      if (DateTimeLock) DateTimeLock->set_name(oggetto.get_DateTime());	
   }

   return GS_GOOD;
}


/**************************************************************/
/*.doc get_cls_sub_id                              <internal> */
/*+                                                                       
  Funzione che legge informazioni GEOsim da un oggetto grafico
  (<id> solo se non sottoclasse). 
  Parametri:
  ads_name entity;                                 Oggetto da verificare.
  int      *cls;                                   codice classe
  int      *sub;                                   codice sottoclasse
  TCHAR    id[MAX_LEN_HANDLE + DWG_FILE_NAME_LEN - 1]; identificatore entità inizializzato
                                                   solo se non sottoclasse
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/**************************************************************/
int get_cls_sub_id(ads_name entita, int *cls, int *sub,
                   TCHAR id[MAX_LEN_HANDLE + DWG_FILE_NAME_LEN - 1])
{
   C_EED   eed;
   C_CLASS *pClass;

   // verifico che l' entità sia di GEOsim
   if (eed.load(entita) == GS_BAD)
      { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }

   *cls = eed.cls;
   *sub = eed.sub;
   
   // ricavo il puntatore della classe. 
   if ((pClass = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL)
      return GS_BAD;

   if (eed.sub == 0) // NON è sotto-classe
   {
      // devo trattare gli spaghetti in un modo diverso
      if (pClass->get_category() == CAT_SPAGHETTI)
      {
         C_STRING IdInternal, tablename;
         // cerco nelle tabelle OD l' identificatore dell' entità spaghetti
         // ricavo la tabella interna
         gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), eed.cls, eed.sub,
                                   tablename);
         // ricavo l' identificatore dell 'entità 
         if (gsc_getIDfromODTable(entita, tablename, IdInternal) == GS_BAD)
         // se l'entità non ha ID allora si tratta di uno spaghetti non bloccato
         // result è già settato a GS_BAD quindi return.
            { wcscpy(id, GS_EMPTYSTR); return GS_GOOD; }
         gsc_strcpy(id, IdInternal.get_name(), MAX_LEN_HANDLE + DWG_FILE_NAME_LEN - 1);
      }
      else
      {
         long key_val;

         if (pClass->getKeyValue(entita, &key_val) == GS_BAD) return GS_BAD;
         swprintf(id, _T("%ld"), key_val); // inizializzo membro di C_LOCK
      }
   }

   return GS_GOOD;
}


