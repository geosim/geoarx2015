/**********************************************************
Name: GS_QUERY.CPP
                                   
Module description: File funzioni di base per l'inserimento,
                    l'aggiornamento e l'interrogazione, filtro
                    per le entità GEOSIM. 
            
Author: Roberto Poltini

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.A.

**********************************************************/


/**********************************************************/
/*   INCLUDE                                              */
/**********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <string.h>
#include <ctype.h>

#include "rxdefs.h"   
#include "adslib.h"   
#include <adeads.h>
#include <dbmain.h>
#include <dbpl.h>

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_opcod.h"

#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_list.h"
#include "gs_ase.h"
#include "gs_dbref.h"
#include "gs_init.h"
#include "gs_user.h"
#include "gs_class.h"
#include "gs_prjct.h"
#include "gs_area.h"
#include "gs_graph.h"
#include "gs_attbl.h"   // gestione blocchi attributi visibili
#include "gs_lisp.h"
#include "gs_cmd.h"
#include "gs_sec.h"     // gestione tabelle secondarie
#include "gs_query.h"
#include "gs_lock.h"
#include "gs_setv.h"
#include "gs_ade.h"
#include "gs_dwg.h"     // gestione disegni
#include "gs_topo.h"
#include "gs_evid.h"


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
   double  tempo=0, tempo1=0, tempo2=0, tempo3=0, tempo4=0, tempo5=0, tempo6=0, tempo7=0;
   double  tempo8=0, tempo9=0, tempo10=0, tempo11=0, tempo12=0, tempo13=0, tempo14=0;
#endif


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


int GS_CURRENT_OPERATION = NONE;  // codice operazione corrente di GEOsim


/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/


int gsc_is_group_existing(C_CLASS *pCls, C_CLS_PUNT_LIST *p_cls_punt_list);
int gsc_is_group_existing(C_CLASS *pCls, C_2INT_LONG_LIST &memberList);


static int gsc_areActionsOnErase(TCHAR *usr_cmds);
static int gsc_doActionOnInsert(const TCHAR *UsrFunction, ads_name ent, presbuf info,
                                C_RB_LIST *pColValues, short visib_block, int Reason = UNKNOWN_MOD);
static int gsc_doActionOnInsert(const TCHAR *UsrFunction, ads_name SelSet, C_RB_LIST *pColValues,
                                int Reason = UNKNOWN_MOD);
static int gsc_doActionOnUpdate(const TCHAR *UsrFunction, ads_name ent,
                                int Reason = UNKNOWN_MOD);
static int gsc_doActionOnUpdate(const TCHAR *UsrFunction, int cls, int sub, long gs_id,
                                C_RB_LIST *pColValues, int Reason = UNKNOWN_MOD);
static int gsc_doActionOnErase(const TCHAR *UsrFunction, ads_name Entity,
                               int Reason = UNKNOWN_MOD);
static int gsc_doActionOnErase(const TCHAR *UsrFunction, int cls, int sub, long gs_id, 
                               int Reason = UNKNOWN_MOD);


int gsc_verify_topo_on_align_for_link(C_SUB *pLinkSub, ads_name LinkEnt,
                                      C_SUB *pNewNode,
                                      C_RB_LIST &NewNodeDefaultValues,
                                      int NewNodeIsDefCalc,
                                      _RecordsetPtr &pNewNodeInsRs,
                                      C_CLS_PUNT **pInitNode, int *NewInitNode,
                                      C_CLS_PUNT **pFinalNode, int *NewFinalNode);

int gsc_insBlockAttrib(C_CLASS *pClass, C_SELSET &ssPR, C_SELSET &ssDA,
                       long BitForChangeToNewFAS, C_FAS *pNewFas, 
                       C_RB_LIST &ColValues);
int gsc_getDefAttrFromGSblock(ads_name BlockGs, TCHAR *NameAttr, C_RB_LIST &DefAttr);


/////////////// FUNZIONI PER AMBIENTE LISP ////////////////////////


/*********************************************************/
/*.doc gs_ins_data <external> */
/*+                                                                       
  Inserisce una scheda nella tabella temporanea della classe e
  visualizza gli attributi visibili attivando il collegamento
  tra grafica e tabelle.
  Parametri:

  Per classi NON gruppo:
  Lista RESBUF (<cls><sub><ename>[((<nome colonna><valore>) ...)[<visib. blocco attr>]])
  
  Per classi gruppo:
  Lista RESBUF (<cls><SelSet>[((<nome colonna><valore>) ...)])

  Restituisce codice nuova entità in caso di successo, GS_CAN in caso di annullamento
  altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gs_ins_data(void)
{
   presbuf   arg = acedGetArgs();
   C_RB_LIST ColValues; 
   int       cls, sub, rc, visib_block = VISIBLE;
   ads_name  newent;
   long      new_code;
   C_CLASS   *pCls;

   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   // codice classe
   if (!arg  || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   cls = (int) arg->resval.rint;

   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls)) == NULL) return RTERROR;
   
   if (pCls->get_category() == CAT_GROUP) // classe gruppo
   {
      C_CLS_PUNT_LIST lista_cls;

      if ((arg = arg->rbnext) == NULL || arg->restype != RTPICKS)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

      // lista oggetti grafici che compongono il gruppo
      if (lista_cls.from_ss(arg->resval.rlname) == GS_BAD) return RTERROR;
      // lista <nome colonna><valore> (opzionale)
      if ((arg = arg->rbnext) == NULL)
      {  // leggo scheda di default
         if (pCls->get_default_values(ColValues, (C_SELSET *) NULL) == GS_BAD) return RTERROR;        
      }
      else
      {
         if (arg->restype != RTLB)
         {  // leggo scheda di default
            if (pCls->get_default_values(ColValues, (C_SELSET *) NULL) == GS_BAD) return RTERROR;           
         }
         else
            // lista <nome colonna><valore>
            if ((ColValues << gsc_listcopy(arg, &arg)) == NULL)
               { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      }

      // inserimento gruppo
      if ((rc = pCls->ins_data(&lista_cls, ColValues, &new_code)) == GS_BAD)
         return RTERROR;   
      else if (rc == GS_CAN) return RTCAN;   
   }
   else // non si tratta di classe gruppo
   {
      // codice sotto-classe
      if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      sub = (int) arg->resval.rint;

      // nome oggetto grafico
      if ((arg = arg->rbnext) == NULL)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      if (arg->restype != RTENAME) { GS_ERR_COD=eGSInvRBType; return RTERROR; }
      ads_name_set(arg->resval.rlname, newent);

      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return RTERROR;

      // lista <nome colonna><valore> (opzionale)
      if ((arg = arg->rbnext) == NULL)
      {  // leggo scheda di default
         if (pCls->get_default_values(ColValues, newent) == GS_BAD) return RTERROR;        
      }
      else
      {
         if (arg->restype != RTLB)
         {  // leggo scheda di default
            if (pCls->get_default_values(ColValues, newent) == GS_BAD) return RTERROR;           
         }
         else
            // lista <nome colonna><valore>
            if ((ColValues << gsc_listcopy(arg, &arg)) == NULL)
               { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

         // blocco attributi visibile (opzionale)
         if ((arg = arg->rbnext) != NULL) 
         {
            if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
	         visib_block = (int) arg->resval.rint;
         }
      }

      if ((rc = pCls->ins_data(newent, ColValues, &new_code, visib_block)) == GS_BAD)
         return RTERROR;
      else if (rc == GS_CAN) return RTCAN;
   }
   acedRetReal((double) new_code);

   return RTNORM;
}


/*********************************************************/
/*.doc gs_aggr_data <external> */
/*+                                                                       
  Aggrega un gruppo di selezione ad una entità già esistente
  Parametri:
  Lista RESBUF (<newSS><oldent>)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gs_aggr_data(void)
{        
   presbuf  arg = acedGetArgs();
   ads_name newSS, oldent;
   long     code;
   C_CLASS  *pCls;
   
   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   // gruppo di selezione oggetti grafici nuovi
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype != RTPICKS) { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   ads_name_set(arg->resval.rlname, newSS);
   // nome oggetto grafico a cui aggregarsi
   if ((arg = arg->rbnext) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype != RTENAME) { GS_ERR_COD=eGSInvRBType; return RTERROR; }
   ads_name_set(arg->resval.rlname, oldent);

   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(oldent)) == NULL) return RTERROR;

   if (gsc_startTransaction() == GS_BAD) return GS_BAD;

   if (pCls->aggr_data(newSS, oldent, &code) == GS_BAD)
      { gsc_abortTransaction(); return RTERROR; }

   if (gsc_endTransaction() == GS_BAD) return RTERROR;
   acedRetReal((double) code);

   return RTNORM;
}


/*********************************************************/
/*.doc gs_disaggr_data                        <external> */
/*+                                                                       
  Disgrega una entità. Se l'entità da disgregare
  è un blocco DA, questo verrà cancellato.
  Parametri:
  Lista RESBUF (<ent>|<selection set> [<CounterToVideo>])
  
  Restituisce il numero di oggetti disaggregati in caso di successo altrimenti nil. 
-*/  
/*********************************************************/
int gs_disaggr_data(void)
{        
   presbuf  arg = acedGetArgs();
   ads_name ent;
   int      CounterToVideo;
   long     Refused, i, dummy;
   C_CLASS  *pCls;
   C_SELSET ss;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1080)); // "Disgregazione entità"
   
   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   // gruppo di selezione oggetti grafici nuovi
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype == RTENAME) ss.add(arg->resval.rlname);
   else if (arg->restype == RTPICKS) ss.add_selset(arg->resval.rlname);
   else { GS_ERR_COD = eGSInvRBType; return RTERROR; }

   if ((arg = arg->rbnext) && arg->restype == RTNIL) CounterToVideo = GS_BAD;
   else CounterToVideo = GS_GOOD;

   Refused = i = 0;

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(ss.length());

   while (ss.entname(i++, ent) == GS_GOOD)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) != NULL)
         // se si tratta di un blocco DA lo cancello
         if (gsc_is_DABlock(ent) == GS_GOOD)
         {
            if (pCls->erase_data(ent) != GS_GOOD) Refused++;
         }
         else
            if (pCls->disaggr_data(ent, &dummy) != GS_GOOD) Refused++;
   }
   i--;

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      if (i >= 0)
         acutPrintf(gsc_msg(308), i, Refused); // "\n%ld entità GEOsim elaborate, %ld scartate."
   }
   acedRetReal(i - Refused);

   return RTNORM;
}


/*********************************************************/
/*.doc gs_query_data <external> */
/*+                                                                       
  Interroga la scheda dell'entita' della classe.
  Parametri:
  Lista RESBUF (ename oggetto grafico)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gs_query_data(void)
{        
   presbuf   arg = acedGetArgs();
   C_RB_LIST ColValues;
   ads_name  entity;
   C_CLASS   *pCls;
   int       cls;
   
   acedRetNil();
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // nome oggetto grafico (per aggiornare da video)
   if (!arg || arg->restype == RTENAME)
   {
      ads_name_set(arg->resval.rlname, entity);
      pCls = GS_CURRENT_WRK_SESSION->find_class(arg->resval.rlname);
      if (pCls->query_data(arg->resval.rlname, ColValues) == GS_BAD) return RTERROR;
   }
   else if (gsc_rb2Int(arg, &cls) == GS_GOOD) // codice classe
   {
      long gs_id;
      int  sub;

      // codice sottoclasse
      if (!(arg = arg->rbnext) || gsc_rb2Int(arg, &sub) == GS_BAD)
         { GS_ERR_COD = eGSInvRBType; return RTERROR; }    
      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return RTERROR;
      // gs_id
      if (!(arg = arg->rbnext) || gsc_rb2Lng(arg, &gs_id) == GS_BAD)
         { GS_ERR_COD = eGSInvRBType; return RTERROR; }     

      if (pCls->query_data(gs_id, ColValues) != GS_GOOD)
         return RTERROR;
   }
   else
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }

   ColValues.remove_head(); ColValues.remove_tail();  
   ColValues.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_upd_data <external> */
/*+                                                                       
  Aggiorna la scheda dell'entita' della classe.
  Parametri:
  Lista RESBUF:
  (ename oggetto grafico) (<lista valori>))
  oppure
  (gs_id (<lista valori>))
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gs_upd_data(void)
{        
   presbuf   arg;
   int       cls, sub = 0;
   C_RB_LIST ColValues;
   
   acedRetNil();

   if (!(arg = acedGetArgs())) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype == RTENAME)
   {
      if (!arg->rbnext || arg->rbnext->restype != RTLB)
         { GS_ERR_COD = eGSInvRBType; return RTERROR; }
      // lista valori
      ColValues << gsc_rblistcopy(arg->rbnext);
      if (gsc_upd_data(arg->resval.rlname, ColValues) == GS_BAD) return RTERROR;
   }
   else if (gsc_rb2Int(arg, &cls) == GS_GOOD) // codice classe
   {
      C_CLASS *pCls;
      long     gs_id;

      // codice sottoclasse
      if (!(arg = arg->rbnext) || gsc_rb2Int(arg, &sub) == GS_BAD)
         { GS_ERR_COD = eGSInvRBType; return RTERROR; }    
      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return RTERROR;
      // gs_id
      if (!(arg = arg->rbnext) || gsc_rb2Lng(arg, &gs_id) == GS_BAD)
         { GS_ERR_COD = eGSInvRBType; return RTERROR; }     
      // lista valori
      if (!(arg = arg->rbnext) || arg->restype != RTLB)
         { GS_ERR_COD = eGSInvRBType; return RTERROR; }

      ColValues << gsc_rblistcopy(arg);

      if (pCls->upd_data(gs_id, ColValues, NULL, RECORD_MOD) != GS_GOOD)
         return RTERROR;
   }
   else
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_erase_data <external> */
/*+                                                                       
  Cancella gli oggetti grafici delle entita' della/e classe/i
  se ultima entità cancella anche la scheda associata.
  Parametri:
  Lista RESBUF <ent>
  Per classi gruppo:
  Lista RESBUF <cls><ent>
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gs_erase_data(void)
{        
   presbuf arg = acedGetArgs();
   C_EED   eed;
   C_CLASS *pCls;

   acedRetNil();

   // nome oggetto grafico (per aggiornare da video)
   if (!arg) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      
   if (arg->restype != RTENAME)
   {
      int            cls;
      long           KeyVal;
      C_PREPARED_CMD pTempLnkCmd, pOldLnkCmd;
      C_LONG_LIST    KeyGroupList;
      C_LONG         *pKeyComp;

      // codice classe gruppo
      if (arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      cls = arg->resval.rint;

      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls)) == NULL) return RTERROR;
   
      if (pCls->get_category() != CAT_GROUP)  // deve essere gruppo
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      
      if (!(arg = arg->rbnext) || arg->restype != RTENAME)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

      /////////////////////////////////////////////////////////////////////////
      // Compilo le istruzioni di lettura dei codici dei gruppi legati ad un membro
      if (pCls->prepare_reldata_where_member(pTempLnkCmd, TEMP) == GS_BAD) return RTERROR;
      if (pCls->prepare_reldata_where_member(pOldLnkCmd, OLD) == GS_BAD) return RTERROR;

      if (eed.load(arg->resval.rlname) == GS_BAD)
         { GS_ERR_COD = eGSGEOsimObjNotFound; return RTERROR; }
      if (gsc_getKeyValue(arg->resval.rlname, &KeyVal) == GS_BAD) return RTERROR;

      // leggo i codici dei gruppi legati a questa entità
      if (pCls->get_group_list(pTempLnkCmd, pOldLnkCmd, eed.cls, KeyVal, KeyGroupList) == GS_BAD)
	      return RTERROR;

      // ciclo per ogni gruppo
      pKeyComp = (C_LONG *) KeyGroupList.get_head();
      while (pKeyComp)
      {
         pCls->erase_data(pKeyComp->get_id());         
         
         pKeyComp = (C_LONG *) KeyGroupList.get_next();
      }
   }
   else
   {
      if (eed.load(arg->resval.rlname) == GS_BAD)
         { GS_ERR_COD = eGSGEOsimObjNotFound; return RTERROR; }

      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL) return RTERROR;

      if (pCls->erase_data(arg->resval.rlname) == GS_BAD) return RTERROR;
   }

   acedRetT();

   return RTNORM;
}


/////////////// FUNZIONI PER AMBIENTE C ////////////////////////


int set_GS_CURRENT_OPERATION(int Value)
{
   int OldValue = GS_CURRENT_OPERATION;
   
   GS_CURRENT_OPERATION = Value;
   return OldValue;
}


/*********************************************************/
/*.doc gsc_upd_data <external> */
/*+                                                                       
  Aggiorna una entità.

  Parametri:
  ads_name  entity;      nome oggetto grafico
  C_RB_LIST &ColValues;  Lista ((<nome colonna><valore>) ...)
  oppure:
  int cls;               Classe
  int sub;               Sottoclasse
  long gs_id;            Codice scheda
  C_RB_LIST &ColValues;  Lista ((<nome colonna><valore>) ...)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_upd_data(ads_name entity, C_RB_LIST &ColValues)
{                 
   C_CLASS *pCls;
   C_EED    eed;
   
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return NULL; }
   if (eed.load(entity) == GS_BAD)
      { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }

   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL) return GS_BAD;

   return pCls->upd_data(entity, ColValues);
}


/*********************************************************/
/*.doc gsc_UpdDataForSave                     <external> */
/*+                                                                       
  Funzione speciale di aggiornamento da lanciare durante il salvataggio qualora
  esistano degli attributi calcolati da GS_ID per le sole entità appartenenti
  a classi C_SIMPLEX.
  Parametri:
  C_CLASS *pCls;              Puntatore alla classe
  C_RB_LIST &ColValues;       Lista ((<nome colonna><valore>) ...)
  C_SELSET &EntSS;            Gruppo di selezione dell'entità
  C_STRING &SubstUpdUsrFun;   Eventuale funzione di virtualizzazione
  C_STRING &BeforeUpdUsrFun;  Eventuale funzione di virtualizzazione
  C_STRING &AfterUpdFun;      Eventuale funzione di virtualizzazione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_UpdDataForSave(C_CLASS *pCls, C_RB_LIST &ColValues, C_SELSET &EntSS,
                       C_STRING &SubstUpdUsrFun, C_STRING &BeforeUpdUsrFun, C_STRING &AfterUpdFun)
{
   int     result = GS_BAD, OldOp;
   int     Cls = pCls->ptr_id()->code, Sub = pCls->ptr_id()->sub_code;
   presbuf p;
   TCHAR   *KeyAttrib = pCls->ptr_info()->key_attrib.get_name();
   long    Key;

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(MODIFY);
   
   // estraggo il codice entità
   if ((p = ColValues.CdrAssoc(KeyAttrib)) == NULL || gsc_rb2Lng(p, &Key) == GS_BAD)
      return GS_BAD;
   // validazione e ricalcolo dati
   if (pCls->CalcValidData(ColValues, MODIFY, &EntSS) == GS_BAD) return GS_BAD;

   // eventuale chiamata ad una funzione utente esterna (prima della modifica o in
   // completa sostituzione della funzione di GEOsim)
   if (SubstUpdUsrFun.len() > 0)
      return gsc_doActionOnUpdate(SubstUpdUsrFun.get_name(), Cls, Sub, Key,
                                  &ColValues, RECORD_MOD);

   if (BeforeUpdUsrFun.len() > 0)
   {
      if (gsc_doActionOnUpdate(BeforeUpdUsrFun.get_name(), Cls, Sub, Key,
                               &ColValues, RECORD_MOD) == GS_BAD) return GS_BAD;
      // validazione e ricalcolo dati ("gsc_doActionOnUpdate" potrebbe cambiare qualcosa)
      if (pCls->CalcValidData(ColValues, MODIFY, &EntSS) == GS_BAD) return GS_BAD;
   }

   if (pCls->updtoDA(EntSS, ColValues) == GS_BAD) return GS_BAD;

   // se esistono funzioni di calcolo grafico vengono applicate ora
   if (pCls->graph_calc(ColValues, &EntSS) == GS_BAD) return GS_BAD;

   // eventuale chiamata ad una funzione utente esterna (dopo la modifica)
   if (AfterUpdFun.len() > 0)
      if (gsc_doActionOnUpdate(AfterUpdFun.get_name(), Cls, Sub, Key,
                               &ColValues, RECORD_MOD) == GS_BAD)
         return GS_BAD;

   // Notifico in file log
   TCHAR Msg[MAX_LEN_MSG];
   swprintf(Msg, MAX_LEN_MSG, _T("Updated entity (on saving): key %ld, prj %d, class %d, subclass %d."),
            Key, pCls->ptr_id()->pPrj->get_key(), Cls, Sub);
   gsc_write_log(Msg);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return GS_GOOD;
}


/*********************************************************/
/*  INIZIO FUNZIONI DELLA CATEGORIA C_SIMPLEX            */
/*********************************************************/


/*****************************************************************************/
/*.doc C_SIMPLEX::ins_data <external>                                        */
/*+                                                                       
  Inserisce una scheda nella tabella temporanea della classe e
  visualizza gli attributi visibili attivando il collegamento
  tra grafica e tabelle.
  Parametri:
  C_CLS_PUNT_LIST *lista_cls;  Nuovo oggetto grafico; se si tratta di superfici 
                               il secondo oggetto rappresenta il riempimento
  C_RB_LIST       &ColValues;  Lista ((<nome colonna><valore>) ...)
  long            *gs_id;		 Nuovo codice entità (default = NULL)
  int             visib_block; Visibilità blocco se = INVISIBLE invisibile altrimenti 
                               visibile (default = VISIBLE)
  int Reason;                  Origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                               RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                               (default = UNKNOWN_MOD)
  _RecordsetPtr   *pRsIns;     RecordSet per inserimento dati (in caso
                               di inserimenti multipli); default = NULL.
  C_BTREE *pPtObjsBTree = NULL; non usato, solo per compatibilità con C_SUB::ins_data

  Restituisce GS_GOOD in caso di successo, GS_CAN in caso di annullamento
  altrimenti restituisce GS_BAD.
  N.B. : il primo oggetto grafico della lista <lista_cls> viene sostituito con 
         un altro oggetto grafico uguale ma con differente handle e con le entità estese
         di GEOsim. L'inserimento dell'oggetto grafico nel database di autocad 
         dovrebbe avvenire con le entità estese di GEOsim per gestire l'UNDO
-*/  
/*****************************************************************************/
int C_SIMPLEX::ins_data(ads_name lista_ent, C_RB_LIST &ColValues, long *gs_id, 
                        int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
{
   C_CLS_PUNT_LIST lista_cls;
   C_CLS_PUNT      *p;
   long            len;

   // Converto una lista di ename in una lista C_CLS_PUNT_LIST
   if (ads_sslength(lista_ent, &len) != RTNORM)
   {
      len = 0;
      // è una sola entità
      if ((p = new C_CLS_PUNT(this, lista_ent)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      lista_cls.add_tail(p);
   }
   else
   {  // è un gruppo di selezione
      ads_name ent;

      for (long i = 0; i < len; i++)
      {
         if (acedSSName(lista_ent, i, ent) != RTNORM) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
         if ((p = new C_CLS_PUNT(this, ent)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         lista_cls.add_tail(p);
      }
   }

   int ret = ins_data(&lista_cls, ColValues, gs_id, visib_block, Reason, pRsIns);

   if (len == 0)
      ads_name_set(((C_CLS_PUNT *) lista_cls.get_head())->ent, lista_ent);
   else
   {
      ads_ssfree(lista_ent);
      lista_cls.to_ssgroup(lista_ent);
   }

   return ret;
}
int C_SIMPLEX::ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues, long *gs_id, 
                        int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
{
   C_ATTRIB_LIST *p_attrib_list = ptr_attrib_list();
   C_EED         eed;
   int           result = GS_BAD, InsRecord = FALSE, InsLink = FALSE, OldOp;
   C_LINK        Link, OrigLink;
   ads_name      NewEnt, newDABlock;
   long          new_key;
   presbuf       pKey;
   C_CLS_PUNT    *pEntCls;
   C_STRING      UsrFunction;
   C_RB_LIST     InfoList;

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
#endif

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t1);
#endif

   if (!lista_cls || lista_cls->get_count() == 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   ads_name_set(((C_CLS_PUNT *) lista_cls->get_head())->ent, NewEnt);
   ads_name_clear(newDABlock);

   if (!(pKey = ColValues.CdrAssoc(ptr_info()->key_attrib.get_name()))) return GS_BAD;

   // verifico che gli oggetti grafici non siano già di "GEOsim"
   pEntCls = (C_CLS_PUNT *) lista_cls->get_head();
   while (pEntCls)
   {
      // controllo che la tipologia sia giusta.
      if (check_graph_compatib(pEntCls->ent) == GS_BAD)
         { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
      // verifico che l'oggetto grafico non sia già legato ad un record di questa classe
      if (eed.load(pEntCls->ent) == GS_GOOD || getKeyValue(pEntCls->ent, &new_key) == GS_GOOD)
         { GS_ERR_COD = eGSGEOsimObjFound; return GS_BAD; }
      pEntCls = (C_CLS_PUNT*) lista_cls->get_next();
   }

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(INSERT);

   do
   {
      // ricavo codice per prossimo inserimento
      if ((new_key = GetNewEntCode()) >= 0) break;
      // modifico <key_attrib>
      gsc_RbSubst(pKey, new_key);

      // eventuale chiamata ad una funzione utente esterna (prima dell'inserimento o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_INSERT, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
		   if (!InfoList.get_head())
            if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			      break;
         if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                  &ColValues, visib_block, Reason) == GS_BAD) break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_INSERT, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
		      if (!InfoList.get_head())
               if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			         break;
            if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                     &ColValues, visib_block, Reason) == GS_BAD) break;
         }

      eed.cls    = id.code;
      eed.sub    = id.sub_code;
      eed.num_el = lista_cls->get_count(); // fattore di aggregazione
      eed.gs_id  = new_key;

      // Collego il primo oggetto grafico alla tabella senza inserirlo nel gruppo 
      // di selezione del salvataggio
      if (OrigLink.Set(NewEnt, id.code, id.sub_code, new_key, INSERT) == GS_BAD) break;
      InsLink = TRUE; // inserimento link avvenuto

      // inserisco etichette "entità di GEOsim" senza inserirlo nel gruppo 
      // di selezione del salvataggio
      if (eed.save(NewEnt, GS_BAD) == GS_BAD) break; 

      C_SELSET SelSet;
      SelSet.add(NewEnt);

      // Collego gli oggetti grafici successivi alla tabella temp
      result = GS_GOOD;
      pEntCls = (C_CLS_PUNT*) lista_cls->getptr_at(2);
      while (pEntCls)
      {
         if (OrigLink.Set(pEntCls->ent, id.code, id.sub_code, new_key, INSERT) == GS_BAD)
            { result = GS_BAD; break; }
         // inserisco etichette "entità di GEOsim" agli oggetti grafici
         // che vengono aggiunti in GEOsimAppl::SAVE_SS per salvataggio
         if (eed.save(pEntCls->ent) == GS_BAD) { result = GS_BAD; break; }

         SelSet.add(pEntCls->ent);

         pEntCls = (C_CLS_PUNT*) pEntCls->get_next();
      }

      if (result == GS_BAD) break;
      result = GS_BAD;

      // Se esiste qualche azione sull'evento cancellazione
      if (gsc_areActionsOnErase(id.usr_cmds) == GS_GOOD)
      {
         // Faccio la copia di una sola entità e cancello la vecchia (è sufficiente una sola)
         // perchè l'inserimento dell'oggetto grafico nel database di autocad 
         // dovrebbe avvenire con le entità estese di GEOsim per gestire l'UNDO.
         if (gsc_DeepClone2ModSpace(NewEnt) != GS_GOOD || gsc_EraseEnt(NewEnt) != GS_GOOD)
            { GS_ERR_COD = eGSErrorExecutingCommand; break; }
         if (acdbEntLast(NewEnt) != RTNORM) { GS_ERR_COD = eGSInvEntityOp; break; }

         // sostituisco entità nella lista degli oggetti grafici (parametro della funzione)
         ads_name_set(NewEnt, ((C_CLS_PUNT*) lista_cls->get_head())->ent);

         // Collego il nuovo oggetto grafico alla tabella
         if (Link.Set(NewEnt, id.code, id.sub_code, new_key, INSERT) == GS_BAD)
            { result = GS_BAD; break; }
         // inserisco etichetta "entità di GEOsim" al nuovo oggetto grafico
         // che viene aggiunto in GEOsimAppl::SAVE_SS per salvataggio
         if (eed.save(NewEnt) == GS_BAD) { result = GS_BAD; break; }
      }

      // inserisco record in tabella (fa anche il ricalcolo e la validazione)
      if (ins_row(ColValues, NULL, NULL, pRsIns, &SelSet) == GS_BAD) break;
      InsRecord = TRUE; // inserimento record avvenuto

      if (gs_id) *gs_id = new_key;

      if (id.type != TYPE_TEXT && 
          visib_block == VISIBLE && p_attrib_list->is_visible() == GS_GOOD)
      {
         C_RB_LIST      VisValues;
         C_ATTRIB_BLOCK attrib_block;
         C_SET_VISIB    *pActiveSet = NULL;

         eed.num_el++; // + 1 scheda attributi

         // ricavo gli attributi visibili
         if ((VisValues << p_attrib_list->get_vis_values(ColValues)) == GS_BAD)
            break;
         if (attrib_block.set_layer(ptr_fas()->layer) == GS_BAD) break;
         // altezza testo degli attributi del blocco
         if (attrib_block.set_h_text(ptr_fas()->h_text) == GS_BAD) break;
         // stile testo degli attributi del blocco
         if (attrib_block.set_text_style(ptr_fas()->style) == GS_BAD) break;
         // colore
         if (attrib_block.set_color(ptr_fas()->color) == GS_BAD) break;

         if (GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO)
         {
            ads_point point;
            double    rot;

            if (id.type == TYPE_POLYLINE) // punto medio tratto + lungo
            {
               if (gsc_getPntRtzOnObj(NewEnt, point, &rot, _T("MML"), 0, 0, GS_GOOD) == GS_BAD) break;
            }
            else if (id.type == TYPE_NODE) // punto di inserimento
            {
               if (gsc_getPntRtzOnObj(NewEnt, point, &rot, _T("S"), 0, 0, GS_GOOD) == GS_BAD) break;
            }
            else if (id.type == TYPE_SURFACE) // centroide
            {
               if (gsc_getPntRtzOnObj(NewEnt, point, &rot, _T("C"), 0, 0, GS_GOOD) == GS_BAD) break;
            }
            else break;

            attrib_block.set_insert_point(point);
            attrib_block.set_rotation(gsc_rad2grd(rot));
         }

         if ((result = attrib_block.insert(VisValues, id.code, id.sub_code, new_key,
                                           ptr_fas()->file_ref_block, ptr_fas()->ref_block,
                                           GEOsimAppl::GLOBALVARS.get_InsPos(),
                                           eed.num_el)) != GS_GOOD)
            break;
         result = GS_BAD;

         // inserimento blocco attributi avvenuto
         if (acdbEntLast(newDABlock) != RTNORM) { GS_ERR_COD = eGSInvEntityOp; break; }

         // aggiorno il n. di aggregazioni "entità di GEOsim" a oggetti grafici
         result = GS_GOOD;
         pEntCls = (C_CLS_PUNT *) lista_cls->get_head();
         while (pEntCls)
         {
            if (eed.save(pEntCls->ent) == GS_BAD) { result = GS_BAD; break; }
            pEntCls = (C_CLS_PUNT*) lista_cls->get_next();
         }
         if (result == GS_BAD) break;
         result = GS_BAD;
      }
      else
         if (id.type == TYPE_TEXT)
            // aggiorno il testo
            if (updtoDA(NewEnt, ColValues, GS_GOOD) == GS_BAD) break;

      // se esistono funzioni di calcolo grafico vengono applicate ora
      if (graph_calc(ColValues) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (dopo l'inserimento)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_INSERT, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
		   if (!InfoList.get_head())
            if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			      break;
         if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                  &ColValues, visib_block, Reason) == GS_BAD) break;
      }

      gsc_addEnt2savess(NewEnt);
      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Inserted entity: key %ld, prj %d, class %d, subclass %d."),
               new_key, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   if (result != GS_GOOD)
   {
      if (InsLink) // se erano stati inseriti i link
      {
         ads_name OrigEnt;

         pEntCls = (C_CLS_PUNT *) lista_cls->get_head(); // cancellazione dei link

         // se la prima entità è diversa da quella di OrigLink
         // significa che era stata cancellata
         OrigLink.GetEnt(OrigEnt);
         if (ads_name_equal(NewEnt, OrigEnt))
            OrigLink.erase();
         
         while (pEntCls)
         {
            Link.erase(pEntCls->ent);
	         eed.clear(pEntCls->ent);

            pEntCls = (C_CLS_PUNT*) lista_cls->get_next();
         }
      }
      if (InsRecord) // se era stato inserito il record
         Ins_Ent_Into_GsDelete(new_key); // inserimento in GS_DELETE

      if (!ads_name_nil(newDABlock)) // se era stato inserito il blocco attributi
         gsc_EraseEnt(newDABlock);
   }

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   tempo2 += (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
   _ftime(&t1);
#endif

   return result;
}


/*********************************************************/
/*.doc C_SIMPLEX::query_data <external> */
/*+                                                                       
  Interroga la scheda dell'entita' della classe semplice.
  Parametri:
  ads_name  entity;	                   Oggetto grafico
  C_RB_LIST &ColValues;                 Lista ((colonna valore)..)
  C_PREPARED_CMD_LIST *pTempOldCmdList; Lista di comandi per lettura dati TEMP (1 elemento)
                                        e OLD (2 elemento) (in caso di letture multiple);
                                        default = NULL.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/  
/*********************************************************/
int C_SIMPLEX::query_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
{                 
   long key;

   // verifico che l'oggetto grafico sia già legato ad un record di questa classe
   if (getKeyValue(entity, &key) == GS_BAD) return GS_BAD;
   return query_data(key, ColValues, pTempOldCmdList);
}
int C_SIMPLEX::query_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
{                    
   if (gsc_check_op(opQryEntity) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   if (pTempOldCmdList)
      return gsc_get_data(*pTempOldCmdList, gs_id, ColValues);
   else
   {
      C_PREPARED_CMD_LIST CmdList;

      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (prepare_data(CmdList) == GS_BAD) return GS_BAD;
      ColValues.remove_all();
      return gsc_get_data(CmdList, gs_id, ColValues);
   }
}


/*********************************************************/
/*.doc C_SIMPLEX::upd_data <external> */
/*+                                                                       
  Aggiorna la scheda dell'entita' della classe con una nuova serie di valori.
  Se la scheda non era stata ancora modificata la funzione la inserisce 
  nella tabella temporanea.
  Parametri:

  ads_name entity;      	 Oggetto grafico
  C_RB_LIST &ColValues;	    Lista (colonna-valore)
  C_SELSET *GlobalSelSet;   Gruppo di selezione globale per la scheda corrente
                            (default = NULL)
  int      Reason;          origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)
  oppure:

  ads_name entity;      	 Oggetto grafico
  C_RB_LIST &ColValues;	    Lista (colonna-valore)
  C_PREPARED_CMD *pTempCmd; Comando per lettura dati TEMP
  C_SELSET *GlobalSelSet;   Gruppo di selezione globale per la scheda corrente
                            (default = NULL)
  int      Reason;          origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B.: Nel caso di superficie con riempimento, questa funzione non riallinea
        il riempimento ad un eventuale cambiamento del perimetro.
-*/  
/*********************************************************/
int C_SIMPLEX::upd_data(ads_name entity, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet,
                        int Reason)
{
   return upd_data(entity, ColValues, NULL, GlobalSelSet, Reason);
}
int C_SIMPLEX::upd_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd, 
                        C_SELSET *GlobalSelSet, int Reason)
{                 
   long KeyValue;

   // Legge il valore chiave
   if (getKeyValue(entity, &KeyValue) == GS_BAD) return GS_BAD;

   return upd_data(KeyValue, ColValues, pTempCmd, GlobalSelSet, Reason);
}
int C_SIMPLEX::upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet, int Reason)
{
   return upd_data(gs_id, ColValues, NULL, GlobalSelSet, Reason);
}
int C_SIMPLEX::upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd, 
                        C_SELSET *GlobalSelSet, int Reason)
{
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();
   C_SELSET       SelSet;
   C_STRING       TempTableRef, UsrFunction;
   int            result = GS_BAD, WhyNotUpd, OldOp;
   C_DBCONNECTION *pConn;

   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(MODIFY);

   do 
   {
      if (GlobalSelSet == NULL)  // se non è passato come parametro, lo ricavo
      {
         int set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;

         if (get_SelSet(gs_id, SelSet) == GS_BAD) break;
         if (is_updateableSS(gs_id, SelSet, &WhyNotUpd, set_lock,
                             TryToExtractPartialEnt) == GS_BAD)
            { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; break; }
      }
      else
      {
         if (GlobalSelSet->copy(SelSet) == GS_BAD) break;
         if (is_updateableSS(gs_id, SelSet, &WhyNotUpd) == GS_BAD)
            { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; break; }
      }
      
      // modifico "key_attrib" con gs_id
      if (ColValues.CdrAssocSubst(ptr_info()->key_attrib.get_name(), gs_id) == GS_BAD)
      { // aggiungo il valore chiave
         presbuf p;

         if ((p = acutBuildList(RTLB, RTLB,
                                RTSTR, ptr_info()->key_attrib.get_name(),
                                RTLONG, gs_id, RTLE, 0)) == NULL) 
            { GS_ERR_COD = eGSOutOfMem; break; }
         ColValues.remove_head();
         ColValues.link_head(p);
      }
         
      // validazione e ricalcolo dati
      if (CalcValidData(ColValues, MODIFY, &SelSet) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (prima della modifica o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                  gs_id, &ColValues, Reason) == GS_BAD) break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_UPDATE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
            if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                     gs_id, &ColValues, Reason) == GS_BAD) break;
            // validazione e ricalcolo dati ("gsc_doActionOnUpdate" potrebbe cambiare qualcosa)
            if (CalcValidData(ColValues, MODIFY, &SelSet) == GS_BAD) break;
         }

      // ricavo tabella temporanea
      if (getTempTableRef(TempTableRef) == GS_BAD) break;
      if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) break;

      // inserisco dati su tabella temporanea
      if (pTempCmd && pTempCmd->pRs != NULL && pTempCmd->pRs.GetInterfacePtr() != NULL)
         result = gsc_DBInsRow(pTempCmd->pRs, ColValues, ONETEST, GS_BAD);
      else
         result = pConn->InsRow(TempTableRef.get_name(), ColValues, ONETEST, GS_BAD);        

      if (result == GS_BAD)
      {
         // è già nel temporaneo
         _RecordsetPtr pRs;
         int           IsRecordsetCloseable = GS_GOOD;

         // se il record era già nel temp non è errore
         if (GS_ERR_COD != eGSIntConstr) break;
         
         // lettura dei dati della classe dal temp
         if (pTempCmd == NULL)
         {
            C_PREPARED_CMD pCmd;

            if (prepare_data(pCmd, TEMP) == GS_BAD) break;
            if (gsc_get_data(pCmd, gs_id, pRs, &IsRecordsetCloseable) == GS_BAD) break;
            // aggiorno il record
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
               { if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
            if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
         }
         else
         {
            if (gsc_get_data(*pTempCmd, gs_id, pRs, &IsRecordsetCloseable) == GS_BAD) break;
            // aggiorno il record
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
               { if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
            if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
         }
      }
      else
         result = GS_BAD;

      if (updtoDA(SelSet, ColValues) == GS_BAD) break;

      // se esistono funzioni di calcolo grafico vengono applicate ora
      if (graph_calc(ColValues, &SelSet) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (dopo la modifica)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                  gs_id, &ColValues, Reason) == GS_BAD)
            break;
      
      // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
      if (gsc_addSS2savess(SelSet) == GS_BAD) break;

      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Updated entity: key %ld, prj %d, class %d, subclass %d."),
               gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_SIMPLEX::aggr_data <external> */
/*+                                                                       
  Aggrega un nuovo aggetto grafico ad una entità già esistente.
  Parametri:
  ads_name newSS;      gruppo di selezione oggetti grafici da aggregare
  ads_name entity;     nome oggetto grafico a cui aggregarsi
  long *gs_id;         codice scheda (out) (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::aggr_data(ads_name newSS, ads_name entity, long *gs_id)
{                 
   int			    result = GS_BAD, WhyNotUpd, is_memb;
   int             set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;
   long            key_val, key_val_aggr, Ndx, n_graphObj, i, Refused = 0;
   C_LINK          Link;
   C_RB_LIST       ColValues;
   C_EED           eed, eed2Aggr;
   ads_name        ent2Aggr, ent;
   C_SELSET        entSS, ss2Aggr, SelSet;
   C_FAMILY_LIST   family_list;
   C_CLS_PUNT_LIST KeyEntList;
   C_CLS_PUNT      *pKeyEnt;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // verifico abilitazione
   if (gsc_check_op(opAggrEntity) == GS_BAD) return GS_BAD;

   if (gsc_getfamily(GS_CURRENT_WRK_SESSION->get_pPrj(), &family_list) == GS_BAD) return GS_BAD;

   do
   {
      // verifico che l'oggetto grafico sia già etichettato "GEOsim"
      if (eed.load(entity) == GS_BAD) { GS_ERR_COD = eGSGEOsimObjNotFound; break; }

      // leggo valore chiave e il gruppo di selezione
      if (get_Key_SelSet(entity, &key_val, entSS) == GS_BAD) break;

      // verifico la possibilità di modificare l'entità
      if (is_updateableSS(key_val, entSS, &WhyNotUpd, set_lock,
                          TryToExtractPartialEnt) == GS_BAD)
         { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; break; }

      // ciclo sugli oggetti da aggregare
      result = GS_GOOD;
      Ndx = 0;
      while (acedSSName(newSS, Ndx++, ent2Aggr) == RTNORM)
      {
         // scarto i blocchi DA
         if (gsc_is_DABlock(ent2Aggr) == GS_GOOD)
            { Refused++; continue; }
         // Se si tratta di superfici scarto i riempimenti
         if (id.type == TYPE_SURFACE && gsc_ishatch(ent2Aggr) == GS_GOOD)
            { Refused++; continue; }
         // controllo che l'oggetto grafico sia compatibile con la classe
         if (check_graph_compatib(ent2Aggr) == GS_BAD)
            { Refused++; continue; }

         // se l'oggetto grafico è già etichettato "GEOsim"
         if (eed2Aggr.load(ent2Aggr) == GS_GOOD)
         {
            // aggregazione non ammessa per oggetti di altre classi
            if (GS_CURRENT_WRK_SESSION->find_class(eed2Aggr.cls, eed2Aggr.sub) != this) continue;

            // leggo valore chiave e il gruppo di selezione
            if (get_Key_SelSet(ent2Aggr, &key_val_aggr, SelSet) == GS_BAD) { result = GS_BAD; break; }
            if (is_updateableSS(key_val_aggr, SelSet, &WhyNotUpd, set_lock,
                                TryToExtractPartialEnt) == GS_BAD)
            {
               Refused++;
               continue;
            }
            // l'entità da aggregare deve essere collegata ad un record diverso da quello
            // dell'entità a cui aggregarsi
            if (key_val_aggr == key_val) { Refused++; continue; }

            // Devo scorrere tutti i Link e vedere se ci sono altre 
            // principali, se non ci sono altre princ. allora devo cancellare in grafica
            // tutte le schede collegate, se no cancello tutte le entita collegate.
            if (SelSet.is_presentGraphicalObject(&n_graphObj) == GS_BAD)
               { result = GS_BAD; break; }

            if (n_graphObj == 1)
            {  // scollego le entita collegate perchè scollegando la princ.
               // devo scollegare tutte le schede
         
               // verifico se la classe che si sta scollegando è legata ad altre
               if (family_list.get_count() > 0)
               {
                  // Controllo se l'entità fa parte di un gruppo
                  if (gsc_is_member_of(this, key_val_aggr, family_list, &is_memb) == GS_BAD)
                     { result = GS_BAD; break; }
                  if (is_memb == GS_GOOD) { Refused++; continue; }
               }
             
               i = 0;
               while (SelSet.entname(i++, ent) == GS_GOOD) // cancellazione dei link
                  if (gsc_ishatch(ent) == GS_GOOD)
                  {
                     // Cancello il riempimento perchè non va aggregato
                     if (gsc_EraseEnt(ent) != GS_GOOD) { result = GS_BAD; break; }
                  }
                  else // aggiungo l'oggetto grafico al gruppo di oggetti da aggregare
                  {
                     if ((pKeyEnt = new C_CLS_PUNT(this, ent, key_val_aggr)) == NULL)
                        { result = GS_BAD; break; }
                     KeyEntList.add_tail(pKeyEnt);

                     if (Link.erase(ent) == GS_BAD) { result = GS_BAD; break; }
                     if (ss2Aggr.add(ent) == GS_BAD) { result = GS_BAD; break; }
                  }
               
               if (result == GS_BAD) break;

               if (gsc_addSS2savess(SelSet) == GS_BAD) { result = GS_BAD; break; }

               if (Ins_Ent_Into_GsDelete(key_val_aggr) == GS_BAD)
                  { result = GS_BAD; break; }
            }
            else // se ci sono più oggetti principali
            {
               SelSet.subtract_ent(ent2Aggr); // tolgo l'entità da SelSet (apparterrà ad un'altra entità)
               eed2Aggr.num_el--;
               if (eed2Aggr.save_selset(SelSet) == GS_BAD) { result = GS_BAD; break; }

               // cancellazione dei link
               if (Link.erase(ent2Aggr) == GS_BAD) { result = GS_BAD; break; }

               // Se si tratta di superfici verifico se ci sono dei riempimenti
               if (id.type == TYPE_SURFACE)
               {
                  TCHAR   tmpHatch[MAX_LEN_HATCHNAME], tmpLayer[MAX_LEN_LAYERNAME];
                  double  tmpScale, tmpRotation;
                  C_COLOR tmpColor;

                  // controllo se c'è almeno un riempimento
                  if (gsc_getInfoHatchSS(SelSet, tmpHatch, &tmpScale, &tmpRotation,
                                         tmpLayer, &tmpColor) == GS_GOOD)
                     // Aggiorno i riempimenti modificando SelSet
                     if (gsc_UpdToDefHatch(SelSet, tmpHatch, tmpScale, gsc_rad2grd(tmpRotation),
                                           &tmpColor, tmpLayer) == GS_BAD)
                        return GS_BAD;
               }

               // Se c'è almeno un attributo calcolato dalla grafica o un riempimento
               // che compone l'entità key_val_aggr
               if (ptr_attrib_list()->is_from_graph())
               {
                  // leggo scheda
                  if (query_data(key_val_aggr, ColValues) == GS_BAD)
                     { result = GS_BAD; break; }
                  // Aggiorno database e video
                  if (upd_data(key_val_aggr, ColValues, &SelSet) == GS_BAD)
                  {
                     eed2Aggr.num_el++; // ripristino i link
                     SelSet.add(ent2Aggr);
                     eed2Aggr.save_selset(SelSet);

                     Link.Set(ent2Aggr, id.code, id.sub_code, key_val_aggr, MODIFY);

                     Refused++;
                     continue;
                  }
               }
               if ((pKeyEnt = new C_CLS_PUNT(this, ent2Aggr, key_val_aggr)) == NULL)
                  { result = GS_BAD; break; }
               KeyEntList.add_tail(pKeyEnt);

               if (ss2Aggr.add(ent2Aggr) == GS_BAD) { result = GS_BAD; break; }
            }
         }
         else
            if (ss2Aggr.add(ent2Aggr) == GS_BAD) { result = GS_BAD; break; }
      }

      if (result == GS_BAD) 
      {
         C_LINK_SET LinkSet;

         LinkSet.RefreshSS(newSS);
         break;
      }
      result = GS_BAD;

      if (ss2Aggr.length() > 0)
      {
         // Collego gli oggetti grafici alla tabella temp
         i = 0;
         while (ss2Aggr.entname(i++, ent) == GS_GOOD)
         {
            if (Link.Set(ent, id.code, id.sub_code, key_val, MODIFY) == GS_BAD)
               break;
         }
         eed.num_el = entSS.length() + ss2Aggr.length(); // nuovo fattore di aggregazione

         // inserisco etichetta "entità di GEOsim" a oggetto grafico
         if (eed.save_selset(ss2Aggr) == GS_BAD) break;

         // aggiorno n. aggregate su tutti gli oggetti
         if (eed.save_aggr(entSS, eed.num_el) == GS_BAD) break;

         // Se si tratta di superfici devo rifare eventuali riempimenti
         if (id.type == TYPE_SURFACE)
         {
            TCHAR   tmpHatch[MAX_LEN_HATCHNAME], tmpLayer[MAX_LEN_LAYERNAME];
            double  tmpScale, tmpRotation;
            C_COLOR tmpColor;

            entSS.add_selset(ss2Aggr);

            // controllo se c'è almeno un riempimento
            if (gsc_getInfoHatchSS(entSS, tmpHatch, &tmpScale, &tmpRotation,
                                   tmpLayer, &tmpColor) == GS_GOOD)
               // Aggiorno i riempimenti modificando SelSet
               if (gsc_UpdToDefHatch(entSS, tmpHatch, tmpScale, gsc_rad2grd(tmpRotation),
                                     &tmpColor, tmpLayer) == GS_BAD)
                  return GS_BAD;
         }

         // ricalcolo, aggiorno database e video
         if (query_data(key_val, ColValues) == GS_BAD) break;  // leggo scheda a cui aggregarsi
         if (upd_data(key_val, ColValues) == GS_BAD)
         {
            C_LINK_SET LinkSS;

            // devo riportare tutti gli oggetti da aggregare com'erano prima
            pKeyEnt = (C_CLS_PUNT *) KeyEntList.get_head();
            while (pKeyEnt)
            {
               Link.Set(pKeyEnt->ent, id.code, id.sub_code, pKeyEnt->gs_id, MODIFY);
               pKeyEnt = (C_CLS_PUNT *) pKeyEnt->get_next();
            }

            pKeyEnt = (C_CLS_PUNT *) KeyEntList.get_head();
            while (pKeyEnt)
            {
               LinkSS.GetSS(id.code, id.sub_code, pKeyEnt->gs_id, SelSet);
               // aggiorno n. aggregate su tutti gli oggetti
               if (eed.save_aggr(SelSet, (int) SelSet.length()) == GS_BAD) break;
               pKeyEnt = (C_CLS_PUNT *) pKeyEnt->get_next();
            }

            break;
         }
      }
      result = GS_GOOD;
   }
   while (0);
   
   if (result == GS_GOOD)
   {
      if (gs_id != NULL) *gs_id = key_val; // codice record
      setModified(GS_GOOD); // classe modificata
   }
   
   acutPrintf(gsc_msg(227), ss2Aggr.length(),  Refused); // "\nOggetti grafici elaborati %ld, scartati %ld."

   return result;
}


/*********************************************************/
/*.doc C_SIMPLEX::disaggr_data                <external> */
/*+                                                                       
  Disgrega un oggetto grafico esistente.
  Parametri:
  ads_name entity;     oggetto grafico da disgregare
  long *gs_id;         codice scheda (out) (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::disaggr_data(ads_name entity, long *gs_id)
{                 
   int	    result = GS_BAD, Inserted = FALSE, LinkErased = FALSE, WhyNotUpd;
   int       set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;
   long      key_val_aggr, n_graphObj;
   C_LINK    Link;
   C_RB_LIST ColValues;
   C_EED     eed, eedAggr;
   C_SELSET  entSS;
   ads_name  PrevEntity;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // verifico abilitazione
   if (gsc_check_op(opAggrEntity) == GS_BAD) return GS_BAD;

   // non si può disaggregare un blocco DA
   if (gsc_is_DABlock(entity) == GS_GOOD)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // verifico che l'oggetto grafico sia già etichettato "GEOsim"
   if (eedAggr.load(entity) == GS_BAD) { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }

   // leggo valore chiave e il gruppo di selezione
   if (get_Key_SelSet(entity, &key_val_aggr, entSS) == GS_BAD) return GS_BAD;
   
   // verifico la possibilità di modificare l'entità
   if (is_updateableSS(key_val_aggr, entSS, &WhyNotUpd, set_lock,
                       TryToExtractPartialEnt) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Devo scorrere tutti i Link e vedere se ci sono altre 
   // principali, se non ci sono altre princ. allora non è possibile disgregare
   if (entSS.is_presentGraphicalObject(&n_graphObj) == GS_BAD) return GS_BAD;

   if (n_graphObj == 1) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // leggo scheda
   if (query_data(key_val_aggr, ColValues) == GS_BAD) return GS_BAD;

   ads_name_set(entity, PrevEntity);
   eed = eedAggr;

   do
   {
      // Cancellazione link a database e EED
      if (Link.erase(entity) == GS_BAD || eed.clear() == GS_BAD) break;
      entSS.subtract_ent(entity);
      eedAggr.num_el--;
      LinkErased = TRUE;

      // Se si tratta di superfici devo rifare eventuali riempimenti
      if (id.type == TYPE_SURFACE && gsc_strcmp(fas.hatch, GS_EMPTYSTR) != 0)
         if (gsc_UpdToDefHatch(entSS, fas.hatch, fas.hatch_scale, fas.hatch_rotation,
                               &fas.hatch_color, fas.hatch_layer) == GS_BAD)
            break;

      if (eedAggr.save_selset(entSS) == GS_BAD) break;

      // Aggiorno database e video
      if (upd_data(key_val_aggr, ColValues, &entSS) == GS_BAD) break;

      // Inserisco nuovo oggetto
      if (ins_data(entity, ColValues, gs_id) == GS_BAD) break;
      Inserted = TRUE;

      result = GS_GOOD;
   }
   while (0);
   
   if (result == GS_BAD)
   {
      if (LinkErased)
      {
         if (!ads_name_equal(entity, PrevEntity))
         { // ins_data ha inserito un nuovo oggetto
            if (gsc_IsErasedEnt(PrevEntity) == GS_GOOD)
               gsc_UnEraseEnt(PrevEntity); // la rispristino

            ads_name_set(PrevEntity, entity);
         }            
         eedAggr.num_el++; // ripristino le aggregazioni
         eedAggr.save(entity);
         eedAggr.save_selset(entSS);

         Link.Set(entity, id.code, id.sub_code, key_val_aggr, MODIFY);

         // Ri-aggiorno database e video
         entSS.add(entity);
         upd_data(key_val_aggr, ColValues, &entSS);
      }
   }

   return result;
}


/*********************************************************/
/*.doc (new 2) C_SIMPLEX::is_updateable <internal> */
/*+                                                                       
  Verifica la possibilità di modificare l'oggetto.
  Se la variabile "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento 
  delle entità nel salvataggio) questa funzione non effettua alcun controllo 
  e restituisce sempre GS_GOOD.
  Parametri:
  ads_name  entity;           Oggetto grafico
  int *WhyNot;                Codice rappresentante il motivo per cui non è aggiornabile
                              (default = NULL)
  int set_lock;               flag se = GS_GOOD la funzione prova a bloccare l'entità
                              (se questa era risultata aggiornabile) default = GS_GOOD
  int TryToExtractPartialEnt; Se = GS_GOOD e l'entità non risulta aggiornabile perchè
                              parzialmente estratta la funzione effettua l'estrazione
                              di tutte le istanze grafiche dell'entità (default GS_BAD).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::is_updateable(ads_name entity, int *WhyNot, int set_lock, int TryToExtractPartialEnt)
{                 
   long     key;
   C_SELSET entSS;

   // leggo valore chiave e il gruppo di selezione
   if (get_Key_SelSet(entity, &key, entSS) == GS_BAD) return GS_BAD;
   return is_updateableSS(key, entSS, WhyNot, set_lock, TryToExtractPartialEnt);
}
int C_SIMPLEX::is_updateable(long gs_id, int *WhyNot, int set_lock,
                             int TryToExtractPartialEnt)
{                 
   C_SELSET entSS;

   if (get_SelSet(gs_id, entSS) == GS_BAD) return GS_BAD;
   return is_updateableSS(gs_id, entSS, WhyNot, set_lock, TryToExtractPartialEnt);
}


/*********************************************************/
/*.doc (new 2) C_SIMPLEX::is_updateable <internal> */
/*+                                                                       
  Verifica la possibilità di modificare un'entità.
  Parametri:
  long gs_id;                 Codice entità
  C_SELSET &SelSet;           Gruppo di selezione degli oggetti in grafica 
                              appartenenti alla stessa entità
  int *WhyNot;                Flag rappresentante il motivo per cui non è aggiornabile
                              (default = NULL)
  int set_lock;               flag se = GS_GOOD la funzione prova a bloccare l'entità
                              (se questa era risultata aggiornabile) (default = GS_GOOD)
  int TryToExtractPartialEnt; Se = GS_GOOD e l'entità non risulta aggiornabile perchè
                              parzialmente estratta la funzione effettua l'estrazione
                              di tutte le istanze grafiche dell'entità MODIFICANDO
                              IL PARAMETRO <SelSet> (default GS_BAD).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::is_updateableSS(long gs_id, C_SELSET &SelSet, int *WhyNot, int set_lock,
                               int TryToExtractPartialEnt)
{
   int      WhyNotUpdateable, Result;
   ads_name ss;

   SelSet.get_selection(ss);

   Result = is_updateableSS(gs_id, ss, &WhyNotUpdateable, set_lock);

   // Se l'entità non è modificabile perchè estratta parzialmente
   // e si vuole tentare un'estrazione totale dell'entità
   if (Result == GS_BAD && WhyNotUpdateable == eGSPartialEntExtract && TryToExtractPartialEnt == GS_GOOD)
      // Provo ad estrarre totalmente le istanze grafiche dell'entità
      if (entExtract(gs_id) == GS_GOOD && get_SelSet(gs_id, SelSet) == GS_GOOD)
      {
         SelSet.get_selection(ss);
         Result = is_updateableSS(gs_id, ss, &WhyNotUpdateable, set_lock);
      }

   if (WhyNot) *WhyNot = WhyNotUpdateable;

   return Result;
}
// il SelSet deve appartenere alla stessa entità GEosim
int C_SIMPLEX::is_updateableSS(long gs_id, ads_name SelSet, int *WhyNot, int set_lock)
{
   C_EED    eed;
   int      result = GS_GOOD, islocked = GS_BAD, res = GS_BAD;
   ads_name entity;
   C_LOCK   Lock;
   long     Qty;

   if (WhyNot) *WhyNot = eGSUnknown;

   // verifico abilitazione
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(WhyNot) == GS_BAD) return GS_BAD;
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      if (WhyNot) *WhyNot = GS_ERR_COD;
      return GS_BAD;
   }

   // se entità nuova salto i controlli
   if (is_NewEntity(gs_id) == GS_GOOD) return GS_GOOD;

   // se non si è abilitati a inserire in GEOsimAppl::SAVE_SS
   // ritorno sempre GS_GOOD perchè l'entità non verrà salvata
   if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD) return GS_GOOD;

   // ricavo il numero di oggetti del gruppo di selezione
   if (ads_sslength(SelSet, &Qty) != RTNORM || Qty <= 0)
   {
      GS_ERR_COD = eGSPartialEntExtract;
      if (WhyNot) *WhyNot = eGSPartialEntExtract;
      return GS_BAD;
   }

   do
   {
      // ricavo l'oggetto grafico collegato
      if (acedSSName(SelSet, 0, entity) != RTNORM)
         { GS_ERR_COD = eGSInvalidSelectionSet; result = GS_BAD; break; }
      // carico i dati estesi di GEOsim
      if (eed.load(entity) == GS_BAD) 
         { GS_ERR_COD = eGSGEOsimObjNotFound; result = GS_BAD; break; }
      // verifico se ho tutte le aggregate
      if (Qty < eed.num_el)
      {
         if (WhyNot) *WhyNot = eGSPartialEntExtract;
         result = GS_BAD; 
         break;
      }

      // setto le informazioni necessarie a C_LOCK
      Lock.set_ClsSubId(id.code, 0, gs_id);
      // se devo bloccare l'oggetto
      if (set_lock == GS_GOOD)
      {
         if (Lock.set_locked(&res) == GS_BAD) { result = GS_BAD; break; }
         if (res == GS_BAD)
         {
            if (WhyNot) *WhyNot = eGSObjectIsLockedByAnotherUser;
            result = GS_BAD; 
            break;
         }
      }
      else
      {
         // verifico se l' oggetto è libero (controllo solo su altre sessioni)
         if (Lock.is_locked(&islocked, GS_GOOD) == GS_BAD)
            { result = GS_BAD; break; }
         // se è bloccato
         if (islocked == TEMPORARY_LOCKED || islocked == PERMANENT_LOCKED)
         {
            if (WhyNot) *WhyNot = eGSObjectIsLockedByAnotherUser;
            result = GS_BAD;
            break;
         }
      }
   }
   while (0);

   return result;
}


/*********************************************************/
/*.doc C_SIMPLEX::is_NewEntity                <external> */
/*+                                                                       
  Funzione che verifica se una entità di GEOsim è nuova. 
  Parametri:
  ads_name entity;   Entità grafica
  oppure
  long Key;          Codice entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::is_NewEntity(ads_name entity)
{
   long Key;

   // leggo valore chiave e il gruppo di selezione
   if (getKeyValue(entity, &Key) == GS_BAD) return GS_BAD;
   return is_NewEntity(Key);
}
int C_SIMPLEX::is_NewEntity(long Key)
{
   // Se il codice è negativo si tratta di un'entità nuova
   // inserita nella sessione di lavoro
   return (Key < 0) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc C_SIMPLEX::import_data                 <external> */
/*+                                                                       
  Funzione che importa un serie di oggetti nella classe. 
  Parametri:
  C_SELSET &objGraph;
  C_SELSET &objDA;
  int      loadGSent;        Flag che determina se importare gli oggetti di GEOsim.
  long BitForChangeToNewFAS; Opzionale, Flag a bit per indicare quali caratteristiche
                             grafiche cambiare
  C_FAS *pNewFas;            I valori delle caratteristiche grafiche da cambiare (se = NULL
                             verranno considerati i valori di default)
  C_RB_LIST &ColValues;  
  C_CLASS *pObj;
  _RecordsetPtr pInsRs;
  int *objLocked;

  Restituisce il numero di oggetti grafici importati. 
-*/  
/*********************************************************/
long C_SIMPLEX::import_data(C_SELSET &objGraph, C_SELSET &objDA, int loadGSent,
                            long BitForChangeToNewFAS, C_FAS *pNewFas, C_RB_LIST &ColValues, 
                            C_CLASS *pObj, _RecordsetPtr pInsRs, int *objLocked)
{
   C_SELSET toDel;
   long     ObjLoaded = 0, BlkLoaded = 0;
   int      OldOp;

   if (objGraph.length() <= 0) return 0;
   if (import_data(objGraph, ColValues, toDel, pInsRs, pObj, loadGSent, 
                   BitForChangeToNewFAS, pNewFas) == GS_BAD) 
   {
      if (GS_ERR_COD == eGSObjectIsLockedByAnotherUser)
         *objLocked = GS_GOOD;
      else
         gsc_print_error();
      return 0;
   }
   // Conto quante entità sono state caricate
   ObjLoaded = objGraph.length();

   // Funzione che carica se necessario i blocchi attributi.
   BlkLoaded = gsc_insBlockAttrib(this, objGraph, objDA, 
                                  BitForChangeToNewFAS, pNewFas,
                                  ColValues);

   // A questo punto devo cancellare le varie entità di partenza
   if (pObj == NULL)
   {
      // Siamo nel caso di entità non di GEOsim che possono avere blocchi DA
      objDA.Erase();
      // Devo cancellare gli oggetti di partenza
      toDel.Erase();
   }
   else
   {
      // Siamo nel caso di entità Di GEOsim e quindi devo cancellare
      // le entità di partenza, richiamo la erase_data
      long     i = 0;
      ads_name ent;

      while (toDel.entname(i++, ent) == GS_GOOD)
         pObj->erase_data(ent);
   }

   // Setto l'operazione corrente memorizzando il valore precedente
   OldOp = set_GS_CURRENT_OPERATION(INSERT);
   // Ricalcolo l'eventuali funzioni di calcolo dalla grafica
   int ressss = graph_calc(ColValues);
   // Setto l'operazione corrente a quella precedente
   set_GS_CURRENT_OPERATION(OldOp);

   return BlkLoaded + ObjLoaded;
}


/*********************************************************/
/*.doc C_SIMPLEX::import_data                 <external> */
/*+                                                                       
  Funzione che importa un oggetto nella classe. 
  Parametri:
   C_SELSET   &objGraph    Oggetto da importare.
   C_RB_LIST  &ColValue    Resbuf contenente il record da inserire.
                           Questo è gia stato adattato, o è quello 
                           di default oppure ho filtrato i campi.
   C_SELSET   &ToDel       Gruppo di selezione degli oggetti da cancellare.
   _RecordsetPtr pInsRs
   C_CLASS    *pObj        Eventuale puntatore alla classe (... se di GEOsim).
   int         loadGSent   Flag che determina se importare gli oggetti di GEOsim.
   long BitForChangeToNewFAS; Opzionale, Flag a bit per indicare quali caratteristiche
                              grafiche cambiare (default = GSNoneSetting)
   C_FAS *pNewFas;            Opzionale, i valori delle caratteristiche grafiche da cambiare 
                              (se = NULL si intendono i valori di default della classe)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::import_data(C_SELSET &objGraph, C_RB_LIST &ColValue, C_SELSET &ToDel, _RecordsetPtr pInsRs,
                           C_CLASS *pObj, int loadGSent,
                           long BitForChangeToNewFAS, C_FAS *pNewFas)
{
   C_LINK        Link;
   C_EED         eed ;
   C_FAMILY_LIST family_list;
   C_SELSET      objToImporting, objCopied;
   C_STRING      nObjData;
   ads_name      ent, ss_objToImporting;
   long          Key, gs_id, i;
   int           result = GS_BAD, WhyNotUpd;
   int           rescmplx = GS_BAD, prj;

   // Verifico l'abilitazione dell'utente;
   if (gsc_check_op(opImportEntity) == GS_BAD) return GS_BAD;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   prj = GS_CURRENT_WRK_SESSION->get_PrjId();

   // Controllo la compatibilità degli oggetti con la classe e nel 
   // caso di incompatibilità li elimino.
   //if (check_SS_graph_compatib_on_import(objGraph, GS_GOOD) == GS_BAD) return GS_BAD;

   //if (objGraph.length() == 0) return GS_BAD;

   // I seguenti controlli hanno senso solo per oggetti di GEOsim (pObj != NULL)
   if (pObj != NULL)
   {
      // Per prima cosa se siamo nel caso di oggetti di GEOsim ed il
      // flag di importazione di detti oggetti non è selezionato
      // segnalo un errore
      if (pObj && loadGSent == GS_BAD) { GS_ERR_COD = eGSGEOsimObjFound; return GS_BAD; }

      // Verifico che non si cerchi di importare un'oggetto di una classe
      // nella stessa classe, in tal caso segnalo l'errore
      if (this == pObj) return GS_BAD; 

      // Verifico anche che l'entità di partenza non faccia parte 
      // di un gruppo, prima ricavo il GS_ID
      // Solo se la classe di partenza non è spaghetti
      // Ricavo il primo elemento del gruppo di selezione per poter verificarlo
      if (objGraph.entname(0, ent) != GS_GOOD) return GS_BAD;

      if (pObj->get_category() != CAT_SPAGHETTI) 
      {
         if (pObj->getKeyValue(ent, &Key) == GS_BAD) return GS_BAD;
         if (gsc_getfamily(GS_CURRENT_WRK_SESSION->get_pPrj(), &family_list) == GS_BAD) return GS_BAD;
         if (family_list.get_count() > 0)
         {
            // Controllo se l'entità fa parte di un gruppo
            if (gsc_is_member_of(pObj, Key, family_list, &rescmplx) == GS_BAD) return GS_BAD;
            if (rescmplx == GS_GOOD) { GS_ERR_COD = eGSMemberOfGroup; return GS_BAD; }
         }
      }
      // A questo punto verifico che l'elemento sia modificabile e lo blocco
      GS_ERR_COD = eGSUnknown;
      if (pObj->is_updateable(ent, &WhyNotUpd, GS_GOOD, GS_GOOD) == GS_BAD)
         { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }
   }

   // leggo quali caratteristiche grafiche si possono modificare:
   long FlagFas = GSNoneSetting;
   FlagFas = what_is_graph_updateable();

   // Effettuo la copia degli oggetti nel caso in cui:
   // 1) si trattano oggetti non di GEOsim che siano ricavati 
   //    da un attach ad altri disegni.

   // Si dovranno cancellare gli oggetti appartenenti al gruppo <objGraph>
   // nel caso in cui siano oggetti:
   // 1) non di GEOsim che siano ricavati da un attach ad altri disegni.
   // 2) blocchi con attributi importati in una classe semplice di tipo NODO quando
   //    si vuole convertire il blocco con quello di default
   // 3) punti autocad importati in una classe semplice di tipo NODO
   i = 0;
   while (objGraph.entname(i++, ent) == GS_GOOD)
   {
      // Se si vogliono importare dei blocchi con attributi
      // in una classe semplice di tipo NODO quando si vuole convertire
      // il blocco con quello di default o quando il blocco è calcolato
      if (((BitForChangeToNewFAS & GSBlockNameSetting) || !(FlagFas & GSBlockNameSetting)) &&
          id.type == TYPE_NODE && gsc_CountAttributes(ent) > 0)
      {  
         // Facendo così evito che ci siano blocchi con attributi
         // sostituiti con i blocchi di GEOsim (che non hanno attributi)
         // In questo caso vengono perdute le informazioni EED, OD e ASE
         // eventualmente associate all'oggetto
         ads_point InsPt;
         ads_name  NewBlock;
         double    Rot;

         // L'oggetto di partenza sarà cancellato
         ToDel.add(ent);

         // Inserisco il blocco di default al posto del punto
         if (gsc_get_firstPoint(ent, InsPt) != GS_GOOD ||
             gsc_get_rotation(ent, &Rot) != GS_GOOD ||
             gsc_insert_block(ptr_fas()->block, InsPt, ptr_fas()->block_scale,
                              ptr_fas()->block_scale, gsc_rad2grd(Rot)) != GS_GOOD)
            continue;
        
         acdbEntLast(NewBlock);
         objToImporting.add(NewBlock);
         objCopied.add(NewBlock);
      }
      else
      // Accetto l'eccezione in cui si vogliono importare dei points
      // in una classe semplice di tipo NODO
      if (id.type == TYPE_NODE && gsc_isPoint(ent) == GS_GOOD)
      {
         // In questo caso vengono perdute le informazioni EED, OD e ASE
         // eventualmente associate all'oggetto
         ads_point InsPt;
         ads_name  NewBlock;

         // L'oggetto di partenza sarà cancellato
         ToDel.add(ent);

         // Inserisco il blocco di default al posto del punto
         if (gsc_get_firstPoint(ent, InsPt) != GS_GOOD ||
             gsc_insert_block(ptr_fas()->block, InsPt, ptr_fas()->block_scale,
                              ptr_fas()->block_scale, ptr_fas()->rotation) != GS_GOOD)
            continue;
        
         acdbEntLast(NewBlock);
         objToImporting.add(NewBlock);
         objCopied.add(NewBlock);
      }
      else
      // Se gli oggetti da importare sono di una classe di GEOsim
      if (pObj != NULL) // Elimino i link di GEOsim, e poi le EED   
      { 
         // L'oggetto di partenza sarà cancellato
         ToDel.add(ent);

         // Eseguo la copia dell'oggetto da importare 
         if (gsc_DeepClone2ModSpace(ent) != GS_GOOD) return GS_BAD;
         acdbEntLast(ent);
         objToImporting.add(ent);
         objCopied.add(ent);

         // Se si tratta di entità di GEOsim e non sono spaghetti
         // Elimino il collegamento a DB e a OD
         if (pObj->get_category() != CAT_SPAGHETTI) 
         {
            if (Link.erase(ent) == GS_BAD) continue;
         }
         else
         {  // Elimino il collegamento a OD
            gsc_getODTableName(prj, pObj->ptr_id()->code, pObj->ptr_id()->sub_code,
                              nObjData);
            if (gsc_delID2ODTable(ent, nObjData) == GS_BAD) continue;
         }
         // Eliminazione EED
         if (eed.load(ent) == GS_GOOD) eed.clear();
      }
      else // non si tratta di entità di GEOsim
      {
         C_RB_LIST DwgAndHandle;

         // se è una entità legata ad un disegno la copio
         if ((DwgAndHandle << gsc_ade_qrygetdwgandhandle(ent)) != NULL)
         { 
            // L'oggetto di partenza sarà cancellato
            ToDel.add(ent);

            // Eseguo la copia dell'oggetto da importare 
            if (gsc_DeepClone2ModSpace(ent) != GS_GOOD) return GS_BAD;
            acdbEntLast(ent);
            objCopied.add(ent);
         }
         objToImporting.add(ent);
      }
   }

   // Mi ricavo il gruppo di selezione perchè la ins_data vuole un ads_name
   objToImporting.get_selection(ss_objToImporting);
   objToImporting.ReleaseAllAtDistruction(GS_BAD);

   result = GS_BAD;
   do
   {
      // Setto l'operazione corrente
      GS_CURRENT_OPERATION = INSERT;

      // Nel caso si tratti di un importazione di testi
      // mi ricavo il testo grafico del primo elemento del
      // gruppo di selezione
      if (id.category == CAT_SIMPLEX && id.type == TYPE_TEXT)
      {
         C_STRING TextValue;

         if (acedSSName(ss_objToImporting, 0, ent) != RTNORM) break;
         // Leggo il valore del testo
         if (gsc_getInfoText(ent, &TextValue) == GS_BAD) break;
         // Modifico il campo TEXT con valore letto dalla grafica
         if (ColValue.CdrAssocSubst(ptr_attrib_list()->getFirstVisibleAttrib()->get_name(), 
                                    TextValue.get_name()) == GS_BAD) 
            break;
      }

      // Se è una superficie e ha impostato un riempimento
      if (get_type() == TYPE_SURFACE)
      {
         C_STRING Hatch;

         if (pNewFas) // se bisogna non si deve usare FAS di default
            { if (BitForChangeToNewFAS & GSHatchNameSetting) Hatch = pNewFas->hatch; }
         else Hatch = ptr_fas()->hatch;

         if (Hatch.len() > 0)
         {
            C_SELSET HatchSS;
            C_FAS    *pFas = (pNewFas) ? pNewFas : ptr_fas();

            if (gsc_setHatchSS(ss_objToImporting, Hatch.get_name(), pFas->hatch_scale,
                               pFas->hatch_rotation, &pFas->hatch_color, pFas->hatch_layer,
                               &HatchSS) == GS_GOOD)
            {
               i = 0;
               while (HatchSS.entname(i++, ent) == GS_GOOD)
                  acedSSAdd(ent, ss_objToImporting, ss_objToImporting);
            }
         }
      }

      // Eseguo l'operazione di importazione
      if (ins_data(ss_objToImporting, ColValue, &gs_id, INVISIBLE, UNKNOWN_MOD, pInsRs) == GS_BAD)
         break;

      result = GS_GOOD;
   }
   while (0);

   if (result == GS_BAD) // Se si è verificato un errore
   {
      // gli oggetti copiati devono essere cancellati
      objCopied.Erase();
      // Rilascio il gruppo di selezione
      if (!ads_name_nil(ss_objToImporting)) acedSSFree(ss_objToImporting);
   }
   else
   {
      // Se il flag di applicazione della FAS è attivo allora 
      // ricavo quali sono le caratteristiche grafiche modificabili 
      // della classe tenendo conto di eventuali funzioni di modifica FAS
      if (BitForChangeToNewFAS != GSNoneSetting)
      {  
         long  BitForChangeFAS;
         C_FAS *pFas = (pNewFas) ? pNewFas : ptr_fas();

         BitForChangeFAS = what_is_graph_updateable();
         // Se si poteva variare la rotazione o l'elevazione, queste vengono inibite perchè si
	      // devono variare solo caratteristiche "estetiche" e non geometriche degli oggetti
         if (BitForChangeFAS & GSRotationSetting) BitForChangeFAS -= GSRotationSetting;
         if (BitForChangeFAS & GSElevationSetting) BitForChangeFAS -= GSElevationSetting;
         
         BitForChangeToNewFAS = BitForChangeToNewFAS & BitForChangeFAS;

         // Modifico l'entità con la FAS
         gsc_modifyEntToFas(ss_objToImporting, pFas, BitForChangeToNewFAS);
         // Se è una superficie e devo modificare la fas inserisco riempimento
         if (get_type() == TYPE_SURFACE && 
             (BitForChangeToNewFAS & GSHatchNameSetting) && gsc_strcmp(pFas->hatch, GS_EMPTYSTR) != 0)
            gsc_hatch(ss_objToImporting, pFas->hatch, pFas->hatch_scale, 
                      pFas->hatch_rotation, &(pFas->hatch_color), pFas->hatch_layer);
      }

      // A questo punto scarico gli oggetti importati nel gruppo di 
      // selezione passato come parametro.
      objGraph << ss_objToImporting;
   }

   // Setto l'operazione corrente
   GS_CURRENT_OPERATION = NONE;

   return result;
}                         


/*********************************************************/
/*.doc C_SIMPLEX::erase_data(ads_name Entity) <external> */
/*+                                                                       
  Cancella un oggetto grafico dell'entita della classe.
  Parametri:
  ads_name entity;  Oggetto da cancellare
  int      Reason;  origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                    RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                    (default = UNKNOWN_MOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::erase_data(ads_name Entity, int Reason)
{
   long           gs_id, graphObj;
   int            result = GS_BAD, last_echo, WhyNotUpd, OldOp;
   int            set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;
   C_EED          eed;
   C_SELSET       SelSet;
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();
   C_RB_LIST      ColValues;
   C_DBCONNECTION *pConn;
   bool           EraseEnt;
   
   // verifico l'abilitazione dell'utente;
   if (gsc_check_op(opDelEntity) == GS_BAD) return GS_BAD;

   // leggo valore chiave e il gruppo di selezione
   if (get_Key_SelSet(Entity, &gs_id, SelSet) == GS_BAD) return GS_BAD;
   // verifico che l'elemento sia cancellabile 
   if (is_updateableSS(gs_id, SelSet, &WhyNotUpd, set_lock,
                       TryToExtractPartialEnt) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   eed.cls    = id.code;
   eed.sub    = id.sub_code;
   eed.num_el = SelSet.length(); // numero di oggetti grafici
   eed.num_el--;   // tolgo 1 a num_elem

   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   int IsTransactionSupported = pConn->BeginTrans();

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(ERASE);

   do
   {
      if (gsc_set_echo(0, &last_echo) == GS_BAD) break;

      // Se non si tratta di un blocco di attributi
      if (gsc_is_DABlock(Entity) == GS_GOOD) EraseEnt = false;
      else
      // Se si tratta di un riempimento di superficie
      if (id.type == TYPE_SURFACE && gsc_ishatch(Entity) == GS_GOOD) EraseEnt = false;
      else
      {
         // Se è una principale devo scorrere tutti i Link e vedere se ci sono altre 
         // principali, se non ci sono altre princ. allora devo cancellare in grafica
         // tutte le schede collegate, se no cancello tutte le entita collegate.
         if (SelSet.is_presentGraphicalObject(&graphObj) == GS_BAD) break;
         // Se era l'ultimo oggetto principale
         EraseEnt = (graphObj == 1) ? true : false;
      }

      if (EraseEnt) // cancellazione dell'entità
      {  // cancello le entita collegate perchè cancellando la princ.
         // devo cancellare tutte le schede collegate
         C_FAMILY_LIST family_list;

         // eventuale chiamata ad una funzione utente esterna (prima della cancellazione o in
         // completa sostituzione della funzione di GEOsim)
         C_STRING UsrFunction;
         if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_ERASE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
            if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
               break;
            result = GS_GOOD;
            break;
         }
         else
            if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_ERASE, UsrFunction) == GS_GOOD &&
                UsrFunction.len() > 0)
               if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
                  break;
         
         // verifico se la classe che si sta cancellando è legata ad altre
         if (gsc_getfamily(GS_CURRENT_WRK_SESSION->get_pPrj(), &family_list) == GS_BAD) break;
         if (family_list.get_count() > 0)
         {
            // Controllo se l'entità fa parte di un gruppo
            if (gsc_is_member_of(this, gs_id, family_list, &result) == GS_BAD) break;
            if (result == GS_GOOD)
               { result = GS_BAD; GS_ERR_COD = eGSMemberOfGroup; break; }
         }

         if (gsc_addSS2savess(SelSet) == GS_BAD) break;
         if (SelSet.Erase() != GS_GOOD) break;

         if (Ins_Ent_Into_GsDelete(gs_id) == GS_BAD)
         {
            SelSet.UnErase(); // ripristino oggetto
            break;
         }
         
         // eventuale chiamata ad una funzione utente esterna (dopo la cancellazione)
         if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_ERASE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
            if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
               result = GS_BAD;

         // Notifico in file log
         TCHAR Msg[MAX_LEN_MSG];
         swprintf(Msg, MAX_LEN_MSG, _T("Erased entity: key %ld, prj %d, class %d, subclass %d."),
                  gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
         gsc_write_log(Msg);
      }
      else
      {
         // devo diminuire di 1 il fattore di aggregazione di
         // tutti gli oggetti collegati
         // aggiungendo in GEOsimAppl::SAVE_SS per salvataggio (vedi C_EED::save)
         if (eed.save_aggr(SelSet, eed.num_el) == GS_BAD) break;

         if (gsc_EraseEnt(Entity) != GS_GOOD) break;
         SelSet.subtract_ent(Entity);

         C_PREPARED_CMD_LIST CmdList;

         // Preparo i comandi di lettura dei dati della classe dal temp/old
         if (prepare_data(CmdList) == GS_BAD) break;

         // leggo scheda
         if (query_data(gs_id, ColValues, &CmdList) == GS_GOOD)
         {  // Aggiorno database e video
            if (upd_data(gs_id, ColValues, (C_PREPARED_CMD *) CmdList.get_head(), &SelSet) == GS_BAD) 
            {
               acdbEntDel(Entity); // ripristino oggetto
               break;
            }
         }
         else // se non c'è più la scheda cancello lo stesso
            if (GS_ERR_COD != eGSInvalidKey)
            {
               acdbEntDel(Entity); // ripristino oggetto
               break;
            }
      }

      result = GS_GOOD;
   }
   while (0);

   if (gsc_set_echo(last_echo) == GS_BAD) result = GS_BAD;

   if (result != GS_GOOD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); }
   else
   {
      if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();
      setModified(GS_GOOD); // classe modificata
   }

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_SIMPLEX::erase_data                  <external> */
/*+                                                                       
  Cancella un'entità della classe.
  Parametri:
  long     Key;     Oggetto da cancellare
  int      Reason;  origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                    RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                    (default = UNKNOWN_MOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::erase_data(long Key, int Reason)
{
   C_SELSET       SelSet;
   int            result = GS_BAD, last_echo, WhyNotUpd, OldOp;
   int            set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;
   long           i = 0;
   C_FAMILY_LIST  family_list;
   C_DBCONNECTION *pConn;
   
   // verifico l'abilitazione dell'utente;
   if (gsc_check_op(opDelEntity) == GS_BAD) return GS_BAD;

   // leggo il gruppo di selezione
   if (get_SelSet(Key, SelSet) == GS_BAD) return GS_BAD;
   // verifico che l'elemento sia cancellabile 
   if (is_updateableSS(Key, SelSet, &WhyNotUpd, set_lock,
                       TryToExtractPartialEnt) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   int IsTransactionSupported = pConn->BeginTrans();

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(ERASE);

   do
   {
      if (gsc_set_echo(0, &last_echo) == GS_BAD) break;

      // cancello le entita collegate perchè cancellando la princ.
      // devo cancellare tutte le schede collegate

      // eventuale chiamata ad una funzione utente esterna (prima della cancellazione o in
      // completa sostituzione della funzione di GEOsim)
      C_STRING UsrFunction;
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_ERASE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
         if (gsc_doActionOnErase(UsrFunction.get_name(), id.code, id.sub_code, Key, Reason) == GS_BAD)
            break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_ERASE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
            if (gsc_doActionOnErase(UsrFunction.get_name(), id.code, id.sub_code, Key, Reason) == GS_BAD)
               break;
            
      // verifico se la classe che si sta cancellando è legata ad altre
      if (gsc_getfamily(GS_CURRENT_WRK_SESSION->get_pPrj(), &family_list) == GS_BAD) break;
      if (family_list.get_count() > 0)
      {
         // Controllo se l'entità fa parte di un gruppo
         if (gsc_is_member_of(this, Key, family_list, &result) == GS_BAD) break;
         if (result == GS_GOOD)
            { result = GS_BAD; GS_ERR_COD = eGSMemberOfGroup; break; }
      }

      if (gsc_addSS2savess(SelSet) == GS_BAD) break;
      if (SelSet.Erase() != GS_GOOD) break;
      if (Ins_Ent_Into_GsDelete(Key) == GS_BAD)
      {
         SelSet.UnErase(); // ripristino oggetto
         break;
      }
      
      // eventuale chiamata ad una funzione utente esterna (dopo la cancellazione)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_ERASE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnErase(UsrFunction.get_name(), id.code, id.sub_code, Key, Reason) == GS_BAD)
            result = GS_BAD;

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Erased entity: key %ld, prj %d, class %d, subclass %d."),
               Key, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   if (gsc_set_echo(last_echo) == GS_BAD) result = GS_BAD;

   if (result != GS_GOOD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); }
   else
   {
      if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();
      setModified(GS_GOOD); // classe modificata
   }

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/**********************************************************/
/*.doc C_SIMPLEX::update_data(ads_name Entity) <external> */
/*+                                                                       
  Sposta un entita della classe.
  Parametri:
  ads_name entity;      	Oggetto da spostato
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
// USARE ALIGN
int C_SIMPLEX::update_data(ads_name Entity)
{
   C_RB_LIST ColValues;
   C_SELSET  SelSet;

   // Leggo i dati dal database
   if (query_data(Entity, ColValues) == GS_BAD) return GS_BAD;
   // ricavo i collegamenti con la grafica
   if (get_SelSet(ColValues, SelSet) == GS_BAD) return GS_BAD;
   // Aggiorno i dati con eventuali attributi visibili
   if (updfromDA(ColValues, Entity, &SelSet) == GS_BAD) return GS_BAD;

   // se è superficie
   if (id.type == TYPE_SURFACE)
   {
      TCHAR   tmpHatch[MAX_LEN_HATCHNAME], tmpLayer[MAX_LEN_LAYERNAME];
      double  tmpScale, tmpRotation;
      C_COLOR tmpColor;

      // controllo se c'è almeno un riempimento
      if (gsc_getInfoHatchSS(SelSet, tmpHatch, &tmpScale, &tmpRotation,
                             tmpLayer, &tmpColor) == GS_GOOD)
         // Aggiorno i riempimenti modificando SelSet
         if (gsc_UpdToDefHatch(SelSet, tmpHatch, tmpScale, gsc_rad2grd(tmpRotation),
                               &tmpColor, tmpLayer) == GS_BAD)
            return GS_BAD;
   }

   // Aggiorno il database
   return upd_data(Entity, ColValues, &SelSet);
}


/*********************************************************/
/*.doc C_SIMPLEX::insert_data(ads_name Entity) <external> */
/*+                                                                       
  Inserisce una entità nuova.
  Parametri:
  ads_name entity;      	Oggetto da inserire
  long OldKey;             Vecchio codice a cui era legato l'oggetto 
                           (nel caso di blocco attributi)
  long *NewKey;            Nuovo codice (nel caso di oggetto grafico)
                           default = NULL
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::insert_data(ads_name Entity, long OldKey, long *NewKey)
{
   long            new_key;
   C_EED           eed;
   C_SELSET        SelSet;
   int             result = GS_BAD;
   C_RB_LIST       ColValues;
   C_CLS_PUNT_LIST lista_cls;
   C_CLS_PUNT      *pClsPunt;

   if ((pClsPunt = new C_CLS_PUNT(this, Entity)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   lista_cls.add_tail(pClsPunt);

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;

   // verifico che ci sia un'area attiva;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;
  
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   do
   {  // se non è una entità di GEOsim
      if (eed.load(Entity) == GS_BAD)
      {
         if (gsc_is_DABlock(Entity) == GS_GOOD)
            { GS_ERR_COD = eGSInvClassType; break; }

         // leggo la scheda di default
         if (get_default_values(ColValues, Entity) == GS_BAD) break;
         if (ins_data(&lista_cls, ColValues, &new_key, INVISIBLE) == GS_BAD)
            break;
         result = GS_GOOD; 
      }
      else  // è una entità di GEOsim
      {
         C_LINK Link;

         // inserisco nuovo link
         if (Link.Set(Entity, id.code, id.sub_code, OldKey, MODIFY) == GS_BAD) break;

         if (gsc_is_DABlock(Entity) == GS_GOOD)
         {  // Se blocco attributi allora incremento di 1 il quante in tutti
            // gli oggetti collegati
            // Ricavo il linkset della scheda
            if (get_SelSet(OldKey, SelSet) == GS_BAD) break;

             // aggiorno n. aggregate su tutti gli oggetti
            if (eed.save_aggr(SelSet, SelSet.length()) == GS_BAD) { result = GS_BAD; break; }
            if (update_data(Entity) == GS_BAD)        // ricalcolo dati
               { GS_ERR_COD = eGSUnknown; break; }
            // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
            if (gsc_addSS2savess(SelSet) == GS_BAD) break;
         }
         else
         {  // se è un oggetto grafico va inserita anche una nuova scheda
            // contenente gli stessi dati della copia.
            // inserisco nuovo link
            if (Link.Set(Entity, id.code, id.sub_code, OldKey, MODIFY) == GS_BAD) break;
            // interrogo l 'entità
            if (query_data(Entity, ColValues) == GS_BAD) { result = GS_BAD; break; }
            // cancella tutti i Link di GEOsim di una entità
            if (Link.erase() == GS_BAD) { result = GS_BAD; break; }
            // cancello eed di GEOsim
            eed.clear(Entity); 
            if (ins_data(&lista_cls, ColValues, &new_key, INVISIBLE) == GS_BAD)
               break;
         }
         result = GS_GOOD;
      }
   }
   while (0);

   if (NewKey) *NewKey = new_key;

   return result;
}


/*********************************************************/
/*.doc C_SIMPLEX::align_for_inserted          <internal> */
/*+
  Questa funzione riallinea gli oggetti del gruppo di selezione.
  Se l'oggetto è un blocco attributi o un testo allora verrà
  aggregato se modificabile altrimenti saranno cancellati i collegamenti a GEOsim.
  Se si tratta di un oggetto grafico di tipo diverso (es. polilinea) verrà inserito
  una nuova entità di GEOsim della stessa classe derivando gli attributi
  dall'entità che lo ha generato.
  Parametri:
  C_SELSET *SelSet;   Gruppo di selezione degli oggetti della classe
                      N.B.: Gli oggetti del gruppo di selezione vengono
                      eliminati dal gruppo man mano che vengono elaborati !
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)
  int Reason;         flag a bit, origine della modifica sugli oggetti da allineare:
                      - NO_EXTERN_ACTION (non intraprende azioni esterne)
                      (default = UNKNOWN_MOD)

  Restituisce il numero di oggetti riallineati in caso di successo altrimenti -1.
-*/  
/*********************************************************/
long C_SIMPLEX::align_for_inserted(C_SELSET &SelSet, int CounterToVideo, int Reason)
{
	ads_name       entity;
	long           SelSet_len, qty = 0, i = 0, gs_id, Refused = 0, Locked = 0;
   int            Prcnt, WhyNotUpd;
   int            set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;
   C_EED          eed;
   C_INFO         *p_info = ptr_info();
   C_SELSET       entSelSet;
   TCHAR          Msg[MAX_LEN_MSG];       // per notifica in file .log
   TCHAR          Handle[MAX_LEN_HANDLE]; // per notifica in file .log
   C_STRING       TempTableRef;
   C_RB_LIST      ColValues;
   C_LINK         Link;
   C_DBCONNECTION *pTempConn;
   _RecordsetPtr  pInsRs;
   C_PREPARED_CMD_LIST TempOldCmdList;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1081)); // "Riallineamento banca dati"

   // verifico l'abilitazione dell'utente
   // bisogna avere entrambe le abilitazioni es. inserimento di un
   // blocco DA significa nuova aggregata = modifica dell'entità
   if (gsc_check_op(opInsEntity) == GS_BAD || gsc_check_op(opAggrEntity) == GS_BAD)
      return -1;

   if (!GS_CURRENT_WRK_SESSION) return 0;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return 0;

   // se non ci sono oggetti nel gruppo di selezione
   if ((SelSet_len = SelSet.length()) <= 0) return 0;

   if (CounterToVideo == GS_GOOD)
   {
      acutPrintf(gsc_msg(113), id.name); // "\n\nEntità classe %s:\n"
      StatusBarProgressMeter.Init(SelSet_len);
   }

   // se la classe non è modificabile
   if (id.abilit != GSUpdateableData)
   {
      while (SelSet.entname(i++, entity) == GS_GOOD)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(i);
         
         Link.erase(entity);
         eed.clear(entity);
         gsc_EraseEnt(entity);
      }

      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.End(gsc_msg(310), i); // "%ld entità GEOsim elaborate."

      return 0;
   }

   // ricavo le connessioni ai database
   if ((pTempConn = p_info->getDBConnection(TEMP)) == NULL) return -1;

   // ricavo la tabella temporanea
   if (getTempTableRef(TempTableRef) == GS_BAD) return -1;

   // preparo istruzione per l'inserimento di record nella tabella temp
   if (pTempConn->InitInsRow(TempTableRef.get_name(), pInsRs) == GS_BAD) return -1;

   // Compilo le istruzioni di lettura dei dati della classe dal temp/old
   if (prepare_data(TempOldCmdList) == GS_BAD) return -1;

   eed.cls = id.code;
   eed.sub = id.sub_code;

   if (id.type != TYPE_TEXT)
   {
      int InsDABlock = (GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO) ? VISIBLE : INVISIBLE;

      // ciclo su tutti gli oggetti che devono essere inseriti come nuovi
      // (scarto i testi e i blocchi di attributi)
      while (SelSet.entname(i, entity) == GS_GOOD)
      {
         if (gsc_is_DABlock(entity) == GS_GOOD) { i++; continue; }

         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(++qty);

         SelSet.subtract_ent(entity); // sottraggo dal Selection Set

         // Elimino i riempimenti delle superfici
         if (id.type == TYPE_SURFACE && gsc_ishatch(entity) == GS_GOOD)
            gsc_EraseEnt(entity);
         else
         {
            Link.SetEnt(entity);
            // legge i dati
            if (Link.GetKey(&gs_id) == GS_BAD ||
                query_data(gs_id, ColValues, &TempOldCmdList) == GS_BAD)
            {
               // Notifico in file log
               gsc_enthand(entity, Handle);
               swprintf(Msg, MAX_LEN_MSG, _T("Invalid key on entity: handle <%s>, prj %d, class %d."),
                        Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code);      
               gsc_write_log(Msg);

               Link.erase();
               eed.clear(entity);
               gsc_EraseEnt(entity);
               Refused++;
               continue;
            }

            // cancella il Link di GEOsim dell'entità
            if (Link.erase() == GS_BAD)
            {
               // Notifico in file log
               gsc_enthand(entity, Handle);
               swprintf(Msg, MAX_LEN_MSG, _T("LINK ASE deletion failed; handle <%s>, prj %d, class %d."),
                        Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code);

               eed.clear(entity);
               gsc_EraseEnt(entity);
               Refused++;
               continue;
            }

            // cancella la eed di GEOsim dell'entità
            if (eed.clear(entity) == GS_BAD)
            {
               // Notifico in file log
               gsc_enthand(entity, Handle);
               swprintf(Msg, MAX_LEN_MSG, _T("EED deletion failed; handle <%s>, prj %d, class %d."),
                        Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code);

               gsc_EraseEnt(entity);
               Refused++;
               continue;
            }

            if (id.type == TYPE_SURFACE)
            {
               C_LINK_SET LinkSS;
               C_SELSET OldEntSelSet;
               TCHAR    tmpHatch[MAX_LEN_HATCHNAME], tmpLayer[MAX_LEN_LAYERNAME];
               double   tmpScale, tmpRotation;
               C_COLOR  tmpColor;

               // verifico se l'entità originale aveva un riempimento
               if (LinkSS.GetSS(id.code, id.sub_code, gs_id, OldEntSelSet) == GS_BAD) return GS_BAD;

               // controllo se c'è almeno un riempimento
               if (gsc_getInfoHatchSS(OldEntSelSet, tmpHatch, 
                                       &tmpScale, &tmpRotation,
                                       tmpLayer, &tmpColor) == GS_GOOD)
               {
                  entSelSet.clear();
                  entSelSet.add(entity);
                  // se ritorna GS_CAN la superficie era troppo piccola per
                  // contenere il riempimento
                  if (gsc_setHatchEnt(entity, tmpHatch, tmpScale,
                                      tmpRotation, &tmpColor, tmpLayer) == GS_GOOD)
                  {
                     acdbEntLast(entity); // aggiungo anche il riempimento
                     entSelSet.add(entity);
                     entSelSet.get_selection(entity); // entity è un gruppo di selezione
                  }
               }
            }

            if (ins_data(entity, ColValues, NULL, InsDABlock, UNKNOWN_MOD, pInsRs.GetInterfacePtr()) == GS_BAD)
            {
               if (gsc_enthand(entity, Handle) == GS_GOOD) // se esiste ancora
               {
                  swprintf(Msg, MAX_LEN_MSG, _T("Record append failed on entity - handle <%s>, prj %d, class %d, subclass %d."),
                           Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
                  gsc_write_log(Msg);
               }                
               gsc_EraseEnt(entity);
               Refused++;
               continue;
            }
         }
      }
   }
   
   // ciclo su tutti gli oggetti che devono essere aggregati
   // (testi oppure blocchi di attributi)
   while (SelSet.entname(0, entity) == GS_GOOD)
   {
      if (CounterToVideo == GS_GOOD)         
      {
         Prcnt = (int) ((SelSet_len - SelSet.length()) * 100 / SelSet_len);
         StatusBarProgressMeter.Set_Perc(Prcnt);
      }

      // leggo valore chiave e il gruppo di selezione
      Link.SetEnt(entity);
      qty++;
      if (Link.GetKey(&gs_id) == GS_BAD || get_SelSet(gs_id, entSelSet) == GS_BAD)
      {
         // Notifico in file log
         gsc_enthand(entity, Handle);
         swprintf(Msg, MAX_LEN_MSG, _T("Invalid key on entity: handle <%s>, prj %d, class %d."),
                  Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code);      
         gsc_write_log(Msg);

         Link.erase();
         eed.clear(entity);
         SelSet.subtract_ent(entity);
         gsc_EraseEnt(entity);
         Refused++;
         continue;
      }

      SelSet.subtract(entSelSet); // sottraggo dal Selection Set

      // se è aggiornabile viene bloccata
      if (is_updateableSS(gs_id, entSelSet, &WhyNotUpd, set_lock,
                          TryToExtractPartialEnt) == GS_GOOD)
      {
         // legge i dati
         if (query_data(gs_id, ColValues, &TempOldCmdList) == GS_BAD)
         {
            Link.erase();
            eed.clear(entity);
            gsc_EraseEnt(entity);
            Refused++;
            continue;
         }

         // aggiorno n. aggregate su tutti gli oggetti (va in GEOsimAppl::SAVE_SS)
         if (eed.save_aggr(entSelSet, entSelSet.length()) == GS_BAD)
         {
            Link.erase();
            eed.clear(entity);
            gsc_EraseEnt(entity);
            Refused++;
            continue;
         }

         if (upd_data(gs_id, ColValues,
                      ((C_PREPARED_CMD *) TempOldCmdList.get_head()), 
                      &entSelSet) == GS_BAD)
         {
            // aggiorno n. aggregate su tutti gli oggetti (va in GEOsimAppl::SAVE_SS)
            eed.save_aggr(entSelSet, entSelSet.length() - 1);
            GEOsimAppl::SAVE_SS.subtract_ent(entity);
         
            Link.erase();
            eed.clear(entity);
            gsc_EraseEnt(entity);
            Refused++;
            continue; 
         }

         // scrivo solo se la variabile globale "logfile" = GS_GOOD
         if (GEOsimAppl::GLOBALVARS.get_LogFile() == GS_GOOD)
         {
            // Notifico in file log
            swprintf(Msg, MAX_LEN_MSG, _T("Aggregated entity: key %ld, prj %d, class %d, subclass %d."),
                     gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
            gsc_write_log(Msg);
         }
      }
      else // se era già bloccata
      {
         gsc_enthand(entity, Handle);
         swprintf(Msg, MAX_LEN_MSG, _T("Locked entity - handle <%s>, prj %d, class %d."),
                  Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code);
         gsc_write_log(Msg);

         // cancella tutti i Link e la eed di GEOsim di una entità
         Link.erase();
         eed.clear(entity);
         gsc_EraseEnt(entity);
         Refused++;
         if (WhyNotUpd == eGSObjectIsLockedByAnotherUser) Locked++;
      }
   }

   if (CounterToVideo == GS_GOOD)
      if (qty >= 0)
      {
         acutPrintf(gsc_msg(308), qty, Refused); // "\n%ld entità GEOsim elaborate, %ld scartate."
         if (Locked > 0) acutPrintf(gsc_msg(770), Locked); // "\n%ld entità GEOsim bloccata/e da un' altro utente."
      }

   return qty;
}


/*********************************************************/
/*  FINE FUNZIONI DELLA CATEGORIA C_SIMPLEX              */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_SPAGHETTI          */
/*********************************************************/


/*********************************************************/
/*.doc (new 2) C_SPAGHETTI::is_updateable <internal> */
/*+                                                                       
  Verifica la possibilità di modificare l'oggetto.
  Se la variabile "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento 
  delle entità nel salvataggio) questa funzione non effettua alcun controllo 
  e restituisce sempre GS_GOOD.
  Parametri:
  ads_name   entity;          Oggetto grafico
  int        *WhyNot;         Codice rappresentante il motivo per cui non è aggiornabile
                              (default = NULL)
  int        set_lock;        flag se = GS_GOOD la funzione prova a bloccare l'entità
                              (se questa era risultata aggiornabile) default = GS_GOOD
  int TryToExtractPartialEnt; Usato solo per compatibilità

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SPAGHETTI::is_updateable(ads_name entity, int *WhyNot, int set_lock, int TryToExtractPartialEnt)
{                 
   C_EED     eed;
   C_RB_LIST p;
   C_LOCK    oggetto;
   long      lockArea = 0;
   int       islocked, res;

   if (WhyNot) *WhyNot = eGSUnknown;

   // verifico abilitazione
   if (GS_CURRENT_WRK_SESSION == NULL) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(WhyNot) == GS_BAD) return GS_BAD;
   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      if (WhyNot) *WhyNot = GS_ERR_COD;
      return GS_BAD;
   }

   // carico i dati estesi di GEOsim
   if (eed.load(entity) == GS_BAD) { GS_ERR_COD = eGSGEOsimObjNotFound; return GS_BAD; }
   // verifico che l'oggetto grafico sia già legato a questa classe
   if (eed.cls == id.code && eed.sub != id.sub_code) return GS_BAD;

   // se entità nuova salto i controlli
   if (is_NewEntity(entity) == GS_GOOD) return GS_GOOD;

   // se non si è abilitati a inserire in GEOsimAppl::SAVE_SS
   // ritorno sempre GS_GOOD perchè l'entità non verrà salvata
   if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD) return GS_GOOD;

   // se devo bloccare l'oggetto
   if (set_lock == GS_GOOD)
   {
      if (oggetto.set_locked(entity, &res) == GS_BAD) return GS_BAD;
      if (res == GS_BAD)
      {
         if (WhyNot) *WhyNot = eGSObjectIsLockedByAnotherUser;
         return GS_BAD;
      }
   }
   else
   {
      // verifico se l'oggetto è libero (controllo solo su altre sessioni)
      if (oggetto.is_locked(entity, &islocked, GS_GOOD) == GS_BAD)  
         return GS_BAD;
      // se è bloccato
      if (islocked == TEMPORARY_LOCKED || islocked == PERMANENT_LOCKED)
      {
         if (WhyNot) *WhyNot = eGSObjectIsLockedByAnotherUser;
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SPAGHETTI::is_NewEntity              <external> */
/*+                                                                       
  Funzione che verifica se una entità di GEOsim è nuova. 
  Parametri:
  ads_name entity;   Entità grafica
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SPAGHETTI::is_NewEntity(ads_name entity)
{
   C_STRING IdInternal, TableName;

   // ricavo il nome della tabella interna
   gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code,
                      TableName);

   if (gsc_getIDfromODTable(entity, TableName, IdInternal) == GS_BAD)
      return GS_GOOD;

   if (GS_CURRENT_WRK_SESSION->HasBeenFrozen() == GS_GOOD) return GS_GOOD;

   // se NON si tratta di una sessione scongeleta provo un'altra tecnica
   // perchè ad esempio ripristinando una 3DPOLI precedentemente cancellata
   // non si riporta i dati oggetti che aveva prima della sua cancellazione

   // Se la classe usa i DWG
   if (ptr_GphInfo()->getDataSourceType() == GSDwgGphDataSource)
   {
      C_RB_LIST RbList;

      // ricavo ID del DWG di provenienza dell'entità 
      if ((RbList << gsc_ade_qrygetdwgandhandle(entity)) == NULL) return GS_GOOD;
   }
   else 
   {
      TCHAR Handle[MAX_LEN_HANDLE];

      // ricavo l'handle
      if (gsc_enthand(entity, Handle) == GS_GOOD)
         // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
         // è un oggetto estratto e lo scarto
         if (!((C_DBGPH_INFO *) ptr_GphInfo())->HandleId_LinkTree.search(Handle))
            return GS_GOOD;
   }

   return GS_BAD;
}


/*********************************************************/
/*.doc C_SPAGHETTI::ins_data <external> */
/*+                                                                       
  Marchia l'oggetto grafico come entità GEOsim.
  Parametri:
  C_CLS_PUNT_LIST *lista_cls;  Nuovo oggetto grafico
  C_RB_LIST       &ColValues;  Non usato (presente solo per compatibilità)
  long            *gs_id;		 Non usato (presente solo per compatibilità)
  int             visib_block; Non usato (presente solo per compatibilità)
  int             Reason;      origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                               RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                               (default = UNKNOWN_MOD)
  _RecordsetPtr   *pRsIns;     Non usato (presente solo per compatibilità)
  C_BTREE *pPtObjsBTree; non usato, solo per compatibilità con C_SUB::ins_data

  Restituisce GS_GOOD in caso di successo, GS_CAN in caso di annullamento
  altrimenti restituisce GS_BAD.
  N.B. : il primo oggetto grafico della lista <lista_cls> viene sostituito con 
         un altro oggetto grafico uguale ma con differente handle e con le entità estese
         di GEOsim. L'inserimento dell'oggetto grafico nel database di autocad 
         dovrebbe avvenire con le entità estese di GEOsim per gestire l'UNDO
-*/  
/*********************************************************/
int C_SPAGHETTI::ins_data(ads_name lista_ent, C_RB_LIST &ColValues, long *gs_id, 
                          int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
{
   C_CLS_PUNT_LIST lista_cls;
   C_CLS_PUNT      *p;
   long            len;

   // Converto una lista di ename in una lista C_CLS_PUNT_LIST
   if (ads_sslength(lista_ent, &len) != RTNORM)
   {
      len = 0;
      // è una sola entità
      if ((p = new C_CLS_PUNT(this, lista_ent)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      p->cls = this;
      ads_name_set(lista_ent, p->ent);
      lista_cls.add_tail(p);
   }
   else
   {  // è un gruppo di selezione
      ads_name ent;

      for (long i = 0; i < len; i++)
      {
         if (acedSSName(lista_ent, i, ent) != RTNORM) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
         if ((p = new C_CLS_PUNT) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         p->cls = this;
         ads_name_set(ent, p->ent);
         lista_cls.add_tail(p);
      }
   }

   int ret = ins_data(&lista_cls, ColValues, gs_id, visib_block, Reason, pRsIns);

   if (len == 0)
      ads_name_set(((C_CLS_PUNT *) lista_cls.get_head())->ent, lista_ent);
   else
   {
      ads_ssfree(lista_ent);
      lista_cls.to_ssgroup(lista_ent);
   }

   return ret;
}
int C_SPAGHETTI::ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues,
                          long *gs_id, int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
{                 
   C_EED      eed;
   ads_name   NewEnt;
   int        result = GS_BAD, OldOp;
   C_CLS_PUNT *pEntCls;
   C_STRING   UsrFunction;
   C_RB_LIST  InfoList;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;

   if (!lista_cls || lista_cls->get_count()==0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }
   
   ads_name_set(((C_CLS_PUNT*) lista_cls->get_head())->ent, NewEnt);

   // verifico che gli oggetti grafici non siano già di "GEOsim"
   pEntCls = (C_CLS_PUNT *) lista_cls->get_head();
   while (pEntCls)
   {
      // verifico che l'oggetto grafico non sia già di questa classe
      if (eed.load(pEntCls->ent) == GS_GOOD)
         { GS_ERR_COD = eGSGEOsimObjFound; return GS_BAD; }
      pEntCls = (C_CLS_PUNT*) lista_cls->get_next();
   }

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(INSERT);

   do
   {
      // eventuale chiamata ad una funzione utente esterna (prima dell'inserimento o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_INSERT, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
		   if (!InfoList.get_head())
            if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			      break;
         if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                  NULL, INVISIBLE, Reason) == GS_BAD) break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_INSERT, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
		      if (!InfoList.get_head())
               if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			         break;
            if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                     NULL, INVISIBLE, Reason) == GS_BAD) break;
         }

      eed.cls = id.code;
      eed.sub = id.sub_code;
      // inserisco etichette "entità di GEOsim" senza inserirlo nel gruppo 
      // di selezione del salvataggio
      if (eed.save(NewEnt, GS_BAD) == GS_BAD) break; 

      // Se esiste qualche azione sull'evento cancellazione
      if (gsc_areActionsOnErase(id.usr_cmds) == GS_GOOD)
      {
         // Faccio la copia di una sola entità e cancello la vecchia (è sufficiente una sola)
         // perchè l'inserimento dell'oggetto grafico nel database di autocad 
         // dovrebbe avvenire con le entità estese di GEOsim per gestire l'UNDO.
         if (gsc_DeepClone2ModSpace(NewEnt) != GS_GOOD || gsc_EraseEnt(NewEnt) != GS_GOOD)
            { GS_ERR_COD = eGSErrorExecutingCommand; break; }
         if (acdbEntLast(NewEnt) != RTNORM) { GS_ERR_COD = eGSInvEntityOp; break; }

         // sostituisco entità nella lista degli oggetti grafici (parametro della funzione)
         ads_name_set(NewEnt, ((C_CLS_PUNT*) lista_cls->get_head())->ent);
      }

      // eventuale chiamata ad una funzione utente esterna (dopo l'inserimento)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_INSERT, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
		   if (!InfoList.get_head())
            if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			      break;
         if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                  NULL, INVISIBLE, Reason) == GS_BAD) break;
      }

      // inserisco oggetto grafico nel gruppo di selezione del salvataggio
      gsc_addEnt2savess(NewEnt);
      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG], hand[MAX_LEN_HANDLE];
      gsc_enthand(NewEnt, hand);
      swprintf(Msg, MAX_LEN_MSG, _T("Inserted entity: handle <%s>, prj %d, class %d, subclass %d."),
               hand, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}
 

/************************************************************/
/*.doc C_SPAGHETTI::insert_data(ads_name Entity) <external> */
/*+                                                                       
  inserimento di una entità spaghetti.
  Parametri:
  ads_name entity;      	Oggetto da inserire.
  long OldKey;             Usato solo per compatibilità
  long *NewKey;            Usato solo per compatibilità

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************/
int C_SPAGHETTI::insert_data(ads_name Entity, long OldKey, long *NewKey)
{
   C_EED           eed;
   C_CLS_PUNT_LIST lista_cls;
   C_CLS_PUNT      *pClsPunt;
   C_STRING        tablename;
   C_RB_LIST       dummy;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;

   // verifico che ci sia un'area attiva
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   if ((pClsPunt = new C_CLS_PUNT) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   pClsPunt->cls = this;
   ads_name_set(Entity, pClsPunt->ent);
   lista_cls.add_tail(pClsPunt);

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   // se è una entità di GEOsim tolgo etichetta di GEOsim
   if (eed.load(Entity) == GS_GOOD) eed.clear(Entity); 

   // cancello Id dwg originale
   // setto la tabella interna entità
   gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code,
                             tablename);
   if (ade_odrecordqty(Entity, tablename.get_name()) > 0)
      // cancello la tabella OD per memorizzazione ID dwg originale
      if (ade_oddelrecord(Entity, tablename.get_name(), 0) != RTNORM)
         { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   if (ins_data(&lista_cls, dummy) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SPAGHETTI::import_data(ads_name Entity)         */
/*+                                                                       
  Funzione che importa un serie di oggetti nella classe. 
  Parametri:
  C_SELSET &objGraph;
  C_SELSET &objDA;
  int      loadGSent;        Flag che determina se importare gli oggetti di GEOsim.
  long BitForChangeToNewFAS; Opzionale, Flag a bit per indicare quali caratteristiche
                             grafiche cambiare
  C_FAS *pNewFas;            I valori delle caratteristiche grafiche da cambiare (se = NULL
                             verranno considerati i valori di default)
  C_RB_LIST &ColValues;  
  C_CLASS *pObj;
  _RecordsetPtr pInsRs;
  int *objLocked;
  
  Restituisce il numero degli oggetti grafici importati. 
-*/  
/*********************************************************/
long C_SPAGHETTI::import_data(C_SELSET &objGraph, C_SELSET &objDA, int loadGSent, 
                              long BitForChangeToNewFAS, C_FAS *pNewFas,
                              C_RB_LIST &ColValues, 
                              C_CLASS *pObj, _RecordsetPtr pInsRs, int *objLocked)
{
   C_SELSET    toDel;
   C_CLASS    *pOrig = NULL;
   ads_name    obj;
   long        ind = 0, Ok = 0;

   // Siccome stiamo importando in entità spaghetti devo prima 
   // elaborare le schede di GEOsim o i DA
   if (objDA.length() > 0)
   {
      if (import_data(objDA, ColValues, toDel, pInsRs, pObj, loadGSent,
                      BitForChangeToNewFAS, pNewFas) == GS_GOOD)
         Ok++;
      else
      {
         if (GS_ERR_COD == eGSObjectIsLockedByAnotherUser)
            *objLocked = GS_GOOD;
         else
            gsc_print_error();
      }
   }
   // Poi elaboro le entità principali
   if (objGraph.length() > 0)
   {
      if (import_data(objGraph, ColValues, toDel, pInsRs, pObj, loadGSent,
                      BitForChangeToNewFAS, pNewFas) == GS_GOOD)
         Ok++;
      else
      {
         if (GS_ERR_COD == eGSObjectIsLockedByAnotherUser)
            *objLocked = GS_GOOD;
         else
            gsc_print_error();
      }
   }   
   // All termine dell'importazione cancello gli oggetti di partenza.
   if (pObj != NULL)
   {
      ind = 0;
      while (toDel.entname(ind++, obj) == GS_GOOD)
         pObj->erase_data(obj);
   }
   else
      toDel.Erase();

   return Ok;
}


/*********************************************************/
/*.doc C_SPAGHETTI::import_data(ads_name Entity) <external> */
/*+                                                                       
  Funzione che importa un oggetto nella classe. 
  Parametri:
  C_SELSET   &objGraph    Oggetto da importare.
  C_RB_LIST  &ColValue    solo per compatibilità
  C_SELSET   &ToDel       Gruppo di selezione degli oggetti da cancellare.
  _RecordsetPtr pInsRs
  C_CLASS    *pObj        Eventuale puntatore alla classe (... se di GEOsim).
  int         loadGSent   Flag che determina se importare gli oggetti di GEOsim.
  long BitForChangeToNewFAS; Opzionale, Flag a bit per indicare quali caratteristiche
                             grafiche cambiare (default = GSNoneSetting)
  C_FAS *pNewFas;            Opzionale, i valori delle caratteristiche grafiche da cambiare 
                             (se = NULL si intendono i valori di default della classe)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SPAGHETTI::import_data(C_SELSET &objGraph, C_RB_LIST &ColValue, C_SELSET &ToDel, 
                             _RecordsetPtr pInsRs,
                             C_CLASS *pObj, int loadGSent, 
                             long BitForChangeToNewFAS, C_FAS *pNewFas)
{
   C_LINK        Link;
   C_EED         eed;
   C_FAMILY_LIST family_list;
   C_SELSET      objToImporting, objImported, objCopied;
   C_STRING      nObjData;
   ads_name      marker, ent, ss_objToImporting;
   int           result = GS_BAD, WhyNotUpd;
   int           rescmplx = GS_BAD, prj;
   long          Key, gs_id, num_objCopied, num_objImported;

   // Verifico l'abilitazione dell' utente;
   if (gsc_check_op(opImportEntity) == GS_BAD) return GS_BAD;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   prj = GS_CURRENT_WRK_SESSION->get_PrjId();

   // Inizializzo i gruppi a nil
   ads_name_clear(ss_objToImporting);

   // Controllo la compatibilità degli oggetti con la classe e nel 
   // di non compatibilità li elimino.
   if (check_SS_graph_compatib_on_import(objGraph, GS_GOOD) == GS_BAD) return GS_BAD;
   
   // I seguenti controlli hanno senso solo per oggetti di GEOsim (pObj != NULL)
   if (pObj != NULL)
   {
      // Per prima cosa se siamo nel caso di oggetti di GEOsim ed il
      // flag di importazione di detti oggetti non è selezionato
      // segnalo un errore
      if (pObj && loadGSent == GS_BAD) { GS_ERR_COD = eGSGEOsimObjFound; return GS_BAD; }

      // Verifico che non si cerchi di importare un'oggetto di una classe
      // nella stessa classe, in tal caso segnalo l'errore
      if (this == pObj) return GS_BAD; 

      // Verifico anche che l'entità di partenza non faccia parte 
      // di un gruppo, prima ricavo il GS_ID
      // Solo se la classe di partenza non è spaghetti
      // Ricavo il primo elemento del gruppo di selezione per poter verificarlo
      if (objGraph.entname(0, ent) != GS_GOOD) return GS_BAD;
      if (pObj->get_category() != CAT_SPAGHETTI) 
      {
         if (pObj->getKeyValue(ent, &Key) == GS_BAD) return GS_BAD;
         if (gsc_getfamily(GS_CURRENT_WRK_SESSION->get_pPrj(), &family_list) == GS_BAD) return GS_BAD;
         if (family_list.get_count() > 0)
         {
            // Controllo se l'entità fa parte di un gruppo
            if (gsc_is_member_of(pObj, Key, family_list, &rescmplx) == GS_BAD) return GS_BAD;
            if (rescmplx == GS_GOOD) { GS_ERR_COD = eGSMemberOfGroup; return GS_BAD; }
         }
      }
   
      // A questo punto verifico che l'elemento sia modificabile e lo blocco
      GS_ERR_COD = eGSUnknown;
      if (pObj->is_updateable(ent, &WhyNotUpd, GS_GOOD, GS_GOOD) == GS_BAD)
         { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }
   }

   // A questo punto posso copiare gli oggetti
   // Ricavo il gruppo di selezione per copiare gli oggetti
   // Memorizzo l'ultimo oggetto ne DB di AutoCAD
   if (acdbEntLast(marker) != RTNORM) return GS_BAD;

   // Eseguo la copia dell' oggetto da importare 
   if (gsc_DeepClone2ModSpace(objGraph) != GS_GOOD) return GS_BAD;

   // A questo punto mi scorro il database di AutoCad dal marker
   while (gsc_mainentnext(marker, marker) == GS_GOOD)
   {
      // Memorizzo l'ent in una C_selset
      objCopied.add(marker);
      // A questo punto in marker ho la nuova entità copiata da importare
      // Per prima cosa gli elimino i link di GEOsim, e poi le EED   
      if (pObj != NULL)
      {
         // Se si tratta di entità di GEOsim e non sono spaghetti
         // Elimino il collegamento a DB
         if (pObj->get_category() != CAT_SPAGHETTI) 
         {
            if (Link.erase(marker) == GS_BAD) continue;
         }
         else
         {  // Elimino il collegamento a OD
            gsc_getODTableName(prj, pObj->ptr_id()->code, pObj->ptr_id()->sub_code,
                              nObjData);
            if (gsc_delID2ODTable(marker, nObjData) == GS_BAD) continue;
         }

         // Eliminazione EED
         if (eed.load(marker) == GS_GOOD) eed.clear();
      }
      objToImporting.add(marker);
   }
   num_objCopied = objCopied.length();
   // Mi ricavo il gruppo di selezione perchè la ins_data vuole un ads_name
   objToImporting.copy(ss_objToImporting);

   result = GS_BAD;
   do
   {
      // Setto l'operazione corrente
      GS_CURRENT_OPERATION = INSERT;
      
      // Eseguo l'operazione di importazione
      if (ins_data(ss_objToImporting, ColValue, &gs_id) == GS_BAD)
         { acedSSFree(ss_objToImporting); break; }
      else
      {
         objImported << ss_objToImporting;
         num_objImported = objImported.length();
         if ((num_objCopied - num_objImported) > 0)
         {
            // Se il numero di oggetti copiati non è uguale
            // a quelli importati devo cancellarli
            objCopied.subtract(objImported);
            objCopied.Erase();
         }
      }

      result = GS_GOOD;
   }
   while (0);

   if (result == GS_BAD)
   {
      // Se si è verificato un errore, e gli oggetti sono stati
      // già copiati li devo cancellare
      if (objCopied.length() > 0)
         objCopied.Erase();
   }
   else
   {
      // Se tutto è andato bene, gli oggetti di partenza devono essere aggiunti
      // al gruppo di selezione per poi cancellarli.
      ToDel.add_selset(objGraph); 
      // Se il flag di applicazione della FAS è attivo allora 
      // ricavo quali sono le caratteristiche grafiche modificabili 
      // della classe tenendo conto di eventuali funzioni di modifica FAS
      if (BitForChangeToNewFAS != GSNoneSetting)
      {  
         long  BitForChangeFAS;
         C_FAS *pFas = (pNewFas) ? pNewFas : ptr_fas();

         BitForChangeFAS = what_is_graph_updateable();
         // Se si poteva variare la rotazione o l'elevazione, queste vengono inibite perchè si
	      // devono variare solo caratteristiche "estetiche" e non geometriche degli oggetti
         if (BitForChangeFAS & GSRotationSetting) BitForChangeFAS -= GSRotationSetting;
         if (BitForChangeFAS & GSElevationSetting) BitForChangeFAS -= GSElevationSetting;

         BitForChangeToNewFAS = BitForChangeToNewFAS & BitForChangeFAS;

         // Modifico l' entità con la FAS originale della classe
         gsc_modifyEntToFas(objImported, pFas, BitForChangeToNewFAS);
      }
   }

   // Setto l'operazione corrente
   GS_CURRENT_OPERATION = NONE;

   // Prima di ritornare un valore devo rilasciare, se necessario 
   // tutti i gruppi di selezione
   if (!ads_name_nil(ss_objToImporting))
   {
      acedSSFree(ss_objToImporting);
      // Per evitare che il distruttore di objImported rilasci di nuovo il gruppo
      ads_name_clear(ss_objToImporting);
   }

   return result;
}                       


/***********************************************************/
/*.doc C_SPAGHETTI::erase_data(ads_name Entity) <external> */
/*+                                                                       
  Cancella una entità di tipo spaghetti.
  Parametri:
  ads_name entity;      	Oggetto da cancellare.
  int      Reason;         origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                           RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                           (default = UNKNOWN_MOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SPAGHETTI::erase_data(ads_name Entity, int Reason)
{
   int       result = GS_BAD, WhyNotUpd, OldOp;
   C_RB_LIST listone; 

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opDelEntity) == GS_BAD) return GS_BAD;
     
   if (is_updateable(Entity, &WhyNotUpd) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(ERASE);

   // eventuale chiamata ad una funzione utente esterna (prima della cancellazione o in
   // completa sostituzione della funzione di GEOsim)
   C_STRING UsrFunction;
   if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_ERASE, UsrFunction) == GS_GOOD &&
       UsrFunction.len() > 0)
   {
      if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
         return GS_BAD;
      return GS_GOOD;
   }
   else
      if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_ERASE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
            return GS_BAD;

   do
   {
      TCHAR Msg[MAX_LEN_MSG], Handle[MAX_LEN_HANDLE];

      // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
      if (gsc_addEnt2savess(Entity) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_enthand(Entity, Handle) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_EraseEnt(Entity) != GS_GOOD) { result = GS_BAD; break; }
      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      swprintf(Msg, MAX_LEN_MSG, _T("Erased entity: handle <%s>, prj %d, class %d, subclass %d."),
               Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      // eventuale chiamata ad una funzione utente esterna (dopo la cancellazione)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_ERASE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
            result = GS_BAD;

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);
   
   return result;
}


/*********************************************************/
/*.doc C_SPAGHETTI::upd_data <external> */
/*+                                                                       
  Funzione di Update per spaghetti, serve solo a mettere
  l' entità nel gruppo di selezione.
  Parametri:
  ads_name entity;
  C_RB_LIST &ColValues;     Usato per compatibilità
  C_SELSET *GlobalSelSet;   Usato per compatibilità
  int      Reason;          origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SPAGHETTI::upd_data(ads_name entity, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet,
                          int Reason)
{                 
   int      result = GS_BAD, WhyNotUpd, OldOp;
   C_STRING UsrFunction;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;
   
   if (is_updateable(entity, &WhyNotUpd) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(MODIFY);

   do
   {
      TCHAR Msg[MAX_LEN_MSG], Handle[MAX_LEN_HANDLE];

      // eventuale chiamata ad una funzione utente esterna (prima della modifica o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), entity, Reason) == GS_BAD) break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_UPDATE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
            if (gsc_doActionOnUpdate(UsrFunction.get_name(), entity, Reason) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (dopo la modifica)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), entity, Reason) == GS_BAD)
            break;

      // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
      if (gsc_addEnt2savess(entity) == GS_BAD) break;
      setModified(GS_GOOD); // classe modificata

      if (gsc_enthand(entity, Handle) == GS_BAD) break;
      // Notifico in file log
      swprintf(Msg, MAX_LEN_MSG, _T("Updated entity: handle <%s>, prj %d, class %d, subclass %d."),
               Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);
   
   return result;
}


/*********************************************************/
/*.doc C_SPAGHETTI::update_data <external> */
/*+                                                                       
      Funzione di Update per spaghetti. 
      E' uguale alla upd_data, ma serve per uniformare i 
      comandi.
-*/  
/*********************************************************/
int C_SPAGHETTI::update_data(ads_name entity)
{                 
   C_RB_LIST dummy;
   return upd_data(entity, dummy);
}


/*********************************************************/
/*.doc C_SPAGHETTI::align_for_inserted        <internal> */
/*+
  Questa funzione riallinea gli oggetti del gruppo di selezione
  che verranno considerati come oggetti da inserire.
  Parametri:
  C_SELSET *SelSet;   Gruppo di selezione
                      N.B.: Gli oggetti del gruppo di selezione vengono
                      eliminati dal gruppo man mano che vengono elaborati !
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)
  int Reason;         flag a bit, origine della modifica sugli oggetti da allineare:
                      - NO_EXTERN_ACTION (non intraprende azioni esterne)
                      (default = UNKNOWN_MOD)

  Restituisce il numero di oggetti riallineati in caso di successo altrimenti 
  restituisce -1.
-*/  
/*********************************************************/
long C_SPAGHETTI::align_for_inserted(C_SELSET &SelSet, int CounterToVideo, int Reason)
{
   C_STRING  tablename;
   long      i = 0, Refused = 0, SelSet_len;
   int       Prcnt;
   ads_name  entity;
   C_EED     eed;
   C_RB_LIST dummy;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1081)); // "Riallineamento banca dati"
   
   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opInsEntity) == GS_BAD) return -1;

   if (!GS_CURRENT_WRK_SESSION) return 0;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return 0;

   // ci sono oggetti nel gruppo di selezione
   if ((SelSet_len = SelSet.length()) <= 0) return 0;

   // setto la tabella interna della vecchia entità
   gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code,
                             tablename);

   if (CounterToVideo == GS_GOOD)
   {
      acutPrintf(gsc_msg(113), id.name); // "\n\nEntità classe %s:\n"
      StatusBarProgressMeter.Init(SelSet_len);
   }

   // se la classe non è modificabile
   if (id.abilit != GSUpdateableData)
   {
      while (SelSet.entname(0, entity) == GS_GOOD)
      { 
         i++;
         Refused++;
         if (CounterToVideo == GS_GOOD)
         {
            Prcnt = (int) (i * 100 / SelSet_len);
            StatusBarProgressMeter.Set_Perc(Prcnt);
         }

         SelSet.subtract_ent(entity); // sottraggo dal Selection Set
         // se non è nuova
         if (ade_odrecordqty(entity, tablename.get_name()) > 0)
            // cancello la tabella OD per memorizzazione ID dwg originale
            ade_oddelrecord(entity, tablename.get_name(), 0);
         eed.clear(entity);
         gsc_EraseEnt(entity);
      }
   }
   else
   {
      while (SelSet.entname(0, entity) == GS_GOOD)
      { 
         i++;
         if (CounterToVideo == GS_GOOD)
         {
            Prcnt = (int) (i * 100 / SelSet_len);
            StatusBarProgressMeter.Set_Perc(Prcnt);
         }

         SelSet.subtract_ent(entity); // sottraggo dal Selection Set
         // se non è nuova
         if (ade_odrecordqty(entity, tablename.get_name()) > 0)
            // cancello la tabella OD per memorizzazione ID dwg originale
            ade_oddelrecord(entity, tablename.get_name(), 0);
         
         if (eed.clear(entity) == GS_BAD)
         {
            gsc_EraseEnt(entity);
            Refused++;
            continue;
         }

         if (ins_data(entity, dummy) == GS_BAD)
         {
            eed.clear(entity);
            gsc_EraseEnt(entity);
            Refused++;
            continue;
         }
      }
   }

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      if (i >= 0)
         acutPrintf(gsc_msg(308), i, Refused); // "\n%ld entità GEOsim elaborate, %ld scartate."
   }

   return i - Refused;
}


/*********************************************************/
/*  FINE FUNZIONI DELLA CATEGORIA C_SPAGHETTI            */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_SUB                */
/*********************************************************/


/**************************************************************************/
/*.doc C_SUB::setModified                                      <external> */
/*+                                                                       
  Aggiorna il flag che identifica una classe modificata e se la classe è estratta
  in una sessione di lavoro viene aggiornato anche il file GS_CLASS_FILE.
  Parametri:
  int value;

  Ritorna GS_GOOD in caso di successo, altrimenti GS_BAD.
-*/  
/*****************************************************************************/
int C_SUB::setModified(int value)
{
   C_CLASS *pMother;

   // Ritorna il puntatore alla classe madre
   if ((pMother = (C_EXTERN*) ((C_PROJECT *)(id.pPrj))->find_class(id.code)) == NULL)
      return GS_BAD;

   return pMother->setModified(value);
}

   
/*****************************************************************************/
/*.doc C_SUB::isDataModified                                    <external> */
/*+                                                                       
  Ritorna GS_GOOD se c'è stato almeno un cambiamento della banca dati della 
  classe (inserimento, cancellazione, modifica) altrimenti GS_BAD.
-*/  
/*****************************************************************************/
int C_SUB::isDataModified(void)
{
   C_CLASS *pMother;

   // Ritorna il puntatore alla classe madre
   if ((pMother = (C_EXTERN*) ((C_PROJECT *)(id.pPrj))->find_class(id.code)) == NULL)
      return GS_BAD;

   return pMother->isDataModified();
}


/*********************************************************/
/*.doc C_SUB::ins_data <external> */
/*+                                                                       
  Inserisce una scheda nella tabella temporanea della sotto-classe e
  visualizza gli attributi visibili attivando il collegamento
  tra grafica e tabelle.
  Parametri:
  C_CLS_PUNT_LIST *lista_cls;  Nuovo oggetto grafico.
                               1) Se si tratta di inserimento di un elemento lineare
                                 il secondo elemento della lista avrà:
                                 - <ent>   = oggetto grafico del nodo iniziale,
                                 - <gs_id> = ID del nodo iniziale,
                                 - <cls>   = sottoclasse di appartenenza del nodo iniziale
                                 Il terzo elemento della lista avrà:
                                 - <ent>   = oggetto grafico del nodo finale,
                                 - <gs_id> = ID del nodo finale,
                                 - <cls>   = sottoclasse di appartenenza del nodo finale
  presbuf         col_values;  Non usato (presente solo per compatibilità)
  long            *gs_id;		 Codice nuova entità
  int             visib_block;
  int             Reason;      origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                               RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                               (default = UNKNOWN_MOD)
  _RecordsetPtr   *pRsIns;     RecordSet per inserimento dati (in caso
                               di inserimenti multipli); default = NULL.
  C_BTREE *pPtObjsBTree; puntatore alla lista degli oggetti puntuali esistenti in grafica; default = NULL.
                          la lista viene aggiornata se l'inserimento di un oggetto puntuale ha successo

  Restituisce GS_GOOD in caso di successo, GS_CAN in caso di annullamento
  altrimenti restituisce GS_BAD. 
  N.B. : il primo oggetto grafico della lista <lista_cls> viene sostituito con 
         un altro oggetto grafico uguale ma con differente handle e con le entità estese
         di GEOsim. L'inserimento dell'oggetto grafico nel database di autocad 
         dovrebbe avvenire con le entità estese di GEOsim per gestire l'UNDO
-*/  
/*********************************************************/
int C_SUB::ins_data(ads_name lista_ent, C_RB_LIST &ColValues, long *gs_id, 
                    int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
{
   C_CLS_PUNT_LIST lista_cls;
   C_CLS_PUNT      *p;
   long            len;

   // Converto una lista di ename in una lista C_CLS_PUNT_LIST
   if (ads_sslength(lista_ent, &len) != RTNORM)
   {
      len = 0;
      // è una sola entità
      if ((p = new C_CLS_PUNT) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      p->cls = this;
      ads_name_set(lista_ent, p->ent);
      lista_cls.add_tail(p);
   }
   else
   {  // è un gruppo di selezione
      ads_name ent;

      for (long i = 0; i < len; i++)
      {
         if (acedSSName(lista_ent, i, ent) != RTNORM) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
         if ((p = new C_CLS_PUNT(this, ent)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         lista_cls.add_tail(p);
      }
   }

   int ret = ins_data(&lista_cls, ColValues, gs_id, visib_block, Reason, pRsIns);

   if (len == 0)
      ads_name_set(((C_CLS_PUNT *) lista_cls.get_head())->ent, lista_ent);
   else
   {
      ads_ssfree(lista_ent);
      lista_cls.to_ssgroup(lista_ent);
   }

   return GS_GOOD;
}
int C_SUB::ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues, long *gs_id, 
                    int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
{
   C_ATTRIB_LIST *p_attrib_list = ptr_attrib_list();
   C_EED         eed;
   int           result = GS_BAD, InsRecord = FALSE, InsLink = FALSE, OldOp;
   C_LINK        Link, OrigLink;
   ads_name      NewEnt, newDABlock;
   long          new_key, initial_node, final_node;
   int           init_node_sub, final_node_sub;
   presbuf       pKey;
   C_CLS_PUNT    *pEntCls;
   C_STRING      UsrFunction;
   C_RB_LIST     InfoList;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;

   if (!lista_cls || lista_cls->get_count( )== 0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   ads_name_set(((C_CLS_PUNT*) lista_cls->get_head())->ent, NewEnt);
   ads_name_clear(newDABlock);

   if (!(pKey = ColValues.CdrAssoc(ptr_info()->key_attrib.get_name()))) return GS_BAD;

   // verifico che il primo oggetto grafico non sia già di "GEOsim"
   pEntCls = (C_CLS_PUNT *) lista_cls->get_head();

   // controllo che la tipologia sia giusta.
   if (check_graph_compatib(pEntCls->ent) == GS_BAD)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // verifico che l'oggetto grafico non sia già legato ad un record di questa classe
   if (eed.load(pEntCls->ent) == GS_GOOD || getKeyValue(pEntCls->ent, &new_key) == GS_GOOD)
      { GS_ERR_COD = eGSGEOsimObjFound; return GS_BAD; }

   // se si tratta di un elemento nodale
   if (id.type == TYPE_TEXT || id.type == TYPE_NODE)
   {  // se il nodo non rispetta le regole di sovrapposizione
      ads_point pIns;

      if (gsc_get_firstPoint(pEntCls->ent, pIns) == GS_BAD) return GS_BAD;
      if (pPtObjsBTree)
      {
         if (gsc_OverlapValidation(pIns, this, *((C_POINT_OBJS_BTREE*) pPtObjsBTree)) == GS_BAD)
            { GS_ERR_COD = eGSOverlapValidation; return GS_BAD; }
      }
      else
         if (gsc_OverlapValidation(pIns, this) == GS_BAD)
            { GS_ERR_COD = eGSOverlapValidation; return GS_BAD; }
   }

   // per ottimizzare i tempi non verifico gli oggetti successivi nella lista
   // "lista_cls" ma do per scontato che siano gli oggetti della stessa simulazione
   // a cui il primo oggetto grafico della lista deve collegarsi topologicamente

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(INSERT);

   do
   {
      // ricavo codice per prossimo inserimento
      if ((new_key = GetNewEntCode()) <= 0) break;
      // modifico <key_attrib>
      gsc_RbSubst(pKey, new_key);

      // eventuale chiamata ad una funzione utente esterna (prima dell'inserimento o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_INSERT, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
		   if (!InfoList.get_head())
            if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			      break;
         if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                  &ColValues, visib_block, Reason) == GS_BAD) break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_INSERT, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
		      if (!InfoList.get_head())
               if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			         break;
            if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                     &ColValues, visib_block, Reason) == GS_BAD) break;
         }

      eed.cls    = id.code;
      eed.sub    = id.sub_code;
      eed.num_el = 1; // fattore di aggregazione forzato ad 1
      eed.gs_id  = new_key;

      // Collego il primo oggetto grafico alla tabella
      if (OrigLink.Set(NewEnt, id.code, id.sub_code, new_key, INSERT) == GS_BAD)
         { result = GS_BAD; break; }
      InsLink = TRUE; // inserimento link avvenuto

      // inserisco etichette "entità di GEOsim" senza inserirlo nel gruppo 
      // di selezione del salvataggio (lo faccio dopo)
      if (eed.save(NewEnt, GS_BAD) == GS_BAD) break; 

      // Purtoppo per le simulazioni sono costretto a copiare comunque l'oggetto
      // indipendentemente dall'esistenza di qualche azione sull'evento cancellazione
      // Infatti questo tipo di classe è soggetto a controlli topologici i quali
      // necessitano di intrappolare anche gli eventi UNDO e per far ciò bisogna che
      // l'oggetto sia inserito nel database grafico di ACAD corredato delle EED

      // Faccio la copia di una sola entità e cancello la vecchia (è sufficiente una sola)
      // perchè l'inserimento dell'oggetto grafico nel database di autocad 
      // dovrebbe avvenire con le entità estese di GEOsim per gestire l'UNDO.
      if (gsc_DeepClone2ModSpace(NewEnt) != GS_GOOD || gsc_EraseEnt(NewEnt) != GS_GOOD)
         { GS_ERR_COD = eGSErrorExecutingCommand; break; }
      if (acdbEntLast(NewEnt) != RTNORM) { GS_ERR_COD = eGSInvEntityOp; break; }

      // Collego il nuovo oggetto grafico alla tabella
      if (Link.Set(NewEnt, id.code, id.sub_code, new_key, INSERT) == GS_BAD)
         { result = GS_BAD; break; }

      // sostituisco entità nella lista degli oggetti grafici (parametro della funzione)
      ads_name_set(NewEnt, ((C_CLS_PUNT*) lista_cls->get_head())->ent);

      if (id.type == TYPE_POLYLINE)
      { 
         C_TOPOLOGY topo;

         // topologia dinamica di GEOsim
         if (lista_cls->get_TopoLinkInfo(NULL, NULL, &init_node_sub, &initial_node,
                                         &final_node_sub, &final_node) == GS_BAD)
            break;

         topo.set_type(TYPE_POLYLINE);                      // tipologia di tipo rete
         topo.set_cls(GS_CURRENT_WRK_SESSION->find_class(id.code)); // per classe madre
         if (topo.editlink(id.sub_code, new_key, NewEnt,
                           init_node_sub, initial_node,
                           final_node_sub, final_node) == GS_BAD) break;
         // Aggiorno i dati della eed perchè mi può servire la C_EED::save per
         // aggiornare il fattore di aggregazione in caso di inserimento del blocco DA
         // e il nodo iniziale e finale per i campi calcolati
         eed.set_initial_node(initial_node);
         eed.set_final_node(final_node);
      }

      // inserisco record in tabella (fa anche il ricalcolo e la validazione)
      C_SELSET SelSet;
      SelSet.add(NewEnt);
      if (ins_row(ColValues, NULL, NULL, pRsIns, &SelSet) == GS_BAD) break;
      InsRecord = TRUE; // inserimento record avvenuto

      // FINO QUI OK

      if (gs_id) *gs_id = new_key;

      if (id.type != TYPE_TEXT && 
          visib_block == VISIBLE && p_attrib_list->is_visible() == GS_GOOD)
      {
         C_RB_LIST      VisValues;
         C_ATTRIB_BLOCK attrib_block;
         C_SET_VISIB    *pActiveSet = NULL;

         eed.num_el++; // + 1 scheda attributi

         // ricavo gli attributi visibili
         if ((VisValues << p_attrib_list->get_vis_values(ColValues)) == GS_BAD)
            break;
         if (attrib_block.set_layer(ptr_fas()->layer) == GS_BAD) break;
         // altezza testo degli attributi del blocco
         if (attrib_block.set_h_text(ptr_fas()->h_text) == GS_BAD) break;
         // stile testo degli attributi del blocco
         if (attrib_block.set_text_style(ptr_fas()->style) == GS_BAD) break;
         // colore
         if (attrib_block.set_color(ptr_fas()->color) == GS_BAD) break;

         if (GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO)
         {
            ads_point point;
            double    rot;

            if (id.type == TYPE_POLYLINE) // punto medio tratto + lungo
            {
               if (gsc_getPntRtzOnObj(NewEnt, point, &rot, _T("MML"), 0, 0, GS_GOOD) == GS_BAD) break;
            }
            else if (id.type == TYPE_NODE) // punto di inserimento
            {
               if (gsc_getPntRtzOnObj(NewEnt, point, &rot, _T("S"), 0, 0, GS_GOOD) == GS_BAD) break;
            }
            else if (id.type == TYPE_SURFACE) // centroide
            {
               if (gsc_getPntRtzOnObj(NewEnt, point, &rot, _T("C"), 0, 0, GS_GOOD) == GS_BAD) break;
            }
            else break;

            attrib_block.set_insert_point(point);
            attrib_block.set_rotation(gsc_rad2grd(rot));
         }

         if ((result = attrib_block.insert(VisValues, id.code, id.sub_code, new_key,
                                           ptr_fas()->file_ref_block, ptr_fas()->ref_block,
                                           GEOsimAppl::GLOBALVARS.get_InsPos(),
                                           eed.num_el)) != GS_GOOD)
            break;
         result = GS_BAD;
         
         // inserimento blocco attributi avvenuto
         if (acdbEntLast(newDABlock) != RTNORM) { GS_ERR_COD = eGSInvEntityOp; break; }

         // aggiorno il n. di aggregazioni "entità di GEOsim" a oggetto grafico
         // e lo inserisco nel gruppo di selezione del salvataggio
         if (eed.save(NewEnt) == GS_BAD) break;
      }
      else
         if (id.type == TYPE_TEXT)
            // aggiorno il testo
            if (updtoDA(NewEnt, ColValues, GS_GOOD) == GS_BAD) break;

      // se esistono funzioni di calcolo grafico vengono applicate ora
      if (graph_calc(ColValues) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (dopo l'inserimento)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_INSERT, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
		   if (!InfoList.get_head())
            if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			      break;
         if (gsc_doActionOnInsert(UsrFunction.get_name(), NewEnt, InfoList.get_head(),
                                  &ColValues, visib_block, Reason) == GS_BAD) break;
      }

      gsc_addEnt2savess(NewEnt);
      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Inserted entity: key %ld, prj %d, class %d, subclass %d."),
               new_key, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      // aggiorno la lista degli oggetti puntuali
      if (pPtObjsBTree)
         (((C_POINT_OBJS_BTREE*) pPtObjsBTree))->add_ent(NewEnt);

      result = GS_GOOD;
   }
   while (0);

   if (result != GS_GOOD)
   {
      if (InsLink) // se erano stati inseriti i link
      {
         ads_name OrigEnt;

         pEntCls = (C_CLS_PUNT *) lista_cls->get_head(); // cancellazione dei link

         // se la prima entità è diversa da quella di OrigLink
         // significa che era stata cancellata
         OrigLink.GetEnt(OrigEnt);
         if (ads_name_equal(NewEnt, OrigEnt))
            OrigLink.erase();

         Link.erase(pEntCls->ent);
	      eed.clear(pEntCls->ent);
         if (id.type == TYPE_POLYLINE)
         {
            C_TOPOLOGY topo;

            topo.set_type(TYPE_POLYLINE);                      // tipologia di tipo rete
            topo.set_cls(GS_CURRENT_WRK_SESSION->find_class(id.code)); // per classe madre
            topo.editdelelem(new_key, TYPE_POLYLINE);
         }
      }
      if (InsRecord) // se era stato inserito il record
         Ins_Ent_Into_GsDelete(new_key); // inserimento in GS_DELETE
 
      if (!ads_name_nil(newDABlock)) // se era stato inserito il blocco attributi
         gsc_EraseEnt(newDABlock);
   }

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_SUB::UpdTopoLinksOnNode              <internal> */
/*+                                                                       
  Aggiorna la topologia sui link indicati per inserimento di un nodo.
  Parametri:
  ads_name entity;		                Oggetto grafico
  C_RB_LIST &ColValues;                 Lista ((colonna valore)..)
  C_PREPARED_CMD_LIST *pTempOldCmdList; Lista di comandi per lettura dati TEMP (1 elemento)
                                        e OLD (2 elemento) (in caso di letture multiple);
                                        default = NULL.
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/  
/*********************************************************/
/*
int C_SUB::UpdTopoLinksOnNode(
      // se esistono link che devono essere connessi a questo nodo
      if (TopoClsList.get_count() > 0)
      {
         // modifico la connessione dei link
         pTopoCls = (C_CLS_PUNT *) TopoClsList.get_head();
         while ()
         {
            int C_SUB::upd_data(long gs_id, C_RB_LIST &ColValues, _CommandPtr &pTempCmd, 
                    _RecordsetPtr &pInsRs, C_SELSET *GlobalSelSet, int Reason)

            pTopoCls = (C_CLS_PUNT *) TopoClsList.get_next();
         }
      }
}
*/


/*********************************************************/
/*.doc C_SUB::query_data <external> */
/*+                                                                       
  Interroga la scheda dell'entita' della sotto-classe.
  Parametri:
  ads_name entity;		                Oggetto grafico
  C_RB_LIST &ColValues;                 Lista ((colonna valore)..)
  C_PREPARED_CMD_LIST *pTempOldCmdList; Lista di comandi per lettura dati TEMP (1 elemento)
                                        e OLD (2 elemento) (in caso di letture multiple);
                                        default = NULL.
  
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/  
/*********************************************************/
int C_SUB::query_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
{                 
   long key;

   // verifico che l'oggetto grafico sia già legato ad un record di questa classe
   if (getKeyValue(entity, &key) == GS_BAD) return GS_BAD;
   return query_data(key, ColValues, pTempOldCmdList);
}
int C_SUB::query_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
{                   
   if (gsc_check_op(opQryEntity) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
   
   if (pTempOldCmdList)
      return gsc_get_data(*pTempOldCmdList, gs_id, ColValues);
   else
   {
      C_PREPARED_CMD pTempCmd;

      // Compilo le istruzioni di lettura dei dati della classe dal temp
      if (prepare_data(pTempCmd, TEMP) == GS_BAD) return GS_BAD; 
      ColValues.remove_all();
      return gsc_get_data(pTempCmd, gs_id, ColValues);
   }
}


/*********************************************************/
/*.doc C_SUB::upd_data                        <external> */
/*+                                                                       
  Aggiorna la scheda dell'entita' della sotto-classe
  Parametri:
  ads_name entity;      	 Oggetto grafico
  C_RB_LIST &ColValues;	    Lista (colonna-valore)
  C_SELSET *GlobalSelSet;   Gruppo di selezione globale per la scheda corrente
                            (default = NULL)
  int      Reason;          origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)
  oppure:

  ads_name entity;      	 Oggetto grafico
  C_RB_LIST &ColValues;	    Lista (colonna-valore)
  C_PREPARED_CMD *pTempCmd; Comando per lettura dati TEMP
  C_SELSET *GlobalSelSet;   Gruppo di selezione globale per la scheda corrente
                            (default = NULL)
  int         Reason;       origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::upd_data(ads_name entity, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet,
                    int Reason)
{
   return upd_data(entity, ColValues, NULL, GlobalSelSet, Reason);
}
int C_SUB::upd_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd, 
                    C_SELSET *GlobalSelSet, int Reason)
{                 
   long KeyValue;

   // Legge il valore chiave
   if (getKeyValue(entity, &KeyValue) == GS_BAD) return GS_BAD;

   return upd_data(KeyValue, ColValues, pTempCmd, GlobalSelSet,  Reason);
}
int C_SUB::upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet, int Reason)
{
   return upd_data(gs_id, ColValues, NULL, GlobalSelSet, Reason);
}
int C_SUB::upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd, 
                    C_SELSET *GlobalSelSet, int Reason)
{                 
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();
   C_SELSET       SelSet;
   C_STRING       TempTableRef, UsrFunction;
   int            result = GS_BAD, WhyNotUpd, OldOp;
   _RecordsetPtr  pRs;
   int            IsRecordsetCloseable = GS_GOOD;
   C_LINK_SET     LinkSet;

   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(MODIFY);
   
   do 
   {
      if (GlobalSelSet == NULL)  // se non è passato come parametro, lo ricavo
      {
         int set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;

         if (get_SelSet(gs_id, SelSet) == GS_BAD) break;
         if (is_updateableSS(gs_id, SelSet, &WhyNotUpd, set_lock,
                             TryToExtractPartialEnt) == GS_BAD)
            { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; break; }
      }
      else
      {
         if (GlobalSelSet->copy(SelSet) == GS_BAD) break;
         if (is_updateableSS(gs_id, SelSet, &WhyNotUpd) == GS_BAD)
            { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; break; }
      }

      // modifico "key_attrib" con gs_id
      if (ColValues.CdrAssocSubst(ptr_info()->key_attrib.get_name(), gs_id) == GS_BAD)
      { // aggiungo il valore chiave
         presbuf p;

         if ((p = acutBuildList(RTLB, RTLB,
                                RTSTR, ptr_info()->key_attrib.get_name(),
                                RTLONG, gs_id, RTLE, 0)) == NULL) 
            { GS_ERR_COD = eGSOutOfMem; break; }
         ColValues.remove_head();
         ColValues.link_head(p);
      }

      // validazione e ricalcolo dati
      if (CalcValidData(ColValues, MODIFY, &SelSet) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (prima della modifica o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                  gs_id, &ColValues, Reason) == GS_BAD) break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_UPDATE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
            if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                     gs_id, &ColValues, Reason) == GS_BAD) break;
            // validazione e ricalcolo dati ("gsc_doActionOnUpdate" potrebbe cambiare qualcosa)
            if (CalcValidData(ColValues, MODIFY, &SelSet) == GS_BAD) break;
         }

      // è SICURAMENTE nel temporaneo

      // lettura dei dati della classe dal temp
      if (pTempCmd == NULL)
      {
         C_PREPARED_CMD pCmd;

         if (prepare_data(pCmd, TEMP) == GS_BAD) break;
         if (gsc_get_data(pCmd, gs_id, pRs, &IsRecordsetCloseable) == GS_BAD) break;
         // aggiorno il record
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
            { if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
         if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
      }
      else 
      {
         if (gsc_get_data(*pTempCmd, gs_id, pRs, &IsRecordsetCloseable) == GS_BAD) break;
         // aggiorno il record
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
            { if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
         if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
      }
         
      if (updtoDA(SelSet, ColValues) == GS_BAD) break;

      // se esistono funzioni di calcolo grafico vengono applicate ora
      if (graph_calc(ColValues, &SelSet) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (dopo la modifica)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                  gs_id, &ColValues, Reason) == GS_BAD)
            break;

      // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
      if (gsc_addSS2savess(SelSet) == GS_BAD) break;
      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Updated entity: key %ld, prj %d, class %d, subclass %d."),
               gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_SUB::erase_data(ads_name Entity)     <external> */
/*+                                                                       
  Cancella un entita della classe.
  Parametri:
  ads_name Entity;      	Oggetto da cancellare
  int      Reason;         origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                           RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                           (default = UNKNOWN_MOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::erase_data(ads_name Entity, int Reason)
{
   long           gs_id, graphObj;
   int            result = GS_BAD, last_echo, WhyNotUpd, OldOp;
   int            set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;
   C_EED          eed;
   C_SELSET       SelSet;
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();
   C_RB_LIST      ColValues;
   C_DBCONNECTION *pConn;
   bool           EraseEnt;

   // verifico l'abilitazione dell'utente;
   if (gsc_check_op(opDelEntity) == GS_BAD) return GS_BAD;
     
   // leggo valore chiave e il gruppo di selezione
   if (get_Key_SelSet(Entity, &gs_id, SelSet) == GS_BAD) return GS_BAD;

   // verifico che l'elemento sia cancellabile 
   if (is_updateableSS(gs_id, SelSet, &WhyNotUpd, set_lock,
                       TryToExtractPartialEnt) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   eed.cls    = id.code;
   eed.sub    = id.sub_code;
   eed.num_el = SelSet.length(); // numero di oggetti grafici
   eed.num_el = eed.num_el - 1;   // tolgo 1 a num_elem

   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   int IsTransactionSupported = pConn->BeginTrans();

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(ERASE);

   do
   {
      if (gsc_set_echo(0, &last_echo) == GS_BAD) break;

      // Se non si tratta di un blocco di attributi
      if (gsc_is_DABlock(Entity) == GS_GOOD) EraseEnt = false;
      else
      // Se si tratta di un riempimento di superficie
      if (id.type == TYPE_SURFACE && gsc_ishatch(Entity) == GS_GOOD) EraseEnt = false;
      else
      {
         // Se è una principale devo scorrere tutti i Link e vedere se ci sono altre 
         // principali, se non ci sono altre princ. allora devo cancellare in grafica
         // tutte le schede collegate, se no cancello tutte le entita collegate.
         if (SelSet.is_presentGraphicalObject(&graphObj) == GS_BAD) break;
         // Se era l'ultimo oggetto principale
         EraseEnt = (graphObj == 1) ? true : false;
      }

      if (EraseEnt) // cancellazione dell'entità
      {
         TCHAR Handle[MAX_LEN_HANDLE];

         gsc_enthand(Entity, Handle);

         // cancello le entita collegate perchè cancellando la princ.
         // devo cancellare tutte le schede collegate

         // eventuale chiamata ad una funzione utente esterna (prima della cancellazione o in
         // completa sostituzione della funzione di GEOsim)
         C_STRING UsrFunction;
         if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_ERASE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
            if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
               break;
            result = GS_GOOD;
            break;
         }
         else
            if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_ERASE, UsrFunction) == GS_GOOD &&
                UsrFunction.len() > 0)
               if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
                  break;

         if (gsc_addSS2savess(SelSet) == GS_BAD) break;
         if (SelSet.Erase() != GS_GOOD) break;

         if (Ins_Ent_Into_GsDelete(gs_id) == GS_BAD)
         {
            SelSet.UnErase(); // ripristino oggetto
            break;
         }

         // eventuale chiamata ad una funzione utente esterna (dopo la cancellazione)
         if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_ERASE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
            if (gsc_doActionOnErase(UsrFunction.get_name(), Entity, Reason) == GS_BAD)
               result = GS_BAD;

         // allineo la topologia (in caso di cancellazione)
         C_STR_BTREE HandleList;
         C_SELSET    ToAlign;

         HandleList.add(&Handle);
         gsc_topo_align_for_erased(HandleList, ToAlign, &(GEOsimAppl::REFUSED_SS));
         if (id.type == TYPE_POLYLINE)
            gsc_verify_topo_on_align_for_node(ToAlign);
         //              selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
         gsc_class_align(ToAlign, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), 
                         GS_BAD, RECORD_MOD);

         // Notifico in file log
         TCHAR Msg[MAX_LEN_MSG];
         swprintf(Msg, MAX_LEN_MSG, _T("Erased entity: key %ld, prj %d, class %d, subclass %d."),
                  gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
         gsc_write_log(Msg);
      }
      else
      { 
         // aggiorno n. aggregate su tutti gli oggetti
         // aggiungendo in GEOsimAppl::SAVE_SS per salvataggio (vedi C_EED::save)
         if (eed.save_aggr(SelSet, eed.num_el) == GS_BAD) { result = GS_BAD; break; }

         if (gsc_EraseEnt(Entity) != GS_GOOD) break;
         SelSet.subtract_ent(Entity);

         C_PREPARED_CMD_LIST CmdList;

         // Preparo i comandi di lettura dei dati della classe dal temp/old
         if (prepare_data(CmdList) == GS_BAD) break;

         // Se c'è almeno un attributo calcolato dalla grafica
         if (p_attrib_list->is_from_graph())
         {
            // leggo scheda
            if (query_data(gs_id, ColValues) == GS_GOOD)
            {  // Aggiorno database e video
               if (upd_data(gs_id, ColValues, &SelSet) == GS_BAD)
               {
                  acdbEntDel(Entity); // ripristino oggetto
                  break;
               }
            }
            else // se non c'è più la scheda cancello lo stesso
               if (GS_ERR_COD == eGSInvalidKey)
               {
                  acdbEntDel(Entity); // ripristino oggetto
                  break;
               }
         }
      }

      result = GS_GOOD;
   }
   while (0);
   
   if (gsc_set_echo(last_echo) == GS_BAD) result = GS_BAD;

   if (result != GS_GOOD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); }
   else
   {
      if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();
      setModified(GS_GOOD); // classe modificata
   }
   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_SUB::insert_data(ads_name Entity) <external> */
/*+                                                                       
  Copia un entita della classe.
  Parametri:
  ads_name entity;      	Oggetto da copiato
  long OldKey;             Vecchio codice a cui era legato l'oggetto 
                           (nel caso di blocco attributi)
  long *NewKey;            Nuovo codice (nel caso di oggetto grafico)
                           default = NULL
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::insert_data(ads_name Entity, long OldKey, long *NewKey)
{
   long            new_key;
   C_EED           eed;
   C_SELSET        entSS;
   int             result = GS_BAD;
   C_RB_LIST       ColValues;
   C_CLS_PUNT_LIST lista_cls;
   C_CLS_PUNT      *pClsPunt;

   if ((pClsPunt = new C_CLS_PUNT(this, Entity)) == NULL) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   lista_cls.add_tail(pClsPunt);

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;

   // verifico che ci sia un'area attiva;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   // controllo che la tipologia sia giusta.
   if (check_graph_compatib(Entity) == GS_BAD)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   do
   {  // se non è una entità di GEOsim
      if (eed.load(Entity) == GS_BAD)
      {
         if (gsc_is_DABlock(Entity) == GS_GOOD)
            { GS_ERR_COD = eGSInvClassType; break; }

         // leggo la scheda di default
         if (get_default_values(ColValues, Entity) == GS_BAD) break;
         if (ins_data(&lista_cls, ColValues, &new_key, INVISIBLE) == GS_BAD)
            break;
         result = GS_GOOD; 
      }
      else  // è una entità di GEOsim
      {
         C_LINK Link;

         // inserisco nuovo link
         if (Link.Set(Entity, id.code, id.sub_code, OldKey, MODIFY) == GS_BAD) break;

         if (gsc_is_DABlock(Entity) == GS_GOOD)
         {  // Se blocco attributi allora incremento di 1 il quante in tutti
            // gli oggetti collegati
            long OldKey;

            // leggo valore chiave e il gruppo di selezione
            if (get_Key_SelSet(Entity, &OldKey, entSS) == GS_BAD) break;

             // aggiorno n. aggregate su tutti gli oggetti
            if (eed.save_aggr(entSS, entSS.length()) == GS_BAD) { result = GS_BAD; break; }

            // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
            if (gsc_addSS2savess(entSS) == GS_BAD) break;
            setModified(GS_GOOD); // classe modificata
         }
         else
         {  // se è un oggetto grafico va inserita anche una nuova scheda
            // contenente gli stessi dati della copia.
            if (query_data(Entity, ColValues) == GS_BAD)
               { result = GS_BAD; break; }
            // cancella tutti i Link di GEOsim di una entità
            if (Link.erase() == GS_BAD) { result = GS_BAD; break; }
            // cancello etichetta di GEOsim
            eed.clear();

            if (ins_data(&lista_cls, ColValues, &new_key, INVISIBLE) == GS_BAD)
               break;
         }
         result = GS_GOOD;
      }
   }
   while (0);

   if (NewKey) *NewKey = new_key;

   return result;
}


/*********************************************************/
/*.doc C_SUB::update_data <external> */
/*+                                                                       
  Funzione di aggiornamento di una entità.
  Parametri:
  ads_name entity;      	Oggetto da aggiornare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::update_data(ads_name Entity)
{
   C_RB_LIST ColValues;
   C_SELSET  SelSet;

   // Leggo i dati dal database
   if (query_data(Entity, ColValues) == GS_BAD) return GS_BAD;
   // ricavo i collegamenti con la grafica
   if (get_SelSet(ColValues, SelSet) == GS_BAD) return GS_BAD;
   // Aggiorno i dati con eventuali attributi visibili
   if (updfromDA(ColValues, Entity, &SelSet) == GS_BAD) return GS_BAD;

   // se è superficie
   if (get_type() == TYPE_SURFACE)
   {
      TCHAR    tmpHatch[MAX_LEN_HATCHNAME], tmpLayer[MAX_LEN_LAYERNAME];
      double   tmpScale, tmpRotation;
      C_COLOR  tmpColor;
         
      // controllo se c'è almeno un riempimento
      if (gsc_getInfoHatchSS(SelSet, tmpHatch, &tmpScale, &tmpRotation,
                           tmpLayer, &tmpColor) == GS_GOOD)
         // Aggiorno i riempimenti modificando ls
         if (gsc_UpdToDefHatch(SelSet, tmpHatch, tmpScale, gsc_rad2grd(tmpRotation),
                               &tmpColor, tmpLayer) == GS_BAD)
            return GS_BAD;
   }

   // Aggiorno il database
   if (upd_data(Entity, ColValues, &SelSet) != GS_GOOD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SUB::ins_row <external> */
/*+                                                                       
  Inserisce un record nella tabella temporanea della classe.
  Parametri:
  C_RB_LIST       &ColValues;       Lista ((<nome colonna><valore>) ...)
  						            		di tutti gli attributi della classe
  long            *gs_id;           Puntatore al codice della nuova scheda;
                                    se = NULL non viene ricalcolato
  C_CLS_PUNT_LIST *lista_cls_punt;  Non usato (presente solo per compatibilità)
  _RecordsetPtr    *pRsIns;         Istruzione precompilata per inserimento dati,
                                    se non presente verrà inizializzata
                                    dalla funzione stessa (default = NULL).
  C_SELSET *GlobalSelSet;           Gruppo di selezione globale per la scheda corrente
                                    (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::ins_row(C_RB_LIST &ColValues, long *gs_id, C_CLS_PUNT_LIST *lista_cls_punt,
                   _RecordsetPtr pRsIns, C_SELSET *GlobalSelSet)
{
   long   new_key;
   C_INFO *p_info = ptr_info();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;
   
   if (gs_id)
   {
      // ricavo codice per prossimo inserimento
      if ((new_key = GetNewEntCode()) <= 0) return GS_BAD;

      // modifico (key_attrib)
      if (ColValues.CdrAssocSubst(p_info->key_attrib.get_name(), new_key) == GS_BAD) return GS_BAD;
   }
   else
      if (getKeyValue(ColValues, &new_key) == GS_BAD) return GS_BAD;

   // validazione e ricalcolo dati
   if (CalcValidData(ColValues, INSERT, GlobalSelSet) == GS_BAD) return GS_BAD;

   // aggiungo row in tabella temporanea classe
   if (pRsIns)
   {
      if (gsc_DBInsRow(pRsIns, ColValues) == GS_BAD) return GS_BAD;
   }
   else
   {
      C_DBCONNECTION *pConn;
      C_STRING       TempTableRef;

      if ((pConn = p_info->getDBConnection(TEMP)) == GS_BAD) return GS_BAD;

      // ricavo tabella temporanea
      if (getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

      if (pConn->InsRow(TempTableRef.get_name(), ColValues) == GS_BAD) return GS_BAD;
   }

   // aggiorno last nella sottoclasse e anche nelle altre dello stesso tipo
   // (puntuali, lineari, superficie)
   if (set_last(new_key) == GS_BAD) return GS_BAD;
   
   if (gs_id) (*gs_id) = new_key;

   return GS_GOOD;
}

 
/*********************************************************/
/*.doc (new 2) C_SUB::GetNewEntCode <external> */
/*+                                                                       
  Ricava codice valido per prossimo inserimento.
  Parametri:

  Restituisce n>0 in caso di successo altrimenti restituisce 0.
-*/  
/*********************************************************/
long C_SUB::GetNewEntCode()
{
   return ptr_info()->TempLastId + 1;
}


/*********************************************************/
/*.doc (new 2) C_SUB::is_updateable <internal> */
/*+                                                                       
  Verifica la possibilità di modificare l'oggetto.
  Se la variabile "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento 
  delle entità nel salvataggio) questa funzione non effettua alcun controllo 
  e restituisce sempre GS_GOOD.
  Parametri:
  ads_name  entity;           Oggetto grafico
  int *WhyNot;                Codice rappresentante il motivo per cui non è aggiornabile
                              (default = NULL)
  int set_lock;               flag se = GS_GOOD la funzione prova a bloccare l'entità
                              (se questa era risultata aggiornabile) default = GS_GOOD
  int TryToExtractPartialEnt; Se = GS_GOOD e l'entità non risulta aggiornabile perchè
                              parzialmente estratta la funzione effettua l'estrazione
                              di tutte le istanze grafiche dell'entità MODIFICANDO
                              IL PARAMETRO <SelSet> (default GS_BAD).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::is_updateable(ads_name entity, int *WhyNot, int set_lock,
                         int TryToExtractPartialEnt)
{                 
   long     key;
   C_SELSET entSS;

   // leggo valore chiave e il gruppo di selezione
   if (get_Key_SelSet(entity, &key, entSS) == GS_BAD) return GS_BAD;
   return is_updateableSS(key, entSS, WhyNot, set_lock, TryToExtractPartialEnt);
}
int C_SUB::is_updateable(long gs_id, int *WhyNot, int set_lock,
                         int TryToExtractPartialEnt)
{                 
   C_SELSET entSS;

   if (get_SelSet(gs_id, entSS) == GS_BAD) return GS_BAD;
   return is_updateableSS(gs_id, entSS, WhyNot, set_lock, TryToExtractPartialEnt);
}

/*********************************************************/
/*.doc C_SUB::is_updateable                   <internal> */
/*+                                                                       
  Verifica la possibilità di modificare un'entità.
  Parametri:
  long gs_id;                 Codice entità
  C_SELSET &SelSet;           Gruppo di selezione degli oggetti in grafica 
                              appartenenti alla stessa entità
  int *WhyNot;                Flag rappresentante il motivo per cui non è aggiornabile
                              (default = NULL)
  int set_lock;               flag se = GS_GOOD la funzione prova a bloccare l'entità
                              (se questa era risultata aggiornabile) (default = GS_GOOD)
  int TryToExtractPartialEnt; Se = GS_GOOD e l'entità non risulta aggiornabile perchè
                              parzialmente estratta la funzione effettua l'estrazione
                              di tutte le istanze grafiche dell'entità MODIFICANDO
                              IL PARAMETRO <SelSet> (default GS_BAD).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::is_updateableSS(long gs_id, C_SELSET &SelSet, int *WhyNot, int set_lock,
                           int TryToExtractPartialEnt)
{
   int      WhyNotUpdateable, Result;
   ads_name ss;

   SelSet.get_selection(ss);

   Result = is_updateableSS(gs_id, ss, &WhyNotUpdateable, set_lock);

   // Se l'entità non è modificabile perchè estratta parzialmente
   // e si vuole tentare un'estrazione totale dell'entità
   if (Result == GS_BAD && WhyNotUpdateable == eGSPartialEntExtract && TryToExtractPartialEnt == GS_GOOD)
      // Provo ad estrarre totalmente le istanze grafiche dell'entità
      if (entExtract(gs_id) == GS_GOOD && get_SelSet(gs_id, SelSet) == GS_GOOD)
      {
         SelSet.get_selection(ss);
         Result = is_updateableSS(gs_id, ss, &WhyNotUpdateable, set_lock);
      }

   if (WhyNot) *WhyNot = WhyNotUpdateable;

   return Result;
}
// il SelSet deve appartenere alla stessa entità GEosim
int C_SUB::is_updateableSS(long gs_id, ads_name SelSet, int *WhyNot, int set_lock)
{
   C_EED    eed;
   int      result = GS_GOOD;
   ads_name entity;
   long     Qty;

   if (WhyNot) *WhyNot = eGSUnknown;

   // verifico abilitazione
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(WhyNot) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      if (WhyNot) *WhyNot = GS_ERR_COD;
      return GS_BAD;
   }

   // se non si è abilitati a inserire in GEOsimAppl::SAVE_SS
   // ritorno sempre GS_GOOD perchè l'entità non verrà salvata
   if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD) return GS_GOOD;

   // ricavo il numero di oggetti del gruppo di selezione
   if (ads_sslength(SelSet, &Qty) != RTNORM || Qty <= 0)
      { GS_ERR_COD = eGSInvalidSelectionSet; return GS_BAD; }

   do
   {
      // ricavo l'oggetto grafico collegato
      if (acedSSName(SelSet, 0, entity) != RTNORM) { result = GS_BAD; break; }

      // carico i dati estesi di GEOsim
      if (eed.load(entity) == GS_BAD)
         { GS_ERR_COD = eGSGEOsimObjNotFound; result = GS_BAD; break; }
      // verifico se ho tutte le aggregate
      if (Qty < eed.num_el)
      {
         if (WhyNot) *WhyNot = eGSPartialEntExtract;
         result = GS_BAD; 
         break;
      }

      // se l'entità è nuova e non è ancora stata inserita in GEOsimAppl::SAVE_SS può essere
      // aggiornata ed inserita nel salvataggio solo se non ha regole di connessione
      // con altri oggetti.
      // Questo controlla la seguente casistica:
      // Con "AddEntityToSaveSet" = OFF si inserisce un lato di una simulazione acqua, 
      // successivamente con "AddEntityToSaveSet" = ON si aggiorna la scheda del lato
      // e si salva
      if (is_NewEntity(gs_id) == GS_GOOD) // entità nuova
      {
         C_EXTERN *p_mother;

         if ((p_mother = (C_EXTERN *) GS_CURRENT_WRK_SESSION->find_class(id.code)) == NULL)
            { result = GS_BAD; break; }

         if (GEOsimAppl::SAVE_SS.is_member(entity) == GS_BAD)
            if (p_mother->is_IndipendentSub(ptr_id()->sub_code) == GS_BAD)
               // ha regole di connessione con altri oggetti
               { GS_ERR_COD = eGSReferenceToTemporaryEnts; return GS_BAD; }
      }

      // Controllo topologico
   }
   while (0);

   return result;
}


/*********************************************************/
/*.doc C_SUB::is_NewEntity                    <external> */
/*+                                                                       
  Funzione che verifica se una entità di GEOsim è nuova. 
  Parametri:
  ads_name entity;   Entità grafica
  oppure
  long Key;          Codice entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::is_NewEntity(ads_name entity)
{
   long Key;

   // leggo valore chiave e il gruppo di selezione
   if (getKeyValue(entity, &Key) == GS_BAD) return GS_BAD;
   return is_NewEntity(Key);
}
int C_SUB::is_NewEntity(long Key)
{
   // Se il codice è più grande del maggior codice memorizzato nelle tabelle
   // OLD sitratta di un'entità nuova inserita nella sessione di lavoro
   return (Key > info.OldLastId) ? GS_GOOD : GS_BAD;
}


/*****************************************************************************/
/*.doc C_SUB::set_last <external> */
/*+                                                                       
  Aggiorna il codice temporaneo dell'ultima entità della sottoclasse.
  Se la sottoclasse risulta estratta in una sessione di lavoro viene 
  aggiornato anche il file GS_CLASS_FILE.
  La codificazione è unica per tutte le sottoclassi nodali (testi e nodi),
  unica per tutte le sottoclassi lineari (polilinee) e unica per tutte quelle
  superfici. 
  Parametri:
  long value;     codice ultima scheda inserita

  Ritorna GS_GOOD in caso di successo, altrimenti GS_BAD.
-*/  
/*****************************************************************************/
int C_SUB::set_last(long value)
{
   C_EXTERN *pMother;
   C_SUB    *pSub, *pPrevious;
   int      Skip;

   // Ritorna il puntatore alla classe madre
   if ((pMother = (C_EXTERN*) ((C_PROJECT *)(id.pPrj))->find_class(id.code)) == NULL) return GS_BAD;

   pPrevious = (C_SUB *) pMother->ptr_sub_list()->get_cursor();
   pSub      = (C_SUB *) pMother->ptr_sub_list()->get_head();
   while (pSub)
   {
      Skip = TRUE;
      switch (id.type)
      {
         case TYPE_TEXT: // se entità puntuali
         case TYPE_NODE: // aggiorno "last" nelle sottoclassi puntuali
            if (pSub->get_type() == TYPE_TEXT ||
                pSub->get_type() == TYPE_NODE) Skip = FALSE;
            break;
         case TYPE_POLYLINE: // se entità lineare aggiorno "last" nelle sottoclassi lineari
            if (pSub->get_type() == TYPE_POLYLINE) Skip = FALSE;
            break;
         case TYPE_SURFACE: // se entità superficie aggiorno "last" nelle sottoclassi superficie
            if (pSub->get_type() == TYPE_SURFACE) Skip = FALSE;
            break;
      }
      
      if (!Skip)
      {
         pSub->ptr_info()->TempLastId = value;
         // la C_INFO viene salvata su file dalla C_WRK_SESSION::freeze
      }

      pSub = (C_SUB *) pSub->get_next();
   }
   pMother->ptr_sub_list()->set_cursor(pPrevious);
    
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SUB::align_for_inserted            <internal> */
/*+
  Questa funzione riallinea gli oggetti del gruppo di selezione.
  Se l'oggetto è un blocco attributi o un testo allora verrà
  aggregato se modificabile altrimenti saranno cancellati i collegamenti a GEOsim.
  Se si tratta di un oggetto grafico di tipo diverso (es. polilinea) verrà inserito
  una nuova entità di GEOsim della stessa classe derivando gli attributi
  dall'entità che lo ha generato.
  Parametri:
  C_SELSET *SelSet;   Gruppo di selezione degli oggetti della classe
                      N.B.: Gli oggetti del gruppo di selezione vengono
                      eliminati dal gruppo man mano che vengono elaborati !
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)
  int Reason;         flag a bit, origine della modifica sugli oggetti da allineare:
                      - NO_EXTERN_ACTION (non intraprende azioni esterne)
                      (default = UNKNOWN_MOD)

  Restituisce il numero di oggetti riallineati in caso di successo altrimenti -1.
  N.B. SelSet conterrà le entità nuove. Questa funzione verrà chiamata prima per le
       entità nodali e dopo per quelle lineari.
-*/  
/*********************************************************/
long C_SUB::align_for_inserted(C_SELSET &SelSet, int CounterToVideo, int Reason)
{
	ads_name       entity;
	long           SelSet_len, qty = 0, i = 0, gs_id, Refused = 0, Locked = 0;
   int            Prcnt, WhyNotUpd;
   int            set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;
   C_EED          eed;
   C_INFO         *p_info = ptr_info();
   C_SELSET       entSelSet, NewSelSet;
   TCHAR          Msg[MAX_LEN_MSG];       // per notifica in file .log
   TCHAR          Handle[MAX_LEN_HANDLE]; // per notifica in file .log
   C_STRING       TempTableRef;
   C_RB_LIST      ColValues;
   C_LINK         Link;
   C_DBCONNECTION *pTempConn;
   _RecordsetPtr  pInsRs;
   C_PREPARED_CMD_LIST TempOldCmdList;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1081)); // "Riallineamento banca dati"

   // verifico l'abilitazione dell'utente
   // bisogna avere entrambe le abilitazioni es. inserimento di un
   // blocco DA significa nuova aggregata = modifica dell'entità
   if (gsc_check_op(opInsEntity) == GS_BAD || gsc_check_op(opAggrEntity) == GS_BAD)
      return -1;

   if (!GS_CURRENT_WRK_SESSION) return 0;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return 0;

   // se non ci sono oggetti nel gruppo di selezione
   if ((SelSet_len = SelSet.length()) <= 0) return 0;

   if (CounterToVideo == GS_GOOD)
   {
      acutPrintf(gsc_msg(113), id.name); // "\n\nEntità classe %s:\n"
      StatusBarProgressMeter.Init(SelSet_len);
   }
   // se la classe non è modificabile
   if (id.abilit != GSUpdateableData)
   {
      while (SelSet.entname(i++, entity) == GS_GOOD)
      {
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(i);
         
         Link.erase(entity);
         eed.clear(entity);
         gsc_EraseEnt(entity);
      }

      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.End(gsc_msg(310), i); // "%ld entità GEOsim elaborate."

      return 0;
   }

   // ricavo le connessioni ai database
   if ((pTempConn = p_info->getDBConnection(TEMP)) == NULL) return -1;

   // ricavo la tabella temporanea
   if (getTempTableRef(TempTableRef) == GS_BAD) return -1;

   // Compilo le istruzioni di lettura dei dati della classe dal temp/old
   if (prepare_data(TempOldCmdList) == GS_BAD) return -1;

   // preparo istruzione per l'inserimento di record nella tabella temp
   if (pTempConn->InitInsRow(TempTableRef.get_name(), pInsRs) == GS_BAD) return -1;

   eed.cls = id.code;
   eed.sub = id.sub_code;

   if (id.type != TYPE_TEXT)
   {
      C_RB_LIST     NewNodeDefaultValues;
      C_SUB         *pNewNode = NULL;
      _RecordsetPtr pNewNodeInsRs;
      int           NewNodeIsDefCalc, InsDABlock;

      InsDABlock = (GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO) ? VISIBLE : INVISIBLE;

      if (id.type == TYPE_POLYLINE)
      {  // Ricavo la sottoclasse nodale da inserire a inizio e fine lato se ci fosse bisogno
         pNewNode = ((C_EXTERN *) GS_CURRENT_WRK_SESSION->find_class(id.code))->getJollyNodalSub(id.sub_code);

         if (pNewNode)
         {
            C_STRING NewNodeTempTableRef;

            // ricavo la tabella temporanea
            if (pNewNode->getTempTableRef(NewNodeTempTableRef) == GS_BAD)
               { gsc_DBCloseRs(pInsRs); return -1; }

            // preparo istruzione per l'inserimento di record nella tabella temp
            if (pTempConn->InitInsRow(NewNodeTempTableRef.get_name(), pNewNodeInsRs) == GS_BAD)
               { gsc_DBCloseRs(pInsRs); return -1; }

            // leggo i valori di default 
            if (pNewNode->get_default_values(NewNodeDefaultValues) == GS_BAD)
               { gsc_DBCloseRs(pInsRs); gsc_DBCloseRs(pNewNodeInsRs); return -1; } 

            NewNodeIsDefCalc = pNewNode->ptr_attrib_list()->is_DefCalculated();
         }
      }

      // ciclo su tutti gli oggetti che devono essere inseriti come nuovi
      // (scarto i testi e i blocchi di attributi)
      while (SelSet.entname(i, entity) == GS_GOOD)
      {
         if (gsc_is_DABlock(entity) == GS_GOOD) { i++; continue; }

         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set(++qty);

         SelSet.subtract_ent(entity); // sottraggo dal Selection Set

         Link.SetEnt(entity);
         // legge i dati
         if (Link.GetKey(&gs_id) == GS_BAD ||
             query_data(gs_id, ColValues, &TempOldCmdList) == GS_BAD)
         {
            // Notifico in file log
            gsc_enthand(entity, Handle);
            swprintf(Msg, MAX_LEN_MSG, _T("Invalid key on entity: handle <%s>, prj %d, class %d."),
                     Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code);      
            gsc_write_log(Msg);

            Link.erase();
            eed.clear(entity);
            gsc_EraseEnt(entity);
            Refused++;
            continue;
         }

         // cancella tutti i Link e la eed di GEOsim dell'entità
         if (Link.erase() == GS_BAD)
         {
            // Notifico in file log
            gsc_enthand(entity, Handle);
            swprintf(Msg, MAX_LEN_MSG, _T("LINK ASE deletion failed; handle <%s>, prj %d, class %d, subclass %d."),
                     Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
            eed.clear(entity);
            gsc_EraseEnt(entity);
            Refused++;
            continue;
         }        
      
         if (eed.clear(entity) == GS_BAD)
         {
            // Notifico in file log
            gsc_enthand(entity, Handle);
            swprintf(Msg, MAX_LEN_MSG, _T("LINK ASE or EED deletion failed; handle <%s>, prj %d, class %d, subclass %d."),
                     Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
            gsc_EraseEnt(entity);
            Refused++;
            continue;
         }        

         if (id.type == TYPE_POLYLINE)
         {
            int        InitNodeCreated, FinalNodeCreated;
            C_CLS_PUNT *pInitNode, *pFinalNode, *pLink;

            if (gsc_verify_topo_on_align_for_link(this, entity, pNewNode,
                                                  NewNodeDefaultValues,
                                                  NewNodeIsDefCalc,
                                                  pNewNodeInsRs,
                                                  &pInitNode, &InitNodeCreated,
                                                  &pFinalNode, &FinalNodeCreated) == GS_BAD)
            {
               gsc_DBCloseRs(pInsRs); gsc_DBCloseRs(pNewNodeInsRs);
               return GS_BAD;
            }

            C_CLS_PUNT_LIST lista_cls;

            if ((pLink = new C_CLS_PUNT(this, entity)) == NULL)
            {
               gsc_DBCloseRs(pInsRs); gsc_DBCloseRs(pNewNodeInsRs);
               free(pInitNode); free(pFinalNode);
               GS_ERR_COD = eGSOutOfMem;
               return GS_BAD;
            }
            lista_cls.add_tail(pLink);
            lista_cls.add_tail(pInitNode);
            lista_cls.add_tail(pFinalNode);

            // Inserisco il nuovo lato
            if (ins_data(&lista_cls, ColValues, NULL, InsDABlock, UNKNOWN_MOD, pInsRs.GetInterfacePtr()) == GS_BAD)
            {
               if (InitNodeCreated == GS_GOOD) pNewNode->erase_data(pInitNode->ent);
               if (FinalNodeCreated == GS_GOOD) pNewNode->erase_data(pFinalNode->ent);

               if (gsc_enthand(entity, Handle) == GS_GOOD) // se esiste ancora
               {
                  swprintf(Msg, MAX_LEN_MSG, _T("Record append failed on entity - handle <%s>, prj %d, class %d, subclass %d."),
                           Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
                  gsc_write_log(Msg);
               }                
               Link.erase();
               eed.clear(entity);
               gsc_EraseEnt(entity);
               Refused++;
               continue;
            }
            else
               NewSelSet.add(((C_CLS_PUNT *) lista_cls.get_head())->ent);
         }
         else if (id.type == TYPE_NODE)
         {
            // per oggetti nodali viene effettuato solo il controllo di sovrapposizione
            if (ins_data(entity, ColValues, NULL, InsDABlock, UNKNOWN_MOD, pInsRs.GetInterfacePtr()) == GS_BAD)
            {
               if (gsc_enthand(entity, Handle) == GS_GOOD) // se esiste ancora
               {
                  swprintf(Msg, MAX_LEN_MSG, _T("Record append failed on entity - handle <%s>, prj %d, class %d, subclass %d."),
                           Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
                  gsc_write_log(Msg);
               }                
               Link.erase();
               eed.clear(entity);
               gsc_EraseEnt(entity);
               Refused++;
               continue;
            }
            else
               NewSelSet.add(entity);
         }
      }

      gsc_DBCloseRs(pNewNodeInsRs);
   }

   // ciclo su tutti gli oggetti che devono essere aggregati
   // (testi oppure blocchi di attributi)
   while (SelSet.entname(0, entity) == GS_GOOD)
   {
      if (CounterToVideo == GS_GOOD)
      {
         Prcnt = (int) ((SelSet_len - SelSet.length()) * 100 / SelSet_len);
         StatusBarProgressMeter.Set_Perc(Prcnt);
      }

      // leggo valore chiave e il gruppo di selezione
      Link.SetEnt(entity);
      qty++;
      if (Link.GetKey(&gs_id) == GS_BAD || get_SelSet(gs_id, entSelSet) == GS_BAD)
      {
         // Notifico in file log
         gsc_enthand(entity, Handle);
         swprintf(Msg, MAX_LEN_MSG, _T("Invalid key on entity: handle <%s>, prj %d, class %d, subclass %d."),
                  Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);      
         gsc_write_log(Msg);

         Link.erase();
         eed.clear(entity);
         SelSet.subtract_ent(entity);
         gsc_EraseEnt(entity);
         Refused++;
         continue;
      }

      SelSet.subtract(entSelSet); // sottraggo dal Selection Set

      // se è aggiornabile viene bloccata
      if (is_updateableSS(gs_id, entSelSet, &WhyNotUpd, set_lock,
                          TryToExtractPartialEnt) == GS_GOOD)
      {
         // legge i dati
         if (query_data(gs_id, ColValues, &TempOldCmdList) == GS_BAD)
            { Link.erase(); eed.clear(entity); gsc_EraseEnt(entity); Refused++; continue; }

         // aggiorno n. aggregate su tutti gli oggetti (va in GEOsimAppl::SAVE_SS)
         if (eed.save_aggr(entSelSet, entSelSet.length()) == GS_BAD)
            { Link.erase(); eed.clear(entity); gsc_EraseEnt(entity); Refused++; continue; }

         if (upd_data(gs_id, ColValues,
                      ((C_PREPARED_CMD *) TempOldCmdList.get_head()), // Temp
                      &entSelSet) == GS_BAD)
         {
            // aggiorno n. aggregate su tutti gli oggetti (va in GEOsimAppl::SAVE_SS)
            eed.save_aggr(entSelSet, entSelSet.length() - 1);
            GEOsimAppl::SAVE_SS.subtract_ent(entity);
         
            Link.erase(); eed.clear(entity); gsc_EraseEnt(entity); Refused++; continue; 
         }

         // scrivo solo se la variabile globale "logfile" = GS_GOOD
         if (GEOsimAppl::GLOBALVARS.get_LogFile() == GS_GOOD)
         {
            // Notifico in file log
            swprintf(Msg, MAX_LEN_MSG, _T("Aggregated entity: key %ld, prj %d, class %d, subclass %d."),
                     gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
            gsc_write_log(Msg);
         }
      }
      else // se era già bloccata
      {
         gsc_enthand(entity, Handle);
         swprintf(Msg, MAX_LEN_MSG, _T("Locked entity - handle <%s>, prj %d, class %d, subclass %d."),
                  Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
         gsc_write_log(Msg);

         // cancella tutti i Link e la eed di GEOsim di una entità
         Link.erase(); eed.clear(entity); gsc_EraseEnt(entity);
         Refused++;
         if (WhyNotUpd == eGSObjectIsLockedByAnotherUser) Locked++;
      }
   }

   gsc_DBCloseRs(pInsRs);

   // Copio le entità nuove in SelSet
   NewSelSet.copy(SelSet);

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      if (qty >= 0)
      {
         acutPrintf(gsc_msg(308), qty, Refused); // "\n%ld entità GEOsim elaborate, %ld scartate."
         if (Locked > 0) acutPrintf(gsc_msg(770), Locked); // "\n%ld entità GEOsim bloccata/e da un' altro utente."
      }
   }
   return qty;
}


/*************************************************************/
/*.doc gsc_verify_topo_on_align_for_link          <internal> */
/*+
  La funzione verifica che, il link abbia ancora nodo iniziale e finale 
  e se non dovessero esserci li inserisce.
  Parametri:
  C_SUB    *pLinkSub;               Sottoclasse Link
  ads_name LinkEnt;                 Entità Link
  C_SUB    *pNewNode;               Sottoclasse nodale da inserire dove sia il caso (jolly)
  C_RB_LIST &NewNodeDefaultValues;  Valori di default da inserire per sottoclasse jolly
  int       IsDefCalc;              Flag; se = GS_GOOD i valori di default sono calcolati
  _RecordsetPtr *pRsSub;            Recordset per inserimento dati sottoclasse jolly
  C_CLS_PUNT **pInitNode;           Nodo iniziale (out)
  C_CLS_PUNT **pFinalNode;          Nodo finale   (out)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_verify_topo_on_align_for_link(C_SUB *pLinkSub, ads_name LinkEnt,
                                      C_SUB *pNewNode,
                                      C_RB_LIST &NewNodeDefaultValues,
                                      int NewNodeIsDefCalc,
                                      _RecordsetPtr &pNewNodeInsRs,
                                      C_CLS_PUNT **pInitNode, int *NewInitNode,
                                      C_CLS_PUNT **pFinalNode, int *NewFinalNode)
{
   C_POINT_LIST StartEndPts;
   C_POINT      *Pt;
   ads_name     PunctualEnt;
   C_SUB        *pPunctualSub;
   long         NodeKey;

   // ricavo il punto iniziale e finale del lato
   if (StartEndPts.add_sten_point(LinkEnt) == GS_BAD || StartEndPts.get_count() != 2)
      return GS_BAD;
            
   // verifico se il nuovo lato abbia un nodo compatibile coincidente al punto iniziale
   Pt = (C_POINT *) StartEndPts.get_head();
   *NewInitNode = GS_BAD;
   if (gsc_get_AdjNode(pLinkSub, Pt->point, &pPunctualSub, PunctualEnt) == GS_BAD)           
   {  // Se non esiste alcuna entità compatibile 
      if (pNewNode)
      {  // se esiste una sottoclasse nodale da utilizzare creo un nuovo nodo
         if (gsc_insert_new_node(pNewNode, Pt->point, PunctualEnt, &NodeKey,
                                 NewNodeDefaultValues, NewNodeIsDefCalc, pNewNodeInsRs) == GS_BAD)
            return GS_BAD;
         if ((*pInitNode = new C_CLS_PUNT(pNewNode, PunctualEnt, NodeKey)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         *NewInitNode = GS_GOOD;
      }
      else return GS_BAD;
   }
   else // esisteva un nodo compatibile da utilizzare
      if ((*pInitNode = new C_CLS_PUNT(pPunctualSub, PunctualEnt)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   // verifico se il nuovo lato abbia un nodo compatibile coincidente al punto finale.
   // Le informazioni sono lette direttamente dalla grafica.
   Pt = (C_POINT *) StartEndPts.get_next();
   *NewFinalNode = GS_BAD;
   if (gsc_get_AdjNode(pLinkSub, Pt->point, &pPunctualSub, PunctualEnt) == GS_BAD)           
   {  // Se non esiste alcuna entità compatibile 
      if (pNewNode)
      {  // se esiste una sottoclasse nodale da utilizzare creo un nuovo nodo
         if (gsc_insert_new_node(pNewNode, Pt->point, PunctualEnt, &NodeKey,
                                 NewNodeDefaultValues, NewNodeIsDefCalc, pNewNodeInsRs) == GS_BAD)
            return GS_BAD;
         if ((*pFinalNode = new C_CLS_PUNT(pNewNode, PunctualEnt, NodeKey)) == NULL)
            { free(*pInitNode); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         *NewFinalNode = GS_GOOD;
      }
      else
         { free(*pInitNode); return GS_BAD; }
   }
   else // esisteva un nodo compatibile da utilizzare
      if ((*pFinalNode = new C_CLS_PUNT(pPunctualSub, PunctualEnt)) == NULL)
         { free(*pInitNode); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   return GS_GOOD;
}


/*************************************************************/
/*.doc gsc_verify_topo_on_align_for_node          <internal> */
/*+
  La funzione verifica che, dopo aver inserito le entità puntuali e lineari 
  della classe simulazione attraverso il riallineamento da reattori, ogni
  entità nodale che per esistere necessitano di essere topologicamente connesse
  ad altre entità non siano isolate.
  Parametri:
  C_SELSET *SelSet;     Gruppo di selezione degli oggetti della classe
                        N.B.: Gli oggetti del gruppo di selezione vengono
                        eliminati dal gruppo man mano che vengono elaborati !
  int CounterToVideo;   Flag, se = GS_GOOD stampa a video il numero di entità che si 
                        stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_verify_topo_on_align_for_node(C_SELSET &SelSet, int CounterToVideo)
{
   C_SELSET   InternalSelSet, ClassSS;
   C_CLASS    *pNodeSub;
   C_TOPOLOGY topo;
   ads_name   entity;
   C_ID       *p_id;

   // se non ci sono oggetti nel gruppo di selezione
   if (SelSet.length() <= 0) return GS_GOOD;

   SelSet.copy(InternalSelSet);

   topo.set_type(TYPE_POLYLINE); // tipologia di tipo rete

   while (InternalSelSet.entname(0, entity) == GS_GOOD)
   {
      // Ritorna il puntatore alla classe cercata
      if ((pNodeSub = GS_CURRENT_WRK_SESSION->find_class(entity)) == NULL)
         { InternalSelSet.subtract_ent(entity); continue; }
      if (pNodeSub->is_subclass() == GS_BAD || !(p_id = pNodeSub->ptr_id()) ||
          p_id->type != TYPE_NODE)
         { InternalSelSet.subtract_ent(entity); continue; }

      InternalSelSet.copy(ClassSS);
      // Seleziono tutti gli oggetti della simulazione
      ClassSS.intersectClsCode(p_id->code);

      // classe madre
      topo.set_cls(GS_CURRENT_WRK_SESSION->find_class(p_id->code));

      topo.EraseIsolatedNodes(ClassSS);

      InternalSelSet.subtract(ClassSS);
   }

   return GS_GOOD;
}


/***********************************************************/
/*  FINE   FUNZIONI DELLA CATEGORIA C_SUB                  */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_EXTERN               */
/***********************************************************/


int C_EXTERN::set_last(long value) // si utilizzano le sottoclassi
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


/***********************************************************/
/*  FINE   FUNZIONI DELLA CATEGORIA C_EXTERN               */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_CLASS                */
/***********************************************************/


// inserimento dati
int C_CLASS::ins_data(ads_name lista_ent, C_RB_LIST &ColValues, long *gs_id, 
                      int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues,
	                   long *gs_id, int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

/*****************************************************************************/
/*.doc C_CLASS::data_to_html                                                 */
/*+
  Questa funzione esporta il contenuto della scheda in un file HTML.
  Parametri
  C_STRING &Path;                       Path completa file html
  long gs_id;                           Codice entità
  C_PREPARED_CMD_LIST *pTempOldCmdList; Lista di comandi per lettura dati TEMP (1 elemento)
                                        e OLD (2 elemento) (in caso di letture multiple);
                                        default = NULL.
  int What;                             Flag di filtro (default = ALL)
                                        GRAPHICAL per stampare la regione grafica
                                        RECORD per stampare i valori degli attributi
  C_STR_LIST *PrintableAttrNameList;    Lista con i nomi dei soli attributi da stampare
                                        (default = NULL cioè tutti gli attributi)
  const TCHAR *Mode;                    Modo di apertura del file html ("w" distrugge 
                                        il contenuto, "a" aggiunge al contenuto)
                                        (default = _T("w"))
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************************************************************/
int C_CLASS::data_to_html(const TCHAR *Path, long gs_id, 
                          C_PREPARED_CMD_LIST *pTempOldCmdList,
                          int What, C_STR_LIST *PrintableAttrNameList,
                          const TCHAR *Mode)
{
   C_STRING      TitleBorderColor("#808080"), TitleBgColor("#c0c0c0");
   C_STRING      BorderColor("#00CCCC"), BgColor("#99FFFF");
   C_STRING      Msg, PrjName, ClassName, FieldName, ImagePath, _Mode(Mode);
   C_ATTRIB_LIST *pAttribList;
   C_ATTRIB      *pAttrib;
   presbuf       pName, pVal;
   int           i = 0, Result = GS_BAD;
   FILE          *file;
   C_RB_LIST     ColValues;

   if (!(pAttribList = ptr_attrib_list())) return GS_BAD;
   if (query_data(gs_id, ColValues, pTempOldCmdList) == GS_BAD) return GS_BAD;

   PrjName = get_pPrj()->get_name();
   get_CompleteName(ClassName);

   PrjName.toHTML();
   ClassName.toHTML();

   if (What == ALL || What == GRAPHICAL) // creo la bitmap temporanea
   {
      C_SELSET entSS;

      if (get_SelSet(gs_id, entSS) == GS_GOOD)
      {
         CBitmap   Image;
         HANDLE    hDIB;
         ads_point LeftLowerPt, RightUpperPt;

         // Leggo le coordinate limite delo schermo
         if (gsc_getActualScreeRect(LeftLowerPt, RightUpperPt) == GS_BAD)
            return GS_BAD;

         entSS.zoom(GEOsimAppl::GLOBALVARS.get_AutoZoomMinXDim(),
                    GEOsimAppl::GLOBALVARS.get_AutoZoomMinYDim());
	      acedUpdateDisplay();
      
         gsc_CaptureWindowScreen(adsw_acadDocWnd(), Image);
         hDIB = DDBToDIB(Image);
         if (gsc_dir_from_path(Path, ImagePath) == GS_GOOD)
         {
            ImagePath += _T("\\ZOOM_ON_CLS");
            ImagePath += id.code;
            ImagePath += _T("_SUB");
            ImagePath += id.sub_code;
            ImagePath += _T("_ENT");
            ImagePath += gs_id;
            ImagePath += _T(".BMP");
            gsc_WriteDIB(ImagePath.get_name(), hDIB);
         }
    
         GlobalFree(hDIB);

         // Zoom precedente
         gsc_zoom(LeftLowerPt, RightUpperPt);
	      acedUpdateDisplay();
      }
   }


   if (_Mode.comp(_T("a")) == 0) // se modo era append
      if (gsc_path_exist(Path) == GS_BAD) _Mode = _T("w");

   if ((file = gsc_fopen(Path, _Mode.get_name())) == NULL) return GS_BAD;

   do
   {
      if (_Mode.comp(_T("w")) == 0) // creazione html
         // Intestazione
         if (fwprintf(file, _T("<html>\n<head>\n<title>Scheda GEOsim</title>\n</head>\n<body bgcolor=\"#FFFFFF\">\n")) < 0)
            break;

      if (fwprintf(file, _T("\n<table bordercolor=\"%s\" bgcolor=\"%s\" width=\"100%%\" border=\"1\">"),
                   TitleBorderColor.get_name(), TitleBgColor.get_name()) < 0)
         break;

      // Progetto: Classe:
      if (fwprintf(file, _T("\n<tr><td align=\"center\"><b><font size=\"4\">Progetto: %s<br>Classe: %s"),
                   PrjName.get_name(), ClassName.get_name()) < 0)
         break;

      if (fwprintf(file, _T("</font></b></td></tr></table><br>")) < 0)
         break;
      
      if (ImagePath.get_name()) // stampo bitmap della zona
      {
         gsc_nethost2drive(ImagePath);
         if (fwprintf(file, _T("<td><table width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\"><tr>")) < 0)
            break;
         if (fwprintf(file, _T("<td align=\"middle\"><img src=\"%s\" height=\"300\" width\"400\"></td></tr></table></td></tr><br>"),
                      ImagePath.get_name()) < 0)
            break;
      }

      if (What == ALL || What == RECORD) // stampo valori degli attributi
      {
         // intestazione tabella
         if (fwprintf(file, _T("\n<table bordercolor=\"%s\" cellspacing=\"2\" cellpadding=\"2\" border=\"1\">"),
                      BorderColor.get_name()) < 0)
            break;

         Result = GS_GOOD;
         while ((pName = ColValues.nth(i++)))
         {
            pVal = gsc_nth(1, pName);
            pName = gsc_nth(0, pName);

            if (PrintableAttrNameList) // Stampo solo gli attributi in lista
               if (!PrintableAttrNameList->search_name(pName->resval.rstring, FALSE))
                  continue;

            if ((pAttrib = (C_ATTRIB *) pAttribList->search_name(pName->resval.rstring, 
                                                                 FALSE)) == NULL)
               { Result = GS_BAD; break; }
            pAttrib->ParseToString(pVal, Msg, &ColValues, id.code, id.sub_code);
            Msg.toHTML();
            FieldName = pAttrib->Caption;
            FieldName.toHTML();
            if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\" width=\"30%%\"><b>%s:</b></td><td width=\"70%%\">%s</td></tr>"),
                         BgColor.get_name(), FieldName.get_name(), Msg.get_name()) < 0)
               { Result = GS_BAD; break; }
         }
         
         if (Result == GS_BAD) break;
         Result = GS_BAD;

         // fine tabella
         if (fwprintf(file, _T("\n</table>")) < 0) break;
      }

      if (_Mode.comp(_T("w")) == 0) // creazione html
         // fine html
         if (fwprintf(file, _T("\n</body></html>")) < 0) break;

      Result = GS_GOOD;
   }
   while (0);

   gsc_fclose(file);

   return Result;
}


// interrogazione dati
/*********************************************************/
/*.doc C_SIMPLEX::query_data <external> */
/*+                                                                       
  Interroga la scheda delle entita' della classe. I valori dello stesso attributo 
  che sono diversi tra 2 o più entità sono posti = NIL.
  Parametri:
  C_LONG_BTREE &KeyList;    Lista di codici delle entità da interrogare
  C_RB_LIST &ColValues;     Lista (colonna-valore) dei SOLI attributi da aggiornare
  int CounterToVideo;       flag, se = GS_GOOD stampa a video il numero di entità che si 
                            stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/  
/*********************************************************/
int C_CLASS::query_data(C_LONG_BTREE &KeyList, C_RB_LIST &ColValues,
                        int CounterToVideo)
{
   C_PREPARED_CMD_LIST TempOldCmdList;
   C_BLONG             *pKey = (C_BLONG *) KeyList.go_top();
   C_RB_LIST           _ColValues;
   long                i, Qty = 0;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(343)); // "Interrogazione entità"

   if (!ptr_info() || !ptr_attrib_list())
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
     
   i = 1;
   if (CounterToVideo == GS_GOOD) acutPrintf(GS_LFSTR);
      StatusBarProgressMeter.Init(KeyList.get_count());

   ColValues.remove_all();
   while (pKey)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      if (query_data(pKey->get_key(), _ColValues, &TempOldCmdList) == GS_GOOD)
      {
         if (!ColValues.get_head()) _ColValues.copy(ColValues);
         else // Confronto i campi annullando quelli diversi
            ColValues.SubstRTNONEtoDifferent(_ColValues); // no case sensitive

         Qty++;
      }

      i++;
      pKey = (C_BLONG *) KeyList.go_next();
   }

   i--;
   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      acutPrintf(gsc_msg(308), i, i - Qty); // "\n%ld entità GEOsim elaborate, %ld scartate."
   }
   if (ColValues.GetCount() == 0)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_query_AllData_from_temp            <internal> */
/*+                                                                       
  Funzione interna di ausilio a C_CLASS::query_AllData che interroga le scheda delle entita' della classe
  nella tabella temporanea.
  Parametri:
  C_CLASS *pCls;              Classe da interrogare
  C_STRING &StrKeyList;       Lista di codici delle entità da interrogare in formato stringa
  C_RB_LIST &ColValues;       Lista di resbuf contenente i dati delle schede.
                              (((<attr><val>)...) ((<attr><val>)...) ...)
  C_RB_LIST &SingleColValues; Lista di resbuf contenente i dati di una singola scheda
                              in cui i valori dello stesso attributo che sono diversi tra 
                              2 o più entità sono posti = NIL.
                              ((<attr><val>)...) ((<attr><val>)...)
  C_LONG_BTREE &TempKeyList;  Lista dei codici trovati nella tabella temporanea

  Restituisce il numero di schede lette in caso di successo altrimenti -1.  
-*/  
/*********************************************************/
int gsc_query_AllData_from_temp(C_CLASS *pCls, C_STRING &StrKeyList, C_RB_LIST &ColValues, C_RB_LIST &SingleColValues,
                                C_LONG_BTREE &TempKeyList)
{
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   C_STRING       TempTableRef, statement, Field;
   C_RB_LIST      _ColValues;
   presbuf        pRbKey;
   long           Key, i = 0;

   // Ricavo la connessione OLE - DB
   if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return -1;

   Field = pInfo->key_attrib;
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return -1;

   // Leggo dal TEMP
   if (pCls->getTempTableRef(TempTableRef) == GS_BAD) return -1;
   statement = _T("SELECT * FROM ");
   statement += TempTableRef;
   statement += _T(" WHERE ");
   statement += Field;
   statement += _T(" IN (");
   statement += StrKeyList;
   statement += _T(") ORDER BY ");
   statement += Field;

   if (pConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD)
      return -1;
   if (gsc_InitDBReadRow(pRs, _ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return -1; }
   if (!(pRbKey = _ColValues.CdrAssoc(pInfo->key_attrib.get_name())))
      { gsc_DBCloseRs(pRs); return -1; }

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      i++;
      if (gsc_DBReadRow(pRs, _ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return -1; }
      gsc_rb2Lng(pRbKey, &Key);
      TempKeyList.add(&Key);
      if (!(ColValues += gsc_rblistcopy(_ColValues.get_head())))
         { gsc_DBCloseRs(pRs); return -1; }

      if (!SingleColValues.get_head()) _ColValues.copy(SingleColValues);
      else // Confronto i campi annullando quelli diversi
         SingleColValues.SubstRTNONEtoDifferent(_ColValues); // no case sensitive

      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   return i;
}
/*********************************************************/
/*.doc gsc_query_AllData_from_old            <internal> */
/*+                                                                       
  Funzione interna di ausilio a C_CLASS::query_AllData che interroga le scheda delle entita' della classe
  nella tabella old.
  Parametri:
  C_CLASS *pCls;              Classe da interrogare
  C_STRING &StrKeyList;       Lista di codici delle entità da interrogare in formato stringa
  C_RB_LIST &ColValues;       Lista di resbuf contenente i dati delle schede.
                              (((<attr><val>)...) ((<attr><val>)...) ...)
  C_RB_LIST &SingleColValues; Lista di resbuf contenente i dati di una singola scheda
                              in cui i valori dello stesso attributo che sono diversi tra 
                              2 o più entità sono posti = NIL.
                              ((<attr><val>)...) ((<attr><val>)...)

  Restituisce il numero di schede lette in caso di successo altrimenti -1.  
-*/  
/*********************************************************/
int gsc_query_AllData_from_old(C_CLASS *pCls, C_STRING &StrKeyList, C_RB_LIST &ColValues, C_RB_LIST &SingleColValues)
{
   C_INFO         *pInfo = pCls->ptr_info();
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   C_STRING       statement, Field;
   C_RB_LIST      _ColValues;
   presbuf        pRbKey, pRbPrevRow, pRb;
   long           Key, TempKey, i = 0;

   // Ricavo la connessione OLE - DB
   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return -1;

   Field = pInfo->key_attrib;
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return -1;

   statement = _T("SELECT * FROM ");
   statement += pInfo->OldTableRef;
   statement += _T(" WHERE ");
   statement += Field;
   statement += _T(" IN (");
   statement += StrKeyList;
   statement += _T(") ORDER BY ");
   statement += Field;

   if (pConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD)
      return -1;
   if (gsc_InitDBReadRow(pRs, _ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return -1; }
   if (!(pRbKey = _ColValues.CdrAssoc(pInfo->key_attrib.get_name())))
      { gsc_DBCloseRs(pRs); return -1; }
   pRbPrevRow = ColValues.get_head(); // posizione il cursore all'inizio

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      i++;
      if (gsc_DBReadRow(pRs, _ColValues) == GS_BAD) return -1; 
      gsc_rb2Lng(pRbKey, &Key);

      // Cerco la posizione giusta nel risultato
      while (pRbPrevRow->rbnext) // se c'è una riga
      {
         // leggo il codice chiave della riga
         if (!(pRb = gsc_nth(1, gsc_assoc(pInfo->key_attrib.get_name(), pRbPrevRow->rbnext, FALSE))))
            return -1;
         gsc_rb2Lng(pRbKey, &TempKey);
         if (Key < TempKey) break;
         
         pRbPrevRow = gsc_scorri(pRbPrevRow->rbnext); // vado alla chiusa tonda successiva
      }

      // Inserisco il record nella posizione del cursore
      ColValues.set_cursor(pRbPrevRow);
      ColValues.insert_after(_ColValues);
      pRbPrevRow = gsc_scorri(pRbPrevRow->rbnext); // vado alla chiusa tonda successiva

      if (!SingleColValues.get_head()) _ColValues.copy(SingleColValues);
      else // Confronto i campi annullando quelli diversi
         SingleColValues.SubstRTNONEtoDifferent(_ColValues); // no case sensitive

      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   return i;
}
/*********************************************************/
/*.doc C_CLASS::query_AllData                 <external> */
/*+                                                                       
  Interroga tutte le scheda delle entita' della classe.
  Parametri:
  C_LONG_BTREE &KeyList;      Lista di codici delle entità da interrogare
  C_RB_LIST &ColValues;       Lista di resbuf contenente i dati delle schede.
                              (((<attr><val>)...) ((<attr><val>)...) ...)
  C_RB_LIST &SingleColValues; Lista di resbuf contenente i dati di una singola scheda
                              in cui i valori dello stesso attributo che sono diversi tra 
                              2 o più entità sono posti = NIL.
                              ((<attr><val>)...) ((<attr><val>)...)
  int CounterToVideo;         Flag, se = GS_GOOD stampa a video il numero di entità che si 
                              stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/  
/*********************************************************/
int C_CLASS::query_AllData(C_LONG_BTREE &KeyList, C_RB_LIST &ColValues, C_RB_LIST &SingleColValues,
                           int CounterToVideo)
{       
   C_BLONG        *pKey;
   C_LONG_BTREE   TempKeyList;
   C_STRING       StrKeyList;
   long           Key, qty, tot = 0;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(343)); // "Interrogazione entità"

   if (gsc_check_op(opQryEntity) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   if (!ptr_info() || !ptr_attrib_list())
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (KeyList.get_count() == 0) return GS_GOOD;

   ColValues.remove_all();
   SingleColValues.remove_all();

   if (!(ColValues << acutBuildList(RTLB, 0))) return GS_BAD;

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.Init(gsc_msg(1048), LARGE_STEP);

   if (isDataModified() == GS_GOOD)
   {
      qty = 0;
      pKey = (C_BLONG *) KeyList.go_top();
      while (pKey)
      {
         if (qty != 0) StrKeyList += _T(",");
         StrKeyList += pKey->get_key();

         qty++;
         if ((qty % LARGE_STEP) == 0) // ogni 1000 per velocizzare
         {
            if ((qty = gsc_query_AllData_from_temp(this, StrKeyList, ColValues, SingleColValues, TempKeyList)) == -1)
               return GS_BAD;
            tot += qty;
            if (CounterToVideo == GS_GOOD)
               StatusLineMsg.Set(tot); // "%ld entità lette."

            qty = 0;
            StrKeyList.clear();
         }
         pKey = (C_BLONG *) KeyList.go_next();
      }
      if (StrKeyList.get_name())
      {
         if ((qty = gsc_query_AllData_from_temp(this, StrKeyList, ColValues, SingleColValues, TempKeyList)) == -1)
            return GS_BAD;
         tot += qty;
         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(tot); // "%ld entità lette."
      }
   }

   // Tutti i record non trovati nel TEMP vengono letti dall'OLD
   StrKeyList.clear();
   qty = 0;
   pKey = (C_BLONG *) KeyList.go_top();
   while (pKey)
   {
      Key = pKey->get_key();
      if (TempKeyList.search(&Key) == NULL)
      {
         if (qty != 0) StrKeyList += _T(",");
         StrKeyList += Key;

         qty++;
         if ((qty % LARGE_STEP) == 0) // ogni 1000 per velocizzare
         {
            if ((qty = gsc_query_AllData_from_old(this, StrKeyList, ColValues, SingleColValues)) == -1)
               return GS_BAD;
            tot += qty;
            if (CounterToVideo == GS_GOOD) 
               StatusLineMsg.Set(tot); // "%ld entità lette."

            qty = 0;
            StrKeyList.clear();
         }
      }
      pKey = (C_BLONG *) KeyList.go_next();
   }
   if (StrKeyList.get_name())
   {
      if ((qty = gsc_query_AllData_from_old(this, StrKeyList, ColValues, SingleColValues)) == -1)
         return GS_BAD;
      tot += qty;
      if (CounterToVideo == GS_GOOD)
         StatusLineMsg.Set(tot); // "%ld entità lette."
   }

   if (ColValues.GetCount() == 1)
      ColValues.remove_all();
   else
      if (!(ColValues += acutBuildList(RTLE, 0))) return GS_BAD;

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.End(gsc_msg(1048), tot); // "%ld entità lette."

   return GS_GOOD;
}


int C_CLASS::query_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::query_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::query_data(ads_point pt, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

// aggiornamento dati
/*********************************************************/
/*.doc C_CLASS::upd_data                      <external> */
/*+                                                                       
  Aggiorna la scheda dell'entita' della classe con una nuova serie di valori.
  Se la scheda non era stata ancora modificata la funzione la inserisce 
  nella tabella temporanea.
  Parametri:
  C_LONG_LIST &KeyList;     Lista di codici delle entità da aggiornare
  C_RB_LIST &ColValues;     Lista ((<colonna><valore>[<operatore>[<Perc>]])(...)) 
                            dei SOLI attributi da aggiornare
  int      Reason;          origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)
  int CounterToVideo;       flag, se = GS_GOOD stampa a video il numero di entità che si 
                            stanno elaborando (default = GS_BAD)

  <colonna>   = resbuf tipo stringa rappresentante il nome dell'attributo
  <valore>    = resbuf rappresentante un valore (numero, stringa, logico ...)
  <operatore> = resbuf tipo stringa rappresentante l'operatore da applicare al
                valore dell'attributo ("+", "-", "*", "/")
  <Perc>      = resbuf tipo logico che indica se il valore è assoluto (RTNIL) oppure
                è in percentuale del valore dell'attributo (se non presente viene 
                considerato RTNIL)

  Ad esempio (("DIAMETRO" "+" 10 T)("COSTO" "-" 5 nil)) significa che:
  il valore dell'attributo DIAMETRO deve essere aumentato del 10%
  il valore dell'attributo COSTO deve essere diminuito di 5

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B.: Nel caso di superficie con riempimento, questa funzione non riallinea
        il riempimento ad un eventuale cambiamento del perimetro.
-*/  
/*********************************************************/
int C_CLASS::upd_data(C_LONG_BTREE &KeyList, C_RB_LIST &ColValues, int Reason,
                      int CounterToVideo)
{
   C_BLONG     *pKey = (C_BLONG *) KeyList.go_top();
   C_LONG_LIST _KeyList;

   while (pKey)
   {
      _KeyList.add_tail_long(pKey->get_key());
      pKey = (C_BLONG *) KeyList.go_next();
   }

   return upd_data(_KeyList, ColValues, Reason, CounterToVideo);
}
int C_CLASS::upd_data(C_LONG_LIST &KeyList, C_RB_LIST &ColValues, int Reason,
                      int CounterToVideo)
{
   C_LONG              *pKey = (C_LONG *) KeyList.get_head();
   long                i, Qty = 0, j;
   int                 WhyNotUpd;
   bool                IsUpdateable;
   presbuf             rbField, pOperat, pPerc, pVal, pPrevVal;
   TCHAR               *pName;
   C_RB_LIST           OldColValues;
   C_PREPARED_CMD_LIST TempOldCmdList;
   C_STRING            TempTableRef;
   C_SELSET            EntSS;
   C_ATTRIB_LIST       *pAttribList = ptr_attrib_list();
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(340)); // "Modifica entità"

   if (!ptr_info() || !pAttribList)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // preparo il comando per la lettura in TEMP e OLD
   if (prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;
   // creo la tabella TEMP
   if (getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

   i = 1;
   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(KeyList.get_count());

   GEOsimAppl::REFUSED_SS.clear(); // Pulisco il gruppo degli oggetti rifiutati

   while (pKey)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      if (query_data(pKey->get_id(), OldColValues, &TempOldCmdList) == GS_BAD)
         { i++; pKey = (C_LONG *) KeyList.get_next(); continue; }

      j = 0;
      // Modifico i valori dell'entità
      while ((rbField = ColValues.nth(j++)) != NULL)
      {
         pName   = gsc_nth(0, rbField)->resval.rstring;
         pVal    = gsc_nth(1, rbField);
         pOperat = gsc_nth(2, rbField);

         if (pOperat && pOperat->restype == RTSTR) // bisogna eseguire un'operazione
         {
            bool IsPerctValue = FALSE;

            if ((pPerc = gsc_nth(3, rbField)) && pPerc->restype == RTT)
               IsPerctValue = TRUE;

            pPrevVal = OldColValues.CdrAssoc(pName);
            // se l'attributo di destinazione è di tipo carattere
            if (gsc_DBIsChar(((C_ATTRIB *) pAttribList->search_name(pName, FALSE))->ADOType) == GS_GOOD)
            {  // se l'operatore è "+" faccio la concatenazione tra stringhe
               if (gsc_strcmp(pOperat->resval.rstring, _T("+")) == 0)
               {
                  TempTableRef = pPrevVal;
                  TempTableRef += pVal;
                  gsc_RbSubst(pPrevVal, TempTableRef.get_name());
               }                 
            }
            else
               gsc_rbExeMathOp(pPrevVal, pVal, pOperat->resval.rstring, IsPerctValue);
         }
         else
            OldColValues.CdrAssocSubst(pName, pVal);
      }

      // Prima di chiamare la upd_data verifico se l'entità è modificabile
      // se non lo fosse perchè estratta parzialmente devo chiudere la
      // transazione di ASE, estrarre gli oggetti dell'entità e riaprire la transazione
      // altrimenti la funzione ade_qrydefine dà errore e mi lascia aperta la
      // transazione senza possibilità di richiuderla
      IsUpdateable = TRUE;
      if (ptr_fas())
      {
         if (get_SelSet(pKey->get_id(), EntSS) == GS_BAD)
            { i++; pKey = (C_LONG *) KeyList.get_next(); continue; }

         // Se non è aggiornabile perchè estratta parzialmente (senza estraz. automatica)
         if (is_updateableSS(pKey->get_id(), EntSS, &WhyNotUpd, GS_GOOD, GS_BAD) == GS_BAD)
            if (WhyNotUpd == eGSPartialEntExtract)
            {
               // Riprovo con estraz. automatica
               if (is_updateableSS(pKey->get_id(), EntSS, &WhyNotUpd, GS_GOOD, GS_GOOD) == GS_BAD)
                  IsUpdateable = FALSE;
            }
            else
               IsUpdateable = FALSE;

         if (IsUpdateable &&
             upd_data(pKey->get_id(), OldColValues,
                      ((C_PREPARED_CMD *) TempOldCmdList.get_head()), // Comando per TEMP
                      &EntSS, Reason) == GS_GOOD)
            Qty++;
         else // Aggiungo gli oggetti nel gruppo di selezione degli oggetti rifiutati
            GEOsimAppl::REFUSED_SS.add_selset(EntSS);
      }
      else // Classe senza grafica (es. gruppo)
      {
         if (is_updateable(pKey->get_id(), &WhyNotUpd, GS_GOOD, GS_BAD) == GS_BAD)
            if (WhyNotUpd == eGSPartialEntExtract)
            {
               // Riprovo con estraz. automatica
               if (is_updateable(pKey->get_id(), &WhyNotUpd, GS_GOOD, GS_GOOD) == GS_BAD)
                  IsUpdateable = GS_BAD;
            }
            else
               IsUpdateable = FALSE;

         if (IsUpdateable &&
             upd_data(pKey->get_id(), OldColValues,
                      ((C_PREPARED_CMD *) TempOldCmdList.get_head()), // Comando per TEMP
                      NULL, Reason) == GS_GOOD)
            Qty++;
         else // Aggiungo gli oggetti nel gruppo degli oggetti rifiutati
            GEOsimAppl::REFUSED_SS.add_selset(EntSS);
      }

      i++;
      pKey = (C_LONG *) KeyList.get_next();
   }

   i--;
   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      acutPrintf(gsc_msg(308), i, i - Qty); // "\n%ld entità GEOsim elaborate, %ld scartate."
   }

   return GS_GOOD;
}

int C_CLASS::upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet, int Reason)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                      C_SELSET *GlobalSelSet, int Reason)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::upd_data(ads_name entity, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet, int Reason)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::upd_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                      C_SELSET *GlobalSelSet, int Reason)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
// cancellazione dati
int C_CLASS::erase_data(long gs_id, int Reason)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::erase_data(ads_name ent, int Reason)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
// Per i comandi di AutoCAD con Reattori.
int C_CLASS::insert_data(ads_name ent, long OldKey, long *NewKey)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::update_data(ads_name ent)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

long C_CLASS::import_data(C_SELSET &objGraph, C_SELSET &objDA, int loadGSent, 
                          long BitForChangeToNewFAS, C_FAS *pNewFas,
                          C_RB_LIST &ColValues, C_CLASS *pObj, _RecordsetPtr pInsRs, int *objLocked)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::import_data(C_SELSET &objGraph, C_RB_LIST &ColValue, C_SELSET &ToDel, _RecordsetPtr pInsRs, 
                         C_CLASS *pObj, int loadGSent,
                         long BitForChangeToNewFAS, C_FAS *pNewFas)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
      
// AGGREGAZIONE E DISAGGREGAZIONE DATI
int C_CLASS::aggr_data(ads_name newSS, ads_name ent, long *gs_id)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::disaggr_data(ads_name ent, long *gs_id)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

// controllo modificabilità
int C_CLASS::is_updateable(long gs_id, int *WhyNot, int set_lock,
                           int TryToExtractPartialEnt)
   { if (WhyNot) *WhyNot = eGSUnknown; GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::is_updateable(ads_name entity, int *WhyNot, int set_lock, int TryToExtractPartialEnt)
   { if (WhyNot) *WhyNot = eGSUnknown; GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::is_updateableSS(long gs_id, C_SELSET &SelSet, int *WhyNot, int set_lock,
                             int TryToExtractPartialEnt)
   { if (WhyNot) *WhyNot = eGSUnknown; GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::is_updateableSS(long gs_id, ads_name SelSet, int *WhyNot, int set_lock)
   { if (WhyNot) *WhyNot = eGSUnknown; GS_ERR_COD = eGSInvClassType; return GS_BAD; }
// entità nuova
int C_CLASS::is_NewEntity(long Key) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_CLASS::is_NewEntity(ads_name entity) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

// gestione link per gruppi
int C_CLASS::set_status_link_ent(long gs_id, int value)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

long C_CLASS::align_for_inserted(C_SELSET &SelSet, int CounterToVideo, int Reason)
   { return 0; }


/*****************************************************************************/
/*.doc C_CLASS::set_last <external> */
/*+                                                                       
  Aggiorna il codice dell'ultima entità della classe e se la classe è estratta
  in una sessione di lavoro viene aggiornato anche il file GS_CLASS_FILE.
  Parametri:
  long value;     codice ultima scheda inserita

  Ritorna GS_GOOD in caso di successo, altrimenti GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS::set_last(long value)
{
   C_INFO *p_info = ptr_info();
   
   if (!p_info) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   
   p_info->TempLastId = value;

   // la C_INFO viene salvata su file dalla C_WRK_SESSION::freeze

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_CLASS::setModified                                      <external> */
/*+                                                                       
  Aggiorna il flag che identifica una classe modificata e se la classe è estratta
  in una sessione di lavoro viene aggiornato anche il file GS_CLASS_FILE.
  Parametri:
  int value;

  Ritorna GS_GOOD in caso di successo, altrimenti GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS::setModified(int value)
{
   if (value == id.modified) return GS_GOOD;
   
   id.modified = value;

   if (is_extracted()) // se classe estratta
   {
      C_STRING pathfile;
      TCHAR    buf[ID_PROFILE_LEN];

      // salvo modifica in GS_CLASS_FILE
      GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile, id.code);
      swprintf(buf, ID_PROFILE_LEN, _T("%d.%d"), id.code, id.sub_code);
      if (id.ToFile(pathfile, buf) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}

   
/*****************************************************************************/
/*.doc C_CLASS::isDataModified                                    <external> */
/*+                                                                       
  Ritorna GS_GOOD se c'è stato almeno un cambiamento della banca dati della 
  classe (inserimento, cancellazione, modifica) altrimenti GS_BAD.
-*/  
/*****************************************************************************/
int C_CLASS::isDataModified(void)
{
   // se modificabile ed estratta
   if (id.abilit == GSUpdateableData && is_extracted() == GS_GOOD && id.modified == GS_GOOD)
      return GS_GOOD;
   else
      return GS_BAD;
}


/*********************************************************/
/*.doc C_CLASS::prepareInsRs                  <internal> */
/*+
  Questa funzione prepara un recordset per l'inserimento di righe 
  nella tabella OLD o TEMP.
  Parametri:
  _RecordsetPtr &pInsRs;   RecordSet da preparare
  int           Type;      Tabella temporanea o old (default = TEMP)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_CLASS::prepareInsRs(_RecordsetPtr &pInsRs, int Type)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;

   if (!ptr_info()) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (Type == OLD) // ricavo il riferimento alla tabella old
      TableRef = ptr_info()->OldTableRef;
   else
      // ricavo il riferimento alla tabella temporanea
      if (getTempTableRef(TableRef) == GS_BAD) return GS_BAD; 

   // Ricavo le connessioni ai database
   if ((pConn = ptr_info()->getDBConnection(Type)) == NULL) return GS_BAD;
   // preparo istruzione per l'inserimento di record nella tabella
   if (pConn->InitInsRow(TableRef.get_name(), pInsRs) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}

  
/*********************************************************/
/*.doc C_CLASS::ins_row                       <internal> */
/*+                                                                       
  Inserisce un record nella tabella temporanea della classe.
  Per le classi senza rappresentazione grafica diretta, se la variabile
  "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento entità nel
  salvataggio) questa funzione inserisce le relazioni temporanee marcandole
  in modo che non vengano considerate dal salvataggio.
  Parametri:
  C_RB_LIST       &ColValues; 	   Lista ((<nome colonna><valore>) ...)
  						      		      di tutti gli attributi della classe
  long            *gs_id;           Puntatore al codice della nuova scheda;
                                    se = NULL non viene ricalcolato
  C_CLS_PUNT_LIST *lista_cls_punt;  Lista entità membri (default = NULL)
  _RecordsetPtr    *pRsIns;         Istruzione precompilata per inserimento dati,
                                    se non presente verrà inizializzata
                                    dalla funzione stessa (default = NULL).
  C_SELSET *GlobalSelSet;           Gruppo di selezione globale per la scheda corrente
                                    (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::ins_row(C_RB_LIST &ColValues, long *gs_id, C_CLS_PUNT_LIST *lista_cls_punt,
                    _RecordsetPtr pRsIns, C_SELSET *GlobalSelSet)
{
   long     new_key;
   C_INFO   *p_info;

   if ((p_info = ptr_info()) == NULL || ptr_attrib_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;
   
   if (gs_id)
   {
      // ricavo codice per prossimo inserimento
      if ((new_key = GetNewEntCode()) >= 0) return GS_BAD;
      // modifico (key_attrib)
      if (ColValues.CdrAssocSubst(p_info->key_attrib.get_name(), new_key) == GS_BAD) return GS_BAD;
   }
   else
      if (getKeyValue(ColValues, &new_key) == GS_BAD) return GS_BAD;

   // validazione e ricalcolo dati
   if (CalcValidData(ColValues, INSERT, GlobalSelSet) == GS_BAD) return GS_BAD;

   // aggiungo row in tabella temporanea classe
   if (pRsIns)
   {
      if (gsc_DBInsRow(pRsIns, ColValues) == GS_BAD) return GS_BAD;
   }
   else
   {
      C_DBCONNECTION *pConn;
      C_STRING       TempTableRef;

      if ((pConn = p_info->getDBConnection(TEMP)) == GS_BAD) return GS_BAD;

      // ricavo tabella temporanea
      if (getTempTableRef(TempTableRef) == GS_BAD)  return GS_BAD;

      if (pConn->InsRow(TempTableRef.get_name(), ColValues) == GS_BAD) return GS_BAD;
   }

   // aggiorno last tabella temporanea della classe
   if (set_last(new_key) == GS_BAD) return GS_BAD;

   if (gs_id) (*gs_id) = new_key;
  
   // se gruppo
   if (ptr_group_list())
      return ((C_GROUP *) this)->ins_members_row(new_key, *lista_cls_punt);
   else            
      return GS_GOOD;
}


/*********************************************************/
/*.doc C_GROUP::ins_members_row <external> */
/*+                                                                       
  Inserisce i record nella tabella di relazione del gruppo sostituendo relazioni esistenti.
  Se la variabile "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento entità nel
  salvataggio) questa funzione inserisce le relazioni temporanee marcandole
  in modo che non vengano considerate dal salvataggio.
  Parametri:
  long            gs_id;            Codice del gruppo;
  C_2INT_LONG_LIST &lista_cls_punt;  Lista entità membri da inserire
  oppure
  long            gs_id;            Codice del gruppo;
  C_CLS_PUNT_LIST &lista_cls_punt;  Lista entità membri da inserire

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::ins_members_row(long gs_id, C_2INT_LONG_LIST &memberList)
{  
	C_2INT_LONG    *pMember;
   C_RB_LIST      LnkValues;
   presbuf        p_cls, p_id;
   int            state;
   C_DBCONNECTION *pConn;
   C_STRING       LnkTempTableRef, statement;
   _RecordsetPtr  pLnkRsIns;

   // ricavo tabella delle relazioni temporanee
   if (getTempLnkTableRef(LnkTempTableRef) == GS_BAD) return GS_BAD;
   if ((pConn = ptr_info()->getDBConnection(TEMP)) == GS_BAD) return GS_BAD;

   // cancello le relazioni precedenti
   statement = _T("DELETE FROM ");
   statement += LnkTempTableRef;
   statement += _T(" WHERE KEY_ATTRIB=");
   statement += gs_id;
   if (pConn->ExeCmd(statement) == GS_BAD) return GS_BAD;

   // poichè la tabella delle relazioni temporanee fa fede per sapere quali gruppi
   // salvare (come GEOsimAppl::SAVE_SS per le classi con rappresentazione grafica diretta)
   // se non si è abilitati a inserire in GEOsimAppl::SAVE_SS allora nel valore di "STATUS"
   // viene settato il bit n. 6 (100000 = 32 = NOSAVE)
   state = (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD) ? NOSAVE : 0;
   
	if ((LnkValues << acutBuildList(RTLB,
	  						                  RTLB, RTSTR, _T("CLASS_ID"), RTSHORT, 0, RTLE,
                                       RTLB, RTSTR, _T("ENT_ID"), RTLONG, 0, RTLE,
                                       RTLB, RTSTR, _T("KEY_ATTRIB"), RTLONG, gs_id, RTLE,
                                       RTLB, RTSTR, _T("STATUS"), RTLONG, INSERTED | state, RTLE,
                                    RTLE, 0)) == NULL)
      return GS_BAD;

   p_cls = LnkValues.get_head()->rbnext->rbnext->rbnext;
   p_id  = p_cls->rbnext->rbnext->rbnext->rbnext;

   if (pConn->InitInsRow(LnkTempTableRef.get_name(), pLnkRsIns) == GS_BAD) return GS_BAD;

   pMember = (C_2INT_LONG *) memberList.get_head();
   while (pMember)
   {
      gsc_RbSubst(p_cls, pMember->get_key());
      gsc_RbSubst(p_id, pMember->get_id());

      // aggiungo row in tabella temporanea classe
      if (gsc_DBInsRow(pLnkRsIns, LnkValues) == GS_BAD)
         { gsc_DBCloseRs(pLnkRsIns); return GS_BAD; }
      pMember = (C_2INT_LONG *) memberList.get_next();
   } 
   gsc_DBCloseRs(pLnkRsIns);
            
   return GS_GOOD;
}
int C_GROUP::ins_members_row(long gs_id, C_CLS_PUNT_LIST &lista_cls_punt)
{  
   C_2INT_LONG_LIST memberList;
   
   if (lista_cls_punt.to_C_2INT_LONG_LIST(memberList) == GS_BAD) return GS_BAD;

   return ins_members_row(gs_id, memberList);
}


/*********************************************************/
/*.doc (new 2) C_CLASS::GetNewEntCode <external> */
/*+                                                                       
  Ricava codice valido per prossimo inserimento.
  Parametri:

  Restituisce n<>0 in caso di successo altrimenti restituisce 0.
-*/  
/*********************************************************/
long C_CLASS::GetNewEntCode()
{
   if (ptr_info() == NULL) { GS_ERR_COD = eGSInvClassType; return 0; }
   // ricavo nuovo key_attrib (codici progressivi negativi)
   // ptr_info()->last può essere positivo o negativo, non importa
   return -1 * (abs(ptr_info()->TempLastId) + 1);
}


/*********************************************************/
/*.doc C_CLASS::check_sql <external> */
/*+                                                                       
  Controlla la correttezza dell'istruzione SQL per la tabella della
  classe nel progetto corrente.
  Parametri:
  const TCHAR *WhereSql;      Condizione SQL
  
  Restituisce NULL in caso di successo oppure una stringa con messaggio 
  di errore. 
  N.B. Alloca memoria
-*/  
/*********************************************************/
TCHAR *C_CLASS::check_sql(const TCHAR *WhereSql)
{
   C_INFO         *p_info = ptr_info();
   C_DBCONNECTION *pConn;
   C_STRING       statement;
   _CommandPtr    pCmd;
   
   if (!p_info) { GS_ERR_COD = eGSInvClassType; return NULL; }

   if ((pConn = p_info->getDBConnection(OLD)) == NULL) return NULL;

   statement = _T("SELECT * FROM ");
   statement += p_info->OldTableRef;
   if (WhereSql) statement += WhereSql;

   if (pConn->PrepareCmd(statement, pCmd) == GS_GOOD) return NULL;
   
   if (pConn->get_Connection()->Errors->Count > 0)
   {
      _bstr_t dummy;
      
      // messaggio di errore
      dummy = pConn->get_Connection()->Errors->GetItem((long)0)->Description;
      return gsc_tostring((wchar_t*) dummy);
   }
   return NULL;
}

/*********************************************************/
/*.doc C_CLASS::check_graph_compatib          <external> */
/*+                                                                       
  Verifica che l'oggetto grafico sia compatibile con la classe (es. aggr_data)
  e se il caso (es. superfici = polilinee chiuse) modifica l'oggetto.
  Parametri:
  C_SELSET &SelSet;   Gruppo di oggetti da verificare
  oppure
  ads_name ent;       Oggetto da verificare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::check_graph_compatib(C_SELSET &SelSet)
{
   long     i = 0;
   ads_name ent;
   
   while (SelSet.entname(i++, ent) == GS_GOOD)
      if (check_graph_compatib(ent) == GS_BAD) return GS_BAD;
   
   return GS_GOOD;
}
int C_CLASS::check_graph_compatib(ads_name ent)
{                 
   TCHAR type[MAX_LEN_GRAPH_TYPE];

   if (ptr_GphInfo())
      if (ptr_GphInfo()->HasCompatibleGeom(ent) == false) return GS_BAD;

   if (id.category == CAT_SPAGHETTI) return GS_GOOD;

   if (gsc_graph_type(ent, type) == GS_BAD) return GS_BAD;
   if (gsc_strcmp(_T("INSERT"), type) == 0)
      // se blocco DA va bene lo stesso.
      if (gsc_is_DABlock(ent) == GS_GOOD) return GS_GOOD;

   switch (id.category)
   {
      case CAT_SIMPLEX :   // SEMPLICE
      case CAT_SUBCLASS:   // SOTTOCLASSE
         switch (id.type)
         {
            case TYPE_POLYLINE:
            {
               // se non è una polilinea, un arco, una linea
               if (gsc_strcmp(_T("POLYLINE"), type) != 0 && gsc_strcmp(_T("LWPOLYLINE"), type) != 0 &&
                   gsc_strcmp(_T("ARC"), type) != 0 && gsc_strcmp(_T("LINE"), type) != 0)
                  return GS_BAD;

               // La lunghezza non può essere = 0
               presbuf result;
               if ((result = gsc_get_graphical_data(ent, _T(".length"), _T("real"))) == NULL)
                  return GS_BAD;
               if (result->resval.rreal == 0) { acutRelRb(result); return GS_BAD; }
               acutRelRb(result);

			      break;
            }
            case TYPE_TEXT:
               if (gsc_strcmp(_T("TEXT"), type) != 0 && gsc_strcmp(_T("MTEXT"), type) != 0) return GS_BAD;
			      break;
            case TYPE_NODE:
               if (gsc_strcmp(_T("INSERT"), type) != 0) return GS_BAD;
               if (gsc_is_DABlock(ent) == GS_GOOD) return GS_BAD;
			      break;
            case TYPE_SURFACE:
               // se non è un riempimento nè un MPolygon 
               if (gsc_strcmp(_T("HATCH"), type) != 0 && gsc_strcmp(_T("MPOLYGON"), type) != 0)
               {  // se non è una polilinea
                  if (gsc_strcmp(_T("POLYLINE"), type) != 0 && gsc_strcmp(_T("LWPOLYLINE"), type) != 0)
                  {  // se non è un cerchio o una ellisse
                     if (gsc_strcmp(_T("CIRCLE"), type) != 0 && gsc_strcmp(_T("ELLIPSE"), type) != 0)
                        return GS_BAD;
                  }
                  else
                     // Chiudo la polilinea se non era ancora chiusa
                     if (gsc_close_pline(ent) == GS_BAD) return GS_BAD;

                  // L'area non può essere = 0
                  presbuf result;
                  if ((result = gsc_get_graphical_data(ent, _T(".area"), _T("real"))) == NULL)
                     return GS_BAD;
                  if (result->resval.rreal == 0) { acutRelRb(result); return GS_BAD; }
                  acutRelRb(result);
               }
			      break;
            default:
               return GS_BAD;
			      break;
         }
		   break;
      case CAT_GROUP :
      case CAT_GRID :
      case CAT_EXTERN :
      default: return GS_BAD;
   }

   return GS_GOOD;
}

/*********************************************************/
/*.doc C_CLASS::check_SS_graph_compatib_on_import <external> */
/*+                                                                       
   Funzione che verifica la compatibilità tra la definizione della classe
   e gli oggetti appartenenti al gruppo di selezione che deve essere importato.
   Parametri:
   C_SELSET &SelSet;          Gruppo da controllare;
   int      RemoveIncompatib; Flag che determina l'eliminazione o meno
                              degli oggetti incompatibili (default = GS_BAD)

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::check_SS_graph_compatib_on_import(C_SELSET &SelSet, int RemoveIncompatib)
{
   long     i = 0;
   ads_name ent;
   
   if (RemoveIncompatib == GS_BAD)
   {
      while (SelSet.entname(i++, ent) == GS_GOOD)
         if (check_graph_compatib(ent) == GS_BAD)
            // Accetto l'eccezione in cui si vogliono importare dei points
            // in classe semplice di tipo NODO
            if (id.category != CAT_SIMPLEX || id.type != TYPE_NODE ||
                gsc_isPoint(ent) != GS_GOOD)
               return GS_BAD;
   }
   else
   {
      while (SelSet.entname(i, ent) == GS_GOOD)
         if (check_graph_compatib(ent) == GS_BAD)
            // Accetto l'eccezione in cui si vogliono importare dei points
            // in classe semplice di tipo NODO
            if (id.category != CAT_SIMPLEX || id.type != TYPE_NODE ||
                gsc_isPoint(ent) != GS_GOOD)
               SelSet.subtract_ent(ent);
            else
               i++;
         else
            i++;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLASS::prepare_data                  <internal> */
/*+                                                                       
  Preparo istruzione per la selezione delle schede della classe
  dal temporaneo o dall'old delle schede.
  Parametri:
  C_PREPARED_CMD_LIST &CmdList; Comandi per TEMP (primo) e OLD (secondo) da preparare
  const TCHAR  *what;           Eventuale espressione da ritornare (default = NULL)
  const TCHAR *WhereSql;        Eventuale condizione di filtro (default = NULL)

  oppure

  C_PREPARED_CMD &pCmd;    Comando da preparare
  int Type;                Tipo di dato se = OLD da tabella OLD
                           se = TEMP da tabella temporanea
  const TCHAR *what;       Eventuale espressione da ritornare (default = NULL)
  const TCHAR *WhereSql;   Eventuale condizione di filtro (default = NULL)

  oppure

  _CommandPtr &pCmd;       Comando da preparare
  int Type;                Tipo di dato se = OLD da tabella OLD
                           se = TEMP da tabella temporanea
  const TCHAR  *what;      Eventuale espressione da ritornare (default = NULL)
  const TCHAR  *WhereSql;  Eventuale condizione di filtro (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::prepare_data(C_PREPARED_CMD_LIST &CmdList, const TCHAR *what, 
                          const TCHAR *WhereSql)
{
   C_PREPARED_CMD *pCmd;
   
   CmdList.remove_all();

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_data(*pCmd, TEMP, what, WhereSql) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { CmdList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_data(*pCmd, OLD, what, WhereSql) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   return GS_GOOD;
}
int C_CLASS::prepare_data(C_PREPARED_CMD &pCmd, int Type, const TCHAR *what, 
                          const TCHAR *WhereSql)
{
   // Se non ci sono vincoli nei campi da cercare e nelle condizioni di ricerca
   // verifico se si può usare il metodo seek con i recordset
   if (gsc_strlen(what) > 0 || gsc_strlen(WhereSql) > 0 ||
       (pCmd.pRs = prepare_data(Type)) == NULL)
      // Se non è possibile usare la seek uso il comando preparato
      if (prepare_data(pCmd.pCmd, Type, what, WhereSql) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
int C_CLASS::prepare_data(_CommandPtr &pCmd, int Type, const TCHAR *what, 
                          const TCHAR *WhereSql)
{                 
   C_INFO         *p_info = ptr_info();
   C_STRING       statement, TableRef, FldName;
   C_DBCONNECTION *pConn;
   _ParameterPtr  pParam;
   C_ATTRIB       *pAttrib;
   DataTypeEnum   DataType;
   long           Size = 0;

   if (!p_info || !ptr_attrib_list())
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((pConn = p_info->getDBConnection(Type)) == NULL) return GS_BAD;
   // Attributo chiave di ricerca
   pAttrib = (C_ATTRIB *) ptr_attrib_list()->search_name(p_info->key_attrib.get_name(), FALSE);

   if (Type == OLD) // Connessione alla tabella OLD
   {
      TableRef = p_info->OldTableRef;
      if (pAttrib->init_ADOType(pConn) == NULL) return GS_BAD;
      DataType = pAttrib->ADOType;
      Size = pConn->ptr_DataTypeList()->search_Type(DataType, TRUE)->get_Size();
   }
   else
   {
      if (getTempTableRef(TableRef) == GS_BAD) return GS_BAD;
      if (pAttrib->init_TempADOType(ptr_info()->getDBConnection(OLD), pConn) == NULL)
         return GS_BAD;
      DataType = pAttrib->TempADOType;
   }

   FldName = p_info->key_attrib;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   // leggo record
   statement = _T("SELECT ");
   statement += (what != NULL && wcslen(what) > 0) ? what : _T("*");
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += FldName;
   statement += _T("=?");

   if (WhereSql != NULL && wcslen(WhereSql) > 0)
   {
      statement += _T(" AND (");
      statement += WhereSql;
      statement += _T(")");
   }

   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), DataType, Size) == GS_BAD) return GS_BAD;
   pParam->PutType(adDouble);
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLASS::prepare_data                  <internal> */
/*+                                                                       
  Preparo recordset per la selezione delle schede della classe
  dal temporaneo o dall'old delle schede in modalità seek.
  Parametri:
  int Type;                Tipo di dato se = OLD da tabella OLD
                           se = TEMP da tabella temporanea
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
_RecordsetPtr C_CLASS::prepare_data(int Type)
{  
   C_INFO         *p_info = ptr_info();
   C_STRING       TableRef, InitQuotedId, FinalQuotedId;
   C_STRING       IndexName;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   bool           RsOpened = false;

   if (!p_info || !ptr_attrib_list())
      { GS_ERR_COD = eGSInvClassType; return NULL; }

   // Verifico se ho già provato il metodo seek
   if (Type == TEMP)
      { if (p_info->SeekSupportedOnTemp == false) return NULL; }
   else
      { if (p_info->SeekSupportedOnOld == false) return NULL; }

   if ((pConn = p_info->getDBConnection(OLD)) == NULL) return NULL;
   TableRef = p_info->OldTableRef;

   if (Type == TEMP) 
   {
      if ((pConn = p_info->getDBConnection(TEMP)) == NULL) return NULL;
      if (getTempTableRef(TableRef) == GS_BAD) return NULL;
   }

   // 1) il metodo seek è attualmente supportato solo da JET
   // 2) se si prova ad aprire un recordset con la modalità adCmdTableDirect
   //    mentre si è in una transazione questa viene automaticamente abortita
   //    con PostgreSQL
   if (gsc_strcmp(pConn->get_DBMSName(), ACCESS_DBMSNAME, FALSE) != 0)
   {
      // Mi segno che non supporta il metodo seek per non riprovare più
      if (Type == TEMP) p_info->SeekSupportedOnTemp = false;
      else p_info->SeekSupportedOnOld = false;

      return NULL;
   }

   if (p_info->get_PrimaryKeyName(IndexName) == GS_BAD)
      return NULL;

   // Se la connessione ole-db non supporta l'uso del catalogo e dello schema
   if (pConn->get_CatalogUsage() == 0 && pConn->get_SchemaUsage() == 0)
   {
      // rimuove se esiste il prefisso e il suffisso (es. per ACCESS = [ ])
      InitQuotedId = pConn->get_InitQuotedIdentifier();
      FinalQuotedId = pConn->get_FinalQuotedIdentifier();
      TableRef.removePrefixSuffix(InitQuotedId.get_name(), FinalQuotedId.get_name());
   }

   // istanzio un recordset
   if (FAILED(pRs.CreateInstance(__uuidof(Recordset))))
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

	try
   {  
      pRs->CursorLocation = adUseServer;
      // apro il recordset
      pRs->Open(TableRef.get_name(), pConn->get_Connection().GetInterfacePtr(), 
                adOpenDynamic, adLockOptimistic, adCmdTableDirect);
      RsOpened = true;

      if (!pRs->Supports(adIndex) || !pRs->Supports(adSeek))
      {  // non supportato
         gsc_DBCloseRs(pRs);
         // Mi segno che non supporta il metodo seek per non riprovare più
         if (Type == TEMP) p_info->SeekSupportedOnTemp = false;
         else p_info->SeekSupportedOnOld = false;

         return NULL;
      }
      // Setto l'indice di ricerca
      if (gsc_DBSetIndexRs(pRs, IndexName.get_name(), GS_BAD) == GS_BAD)
      {
         gsc_DBCloseRs(pRs);
         return NULL;
      }
   }

   #if defined(GSDEBUG) // se versione per debugging
	   catch (_com_error &e)
	   {
         printDBErrs(e);
   #else
	   catch (_com_error)
	   {
   #endif  
         // Mi segno che non supporta il metodo seek per non riprovare più
         if (Type == TEMP) p_info->SeekSupportedOnTemp = false;
         else p_info->SeekSupportedOnOld = false;

         if (RsOpened) gsc_DBCloseRs(pRs);
         else pRs.Release();

         return NULL;
      }

   return pRs;
}


/******************************************************************/
/*.doc C_CLASS::prepare_reldata_where_member           <internal> */
/*+                                                                       
  Preparo comando per la selezione delle relazioni di un gruppo
  attraverso il codice della classe figlia e il codice di una entità.
  Parametri:
  C_PREPARED_CMD_LIST &CmdList; Comandi per TEMP (primo) e OLD (secondo) da preparare
  
  oppure:

  C_PREPARED_CMD &pCmd; Comando da preparare
  int Type;             Tipo di relazioni se = OLD relazioni da tabella OLD
                        se = TEMP relazioni temporanee

  Restituisce GS_GOOD in caso di successo, GS_CAN se non si può prepare il comando
  altrimenti restituisce GS_BAD.
  N.B. da applicare solo per classi GRUPPI e GRUPPI.
-*/  
/******************************************************************/
int C_CLASS::prepare_reldata_where_member(C_PREPARED_CMD_LIST &CmdList)
{
   C_PREPARED_CMD *pCmd;
   
   CmdList.remove_all();

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_reldata_where_member(*pCmd, TEMP) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { CmdList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_reldata_where_member(*pCmd, OLD) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   return GS_GOOD;
}
int C_CLASS::prepare_reldata_where_member(C_PREPARED_CMD &pCmd, int Type)
{                 
   C_INFO         *p_info = ptr_info();
   C_STRING       statement, LinkTableRef;
   C_DBCONNECTION *pConn;
   _ParameterPtr  pParam;

   if (!p_info || ptr_group_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((pCmd.pRs = prepare_reldata_where_member(Type)) != NULL) return GS_GOOD; 
   if ((pConn = p_info->getDBConnection(Type)) == NULL) return GS_BAD;

   if (Type == OLD) // Connessione alla tabella OLD
   {
      // leggo record in OLD
      statement = _T("SELECT KEY_ATTRIB,CLASS_ID,ENT_ID FROM ");
      if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD;
   }
   else
   {
      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD;

      // se non esiste la tabella temporanea (creata solo se modificato qualcosa)
      if (pConn->ExistTable(LinkTableRef) == GS_BAD) return GS_CAN;

      // leggo record in TEMP
      statement = _T("SELECT KEY_ATTRIB,CLASS_ID,ENT_ID,STATUS FROM ");
   }
   statement += LinkTableRef;
   statement += _T(" WHERE CLASS_ID=? AND ENT_ID=?");

   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd.pCmd) == GS_BAD) return GS_BAD;

   if (gsc_CreateDBParameter(pParam, _T("CLASS_ID"), adDouble) == GS_BAD) return GS_BAD;
   pCmd.pCmd->Parameters->Append(pParam);
   if (gsc_CreateDBParameter(pParam, _T("ENT_ID"), adDouble) == GS_BAD) return GS_BAD;
   pCmd.pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLASS::prepare_reldata_where_member  <internal> */
/*+                                                                       
  Preparo un recordset per la selezione delle relazioni di un gruppo
  attraverso il codice della classe figlia e il codice di una
  entità in modalità seek.
  Parametri:
  int Type;                Tipo di dato se = OLD da tabella OLD
                           se = TEMP da tabella temporanea
  
  Restituisce un recordset in caso di successo altrimenti restituisce NULL. 
-*/  
/*********************************************************/
_RecordsetPtr C_CLASS::prepare_reldata_where_member(int Type)
{        
   C_INFO         *p_info = ptr_info();
   C_STRING       LinkTableRef, InitQuotedId, FinalQuotedId;
   C_STRING       Catalog, Schema, Name, IndexRef;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   bool           RsOpened = false;

   if (!p_info || ptr_group_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return NULL; }

   if ((pConn = p_info->getDBConnection(Type)) == NULL) return NULL;

   // 1) il metodo seek è attualmente supportato solo da JET
   // 2) se si prova ad aprire un recordset con la modalità adCmdTableDirect
   //    mentre si è in una transazione questa viene automaticamente abortita
   //    con PostgreSQL
   if (gsc_strcmp(pConn->get_DBMSName(), ACCESS_DBMSNAME, FALSE) != 0)
      return NULL;

   // nome indice per campo chiave di ricerca
   if (Type == TEMP) 
   {
      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return NULL;
      // se non esiste la tabella temporanea (creata solo se modificato qualcosa)
      if (pConn->ExistTable(LinkTableRef) == GS_BAD) return NULL;
   }
   else
      if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return NULL;
   
   if (pConn->split_FullRefTable(LinkTableRef, Catalog, Schema, Name) == GS_BAD)
      return NULL;

   // creo indice per CLASS_ID, ENT_ID
   if (IndexRef.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                Schema.get_name(),
                                                Name.get_name(),
                                                _T("CLASS_ID, ENT_ID"))) == NULL)
      return GS_BAD;

   // Se la connessione ole-db non supporta l'uso del catalogo e dello schema
   if (pConn->get_CatalogUsage() == 0 && pConn->get_SchemaUsage() == 0)
   {
      // rimuove se esiste il prefisso e il suffisso (es. per ACCESS = [ ])
      InitQuotedId = pConn->get_InitQuotedIdentifier();
      FinalQuotedId = pConn->get_FinalQuotedIdentifier();
      LinkTableRef.removePrefixSuffix(InitQuotedId.get_name(), FinalQuotedId.get_name());
      IndexRef.removePrefixSuffix(InitQuotedId.get_name(), FinalQuotedId.get_name());
   }

   // istanzio un recordset
   if (FAILED(pRs.CreateInstance(__uuidof(Recordset))))
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

	try
   {  
      pRs->CursorLocation = adUseServer;
      // apro il recordset
      pRs->Open(LinkTableRef.get_name(), pConn->get_Connection().GetInterfacePtr(), 
                adOpenDynamic, adLockOptimistic, adCmdTableDirect);
      RsOpened = true;
      
      if (!pRs->Supports(adIndex) || !pRs->Supports(adSeek))
      {  // non supportato
         gsc_DBCloseRs(pRs);
         return NULL;
      }
      // Setto l'indice di ricerca
      if (gsc_DBSetIndexRs(pRs, IndexRef.get_name(), GS_BAD) == GS_BAD)
      {
         gsc_DBCloseRs(pRs);
         return NULL;
      }
   }

	catch (_com_error)
   {
      if (RsOpened) gsc_DBCloseRs(pRs);
      else pRs.Release();

      return NULL;
   }

   return pRs;
}


/******************************************************************/
/*.doc C_CLASS::prepare_reldata_where_key              <internal> */
/*+                                                                       
  Preparo comando per la selezione delle relazioni di un
  gruppo attraverso il codice del gruppo stesso.
  Parametri:
  C_PREPARED_CMD_LIST &CmdList; Comandi per TEMP (primo) e OLD (secondo) da preparare

  oppure:

  C_PREPARED_CMD &pCmd; Comando da preparare
  int Type;             Tipo di relazioni se = OLD relazioni da tabella OLD
                        se = TEMP relazioni temporanee

  Restituisce GS_GOOD in caso di successo, GS_CAN se non si può prepare il comando
  altrimenti restituisce GS_BAD.
  N.B. da applicare solo per classi GRUPPI e GRUPPI.
-*/  
/******************************************************************/
int C_CLASS::prepare_reldata_where_key(C_PREPARED_CMD_LIST &CmdList)
{
   C_PREPARED_CMD *pCmd;
   
   CmdList.remove_all();

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_reldata_where_key(*pCmd, TEMP) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { CmdList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_reldata_where_key(*pCmd, OLD) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   return GS_GOOD;
}
int C_CLASS::prepare_reldata_where_key(C_PREPARED_CMD &pCmd, int Type)
{                 
   C_INFO         *p_info = ptr_info();
   C_STRING       statement, LinkTableRef;
   C_DBCONNECTION *pConn;
   _ParameterPtr  pParam;

   if (!p_info || ptr_group_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((pCmd.pRs = prepare_reldata_where_key(Type)) != NULL) return GS_GOOD;

   if ((pConn = p_info->getDBConnection(Type)) == NULL) return GS_BAD;

   if (Type == OLD) // Connessione alla tabella OLD
   {
      if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD;
      // leggo record in OLD
      statement = _T("SELECT KEY_ATTRIB,CLASS_ID,ENT_ID FROM ");
   }
   else
   {
      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD;

      // se non esiste la tabella temporanea (creata solo se modificato qualcosa)
      if (pConn->ExistTable(LinkTableRef) == GS_BAD) return GS_CAN;
   
      // leggo record in TEMP
      statement = _T("SELECT KEY_ATTRIB,CLASS_ID,ENT_ID,STATUS FROM ");
   }
   statement += LinkTableRef;
   statement += _T(" WHERE KEY_ATTRIB=?");

   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd.pCmd) == GS_BAD) return GS_BAD;

   if (gsc_CreateDBParameter(pParam, _T("KEY_ATTRIB"), adDouble) == GS_BAD) return GS_BAD;
   pCmd.pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLASS::prepare_reldata_where_key  <internal> */
/*+                                                                       
  Preparo comando per la selezione delle relazioni di un gruppo
  attraverso il codice del gruppo stesso in modalità seek.
  Parametri:
  int Type;                Tipo di dato se = OLD da tabella OLD
                           se = TEMP da tabella temporanea
  
  Restituisce un recordset in caso di successo altrimenti restituisce NULL. 
-*/  
/*********************************************************/
_RecordsetPtr C_CLASS::prepare_reldata_where_key(int Type)
{                 
   C_INFO         *p_info = ptr_info();
   C_STRING       LinkTableRef, InitQuotedId, FinalQuotedId;
   C_STRING       Catalog, Schema, Name, IndexRef;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   bool           RsOpened = false;

   if (!p_info || ptr_group_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return NULL; }

   if ((pConn = p_info->getDBConnection(Type)) == NULL) return NULL;

   // 1) il metodo seek è attualmente supportato solo da JET
   // 2) se si prova ad aprire un recordset con la modalità adCmdTableDirect
   //    mentre si è in una transazione questa viene automaticamente abortita
   //    con PostgreSQL
   if (gsc_strcmp(pConn->get_DBMSName(), ACCESS_DBMSNAME, FALSE) != 0)
      return NULL;

   // nome indice per campo chiave di ricerca
   if (Type == TEMP) 
   {
      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return NULL;
      // se non esiste la tabella temporanea (creata solo se modificato qualcosa)
      if (pConn->ExistTable(LinkTableRef) == GS_BAD) return NULL;
   }
   else
      if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return NULL;
      
   if (pConn->split_FullRefTable(LinkTableRef, Catalog, Schema, Name) == GS_BAD)
      return NULL;

   // creo indice per KEY_ATTRIB
   Name += _T('G');
   if (IndexRef.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                Schema.get_name(),
                                                Name.get_name(),
                                                _T("KEY_ATTRIB"))) == NULL)
      return GS_BAD;

   // Se la connessione ole-db non supporta l'uso del catalogo e dello schema
   if (pConn->get_CatalogUsage() == 0 && pConn->get_SchemaUsage() == 0)
   {
      // rimuove se esiste il prefisso e il suffisso (es. per ACCESS = [ ])
      InitQuotedId = pConn->get_InitQuotedIdentifier();
      FinalQuotedId = pConn->get_FinalQuotedIdentifier();
      LinkTableRef.removePrefixSuffix(InitQuotedId.get_name(), FinalQuotedId.get_name());
      IndexRef.removePrefixSuffix(InitQuotedId.get_name(), FinalQuotedId.get_name());
   }

   // istanzio un recordset
   if (FAILED(pRs.CreateInstance(__uuidof(Recordset))))
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

	try
   {  
      pRs->CursorLocation = adUseServer;
      // apro il recordset
      pRs->Open(LinkTableRef.get_name(), pConn->get_Connection().GetInterfacePtr(), 
                adOpenDynamic, adLockOptimistic, adCmdTableDirect);
      RsOpened = true;
      
      if (!pRs->Supports(adIndex) || !pRs->Supports(adSeek))
      {  // non supportato
         gsc_DBCloseRs(pRs);
         return NULL;
      }
      // Setto l'indice di ricerca
      if (gsc_DBSetIndexRs(pRs, IndexRef.get_name(), GS_BAD) == GS_BAD)
      {
         gsc_DBCloseRs(pRs);
         return NULL;
      }
   }

	catch (_com_error)
   {
      if (RsOpened) gsc_DBCloseRs(pRs);
      else pRs.Release();

      return NULL;
   }

   return pRs;
}


/*********************************************************/
/*.doc gsc_get_reldata                           <external> */
/*+                                                                       
  Legge i dati usando il comando preparate nella
  funzione <prepare_reldata_where_member>.
  Parametri:
  _RecordsetPtr &pRs; Recordset aperto in modo "seek"
  int         Cls;
  long 		  Key;
  C_RB_LIST   &ColValues;     (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_reldata(_RecordsetPtr &pRs, int Cls, long Key, C_RB_LIST &ColValues)
{
   SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 3 elementi
   SAFEARRAYBOUND rgsabound;
   _variant_t     var, KeyForSeek;
   long           ix;
   C_RB_LIST  Buffer;
   presbuf    pMotherKey, pChildCls, pChildKey, pStatus;
   long       MotherKey, ChildKey;
   int        ChildCls, Status = UNMODIFIED, result = GS_GOOD, Found = FALSE;

   rgsabound.lLbound   = 0;
   rgsabound.cElements = 2;
   psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

   // Codice classe
   ix  = 0;
   gsc_set_variant_t(var, (short) Cls);
   SafeArrayPutElement(psa, &ix, &var);

   // Codice chiave
   ix  = 1;
   gsc_set_variant_t(var, Key);
   SafeArrayPutElement(psa, &ix, &var);

   KeyForSeek.vt = VT_ARRAY|VT_VARIANT;
   KeyForSeek.parray = psa;  

   if (gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD) { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   if (gsc_InitDBReadRow(pRs, Buffer) == GS_BAD) return GS_BAD;

   pMotherKey = Buffer.CdrAssoc(_T("KEY_ATTRIB"));
   pChildCls  = Buffer.CdrAssoc(_T("CLASS_ID"));
   pChildKey  = Buffer.CdrAssoc(_T("ENT_ID"));
   pStatus    = Buffer.CdrAssoc(_T("STATUS"));

   // ciclo di lettura
   if ((ColValues << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, Buffer) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_rb2Lng(pMotherKey, &MotherKey) == GS_BAD) MotherKey = 0; // Codice gruppo
      if (gsc_rb2Int(pChildCls, &ChildCls) == GS_BAD) ChildCls = 0;    // Codice classe figlia collegata
      if (gsc_rb2Lng(pChildKey, &ChildKey) == GS_BAD) ChildKey = 0;    // Codice dell'entità figlia collegata
      if (pStatus) // presente solo nelle relazioni temporanee
         gsc_rb2Int(pStatus, &Status); // Stato della relazione

      if (ChildCls != Cls || ChildKey != Key) break;

      if ((ColValues += acutBuildList(RTLB,
      							                 RTLONG,  MotherKey,
								                    RTSHORT, ChildCls,
								                    RTLONG,  ChildKey,
								                    RTSHORT, Status,
								              RTLE, 0)) == NULL)
         { result = GS_BAD; break; }
      gsc_Skip(pRs);
   }

   if ((ColValues += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_reldata                        <external> */
/*+                                                                       
  Legge i dati usando il comando preparato con la funzione 
  <prepare_reldata_where_member>. Ritorn una lista nel seguente formato:
  ((<codice entità compl><classe figlia><codice figlia><stato>) ...)

  Parametri:
  _CommandPtr &pTempCmd;      primo comando da provare
  _CommandPtr &pOldCmd;       se il primo fallisce prova con il secondo (se inizializzato)
  int         Cls;
  long 		  Key;
  
  C_RB_LIST   &ColValues;     (output)
  oppure
  _RecordsetPtr &pRs;         Apre il recordset e lo lascia aperto in modifica

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_reldata(_CommandPtr &pCmd, int Cls, long Key, C_RB_LIST &ColValues)
{
   _RecordsetPtr pInternalRs;
   C_RB_LIST     Buffer;
   presbuf       pMotherKey, pChildCls, pChildKey, pStatus;
   long          MotherKey, ChildKey;
   int           ChildCls, Status = UNMODIFIED, result = GS_GOOD;

	ColValues.remove_all();
   // cerco
   if (gsc_get_reldata(pCmd, Cls, Key, pInternalRs) == GS_BAD) return GS_BAD;

   if (gsc_InitDBReadRow(pInternalRs, Buffer) == GS_BAD)
      { gsc_DBCloseRs(pInternalRs); return GS_BAD; }

   pMotherKey = Buffer.CdrAssoc(_T("KEY_ATTRIB"));
   pChildCls  = Buffer.CdrAssoc(_T("CLASS_ID"));
   pChildKey  = Buffer.CdrAssoc(_T("ENT_ID"));
   pStatus    = Buffer.CdrAssoc(_T("STATUS"));

   // ciclo di lettura
   if ((ColValues << acutBuildList(RTLB, 0)) == NULL)
      { gsc_DBCloseRs(pInternalRs); return GS_BAD; }

   while (gsc_isEOF(pInternalRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pInternalRs, Buffer) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_rb2Lng(pMotherKey, &MotherKey) == GS_BAD) MotherKey = 0; // Codice gruppo
      if (gsc_rb2Int(pChildCls, &ChildCls) == GS_BAD) ChildCls = 0;   // Codice classe figlia collegata
      if (gsc_rb2Lng(pChildKey, &ChildKey) == GS_BAD) ChildKey = 0;   // Codice dell'entità figlia collegata
      if (pStatus) // presente solo nelle relazioni temporanee
         gsc_rb2Int(pStatus, &Status); // Stato della relazione

      if ((ColValues += acutBuildList(RTLB,
      							                 RTLONG,  MotherKey,
								                    RTSHORT, ChildCls,
								                    RTLONG,  ChildKey,
								                    RTSHORT, Status,
								              RTLE, 0)) == NULL)
         { result = GS_BAD; break; }
      gsc_Skip(pInternalRs);
   }
   if (gsc_DBCloseRs(pInternalRs) == GS_BAD) return GS_BAD;

   if ((ColValues += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   return GS_GOOD;
}
int gsc_get_reldata(C_PREPARED_CMD &pCmd, int Cls, long Key, C_RB_LIST &ColValues)
{
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {  // Uso il recordset preparato per la seek
      if (gsc_get_reldata(pCmd.pRs, Cls, Key, ColValues) == GS_GOOD) return GS_GOOD;
   }
   else
      if (pCmd.pCmd != NULL && pCmd.pCmd.GetInterfacePtr())
         if (gsc_get_reldata(pCmd.pCmd, Cls, Key, ColValues) == GS_GOOD) return GS_GOOD;

   return GS_BAD;
}
int gsc_get_reldata(C_PREPARED_CMD_LIST &TempOldCmdList, int Cls, long Key, 
                    C_RB_LIST &ColValues, int *Source)
{
   // Cerco nel temporaneo
   if (gsc_get_reldata(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(1)), Cls, Key, ColValues) == GS_GOOD)
      { if (Source) *Source = TEMP; return GS_GOOD; }

   // Cerco nell'old
   if (gsc_get_reldata(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(2)), Cls, Key, ColValues) == GS_GOOD)
      { if (Source) *Source = OLD; return GS_GOOD; }

   return GS_BAD;
}   
int gsc_get_reldata(_CommandPtr &pCmd, int Cls, long Key, _RecordsetPtr &pRs)
{
   if (gsc_SetDBParam(pCmd, 0, Cls) == GS_BAD) return GS_BAD;
   if (gsc_SetDBParam(pCmd, 1, Key) == GS_BAD) return GS_BAD;

   // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
   // in una transazione fa casino (al secondo recordset che viene aperto)
   if (gsc_ExeCmd(pCmd, pRs, adOpenForwardOnly, adLockPessimistic) == GS_BAD)
      return GS_BAD;

   if (gsc_isEOF(pRs) == GS_GOOD)
   { 
      gsc_DBCloseRs(pRs);
      GS_ERR_COD = eGSInvalidKey;
      return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_get_reldata(C_PREPARED_CMD &pCmd, int Cls, long Key, _RecordsetPtr &pRs,
                    int *IsRsCloseable)
{
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {  // Uso il recordset preparato per la seek
      SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 3 elementi
      SAFEARRAYBOUND rgsabound;
      _variant_t     var, KeyForSeek;
      long           ix;

      IsRsCloseable = GS_BAD;

      rgsabound.lLbound   = 0;
      rgsabound.cElements = 2;
      psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

      // Codice classe
      ix  = 0;
      gsc_set_variant_t(var, (short) Cls);
      SafeArrayPutElement(psa, &ix, &var);

      // Codice chiave
      ix  = 1;
      gsc_set_variant_t(var, Key);
      SafeArrayPutElement(psa, &ix, &var);

      KeyForSeek.vt = VT_ARRAY|VT_VARIANT;
      KeyForSeek.parray = psa;  

      if (gsc_DBSeekRs(pCmd.pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD) return GS_BAD;
      if (gsc_isEOF(pCmd.pRs) == GS_BAD) { pRs = pCmd.pRs; return GS_GOOD; }
      GS_ERR_COD = eGSInvalidKey;
   }
   else
      if (pCmd.pCmd != NULL && pCmd.pCmd.GetInterfacePtr())
      {
         *IsRsCloseable = GS_GOOD;
         if (gsc_get_reldata(pCmd.pCmd, Cls, Key, pRs) == GS_GOOD) return GS_GOOD;
      }

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_get_reldata                        <external> */
/*+                                                                       
  Legge i dati usando il comando preparate nella
  funzione <prepare_reldata_where_key>.
  Parametri:
  _RecordsetPtr &pRs; Recordset aperto in modo "seek"
  long 		  Key;
  C_RB_LIST   &ColValues;     (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_reldata(_RecordsetPtr &pRs, long Key, C_RB_LIST &ColValues)
{
   _variant_t KeyForSeek(Key);
   C_RB_LIST  Buffer;
   presbuf    pMotherKey, pChildCls, pChildKey, pStatus;
   long       MotherKey, ChildKey;
   int        ChildCls, Status = UNMODIFIED, result = GS_GOOD, Found = FALSE;

   if (gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD) { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   if (gsc_InitDBReadRow(pRs, Buffer) == GS_BAD) return GS_BAD;

   pMotherKey = Buffer.CdrAssoc(_T("KEY_ATTRIB"));
   pChildCls  = Buffer.CdrAssoc(_T("CLASS_ID"));
   pChildKey  = Buffer.CdrAssoc(_T("ENT_ID"));
   pStatus    = Buffer.CdrAssoc(_T("STATUS"));

   // ciclo di lettura
   if ((ColValues << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, Buffer) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_rb2Lng(pMotherKey, &MotherKey) == GS_BAD) MotherKey = 0; // Codice gruppo
      if (gsc_rb2Int(pChildCls, &ChildCls) == GS_BAD) ChildCls = 0;   // Codice classe figlia collegata
      if (gsc_rb2Lng(pChildKey, &ChildKey) == GS_BAD) ChildKey = 0;   // Codice dell'entità figlia collegata
      if (pStatus) // presente solo nelle relazioni temporanee
         gsc_rb2Int(pStatus, &Status); // Stato della relazione

      if (MotherKey != Key) break;

      if ((ColValues += acutBuildList(RTLB,
      							                 RTLONG,  MotherKey,
								                    RTSHORT, ChildCls,
								                    RTLONG,  ChildKey,
								                    RTSHORT, Status,
								              RTLE, 0)) == NULL)
         { result = GS_BAD; break; }
      gsc_Skip(pRs);
   }

   if ((ColValues += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_reldata                        <external> */
/*+                                                                       
  Legge i dati usando il comando preparate nella
  funzione <prepare_reldata_where_key>.
  Parametri:
  _CommandPtr &pCmd;       Comando ADO
  long 		  Key;         Codice chiave
  C_RB_LIST   &ColValues;  (output)

  oppure

  C_PREPARED_CMD &pCmd
  long 		  Key;         Codice chiave
  C_RB_LIST   &ColValues;  (output)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_reldata(_CommandPtr &pCmd, long Key, C_RB_LIST &ColValues)
{
   _RecordsetPtr pInternalRs;
   C_RB_LIST     Buffer;
   presbuf       pMotherKey, pChildCls, pChildKey, pStatus;
   long          MotherKey, ChildKey;
   int           ChildCls, Status = UNMODIFIED, result = GS_GOOD, Found = FALSE;

   if (gsc_SetDBParam(pCmd, 0, Key) == GS_BAD) return GS_BAD;
   if (gsc_ExeCmd(pCmd, pInternalRs) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pInternalRs) == GS_GOOD)
      { gsc_DBCloseRs(pInternalRs); return GS_BAD; }

   if (gsc_InitDBReadRow(pInternalRs, Buffer) == GS_BAD)
      { gsc_DBCloseRs(pInternalRs); return GS_BAD; }

   pMotherKey = Buffer.CdrAssoc(_T("KEY_ATTRIB"));
   pChildCls  = Buffer.CdrAssoc(_T("CLASS_ID"));
   pChildKey  = Buffer.CdrAssoc(_T("ENT_ID"));
   pStatus    = Buffer.CdrAssoc(_T("STATUS"));

   // ciclo di lettura
   if ((ColValues << acutBuildList(RTLB, 0)) == NULL)
      { gsc_DBCloseRs(pInternalRs); return GS_BAD; }

   while (gsc_isEOF(pInternalRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pInternalRs, Buffer) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_rb2Lng(pMotherKey, &MotherKey) == GS_BAD) MotherKey = 0; // Codice gruppo
      if (gsc_rb2Int(pChildCls, &ChildCls) == GS_BAD) ChildCls = 0;   // Codice classe figlia collegata
      if (gsc_rb2Lng(pChildKey, &ChildKey) == GS_BAD) ChildKey = 0;   // Codice dell'entità figlia collegata
      if (pStatus) // presente solo nelle relazioni temporanee
         gsc_rb2Int(pStatus, &Status); // Stato della relazione

      if ((ColValues += acutBuildList(RTLB,
      							                 RTLONG,  MotherKey,
								                    RTSHORT, ChildCls,
								                    RTLONG,  ChildKey,
								                    RTSHORT, Status,
								              RTLE, 0)) == NULL)
         { result = GS_BAD; break; }
      gsc_Skip(pInternalRs);
   }
   if (gsc_DBCloseRs(pInternalRs) == GS_BAD) return GS_BAD;

   if ((ColValues += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   return GS_GOOD;
}
int gsc_get_reldata(C_PREPARED_CMD &pCmd, long Key, C_RB_LIST &ColValues)
{
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {  // Uso il recordset preparato per la seek
      if (gsc_get_reldata(pCmd.pRs, Key, ColValues) == GS_GOOD) return GS_GOOD;
   }
   else
      if (pCmd.pCmd != NULL && pCmd.pCmd.GetInterfacePtr())
         if (gsc_get_reldata(pCmd.pCmd, Key, ColValues) == GS_GOOD) return GS_GOOD;

   return GS_BAD;
}
int gsc_get_reldata(C_PREPARED_CMD_LIST &TempOldCmdList, long Key, C_RB_LIST &ColValues,
                    int *Source)
{
   // Cerco nel temporaneo
   if (gsc_get_reldata(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(1)), Key, ColValues) == GS_GOOD)
      { if (Source) *Source = TEMP; return GS_GOOD; }

   // Cerco nell'old
   if (gsc_get_reldata(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(2)), Key, ColValues) == GS_GOOD)
      { if (Source) *Source = OLD; return GS_GOOD; }

   return GS_BAD;
}
int gsc_get_reldata(_CommandPtr &pCmd, long Key, _RecordsetPtr &pRs)
{
   C_RB_LIST Buffer;

   // cerco nel temp
   if (gsc_SetDBParam(pCmd, 0, Key) == GS_BAD) return GS_BAD;

   // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
   // in una transazione fa casino (al secondo recordset che viene aperto)
   if (gsc_ExeCmd(pCmd, pRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      gsc_DBCloseRs(pRs);
      GS_ERR_COD = eGSInvalidKey;
		return GS_BAD;
   }

   return GS_GOOD;
}
int gsc_get_reldata(C_PREPARED_CMD &pCmd, long Key, _RecordsetPtr &pRs, int *IsRsCloseable)
{
   // Cerco nel temporaneo
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {  // Uso il recordset preparato per la seek
      _variant_t KeyForSeek(Key);

      *IsRsCloseable = GS_BAD;        
      if (gsc_DBSeekRs(pCmd.pRs, KeyForSeek, adSeekFirstEQ) == GS_GOOD &&
          gsc_isEOF(pCmd.pRs) == GS_BAD)
      { 
         pRs = pCmd.pRs;
         return GS_GOOD;
      }
   }
   else
      if (pCmd.pCmd != NULL && pCmd.pCmd.GetInterfacePtr())
      {
         *IsRsCloseable = GS_GOOD;
         if (gsc_get_data(pCmd.pCmd, Key, pRs) == GS_GOOD) return GS_GOOD;
      }

   GS_ERR_COD = eGSInvalidKey;

   return GS_BAD;
}


/*********************************************************/
/*.doc gsc_get_data                           <external> */
/*+                                                                       
  Legge i dati di una scheda di un'entità con il recordset preparato
  attraverso la funzione <prepare_data>.
  Parametri:
  _RecordsetPtr &pRs; Recordset aperto in modo "seek"
  long 		  Key;
  C_RB_LIST   &ColValues;     (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_data(_RecordsetPtr &pRs, long Key, C_RB_LIST &ColValues)
{
   _variant_t KeyForSeek(Key);

   if (gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD) return GS_BAD;
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_data                           <external> */
/*+                                                                       
  Legge i dati i comandi preparati attraverso le funzioni 
  <prepare_data_from_temp> e <prepare_data_from_old>.
  Parametri:
  _CommandPtr &pTempCmd;      primo comando da provare
  _CommandPtr &pOldCmd;       se il primo fallisce prova con il secondo (se inizializzato)
  long 		  Key;
  
  C_RB_LIST   &ColValues;     (output)
  oppure
  _RecordsetPtr &pRs;         Apre il recordset e lo lascia aperto in modifica

  int         *Source;        Provenienza dei dati (TEMP o OLD; default = NULL);

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_data(_CommandPtr &pCmd, long Key, C_RB_LIST &ColValues)
{
   _RecordsetPtr pInternalRs;

   // cerco nel temp
   if (gsc_SetDBParam(pCmd, 0, Key) == GS_BAD) return GS_BAD;

   if (gsc_ExeCmd(pCmd, pInternalRs) == GS_BAD) return GS_BAD;
   if (gsc_DBReadRow(pInternalRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pInternalRs); GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   if (gsc_DBCloseRs(pInternalRs) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
int gsc_get_data(C_PREPARED_CMD &pCmd, long Key, C_RB_LIST &ColValues)
{
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {  // Uso il recordset preparato per la seek
      if (gsc_get_data(pCmd.pRs, Key, ColValues) == GS_GOOD) return GS_GOOD;
   }
   else
      if (pCmd.pCmd != NULL && pCmd.pCmd.GetInterfacePtr())
         if (gsc_get_data(pCmd.pCmd, Key,ColValues) == GS_GOOD) return GS_GOOD;

   return GS_BAD;
}
int gsc_get_data(C_PREPARED_CMD_LIST &TempOldCmdList, long Key, C_RB_LIST &ColValues,
                 int *Source)
{
   // Cerco nel temporaneo
   if (gsc_get_data(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(1)), Key, ColValues) == GS_GOOD)
      { if (Source) *Source = TEMP; return GS_GOOD; }

   // Cerco nell'old
   if (gsc_get_data(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(2)), Key, ColValues) == GS_GOOD)
      { if (Source) *Source = OLD; return GS_GOOD; }

   return GS_BAD;
}
int gsc_get_data(_CommandPtr &pTempCmd, _CommandPtr &pOldCmd,
                 long Key, C_RB_LIST &ColValues, int *Source)
{
   _RecordsetPtr pInternalRs;

   // cerco nel temp
   if (gsc_SetDBParam(pTempCmd, 0, Key) == GS_BAD) return GS_BAD;

   if (gsc_ExeCmd(pTempCmd, pInternalRs) == GS_BAD) return GS_BAD;
   if (gsc_DBReadRow(pInternalRs, ColValues) == GS_BAD)
   {
      if (gsc_DBCloseRs(pInternalRs) == GS_BAD) return GS_BAD;
   
      if (pOldCmd != NULL && pOldCmd.GetInterfacePtr())
      {
         // cerco nell'old
         if (gsc_SetDBParam(pOldCmd, 0, Key) == GS_BAD) return GS_BAD;
         if (gsc_ExeCmd(pOldCmd, pInternalRs) == GS_BAD) return GS_BAD;
         if (gsc_DBReadRow(pInternalRs, ColValues) == GS_BAD)
            { gsc_DBCloseRs(pInternalRs); GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

         if (gsc_DBCloseRs(pInternalRs) == GS_BAD) return GS_BAD;
         if (Source) *Source = OLD;
      }
      else
         { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   }
   else if (Source) *Source = TEMP;

   return GS_GOOD;
}
int gsc_get_data(_CommandPtr &pCmd, long Key, _RecordsetPtr &pRs)
{
   if (gsc_SetDBParam(pCmd, 0, Key) == GS_BAD) return GS_BAD;
   // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
   // in una transazione fa casino (al secondo recordset che viene aperto)
   if (gsc_ExeCmd(pCmd, pRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD)
      return GS_BAD;  
   if (gsc_isEOF(pRs) == GS_GOOD)
      { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   return GS_GOOD;
}
int gsc_get_data(C_PREPARED_CMD &pCmd, long Key, _RecordsetPtr &pRs, int *IsRsCloseable)
{
   // Cerco nel temporaneo
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {  // Uso il recordset preparato per la seek
      _variant_t KeyForSeek(Key);

      *IsRsCloseable = GS_BAD;        
      if (gsc_DBSeekRs(pCmd.pRs, KeyForSeek, adSeekFirstEQ) == GS_GOOD &&
          gsc_isEOF(pCmd.pRs) == GS_BAD)
      { 
         pRs = pCmd.pRs;
         return GS_GOOD;
      }
   }
   else
      if (pCmd.pCmd != NULL && pCmd.pCmd.GetInterfacePtr())
      {
         *IsRsCloseable = GS_GOOD;
         if (gsc_get_data(pCmd.pCmd, Key, pRs) == GS_GOOD) return GS_GOOD;
      }

   GS_ERR_COD = eGSInvalidKey;

   return GS_BAD;
}
int gsc_get_data(C_PREPARED_CMD_LIST &TempOldCmdList, long Key, _RecordsetPtr &pRs,
                 int *IsRsCloseable, int *Source)
{
   // Cerco nel temporaneo
   if (gsc_get_data(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(1)), Key,
                    pRs, IsRsCloseable) == GS_GOOD)
   {
      if (Source) *Source = TEMP;
      return GS_GOOD;
   }

   // Cerco nell'old
   if (gsc_get_data(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(2)), Key,
                    pRs, IsRsCloseable) == GS_GOOD)
   {
      if (Source) *Source = OLD;
      return GS_GOOD;
   }

   return GS_BAD;
}

 
/////////////////////////////////////////////////////////////
///////        INIZIO FUNZIONI GESTIONE GS_DELETED    ///////
/////////////////////////////////////////////////////////////


/*************************************************************/
/*.doc (new 2) C_CLASS::Ins_Ent_Into_GsDelete(long ConEntit) */
/*+                                                                       
  Funzione che aggiorna il file GS_DELETE (che serve a marcare le
  cancellate) inserendo codice classe, codice sottoclasse, e codice 
  entità che è stata cancellata.
  Parametri:
  long Key;                Codice entità che è stata cancellata.
  C_RB_LIST *param_list;   Lista dei parametri (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CLASS::Ins_Ent_Into_GsDelete(long Key, C_RB_LIST *param_list)
{                 
   _RecordsetPtr pRsDel;

   // se non si è abilitati a inserire in GEOsimAppl::SAVE_SS allora nel valore 
   // di "STATUS" viene settato il bit n. 6 (100000 = 32 = NOSAVE)
   int Flag = (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD ) ? ERASED | NOSAVE : ERASED;

   if ((pRsDel = GS_CURRENT_WRK_SESSION->get_RsInsertDeleted()) == NULL) return GS_BAD;

   if (param_list)
   {
      if (param_list->get_head() == NULL)
      {
         // preparo il resbuf per l'inserimento di record
         if (((*param_list) << acutBuildList(RTLB, RTLB,
                                             RTSTR, _T("CLASS_ID"), RTSHORT, id.code,
                                             RTLE, RTLB,
                                             RTSTR, _T("SUB_CL_ID"), RTSHORT, id.sub_code,
                                             RTLE, RTLB,
                                             RTSTR, _T("KEY_ATTRIB"), RTLONG, Key,
                                             RTLE, RTLB,
                                             RTSTR, _T("STATUS"), RTSHORT, Flag,
                                             RTLE, RTLE, 0)) == NULL)
            return GS_BAD;
      }
      else
         if (param_list->CdrAssocSubst(_T("KEY_ATTRIB"), Key) == GS_BAD) return GS_BAD;

      if (gsc_DBInsRow(pRsDel, *param_list) == GS_BAD) return GS_BAD;
   }
   else
   {
      C_RB_LIST ColValues;

      // preparo il resbuf per l'inserimento di record
      if ((ColValues << acutBuildList(RTLB, RTLB,
                                      RTSTR, _T("CLASS_ID"), RTSHORT, id.code,
                                      RTLE, RTLB,
                                      RTSTR, _T("SUB_CL_ID"), RTSHORT, id.sub_code,
                                      RTLE, RTLB,
                                      RTSTR, _T("KEY_ATTRIB"), RTLONG, Key,
                                      RTLE, RTLB,
                                      RTSTR, _T("STATUS"), RTSHORT, Flag,
                                      RTLE, RTLE, 0)) == NULL)
         return GS_BAD;

      if (gsc_DBInsRow(pRsDel, ColValues) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_get_gs_delete_struct               <internal> */
/*+
  Questa funzione restituisce la struttura della tabella GS_DELETE di GEOsim.
  N.B. Utilizzando il Provider per ACCESS 97 non è possibile creare tabelle
       con strutture che superano una certa dimensione perchè si genera un 
       errore "record too large". Si utilizza perciò un database
       già preparato contenente la tabella in questione.
       La funzione ha, quindi, solo uno scopo documentativo.
-*/  
/*********************************************************/
void gsc_get_gs_delete_struct(C_STRING &Stru)
{  
   Stru =  _T(" (CLASS_ID SMALLINT NOT NULL, "); // Codice della classe
   Stru += _T("SUB_CL_ID BYTE NOT NULL, ");      // Codice della sottoclasse
   Stru += _T("KEY_ATTRIB LONG NOT NULL, ");     // Codice dell'entità
   Stru += _T("STATUS BYTE)");                   // Assume valore ERASED se la
                                             // cancellazione è da salvare, assume
                                             // valore ERASED | NOSAVE se la
                                             // cancellazione NON è da salvare
}


/*********************************************************/
/*.doc gsc_EmptyGsDeletedNoSub
/*+                                                                      
  Svuota la tabella temporanea GS_DELETED filtrando solo le righe
  che NON sono relative a sottoclassi (per il salvataggio).
  Parametri:
  int *cls;    Se indicato è il codice della classe proprietaria 
               dei record da cancellare; default = NULL.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_EmptyGsDeletedNoSub(int *cls)
{
   C_DBCONNECTION *pDBConn;
   C_STRING       TableRef, Cond;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->getDeletedTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;

   // cancello solo le entità non appartenenti a simulazioni
   if (cls)
   {
      Cond = _T("CLASS_ID=");
      Cond += *cls;
      Cond += _T(" AND ");
   }
   Cond += _T("SUB_CL_ID = 0");

   return pDBConn->DelRows(TableRef.get_name(), Cond.get_name());
}


/////////////////////////////////////////////////////////////
///////        FINE FUNZIONI GESTIONE GS_DELETED      ///////
/////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc (new 2) gsc_copy_rel_to_temp <internal> */
/*+                                                                       
  Copio link in un recordset specificato.
  Parametri:
  _RecordsetPtr &pRsIns;   Recordset in cui inserire le relazioni
  C_RB_LIST &LinkValues;   Lista delle relazioni
  int       Status;        Stato delle relazioni (default = UNMODIFIED)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_copy_rel_to_temp(_RecordsetPtr &pRsIns, C_RB_LIST &LinkValues, int Status)
{
   C_RB_LIST Buffer;
   presbuf   p, pKey, pCls, pId;
   int       state;
   long      i = 0;

   // preparo il resbuf per l'inserimento di record nel temp dei link
   if ((Buffer << acutBuildList(RTLB, RTLB,
             	                 RTSTR, _T("KEY_ATTRIB"), RTLONG, 0,
        	                       RTLE, RTLB,
             	                 RTSTR, _T("CLASS_ID"), RTSHORT, 0,
        			                 RTLE, RTLB,
             	                 RTSTR, _T("ENT_ID"), RTLONG, 0,
      	                       RTLE, 0)) == NULL)
      return GS_BAD;

   pKey = Buffer.get_head()->rbnext->rbnext->rbnext;
   pCls = pKey->rbnext->rbnext->rbnext->rbnext;
   pId  = pCls->rbnext->rbnext->rbnext->rbnext;

   // poichè il file delle relazioni temporanee fa fede per sapere quali gruppi
   // salvare (come GEOsimAppl::SAVE_SS per le classi con rappresentazione grafica diretta)
   // se non si è abilitati a inserire in GEOsimAppl::SAVE_SS allora nel valore di "STATUS"
   // viene settato il bit n. 6 (100000 = 32 = NOSAVE)
   state = (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD) ? NOSAVE : 0;

   state = Status | state;

   // aggiungo lo status
   if ((Buffer += acutBuildList(RTLB,
             	                 RTSTR, _T("STATUS"), RTSHORT, state,
      	                       RTLE, RTLE, 0)) == NULL)
      return GS_BAD;

   // copio link su tabella temporanea
   while ((p = LinkValues.nth(i++)) != NULL)
   {
      // inizializzo il resbuf per l'inserimento dei link in temp
	   gsc_RbSubst(pKey, (p = p->rbnext));
	   gsc_RbSubst(pCls, (p = p->rbnext));
	   gsc_RbSubst(pId,  (p = p->rbnext));

      if (gsc_DBInsRow(pRsIns, Buffer) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


////////////////////////////////////////////////////////////////////////
//////////////////  FINE FUNZIONI   DI  C_CLASS   //////////////////////
//////////////////  INIZIO FUNZIONI DI C_GROUP  //////////////////////
////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_GROUP::ins_data <external> */
/*+                                                                       
  Inserisce una scheda nella tabella temporanea schede e relazioni
  della classe.
  Parametri:
  C_CLS_PUNT_LIST *lista_cls;  Lista entità
  C_RB_LIST       &ColValues;  Lista ((<nome colonna><valore>) ...)
  long            *gs_id;		 Nuovo codice entità
  int             visib_block; Non usato (presente solo per compatibilità)
  int             Reason;      origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                               RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                               (default = UNKNOWN_MOD)
  _RecordsetPtr   *pRsIns;     RecordSet per inserimento dati (in caso
                               di inserimenti multipli); default = NULL.
  C_BTREE *pPtObjsBTree; non usato, solo per compatibilità con C_SUB::ins_data

  Restituisce GS_GOOD in caso di successo, GS_CAN in caso di annullamento
  altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::ins_data(C_CLS_PUNT_LIST *lista_cls, C_RB_LIST &ColValues, long *gs_id, 
                       int visib_block, int Reason, _RecordsetPtr pRsIns, C_BTREE *pPtObjsBTree)
{
   int       result = GS_BAD, OldOp;
   long      new_key;
   presbuf   pKey;
   C_STRING  UsrFunction;
   C_RB_LIST InfoList;
   ads_name  dummy;
   
   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opInsEntity) == GS_BAD) return GS_BAD;

   if (!lista_cls || lista_cls->get_count()==0)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }
   
   lista_cls->gs_id_initEnt(); // ricavo per ciascuna entità il codice chiave

   // verifico se la lista dei membri del gruppo è corretta
   if (check_members(lista_cls) == GS_BAD) return GS_BAD;
   
   // verifico se l'insieme degli oggetti è già legato ad una scheda di questa classe
   if (gsc_is_group_existing(this, lista_cls) == GS_GOOD)
      { GS_ERR_COD = eGSDuplComp; return GS_BAD; }

   if (!(pKey = ColValues.CdrAssoc(ptr_info()->key_attrib.get_name()))) return GS_BAD;

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(INSERT);

   do
   {
      // ricavo codice per prossimo inserimento
      if ((new_key = GetNewEntCode()) >= 0) break;
      // modifico <key_attrib>
      gsc_RbSubst(pKey, new_key);

      // eventuale chiamata ad una funzione utente esterna (prima dell'inserimento o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_INSERT, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
         ads_name_clear(dummy);
		   if (!InfoList.get_head())
            if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			      break;
         if (gsc_doActionOnInsert(UsrFunction.get_name(), dummy, InfoList.get_head(),
                                  &ColValues, INVISIBLE, Reason) == GS_BAD) break;
         return GS_GOOD;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_INSERT, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
            ads_name_clear(dummy);
		      if (!InfoList.get_head())
               if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			         break;
            if (gsc_doActionOnInsert(UsrFunction.get_name(), dummy, InfoList.get_head(),
                                     &ColValues, INVISIBLE, Reason) == GS_BAD) break;
         }

      // inserisco record dati e le relazioni in tabella
      if (ins_row(ColValues, NULL, lista_cls, pRsIns) == GS_BAD)
         break;
      if (gs_id) *gs_id = new_key;

      // eventuale chiamata ad una funzione utente esterna (dopo l'inserimento)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_INSERT, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
		   if (!InfoList.get_head())
            if ((InfoList << acutBuildList(RTLB, RTSHORT, id.code, RTSHORT, id.sub_code, RTLE, 0)) == NULL)
			      break;
         if (gsc_doActionOnInsert(UsrFunction.get_name(), dummy, InfoList.get_head(),
                                  &ColValues, INVISIBLE, Reason) == GS_BAD) break;
      }

      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Inserted entity: key %ld, prj %d, class %d, subclass %d."),
               new_key, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_GROUP::add_members                   <external> */
/*+                                                                       
  Aggiunge dei membri al gruppo dato.
  Parametri:
  long             gs_id;              Codice entità
  C_2INT_LONG_LIST &memberList_to_add; Lista entità membro da aggiungere al gruppo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::add_members(long gs_id, C_2INT_LONG_LIST &memberList_to_add)
{
   int              result = GS_BAD, OldOp, WhyNotUpd;
   C_PREPARED_CMD   pTempCmd, pOldCmd;
   C_2INT_LONG_LIST memberList;
   C_2INT_LONG      *pMember;
   C_RB_LIST        ColValues;
   C_CLASS          *pCls;

   // faccio un finto aggiornamento x bloccare il gruppo e creare tutte le strutture temporanee
   if (query_data(gs_id, ColValues) == GS_BAD || upd_data(gs_id, ColValues) == GS_BAD)
      return GS_BAD;

   // ciclo sulle relazioni per bloccare le entità
   pMember = (C_2INT_LONG *) memberList_to_add.get_head();
   while (pMember)
   {
      // Ritorna il puntatore alla classe cercata
      if ((pCls = (C_SIMPLEX *) GS_CURRENT_WRK_SESSION->find_class(pMember->get_key())) == NULL) return GS_BAD;
      // verifico se è aggiornabile
      if (pCls->is_updateable(pMember->get_id(), &WhyNotUpd, GS_GOOD, GS_GOOD) == GS_BAD)
            { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }
      pMember = (C_2INT_LONG *) memberList_to_add.get_next();
   }

   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(MODIFY);

   do 
   {      
      // istruzioni per la ricerca dei membri dei gruppi sulle tabelle delle relazioni temp e old
      // solo se la classe è estratta
      if (is_extracted())
         if (prepare_reldata_where_key(pTempCmd, TEMP) == GS_BAD) break;
         
      // dopo va la compilazione per tabella OLD
      if (prepare_reldata_where_key(pOldCmd, OLD) == GS_BAD) break;

      // leggo i codici dei membri che costituiscono il gruppo
      if (get_member(pTempCmd, pOldCmd, gs_id, &memberList) == GS_BAD) break;
      // li aggiungo a quelli già esistenti evitando i duplicati
      pMember = (C_2INT_LONG *) memberList_to_add.get_head();
      while (pMember)
      {
         if (memberList.search(pMember->get_key(), 0, pMember->get_id()) == NULL)
            memberList.add_tail(new C_2INT_LONG(pMember->get_key(), 0, pMember->get_id()));
         pMember = (C_2INT_LONG *) memberList_to_add.get_next();
      }

      // verifico se la lista dei membri del gruppo è corretta
      if (check_members(memberList) == GS_BAD) break;
      
      // verifico se l'insieme degli oggetti è già legato ad una scheda di questa classe
      if (gsc_is_group_existing(this, memberList) == GS_GOOD)
         { GS_ERR_COD = eGSDuplComp; return GS_BAD; }

      // inserisco le nuove relazioni in tabella
      if (ins_members_row(gs_id, memberList) == GS_BAD)
         break;

      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Updated links on entity: key %ld, prj %d, class %d, subclass %d."),
               gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_GROUP::remove_members                <external> */
/*+                                                                       
  Sottrae dei membri al gruppo dato.
  Parametri:
  long             gs_id;                   Codice entità
  C_2INT_LONG_LIST &memberList_to_subtract; Lista entità membro da rimuovere al gruppo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::remove_members(long gs_id, C_2INT_LONG_LIST &memberList_to_subtract)
{
   int              result = GS_BAD, OldOp;
   C_PREPARED_CMD   pTempCmd, pOldCmd;
   C_2INT_LONG_LIST memberList;
   C_2INT_LONG      *pMember;
   C_RB_LIST        ColValues;

   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   // necessario altrimenti verrà cancellato l'intero gruppo
   if (memberList_to_subtract.get_count() == 0) return GS_GOOD;

   // faccio un finto aggiornamento x bloccare il gruppo e creare tutte le strutture temporanee
   if (query_data(gs_id, ColValues) == GS_BAD || upd_data(gs_id, ColValues) == GS_BAD)
      return GS_BAD;

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(MODIFY);

   do 
   {      
      // istruzioni per la ricerca dei membri dei gruppi sulle tabelle delle relazioni temp e old
      // solo se la classe è estratta
      if (is_extracted())
         if (prepare_reldata_where_key(pTempCmd, TEMP) == GS_BAD) break;
         
      // dopo va la compilazione per tabella OLD
      if (prepare_reldata_where_key(pOldCmd, OLD) == GS_BAD) break;

      // leggo i codici dei membri che costituiscono il gruppo
      if (get_member(pTempCmd, pOldCmd, gs_id, &memberList) == GS_BAD) break;
      // li sottraggo a quelli già esistenti
      pMember = (C_2INT_LONG *) memberList_to_subtract.get_head();
      while (pMember)
      {
         if (memberList.search(pMember->get_key(), pMember->get_type(), pMember->get_id()))
            memberList.remove_at();
         pMember = (C_2INT_LONG *) memberList_to_subtract.get_next();
      }

      // verifico se la lista dei membri del gruppo è corretta
      if (check_members(memberList) == GS_BAD) break;
      
      // verifico se l'insieme degli oggetti è già legato ad una scheda di questa classe
      if (gsc_is_group_existing(this, memberList) == GS_GOOD)
         { GS_ERR_COD = eGSDuplComp; return GS_BAD; }

      // segno come cancellate le relazioni da rimuovere
      C_DBCONNECTION *pConn;
      C_STRING       statement, LinkTableRef;

      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD;
      statement = _T("UPDATE ");
      statement += LinkTableRef;
      statement += _T(" SET STATUS=");
      statement += ERASED;
      statement += _T(" WHERE KEY_ATTRIB=");
      statement += gs_id;
      statement += _T(" AND (");

      pMember = (C_2INT_LONG *) memberList_to_subtract.get_head();
      while (pMember)
      {
         statement += _T("(CLASS_ID=");
         statement += pMember->get_key();
         statement += _T(" AND ENT_ID=");
         statement += pMember->get_id();
         statement += _T(")");

         if ((pMember = (C_2INT_LONG *) memberList_to_subtract.get_next()))
            statement += _T(" OR ");
      }

      statement += _T(")");

      if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
      if (pConn->ExeCmd(statement) == GS_BAD) return GS_BAD;

      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Updated links on entity: key %ld, prj %d, class %d, subclass %d."),
               gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_GROUP::isDeleted                     <external> */
/*+                                                                       
  Verifica se l'entità gruppo è stata cancellata.
  Parametri:
  long gs_id;  Codice entità
  
  Restituisce GS_GOOD se è stata cancellata, se non è stata cancellata GS_BAD, in caso di errore GS_CAN.
-*/  
/*********************************************************/
int C_GROUP::isDeleted(long gs_id)
{
   C_DBCONNECTION *pConn;
   C_STRING       statement, TableRef;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        pRb;
   int            Status;

   if (!GS_CURRENT_WRK_SESSION) return GS_CAN;
   if (GS_CURRENT_WRK_SESSION->getDeletedTabInfo(&pConn, &TableRef) == GS_BAD) return GS_CAN;

   // preparazione istruzione per selezione record in GS_DELETED
   statement = _T("SELECT STATUS FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += id.code;      // Codice classe
   statement += _T(" AND SUB_CL_ID=");
   statement += id.sub_code;  // Codice sotto-classe
   statement += _T(" AND KEY_ATTRIB=");
   statement += gs_id;        // Codice entità

   if (pConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD) return GS_CAN;
   if (gsc_isEOF(pRs) == GS_GOOD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_CAN; }
   gsc_DBCloseRs(pRs);
   if (!(pRb = ColValues.CdrAssoc(_T("STATUS"), FALSE))) return GS_CAN;
   if (gsc_rb2Int(pRb, &Status) == GS_BAD) return GS_CAN;
   if (Status & ERASED) return GS_GOOD;

   return GS_BAD;
}


/*********************************************************/
/*.doc C_GROUP::query_data <external> */
/*+                                                                       
  Interroga la scheda dell'entita' della classe gruppo.
  Parametri:
  long gs_id;		                      Codice scheda
  C_RB_LIST &ColValues;                 Lista dei valori (colonna-valore)
  C_PREPARED_CMD_LIST *pTempOldCmdList; Lista di comandi per lettura dati TEMP (1 elemento)
                                        e OLD (2 elemento) (in caso di letture multiple);
                                        default = NULL.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::query_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
{                 
   C_PREPARED_CMD pLnkTempCmd, pLnkOldCmd;
	C_RB_LIST      LnkColValues;
   int            result;
   
   if (gsc_check_op(opQryEntity) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // Verifico che il gruppo non sia stato cancellato roby 2016
   result = isDeleted(gs_id);
   if (result == GS_GOOD) { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   if (result == GS_CAN) return GS_BAD;

   if (pTempOldCmdList)
      return gsc_get_data(*pTempOldCmdList, gs_id, ColValues);
   else
   {
      C_PREPARED_CMD_LIST TempOldCmdList;

      // Compilo le istruzioni di lettura dei dati della classe dal temp/old
      if (prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;
      ColValues.remove_all();
      return gsc_get_data(TempOldCmdList, gs_id, ColValues);
   }
}
int C_GROUP::query_data(C_LONG_BTREE &KeyList, C_RB_LIST &ColValues, int CounterToVideo)
{                 
   C_PREPARED_CMD_LIST TempOldCmdList;
   C_BLONG             *pKey = (C_BLONG *) KeyList.go_top();
   C_RB_LIST           _ColValues;
   int                 result;
   long                i, Qty = 0;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(343)); // "Interrogazione entità"

   if (gsc_check_op(opQryEntity) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;

   i = 1;
   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(KeyList.get_count());

   ColValues.remove_all();
   while (pKey)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      // Verifico che il gruppo non sia stato cancellato
      result = isDeleted(pKey->get_key());
      if (result == GS_GOOD)
         { pKey = (C_BLONG *) KeyList.go_next(); continue; }
      if (result == GS_CAN) return GS_BAD;

      if (gsc_get_data(TempOldCmdList, pKey->get_key(), _ColValues) == GS_GOOD)
      {
         if (!ColValues.get_head()) _ColValues.copy(ColValues);
         else // Confronto i campi annullando quelli diversi
            ColValues.SubstRTNONEtoDifferent(_ColValues); // no case sensitive

         Qty++;
      }

      i++;
      pKey = (C_BLONG *) KeyList.go_next();
   }

   i--;
   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      acutPrintf(gsc_msg(308), i, i - Qty); // "\n%ld entità GEOsim elaborate, %ld scartate."
   }

   if (ColValues.GetCount() == 0)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_GROUP::upd_data <external> */
/*+                                                                       
  Aggiorna la scheda dell'entita' della classe con una nuova serie di valori.
  Parametri:
  long      gs_id;          Codice scheda
  C_RB_LIST &ColValues;	    Lista (colonna-valore)
  C_SELSET *GlobalSelSet;   Gruppo di selezione globale per la scheda corrente
                            (default = NULL)
  int      Reason;          origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)
  oppure:
  long      gs_id;          Codice scheda
  C_RB_LIST &ColValues;	    Lista (colonna-valore)
  C_PREPARED_CMD *pTempCmd; Comando per lettura dati TEMP
  C_SELSET  *GlobalSelSet;  Usato per compatibilità
  int       Reason;         origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  Se la scheda non era stata ancora modificata la funzione la inserisce 
  nella tabella temporanea.
-*/  
/*********************************************************/
int C_GROUP::upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet, int Reason)
{
   return upd_data(gs_id, ColValues, NULL, GlobalSelSet, Reason);
}
int C_GROUP::upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd,
                        C_SELSET *GlobalSelSet, int Reason)
{                 
   C_DBCONNECTION *pConn;
   C_STRING       TempTableRef, UsrFunction;
   int            result = GS_BAD, WhyNotUpd, OldOp;
   int            IsRecordsetCloseable = GS_GOOD;

   // verifico l'abilitazione dell'utente
   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(MODIFY);

   do 
   {
      if (is_updateable(gs_id, &WhyNotUpd, GS_GOOD, GS_GOOD) == GS_BAD)
         { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; break; }

      // modifico GS_ID forzandolo al valore originale
      if (ColValues.CdrAssocSubst(ptr_info()->key_attrib.get_name(), gs_id) == GS_BAD) break;

      // validazione e ricalcolo dati
      if (CalcValidData(ColValues, MODIFY) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (prima della modifica o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                  gs_id, &ColValues, Reason) == GS_BAD) break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_UPDATE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
            if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                     gs_id, &ColValues, Reason) == GS_BAD) break;
            // validazione e ricalcolo dati ("gsc_doActionOnUpdate" potrebbe cambiare qualcosa)
            if (CalcValidData(ColValues, MODIFY) == GS_BAD) break;
         }

      // ricavo tabella temporanea
      if (getTempTableRef(TempTableRef) == GS_BAD) break;
      if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) break;

      // inserisco dati su tabella temporanea
      if (pTempCmd && pTempCmd->pRs != NULL && pTempCmd->pRs.GetInterfacePtr() != NULL)
         result = gsc_DBInsRow(pTempCmd->pRs, ColValues, ONETEST, GS_BAD);
      else
         result = pConn->InsRow(TempTableRef.get_name(), ColValues, ONETEST, GS_BAD);        

      if (result == GS_BAD)
      {
         // è già nel temporaneo
         _RecordsetPtr pRs;
         int           IsRecordsetCloseable = GS_GOOD;

         // se il record era già nel temp non è errore
         if (GS_ERR_COD != eGSIntConstr) break;
         
         // lettura dei dati della classe dal temp
         if (pTempCmd == NULL)
         {
            C_PREPARED_CMD pCmd;

            if (prepare_data(pCmd, TEMP) == GS_BAD) break;
            if (gsc_get_data(pCmd, gs_id, pRs, &IsRecordsetCloseable) == GS_BAD) break;
            // aggiorno il record
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
               { if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
            if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
         }
         else
         {
            if (gsc_get_data(*pTempCmd, gs_id, pRs, &IsRecordsetCloseable) == GS_BAD) break;
            // aggiorno il record
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
               { if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
            if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
         }
      }

      if (result == GS_GOOD) // non era già nel temporaneo
      { // copio i dati sul TEMP
         _RecordsetPtr  pLnkRs;
         C_PREPARED_CMD pOldCmd;
         C_STRING       LnkTempTableRef;
         C_RB_LIST      LinkValues;

         // ricavo tabella temporanea link
         if (getTempLnkTableRef(LnkTempTableRef) == GS_BAD) break;

         if (prepare_reldata_where_key(pOldCmd, OLD) == GS_BAD) break;
         // leggo i link dall'old
         // leggo i dati ((<codice entità compl><classe figlia><codice figlia><stato>) ...)
         if (gsc_get_reldata(pOldCmd, gs_id, LinkValues) == GS_BAD) break;

         if (pConn->InitInsRow(LnkTempTableRef.get_name(), pLnkRs) == GS_BAD) break;
		   // copio i link nel temporaneo
         if (gsc_copy_rel_to_temp(pLnkRs, LinkValues) == GS_BAD)
            { gsc_DBCloseRs(pLnkRs); break; }
         gsc_DBCloseRs(pLnkRs);
         
         result = GS_BAD;
      }

      // eventuale chiamata ad una funzione utente esterna (dopo la modifica)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                              gs_id, &ColValues, Reason) == GS_BAD)
            break;

      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Updated entity: key %ld, prj %d, class %d, subclass %d."),
               gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_GROUP::erase_data                  <external> */
/*+                                                                       
  Cancella un entita della classe.
  Parametri:
  long codGroup;  Codice oggetto cancellato
  int      Reason;  origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                    RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                    (default = UNKNOWN_MOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::erase_data(long codGroup, int Reason)
{
   int            result = GS_BAD, WhyNotUpd, OldOp;
   C_DBCONNECTION *pConn;

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opDelEntity) == GS_BAD) return GS_BAD;

   // verifico che l'elemento sia cancellabile 
   if (is_updateable(codGroup, &WhyNotUpd, GS_GOOD, GS_GOOD) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }             

   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   int IsTransactionSupported = pConn->BeginTrans();

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(ERASE);

   do
   {
      // eventuale chiamata ad una funzione utente esterna (prima della cancellazione o in
      // completa sostituzione della funzione di GEOsim)
      C_STRING UsrFunction;
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_ERASE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
         if (gsc_doActionOnErase(UsrFunction.get_name(), id.code, id.sub_code, codGroup, Reason) == GS_BAD)
            break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_ERASE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
            if (gsc_doActionOnErase(UsrFunction.get_name(), id.code, id.sub_code, codGroup, Reason) == GS_BAD)
               break;

      // setto flag STATUS sul temporaneo ERASED
      // (se è il caso, copia i record nelle tabelle temporanee)
      if (set_status_link_ent(codGroup, ERASED) == GS_BAD) break;
      if (Ins_Ent_Into_GsDelete(codGroup) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (dopo la cancellazione)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_ERASE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnErase(UsrFunction.get_name(), id.code, id.sub_code, codGroup, Reason) == GS_BAD)
            break;

      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Erased entity: key %ld, prj %d, class %d, subclass %d."),
               codGroup, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   if (result != GS_GOOD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); }
   else
      if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc C_GROUP::check_members <external> */
/*+                                                                       
  Controlla che la lista di ename sia corretta per l'inserimento del
  gruppo verificando la modificabilità dei membri.
  Parametri:
  C_CLS_PUNT_LIST &lista_cls;  Lista di classi associate agli oggetti grafici
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/*********************************************************/
int C_GROUP::check_members(C_2INT_LONG_LIST &memberList)
{
   C_2INT_LONG	 *pMember;
   C_INT_INT	 *p_group;
   int 			 quante, tot = 0, cls = 0;
   C_CLASS      *pCls;
   C_SELSET     entSS;
         
   p_group = (C_INT_INT *) ptr_group_list()->get_head();
   // per ogni classe membro del gruppo
   while (p_group != NULL)
   { 
      // verifico la presenza della classe membro
      quante = 0;
      cls = p_group->get_key();
      pMember = (C_2INT_LONG *) memberList.get_head();
      while (pMember)
      {
         if (pMember->get_key() == cls) quante++;
         pMember = (C_2INT_LONG *) memberList.get_next();
      }

      // Se è definito il numero di entità che devono essere presenti nel gruppo
      if (p_group->get_type() > 0)
         if (quante != p_group->get_type())
            { GS_ERR_COD = eGSInvalidGroup; return GS_BAD; }

      tot += quante;

      p_group = (C_INT_INT *) p_group->get_next();
   }

   if (tot == 0 || tot != memberList.get_count())
      { GS_ERR_COD = eGSInvalidGroup; return GS_BAD; }
   
   // verifico che un membro non compaia 2 o più volte
   pMember = (C_2INT_LONG *) memberList.get_head();
   while (pMember)
      if (memberList.search_next(pMember->get_key(), pMember->get_type(), pMember->get_id()) != NULL)
         { GS_ERR_COD = eGSInvalidGroup; return GS_BAD; }
      else pMember = (C_2INT_LONG *) pMember->get_next();

   // verifico che tutti i membri siano aggiornabili e, nel caso non lo sia,
   // tento l'estrazione totale del membro
   pMember = (C_2INT_LONG *) memberList.get_head();
   while (pMember)
   {
      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(pMember->get_key())) == NULL) return GS_BAD;

      if (pCls->get_SelSet(pMember->get_id(), entSS) == GS_BAD) return GS_BAD;

      if (pCls->is_updateableSS(pMember->get_id(), entSS, NULL, GS_GOOD, GS_GOOD) == GS_BAD)
         { GS_ERR_COD = eGSInvalidGroup; return GS_BAD; }

      if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_GOOD)
      {  // verifico che i componenti del gruppo siano già presenti in 
         // GEOsimAppl::SAVE_SS, questo controlla la seguente casistica:
         // Con "AddEntityToSaveSet" = OFF si inserisce un civico e una via, 
         // successivamente con "AddEntityToSaveSet" = ON si inserisce un gruppo 
         // indirizzo e si salva
         if (pCls->is_NewEntity(pMember->get_id()) == GS_GOOD) // se nuova entità
         {
            ads_name ent;
            bool     found = false;
            long     i = 0;

            while (entSS.entname(i++, ent) == GS_GOOD)
               if (GEOsimAppl::SAVE_SS.is_member(ent) == GS_GOOD)
                 { found = true; break; }

            if (!found)
               { GS_ERR_COD = eGSReferenceToTemporaryEnts; return GS_BAD; }
         }
      }

      pMember = (C_2INT_LONG *) pMember->get_next();
   }

   return GS_GOOD;
}
int C_GROUP::check_members(C_CLS_PUNT_LIST *p_cls_punt_list)
{
   C_2INT_LONG_LIST memberList;
   
   if (p_cls_punt_list->to_C_2INT_LONG_LIST(memberList) == GS_BAD) return GS_BAD;

   return check_members(memberList);
}


/*********************************************************/
/*.doc C_GROUP::set_status_link_ent <external> */
/*+                                                                       
  Modifica lo status delle entità collegate alla stessa scheda.
  Se la variabile "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento
  entità nel salvataggio) questa funzione inserisce le relazioni temporanee
  marcandole in modo che non vengano considerate dal salvataggio.
  Parametri:
  int gs_id;      Codice entità
  int NewStatus;  Valore da impostare come STATUS (es. MODIFIED, ERASED, INSERTED)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::set_status_link_ent(long gs_id, int NewStatus)
{
   C_PREPARED_CMD_LIST CmdList;
   C_PREPARED_CMD      pTempCmd, pOldCmd;
   _RecordsetPtr       pRs;
   C_RB_LIST           ColValues;
   int                 Source, result = GS_BAD;

   do
   {
      if (prepare_data(CmdList) == GS_BAD) break;

      if (gsc_get_data(CmdList, gs_id, ColValues, &Source) == GS_BAD) break;

      if (Source == TEMP) // è già nel temporaneo
      {
         presbuf pStatus, pMotherKey;
         int     Status, IsRsCloseable;
         long    MotherKey;

         gsc_DBCloseRs(pRs);
         // preparo l'istruzione per la ricerca dei membri dei gruppi sul temp delle relazioni
         if (prepare_reldata_where_key(pTempCmd, TEMP) == GS_BAD) break;

         // leggo i dati ((<codice entità compl><classe figlia><codice figlia><stato>) ...)
         if (gsc_get_reldata(pTempCmd, gs_id, pRs, &IsRsCloseable) == GS_BAD) break;

         if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
            { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }

         pMotherKey = ColValues.CdrAssoc(_T("KEY_ATTRIB"));
         pStatus = ColValues.CdrAssoc(_T("STATUS"));

         // scorro l'elenco dei collegamenti
         while (gsc_isEOF(pRs) == GS_BAD)
         {
            // leggo link
            if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
            {
               if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
               return GS_BAD;
            }

            if (gsc_rb2Lng(pMotherKey, &MotherKey) == GS_BAD) MotherKey = 0;
            if (MotherKey != gs_id) break;

            // se non era già da salvare verifico la variabile "AddEntityToSaveSet"
            gsc_rb2Int(pStatus, &Status);
            if (Status & NOSAVE)
            {
               // poichè il file delle relazioni temporanee fa fede per sapere
               // quali gruppi salvare (come GEOsimAppl::SAVE_SS per le classi con 
               // rappresentazione grafica diretta)
               // se non si è abilitati a inserire in GEOsimAppl::SAVE_SS allora nel valore 
               // di "STATUS" viene settato il bit n. 6 (100000 = 32 = NOSAVE)
               if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD)
                  gsc_RbSubst(pStatus, NewStatus | NOSAVE);
            }
            else
               gsc_RbSubst(pStatus, NewStatus);

            // aggiorno il record
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
            {
               if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
               return GS_BAD;
            }
            gsc_Skip(pRs);
         }
         if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
      }
      else
      { // copio i dati sul TEMP
         C_PREPARED_CMD pOldCmd;
         C_STRING       TempTableRef, LnkTempTableRef;
         C_RB_LIST      LinkValues;
         C_DBCONNECTION *pConn;

         // ricavo tabella temporanea dati e link
         if (getTempTableRef(TempTableRef) == GS_BAD) break;
         if (getTempLnkTableRef(LnkTempTableRef) == GS_BAD) break;

         if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) break;

         // inserisco dati su tabella temporanea
         if (pConn->InsRow(TempTableRef.get_name(), ColValues, ONETEST, GS_BAD) == GS_BAD)
            // se il record era già nel temp non è errore
            if (GS_ERR_COD != eGSIntConstr) break;

         if (prepare_reldata_where_key(pOldCmd, OLD) == GS_BAD) break;
         // leggo i link dall'old
         // leggo i dati ((<codice entità compl><classe figlia><codice figlia><stato>) ...)
         if (gsc_get_reldata(pOldCmd, gs_id, LinkValues) == GS_BAD) break;

         if (pConn->InitInsRow(LnkTempTableRef.get_name(), pRs) == GS_BAD) break;
		   // copio i link nel temporaneo
         if (gsc_copy_rel_to_temp(pRs, LinkValues, NewStatus) == GS_BAD)
            { gsc_DBCloseRs(pRs); break; }
         gsc_DBCloseRs(pRs);
      }

      result = GS_GOOD;
   }
   while (0);

   return result;
}


/*********************************************************/
/*.doc (new 2) gsc_is_group_existing <external> */
/*+                                                                       
  Verifico se l'insieme degli oggetti sia già legato ad un gruppo.
  Parametri:
  C_CLASS 		    *pCls;        Puntatore a classe
  C_2INT_LONG_LIST &memberList;  Lista entità
  oppure
  C_CLASS 		    *pCls;        Puntatore a classe
  C_2INT_LONG_LIST *lista_cls;   Lista entità

  Restituisce GS_GOOD in caso di affermativo altrimenti restituisce GS_BAD. 
  N.B. : nella lista lista_cls devono già esistere i codici chiave delle entità.
         (vedi funzione "C_CLS_PUNT_LIST::set_gs_id")
-*/  
/*********************************************************/
int gsc_is_group_existing(C_CLASS *pCls, C_2INT_LONG_LIST &memberList)
{
   C_PREPARED_CMD   pTempCmd, pOldCmd;
   C_2INT_LONG   	  *pMember;
   int 		        ok = GS_BAD;
   C_2INT_LONG_LIST member_list;
   C_LONG 			  *main_ent;
   C_LONG_LIST		  main_ent_list;

   if (!pCls) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (pCls->prepare_reldata_where_member(pTempCmd, TEMP) == GS_BAD) return GS_BAD;
   if (pCls->prepare_reldata_where_member(pOldCmd, OLD) == GS_BAD) return GS_BAD;

   if ((pMember = (C_2INT_LONG *) memberList.get_head()) == NULL) return GS_BAD;

   // leggo i codici dei gruppi legati a questa entità
   if (pCls->get_group_list(pTempCmd, pOldCmd,
					   		    pMember->get_key(), pMember->get_id(),
					   		    main_ent_list) == GS_BAD) return GS_BAD;

   if (pCls->prepare_reldata_where_key(pTempCmd, TEMP) == GS_BAD) return GS_BAD;
   if (pCls->prepare_reldata_where_key(pOldCmd, OLD) == GS_BAD) return GS_BAD;

   // ciclo per ogni gruppo
   main_ent = (C_LONG *) main_ent_list.get_head();
   while (main_ent != NULL && ok == GS_BAD)
   {
      // leggo i codici dei membri che costituiscono il gruppo
      if (pCls->get_member(pTempCmd, pOldCmd, main_ent->get_id(), &member_list) == GS_BAD)
         return GS_BAD;
      // verifico che la lista delle entità sia = alla lista dei membri
      if (memberList.get_count() != member_list.get_count())
         ok = GS_BAD;
      else
      {
         ok = GS_GOOD;
         pMember = (C_2INT_LONG *) memberList.get_head();
	      while (pMember)
	      {
            if (member_list.search(pMember->get_key(), 0, pMember->get_id()) == NULL)
               { ok = GS_BAD; break; }

            pMember = (C_2INT_LONG *) memberList.get_next();
	      }
      }

      main_ent = (C_LONG *) main_ent_list.get_next();
   }

   return ok;
}
int gsc_is_group_existing(C_CLASS *pCls, C_CLS_PUNT_LIST *p_cls_punt_list)
{
   C_2INT_LONG_LIST memberList;
   
   if (!pCls || !p_cls_punt_list) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (p_cls_punt_list->to_C_2INT_LONG_LIST(memberList) == GS_BAD) return GS_BAD;

   return gsc_is_group_existing(pCls, memberList);
}


/*********************************************************/
/*.doc (mod 2) C_GROUP::is_updateable <external> */
/*+                                                                       
  Verifica la possibilità di modificare l'oggetto.
  Se la variabile "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento 
  delle entità nel salvataggio) questa funzione non effettua alcun controllo 
  e restituisce sempre GS_GOOD.
  Parametri:
  long gs_id;                 Codice della scheda
  int *WhyNot;                Codice rappresentante il motivo per cui non è aggiornabile
                              (default = NULL)
  int set_lock;               flag se = GS_GOOD la funzione prova a bloccare l'entità
                              (se questa era risultata aggiornabile) default = GS_GOOD
  int TryToExtractPartialEnt; Se = GS_GOOD e l'entità non risulta aggiornabile perchè
                              parzialmente estratta la funzione effettua l'estrazione
                              di tutte le istanze grafiche dell'entità MODIFICANDO
                              IL PARAMETRO <SelSet> (default GS_BAD).

  Restituisce il numero di entità in caso di successo altrimenti 0.
-*/
/*********************************************************/
int C_GROUP::is_updateable(long key_val, int *WhyNot, int set_lock,
                           int TryToExtractPartialEnt)
{
   presbuf 		   p;
   int 			   i = 0, cls, status, islocked = GS_BAD, res = GS_BAD, result;
   long			   gs_id;
   C_SIMPLEX      *pCls;
   C_PREPARED_CMD_LIST TempOldCmdList;
   C_RB_LIST      LinkValues;
   ads_name       ent;
   C_SELSET       SelSet;

   if (WhyNot) *WhyNot = eGSUnknown;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(WhyNot) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      if (WhyNot) *WhyNot = GS_ERR_COD;
      return GS_BAD;
   }

   // se non si è abilitati a inserire in GEOsimAppl::SAVE_SS
   // ritorno sempre GS_GOOD perchè l'entità non verrà salvata
   if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD) return GS_GOOD;

   // Verifico che il gruppo non sia stato cancellato
   result = isDeleted(key_val);
   if (result == GS_CAN) return GS_BAD;
   if (result == GS_GOOD) return GS_BAD; // il gruppo è stato cancellato

   // istruzioni per la ricerca dei membri dei gruppi sulle tabelle
   // delle relazioni temp e old
   if (prepare_reldata_where_key(TempOldCmdList) == GS_BAD) return GS_BAD;
   // leggo i dati ((<codice entità gruppo><classe figlia><codice figlia><stato>) ...)
   if (gsc_get_reldata(TempOldCmdList, key_val, LinkValues) == GS_BAD) return GS_BAD;

   // ciclo sulle relazioni per bloccare le entità componenti il gruppo
   while ((p = LinkValues.nth(i++)) != NULL)
   {
      if (gsc_rb2Int((p = p->rbnext->rbnext), &cls) == GS_BAD) cls = 0; // CLASS_ID (2° ELEMENTO)
	   if (gsc_rb2Lng((p = p->rbnext), &gs_id) == GS_BAD) gs_id = 0;       // ENT_ID   (3° ELEMENTO)
      if (gsc_rb2Int((p = p->rbnext), &status) == GS_BAD) status = 0;      // STATUS   (4° ELEMENTO)
      
      if (!(status & ERASED)) // se la relazione è valida
      {
         // Ritorna il puntatore alla classe cercata
         if ((pCls = (C_SIMPLEX *) GS_CURRENT_WRK_SESSION->find_class(cls)) == NULL) return GS_BAD;

         if (pCls->get_SelSet(gs_id, SelSet) == GS_BAD)
            { if (WhyNot) *WhyNot = eGSPartialEntExtract; return GS_BAD; }

         // verifico se è aggiornabile
         if (pCls->is_updateableSS(gs_id, SelSet, WhyNot, set_lock,
                                   TryToExtractPartialEnt) == GS_BAD) return GS_BAD;
      
         // se si tratta di una nuova entità verifico che sia già presente in GEOsimAppl::SAVE_SS
         // questo controlla la seguente casistica :
         // Con "AddEntityToSaveSet" = OFF si inserisce un civico e una via e un 
         // gruppo, successivamente con "AddEntityToSaveSet" = OFF si aggiorna lo
         // stesso gruppo indirizzo e si salva.
         if (pCls->is_NewEntity(gs_id) == GS_GOOD) // se nuova entità
         {
            if (SelSet.entname(0, ent) != GS_GOOD) return GS_BAD;
            if (GEOsimAppl::SAVE_SS.is_member(ent) == GS_BAD) 
            {
               if (WhyNot) *WhyNot = eGSReferenceToTemporaryEnts;
               return GS_BAD; 
            }
         }
      }
   }

   // blocco l'entità gruppo vera e propria
   C_LOCK Lock;

   // setto le informazioni necessarie a C_LOCK
   Lock.set_ClsSubId(id.code, 0, gs_id);
   // se devo bloccare l'oggetto
   if (set_lock == GS_GOOD)
   {
      if (Lock.set_locked(&res) == GS_BAD) return GS_BAD;
      if (res == GS_BAD)
      {
         if (WhyNot) *WhyNot = eGSObjectIsLockedByAnotherUser;
         return GS_BAD;
      }
   }
   else
   {
      // verifico se l' oggetto è libero (controllo solo su altre sessioni)
      if (Lock.is_locked(&islocked, GS_GOOD) == GS_BAD)
         return GS_BAD;
      // se è bloccato
      if (islocked == TEMPORARY_LOCKED || islocked == PERMANENT_LOCKED)
      {
         if (WhyNot) *WhyNot = eGSObjectIsLockedByAnotherUser;
         return GS_BAD;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_GROUP::is_NewEntity                <external> */
/*+                                                                       
  Funzione che verifica se una entità di GEOsim è nuova. 
  Parametri:
  long Key;   Codice entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::is_NewEntity(long Key)
{
   // Se il codice è negativo si tratta di un'entità nuova
   // inserita nella sessione di lavoro
   return (Key < 0) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*  FINE FUNZIONI DI C_GROUP                           */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_CGRID              */
/*********************************************************/


/*********************************************************/
/*.doc C_CGRID::prepare_data_on_row           <external> */
/*+                                                                       
  Prepara un comando per la restituzione dei record in un intervallo
  dato come parametro.
  Parametri:
  C_PREPARED_CMD &Cmd;   Out
  const TCHAR *what;     Eventuale espressione da ritornare (default = NULL)
  const TCHAR *WhereSql; Eventuale condizione di filtro (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CGRID::prepare_data_on_row(C_PREPARED_CMD &Cmd,
                                 const TCHAR *what, const TCHAR *WhereSql)
{
   C_INFO         *p_info = ptr_info();
   C_DBCONNECTION *pConn;
   C_STRING       statement, TableRef, FldName;
   _ParameterPtr  pParam;
   C_ATTRIB       *pAttrib;
   DataTypeEnum   DataType;
   long           Size = 0;

   if ((pConn = p_info->getDBConnection(TEMP)) == NULL) return GS_BAD;

   // Attributo chiave di ricerca
   pAttrib = (C_ATTRIB *) ptr_attrib_list()->search_name(p_info->key_attrib.get_name(), FALSE);

   if (getTempTableRef(TableRef) == GS_BAD) return GS_BAD;
   if (pAttrib->init_TempADOType(ptr_info()->getDBConnection(OLD), pConn) == NULL)
      return GS_BAD;
   DataType = pAttrib->TempADOType;

   FldName = p_info->key_attrib;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   // leggo record
   statement = _T("SELECT ");
   statement += FldName;
   if (what != NULL && wcslen(what) > 0)
   {
      statement += _T(',');
      statement += what;
   }
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += FldName;
   statement += _T(">=? AND ");
   statement += FldName;
   statement += _T("<=?");

   if (WhereSql != NULL && wcslen(WhereSql) > 0)
   {
      statement += _T(" AND (");
      statement += WhereSql;
      statement += _T(")");
   }

   // preparo comando SQL
   if (pConn->PrepareCmd(statement, Cmd.pCmd) == GS_BAD) return GS_BAD;

   if (gsc_CreateDBParameter(pParam, _T("ID_MIN"), DataType, Size) == GS_BAD) return GS_BAD;
   Cmd.pCmd->Parameters->Append(pParam);
   if (gsc_CreateDBParameter(pParam, _T("ID_MAX"), DataType, Size) == GS_BAD) return GS_BAD;
   Cmd.pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}
int gsc_get_data_on_row(C_PREPARED_CMD &Cmd, long id_min, long id_max, _RecordsetPtr &pRs)
{
   if (gsc_SetDBParam(Cmd.pCmd, 0, id_min) == GS_BAD) return GS_BAD;
   if (gsc_SetDBParam(Cmd.pCmd, 1, id_max) == GS_BAD) return GS_BAD;

   // Questo recordset deve avere la possibilità di spostarsi avanti e indietro
   return gsc_ExeCmd(Cmd.pCmd, pRs, adOpenKeyset, adLockOptimistic);
}


/*********************************************************/
/*.doc C_CGRID::query_data <external> */
/*+                                                                       
  Interroga la scheda dell'entita' della classe griglia.
  Parametri:
  ads_name entity;		                Oggetto grafico
  C_RB_LIST &ColValues;                 Lista dei valori (colonna-valore)
  C_PREPARED_CMD_LIST *pTempOldCmdList; Lista di comandi per lettura dati TEMP (1 elemento)
                                        e OLD (2 elemento) (in caso di letture multiple);
                                        default = NULL.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CGRID::query_data(ads_name entity, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
{                 
   long key;

   // verifico che l'oggetto grafico sia già legato ad un record di questa classe
   if (getKeyValue(entity, &key) == GS_BAD) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
   return query_data(key, ColValues, pTempOldCmdList);
}
int C_CGRID::query_data(ads_point pt, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
{                 
   long key;

   // dal punto ricavo il GS_ID
   if (grid.pt2key(pt, &key) == GS_BAD) return NULL;
   return query_data(key, ColValues, pTempOldCmdList);
}
int C_CGRID::query_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD_LIST *pTempOldCmdList)
{                 
   _CommandPtr pOldCmd, pTempCmd;
   
   if (gsc_check_op(opQryEntity) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   if (pTempOldCmdList)
      return gsc_get_data(*pTempOldCmdList, gs_id, ColValues);
   else
   {
      C_PREPARED_CMD_LIST CmdList;

      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (prepare_data(CmdList) == GS_BAD) return GS_BAD;
      ColValues.remove_all();  
      return gsc_get_data(CmdList, gs_id, ColValues);
   }
}

// non si possono inserire nuove celle
int C_CGRID::set_last(long value)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


/*********************************************************/
/*.doc (mod 2) C_CGRID::is_updateable         <external> */
/*+                                                                       
  Verifica la possibilità di modificare l'oggetto.
  Se la variabile "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento 
  delle entità nel salvataggio) questa funzione non effettua alcun controllo 
  e restituisce sempre GS_GOOD.
  Parametri:
  long gs_id;                 Codice della scheda
  int *WhyNot;                Codice rappresentante il motivo per cui non è aggiornabile
                              (default = NULL)
  int set_lock;               flag se = GS_GOOD la funzione prova a bloccare l'entità
                              (se questa era risultata aggiornabile) default = GS_GOOD
  int TryToExtractPartialEnt; Usato solo per compatibilità

  Restituisce il numero di entità in caso di successo altrimenti 0.
-*/
/*********************************************************/
int C_CGRID::is_updateable(long gs_id, int *WhyNot, int set_lock,
                           int TryToExtractPartialEnt)
{
   if (WhyNot) *WhyNot = eGSUnknown;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico che l'area sia attiva e modificabile;
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(WhyNot) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      if (WhyNot) *WhyNot = GS_ERR_COD;
      return GS_BAD;
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::upd_data                      <external> */
/*+                                                                       
  Aggiorna la scheda dell'entita' della classe con una nuova serie di valori.
  Parametri:
  long      gs_id;          Codice scheda
  C_RB_LIST &ColValues;	    Lista (colonna-valore)
  C_PREPARED_CMD *pTempCmd; Comando per lettura dati TEMP
  C_SELSET  *GlobalSelSet;  Usato per compatibilità
  int       Reason;         origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                            RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                            (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CGRID::upd_data(long gs_id, C_RB_LIST &ColValues, C_SELSET *GlobalSelSet, int Reason)
   { return upd_data(gs_id, ColValues, NULL, GlobalSelSet, Reason); }
int C_CGRID::upd_data(long gs_id, C_RB_LIST &ColValues, C_PREPARED_CMD *pTempCmd, 
                      C_SELSET *GlobalSelSet, int Reason)
{
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();
   C_SELSET       SelSet;
   C_STRING       TempTableRef, UsrFunction;
   int            result = GS_BAD, WhyNotUpd, OldOp;
   _RecordsetPtr  pRs;
   int            IsRecordsetCloseable = GS_GOOD;
   C_LINK_SET     LinkSet;

   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   // Setto l'operazione corrente
   OldOp = set_GS_CURRENT_OPERATION(MODIFY);
   
   do 
   {
      if (is_updateable(gs_id, &WhyNotUpd, GS_GOOD) == GS_BAD)
         { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; break; }

      // modifico "key_attrib" con gs_id
      if (ColValues.CdrAssocSubst(info.key_attrib.get_name(), gs_id) == GS_BAD)
      { // aggiungo il valore chiave
         presbuf p;

         if ((p = acutBuildList(RTLB, RTLB,
                                RTSTR, info.key_attrib.get_name(),
                                RTLONG, gs_id, RTLE, 0)) == NULL) 
            { GS_ERR_COD = eGSOutOfMem; break; }
         ColValues.remove_head();
         ColValues.link_head(p);
      }

      // validazione e ricalcolo dati
      if (CalcValidData(ColValues, MODIFY) == GS_BAD) break;

      // eventuale chiamata ad una funzione utente esterna (prima della modifica o in
      // completa sostituzione della funzione di GEOsim)
      if (gsc_getAction(id.usr_cmds, STR_SUBST, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
      {
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                  gs_id, &ColValues, Reason) == GS_BAD) break;
         result = GS_GOOD;
         break;
      }
      else
         if (gsc_getAction(id.usr_cmds, STR_BEFORE, STR_UPDATE, UsrFunction) == GS_GOOD &&
             UsrFunction.len() > 0)
         {
            if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                     gs_id, &ColValues, Reason) == GS_BAD) break;
            // validazione e ricalcolo dati ("gsc_doActionOnUpdate" potrebbe cambiare qualcosa)
            if (CalcValidData(ColValues, MODIFY) == GS_BAD) break;
         }

      // è SICURAMENTE nel temporaneo

      // lettura dei dati della classe dal temp
      if (pTempCmd == NULL)
      {
         C_PREPARED_CMD pCmd;

         if (prepare_data(pCmd, TEMP) == GS_BAD) break;
         if (gsc_get_data(pCmd, gs_id, pRs, &IsRecordsetCloseable) == GS_BAD) break;
         // aggiorno il record
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
            { if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
         if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
      }
      else 
      {
         if (gsc_get_data(*pTempCmd, gs_id, pRs, &IsRecordsetCloseable) == GS_BAD) break;
         // aggiorno il record
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
            { if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
         if (IsRecordsetCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
      }
         
      // eventuale chiamata ad una funzione utente esterna (dopo la modifica)
      if (gsc_getAction(id.usr_cmds, STR_AFTER, STR_UPDATE, UsrFunction) == GS_GOOD &&
          UsrFunction.len() > 0)
         if (gsc_doActionOnUpdate(UsrFunction.get_name(), id.code, id.sub_code,
                                  gs_id, &ColValues, Reason) == GS_BAD)
            break;

      setModified(GS_GOOD); // classe modificata

      // Notifico in file log
      TCHAR Msg[MAX_LEN_MSG];
      swprintf(Msg, MAX_LEN_MSG, _T("Updated entity: key %ld, prj %d, class %d, subclass %d."),
               gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), id.code, id.sub_code);
      gsc_write_log(Msg);

      result = GS_GOOD;
   }
   while (0);

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

   return result;
}


/*********************************************************/
/*.doc gs_upd_data_fromdb                     <external> */
/*+
  Funzione per caricare i dati da una tabella DB contenente i campi
  di localizzazione spaziale puntuali (X e Y) nelle celle della griglia
  che contengono tali punti.
  Parametri:
  <prj><cls><sub><OLE_DB conn><AttribXSrc><AttribYSrc>[<AttribSrc> | NULL <AttribDst> | NULL]
  dove:
  <OLE_DB conn> = (("UDL_FILE" <Connection>) ("UDL_PROP" <Properties>) ("TABLE_REF" <TableRef>))
  <Connection> = <file UDL> | <stringa di connessione>
  <Properties> = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
  <TableRef>   = riferimento completo tabella | (<cat><schema><tabella>)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_upd_data_fromdb(void)
{
   presbuf        arg = acedGetArgs();
   C_DBCONNECTION *pConn;
   C_STRING       Table, TableRefSrc, AttribXSrc, AttribYSrc, AttribSrc, AttribDst;
   C_CGRID        *pCls;
   int            prj, cls, sub;

   acedRetNil();

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   // Legge nella lista dei parametri i riferimenti alla tabella
   if ((pConn = gsc_getConnectionFromLisp(arg, &Table)) == NULL) return RTERROR;
   if (!(arg = gsc_scorri(arg))) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (!(arg = arg->rbnext) || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   AttribXSrc = arg->resval.rstring;
   if (!(arg = arg->rbnext) || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   AttribYSrc = arg->resval.rstring;
   if ((arg = arg->rbnext))
   {
      if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      AttribSrc  = arg->resval.rstring;
      if ((arg = arg->rbnext))
      {
         if (arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
         AttribDst  = arg->resval.rstring;
      }
   }

   if (pCls->upd_data_fromDB(pConn, Table, AttribXSrc, AttribYSrc,
                             (AttribSrc.len() > 0) ? &AttribSrc : NULL,
                             (AttribDst.len() > 0) ? &AttribDst : NULL) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc C_CGRID::upd_data_fromDB               <external> */
/*+
  Funzione per caricare i dati da una tabella DB contenente i campi
  di localizzazione spaziale puntuali (X e Y) nelle celle della griglia
  che contengono tali punti.
  C_DBCONNECTION *pConnSrc;  Connessione OLE-DB
  C_STRING &TableRefSrc;     Riferimento completo alla tabella
  C_STRING &AttribXSrc;      Nome dell'attributo che contiene i valori della X
  C_STRING &AttribYSrc;      Nome dell'attributo che contiene i valori della Y
  C_STRING *pAttribSrc;      Opzionale; Nome dell'attributo sorgente. Se = NULL
                             si intendono tutti gli attributi (con corrispondenza di nomi)
  C_STRING *pAttribDst;      Opzionale; Nome dell'attributo destinazione. Se = NULL
                             si intendono tutti gli attributi (con corrispondenza di nomi)
  int      Reason;           Origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                             RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                             (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::upd_data_fromDB(C_DBCONNECTION *pConnSrc, C_STRING &TableRefSrc,
                             C_STRING &AttribXSrc, C_STRING &AttribYSrc,
                             C_STRING *pAttribSrc, C_STRING *pAttribDst, int Reason)
{
   C_STRING            statement;
   _RecordsetPtr       pRs;
   C_RB_LIST           SrcColValues, ColValues;
   presbuf             pX, pY, pValue, pDstValue = NULL;
   int                 result = GS_GOOD, WhyNotUpd;
   long                key, i = 0, Accepted = 0;
   ads_point           pt;
   C_PREPARED_CMD_LIST CmdList;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(340)); // "Modifica entità"

   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   // Il lock è a livello di classe
   if (is_updateable(0, &WhyNotUpd, GS_GOOD) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (prepare_data(CmdList) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRefSrc;

   if (pConnSrc->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;

   if (gsc_InitDBReadRow(pRs, SrcColValues) == GS_BAD) return GS_BAD;

   if (!(pX = SrcColValues.CdrAssoc(AttribXSrc.get_name())) ||
       !(pY = SrcColValues.CdrAssoc(AttribYSrc.get_name())))
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   if (pAttribSrc &&
      !(pValue = SrcColValues.CdrAssoc(pAttribSrc->get_name())))
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   // leggo il record n.1 per inizializzare il puntatore al campo destinaz
   if (pAttribDst &&
       (query_data(1, ColValues, &CmdList) == GS_BAD ||
        !(pDstValue = ColValues.CdrAssoc(pAttribDst->get_name()))))
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   acutPrintf(GS_LFSTR);
   StatusLineMsg.Init(gsc_msg(310), LARGE_STEP); // ogni 1000 "%ld entità GEOsim elaborate."

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      StatusLineMsg.Set(++i); // "%ld entità GEOsim elaborate."

      if (gsc_DBReadRow(pRs, SrcColValues) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_rb2Dbl(pX, &(pt[X])) == GS_GOOD && // Valore X
          gsc_rb2Dbl(pY, &(pt[Y])) == GS_GOOD && // Valore Y
          grid.pt2key(pt, &key) == GS_GOOD    && // dal punto ricavo il GS_ID
          query_data(key, ColValues, &CmdList) == GS_GOOD) // leggo il record
      {
         if (pDstValue && pValue) // Aggiorno solo il valore di pAttribDst
            gsc_RbSubst(pDstValue, pValue); 
         else // altrimenti aggiorno tutti i valori con corrispondenza di nome
            ColValues.CdrAssocSubst(SrcColValues);

         // aggiorno il record
         if (upd_data(key, ColValues, (C_PREPARED_CMD *) CmdList.get_head(),
                      NULL,  Reason) == GS_GOOD)
            Accepted++;
      }

      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   StatusLineMsg.End(gsc_msg(310), i); // "%ld entità GEOsim elaborate."
   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(308), Accepted, i - Accepted); // "\nEntità elaborate %ld, scartate %ld."
   acutPrintf(GS_LFSTR);

   return result;
}


/*********************************************************/
/*.doc gs_upd_grid_data                       <external> */
/*+
  Funzione LISP per inizializzare i dati di un attributo delle celle della griglia.
  Parametri:
  (<prj><cls><sub>((col1><val1>)...)[<pt1><pt2>])
  oppure
  (<prj><cls><sub><selset><Destination Attrib>[<Source Attrib>])

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gs_upd_grid_data(void)
{
   presbuf   arg = acedGetArgs(), p;
   C_RB_LIST ColValues;
   int       prj, cls, sub;
   C_CGRID   *pCls;
   ads_point pt1, pt2;
   bool      pt1Exist = FALSE, Location = FALSE;

   acedRetNil();

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = (C_CGRID *) gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;

   if (arg->restype == RTLB)
   {
      // creo la lista dei valori da inserire nelle colonne della nuova tabella
      if ((p = acutBuildList(RTLB, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      if (gsc_scorcopy(arg, p) == NULL) { acutRelRb(p); return GS_BAD; }
      ColValues << p;
      arg = gsc_scorri(arg);

      if ((arg = arg->rbnext))
      {
         // punto inferiore della finestra
         if (gsc_rb2Pt(arg, pt1) == GS_GOOD) pt1Exist = TRUE;
         if ((arg = arg->rbnext))
         {
            if (pt1Exist && gsc_rb2Pt(arg, pt2) == GS_GOOD) Location = TRUE;
         }
      }

      if (pCls->upd_data(ColValues, (Location) ? pt1 : NULL, (Location) ? pt2 : NULL,
                        RECORD_MOD, GS_GOOD) == GS_BAD)
         return RTERROR;
   }
   else
   {
      C_SELSET SelSet;
      C_STRING AttribDst, AttribSrc;

      if (arg->restype != RTPICKS) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      SelSet.add_selset(arg->resval.rlname);
      if (!(arg = arg->rbnext) || arg->restype != RTSTR)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
      AttribDst = arg->resval.rstring;
      
      // opzionale
      if ((arg = arg->rbnext))
      {
         if (arg->restype != RTSTR)
            { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
         AttribSrc = arg->resval.rstring;
      }

      if (pCls->upd_data_fromGraph(SelSet, AttribDst, 
                                   (AttribSrc.len() > 0) ? &AttribSrc : NULL,
                                   RECORD_MOD) == GS_BAD)
         return RTERROR;
   }

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc C_CGRID::upd_data                      <external> */
/*+
  Funzione per inizializzare i dati di un attributo delle celle della griglia.
  C_RB_LIST &ColValues;	   Lista (colonna-valore)
  ads_point pt1;           Opzionale; Se <> NULL insieme a pt2 definisce la zona 
                           in cui visualizzare la griglia, Rappresenta il punto 
                           a sinistra in basso (default = NULL)
  ads_point pt2;           Rappresenta il punto a destra in alto (default = NULL)
  int       Reason;        Origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                           RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                           (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::upd_data(C_RB_LIST &ColValues, ads_point pt1, ads_point pt2, int Reason,
                      int CounterToVideo)
{
   C_RECT      Rect;
   long        Key, KeyMin, KeyMax;
   ads_point   pt, _pt2;
   C_LONG_LIST KeyList;
   C_LONG      *pKey;

   // Estensioni dela griglia
   Rect.BottomLeft.point[X] = grid.x;
   Rect.BottomLeft.point[Y] = grid.y;
   Rect.TopRight.point[X]   = grid.x + (grid.dx * grid.nx) - (grid.dx / 10);
   Rect.TopRight.point[Y]   = grid.y + (grid.dy * grid.ny) - (grid.dy / 10);

   if (pt1 && pt2) // Solo nella zona
   {
      C_RECT Location(pt1, pt2);

      // Ricavo l'area di intersezione tra i 2 rettangoli
      if (Rect.Intersect(Location) == GS_BAD) // Non c'è intersezione
         return GS_GOOD;
   }

   // Mi ricavo le coordinate del punto della prima cella da visualizzare
   if (grid.pt2key(Rect.BottomLeft.point, &KeyMin) == GS_BAD || 
       grid.key2pt(KeyMin, Rect.BottomLeft.point, FALSE) == GS_BAD)
      return GS_BAD;

   ads_2Dpoint_set(Rect.BottomLeft.point, pt);

   // imposto id superiore
   _pt2[X] = Rect.TopRight.point[X];
   _pt2[Y] = pt[Y];
   if (grid.pt2key(_pt2, &KeyMax) == GS_BAD) return GS_BAD;

   acutPrintf(GS_LFSTR);

   // Ciclo sulla Y
   do
   {
      Key = KeyMin;

      // Ciclo sulla X
      while (Key <= KeyMax)
      {
         if ((pKey = new C_LONG()) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pKey->set_id(Key);
         KeyList.add_tail(pKey);
         Key++;
      }

      pt[Y] += grid.dy;
      // imposto id inferiore e superiore
      KeyMin += grid.nx;
      KeyMax += grid.nx;
   }
   while (pt[Y] <= Rect.Top());

   return C_CLASS::upd_data(KeyList, ColValues, Reason, CounterToVideo);
}


/*********************************************************/
/*.doc C_CGRID::upd_data_fromGraph            <external> */
/*+
  Funzione per aggiornare i dati di un attributo delle celle della griglia
  partendo dalle coordinate degli oggetti di un gruppo di selezione.
  C_SELSET &SelSet;        Gruppo di selezione
  C_STRING &AttribDst;     Nome dell'attributo destinazione
  C_STRING *pAttribSrc;    Utilizzato solo per oggetti di GEOsim con collegamento a DB,
                           Nome dell'attributo da cui leggere i valori.
                           se questo parametro = NULL o se l'oggetto grafico non ha 
                           la scheda o l'attributo "pAttribSrc" verrà usato il valore della quota.
                           (Default = NULL)
  int       Reason;        Origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                           RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                           (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::upd_data_fromGraph(C_SELSET &SelSet, C_STRING &AttribDst,
                                C_STRING *pAttribSrc, int Reason)
{
   C_RB_LIST           ColValues, SrcColValues;
   presbuf             pDstValue = NULL, pSrcValue;
   double              ValueFromDB;
   int                 WhyNotUpd, ConnectionType;
   long                key, nPts = 0, Accepted = 0, ndxEnt = 0;
   C_PREPARED_CMD_LIST CmdList;
   ads_name            ent;
   C_POINT_LIST        PointList;
   C_POINT             *pPoint;
   C_CLASS             *pCls;

   if (gsc_check_op(opModEntity) == GS_BAD) return GS_BAD;

   // Il lock è a livello di classe
   if (is_updateable(0, &WhyNotUpd, GS_GOOD) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (prepare_data(CmdList) == GS_BAD) return GS_BAD;
   
   ConnectionType = CONCT_POINT + CONCT_START_END + CONCT_VERTEX;

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(460)); // "Aggiornamento griglia"
   StatusBarProgressMeter.Init(SelSet.length());

   while (SelSet.entname(ndxEnt++, ent) == GS_GOOD)
   {
      StatusBarProgressMeter.Set(ndxEnt);

      // Ritorna il puntatore alla classe dell'entità
      // e cerca la sua scheda leggendo il valore dell'attributo
      if (!pAttribSrc ||
          !(pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) ||
          pCls->query_data(ent, SrcColValues) != GS_GOOD ||
          !(pSrcValue = SrcColValues.CdrAssoc(pAttribSrc->get_name())) ||
          gsc_rb2Dbl(pSrcValue, &ValueFromDB) != GS_GOOD)
         pSrcValue = NULL;

      // Ricavo un lista di punti notevoli dell'entità
      PointList.remove_all();
      if (PointList.add_connect_point_list(ent, ConnectionType) == GS_GOOD &&
          PointList.get_count() > 0)
      {
         pPoint = (C_POINT *) PointList.get_head();

         while (pPoint)
         {
            nPts++;

            if (grid.pt2key(pPoint->point, &key) == GS_GOOD    && // dal punto ricavo il GS_ID
                query_data(key, ColValues, &CmdList) == GS_GOOD) // leggo il record
            {
               if (!pDstValue)
                  pDstValue = ColValues.CdrAssoc(AttribDst.get_name());

               if (pAttribSrc) // lettura da DB
               {
                  if (pSrcValue) // Aggiorno utilizzando il valore dell'attributo
                     gsc_RbSubst(pDstValue, ValueFromDB);
               }
               else // Aggiorno utilizzando il valore della quota del punto
                  gsc_RbSubst(pDstValue, pPoint->point[Z]);

               // aggiorno il record
               if (upd_data(key, ColValues, (C_PREPARED_CMD *) CmdList.get_head(),
                            NULL,  Reason) == GS_GOOD)
                  Accepted++;
            }
             
            pPoint = (C_POINT *) PointList.get_next();
         }
      }
   }

   StatusBarProgressMeter.End(gsc_msg(70), --ndxEnt); // "%ld oggetti grafici elaborati."
   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(308), Accepted, nPts - Accepted); // "\nEntità elaborate %ld, scartate %ld."
   acutPrintf(GS_LFSTR);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::upd_data_fromGraph            <internal> */
/*+
  Funzione per aggiornare i dati della griglia caricata in memoria
  partendo dalle coordinate degli oggetti di un gruppo di selezione.
  C_SELSET &SelSet;        Gruppo di selezione
  double *Vector;          Griglia caricata in memoria
  C_STRING *pAttribSrc;    Utilizzato solo per oggetti di GEOsim con collegamento a DB,
                           Nome dell'attributo da cui leggere il valore della quota.
                           (Default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int C_CGRID::upd_data_fromGraph(C_SELSET &SelSet, double *Vector, C_STRING *pAttribSrc)
{
   C_RB_LIST    SrcColValues;
   presbuf      pSrcValue;
   double       ValueFromDB;
   int          ConnectionType;
   long         key, nPts = 0, Accepted = 0, ndxEnt = 0;
   ads_name     ent;
   C_POINT_LIST PointList;
   C_POINT      *pPoint;
   C_CLASS      *pCls;

   ConnectionType = CONCT_POINT + CONCT_START_END + CONCT_VERTEX;

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(460)); // "Aggiornamento griglia"
   StatusBarProgressMeter.Init(SelSet.length());

   while (SelSet.entname(ndxEnt++, ent) == GS_GOOD)
   {
      StatusBarProgressMeter.Set(ndxEnt);

      // Ritorna il puntatore alla classe dell'entità
      // e cerca la sua scheda leggendo il valore dell'attributo
      if (!pAttribSrc ||
          !(pCls = GS_CURRENT_WRK_SESSION->find_class(ent)) ||
          pCls->query_data(ent, SrcColValues) != GS_GOOD ||
          !(pSrcValue = SrcColValues.CdrAssoc(pAttribSrc->get_name())) ||
          gsc_rb2Dbl(pSrcValue, &ValueFromDB) != GS_GOOD)
         pSrcValue = NULL;

      // Ricavo un lista di punti notevoli dell'entità
      PointList.remove_all();
      if (PointList.add_connect_point_list(ent, ConnectionType) == GS_GOOD &&
          PointList.get_count() > 0)
      {
         pPoint = (C_POINT *) PointList.get_head();

         while (pPoint)
         {
            nPts++;

            if (grid.pt2key(pPoint->point, &key) == GS_GOOD) // dal punto ricavo il GS_ID
            {
               if (pAttribSrc) // lettura da DB
               {
                  if (pSrcValue) // Aggiorno utilizzando il valore dell'attributo
                     Vector[key] = ValueFromDB;
               }
               else // Aggiorno utilizzando il valore della quota del punto
                  Vector[key] = pPoint->point[Z];

               Accepted++;
            }            
            pPoint = (C_POINT *) PointList.get_next();
         }
      }
   }

   StatusBarProgressMeter.End(gsc_msg(70), --ndxEnt); // "%ld oggetti grafici elaborati."
   acutPrintf(GS_LFSTR);
   acutPrintf(gsc_msg(308), Accepted, nPts - Accepted); // "\nEntità elaborate %ld, scartate %ld."
   acutPrintf(GS_LFSTR);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CGRID::upd_data_fromGraph            <external> */
/*+
  Funzione per aggiornare i dati della griglia caricata in memoria
  partendo da una lista di punti.
  C_POINT_LIST &PtList; Lista di punti
  double *Vector;       Griglia caricata in memoria

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
/* roby
int C_CGRID::upd_data_fromGraph(C_POINT_LIST &PtList, double *Vector)
{
   long    key, nPts = 0, Accepted = 0;
   C_POINT *pPoint;

   acutPrintf("\n");
   pPoint = (C_POINT *) PtList.get_head();

   while (pPoint)
   {
      nPts++;

      if (grid.pt2key(pPoint->point, &key) == GS_GOOD) // dal punto ricavo il GS_ID
      {
         // Aggiorno utilizzando il valore della quota del punto
         Vector[key] = pPoint->point[Z];
         Accepted++;
         pPoint = (C_POINT *) PtList.get_next();
      }
   }

   acutPrintf(gsc_msg(308), Accepted, nPts - Accepted); // "\nEntità elaborate %ld, scartate %ld."
   acutPrintf("\n");

   return GS_GOOD;
}
*/

/*********************************************************/
/*  FINE FUNZIONI DELLA CATEGORIA C_CGRID                */
/*********************************************************/


/*********************************************************/
/*.doc C_CLASS::get_group_list <external> */
/*+                                                                       
  Restituisce la lista dei codici delle entità della classe
  collegate all'entità membro indicata da <cls> e <gs_id>.
  es. (<group1><group2> ...)
  Parametri:
  C_PREPARED_CMD &pTempCmd; Comando per lettura relazioni temporanee
                           (vedi "prepare_reldata_where_member") (se inizializzato)
  C_PREPARED_CMD &pOldCmd;  Comando per lettura relazioni old
                            (vedi "prepare_reldata_where_member")
  int         cls;	    Codice classe membro
  long        gs_id;     Codice entità membro
  C_LONG_LIST &groupIDList; Lista dei codici dei gruppi

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::get_group_list(C_PREPARED_CMD &pTempCmd, C_PREPARED_CMD &pOldCmd,
						           int cls, long gs_id, C_LONG_LIST &groupIDList)
{        
   C_RB_LIST ColValues;
   presbuf   pRel;
   int 	    i, result, Status;
   long      groupId;
   C_CLASS   *pCls;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = get_pPrj()->find_class(cls)) == NULL) return GS_BAD;

   // se l'entità non è nuova
   if (pCls->is_NewEntity(gs_id) == GS_BAD)
      // leggo le relazioni dall'old
      if (gsc_get_reldata(pOldCmd, cls, gs_id, ColValues) == GS_GOOD)
      {
         i = 0;
         while ((pRel = ColValues.nth(i++)) != NULL)
         {
      	   if (gsc_rb2Lng(pRel->rbnext, &groupId) == GS_BAD) return GS_BAD;
            groupIDList.add_tail_long(groupId);
         }
      }

   // se esiste puntatore a istruzione compilata su temp
   if (pTempCmd.pRs.GetInterfacePtr() != NULL || pTempCmd.pCmd.GetInterfacePtr() != NULL)
      if (gsc_get_reldata(pTempCmd, cls, gs_id, ColValues) == GS_GOOD) // leggo le relazioni da temp 
      {
         // scarto quelle cancellate e aggiungo quelle nuove
	      i = 0;
	      while ((pRel = ColValues.nth(i++)) != NULL)
	      {
            // CODICE GRUPPO (1° ELEMENTO)
            if (gsc_rb2Lng(pRel->rbnext, &groupId) == GS_BAD) return GS_BAD;
            // STATUS (4° ELEMENTO)
            if (gsc_rb2Int(pRel->rbnext->rbnext->rbnext->rbnext, &Status) == GS_BAD) return GS_BAD;
            if ((Status & ERASED)) // link cancellato
               groupIDList.remove_key(groupId); // lo rimuovo dalla lista
            else // se relazione valida
            {
               // Verifico che il gruppo non sia stato cancellato roby 2016
               result = ((C_GROUP *) this)->isDeleted(groupId);
               if (result == GS_CAN) return GS_BAD;
               if (result == GS_GOOD)
   		         // link cancellato
                  groupIDList.remove_key(groupId); // lo rimuovo dalla lista
               else
                  // se il gruppo non era già stato analizzato 
        		      if (groupIDList.search_key(groupId) == NULL)
      		         groupIDList.add_tail_long(groupId);
            }
	      }
      }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLASS::get_member <external> */
/*+                                                                       
  Restituisce la lista dei membri che compongono il gruppo.
  es. ((<cls><sub><gs_id>)...)
  Parametri:
  C_PREPARED_CMD &pTempCmd; Comando per lettura relazioni temporanee
                            (vedi "prepare_reldata_where_key")
  C_PREPARED_CMD &pOldCmd;  Comando per lettura relazioni old
                            (vedi "prepare_reldata_where_key")
  long gs_id;               Codice entità gruppo
  C_2INT_LONG_LIST *member_list;  Lista dei membri

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::get_member(C_PREPARED_CMD &pTempCmd, C_PREPARED_CMD &pOldCmd,
                        long gs_id, C_2INT_LONG_LIST *member_list)
{                 
   C_RB_LIST   ColValues;
   presbuf 	   p;
   int 	   	i = 0, Status, result;
   long        Value;
   C_2INT_LONG *member;

   member_list->remove_all();

   // Verifico che il gruppo non sia stato cancellato roby 2016
   result = ((C_GROUP *) this)->isDeleted(gs_id);
   if (result == GS_GOOD) { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   if (result == GS_CAN) return GS_BAD;

   // leggo i dati ((<codice entità compl><classe figlia><codice figlia><stato>) ...)
   // Cerco nel temp
   if (gsc_get_reldata(pTempCmd, gs_id, ColValues) == GS_BAD)
      // Cerco nell'old
      if (gsc_get_reldata(pOldCmd, gs_id, ColValues) == GS_BAD) return GS_BAD;

   while ((p = ColValues.nth(i++)) != NULL)
   {
      p = p->rbnext->rbnext;                          // CLASS_ID (2° ELEMENTO)
      gsc_rb2Int(p->rbnext->rbnext, &Status);         // STATUS (4° ELEMENTO)
      if (!(Status & ERASED)) // se relazione valida
      {
	      if ((member = new C_2INT_LONG) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         if (gsc_rb2Lng(p, &Value) == GS_BAD) Value = 0;
		   member->set_key(Value);                // CLASS_ID
		   member->set_type(0);			            // SUB_CL_ID
         if (gsc_rb2Lng(p->rbnext, &Value) == GS_BAD) Value = 0;
		   member->set_id(Value);                 // ENT_ID (3° ELEMENTO)
		   member_list->add_tail(member);
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) C_CLASS::find_list_memb_simplex <external> */
/*+                                                                       
  Restituisce la lista delle classi non gruppi che 
  compongono il gruppi in esame (praticamente le foglie
  dell'albero)
  Parametri:
  C_INT_INT_LIST *class_code_list;	Lista dei codici e sotto-codici

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::find_list_memb_simplex(C_INT_INT_LIST *class_code_list)
{
   C_GROUP_LIST *p_group_list;
   C_INT_INT    *member;

   if ((p_group_list = ptr_group_list()) != NULL)
   {
      C_INT_INT *p_group;
 	   C_CLASS   *pCls;

      p_group = (C_INT_INT *) p_group_list->get_head();     
      while (p_group != NULL)
      {
         // Ritorna il puntatore alla classe cercata
         if ((pCls = GS_CURRENT_WRK_SESSION->find_class(p_group->get_key())) == NULL)
            return GS_BAD;
		   if (pCls->find_list_memb_simplex(class_code_list) == GS_BAD) return GS_BAD;
         p_group = (C_INT_INT *) p_group_list->get_next();     
      }  
   }
   else // semplice
   {
	   if ((member = new C_INT_INT) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      member->set_key(id.code);   		// CLASS_ID
	   member->set_type(id.sub_code);	// SUB_CL_ID
  	   class_code_list->add_tail(member);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) C_CLASS::CalcValidData         <external> */
/*+                                                                       
  Calcolo e validazione dei dati.
  Parametri:
  C_RB_LIST &ColValues;    Lista ((<nome attr><value>)...) Dati della scheda
  int Op;                  Operazione attuale (NONE, INSERT, MODIFY; default = MODIFY,
                           se = NONE l'operazione riguarda altre situazioni,
                           es. calcolo dei valori di default)
  C_SELSET *GlobalSelSet;  Gruppo di selezione globale per la scheda corrente
                           (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_CLASS::CalcValidData(C_RB_LIST &ColValues, int Op, C_SELSET *GlobalSelSet)
{
#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2, t3;
   _ftime(&t1);
#endif

   C_ATTRIB_LIST *p_attrib_list;

   if ((p_attrib_list = ptr_attrib_list()) == NULL) return GS_GOOD;
   if (p_attrib_list->init_ADOType(ptr_info()->getDBConnection(OLD)) == NULL)
      return GS_BAD;

   // Se c'è almeno un attributo calcolato dalla grafica
   if (p_attrib_list->is_from_graph())
      // inizializzo puntatore globale alla classe per calcolo da grafica
      GS_CALC_CLASS = this;

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
#endif

   // ricalcolo (con controllo valore dati per gli attributi calcolati)
   if (p_attrib_list->calc_all(ColValues, Op, GlobalSelSet) == GS_BAD)
      { GS_CALC_CLASS = NULL; return GS_BAD; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
   tempo4 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
#endif

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
#endif

   // controllo di tutti i valori
   if (p_attrib_list->CheckValues(ColValues) == GS_BAD)
      { GS_CALC_CLASS = NULL; return GS_BAD; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
   tempo5 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
#endif

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
#endif

   // validazione dei dati
   if (p_attrib_list->validate_all(ColValues) == GS_BAD)
      { GS_CALC_CLASS = NULL; return GS_BAD; }

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t3);
   tempo6 += (t3.time + (double)(t3.millitm)/1000) - (t2.time + (double)(t2.millitm)/1000);
#endif
   
   GS_CALC_CLASS = NULL;

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   tempo3 += (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif
    
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_class_align <external> */
/*+
  Questa funzione riallinea gli oggetti del gruppo di selezione.
  In particolare se un oggetto grafico non ha collegamento con il database 
  alfanumerico ed il flag <change_fas> è settato a GS_GOOD verrà reimpostata la
  FAS originale.
  Se l'oggetto ha collegamento con il database alfanumerico si devono distinguere
  2 casi possibili: se l'oggetto è modificabile verrà allineato il record dalla 
  grafica (qualora <AttribValuesFromVideo> sia = GS_GOOD), ricalcolati 
  gli attributi (qualora gli attributi siano calcolati con funzioni GEOlisp) e
  riaggiornata la grafica, prima con impostazione della FAS originale (se il flag 
  <change_fas> è settato a GS_GOOD) e successivamente con eventuali funzioni di 
  calcolo grafico.
  Se l'oggetto non è modificabile verrà solo aggiornata la grafica dal database
  nella stessa modalità descritta per gli oggetti modificabili.

  Parametri:
  C_SELSET *selset;          gruppo di selezione
  int change_fas;            flag, se = GS_GOOD vengono cambiate le caratteristiche grafiche
                             secondo le impostazioni della FAS di default della classe.
  int AttribValuesFromVideo; flag, se = GS_GOOD aggiorna il database leggendo gli attributi
                             visibili da video (default = GS_GOOD)
  C_SELSET *pRefused;        Gli oggetti rifiutati verranno aggiunti a questo gruppo di selezione
                             (perchè bloccati da altri utenti, perchè non soddisfano funzioni 
                             di validità ...); di default = NULL;
  int CounterToVideo;        flag, se = GS_GOOD stampa a video il numero di entità che si 
                             stanno elaborando (default = GS_BAD)
  int Reason;                flag a bit, origine della modifica sugli oggetti da allineare:
                             - GEOMETRY_MOD (modifica di geometria)
                             - FAS_MOD (modifica di solo aspetto grafico)
                             - RECORD_MOD (modifica ai dati alfanumerici, es. attrib. visib.)
                             - UNKNOWN_MOD (modifica di cui non si conosce la causa)
                             - NO_EXTERN_ACTION (non intraprende azioni esterne)
                             - ERASE_MOD (si vuole cancellare gli oggetti)
                             (default = UNKNOWN_MOD)

  Restituisce il numero di oggetti riallineati in caso di successo altrimenti 
  restituisce -1.
-*/  
/*********************************************************/
long gsc_class_align(C_SELSET &selset, int change_fas, int AttribValuesFromVideo, 
                     C_SELSET *pRefused, int CounterToVideo, int Reason)
{
	ads_name      entity;
	long          len_sel_set, qty = 0, gs_id, j;
   long          Removed = 0, Locked = 0, Belonging2Group = 0, PartialExtracted = 0;
   long          MandatoryNotValid = 0, ValidationNotValid = 0, CalculationNotValid = 0;
   C_CLASS       *pclass;
   C_EED         eed;
   int           result, Found, ToWhat, WhyNotUpd;
   C_COLOR       tmpColor;
   long          BitForFAS;
   int           last_echo, ToAlign = TRUE, OldOp, Prcnt;
   C_ATTRIB_LIST *p_attrib_list;
   C_INFO        *p_info;
   C_ID          *p_id;
   C_FAS         *p_fas;
   C_SELSET      private_selset, entSelSet, entGraphObjs;
   TCHAR         tmpHatch[MAX_LEN_HATCHNAME], tmpLayer[MAX_LEN_LAYERNAME];
   TCHAR         Msg[MAX_LEN_MSG], Handle[MAX_LEN_HANDLE]; // per notifica in file .log
   double        tmpScale, tmpRotation;
   C_STRING      UsrFunction;
   C_FAMILY_LIST family_list;
   C_LINK_SET    LinkSet;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1081)); // "Riallineamento banca dati"

#if defined(GSDEBUG) // se versione per debugging
   struct _timeb t1, t2;
   _ftime(&t1);
#endif

   // verifico l'abilitazione dell' utente
   if (gsc_check_op(opModEntity) == GS_BAD) return -1;
   
   // se è stata fatta una modifica di tipo cancellazione 
   // (bisogna avere entrambe le abilitazioni es. cancellazione di una 
   //  aggregata = modifica dell'entità)
   if (Reason & ERASE_MOD)
      if (gsc_check_op(opDelEntity) == GS_BAD) return -1;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return 0; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_BAD) return 0;

   // copia del gruppo di selezione
   if (selset.copy(private_selset) == GS_BAD) return -1;

   // Setto l'operazione corrente
   if (Reason & ERASE_MOD) OldOp = set_GS_CURRENT_OPERATION(ERASE);
   else OldOp = set_GS_CURRENT_OPERATION(MODIFY); 

   if ((len_sel_set = private_selset.length()) > 0) // ci sono oggetti nel gruppo di selezione
   {
      // se è stata fatta una modifica di tipo cancellazione 
      if (Reason & ERASE_MOD)
         // per verificare se la classe è legata ad altre
         if (gsc_getfamily(GS_CURRENT_WRK_SESSION->get_pPrj(), &family_list) == GS_BAD) return -1;

      if (gsc_set_echo(0, &last_echo) == GS_BAD) return -1;

      while (private_selset.entname(0, entity) == GS_GOOD)
   	{
         Found = FALSE;

         // leggo identificatore della classe di appartenenza
         if (eed.load(entity) == GS_BAD)
            { private_selset.subtract_ent(entity); continue; } // non è di GEOsim

         // Ritorna il puntatore alla classe cercata
         if ((pclass = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) == NULL)
            { qty = -1; break; }
         
         if ((p_fas = pclass->ptr_fas()) == NULL)
            { private_selset.subtract_ent(entity); continue; } // per sicurezza
         p_id = pclass->ptr_id();

         pclass->setModified(GS_GOOD); // classe modificata

         j = 0;

         if (CounterToVideo == GS_GOOD)
         {
            acutPrintf(gsc_msg(113), p_id->name); // "\n\nEntità classe %s:\n"
            StatusBarProgressMeter.Init(len_sel_set);
         }

         result = GS_GOOD;

         if ((p_attrib_list = pclass->ptr_attrib_list()) != NULL &&
             (p_info = pclass->ptr_info()) != NULL) // ha un database associato
         {
            _RecordsetPtr       pInsRs, pRs;
            int                 IsRsCloseable = GS_BAD;
            C_PREPARED_CMD_LIST TempOldCmdList;
            int                 toRecalcValidate, isVisibAttrib, Updateable;
            C_STRING            TempTableRef;
            C_PREPARED_CMD_LIST Belong2CmplxGrp;
            C_RB_LIST           ColValues, ins_del_param_list;
            C_DBCONNECTION      *pConn;

            // se ci sono funzioni di calcolo o di validazione o di obbligatorietà
            toRecalcValidate = (p_attrib_list->is_calculated() == GS_GOOD ||
                                p_attrib_list->is_validated() == GS_GOOD ||
                                p_attrib_list->is_mandatory() == GS_GOOD) ? GS_GOOD : GS_BAD;
            isVisibAttrib    = p_attrib_list->is_visible();

            // se è stata fatta una modifica di tipo cancellazione 
            if (Reason & ERASE_MOD)
            {  // inizializzo il resbuf per segnare le entità cancellate
               if (family_list.get_count() > 0)
                  // inizializzo compilazioni per verifica appartenenza a gruppi
                  if (gsc_prepare_for_linked_ent(GS_CURRENT_WRK_SESSION->get_pPrj(), p_id->code, family_list, Belong2CmplxGrp) == GS_BAD)
                     { qty = -1; break; }
            }
            
            // ricavo la tabella temporanea
            if (pclass->getTempTableRef(TempTableRef) == GS_BAD) { qty = -1; break; }

            // Preparo i comandi di lettura dei dati della classe dal temp/old
            if (pclass->prepare_data(TempOldCmdList) == GS_BAD) break;

            // Ricavo le connessioni ai database
            if ((pConn = p_info->getDBConnection(TEMP)) == NULL) break;

            // preparo istruzione per l'inserimento di record nella tabella temp
            if (pConn->InitInsRow(TempTableRef.get_name(), pInsRs) == GS_BAD)
               { qty = -1; break; }

            do
            { 
               Found = FALSE;
               qty++;

               do
               {
                  entSelSet.clear();

                  // leggo valore chiave e il gruppo di selezione
                  if (pclass->get_Key_SelSet(entity, &gs_id, entSelSet) == GS_BAD)
                  {
                     // Notifico in file log
                     gsc_enthand(entity, Handle);
                     swprintf(Msg, MAX_LEN_MSG, _T("Invalid key on entity: handle <%s>, prj %d, class %d, subclass %d."),
                              Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                     gsc_write_log(Msg);

                     if (pRefused) pRefused->add(entity);
                     GEOsimAppl::SAVE_SS.subtract_ent(entity);
                     private_selset.subtract_ent(entity);
                     result = GS_BAD;
                     break;
                  }

                  if (entSelSet.copy(entGraphObjs) == GS_BAD)
                     { result = GS_BAD; break; }
                  if (entGraphObjs.intersectType(GRAPHICAL) == GS_BAD)
                     { result = GS_BAD; break; } 
             
                  // se è stata fatta una modifica che potrebbe riguardare la geometria 
                  if (Reason & GEOMETRY_MOD || Reason & UNKNOWN_MOD)
                     // controllo la compatibilità grafica
                     if (pclass->check_graph_compatib(entGraphObjs) == GS_BAD)
                        { result = GS_BAD; GS_ERR_COD = eGSOpNotAble; break; }

                  // se si vuole impostare la FAS di default
                  if (change_fas == GS_GOOD)
                  { // imposto la FAS di default
                     if (gsc_SetBitForChangeFas(p_id->category, p_id->type, &BitForFAS) == GS_BAD)
                        { result = GS_BAD; break; }
                     // La rotazione e l'elevazione non devono essere variate
                     if (BitForFAS & GSRotationSetting) BitForFAS -= GSRotationSetting;
                     if (BitForFAS & GSElevationSetting) BitForFAS -= GSElevationSetting;

                     if (gsc_modifyEntToFas(entGraphObjs, p_fas, BitForFAS) == GS_BAD)
                        { result = GS_BAD; break; }
                  }

                  // lettura record dalla tabella
                  if (gsc_get_data(TempOldCmdList, gs_id, pRs, &IsRsCloseable, &ToWhat) == GS_BAD)
                  {
                     // Notifico in file log
                     gsc_enthand(entity, Handle);
                     swprintf(Msg, MAX_LEN_MSG, _T("Invalid data on entity - handle <%s>, prj %d, class %d, subclass %d."),
                              Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                     gsc_write_log(Msg);

                     result = GS_BAD; break;
                  }
                  if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
                     { result = GS_BAD; break; }

                  // se è aggiornabile viene bloccata
                  Updateable = pclass->is_updateableSS(gs_id, entSelSet, &WhyNotUpd);
                  if (Updateable != GS_GOOD && WhyNotUpd == eGSPartialEntExtract)
                  {  // Provo ad estrarre totalmente le istanze grafiche dell'entità
                     if (pclass->entExtract(gs_id) == GS_GOOD &&
                         pclass->get_SelSet(gs_id, entSelSet) == GS_GOOD)
                     {
                        Updateable = pclass->is_updateableSS(gs_id, entSelSet, &WhyNotUpd);
                        if (Updateable == GS_GOOD)
                        {  // Mi ricavo i nuovi gruppi di oggetti
                           if (entSelSet.copy(entGraphObjs) == GS_BAD)
                              { result = GS_BAD; break; }
                           if (entGraphObjs.intersectType(GRAPHICAL) == GS_BAD)
                              { result = GS_BAD; break; } 
                        }
                     }
                  }

                  // se è aggiornabile viene bloccata
                  if (Updateable == GS_GOOD)
                  {
                     // se è stata fatta una modifica di tipo cancellazione 
                     if (Reason & ERASE_MOD)
                     {
                        // eventuale chiamata ad una funzione utente esterna (prima della modifica o in
                        // completa sostituzione della funzione di GEOsim)
                        if (gsc_getAction(p_id->usr_cmds, STR_SUBST, STR_ERASE, UsrFunction) == GS_GOOD &&
                            UsrFunction.len() > 0)
                        {
                           if (gsc_doActionOnErase(UsrFunction.get_name(), entity, Reason) == GS_BAD)
                           {
                              // Notifico in file log
                              gsc_enthand(entity, Handle);
                              swprintf(Msg, MAX_LEN_MSG, _T("User function <%s> failed; handle <%s>, prj %d, class %d, subclass %d."),
                                       UsrFunction.get_name(), Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);

                              result = GS_BAD; break;
                           }
                           // sottraggo da private_selset l'entità entity
                           if (private_selset.subtract_ent(entity) == GS_BAD)
                              { result = GS_BAD; break; }
                           ToAlign = FALSE;  // questa cancellazione NON necessita di allineamento
                        }
                        else
                        {
                           if (gsc_getAction(p_id->usr_cmds, STR_BEFORE, STR_ERASE, UsrFunction) == GS_GOOD &&
                               UsrFunction.len() > 0)
                              if (gsc_doActionOnErase(UsrFunction.get_name(), entity, Reason) == GS_BAD)
                              {
                                 // Notifico in file log
                                 gsc_enthand(entity, Handle);
                                 swprintf(Msg, MAX_LEN_MSG, _T("User function <%s> failed; handle <%s>, prj %d, class %d, subclass %d."),
                                          UsrFunction.get_name(), Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);

                                 result = GS_BAD; break;
                              }

                           // devo diminuire di 1 il fattore di aggregazione degli oggetti collegati
                           eed.num_el--;
                           // Se blocco attributi
                           if (gsc_is_DABlock(entity) == GS_GOOD)
                           {
                              // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
                              if (eed.save_aggr(entSelSet, (int) entSelSet.length() - 1) == GS_BAD)
                                 { result = GS_BAD; break; }

                              if (gsc_EraseEnt(entity) != GS_GOOD)
                                 { result = GS_BAD; break; }

                              if (entSelSet.subtract_ent(entity) == GS_BAD)
                                 { result = GS_BAD; break; }

                              // sottraggo da private_selset l'entità entity
                              if (private_selset.subtract_ent(entity) == GS_BAD)
                                 { result = GS_BAD; break; }

                              ToAlign = TRUE;  // questa cancellazione necessita di allineamento
                           }
                           else
                           {  // Se è una principale devo scorrere tutti i Link e vedere se ci sono altre 
                              // principali, se non ce ne sono altre allora devo cancellare in grafica
                              // tutte le schede collegate.
                              long SurfaceGraphObjs = 1;

                              if (pclass->get_type() == TYPE_SURFACE)
                              {  // Verifico se ci sono bordi (scartando i riempimenti)
                                 C_SELSET dummy;

                                 dummy.add_selset(entGraphObjs);
                                 dummy.subtract_ent(entity);
                                 dummy.is_presentGraphicalObject(&SurfaceGraphObjs);
                              }

                              if (entGraphObjs.length() == 1 || SurfaceGraphObjs == 0)
                              {  // cancello le entita collegate perchè cancellando la princ.
                                 // devo cancellare tutte le schede collegate
                                 int isMember = GS_BAD;

                                 // verifico se la classe che si sta cancellando è legata ad altre
                                 if (Belong2CmplxGrp.get_count() > 0)
                                    // Controllo se l'entità fa parte di un gruppo
                                    if (gsc_is_member_of(pclass, gs_id, family_list, &isMember,
                                                         &Belong2CmplxGrp) == GS_BAD)
                                       { result = GS_BAD; break; }

                                 if (isMember == GS_BAD)
                                 {
                                    if (entSelSet.Erase() != GS_GOOD)
                                       { result = GS_BAD; GS_ERR_COD = eGSAdsCommandErr; break; }

                                    // inserisco l'entità in GS_DELETE
                                    if (pclass->Ins_Ent_Into_GsDelete(gs_id, &ins_del_param_list) == GS_BAD)
                                       { result = GS_BAD; break; }
                                    // Notifico in file log
                                    swprintf(Msg, MAX_LEN_MSG, _T("Erased entity: key %ld, prj %d, class %d, subclass %d."),
                                             gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                                    gsc_write_log(Msg);
                                 }
                                 else
                                 {
                                    if (pRefused) pRefused->add_selset(entSelSet);
                                    Belonging2Group++;
                                    // per non inserirla nel gruppo di selezione del salvataggio
                                    Updateable = GS_BAD;
                                 }

                                 // sottraggo da private_selset il gruppo entSelSet
                                 if (private_selset.subtract(entSelSet) == GS_BAD)
                                    { result = GS_BAD; break; }
                                 ToAlign = FALSE;  // questa cancellazione NON necessita di allineamento
                              }
                              else
                              { 
                                 // aggiorno n. aggregate su tutti gli oggetti
                                 if (eed.save_aggr(entSelSet, (int) entSelSet.length() - 1) == GS_BAD)
                                    { result = GS_BAD; break; }

                                 if (gsc_EraseEnt(entity) != GS_GOOD)
                                    { result = GS_BAD; break; }

                                 // sottraggo da private_selset l'entità entity
                                 if (private_selset.subtract_ent(entity) == GS_BAD)
                                    { result = GS_BAD; break; }

                                 if (entSelSet.subtract_ent(entity) == GS_BAD)
                                    { result = GS_BAD; break; }

                                 ToAlign = TRUE;  // questa cancellazione necessita di allineamento
                              }
                           }

                           if (!ToAlign)
                              // eventuale chiamata ad una funzione utente esterna (dopo la cancellazione)
                              if (gsc_getAction(p_id->usr_cmds, STR_AFTER, STR_ERASE, UsrFunction) == GS_GOOD &&
                                  UsrFunction.len() > 0)
                                 if (gsc_doActionOnErase(UsrFunction.get_name(), entity, Reason) == GS_BAD)
                                 {
                                    // Notifico in file log
                                    gsc_enthand(entity, Handle);
                                    swprintf(Msg, MAX_LEN_MSG, _T("User function <%s> failed; handle <%s>, prj %d, class %d, subclass %d."),
                                             UsrFunction.get_name(), Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);

                                    result = GS_BAD; break;
                                 }
                        }
                     }

                     if (ToAlign) // se bisogna allineare
                     {
                        // se è superficie
                        if (p_id->type == TYPE_SURFACE)
                        {
                           int do_it = FALSE;
                  
                           if (change_fas == GS_GOOD)
                           {
                              do_it = TRUE; // imposto la FAS di default
                              tmpScale    = p_fas->hatch_scale;
                              tmpRotation = p_fas->hatch_rotation;
                              gsc_strcpy(tmpHatch, p_fas->hatch, MAX_LEN_HATCHNAME);
                              gsc_strcpy(tmpLayer, p_fas->hatch_layer, MAX_LEN_LAYERNAME);
                              tmpColor = p_fas->hatch_color;
                           }
                           else  
                              // se è stata fatta una modifica che potrebbe riguardare la geometria 
                              if (Reason & GEOMETRY_MOD || Reason & UNKNOWN_MOD || Reason & ERASE_MOD)
                                 // controllo se c'è almeno un riempimento
                                 if (gsc_getInfoHatchSS(entSelSet, tmpHatch, 
                                                        &tmpScale, &tmpRotation,
                                                        tmpLayer, &tmpColor) == GS_GOOD)
                                 {
                                    do_it = TRUE;
                                    tmpRotation = gsc_rad2grd(tmpRotation);
                                 }

                           if (do_it)
                           {
                              // Aggiorno i riempimenti modificando ls
                              if (gsc_UpdToDefHatch(entSelSet, tmpHatch, tmpScale, tmpRotation,
                                                    &tmpColor, tmpLayer) == GS_BAD)
                                 { result = GS_BAD; break; }
                           }
                        }
               
                        // Se esistono attributi a video o si tratta di testo
                        if (AttribValuesFromVideo == GS_GOOD && isVisibAttrib == GS_GOOD &&
									 (p_id->type == TYPE_TEXT || entSelSet.length() - entGraphObjs.length() > 0))
                        {  // Aggiorno da video con validazione e ricalcolo dati
                           if (pclass->updfromDA(ColValues, entity, &entSelSet, &toRecalcValidate) == GS_BAD)
                           {
                              // Notifico in file log
                              gsc_enthand(entity, Handle);
                              swprintf(Msg, MAX_LEN_MSG, _T("Calculation or validation falied on entity - handle <%s>, prj %d, class %d, subclass %d."),
                                       Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                              gsc_write_log(Msg);
                              
                              result = GS_BAD; break;
                           }
                        }
                        else // validazione e ricalcolo dati
                           if (toRecalcValidate == GS_GOOD && 
                               pclass->CalcValidData(ColValues, MODIFY, &entSelSet) == GS_BAD)
                           {
                              // Notifico in file log
                              gsc_enthand(entity, Handle);
                              swprintf(Msg, MAX_LEN_MSG, _T("Calculation or validation falied on entity - handle <%s>, prj %d, class %d, subclass %d."),
                                       Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                              gsc_write_log(Msg);
                              
                              switch (GS_ERR_COD)
                              {
                                 case eGSObbligFound:  MandatoryNotValid++;   break;
                                 case eGSInvalidValid: ValidationNotValid++;  break;
                                 case eGSInvalidCalc:  CalculationNotValid++; break;
                              }

                              result = GS_BAD; break;
                           }

                        // eventuale chiamata ad una funzione utente esterna (prima della modifica o in
                        // completa sostituzione della funzione di GEOsim)
                        if (gsc_getAction(p_id->usr_cmds, STR_SUBST, STR_UPDATE, UsrFunction) == GS_GOOD &&
                            UsrFunction.len() > 0)
                        {
                           if (gsc_doActionOnUpdate(UsrFunction.get_name(), p_id->code, p_id->sub_code,
                                                    gs_id, &ColValues, Reason) == GS_BAD)
                           {
                              // Notifico in file log
                              gsc_enthand(entity, Handle);
                              swprintf(Msg, MAX_LEN_MSG, _T("User function <%s> failed; handle <%s>, prj %d, class %d, subclass %d."),
                                       UsrFunction.get_name(), Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);

                              result = GS_BAD; break;
                           }
                        }
                        else
                        {
                           if (gsc_getAction(p_id->usr_cmds, STR_BEFORE, STR_UPDATE, UsrFunction) == GS_GOOD &&
                               UsrFunction.len() > 0)
                           {
                              if (gsc_doActionOnUpdate(UsrFunction.get_name(), p_id->code, p_id->sub_code,
                                                       gs_id, &ColValues, Reason) == GS_BAD)
                              {
                                 // Notifico in file log
                                 gsc_enthand(entity, Handle);
                                 swprintf(Msg, MAX_LEN_MSG, _T("User function <%s> failed; handle <%s>, prj %d, class %d, subclass %d."),
                                          UsrFunction.get_name(), Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);

                                 result = GS_BAD; break;
                              }
                     
                              // validazione e ricalcolo dati ("gsc_doActionOnUpdate" potrebbe cambiare qualcosa)
                              if (AttribValuesFromVideo == GS_GOOD && isVisibAttrib == GS_GOOD) 
                              {  // Aggiorno da video con validazione e ricalcolo dati
                                 if (pclass->updfromDA(ColValues, entity, &entSelSet, &toRecalcValidate) == GS_BAD)
                                    { result = GS_BAD; break; }
                              }
                              else // validazione e ricalcolo dati
                                 if (toRecalcValidate == GS_GOOD && 
                                     pclass->CalcValidData(ColValues, MODIFY, &entSelSet) == GS_BAD)
                                 {
                                    // Notifico in file log
                                    gsc_enthand(entity, Handle);
                                    swprintf(Msg, MAX_LEN_MSG, _T("Calculation or validation falied on entity - handle <%s>, prj %d, class %d, subclass %d."),
                                             Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                                    gsc_write_log(Msg);

                                    switch (GS_ERR_COD)
                                    {
                                       case eGSObbligFound:  MandatoryNotValid++;   break;
                                       case eGSInvalidValid: ValidationNotValid++;  break;
                                       case eGSInvalidCalc:  CalculationNotValid++; break;
                                    }

                                    result = GS_BAD; break;
                                 }
                           }

                           if (pclass->updtoDA(entSelSet, ColValues) == GS_BAD)
                              { result = GS_BAD; break; }

                           // se esistono funzioni di calcolo grafico vengono applicate ora
                           if (pclass->graph_calc(ColValues, &entSelSet) == GS_BAD)
                              { result = GS_BAD; break; }

                           if (ToWhat == OLD)
                           {
                              // aggiungo la scheda nel TEMP
                              if (gsc_DBInsRow(pInsRs, ColValues, ONETEST, GS_BAD) == GS_BAD)
                                 // se l'errore non era dovuto al fatto che il record esisteva già
                                 if (GS_ERR_COD != eGSIntConstr)
                                 {
                                    gsc_enthand(entity, Handle);
                                    swprintf(Msg, MAX_LEN_MSG, _T("Record append failed on entity - handle <%s>, prj %d, class %d, subclass %d."),
                                             Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                                    gsc_write_log(Msg);

                                    result = GS_BAD;
                                    break;
                                 }                 
                           }
                           else
                           {
                              // aggiorno il record
                              if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
                              {
                                 gsc_enthand(entity, Handle);
                                 swprintf(Msg, MAX_LEN_MSG, _T("Record update failed on entity - handle <%s>, prj %d, class %d, subclass %d."),
                                          Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                                 gsc_write_log(Msg);

                                 result = GS_BAD; break;
                              }
                           }

                           // Notifico in file log
                           swprintf(Msg, MAX_LEN_MSG, _T("Updated entity: key %ld, prj %d, class %d, subclass %d."),
                                    gs_id, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                           gsc_write_log(Msg);
                        }

                        // eventuale chiamata ad una funzione utente esterna (dopo la modifica)
                        if (gsc_getAction(p_id->usr_cmds, STR_AFTER, STR_UPDATE, UsrFunction) == GS_GOOD &&
                            UsrFunction.len() > 0)
                           if (gsc_doActionOnUpdate(UsrFunction.get_name(), p_id->code, p_id->sub_code,
                                                    gs_id, &ColValues, Reason) == GS_BAD)
                           {
                              // Notifico in file log
                              gsc_enthand(entity, Handle);
                              swprintf(Msg, MAX_LEN_MSG, _T("User function <%s> failed; handle <%s>, prj %d, class %d, subclass %d."),
                                       UsrFunction.get_name(), Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);

                              result = GS_BAD; break;
                           }

                        // Solo se non si tratta di cancellazione
                        // Infatti se si cancellasse un blocco DA e relativa primaria
                        // e venisse elaborato prima il blocco DA non si potrebbe
                        // elaborare la primaria
                        if (!(Reason & ERASE_MOD))
                           // sottraggo da private_selset il gruppo entSelSet
                           if (private_selset.subtract(entSelSet) == GS_BAD)
                              { result = GS_BAD; break; }
                     }

                     // Updateable viene impostato a GS_BAD se si tratta di
                     // - cancellazione entità semplice collegata a gruppo
                     if (Updateable == GS_GOOD)
                        // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
                        if (gsc_addSS2savess(entSelSet) == GS_BAD) { result = GS_BAD; break; }
                  }
                  else // se non è aggiornabile
                  {
                     switch (WhyNotUpd)
                     {
                        case eGSObjectIsLockedByAnotherUser:
                           Locked++;
                           break;
                        case eGSPartialEntExtract:
                           PartialExtracted++;
                           break;
                     }

                     if (pclass->updtoDA(entSelSet, ColValues) == GS_BAD)
                        { result = GS_BAD; break; }

                     // se esistono funzioni di calcolo grafico vengono applicate ora
                     if (pclass->graph_calc(ColValues, &entSelSet) == GS_BAD)
                        { result = GS_BAD; break; }

                     // sottraggo da private_selset il gruppo entSelSet
                     if (private_selset.subtract(entSelSet) == GS_BAD) { result = GS_BAD; break; }
   
                     if (pRefused) pRefused->add_selset(entSelSet);
                  }
               }
               while (0);

               if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

               if (result == GS_BAD)
               {
                  Removed++;
                  if (pRefused) pRefused->add_selset(entSelSet);
                  if (GEOsimAppl::SAVE_SS.subtract(entSelSet) == GS_BAD) { qty = -1; break; }
                  // sottraggo da private_selset il gruppo entSelSet
                  if (private_selset.subtract(entSelSet) == GS_BAD)
                     { qty = -1; break; }
                  result = GS_GOOD;
               }

               while (private_selset.entname(j, entity) == GS_GOOD && !Found)
               {
                  // leggo identificatore della classe di appartenenza
                  if (eed.load(entity) == GS_GOOD &&
                      eed.cls == p_id->code && eed.sub == p_id->sub_code)
                     Found = TRUE;
                  else j++;
               }

               if (CounterToVideo == GS_GOOD)
               {
                  Prcnt = (int) ((len_sel_set - private_selset.length()) * 100 / len_sel_set);
                  StatusBarProgressMeter.Set_Perc(Prcnt);
               }
            }
            while (Found);
         }
         else // non ha un database associato
         {
            do
            { 
               qty++;

               do
               {
                  entSelSet.clear();
                  if (entSelSet.add(entity) == GS_BAD) { result = GS_BAD; break; }

                  if (change_fas == GS_GOOD)
                  { // imposto la FAS di default
                     if (gsc_SetBitForChangeFas(p_id->category, p_id->type, &BitForFAS) == GS_BAD)
                        {  result = GS_BAD; break; }
                     if (gsc_modifyEntToFas(entSelSet, p_fas, BitForFAS) == GS_BAD)
                        { result = GS_BAD; break; }
                  }

                  // se è aggiornabile viene bloccata
                  if (pclass->is_updateable(entity, &WhyNotUpd) == GS_GOOD)
                  {
                     if (gsc_enthand(entity, Handle) == GS_BAD) { result = GS_BAD; break; }

                     // se è stata fatta una modifica di tipo cancellazione 
                     if (Reason & ERASE_MOD)
                     {
                        if (gsc_EraseEnt(entity) != GS_GOOD) { result = GS_BAD; break; }
                        swprintf(Msg, MAX_LEN_MSG, _T("Erased entity: handle <%s>, prj %d, class %d, subclass %d."),
                                 Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                        gsc_write_log(Msg);
                     }
                     else
                     {
                        // Notifico in file log
                        swprintf(Msg, MAX_LEN_MSG, _T("Updated entity: handle <%s>, prj %d, class %d, subclass %d."),
                                 Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code, p_id->sub_code);
                        gsc_write_log(Msg);
                     }

                     // Aggiungo in GEOsimAppl::SAVE_SS per salvataggio
                     if (gsc_addSS2savess(entSelSet) == GS_BAD) { result = GS_BAD; break; }
                  }
                  else // se non è aggiornabile
                     if (WhyNotUpd == eGSObjectIsLockedByAnotherUser) Locked++;

                  // sottraggo da private_selset il gruppo entSelSet
                  if (private_selset.subtract(entSelSet) == GS_BAD) { result = GS_BAD; break; }
               }
               while (0);

               if (result == GS_BAD)
               {
                  Removed++;
                  if (pRefused) pRefused->add_selset(entSelSet);
                  if (GEOsimAppl::SAVE_SS.subtract(entSelSet) == GS_BAD) { qty = -1; break; }
                  // sottraggo da private_selset il gruppo entSelSet
                  if (private_selset.subtract(entSelSet) == GS_BAD)
                     { qty = -1; break; }
                  result = GS_GOOD;
               }

               Found = FALSE;
               while (private_selset.entname(j, entity) == GS_GOOD && !Found)
               {
                  // leggo identificatore della classe di appartenenza
                  if (eed.load(entity) == GS_GOOD &&
                      eed.cls == p_id->code && eed.sub == p_id->sub_code)
                     Found = TRUE;
                  else j++;
               }
 
               if (CounterToVideo == GS_GOOD)
               {
                  Prcnt = (int) ((len_sel_set - private_selset.length()) * 100 / len_sel_set);
                  StatusBarProgressMeter.Set_Perc(Prcnt);
               }
            }
            while (Found);
         }  
         StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

         if (qty == -1) break;
      }
      
      gsc_set_echo(last_echo);
      if (CounterToVideo == GS_GOOD)
      {
         if (qty >= 0) acutPrintf(gsc_msg(491), qty); // "\n%ld entità GEOsim elaborate.\n"
         if (Removed > 0) acutPrintf(gsc_msg(118), Removed); // "\n%ld entità rimosse dal salvataggio."
         if (Locked > 0) acutPrintf(gsc_msg(770), Locked);   // "\n%ld entità GEOsim bloccata/e da un' altro utente."
         if (PartialExtracted > 0) acutPrintf(gsc_msg(35), PartialExtracted); // "\n%ld entità GEOsim estratte parzialmente."         
         if (Belonging2Group > 0) acutPrintf(gsc_msg(33), Belonging2Group);   // "\n%ld entità GEOsim appartenenti a gruppo."
         if (MandatoryNotValid > 0) acutPrintf(gsc_msg(430), MandatoryNotValid);    // "\n%ld entità GEOsim con obbligatorietà attributi non soddisfatta."
         if (ValidationNotValid > 0) acutPrintf(gsc_msg(431), ValidationNotValid);  // "\n%ld entità GEOsim con validità attributi non soddisfatta."
         if (CalculationNotValid > 0) acutPrintf(gsc_msg(432), CalculationNotValid); // "\n%ld entità GEOsim con calcolo attributi non valido."
      }

      // Se ci oggetti con problemi
      if (Removed > 0 || Locked > 0 || PartialExtracted > 0 || Belonging2Group > 0)
         gsc_ddalert(gsc_msg(84)); // "Alcune entità di GEOsim non saranno salvate"
   }

   // Setto l'operazione corrente
   set_GS_CURRENT_OPERATION(OldOp);

#if defined(GSDEBUG) // se versione per debugging
   _ftime(&t2);
   tempo2 += (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
#endif

   return qty;
}


/******************************************************************/
/*.doc gsc_ActionOnErase                                          */
/*+                                                            
  Funzione restituisce se ci sono delle azioni da intraprendere
  sull'evento cancellazione dell'entità (prima, dopo o 
  in sostutizione della funzione di GEOsim).
  La sintassi per descrivere la chiamata ad un comando utente è:
  <quando>.<in quale operazione>.<nome comando>[<separatore>]
  <quando> ::= BEFORE|AFTER|SUBST
  <in quale operazione> ::= INSERT|UPDATE|ERASE
  <separatore> ::= ;
  Parametri:
  TCHAR *usr_cmds;         stringa in cui sono memorizzate le funzioni da richiamare
  
  Restituisce GS_GOOD se esiste una o più azioni altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int gsc_areActionsOnErase(TCHAR *usr_cmds)
{
   C_STRING StrToSearch;

   if (!usr_cmds || wcslen(usr_cmds) == 0) return GS_BAD;

   StrToSearch = _T('.');
   StrToSearch += STR_ERASE;
   StrToSearch += _T('.');

   return (gsc_strstr(usr_cmds, StrToSearch.get_name()) == NULL) ? GS_BAD : GS_GOOD;
}


/******************************************************************/
/*.doc gsc_getAction                                              */
/*+                                                            
  Funzione restituisce il nome di una eventuale funzione da richiamare
  prima, dopo o in sostutizione della funzione di GEOsim. 
  La sintassi per descrivere la chiamata ad un comando utente è:
  <quando>.<in quale operazione>.<nome comando>[<separatore>]
  <quando> ::= BEFORE|AFTER|SUBST
  <in quale operazione> ::= INSERT|UPDATE|ERASE
  <separatore> ::= ;
  Parametri:
  TCHAR *usr_cmds;        stringa in cui sono memorizzate le funzioni da richiamare
  const TCHAR *when;      momento in cui richiamare la funzione utente
                          (BEFORE|AFTER|SUBST)
  const TCHAR *OrigCmd;   nome della funzione  di GEOsim in cui deve avvenire la chiamata
                          (INSERT|UPDATE|ERASE)
  C_STRING &result;       Nome della funzione utente
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int gsc_getAction(TCHAR *usr_cmds, const TCHAR *when, const TCHAR *OrigCmd,
                  C_STRING &result)
{
   TCHAR    *pStart, *pEnd;
   C_STRING StrToSearch;

   if (!usr_cmds || wcslen(usr_cmds) == 0) return GS_BAD;
   if (!when || !OrigCmd) return GS_BAD;

   StrToSearch = when;
   StrToSearch += _T(".");
   StrToSearch += OrigCmd;
   StrToSearch += _T(".");

   if ((pStart = gsc_strstr(usr_cmds, StrToSearch.get_name())) == NULL) return GS_BAD;
   pEnd = pStart = pStart + StrToSearch.len();
   while (*pEnd != _T('\0') && *pEnd != _T(';') && *pEnd != _T(' ')) pEnd++; // cerco separatore
   if (result.set_name(pStart, 0, (int) (pEnd - pStart - 1)) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/******************************************************************/
/*.doc gsc_doActionOnInsert                                       */
/*+                                                            
  Funzione che richiama un comando esterna in forma di una funzione LISP.
  La funzione deve restituire T se tutto OK, qualsiasi altro valore (compreso NULL)
  in caso di errore.
  Alla funzione vengono passati i seguenti parametri: oggetto grafico da inserire 
  o già inserito (a seconda del momento in cui avviene questa chiamata), una lista
  di valori che individua il record da inserire (o inserito), un flag che indica se
  si vuole inserire ( o se è stato inserito) un blocco di attributi visibili, il
  nome del comando che ha scatenato questa chiamata.
  Nel caso in cui l'operazione sia effettuata per una classe gruppo
  alla funzione vengono passati i seguenti parametri: gruppo di selezione degli
  oggetti grafici che compongono l'entità da inserire o già inserita (a seconda del
  momento in cui avviene questa chiamata), una lista di valori che individua il 
  record da inserire (o inserito), il nome del comando che ha scatenato questa chiamata.
  Parametri:
  const TCHAR *UsrFunction;   nome funzione da richiamare
  ads_name    ent;            oggetto grafico Geosim
  presbuf     info;           lista di resbuf (<cls>[<sub>])
  C_RB_LIST   *pColValues;    riga associata all'oggetto grafico
  short       visib_block;    se si vuole inserire un blocco con attr. visib.
  int         Reason;         origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                              RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                              (default = UNKNOWN_MOD)

  oppure:
  const TCHAR *UsrFunction;   nome funzione da richiamare
  ads_name    SelSet;         oggetti grafici che compongono l'entità Geosim
  presbuf     info;           lista di resbuf (<cls>[<sub>])
  C_RB_LIST   *pColValues;    riga associata all'oggetto grafico
  int         Reason;         origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                              RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                              (default = UNKNOWN_MOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int gsc_doActionOnInsert(const TCHAR *UsrFunction, ads_name ent, presbuf info,
                         C_RB_LIST *pColValues, short visib_block, int Reason)
{
   C_RB_LIST cmd;
   presbuf   ret;

   // se è indicato che non si devono chiamare funzioni esterne
   if (Reason & NO_EXTERN_ACTION) return GS_GOOD;

   if ((cmd << acutBuildList(RTSTR, UsrFunction, RTENAME, ent, 0)) == NULL)
      return GS_BAD;
   if ((cmd += gsc_rblistcopy(info)) == NULL) return GS_BAD;

   if (pColValues)
   {
      if ((cmd += gsc_rblistcopy(pColValues->get_head())) == NULL) return GS_BAD;
   }
   else
      if ((cmd += acutBuildList(RTNIL, 0)) == NULL) return GS_BAD;

   if ((cmd += acutBuildList(RTSHORT, visib_block, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if ((cmd += acutBuildList(RTSHORT, Reason, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (acedInvoke(cmd.get_head(), &ret) != RTNORM)
      { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (!ret) { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (ret->restype != RTT) { acutRelRb(ret); GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   acutRelRb(ret);

   return GS_GOOD;
}
// per gruppi
int gsc_doActionOnInsert(const TCHAR *UsrFunction, ads_name SelSet, presbuf info,
                         C_RB_LIST *pColValues, int Reason)
{
   C_RB_LIST cmd;
   presbuf   ret;

   // se è indicato che non si devono chiamare funzioni esterne
   if (Reason & NO_EXTERN_ACTION) return GS_GOOD;

   if ((cmd << acutBuildList(RTSTR, UsrFunction, RTPICKS, SelSet, 0)) == NULL)
      return GS_BAD;
   if ((cmd += gsc_rblistcopy(info)) == NULL) return GS_BAD;

   if (pColValues)
   {
      if ((cmd += gsc_rblistcopy(pColValues->get_head())) == NULL) return GS_BAD;
   }
   else
      if ((cmd += acutBuildList(RTNIL, 0)) == NULL) return GS_BAD;

   if ((cmd += acutBuildList(RTSHORT, Reason, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (acedInvoke(cmd.get_head(), &ret) != RTNORM)
      { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (!ret) { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (ret->restype != RTT) { acutRelRb(ret); GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   acutRelRb(ret);

   return GS_GOOD;
}


/******************************************************************/
/*.doc gsc_doActionOnUpdate                                       */
/*+                                                            
  Funzione che richiama un comando esterna in forma di una funzione LISP.
  La funzione deve restituire T se tutto OK qualsiasi altro valore (compreso NULL)
  in caso di errore.
  Alla funzione vengono passati i seguenti parametri: codice classe, codice sottoclasse,
  codice chiave dell'entità GEOsim da modificare o già modificata (a seconda 
  del momento in cui avviene questa chiamata), una lista di valori che 
  individua il record relativo, il nome del comando che ha scatenato questa chiamata.
  Parametri:
  const TCHAR *UsrFunction;   nome funzione da richiamare
  ads_name    Entity;         oggetto grafico GEOsim
  int         Reason;         origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                              RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                              (default = UNKNOWN_MOD)

  oppure:
  const TCHAR *UsrFunction;   nome funzione da richiamare
  int        cls;             codice classe
  int        sub;             codice sottoclasse
  long       gs_id;           codice entità GEOsim
  C_RB_LIST  *pColValues;     riga associata all'oggetto grafico
  int        Reason;          origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                              RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                              (default = UNKNOWN_MOD)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int gsc_doActionOnUpdate(const TCHAR *UsrFunction, ads_name Entity, int Reason)
{  // Usata per classi senza collegamento a DB
   C_RB_LIST cmd;
   presbuf   ret;

   // se è indicato che non si devono chiamare funzioni esterne
   if (Reason & NO_EXTERN_ACTION) return GS_GOOD;

   if ((cmd << acutBuildList(RTSTR, UsrFunction, RTENAME, Entity, 0)) == NULL)
      return GS_BAD;
   if ((cmd += acutBuildList(RTSHORT, Reason, 0)) == NULL)
      return GS_BAD;
   if (acedInvoke(cmd.get_head(), &ret) != RTNORM)
      { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (!ret) { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (ret->restype != RTT) { acutRelRb(ret); GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   acutRelRb(ret);

   return GS_GOOD;
}
int gsc_doActionOnUpdate(const TCHAR *UsrFunction, int cls, int sub, long gs_id,
                         C_RB_LIST *pColValues, int Reason)
{  // Usata per classi con collegamento a DB
   C_RB_LIST cmd;
   presbuf   ret;

   // se è indicato che non si devono chiamare funzioni esterne
   if (Reason & NO_EXTERN_ACTION) return GS_GOOD;

   if ((cmd << acutBuildList(RTSTR, UsrFunction, RTSHORT, cls, RTSHORT, sub,
                             RTLONG, gs_id, 0)) == NULL) return GS_BAD;

   if (pColValues)
   {
      if ((cmd += gsc_rblistcopy(pColValues->get_head())) == NULL) return GS_BAD;
   }
   else
      if ((cmd += acutBuildList(RTNIL, 0)) == NULL) return GS_BAD;

   if ((cmd += acutBuildList(RTSHORT, Reason, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (acedInvoke(cmd.get_head(), &ret) != RTNORM)
      { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (!ret) { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (ret->restype != RTT) { acutRelRb(ret); GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   acutRelRb(ret);

   return GS_GOOD;
}


/******************************************************************/
/*.doc gsc_doActionOnErase                                        */
/*+                                                            
  Funzione che richiama un comando esterna in forma di una funzione LISP.
  La funzione deve restituire T se tutto OK qualsiasi altro valore (compreso NULL)
  in caso di errore.
  Alla funzione vengono passati i seguenti parametri: oggetto grafico di GEOsim
  da cancellare o già cancellata (a seconda del momento in cui avviene 
  questa chiamata), il nome del comando che ha scatenato questa chiamata.
  Nel caso in cui l'operazione sia effettuata per una classe gruppo
  alla funzione vengono passati i seguenti parametri: codice dell'entità di GEOsim
  da cancellare o già cancellata (a seconda del momento in cui avviene questa chiamata),
  il nome del comando che ha scatenato questa chiamata.
  Parametri:
  const TCHAR *UsrFunction;   nome funzione da richiamare
  ads_name    Entity;         oggetto grafico GEOsim
  int         Reason;         origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                              RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                              (default = UNKNOWN_MOD)

  oppure:
  const TCHAR *UsrFunction;   nome funzione da richiamare
  int        cls;             codice classe
  int        sub;             codice sottoclasse
  long       gs_id;           codice chiave entità GEOsim
  int        Reason;          origine della modifica sugli oggetti: GEOMETRY_MOD, FAS_MOD,
                              RECORD_MOD, UNKNOWN_MOD, NO_EXTERN_ACTION (flag a bit)
                              (default = UNKNOWN_MOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int gsc_doActionOnErase(const TCHAR *UsrFunction, ads_name Entity, int Reason)
{
   C_RB_LIST cmd;
   presbuf   ret;

   // se è indicato che non si devono chiamare funzioni esterne
   if (Reason & NO_EXTERN_ACTION) return GS_GOOD;

   if ((cmd << acutBuildList(RTSTR, UsrFunction, RTENAME, Entity, 0)) == NULL)
      return GS_BAD;
   if ((cmd += acutBuildList(RTSHORT, Reason, 0)) == NULL)
      return GS_BAD;
   if (acedInvoke(cmd.get_head(), &ret) != RTNORM)
      { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (!ret) { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (ret->restype != RTT) { acutRelRb(ret); GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   acutRelRb(ret);

   return GS_GOOD;
}
int gsc_doActionOnErase(const TCHAR *UsrFunction, int cls, int sub, long gs_id, int Reason)
{
   C_RB_LIST cmd;
   presbuf   ret;

   // se è indicato che non si devono chiamare funzioni esterne
   if (Reason & NO_EXTERN_ACTION) return GS_GOOD;

   if ((cmd << acutBuildList(RTSTR, UsrFunction, RTLONG, gs_id, 0)) == NULL)
      return GS_BAD;
   if ((cmd += acutBuildList(RTSHORT, Reason, 0)) == NULL)
      return GS_BAD;
   if (acedInvoke(cmd.get_head(), &ret) != RTNORM)
      { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (!ret) { GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   if (ret->restype != RTT) { acutRelRb(ret); GS_ERR_COD = eGSInvUsrCmd; return GS_BAD; }
   acutRelRb(ret);

   return GS_GOOD;
}


/*************************************************************/
/*.doc gsc_prepare_for_linked_ent      <internal>
/*+                                                                       
  Funzione che compila 2 o 4 istruzioni SQL:
  1 istruzione seleziona i gruppi old con membri noti
  2 istruzione seleziona tutti i membri di un gruppo old noto
  3 istruzione seleziona i gruppi temporanei con membri noti
  4 istruzione seleziona tutti i membri di un gruppo temporaneo noto

  Parametri:
  C_PROJECT *pPrj;            Puntatore a progetto
  int       Cls;              Codice classe semplice
  C_FAMILY_LIST &family_list; Lista delle relazioni tra classi (vedi "gsc_getfamily")
  C_REPARED_CMD_LIST &CmdList; Lista dei comandi compilati

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_prepare_for_linked_ent(C_PROJECT *pPrj, int Cls, C_FAMILY_LIST &family_list,
                               C_PREPARED_CMD_LIST &CmdList)
{
   int          result = GS_GOOD;
   C_PREPARED_CMD *pPreparedCmd;
   C_FAMILY     *pFamily;
   C_CLASS      *pMotherCls;

   CmdList.remove_all();

   pFamily = (C_FAMILY *) family_list.get_head();
   while (pFamily)
   {  // se la classe è membro di questa famiglia
      if (pFamily->relation.search_key(Cls) != NULL)
      { 
         // ricavo il puntatore della classe madre
         if ((pMotherCls = pPrj->find_class(pFamily->get_key())) == NULL) return GS_BAD;

         // seleziona i gruppi old con membri noti
         if (!(pPreparedCmd = new C_PREPARED_CMD))
            { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
         CmdList.add_tail(pPreparedCmd);
         pPreparedCmd->set_key(pFamily->get_key());
         if (pMotherCls->prepare_reldata_where_member(*pPreparedCmd, OLD) == GS_BAD)
            { result = GS_BAD; break; }
         
         // seleziona tutti i membri di un gruppo old noto
         if (!(pPreparedCmd = new C_PREPARED_CMD))
            { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
         CmdList.add_tail(pPreparedCmd);
         pPreparedCmd->set_key(pFamily->get_key());
         if (pMotherCls->prepare_reldata_where_key(*pPreparedCmd, OLD) == GS_BAD)
            { result = GS_BAD; break; }

         // solo se la classe è estratta
         if (pMotherCls->is_extracted())
         {  // seleziona i gruppi temporanee con membri noti
            if (!(pPreparedCmd = new C_PREPARED_CMD))
               { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
            CmdList.add_tail(pPreparedCmd);
            pPreparedCmd->set_key(pFamily->get_key());
            if (pMotherCls->prepare_reldata_where_member(*pPreparedCmd, TEMP) == GS_BAD)
               { result = GS_BAD; break; }
      
            // seleziona tutti i membri di un gruppo temporaneo nota
            if (!(pPreparedCmd = new C_PREPARED_CMD))
               { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
            CmdList.add_tail(pPreparedCmd);
            pPreparedCmd->set_key(pFamily->get_key());
            if (pMotherCls->prepare_reldata_where_key(*pPreparedCmd, TEMP) == GS_BAD)
               { result = GS_BAD; break; }
         }
      }

      pFamily = (C_FAMILY *) family_list.get_next();
   }

   return result;
}


/*************************************************************/
/*.doc gsc_is_member_of
/*+                                                                       
  Funzione che controlla se una entità del progetto corrente è
  membro di altra classe (gruppo).
  Parametri:
  C_CLASS       *pClass;         puntatore alla classe entità
  long          gs_id;           codice entità
  C_FAMILY_LIST &family_list;    Lista delle relazioni tra classi (vedi "gsc_getfamily")
  int           *result;         GS_GOOD se l'entità è membro di altra
                                 classe (gruppo) altrimenti GS_BAD
  C_PREPARED_CMD_LIST *pCmdList; lista di comandi preparati
                                 (vedi "gsc_prepare_for_linked_ent") default = NULL
                                 se = NULL la funzione provvederà alla compilazione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*************************************************************/
int gsc_is_member_of(C_CLASS *pClass, long key, C_FAMILY_LIST &family_list, int *result,
                     C_PREPARED_CMD_LIST *pCmdList)
{
   C_CLASS           *pMotherCls;
   C_ENT_FAMILY_LIST member_list;
   C_FAMILY          *pFamily;
   int               cls = pClass->ptr_id()->code;

   *result = GS_BAD;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // le simulazioni non possono far parte di gruppi
   if (pClass->is_subclass() == GS_GOOD) return GS_GOOD;

   pFamily = (C_FAMILY *) family_list.get_head();
   while (pFamily)
   {  // se la classe è membro di questa famiglia
      if (pFamily->relation.search_key(cls) != NULL)
      { 
         // ricavo il puntatore della classe madre
         if ((pMotherCls = GS_CURRENT_WRK_SESSION->find_class(pFamily->get_key())) == NULL)
            return GS_BAD;
         // ricavo, se esiste la lista delle semplici del gruppo
         if (pMotherCls->fill_group_class(cls, key,
                                            member_list, pCmdList) == GS_BAD) 
            return GS_BAD;
         // se la lista member_list contiene elementi allora esiste
         // un gruppo che lega questa entità.
         if (member_list.get_count() > 0) { *result = GS_GOOD; return GS_GOOD; }
      }

      pFamily = (C_FAMILY *) family_list.get_next();
   }

   return GS_GOOD;
}


/*************************************************************/
/*.doc  C_CLASS::fill_group_class
/*+                                                                       
  Funzione che restituisce il gruppo (eventuale) di entità collegate
  tra loro da un entità della classe.
  Parametri:
  int               cls;               codice classe da analizzare
  long              Key;               codice entità da analizzare
  C_ENT_FAMILY_LIST &out;              Lista delle entità collegate
  C_PREPARED_CMD_LIST *pCmdList;       Lista di istruzioni precompilate 
                                       (vedi "gsc_prepare_for_linked_ent") default = NULL
                                       se = NULL la funzione provvederà alla compilazione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. La funzione deve essere applicata solo a classi gruppi
-*/  
/*************************************************************/
int C_CLASS::fill_group_class(int cls, long Key, C_ENT_FAMILY_LIST &out,
                              C_PREPARED_CMD_LIST *pCmdList)
{
   C_ENT_FAMILY     *punt;
   C_LONG_LIST      IDGroupList;
   C_LONG           *pIDGroup;
   C_2INT_LONG_LIST ent_list;
   C_2INT_LONG      *ent_punt;
   C_CLS_PUNT       *new_punt;
   C_PREPARED_CMD   pTempCmd, pOldCmd, NoUsedCmd;
   C_PREPARED_CMD   *pPreparedTempCmd = NULL, *pPreparedOldCmd = NULL;
   C_CLASS          *pClass;
   C_SELSET         entSS;
   int              result = GS_BAD, MotherCls = ptr_id()->code;
   C_ATTRIB_LIST    *p_attrib_list = NULL;
   C_PROJECT        *pPrj = (C_PROJECT *) id.pPrj;

   if (!ptr_info() || ptr_group_list() == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   out.remove_all();

   do
   {
      if (!pCmdList ||
          !(pPreparedOldCmd = (C_PREPARED_CMD *) pCmdList->search_key(MotherCls)))
      {  // prima va la compilazione per tabella TEMP
         // solo se la classe è estratta
         if (is_extracted())
            if (prepare_reldata_where_member(pTempCmd, TEMP) == GS_BAD) break;

         // dopo va la compilazione per tabella OLD
         if (prepare_reldata_where_member(pOldCmd, OLD) == GS_BAD) break;

         // leggo i codici dei gruppi legati a questa entità
         if (get_group_list(pTempCmd, pOldCmd, cls, Key, IDGroupList) == GS_BAD) 
            break;
      }
      else // le istruzioni sono state precompilate
      {
         pPreparedTempCmd = (C_PREPARED_CMD *) pPreparedOldCmd->get_next()->get_next();
         if (pPreparedTempCmd && pPreparedTempCmd->get_key() != MotherCls)
            pPreparedTempCmd = NULL;

         // leggo i codici dei gruppi legati a questa entità
         if (get_group_list((pPreparedTempCmd) ? *pPreparedTempCmd : NoUsedCmd,
                             *pPreparedOldCmd,
                             cls, Key, IDGroupList) == GS_BAD) 
            break;
      }

      if (IDGroupList.get_count() == 0) { result = GS_GOOD; break; }
   
      if (!pCmdList)
      {  // prima va la compilazione per tabella TEMP
         // solo se la classe è estratta
         if (is_extracted())
            if (prepare_reldata_where_key(pTempCmd, TEMP) == GS_BAD) break;
         
         // dopo va la compilazione per tabella OLD
         if (prepare_reldata_where_key(pOldCmd, OLD) == GS_BAD) break;
      }
      else // le istruzioni sono state precompilate
      {
         pPreparedOldCmd  = (C_PREPARED_CMD *) pPreparedOldCmd->get_next();
         if (pPreparedTempCmd)
            pPreparedTempCmd = (C_PREPARED_CMD *) pPreparedTempCmd->get_next();
      }

      result = GS_GOOD;
      // scorro la lista dei gruppi collegati all'entità
      pIDGroup = (C_LONG *) IDGroupList.get_head();
      while (pIDGroup)
      {
         if ((punt = new C_ENT_FAMILY) == NULL)
            { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
         out.add_tail(punt);
         punt->set_id(pIDGroup->get_id()); 

         // leggo i codici dei membri che costituiscono il gruppo
         if (!pCmdList)
         {
            if (get_member(pTempCmd, pOldCmd, pIDGroup->get_id(), &ent_list) == GS_BAD)
               { result = GS_BAD; break; }
         }
         else
            if (get_member((pPreparedTempCmd) ? *pPreparedTempCmd : NoUsedCmd,
                           *pPreparedOldCmd,
   			 	            pIDGroup->get_id(), &ent_list) == GS_BAD)
               { result = GS_BAD; break; }

         // PER OGNI MEMBRO TROVO L'ENTITA'
         ent_punt = (C_2INT_LONG *) ent_list.get_head();
         while (ent_punt)
         {
   	      if ((pClass = pPrj->find_class(ent_punt->get_key(), ent_punt->get_type())) == NULL)
   		      { result = GS_BAD; break; }

            if (pClass->is_extracted() == GS_GOOD)
            {
               if (pClass->get_SelSet(ent_punt->get_id(), entSS, GRAPHICAL) == GS_BAD)
                  { result = GS_BAD; break; }
               // numero di oggetti grafici associati al linkset
               if (entSS.length() <= 0)
               {  // l'entità non è in grafica
                  if (ent_punt->get_id() < 0)
                  {  // entità nuova che non esiste più a cui, probabilmente, 
                     // è stato fatto un UNDO che ha cancellato il suo inserimento
                     out.remove_tail(); // cancello questo gruppo
                     break;
                  }
                  else
                  {  // l'entità esisteva prima della sessione di lavoro corrente
                     if ((new_punt = new C_CLS_PUNT(pClass))==NULL)
                        { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
                     punt->family.add_tail(new_punt);
                     new_punt->gs_id = ent_punt->get_id();
                  }
               }
               else
               {
                  ads_name entity;
                  long     ItemNum = 0;

                  while (entSS.entname(ItemNum++, entity) == GS_GOOD)
                  {
                     if ((new_punt = new C_CLS_PUNT(pClass, entity, ent_punt->get_id())) == NULL)
                        { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
                     punt->family.add_tail(new_punt);
         	      }
               }
            }
            else
            {  // la classe non è estratta
               if ((new_punt = new C_CLS_PUNT(pClass)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; result = GS_BAD; break; }
               punt->family.add_tail(new_punt);
               new_punt->gs_id = ent_punt->get_id();
            }

            if (result == GS_BAD) break;

            ent_punt = (C_2INT_LONG *) ent_punt->get_next();
         }

         if (result == GS_BAD) break;

         pIDGroup = (C_LONG *) pIDGroup->get_next();
      }
   }
   while (0);

   return result;
}


/////////////////////////////////////////////////////////
/////       INIZIO   TOPOLOGIA DINAMICA             /////
/////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gsc_topo_align_for_erased              <external> */
/*+
  Questa funzione viene invocata quando si è verificato l'evento "cancellazione"
  di oggetti simulazione dal database grafico corrente di ACAD. La funzione esegue
  il riallineamento cancellando o inserendo gli elementi collegati agli oggetti cancellati.
  Parametri:
  C_STR_BTREE &HandleList; Lista di handle degli oggetti simulazione cancellati (senza blocchi DA)
  C_SELSET   &ToAlign;     Oggetti da riallineare perchè coinvolti nella cancellazione
  C_SELSET   *pRefused;    Gli oggetti rifiutati verranno aggiunti a questo gruppo di selezione 
                           (perchè bloccati da altri utenti, perchè non soddisfano funzioni 
                           di validità ...); di default = NULL;
  int CounterToVideo;      Flag per visualizzare il contatore di entità elaborate (default=GS_BAD)

  Restituisce il GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Questa funzione viene chiamata dopo la gsc_class_align che cancella dalla grafica gli oggetti.
-*/  
/*********************************************************/
int gsc_topo_align_for_erased(C_STR_BTREE &HandleList, C_SELSET &ToAlign,
                              C_SELSET *pRefused, int CounterToVideo)
{
   C_TOPOLOGY topo;
   ads_name   ent, LinkEnt, NodeEnt;
   long       i = 0, Refused = 0, Key;
   C_CLASS    *pSub, *pLinkedSub;
   TCHAR      Msg[MAX_LEN_MSG];       // per notifica in file .log
   C_RB_LIST  AdjList;
   presbuf    p;
   int        WhyNotUpd, j, Skip, cls, sub;
   int        set_lock = GS_GOOD, TryToExtractPartialEnt = GS_GOOD;
   C_ID       *p_id;
   C_LINK     Link;
   C_LINK_SET LinkSet;
   C_SELSET   LinkSS, GraphSS;
   C_BSTR     *pHandle;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1081)); // "Riallineamento banca dati"

   ToAlign.clear();

   // se non ci sono oggetti nel gruppo di selezione
   if (HandleList.get_count() == 0) return GS_GOOD;

   if (CounterToVideo == GS_GOOD)
   {
      acutPrintf(gsc_msg(849)); // "\n\nControllo topologico per entità cancellate...\n"
      StatusBarProgressMeter.Init(HandleList.get_count());
   }

   topo.set_type(TYPE_POLYLINE); // tipologia di tipo rete

   // per ogni handle della lista
   pHandle = (C_BSTR *) HandleList.go_top();
   while (pHandle)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      // Se l'oggetto è già stato scartato dalla gsc_class_align
      if (acdbHandEnt(pHandle->get_name(), ent) == RTNORM)
         if (GEOsimAppl::REFUSED_SS.is_member(ent) == GS_GOOD)
            { pHandle = (C_BSTR *) HandleList.go_next(); continue; }

      // l'oggetto è modificabile perchè non è stato scartato dalla gsc_class_align

      // legge i dati
      Link.SetEnt(pHandle->get_name());
      if (Link.GetKey(&Key, &cls, &sub) == GS_BAD)
      {
         // Notifico in file log
         swprintf(Msg, MAX_LEN_MSG, _T("Invalid key on entity: handle <%s>, prj %d."),
                 pHandle->get_name(), GS_CURRENT_WRK_SESSION->get_PrjId());
         gsc_write_log(Msg);
         Refused++;
         pHandle = (C_BSTR *) HandleList.go_next();
         continue;
      }

      // Ritorna il puntatore alla classe cercata
      if ((pSub = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL)
         { pHandle = (C_BSTR *) HandleList.go_next(); continue; }
      if (pSub->is_subclass() == GS_BAD || !(p_id = pSub->ptr_id()))
         { pHandle = (C_BSTR *) HandleList.go_next(); continue; }

      // classe madre
      topo.set_cls(GS_CURRENT_WRK_SESSION->find_class(cls));

      // se si tratta di un nodo
      if (pSub->get_type() == TYPE_NODE)
      {
         C_RB_LIST     LinkList, NewNodeDefaultValues;
         _RecordsetPtr pNewNodeInsRs;
         C_SUB         *pNewNode = NULL;
         int           NewNodeIsDefCalc, InitNodeCreated, FinalNodeCreated;
         C_CLS_PUNT    *pInitNode, *pFinalNode;

         // ricavo i link collegati es. ((<sub1> <id1>)(<sub2> <id2>)))
         if ((AdjList << topo.elemadj(Key, TYPE_POLYLINE)) == NULL)
         {
            pHandle = (C_BSTR *) HandleList.go_next();
            continue;
         }

         // se i link collegati sono modificabili (se non dovessero 
         // esserlo per "entità estratta parzialmente" si provvede ad estrarre totalmente
         // l'entità in questione)
         j = 0;
         Skip = FALSE;
         while ((p = AdjList.nth(j++)))
         {
            p = p->rbnext;
            if ((pLinkedSub = GS_CURRENT_WRK_SESSION->find_class(cls, p->resval.rint)) == NULL)
               { Skip = TRUE; break; }

            p = p->rbnext;
            // se il link non è stato cancellato
            if (pLinkedSub->get_SelSet(p->resval.rlong, LinkSS) == GS_GOOD &&
                LinkSS.copy(GraphSS) == GS_GOOD && GraphSS.intersectType(GRAPHICAL) == GS_GOOD &&
                GraphSS.length() > 0)
            {
               // se l'oggetto è modificabile (se non dovesse esserlo per
               // "entità estratta parzialmente" si provvede ad estrarre totalmente)
               if (pLinkedSub->is_updateableSS(p->resval.rlong, LinkSS, &WhyNotUpd,
                                               set_lock, TryToExtractPartialEnt) == GS_BAD)
                  { Skip = TRUE; break; }
            }
         }

         if (Skip)
         {
            // ripristino l'entità
            acdbHandEnt(pHandle->get_name(), ent);
            gsc_UnEraseEnt(ent);
            if (pRefused) pRefused->add(ent);
            Refused++;
            pHandle = (C_BSTR *) HandleList.go_next();
            continue;
         }

         // per ogni link collegato al nodo
         j = 0;
         while ((p = AdjList.nth(j++)))
         {
            p = p->rbnext;
            if ((pLinkedSub = GS_CURRENT_WRK_SESSION->find_class(cls, p->resval.rint)) == NULL)
               { Skip = TRUE; break; }

            p = p->rbnext;
            // se il link non è stato cancellato
            if (pLinkedSub->get_SelSet(p->resval.rlong, LinkSS, GRAPHICAL) == GS_GOOD &&
                LinkSS.length() > 0)
            {
               // ricavo l'entità principale del link
               if (LinkSS.entname(0, LinkEnt) == GS_BAD) { Skip = TRUE; break; }

               // inserisco il link in gruppo per gsc_class_align
               ToAlign.add(LinkEnt);
               // Ricavo la sottoclasse nodale da inserire a inizio e fine lato se ci fosse bisogno
               pNewNode = ((C_EXTERN *) GS_CURRENT_WRK_SESSION->find_class(pLinkedSub->ptr_id()->code))->getJollyNodalSub(pLinkedSub->ptr_id()->sub_code);
         
               if (pNewNode)
               {
                  C_DBCONNECTION *pTempConn;
                  C_STRING NewNodeTempTableRef;

                  // ricavo la tabella temporanea
                  if (pNewNode->getTempTableRef(NewNodeTempTableRef) == GS_BAD)
                  {
                     // ripristino l'entità
                     acdbHandEnt(pHandle->get_name(), ent);
                     gsc_UnEraseEnt(ent);
                     if (pRefused) pRefused->add(ent);
                     Refused++;
                     pHandle = (C_BSTR *) HandleList.go_next();
                     continue;
                  }

                  // leggo i valori di default 
                  if (pNewNode->get_default_values(NewNodeDefaultValues) == GS_BAD)
                  {
                     // ripristino l'entità
                     acdbHandEnt(pHandle->get_name(), ent);
                     gsc_UnEraseEnt(ent);
                     if (pRefused) pRefused->add(ent);
                     Refused++;
                     pHandle = (C_BSTR *) HandleList.go_next();
                     continue;
                  }

                  // ricavo le connessioni ai database
                  if ((pTempConn = pNewNode->ptr_info()->getDBConnection(TEMP)) == NULL)
                  {
                     // ripristino l'entità
                     acdbHandEnt(pHandle->get_name(), ent);
                     gsc_UnEraseEnt(ent);
                     if (pRefused) pRefused->add(ent);
                     Refused++;
                     pHandle = (C_BSTR *) HandleList.go_next();
                     continue;
                  }

                  // preparo istruzione per l'inserimento di record nella tabella temp
                  if (pTempConn->InitInsRow(NewNodeTempTableRef.get_name(), pNewNodeInsRs) == GS_BAD)
                  {
                     // ripristino l'entità
                     acdbHandEnt(pHandle->get_name(), ent);
                     gsc_UnEraseEnt(ent);
                     if (pRefused) pRefused->add(ent);
                     Refused++;
                     pHandle = (C_BSTR *) HandleList.go_next();
                     continue;
                  }

                  NewNodeIsDefCalc = pNewNode->ptr_attrib_list()->is_DefCalculated();
               }

               if (gsc_verify_topo_on_align_for_link((C_SUB *) pLinkedSub, LinkEnt, pNewNode,
                                                     NewNodeDefaultValues,
                                                     NewNodeIsDefCalc,
                                                     pNewNodeInsRs,
                                                     &pInitNode, &InitNodeCreated,
                                                     &pFinalNode, &FinalNodeCreated) == GS_BAD)
               {
                  gsc_DBCloseRs(pNewNodeInsRs);
                  // ripristino l'entità
                  acdbHandEnt(pHandle->get_name(), ent);
                  gsc_UnEraseEnt(ent);
                  if (pRefused) pRefused->add(ent);
                  Refused++;
                  pHandle = (C_BSTR *) HandleList.go_next();
                  continue;
               }
               gsc_DBCloseRs(pNewNodeInsRs);

               // Aggiorno la topologia
               if (topo.editlink(pLinkedSub->ptr_id()->sub_code, p->resval.rlong, LinkEnt, 
                                 ((C_CLASS *) (pInitNode->get_class()))->ptr_id()->sub_code,
                                 pInitNode->get_gs_id(),
                                 ((C_CLASS *) (pFinalNode->get_class()))->ptr_id()->sub_code,
                                 pFinalNode->get_gs_id()) == GS_BAD)
               {
                  if (InitNodeCreated == GS_GOOD) pNewNode->erase_data(pInitNode->ent);
                  if (pInitNode) delete pInitNode;
                  if (FinalNodeCreated == GS_GOOD) pNewNode->erase_data(pFinalNode->ent);
                  if (pFinalNode) delete pFinalNode;

                  // ripristino l'entità
                  acdbHandEnt(pHandle->get_name(), ent);
                  gsc_UnEraseEnt(ent);
                  if (pRefused) pRefused->add(ent);
                  Refused++;
                  pHandle = (C_BSTR *) HandleList.go_next();
                  continue;
               }

               if (pInitNode) delete pInitNode;
               if (pFinalNode) delete pFinalNode;
            }
         }
      }
      else
      if (pSub->get_type() == TYPE_POLYLINE)
      {
         // ricavo i nodi collegati es. ((<init sub> <init id>)(<final sub> <final id>)))
         if ((AdjList << topo.elemadj(Key, TYPE_NODE)) == NULL)
         {
            // Aggiorno la topologia
            topo.editdelelem(Key, TYPE_POLYLINE);
            pHandle = (C_BSTR *) HandleList.go_next();
            continue;
         }

         // se i nodi collegati sono modificabili (se non dovessero 
         // esserlo per "entità estratta parzialmente" si provvede ad estrarre totalmente
         // l'entità in questione)
         j = 0;
         Skip = FALSE;
         while ((p = AdjList.nth(j++)))
         {
            p = p->rbnext;
            if ((pLinkedSub = GS_CURRENT_WRK_SESSION->find_class(p_id->code, p->resval.rint)) == NULL)
               { Skip = TRUE; break; }

            p = p->rbnext;
            // se il link non è stato cancellato
            if (pLinkedSub->get_SelSet(p->resval.rlong, LinkSS) == GS_GOOD &&
                LinkSS.copy(GraphSS) == GS_GOOD && GraphSS.intersectType(GRAPHICAL) == GS_GOOD &&
                GraphSS.length() > 0)
            {
               // se l'oggetto è modificabile (se non dovesse esserlo per
               // "entità estratta parzialmente" si provvede ad estrarre totalmente)
               if (pLinkedSub->is_updateableSS(p->resval.rlong, LinkSS, &WhyNotUpd,
                                               set_lock, TryToExtractPartialEnt) == GS_BAD)
                  { Skip = TRUE; break; }

               // ricavo l'entità principale del nodo
               if (pLinkedSub->get_SelSet(p->resval.rlong, LinkSS, GRAPHICAL) == GS_BAD ||
                   LinkSS.entname(0, NodeEnt) == GS_BAD)
                  { Skip = TRUE; break; }
               // inserisco il nodo in gruppo per gsc_verify_topo_on_align_for_node
               ToAlign.add(NodeEnt);
            }
         }

         if (Skip)
         {
            // ripristino l'entità
            acdbHandEnt(pHandle->get_name(), ent);
            gsc_UnEraseEnt(ent);
            if (pRefused) pRefused->add(ent);
            Refused++;
            pHandle = (C_BSTR *) HandleList.go_next();
            continue;
         }

         // Aggiorno la topologia
         if (topo.editdelelem(Key, TYPE_POLYLINE) == GS_BAD)
         {
            // ripristino l'entità
            acdbHandEnt(pHandle->get_name(), ent);
            gsc_UnEraseEnt(ent);
            if (pRefused) pRefused->add(ent);
            Refused++;
            pHandle = (C_BSTR *) HandleList.go_next();
            continue;
         }
      }

      pHandle = (C_BSTR *) HandleList.go_next();
   }

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      i--;
      if (i >= 0)
         acutPrintf(gsc_msg(308), i, Refused); // "\n%ld entità GEOsim elaborate, %ld scartate."
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_topo_align_for_modified            <external> */
/*+
  Questa funzione viene invocata quando si è verificato l'evento "modifica"
  di oggetti simulazione dal database grafico corrente di ACAD. La funzione esegue
  il riallineamento della posizione degli elementi collegati agli oggetti spostati.
  Parametri:
  C_SELSET &SelSet;     Oggetti simulazione spostati
  C_SELSET &ToAlign;    Oggetti da riallineare perchè coinvolti nello spostamento
                        (link se sono stato spostati dallo spostamento dei nodi, 
                        nodi che potrebbero essere rimasti isolati dallo 
                        spostamento dei link)
  C_SELSET *pRefused;   Gli oggetti rifiutati verranno aggiunti a questo gruppo di selezione 
                        (perchè bloccati da altri utenti, perchè non soddisfano funzioni 
                        di validità ...); di default = NULL;
  int CounterToVideo;   Flag per visualizzare il contatore di entità elaborate (default=GS_BAD)

  Restituisce il GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_topo_align_for_modified(C_SELSET &SelSet, C_SELSET &ToAlign,
                                C_SELSET *pRefused, int CounterToVideo)
{
   C_TOPOLOGY topo;
   ads_name   ent, LinkEnt, NodeEnt;
   long       i = 0, Refused = 0, SelSet_len, Key;
   C_CLASS    *pSub, *pLinkedSub;
   TCHAR      Msg[MAX_LEN_MSG];       // per notifica in file .log
   TCHAR      Handle[MAX_LEN_HANDLE]; // per notifica in file .log
   C_RB_LIST  AdjList;
   presbuf    p;
   int        WhyNotUpd, j, Skip;
   C_ID       *p_id;
   C_LINK     Link;
   ads_point  pIns, pOrig, LinkPoint;
   C_SELSET   LinkSS;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1081)); // "Riallineamento banca dati"

   ToAlign.clear();

   // se non ci sono oggetti nel gruppo di selezione
   if ((SelSet_len = SelSet.length()) <= 0) return GS_GOOD;

   if (CounterToVideo == GS_GOOD)
   {
      acutPrintf(gsc_msg(848)); // "\n\nControllo topologico per entità modificate...\n"
      StatusBarProgressMeter.Init(SelSet_len);
   }

   topo.set_type(TYPE_POLYLINE); // tipologia di tipo rete

   // per ogni oggetto del gruppo di selezione
   while (SelSet.entname(i++, ent) == GS_GOOD)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(i);

      // Salto i blocchi attributi
      if (gsc_is_DABlock(ent) == GS_GOOD) continue;

      // Ritorna il puntatore alla classe cercata
      if ((pSub = GS_CURRENT_WRK_SESSION->find_class(ent)) == NULL) continue;
      if (pSub->is_subclass() == GS_BAD || !(p_id = pSub->ptr_id())) continue;

      // classe madre
      topo.set_cls(GS_CURRENT_WRK_SESSION->find_class(p_id->code));

      // legge i dati
      Link.SetEnt(ent);
      if (Link.GetKey(&Key) == GS_BAD)
      {
         // Notifico in file log
         gsc_enthand(ent, Handle);
         swprintf(Msg, MAX_LEN_MSG, _T("Invalid key on entity: handle <%s>, prj %d, class %d."),
                  Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code);      
         gsc_write_log(Msg);
         if (pRefused) pRefused->add(ent);
         Refused++;
         continue;
      }

      // se l'oggetto è modificabile (se non dovesse esserlo per
      // "entità estratta parzialmente" si provvede ad estrarre totalmente)
      if (pSub->is_updateable(Key, &WhyNotUpd) == GS_BAD)
         if (WhyNotUpd == eGSPartialEntExtract)
         {
            if (pSub->entExtract(Key) == GS_BAD)
               { if (pRefused) pRefused->add(ent); Refused++; continue; }
         }
         else
            { if (pRefused) pRefused->add(ent); Refused++; continue; }

      // se si tratta di un nodo
      if (pSub->get_type() == TYPE_NODE)
      {
         C_RB_LIST LinkList;

         // ricavo i link collegati es. ((<sub1> <id1>)(<sub2> <id2>)))
         if ((AdjList << topo.elemadj(Key, TYPE_POLYLINE)) == NULL)
         {
            // Notifico in file log
            gsc_enthand(ent, Handle);
            swprintf(Msg, MAX_LEN_MSG, _T("Invalid topology on entity: handle <%s>, prj %d, class %d."),
                     Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code);      
            gsc_write_log(Msg);
            if (pRefused) pRefused->add(ent); 
            Refused++;
            // inserisco il nodo in gruppo per gsc_verify_topo_on_align_for_node
            ToAlign.add(ent);
            continue;
         }

         // se i link collegati sono modificabili (se non dovessero 
         // esserlo per "entità estratta parzialmente" si provvede ad estrarre totalmente
         // l'entità in questione)
         j = 0;
         Skip = FALSE;
         while ((p = AdjList.nth(j++)))
         {
            p = p->rbnext;
            if ((pLinkedSub = GS_CURRENT_WRK_SESSION->find_class(p_id->code, p->resval.rint)) == NULL)
               { Skip = TRUE; break; }

            // se l'oggetto è modificabile (se non dovesse esserlo per
            // "entità estratta parzialmente" si provvede ad estrarre totalmente)
            p = p->rbnext;
            if (pLinkedSub->is_updateable(p->resval.rlong, &WhyNotUpd) == GS_BAD)
               if (WhyNotUpd == eGSPartialEntExtract)
               {
                  if (pLinkedSub->entExtract(p->resval.rlong) == GS_BAD)
                     { Skip = TRUE; break; }
               }
               else
                  { Skip = TRUE; break; }
         }

         if (Skip)
         {
            if (pRefused) pRefused->add(ent); 
            Refused++;
            continue;
         }

         // se il nodo non rispetta le regole di sovrapposizione
         if (gsc_get_firstPoint(ent, pIns) == GS_BAD)
         {
            if (pRefused) pRefused->add(ent);
            Refused++;
            continue;
         }
         gsc_enthand(ent, Handle);
         ads_point_set(pIns, pOrig);
         while (gsc_OverlapValidation(pIns, pSub, Handle) == GS_BAD)
         {  // viene spostato finchè trova una posizione valida
            pIns[X] = pIns[X] + 1;
            gsc_set_firstPoint(ent, pIns);
         }

         // per ogni link collegato al nodo
         j = 0;
         while ((p = AdjList.nth(j++)))
         {
            p = p->rbnext;
            if ((pLinkedSub = GS_CURRENT_WRK_SESSION->find_class(p_id->code, p->resval.rint)) == NULL)
               { Skip = TRUE; break; }
            p = p->rbnext;
            // ricavo l'entità principale del link
            if (pLinkedSub->get_SelSet(p->resval.rlong, LinkSS, GRAPHICAL) == GS_BAD ||
                LinkSS.entname(0, LinkEnt) == GS_BAD)
               { Skip = TRUE; break; }

            // ricavo i nodi collegati es. ((<init sub> <init id>)(<final sub> <final id>)))
            if ((LinkList << topo.elemadj(p->resval.rlong, TYPE_NODE)) == NULL)
            {
               // Notifico in file log
               gsc_enthand(LinkEnt, Handle);
               swprintf(Msg, MAX_LEN_MSG, _T("Invalid topology on entity: handle <%s>, prj %d, class %d."),
                        Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), pLinkedSub->ptr_id()->code);      
               gsc_write_log(Msg);
               Skip = TRUE;
               continue;
            }
            
            // se il nodo è il nodo iniziale del link
            if ((p = LinkList.nth(0)) != NULL && gsc_nth(0, p)->resval.rint == p_id->sub_code &&
                gsc_nth(1, p)->resval.rlong == Key)
            {  // se il punto iniziale del link è diverso dal punto di inserimento del nodo
               if (gsc_get_firstPoint(LinkEnt, LinkPoint) == GS_BAD) { Skip = TRUE; break; }
               if (!gsc_2Dpoint_equal(pIns, LinkPoint)) // controllo solo 2D
               {
                  // sposto punto iniziale polilinea sul punto di inserimento del nodo
                  if (gsc_set_firstPoint(LinkEnt, pIns) == GS_BAD) { Skip = TRUE; break; }
                     // inserisco link in gruppo per gsc_class_align
                     ToAlign.add(LinkEnt);
               }
            }
            else  // altrimenti il nodo è il nodo finale del link
            {  // se il punto finale del link è diverso dal punto di inserimento del nodo
               if (gsc_get_lastPoint(LinkEnt, LinkPoint) == GS_BAD) { Skip = TRUE; break; }
               if (!gsc_2Dpoint_equal(pIns, LinkPoint)) // controllo solo 2D
               {
                  // sposto punto iniziale polilinea sul punto di inserimento del nodo
                  if (gsc_set_lastPoint(LinkEnt, pIns) == GS_BAD) { Skip = TRUE; break; }
                     // inserisco link in gruppo per gsc_class_align
                     ToAlign.add(LinkEnt);
               }
            }
         }
      }
      else
      if (pSub->get_type() == TYPE_POLYLINE)
      {
         _RecordsetPtr pNewNodeInsRs;
         C_SUB         *pNewNode = NULL;
         C_RB_LIST     NewNodeDefaultValues;
         int           NewNodeIsDefCalc, InitNodeCreated, FinalNodeCreated;
         C_CLS_PUNT    *pInitNode, *pFinalNode;

         // Ricavo la sottoclasse nodale da inserire a inizio e fine lato se ci fosse bisogno
         C_INT_INT *pConnect = (C_INT_INT *) pSub->ptr_connect_list()->get_head();

         // ricavo i nodi collegati es. ((<init sub> <init id>)(<final sub> <final id>)))
         if ((AdjList << topo.elemadj(Key, TYPE_NODE)) == NULL)
         {
            // Notifico in file log
            gsc_enthand(ent, Handle);
            swprintf(Msg, MAX_LEN_MSG, _T("Invalid topology on entity: handle <%s>, prj %d, class %d."),
                     Handle, GS_CURRENT_WRK_SESSION->get_PrjId(), p_id->code);      
            gsc_write_log(Msg);
            if (pRefused) pRefused->add(ent); 
            Refused++;
            continue;
         }

         // se i nodi collegati sono modificabili (se non dovessero 
         // esserlo per "entità estratta parzialmente" si provvede ad estrarre totalmente
         // l'entità in questione)
         j = 0;
         Skip = FALSE;
         while ((p = AdjList.nth(j++)))
         {
            p = p->rbnext;
            if ((pLinkedSub = GS_CURRENT_WRK_SESSION->find_class(p_id->code, p->resval.rint)) == NULL)
               { Skip = TRUE; break; }

            // se l'oggetto è modificabile (se non dovesse esserlo per
            // "entità estratta parzialmente" si provvede ad estrarre totalmente)
            p = p->rbnext;
            if (pLinkedSub->is_updateable(p->resval.rlong, &WhyNotUpd) == GS_BAD)
               if (WhyNotUpd == eGSPartialEntExtract)
               {
                  if (pLinkedSub->entExtract(p->resval.rlong) == GS_BAD)
                     { Skip = TRUE; break; }
               }
               else
                  { Skip = TRUE; break; }

            // ricavo l'entità principale del nodo
            if (pLinkedSub->get_SelSet(p->resval.rlong, LinkSS, GRAPHICAL) == GS_BAD ||
                LinkSS.entname(0, NodeEnt) == GS_BAD)
               { Skip = TRUE; break; }
            // inserisco il nodo in gruppo per gsc_verify_topo_on_align_for_node
            ToAlign.add(NodeEnt);
         }

         if (Skip)
         {
            if (pRefused) pRefused->add(ent); 
            Refused++;
            continue;
         }

         // Ricavo la sottoclasse nodale da inserire a inizio e fine lato se ci fosse bisogno
         pNewNode = ((C_EXTERN *) GS_CURRENT_WRK_SESSION->find_class(p_id->code))->getJollyNodalSub(p_id->sub_code);
                  
         if (pNewNode)
         {
            C_DBCONNECTION *pTempConn;
            C_STRING NewNodeTempTableRef;

            // ricavo la tabella temporanea
            if (pNewNode->getTempTableRef(NewNodeTempTableRef) == GS_BAD)
               { if (pRefused) pRefused->add(ent); Refused++; continue; }

            // leggo i valori di default 
            if (pNewNode->get_default_values(NewNodeDefaultValues) == GS_BAD)
               { if (pRefused) pRefused->add(ent); Refused++; continue; }

            // ricavo le connessioni ai database
            if ((pTempConn = pNewNode->ptr_info()->getDBConnection(TEMP)) == NULL)
               { if (pRefused) pRefused->add(ent); Refused++; continue; }

            // preparo istruzione per l'inserimento di record nella tabella temp
            if (pTempConn->InitInsRow(NewNodeTempTableRef.get_name(), pNewNodeInsRs) == GS_BAD)
               { if (pRefused) pRefused->add(ent); Refused++; continue; }

            NewNodeIsDefCalc = pNewNode->ptr_attrib_list()->is_DefCalculated();
         }

         if (gsc_verify_topo_on_align_for_link((C_SUB *) pSub, ent, pNewNode,
                                               NewNodeDefaultValues,
                                               NewNodeIsDefCalc,
                                               pNewNodeInsRs,
                                               &pInitNode, &InitNodeCreated,
                                               &pFinalNode, &FinalNodeCreated) == GS_BAD)
         {
            gsc_DBCloseRs(pNewNodeInsRs);
            if (pRefused) pRefused->add(ent);
            Refused++;
            continue;
         }
         gsc_DBCloseRs(pNewNodeInsRs);

         // Aggiorno la topologia
         if (topo.editlink(p_id->sub_code, Key, ent, 
                           ((C_CLASS *) (pInitNode->get_class()))->ptr_id()->sub_code,
                           pInitNode->get_gs_id(),
                           ((C_CLASS *) (pFinalNode->get_class()))->ptr_id()->sub_code,
                           pFinalNode->get_gs_id()) == GS_BAD)
         {
            if (InitNodeCreated == GS_GOOD) pNewNode->erase_data(pInitNode->ent);
            if (pInitNode) delete pInitNode;
            if (FinalNodeCreated == GS_GOOD) pNewNode->erase_data(pFinalNode->ent);
            if (pFinalNode) delete pFinalNode;

            if (pRefused) pRefused->add(ent);
            Refused++;
            continue;
         }

         if (pInitNode) delete pInitNode;
         if (pFinalNode) delete pFinalNode;
      }
   }

   if (CounterToVideo == GS_GOOD)
   {
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
      i--;
      if (i >= 0)
         acutPrintf(gsc_msg(308), i, Refused); // "\n%ld entità GEOsim elaborate, %ld scartate."
   }

   return GS_GOOD;
}


/*******************************************************/
/*.doc int gsc_locatePositionDAblockToGS()             */
/*+                                                                   
   Funzione che inserisce la scheda di GEOsim prendendo come 
   riferimento l'eventuale blocco DA.
   Attenzione, la funzione grazie ai cambiamenti di MAP 2000
   ora deve solo considerare i blocchi di GEOsim.
  
   Parametri:
      C_SELSET    ssPR        Gruppo di selezione delle principali. 
      C_SELSET    ssDA        Gruppo di selezione dei blocchi DA. 
      C_RB_LIST   VisValues   Rb list dei campi visibili

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_locatePositionDAblockToGS(ads_name blockGS, ads_name blockDA, C_RB_LIST &VisValues)
{
   C_ATTRIB_FAS_LIST AttFASBlk;
   C_ATTRIB_FAS      *pAttFAS = NULL;
   C_RB_LIST         Descr, AttribDef;
   presbuf           pVis = NULL, pRb = NULL;
   ads_point         AbsPt, RealPt;
   double            rtBlock = 0.0;            
   long              idx = 0;
   int               result;
   C_ATTRIB_BLOCK    DABlock;

   do
   {
      result = GS_BAD;
      // Ricavo la descrizione del blocco di riferimento
      if ((Descr << acdbEntGet(blockDA)) == NULL) break;
      // Verifico che sia un blocco
      if ((pRb = Descr.SearchType(0)) == NULL || 
           gsc_strcmp(pRb->resval.rstring, _T("INSERT")) != 0) break; 
      // Verifico che sia un blocco $t
      if ((pRb = Descr.SearchType(2)) == NULL || 
           gsc_strcmp(pRb->resval.rstring, _T("$T")) != 0) break; 
      // Mi ricavo anche il punto di inserimento del blocco
      if ((pRb = Descr.SearchType(10)) == NULL) break; 
      ads_point_set(pRb->resval.rpoint, AbsPt);
      // Mi ricavo anche la rotazione del blocco
      if ((pRb = Descr.SearchType(50)) == NULL) break; 
      rtBlock = pRb->resval.rreal;
      // Prima di tutto occorre spostare il blocco dell' entità importata
      // nella posizione del blocco DA e ruotarlo in maniera corretta
      if (gsc_set_firstPoint(blockGS, AbsPt, &rtBlock, GS_BAD) == GS_BAD) break;
      // Ricavo la C_ATTRIB_FAS_LIST dal blocco da usare come riferimento
      if (gsc_from_attblock2attribfaslist(blockDA, &AttFASBlk) == GS_BAD) break;
      // A questo punto mi scorro la RB_LIST VisValues che 
      // contiene i valori visibili per applicare l' eventuale spostamento
      idx = 0;
      while ((pVis = VisValues.nth(idx++)))
      {
         if ((pAttFAS = (C_ATTRIB_FAS *) AttFASBlk.search_name(gsc_nth(0, pVis)->resval.rstring, FALSE)) != NULL)
         {
            // In pAttFAS->pt ho la posizione dell' attributo relativa al blocco
            // Mi calcolo la posizione corretta
            RealPt[0] = AbsPt[0] + pAttFAS->pt[0];
            RealPt[1] = AbsPt[1] + pAttFAS->pt[1];
            RealPt[2] = AbsPt[2] + pAttFAS->pt[2];
            
            DABlock.set_ent_name(blockGS);
            // Per prima sposto l' attributo nella posizione corretta
            if (DABlock.move_attrib(gsc_nth(0, pVis)->resval.rstring, pAttFAS->pt, &pAttFAS->ptr_fas()->rotation) == GS_BAD) break;
            
            // Ricavo dal blocco di GEOsim le caratteristiche dell' attributo di nome pName
            if (gsc_getDefAttrFromGSblock(blockGS, pAttFAS->get_name(), AttribDef) == GS_BAD) break;

            // Chiamo la funzione che cambia l' altezza dell' attributo
            if (DABlock.set_h_text_attrib(pAttFAS->ptr_fas()->h_text, pAttFAS->get_name()) == GS_BAD)
               break;

            // Chiamo la funzione che cambia lo stile di testo dell' attributo
            if (DABlock.set_style_attrib(pAttFAS->ptr_fas()->style, pAttFAS->get_name()) == GS_BAD)
               break;

            // Chiamo la funzione che cambia lo stile di testo dell' attributo
            if (DABlock.set_style_attrib(pAttFAS->ptr_fas()->style, pAttFAS->get_name()) == GS_BAD)
               break;
         }

         pVis = VisValues.get_next();
      }
      result = GS_GOOD;
   }
   while (0);

   return result;
}


/*******************************************************/
/*.doc int gsc_insBlockAttrib()                         */
/*+                                                                   
   Funzione che inserisce gli eventuali blocchi attributi 
   in posizione di default oppure ricavando le caratteristiche 
   grafiche dagli eventuali blocchi DA creati dall' utente. 
  
  Parametri:
  C_CLASS  pClass   Puntatore alla classe di destinazione 
  C_SELSET ssPR     Gruppo di selezione delle principali. 
  C_SELSET ssDA     Gruppo di selezione dei blocchi DA. 
  long BitForChangeToNewFAS; Opzionale, Flag a bit per indicare quali caratteristiche
                             grafiche cambiare (default = GSNoneSetting)
  C_FAS *pNewFas;            Opzionale, i valori delle caratteristiche grafiche da cambiare 
                             (se = NULL si intendono i valori di default della classe)


  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_insBlockAttrib(C_CLASS *pClass, C_SELSET &ssPR, C_SELSET &ssDA,
                       long BitForChangeToNewFAS, C_FAS *pNewFas,
                       C_RB_LIST &ColValues)
{
   C_ID           *pId = NULL;
   C_FAS          *pFas = (pNewFas) ? pNewFas : pClass->ptr_fas();
   C_INFO         *pInfo = NULL;
   C_RB_LIST      VisValues, Descr;
   C_ATTRIB_BLOCK attrib_block;
   C_ATTRIB_LIST  *pAttriblist;
   C_EED          eed;
   C_STRING       TypeBlk;
   ads_name       ent, princ, blockDA, PrimaDiInserimento, DopoInserimento;
   long           index = 0, fAggregazione, key, kkk = 0, blInserted = 0, retValue = 0;
   int            result;
   double         rot;    
   presbuf        p = NULL;
   ads_point      point;
   C_COLOR        color;

   pId  = pClass->ptr_id();
   // Se la classe di destinazione è spaghetti non devo fare niente
   if (pId->category == CAT_SPAGHETTI) return GS_GOOD;
   
   pInfo       = pClass->ptr_info();
   pAttriblist = pClass->ptr_attrib_list();

   // Calcolo il fattore di aggregazione
   fAggregazione = ssPR.length();

   // Se non si tratta di entità testo, e la classe ha 
   // almeno un attributo visibile
   if (pId->type != TYPE_TEXT && pAttriblist->is_visible() == GS_GOOD)
   {
      // Ricavo la prima entità del gruppo delle principali
      if (ssPR.entname(0, princ) != GS_GOOD) return GS_BAD;
      // if (eed.load(princ) == GS_BAD) return GS_BAD;
      // if (pClass->query_data(princ, ColValues) == GS_BAD) return GS_BAD;

      // Ricavo il codice chiave 
      if ((p = ColValues.CdrAssoc(pInfo->key_attrib.get_name())) != NULL)
         if (gsc_rb2Lng(p, &key) == GS_BAD) key = 0;
      // Ricavo gli attributi visibili
      if ((VisValues << pAttriblist->get_vis_values(ColValues)) == GS_BAD) return GS_BAD;
      
      // Inserisco il valore chiave della scheda
      // Da verificare !!!
      // asei_cdsc_setlong(pAttriblist->pColdscCols[pAttriblist->nKeyCol], newKey);
      if (pClass->getKeyValue(princ, &key) == GS_BAD) return GS_BAD;

      if (ssDA.length() > 0)
      {
         // Se esistono blocchi DA da prendere come riferimento allora 
         // allora devo leggere le caratteristiche di questi ed inserire
         // la scheda di GEOsim in posizione ovviamente un blocco di GEOsim
         // per ognun blocco DA

         if (BitForChangeToNewFAS & GSDABlockLayerSetting)
            // Setto il layer per il blocco attributi
            if (attrib_block.set_layer(pFas->layer) == GS_BAD) return GS_BAD;

         if (BitForChangeToNewFAS & GSTextHeightSetting)
            // Setto l'altezza testo degli attributi del blocco
            if (attrib_block.set_h_text(pFas->h_text) == GS_BAD) return GS_BAD;

         if (BitForChangeToNewFAS & GSTextStyleSetting)
            // Setto stile testo degli attributi del blocco
            if (attrib_block.set_text_style(pFas->style) == GS_BAD) return GS_BAD;

         if (BitForChangeToNewFAS & GSColorSetting)
            // Setto il colore
            if (attrib_block.set_color(pFas->color) == GS_BAD) return GS_BAD;

         // !!! non ho messo qui l' else dell'if precedente perchè
         // i valori della FAS vanno letti nel ciclo dopo
         // solo per il caso di ApplFAS = GS_BAD
         
         kkk = 0;
         while (ssDA.entname(kkk++, blockDA) == GS_GOOD)
         {
            // Ricavo dal blocco DA il punto di inserimento e la rotazione
            if ((Descr << acdbEntGet(blockDA)) == NULL) continue;
            // Ricavo la ROTAZIONE del blocco
            if ((p = Descr.SearchType(50)) == NULL) continue;
            rot =  p->resval.rreal;
            // Ora il PUNTO DI INSERIMENTO
            if ((p = Descr.SearchType(10)) == NULL) continue;
            ads_point_set(p->resval.rpoint, point);
            // color
            gsc_get_color(blockDA, color);
            // Setto le prime due caratteristiche
            attrib_block.set_insert_point(point);
            attrib_block.set_rotation(gsc_rad2grd(rot));
            
            // Verifico per prima cosa se è un blocco $T di GEOsim oppure
            // un DA di AutoCAD MTEXT
            if ((p = Descr.SearchType(0)) == NULL) continue;
            TypeBlk.set_name(p->resval.rstring);

            if (!(BitForChangeToNewFAS & GSDABlockLayerSetting))
            {
               // Leggo e setto il layer per il blocco attributi
               if ((p = Descr.SearchType(8)) == NULL) continue;
               if (attrib_block.set_layer(p->resval.rstring) == GS_BAD) continue;
            }

            if (!(BitForChangeToNewFAS & GSTextHeightSetting))
            {
               // Leggo e setto l'altezza testo degli attributi del blocco
               // Se si tratta di un $T l'altezza è memorizzata nel 41
               // se è un MTEXT è nel 40
               if (gsc_strcmp(TypeBlk.get_name(), _T("MTEXT")) == 0)
               {
                  if ((p = Descr.SearchType(40)) == NULL) continue;
               }
               else if (gsc_strcmp(TypeBlk.get_name(), _T("INSERT")) == 0)
                  if ((p = Descr.SearchType(41)) == NULL) continue;

               if (attrib_block.set_h_text(p->resval.rreal) == GS_BAD) continue;
            }

            if (!(BitForChangeToNewFAS & GSColorSetting))
               // Leggo e setto il colore degli attributi del blocco
               if (attrib_block.set_color(color) == GS_BAD) continue;

            // Memorizzo ultimo oggetto nel disegno prima dell' inserimento del blocco
            if (acdbEntLast(PrimaDiInserimento) != RTNORM) continue;
            if (attrib_block.insert(VisValues, pId->code, pId->sub_code, key,
                                    pClass->ptr_fas()->file_ref_block, pClass->ptr_fas()->ref_block,
                                    AUTO, fAggregazione + 1) != GS_GOOD)
               continue;
            else 
               { fAggregazione++; blInserted++; }

            // Memorizzo ultimo oggetto nel disegno dopo
            if (acdbEntLast(DopoInserimento) != RTNORM) continue;
            if (!ads_name_equal(PrimaDiInserimento, DopoInserimento))
               // Modifico la posizione e rotazione di ogni singolo attributo 
               // del blocco appena inserito 
               gsc_locatePositionDAblockToGS(DopoInserimento, blockDA, VisValues);
         }

         result = GS_GOOD;

      }
      else 
      {
         // Se non esistono dei blocchi di riferimento allora devo andare
         // a vedere il valore della variabile di GEOsim InsPos che mi 
         // dice se si deve inserire automaticamente il blocco in posizione 
         // di default
         if (GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO)
         {
            do
            {
               result = GS_BAD;
               // Per prima cosa ricavo il punto di inserimento, la rotazione
               // di default in base al tipo di classe 
               if (pId->type == TYPE_POLYLINE) 
               {
                  // Punto medio tratto + lungo
                  if (gsc_getPntRtzOnObj(princ, point, &rot, _T("MML"), 0, 0, GS_GOOD) == GS_BAD) break;
               }
               else if (pId->type == TYPE_NODE) 
               {
                  // Punto di inserimento
                  if (gsc_getPntRtzOnObj(princ, point, &rot, _T("S"), 0, 0, GS_GOOD) == GS_BAD) break;
               }
               else if (pId->type == TYPE_SURFACE) 
               {
                  // Centroide
                  if (gsc_get_centroidpoint(ssPR, point) == GS_BAD) break;
                  rot = 0.0;
               }
               else break;

               // Setto il punto di inserimento e la rotazione 
               attrib_block.set_insert_point(point);
               attrib_block.set_rotation(gsc_rad2grd(rot));

               // Setto il layer per il blocco attributi
               if (BitForChangeToNewFAS & GSDABlockLayerSetting)
               {
                  if (attrib_block.set_layer(pFas->layer) == GS_BAD) return GS_BAD;
               }
               else
                  if (attrib_block.set_layer(pClass->ptr_fas()->layer) == GS_BAD) return GS_BAD;

               // Setto l'altezza testo degli attributi del blocco
               if (BitForChangeToNewFAS & GSTextHeightSetting)
               {
                  if (attrib_block.set_h_text(pFas->h_text) == GS_BAD) break;
               }
               else
                  if (attrib_block.set_h_text(pClass->ptr_fas()->h_text) == GS_BAD) break;

               // Setto stile testo degli attributi del blocco
               if (BitForChangeToNewFAS & GSTextStyleSetting)
               {
                  if (attrib_block.set_text_style(pFas->style) == GS_BAD) break;
               }
               else
                  if (attrib_block.set_text_style(pClass->ptr_fas()->style) == GS_BAD) break;

               // Setto il colore
               if (BitForChangeToNewFAS & GSColorSetting)
               {
                  if (attrib_block.set_color(pFas->color) == GS_BAD) break;
               }
               else
                  if (attrib_block.set_color(pClass->ptr_fas()->color) == GS_BAD) break;

               if (attrib_block.insert(VisValues, pId->code, pId->sub_code, key,
                                       pClass->ptr_fas()->file_ref_block, pClass->ptr_fas()->ref_block,
                                       GEOsimAppl::GLOBALVARS.get_InsPos(), fAggregazione + 1) != GS_GOOD)
                  break;
               else 
                  fAggregazione++; 
                  // { fAggregazione++; blInserted++; }

               result = GS_GOOD;
            }
            while (0);
            if (result == GS_GOOD)
               fAggregazione = ssPR.length() + 1;
         }
         else
            result = GS_GOOD;
      }

      // Se è andato tutto bene allora devo aggiornare i fattori di aggregazione
      if (result == GS_GOOD)
      {
         index = 0;
         while (ssPR.entname(index++, ent) == GS_GOOD)
         {
            if (eed.load(ent) == GS_GOOD)
            {
               eed.num_el = fAggregazione;
               eed.save();
            }
         }
         retValue = blInserted;
      }
      else
         retValue = GS_BAD;
   }   

   return retValue;
}


/*******************************************************/
/*.doc int gsc_getDefAttrFromGSblock()                 */
/*+                                                                   
   Funzione che restituisce il resbuf con le caratteristiche 
   dell' attributo specificato come parametro leggendole dal
   blocco di GEOsim.
  
   Parametri:
      ads_name    BlockGs  Nome blocco di GEOsim.
      TCHAR   &NameAttr Nome attributo da ricercare.
      C_RB_LIST  &DefAttr  RB_LIST contenente la definizione 
                           dell' attributo.
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_getDefAttrFromGSblock(ads_name BlockGs, TCHAR *NameAttr, C_RB_LIST &DefAttr) 
{
   presbuf     prb;
   ads_name    ndx;
   int         result = GS_GOOD;
   C_STRING    NameTmp;
   C_RB_LIST   AttribList;

   ads_name_set(BlockGs, ndx);
   while (ads_entnext(ndx, ndx) == RTNORM)
   {
      // Leggo le caratteristiche dell' attributo
      if ((AttribList << acdbEntGet(ndx)) == NULL) { result = GS_BAD; break; }
      // Verifico che sia un attributo
      if ((prb = AttribList.SearchType(0)) == NULL || 
           gsc_strcmp(prb->resval.rstring, _T("ATTRIB")) != 0) { result = GS_BAD; break; }
      // Controllo il nome se è uguale al parametro di ingresso
      if ((prb = AttribList.SearchType(2)) == NULL) { result = GS_BAD; break; }
      NameTmp.set_name(prb->resval.rstring);
      if (gsc_strcmp(NameTmp.get_name(), NameAttr) == 0)
      {  
         DefAttr << AttribList.get_head();
         AttribList.ReleaseAllAtDistruction(GS_BAD);
         break;
      }
   }
    
   return result;
}