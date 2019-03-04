/**********************************************************
Name: GS_AREA.CPP
                                   
Module description: File funzioni di base per la gestione
                    delle aree di lavoro GEOSIM 
            
Author: Roberto Poltini

(c) Copyright 1995-2016 by IREN ACQUA GAS  S.p.A.

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
#include "adsdlg.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_opcod.h"
#include "gs_error.h"
#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_init.h"
#include "gs_dbref.h"
#include "gs_user.h"
#include "gs_class.h"
#include "gs_conv.h"
#include "gs_sec.h"
#include "gs_prjct.h"
#include "gs_netw.h"      // funzioni di rete
#include "gs_graph.h"
#include "gs_svdb.h"      // salvataggio database
#include "gs_area.h"
#include "gs_dwg.h"       // gestione disegni
#include "gs_ade.h"
#include "gs_lock.h"
#include "gs_cmd.h"       // per gsc_enable_reactors
#include "gs_query.h"
#include "gs_filtr.h"     // comando di filtro in geoeditor
#include "gs_evid.h"
#include "gs_lisp.h"      // per caricamento file GS_LISP_FILE
#include "gs_impor.h"
#include "gs_topo.h"      // per topologia simulazioni


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

C_WRK_SESSION *GS_CURRENT_WRK_SESSION = NULL; // sessione di lavoro corrente
_RecordsetPtr RS_LOCK_WRKSESSION; // variabile per scongelamento


/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/


int arg_to_prj_area(presbuf *arg, int *prj, long *SessionId);
int gsc_thaw_area2(int prj, long SessionId, bool RecoverDwg = false);
int gsc_prepare_4_thaw(int prj, TCHAR *dir_area);
int gsc_is_extractable(C_INT_INT_LIST &ClassList_to_extract, C_INT_INT *cls);
int gsc_is_completed_extracted(C_INT_INT_LIST &ClassList_to_extract);


//-----------------------------------------------------------------------//
/////////////////////  C_AREACLASS   //////////////////////////////////////
//-----------------------------------------------------------------------//


// costruttore
C_AREACLASS::C_AREACLASS() : C_4INT_STR() { sel = DESELECTED; }

// distruttore
C_AREACLASS::~C_AREACLASS() {}

int C_AREACLASS::get_sel()  { return sel; }

int C_AREACLASS::set_sel(int in)  { sel = in; return GS_GOOD; }

int C_AREACLASS::set(int par_code, TCHAR *par_name, GSDataPermissionTypeEnum par_level, int par_cat,
                     int par_type, int par_sel)
{  
   if (set_key(par_code) == GS_BAD || set_name(par_name) == GS_BAD ||
       set_level(par_level) == GS_BAD || set_category(par_cat) == GS_BAD ||
       set_type(par_type) == GS_BAD || set_sel(par_sel) == GS_BAD)
      return GS_BAD;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_AREACLASS::to_rb                     <internal> */
/*+
  Questa funzione restituisce una lista di resbuf che descrive
  l'oggetto C_AREACLASS con il seguente formato:
  (<code><name><level><cat><type><sub_list><sel>)
    
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
resbuf *C_AREACLASS::to_rb(void)
{
   C_RB_LIST        List;
   C_AREACLASS      *p_sub;
   C_AREACLASS_LIST *sub_list;
            
   if ((List << acutBuildList(RTLB,
                              RTSHORT, get_key(),                       // codice classe
                              RTSTR, (get_name()) ? get_name() : GS_EMPTYSTR, // nome classe
                              RTSHORT, (int) get_level(),                     // livello di abilitazione
                              RTSHORT, get_category(),                  // categoria
                              RTSHORT, get_type(), 0)) == NULL)         // tipo
      return NULL;

   // sub_list
   sub_list = (C_AREACLASS_LIST *) ptr_sub_list();
   p_sub    = (C_AREACLASS *) sub_list->get_head();
   if (p_sub == NULL)
   {
      //                         sub_list        selezionato
      if ((List += acutBuildList(RTNIL, RTSHORT, get_sel(), 0)) == NULL)
         return NULL;
   }
   else
   {
      if ((List += acutBuildList(RTLB, 0)) == NULL) return NULL;

      while (p_sub)
      {
         if ((List += acutBuildList(RTLB,
                                    RTSHORT, p_sub->get_key(),
                                    RTSTR, (p_sub->get_name()) ? p_sub->get_name() : GS_EMPTYSTR,
                                    RTSHORT, p_sub->get_level(),
                                    RTSHORT, p_sub->get_category(),
                                    RTSHORT, p_sub->get_type(),
                                    RTSHORT, p_sub->get_sel(),
                                    RTLE, 0)) == NULL)
            return NULL;

         p_sub = (C_AREACLASS *) sub_list->get_next();
      }
      if ((List += acutBuildList(RTLE, RTSHORT, get_sel(), 0)) == NULL) return NULL;
   }

   if ((List += acutBuildList(RTLE, 0)) == NULL) return NULL;
   List.ReleaseAllAtDistruction(GS_BAD);
                                
   return List.get_head();      
}


//-----------------------------------------------------------------------//
///////////////  C_AREACLASS     FINE     /////////////////////////////////
///////////////  C_AREACLASS_LIST  INIZIO    //////////////////////////////
//-----------------------------------------------------------------------//


// costruttore
C_AREACLASS_LIST::C_AREACLASS_LIST() : C_LIST() { }

// distruttore
C_AREACLASS_LIST::~C_AREACLASS_LIST() {}


resbuf *C_AREACLASS_LIST::to_rb(void)
{
   C_RB_LIST   List;  
   C_AREACLASS *punt;
            
   if (!(punt = (C_AREACLASS *) get_head()))
   {
      if ((List << acutBuildList(RTNIL, 0)) == NULL) return NULL;
   }
   else
      while (punt)
      {
         if ((List += punt->to_rb()) == NULL) return NULL;
         punt = (C_AREACLASS *) get_next();
      }

   List.ReleaseAllAtDistruction(GS_BAD);
                                
   return List.get_head();      
}


/*********************************************************/
/*.doc arg_to_prj_area <internal> */
/*+
  Questa funzione restituisce da una lista di resbuf il codice del
  progetto e il codice della sessione.
  Parametri:
  presbuf *arg;      Lista resbuf proveniente da Autocad
  int *prj;          Codice progetto (out)
  long *SessionId;   Codice sessione (out)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
  N.B. arg si sposta sul resbuf del codice della sessione
-*/  
/*********************************************************/
int arg_to_prj_area(presbuf *arg, int *prj, long *SessionId)
{
   if (arg == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   // codice progetto
   if (*arg == NULL || gsc_rb2Int(*arg, prj) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // codice area
   if ((*arg = (*arg)->rbnext) == NULL || gsc_rb2Lng(*arg, SessionId) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   
   return GS_GOOD;
}


////////////////// INIZIO FUNZIONI LISP (gs) ///////////////////


/*********************************************************/
/*.doc gs_is_frozen_curr_session
/*+
  Questa funzione verifica se la sessione di lavoro corrente
  � stata gi� congelata. 

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR
  al LISP viene restituito (T o nil).
-*/  
/*********************************************************/
int gs_is_frozen_curr_session(void)
{
   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->HasBeenFrozen() == GS_GOOD) acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_ruserWrkSession                     <external> */
/*+
  Questa funzione restituisce una lista delle aree dell'utente specificato
  nel seguente formato: 
  ((<code><nome><dir><type><status><usr><coordinate><data/ora creazione>(<lista classi>)...)
  dove:
  (<lista classi>) = lista di codici delle classi usate nella sessione (<cls 1><cls 2>...<cls n>)
  Parametri
  Lista : (<codice progetto> <codice utente> [stato area [OnlyOwner]])

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_ruserWrkSession(void)
{
   presbuf     arg;
   C_RB_LIST   ret;
   int         user_code, prj_code, Status;
   C_WRK_SESSION_LIST lista;
   
   acedRetNil();
   arg = acedGetArgs();
   // codice del progetto
   if (arg == NULL || arg->restype != RTSHORT) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   prj_code = arg->resval.rint;
   // codice utente
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   user_code = arg->resval.rint;
   // stato aree (opzionale)
   if ((arg = arg->rbnext) != NULL && arg->restype == RTSHORT) // Errore argomenti
   {
      Status = arg->resval.rint;
      // codice utente (opzionale)
      if ((arg = arg->rbnext) != NULL && arg->restype == RTSHORT) // Errore argomenti
      {  // leggo lista
         if (lista.ruserarea(user_code, prj_code, Status, arg->resval.rint, ONETEST) == GS_BAD)
            return RTERROR;
      }
      else // leggo lista
         if (lista.ruserarea(user_code, prj_code, Status, FALSE, ONETEST) == GS_BAD)
            return RTERROR;
   }
   else // leggo lista
      if (lista.ruserarea(user_code, prj_code, -1, FALSE, ONETEST) == GS_BAD)
         return RTERROR;

   ret << lista.to_rb();
   if (ret.IsEmpty()) return RTNORM;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}
                                   

/*********************************************************/
/*.doc gs_rWrkSessionClass <external> */
/*+
  Questa funzione restituisce una lista delle classi
  nella sessione di lavoro specificata nel seguente formato:
  ((<code><name><level><cat><type><sub_list><sel>)...)
  
  se category = CAT_EXTERN
     sub_list = (<sub_code><name><level><cat><type>nil<sel><fas><sql>)
  altrimenti 
     sub_list = nil
    
  Parametri: (<prj><area>)
    
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_rWrkSessionClass(void)
{
   presbuf          arg;
   C_RB_LIST        ret;
   int              prj_code;
   long             SessionId;
   C_AREACLASS_LIST lista;
   C_WRK_SESSION    *pSession;
   
   acedRetNil();
   arg = acedGetArgs();

   if (arg_to_prj_area(&arg, &prj_code, &SessionId) == GS_BAD) return RTERROR;

   // Ritorna il puntatore alla sessione cercata
   if ((pSession = gsc_find_wrk_session(prj_code, SessionId)) == NULL) return RTERROR;

   // Ritorna la lista delle classi della sessione (anche quelle non ancora estratte)
   if (pSession->get_class_list(lista, false) == GS_BAD) { delete pSession; return RTERROR; }
   delete pSession;
   lista.sort_name();
   ret << lista.to_rb();

   if (ret.IsEmpty()) return RTERROR;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}
                                   

/*********************************************************/
/*.doc  gs_drawWrkSessionspatialareas             <external> */
/*+
  Questa funzione disegna le aree spaziali estratte dalle sessioni
  di lavoro del progetto indicato.
  Parametri: <prj>

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int  gs_drawWrkSessionspatialareas(void)
{
   presbuf            arg;
   int                prj_code, AutoCADColorIndex = 1; // rosso
   C_COLOR            color;
   C_RECT_LIST        SpatialAreaList;
   C_RECT             *pSpatialArea;
   C_INT_INT_STR      CurrentUsr;
   C_INT_INT_STR_LIST UsrList;
   C_WRK_SESSION_LIST WrkSessionList;
   C_WRK_SESSION      *pWrkSession;
   C_STRING           Lbl;

   acedRetNil();
   arg = acedGetArgs();

   // codice del progetto
   if (arg == NULL || arg->restype != RTSHORT) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   prj_code = arg->resval.rint;

   // leggo l'utente corrente
   if (gsc_whoami(&CurrentUsr) == GS_BAD) return RTERROR;
   // leggo la lista degli utenti di GEOsim
   if (gsc_getusrlist(&UsrList) == GS_BAD) return RTERROR;

   // leggo lista sessioni di lavoro
   if (WrkSessionList.ruserarea(CurrentUsr.get_key(), prj_code) == GS_BAD) return RTERROR;

   color.setAutoCADColorIndex(AutoCADColorIndex);

   pWrkSession = (C_WRK_SESSION *) WrkSessionList.get_head();
   while (pWrkSession)
   {
      // Leggo i rettangoli che racchiudono aree spaziali estratte nella sessione
      if (pWrkSession->get_spatial_area_list(SpatialAreaList) == GS_GOOD)
      {
         pSpatialArea = (C_RECT *) SpatialAreaList.get_head();
         while (pSpatialArea)
         {
            Lbl = pWrkSession->get_id();
            if (UsrList.search_key(pWrkSession->get_usr()))
            {
               Lbl += _T(" (");
               Lbl += ((C_INT_INT_STR *) UsrList.get_cursor())->get_name();
               Lbl += _T(")");
            }
            pSpatialArea->Draw(_T("0"), &color, EXTRACTION, Lbl.get_name());
            pSpatialArea = (C_RECT *) SpatialAreaList.get_next();
         }
      }

      AutoCADColorIndex++;
      color.setAutoCADColorIndex(AutoCADColorIndex);

      pWrkSession = (C_WRK_SESSION *) WrkSessionList.get_next();
   }

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_clsextrinWrkSession <external> */
/*+
  Questa funzione verifica se esiste una lista di classi estratte
  nella sessione di lavoro attiva.
    
  Restituisce RTNORM in caso di successo altrimenti RTERROR.
-*/  
/*********************************************************/
int gs_clsextrinWrkSession(void)
{
   C_CLS_PUNT_LIST extr_cls_list;

   acedRetNil();

   // Verifico che esista una sessione di lavoro attiva
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   // Ricavo la lista delle classi estratte del progetto corrente
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(extr_cls_list) == GS_BAD) return RTERROR;
   
   // Verifico se la lista delle classi estratte � vuota
   if (extr_cls_list.get_count() == 0) { GS_ERR_COD = eGSNotClassExtr; return RTERROR; }

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_get_NextWrkSessionId                <external> */
/*+
  Questa funzione LISP ritorna il codice che sar� usato 
  per la nuova sessione di lavoro di GEOsim (codice incrementale).
  Parametri:
  Lista RESBUF <prj>
-*/  
/*********************************************************/
int gs_get_NextWrkSessionId(void)
{
   presbuf   arg;
   long      new_code;
   C_PROJECT *pPrj;

   acedRetNil();
   arg = acedGetArgs();

   // codice progetto
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(arg->resval.rint)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return RTERROR; }

   // cerco nuovo codice per la sessione di lavoro
   if ((new_code = pPrj->getNextWrkSessionId()) == 0) return RTERROR;

   acedRetReal(new_code); // devo ritornarlo come real perch� non c'� la funzione acedRetLong
  
   return RTNORM;
}


/*********************************************************/
/*.doc gs_create_WrkSession <external> */
/*+
  Questa funzione LISP crea una sessione di lavoro di GEOsim.
  Parametri:
  Lista RESBUF (<prj><nome><dir>[<tipo>[<coordinate>[<keydwg>]]])
  
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_create_WrkSession(void)
{
   C_WRK_SESSION *pSession;
   C_RB_LIST     lista;
   presbuf       arg;
   long          new_code;
   C_PROJECT     *pPrj;

   acedRetNil();
   arg = acedGetArgs();

   // codice progetto
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(arg->resval.rint)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return RTERROR; }

   // creo resbuf per input dati 
   if ((lista << acutBuildList(RTLB, RTNIL, 0)) == NULL) // code
      return RTERROR;
   
   // nome sessione
   if ((arg = arg->rbnext) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((lista += gsc_copybuf(arg)) == NULL) return RTERROR;
      
   // direttorio sessione
   if ((arg = arg->rbnext) == NULL) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((lista += gsc_copybuf(arg)) == NULL) return RTERROR;
   
   // valori opzionali
   if ((arg = arg->rbnext) != NULL)
   {  // tipo area
      if ((lista += gsc_copybuf(arg)) == NULL) return RTERROR;
      if ((arg = arg->rbnext) != NULL)
      {
         if ((lista += acutBuildList(RTNIL, RTNIL, 0)) == NULL) // usr, status
            return RTERROR;
         // sistema di coordinate
         if ((lista += gsc_copybuf(arg)) == NULL) return RTERROR;

         if ((arg = arg->rbnext) != NULL)
            // key_dwg
            if ((lista += gsc_copybuf(arg)) == NULL) return RTERROR;
      }
   }
   
   if ((lista += acutBuildList(RTLE, 0)) == NULL) return RTERROR;

   // Alloca un oggetto C_WRK_SESSION
   if ((pSession = new C_WRK_SESSION(pPrj)) == NULL) { GS_ERR_COD = eGSOutOfMem; return RTERROR; }
                     
   // Ricopia i dati dalla struttura di ingresso
   if (pSession->from_rb(lista.get_head()) == GS_BAD)
      { delete pSession; return RTERROR; }

   if ((new_code = pSession->create()) == 0)
      { delete pSession; return RTERROR; }
   acedRetReal(new_code); // devo ritornarlo come real perch� non c'� la funzione acedRetLong
  
   return RTNORM;
}


/*********************************************************/
/*.doc gs_del_WrkSession <external> */
/*+
  Questa funzione LISP cancella una sessione di lavoro di GEOsim
  senza alcun salvataggio dei dati.
  Parametri:
  Lista RESBUF: <prj><area><Password>
  
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_del_WrkSession(void)
{
   presbuf arg = acedGetArgs();
   int     prj;
   long    SessionId;

   acedRetNil();

   if (arg_to_prj_area(&arg, &prj, &SessionId) == GS_BAD) return RTERROR;
   // Password
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // chiamata funzione C++ 
   if (gsc_del_area(prj, SessionId, arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_sel_WrkSession_class <external> */
/*+
  Questa funzione LISP seleziona una lista di classi per la sessione di
  lavoro corrente del progetto corrente.
  Parametri:
  Lista RESBUF (<cls1><cls2>...)
  
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_sel_WrkSession_class(void)
{
   presbuf    arg = acedGetArgs();
   C_INT_LIST lista;
   
   acedRetNil();

   // leggo lista delle classi e sottoclassi
   if (lista.from_rb(arg) == GS_BAD) return RTERROR;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }

   if (GS_CURRENT_WRK_SESSION->SelClasses(lista) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_currentWrkSession                   <external> */
/*+
  Questa funzione LISP restituisce i dati della sessione corrente del
  progetto corrente in caso di successo altrimenti restituisce NIL.
  <code><nome><dir><type><status><usr><coordinate><data/ora creazione>
-*/  
/*********************************************************/
int gs_currentWrkSession(void)
{
   C_RB_LIST ret;
   
   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) return RTNORM; 
   if ((ret << GS_CURRENT_WRK_SESSION->to_rb()) == NULL) return RTERROR;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}


/************************************************************************************************************/
/*.doc gs_freeze_WrkSession <external>                                                                      */
/*+
  Questa funzione LISP congela la sessione di lavoro corrente del progetto corrente.
  
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/************************************************************************************************************/
int gs_freeze_WrkSession(void)
{
   acedRetNil();

   // chiamata funzione C++ 
   if (gsc_freeze_area() == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*****************************************************************************/
/*.doc gs_save_WrkSession <external>                                               */
/*+
  Questa funzione LISP salva tutti i dati relativi alla sessione di lavoro corrente
  del progetto corrente nella banca dati centrale.
  
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*****************************************************************************/
int gs_save_WrkSession(void)
{
   acedRetNil();

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }
   if (GS_CURRENT_WRK_SESSION->save() == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*****************************************************************************/
/*.doc gs_exit_WrkSession                                       <external>   */
/*+
  Questa funzione LISP chiude la sessione di lavoro corrente.
  In particolare se questa funzione era stata preceduta da una <gs_save_WrkSession> 
  l'area viene eliminata, mentre se era stata preceduta da una <gs_freeze_WrkSession>
  viene impostato lo status della sessione a WRK_SESSION_FREEZE.
  
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*****************************************************************************/
int gs_exit_WrkSession(void)
{
   acedRetT();
  
   if (gsc_ExitCurrSession() == GS_BAD) { acedRetNil(); return RTERROR; }

   return RTNORM;
}


/****************************************************************************/
/*.doc gs_thaw_WrkSession <external>                                              */
/*+
  Questa funzione LISP seleziona ed apre una sessione di lavoro congelata 
  in funzione del codice del progetto e del codice della sessione.

  Parametri
  input:    Lista RESBUF (<prj><area>)
  
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/************************************************************************/
int gs_thaw_WrkSession(void)
{  
   presbuf arg;
   int 	  prj;
   long    SessionId;
   
   acedRetNil();
   arg = acedGetArgs();
   if (arg_to_prj_area(&arg, &prj, &SessionId) == GS_BAD) return RTERROR;

   if (gsc_thaw_area(prj, SessionId) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/************************************************************************************************************/
/*.doc (new 2) gs_thaw_WrkSession2 <internal>                                                                      */
/*+
  Questa funzione LISP � di solo uso interno e viene utilizzata da <gs_sel_area>.

  Parametri
  input:    Lista RESBUF (<prj><area>[RecoverDwg])
  
  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/************************************************************************************************************/
int gs_thaw_WrkSession2(void)
{  
   presbuf arg;
   int 	  prj;
   long    SessionId;
   bool    RecoverDwg = false;
   
   acedRetNil();
   arg = acedGetArgs();

   // Codice progetto e codice sessione
   if (arg_to_prj_area(&arg, &prj, &SessionId) == GS_BAD) return RTERROR;
   // RecoverDwg
   if ((arg = arg->rbnext) && arg->restype != RTT) RecoverDwg = true;

   if (gsc_thaw_area2(prj, SessionId, RecoverDwg) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsextractareaclass                   <external> */
/*+
  Questa comando estrae le classi selezionate nella sessione
  di lavoro corrente. E definito come comando per non avere il tempo
  di ritardo di autocad successivo l'estrazione.
  
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
void gsextractareaclass(void)
{  
   int 	         Mode;
   C_INT_INT_LIST ErrClsCodeList;
   C_STRING       qryType;

   GEOsimAppl::CMDLIST.StartCmd();

   // modo di estrazione letto dalle impostazioni ADE
   if (gsc_ade_qrygettype(&qryType) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   
   if (qryType.comp(_T("PREVIEW"), GS_BAD) == 0) 
      Mode = PREVIEW;
   else if (qryType.comp(_T("DRAW"), GS_BAD) == 0)
      Mode = EXTRACTION;
   else
      return GEOsimAppl::CMDLIST.ErrorCmd();

   if (!GS_CURRENT_WRK_SESSION) return GEOsimAppl::CMDLIST.ErrorCmd();

   if (GS_CURRENT_WRK_SESSION->extract(Mode, FALSE, &ErrClsCodeList) == GS_BAD ||
       ErrClsCodeList.get_count() > 0)
      GS_CURRENT_WRK_SESSION->DisplayMsgsForClassNotExtracted(ErrClsCodeList);

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gs_retrieve_WrkSession <external> */
/*+
  Questa funzione LISP recupera un'area in cui si era interrotta
  l'applicazione.
  Parametri:
  Lista RESBUF (<prj><area>)
  
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_retrieve_WrkSession(void)
{  
   presbuf arg;
   int 	   prj;
   long     SessionId;
   
   acedRetNil();
   arg = acedGetArgs();
   if (arg_to_prj_area(&arg, &prj, &SessionId) == GS_BAD) return RTERROR;
   if (gsc_retrieve_area(prj, SessionId) == GS_BAD) return RTERROR;
   
   acedRetT();

   return RTNORM;
}


////////////////// INIZIO FUNZIONI C (gsc) ///////////////////
                                                                           

/*********************************************************/
/*.doc gsc_find_wrk_session <external> */
/*+
  Questa funzione restituisce un oggetto C_WRK_SESSION leggendolo da DB.   
  Parametri:
  int prj;          Codice progetto
  long SessionId;   Codice sessione
    
  Restituisce il puntatore alla sessione di lavoro in caso di successo
  altrimenti NULL.
-*/  
/*********************************************************/
C_WRK_SESSION *gsc_find_wrk_session(int prj, long SessionId)
{
   C_PROJECT      *pPrj;
   C_STRING       statement, TableRef;
   _RecordsetPtr  pRs;
   C_WRK_SESSION         *pWkrSession = NULL;
   C_DBCONNECTION *pDBConn;
   C_RB_LIST      ColValues;

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return NULL; }
   
   if (pPrj->getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE GS_ID=");
   statement += SessionId;
        
   // leggo le righe della tabella in ordine di GS_ID senza bloccarla
   if (pDBConn->ExeCmd(statement, pRs) == GS_BAD) return NULL;
          
   if (gsc_isEOF(pRs) == GS_GOOD)
      { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSInvSessionCode; return NULL; }

   // lettura con conversione date
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) 
      { gsc_DBCloseRs(pRs); return NULL; }

   if ((pWkrSession = new C_WRK_SESSION(pPrj)) == NULL)
   { 
      gsc_DBCloseRs(pRs);
      GS_ERR_COD = eGSOutOfMem; 
      return NULL; 
   }

   if (pWkrSession->from_rb_db(ColValues) == GS_BAD)
   {
      delete pWkrSession; 
      gsc_DBCloseRs(pRs);
      return NULL; 
   }
      
   if (gsc_DBCloseRs(pRs) == GS_BAD) return NULL;
      
   return pWkrSession;
}


/************************************************************************************************************/
/*.doc gsc_del_area <external>                                                                              */
/*+
  Questa funzione cancella una area di GEOsim senza alcun salvataggio.
  
  Parametri
  int         prj;        codice progetto
  long        SessionId;  codice sessione
  const TCHAR *Password;  Password dell'utente corrente (per controllo)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************************************************************/
int gsc_del_area(int prj, long SessionId, const TCHAR *Password)
{
   C_PROJECT *pPrj;
   C_WRK_SESSION    *pSession;

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL) return GS_BAD;
   // Cerco l'area
   if ((pSession = gsc_find_wrk_session(prj, SessionId)) == NULL) return GS_BAD;
   // cancello la sessione di lavoro                  
   if (pSession->del(Password) == GS_BAD) return GS_BAD;
   delete pSession;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_set_auxiliary_for_thaw_class <internal>                                                                      */
/*+
   Questa funzione (utilizzata nella funzione C_WRK_SESSION::thaw_2part) ripristina
   le strutture ausiliarie di appoggio alla lista degli attributi
   delle classi presenti in CLASSES_LIST_SEZ.
  
   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/
/*****************************************************************************/
int gsc_set_auxiliary_for_thaw_class()
{
   C_CLASS  *pCls;
   C_SUB    *pSub;
   C_STRING pathfile;
   TCHAR    entry[ID_PROFILE_LEN];
   int      cls, i = 1;
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_B2STR                 *pProfileEntry;

   // deseleziono le classi da estrarre
   if (gsc_desel_all_area_class() == GS_BAD) return GS_BAD;

   GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile);
   if (gsc_read_profile(pathfile, ProfileSections) == GS_BAD) return GS_BAD;
   
   swprintf(entry, ID_PROFILE_LEN, _T("%d"), i);
   ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(CLASSES_LIST_SEZ);
   while (ProfileSection && (pProfileEntry = (C_B2STR *) ProfileSection->get_ptr_EntryList()->search(entry)))
   {
      swscanf(pProfileEntry->get_name2(), _T("%d"), &cls);      
   
      // Ritorna il puntatore alla classe cercata
      if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls)) == NULL) return GS_BAD;

      switch (pCls->ptr_id()->sel)
      {
         case DESELECTED: pCls->ptr_id()->sel = SELECTED; break;
         case EXTRACTED : pCls->ptr_id()->sel = EXTR_SEL; break;
         default: break;
      }

      if (GS_CURRENT_WRK_SESSION->get_level() == GSReadOnlyData)
         pCls->ptr_id()->abilit = GSReadOnlyData;

      if (pCls->get_category() == CAT_EXTERN)
      {
         pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

         while (pSub != NULL)
         {
            switch (pSub->ptr_id()->sel)
            {
               case DESELECTED: pSub->ptr_id()->sel = SELECTED; break;
               case EXTRACTED : pSub->ptr_id()->sel = EXTR_SEL; break;
               default: break;
            } 
                 
            if (GS_CURRENT_WRK_SESSION->get_level() == GSReadOnlyData)
               pSub->ptr_id()->abilit = GSReadOnlyData;
                             
            pSub = (C_SUB *) pCls->ptr_sub_list()->get_next();
         }
      }   
      swprintf(entry, ID_PROFILE_LEN, _T("%d"), ++i);
   }
   
   return GS_GOOD;
}


/****************************************************************************************/
/*.doc gsc_prepare_4_thaw <internal>                                                    */
/*+
   Questa funzione (utilizzata nella funzione C_WRK_SESSION::thaw_1part)
   scrive nei registri i cataloghi e gli schemi delle classi presenti nella congelazione
   precedente per ripristinare correttamente il set di DWG attaccati.
   Parametri:
   int prj;             codice progetto
   TCHAR *dir_area;      direttorio area da scongelare

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/
/****************************************************************************************/
int gsc_prepare_4_thaw(int prj, TCHAR *dir_area)
{
   C_CLASS  *pCls;
   C_SUB    *pSub;
   C_INFO   *pInfo;
   C_STRING pathfile, TableRef;
   TCHAR    entry[ID_PROFILE_LEN];
   int      cls, i = 1;
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_B2STR                 *pProfileEntry;

   pathfile = dir_area;
   pathfile += _T('\\');
   pathfile += GEOTEMPSESSIONDIR;
   pathfile += _T('\\');
   pathfile += GS_CLASS_FILE;
   if (gsc_read_profile(pathfile, ProfileSections) == GS_BAD) return GS_BAD;
   
   swprintf(entry, ID_PROFILE_LEN, _T("%d"), i);
   ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(CLASSES_LIST_SEZ);
   while (ProfileSection && (pProfileEntry = (C_B2STR *) ProfileSection->get_ptr_EntryList()->search(entry)))
   {
      swscanf(pProfileEntry->get_name2(), _T("%d"), &cls);      
   
      // Ritorna il puntatore alla classe cercata
      if ((pCls = gsc_find_class(prj, cls, 0)) == NULL) return GS_BAD;

      // se � una classe con tabella associata e non di categoria simulazione che
      // era deselezionata
      if (pCls->get_category() != CAT_EXTERN)
      {
         if ((pInfo = pCls->ptr_info()) != NULL)
            if (pCls->getTempTableRef(TableRef, GS_BAD) == GS_BAD) return GS_BAD;
      }
      else
      {
         pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

         while (pSub != NULL)
         {
            if (pSub->getTempTableRef(TableRef) == GS_BAD) return GS_BAD;

            pSub = (C_SUB *) pCls->ptr_sub_list()->get_next();
         }
      }   
      swprintf(entry, ID_PROFILE_LEN, _T("%d"), ++i);
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_desel_all_area_class <external> */
/*+
  Questa funzione deseleziona tutte le classi selezionate per la
  prossima estrazione nella sessione di lavoro corrente.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_desel_all_area_class(void)
{
   C_CLASS  *pCls;
   int      i = 1, result = GS_GOOD;
   C_STRING pathfile;

   // verifico abilitazione
   if (gsc_check_op(opSelAreaClass) == GS_BAD) return GS_BAD;
   
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() == WRK_SESSION_FREEZE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   pCls = (C_CLASS *) GS_CURRENT_WRK_SESSION->get_pPrj()->ptr_classlist()->get_head();
   while (pCls != NULL)
   {                                      
      switch (pCls->ptr_id()->sel)
      {
         case SELECTED:
            pCls->ptr_id()->sel = DESELECTED;
	         // scrivo la selezione su file
            GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile, pCls->ptr_id()->code);
	         if (pCls->ToFile_id(pathfile) == GS_BAD) result = GS_BAD;
            break;
         case EXTR_SEL:
            pCls->ptr_id()->sel = EXTRACTED;
	         // scrivo la selezione su file
            GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile, pCls->ptr_id()->code);
	         if (pCls->ToFile_id(pathfile) == GS_BAD) result = GS_BAD;
            break;
         default: break;
      }
      if (result == GS_BAD) break;
      
      pCls = (C_CLASS *) GS_CURRENT_WRK_SESSION->get_pPrj()->ptr_classlist()->get_next();
   }

   return result;
}


/*****************************************************************************/
/*.doc gsc_freeze_area <external>                                            */
/*+
  Questa funzione congela la sessione di lavoro corrente del progetto corrente, 
  salvando i dati nella directory OLD della directory relativa all'area corrente.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int gsc_freeze_area(void)
{
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->freeze() == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/****************************************************************************/
/*.doc gsc_ExitCurrSession <external>                                                                             */
/*+
  Questa funzione esce dalla sessione di lavoro corrente. 
  In particolare se questa funzione era stata preceduta da una <C_WRK_SESSION::save>
  l'area viene eliminata, mentre se era stata preceduta da una <gsc_freeze_area>
  viene impostato lo status della sessione a WRK_SESSION_FREEZE.
  Parametri:
  int for_quit;      Flag, se = GS_GOOD la funzione � chiamata durante la notifica
                     di evento kQuitMsg o (default = GS_BAD)
  TCHAR *NextCmd;    Stringa che identifica una eventuale chiamata ad un comando
                     da lanciare a fine sessione (dopo il comando di "NEW") se
                     il parametro for_quit = GS_BAD; default = NULL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/****************************************************************************/
int gsc_ExitCurrSession(int for_quit, TCHAR *NextCmd)
{
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->exit(for_quit, NextCmd) == GS_BAD) return GS_BAD;
   delete GS_CURRENT_WRK_SESSION;
   GS_CURRENT_WRK_SESSION = NULL;

   // Se esiste il puntatore C_DYNAMIC_QRY_ATTR per l'interrogazione dinamica 
   if (GEOsimAppl::DYNAMIC_QRY_ATTR)
   {
      delete GEOsimAppl::DYNAMIC_QRY_ATTR;
      GEOsimAppl::DYNAMIC_QRY_ATTR = NULL;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_thaw_area <external>                                               */
/*+
  Questa funzione apre una sessione di lavoro congelata in funzione del codice 
  del progetto e del codice della sessione.

  Parametri
  int prj;        codice progetto
  long SessionId; codice sessione
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_thaw_area(int prj, long SessionId)
{  
   C_WRK_SESSION *pSession;

   // Cerco l'area
   if ((pSession = gsc_find_wrk_session(prj, SessionId)) == NULL) return GS_BAD;
   // Scongelo l'area
   if (pSession->thaw_1part() == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/************************************************************************************************************/
/*.doc (new 2) gsc_thaw_area2 <internal> */
/*+
  Questa funzione � di solo uso interno ed � utilizzata in <gsc_sel_area>.
  Parametri
  int prj;           Codice progetto
  long SessionId;    Codice sessione
  bool RecoverDwg;   Flag che indica se lo scongelamento � dovuto al recupero di
                     una sessione non congelata che era stata interrotta generando
                     un file di recover (default = false).
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_thaw_area2(int prj, long SessionId, bool RecoverDwg)
{
   C_WRK_SESSION *pSession;

   // Cerco l'area
   if ((pSession = gsc_find_wrk_session(prj, SessionId)) == NULL) return GS_BAD;
   // Scongelo l'area
   if (pSession->thaw_2part(RecoverDwg) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
                                   

/*********************************************************/
/*.doc gsc_extract_area_class <external> */
/*+
  Questa funzione estrae le classi selezionate nella sessione
  di lavoro corrente.
  Parametri:
  int mode;        modo estrazione
  
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gsc_extract_area_class(int mode)
{  
   C_INT_INT_LIST ErrClsCodeList;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->extract(mode, FALSE, &ErrClsCodeList) == GS_BAD ||
       ErrClsCodeList.get_count() > 0) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_retrieve_area <external> */
/*+
  Questa funzione recupera una sessione di lavoro. Requisito fondamentale:
  nella sessione di lavoro deve essere stato fatto almeno un congelamento
  Parametri:
  int prj;        codice progetto
  long SessionId;       codice sessione
  
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gsc_retrieve_area(int prj, long SessionId)
{  
   C_WRK_SESSION *pSession;
   C_STRING      DWGFile;
   bool          RecoverDWG = FALSE;
   
   // verifico abilitazione
   if (gsc_check_op(opSelArea) == GS_BAD) return GS_BAD;

   // Cerco l'area
   if ((pSession = gsc_find_wrk_session(prj, SessionId)) == NULL) return GS_BAD;
   if (pSession->get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // Verifico che esista il file "DRAW.DWG" nella sottocartella "OLD"
   // della cartella della sessione (quindi se � stato fatto un congelamento)
   DWGFile = pSession->get_dir();
   DWGFile += _T("\\");
   DWGFile += GEOOLDSESSIONDIR;
   DWGFile += _T("\\DRAW.DWG");
   if (gsc_path_exist(DWGFile) == GS_BAD)
   {
      int      res;
      C_STRING source_dir, dest_dir;

      // Forse Autocad si era piantato scrivendo un file di disegno temporaneo
      // 429 -> "La sessione non era stata congelata, AutoCAD Map aveva generato un file di disegno da ripristinare ?"
      // 645 -> "GEOsim - Seleziona file"
      if (gsc_ddgetconfirm(gsc_msg(429), &res) == GS_GOOD && res == GS_GOOD &&
          gsc_GetFileD(gsc_msg(645), _T("_recover.dwg"), _T("dwg"), 4, DWGFile) == RTNORM)
         RecoverDWG = TRUE;
      else
      {
         GS_ERR_COD = eGSOpNotAble;
         return GS_BAD;
      }

      source_dir = pSession->get_dir();
      source_dir += _T('\\');
      source_dir += GEOTEMPSESSIONDIR;

      // creo il direttorio OLD
      dest_dir = pSession->get_dir();
      dest_dir += _T('\\');
      dest_dir += GEOOLDSESSIONDIR;
      if (gsc_mkdir(dest_dir) == GS_BAD) return GS_BAD;

      // copio tutti i files del direttorio OLD nel direttorio TEMP della sessione
      if (gsc_copydir(source_dir.get_name(), _T("*.*"), dest_dir.get_name()) == GS_BAD) return GS_BAD;
      // Copio il file DWG
      dest_dir += _T("\\DRAW.DWG");
      if (gsc_copyfile(DWGFile, dest_dir) == GS_BAD) return GS_BAD;
   }

   pSession->set_status(WRK_SESSION_FREEZE); // la pongo virtualmente in stato WRK_SESSION_FREEZE

   // Scongelo l'area
   if (pSession->thaw_1part(RecoverDWG) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}

                                     
////////////////// INIZIO FUNZIONI C_WRK_SESSION_LIST ///////////////////


C_WRK_SESSION *C_WRK_SESSION_LIST::search_level(GSDataPermissionTypeEnum in)
{
   cursor = head;
   while (cursor != NULL)
   {
      if (((C_WRK_SESSION *) cursor)->get_level() == in) break;
      get_next();
   }
   return (C_WRK_SESSION *) cursor;
}   

/*********************************************************/
/*.doc (new 2) C_WRK_SESSION_LIST::ruserarea <external> */
/*+
  Questa funzione restituisce una lista delle sessioni di lavoro.
  Parametri:
  int user_code;    Codice utente
  int prj_code;     Codice progetto
  int state;        Stato delle aree (WRK_SESSION_ACTIVE, WRK_SESSION_FREEZE,
                    WRK_SESSION_WAIT, WRK_SESSION_SAVE, WRK_SESSION_EXTR) se state = -1  
                    restituisce la lista di tutte le aree dell'utente (default = -1).
  int OnlyOwner;    Se il flag = TRUE restituisce solo la lista delle proprie 
                    sessioni di lavoro aree altrimenti tutte (default = FALSE).
  int retest;       se MORETESTS -> in caso di errore riprova n volte 
                    con i tempi di attesa impostati poi ritorna GS_BAD,
                    ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                    (default = MORETESTS) Aggiunto per tapullo su selezione progetto
                    da DCL (che non si chiude correttamente e rimane inattiva senza
                    possibilit� di controllo) in modo che se questa funzione fallisse
                    non verrebbe aperta alcuna finestra di errore.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION_LIST::ruserarea(int user_code, int prj_code, int state, int OnlyOwner,
                                  int retest)
{
   C_PROJECT      *pPrj;
   C_STRING       statement, TableRef;
   C_WRK_SESSION  *pWrkSession;
   C_RB_LIST      ColValues;
   C_DBCONNECTION *pDBConn;
   _RecordsetPtr  pRs;
   
   remove_all();
   
   if (gsc_superuser() == GS_BAD)  // non � un SUPER USER 
      // se non � "loggato" nessuno o l'utente corrente non sta cercando le sue areee
      if (user_code != GEOsimAppl::GS_USER.code || GEOsimAppl::GS_USER.code == 0)
         { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj_code)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }
   
   // Setto il riferimento di WRKSESSIONS_TABLE_NAME (<catalogo>.<schema>.<tabella>)
   if (pPrj->getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;

   statement = _T("SELECT * FROM ");
   statement += TableRef;

   if (state != -1) // elenco filtrato per stato area
   {
      statement += _T(" WHERE STATUS=");
      statement += state;

      if (OnlyOwner == TRUE) // elenco filtrato per utente
      {
         statement += _T(" AND USR=");
         statement += user_code;
      }
   }
   else
      if (OnlyOwner == TRUE) // elenco filtrato per utente
      {
         statement += _T(" WHERE USR=");
         statement += user_code;
      }

   // leggo le righe della tabella in ordine di GS_ID senza bloccarla
   if (pDBConn->ExeCmd(statement, pRs) == GS_BAD) return GS_BAD;
          
   // scorro l'elenco delle aree
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      // lettura con conversione date
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) 
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      
      if ((pWrkSession = new C_WRK_SESSION(pPrj)) == NULL)
      {
         gsc_DBCloseRs(pRs); 
         GS_ERR_COD = eGSOutOfMem; 
         return GS_BAD; 
      }

      if (pWrkSession->from_rb_db(ColValues) == GS_BAD)
      {
         delete pWrkSession; 
         gsc_DBCloseRs(pRs);
         return GS_BAD; 
      }

      add_tail(pWrkSession);
      gsc_Skip(pRs);
   }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

   // ordino per nome area con sensitive = FALSE
   sort_name();

   return GS_GOOD;
}   


/*********************************************************/
/*.doc C_WRK_SESSION_LIST::rarea <external> */
/*+
  Questa funzione restituisce una lista delle aree.
  Parametri:
  int prj_code;     Codice progetto
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION_LIST::rarea(int prj_code)
{  
   C_PROJECT      *pPrj;
   C_STRING       statement, TableRef;
   C_WRK_SESSION         *pWrkSession;
   C_RB_LIST      ColValues;
   C_DBCONNECTION *pDBConn;
   _RecordsetPtr  pRs;
   
   remove_all();

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj_code)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }
   
   // Setto il riferimento di WRKSESSIONS_TABLE_NAME (<catalogo>.<schema>.<tabella>)
   if (pPrj->getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
         
   // leggo le righe della tabella senza bloccarla
   if (pDBConn->ExeCmd(statement, pRs) == GS_BAD) return GS_BAD;

   // scorro l'elenco delle aree
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      // lettura con conversione date
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) 
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      if ((pWrkSession = new C_WRK_SESSION(pPrj)) == NULL)
      { 
         gsc_DBCloseRs(pRs);
         GS_ERR_COD = eGSOutOfMem; 
         return GS_BAD; 
      }

      if (pWrkSession->from_rb_db(ColValues) == GS_BAD)
      {
         delete pWrkSession; 
         gsc_DBCloseRs(pRs);
         return GS_BAD; 
      }

      add_tail(pWrkSession);
      gsc_Skip(pRs);
   }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

   // ordino per nome area con sensitive = FALSE
   sort_name();
   
   return GS_GOOD;
}            
                                   

/*********************************************************/
/*.doc  C_WRK_SESSION_LIST::to_rb <external> */
/*+
  Questa funzione restituisce una lista resbuf 
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
presbuf C_WRK_SESSION_LIST::to_rb(void)
{
   presbuf       p, lista=NULL;
   C_WRK_SESSION *pSession;

   pSession = (C_WRK_SESSION *) get_head();

   while (pSession)
   {
      if (!lista)
      {
         if (( p = lista = acutBuildList(RTLB, 0)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return NULL; }
      }
      else
      {
         if ((p->rbnext = acutBuildList(RTLB, 0)) == NULL)
            { free(lista); GS_ERR_COD = eGSOutOfMem; return NULL; }
         p = p->rbnext;
      }
      if ((p->rbnext = pSession->to_rb()) == NULL)
         { free(lista); return NULL; }
      while (p->rbnext != NULL) p = p->rbnext;

      if ((p->rbnext = acutBuildList(RTLE, 0)) == NULL)
         { free(lista); GS_ERR_COD = eGSOutOfMem; return NULL; }
      pSession = (C_WRK_SESSION *) get_next();
      p = p->rbnext;
   }
      
   return lista;                       
} 


/////////////////////////////////////////////////////////////
///////////        INIZIO FUNZIONI C_WRK_SESSION         ///////////
/////////////////////////////////////////////////////////////


// costruttore
C_WRK_SESSION::C_WRK_SESSION(C_PROJECT *_pPrj) : C_NODE()
{ 
   pPrj    = _pPrj;
   code    = 0;
   name[0] = _T('\0');
   level   = GSReadOnlyData;
   usr     = 0;
   status  = WRK_SESSION_WAIT;
   gsc_strcpy(coordinate, DEFAULT_DWGCOORD, MAX_LEN_COORDNAME);
   SRID_unit = GSSRIDUnit_None;
   pConn   = NULL;
   pLS     = NULL;
   isRsSelectLinkSupported = -1; // non ancora inizializzato
}

// distruttore
C_WRK_SESSION::~C_WRK_SESSION()
{
   TerminateSQL();
   Term_pLS();
   if (pPrj) pPrj->TerminateSQL();
}

int C_WRK_SESSION::copy(C_WRK_SESSION *out)        
{ 
   out->pPrj = pPrj;
   out->code = code;
   gsc_strcpy(out->name, name, MAX_LEN_WRKSESSION_NAME);
   out->dir    = dir;
   out->level  = level;
   out->usr    = usr;
   out->status = status;
   gsc_strcpy(out->coordinate, coordinate, MAX_LEN_COORDNAME);
   out->CreationDateTime = CreationDateTime;
   ClsCodeList.copy(&(out->ClsCodeList));
   out->pConn = pConn;

   return GS_GOOD; 
}

int C_WRK_SESSION::get_PrjId() { return (pPrj) ? pPrj->get_key() : 0; }
C_PROJECT *C_WRK_SESSION::get_pPrj() { return pPrj; }
C_CLASS *C_WRK_SESSION::find_class(int cls, int sub)
   { return (get_pPrj()) ? get_pPrj()->find_class(cls, sub) : NULL; }
C_CLASS *C_WRK_SESSION::find_class(ads_name ent)
   { return (get_pPrj()) ? get_pPrj()->find_class(ent) : NULL; }

long C_WRK_SESSION::get_id() { return code; }

int C_WRK_SESSION::set_id(long in)
{
   code = in;
   return GS_GOOD;
}

TCHAR *C_WRK_SESSION::get_name() { return name; }

TCHAR *C_WRK_SESSION::get_dir() { return dir.get_name(); }

GSDataPermissionTypeEnum C_WRK_SESSION::get_level() { return level; }

int C_WRK_SESSION::set_level(GSDataPermissionTypeEnum in)
{
   level = in; 
   return GS_GOOD; 
}

int C_WRK_SESSION::get_usr() { return usr; }

int C_WRK_SESSION::set_usr(int in)           
{ 
   usr = in; 
   return GS_GOOD; 
}

int C_WRK_SESSION::get_status() { return status; }


/*****************************************************************************/
/*.doc C_WRK_SESSION::set_status                                         <internal> */
/*+
  Questa funzione setta lo stato della sessione.
  Parametri:
  int in;         Nuovo stato (WRK_SESSION_ACTIVE, WRK_SESSION_FREEZE,
                  WRK_SESSION_WAIT, WRK_SESSION_SAVE, WRK_SESSION_EXTR)
  int WriteDB;    Se = GS_GOOD la funzione scrive lo stato anche nel database
                  altrimenti solo in memoria (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B.: Tutti i link Path Name vengono automaticamente cancellati da ACAD.
-*/  
/*****************************************************************************/
int C_WRK_SESSION::set_status(int in, int WriteDB)
{ 
   C_DBCONNECTION *pDBConn;
   C_STRING       statement, TableRef;

   if (in != WRK_SESSION_ACTIVE && in != WRK_SESSION_FREEZE &&
       in != WRK_SESSION_WAIT && in != WRK_SESSION_SAVE &&
       in != WRK_SESSION_EXTR)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (WriteDB != GS_GOOD) 
   {
      status = in; 
      return GS_GOOD;
   }

   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }

   if (pPrj->getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("UPDATE ");
   statement += TableRef;
   statement += _T(" SET STATUS=");
   statement += in;
   statement += _T(" WHERE GS_ID=");
   statement += code;

   if (pDBConn->ExeCmd(statement) == GS_BAD)
   {
      if (GS_ERR_COD == eGSLocked) GS_ERR_COD = eGSLockBySession;
      return GS_BAD;
   }
        
   status = in; 

   return GS_GOOD; 
}

TCHAR *C_WRK_SESSION::get_coordinate() { return coordinate; }

int C_WRK_SESSION::set_coordinate(const TCHAR *SessionCoord)
{
   if (gsc_validcoord(SessionCoord) == GS_BAD) return GS_BAD;
   
   gsc_strcpy(coordinate, SessionCoord, MAX_LEN_COORDNAME);
   gsc_alltrim(coordinate);
   gsc_toupper(coordinate);

   return GS_GOOD;
}

/*****************************************************************************/
/*.doc C_WRK_SESSION::get_SRID_unit                                <external> */
/*+
  Questa funzione restituisce l'unita del sistema di coordinate dello SRID della sessione.
-*/  
/*****************************************************************************/
GSSRIDUnitEnum C_WRK_SESSION::get_SRID_unit(void)
{
   if (SRID_unit == GSSRIDUnit_None)
      // Se � stato dichiarato un sistema di coordinate per la sessione corrente
      if (gsc_strlen(coordinate) > 0)
      {
         C_STRING dummy;

         if (gsc_get_srid(coordinate, GSSRID_Autodesk, GSSRID_Autodesk, dummy, &SRID_unit) != GS_GOOD)
            SRID_unit = GSSRIDUnit_None;
      }

   return SRID_unit;
}

TCHAR *C_WRK_SESSION::get_KeyDwg() { return KeyDwg.get_name(); }

int C_WRK_SESSION::set_KeyDwg(const TCHAR *SessionKeyDwg)
{
   KeyDwg = SessionKeyDwg;
   KeyDwg.alltrim();

   return GS_GOOD;
}

TCHAR *C_WRK_SESSION::get_CreationDateTime(void) { return CreationDateTime.get_name(); }


/*********************************************************/
/*.doc  C_WRK_SESSION::get_pLS                                  */
/*+
  Questa funzione restituisce il percorso completo del file temporaneo
  di informazioni interne della sessione di lavoro.
  parametri:
  C_STRING &FilePath; pecorso completo del file (out)
  int Cls;            Opzionale, codice classe se il file si riferisce ad
                      una classe di entit� (default = 0)

  Ritorna GS_GOOD inn caso di successo altrimento GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::get_TempInfoFilePath(C_STRING &FilePath, int Cls)
{
   FilePath = get_dir();
   FilePath += _T('\\');
   FilePath += GEOTEMPSESSIONDIR;
   FilePath += _T('\\');
   FilePath += GS_CLASS_FILE;
   if (Cls > 0)
   {
      C_STRING drive, dir, name, ext;

      gsc_splitpath(FilePath, &drive, &dir, &name, &ext);
      FilePath = drive;
      FilePath += dir;
      FilePath += name;
      FilePath += Cls;
      FilePath += ext;
   }

   return GS_GOOD;
}

C_DBCONNECTION *C_WRK_SESSION::getDBConnection(void) 
{ 
   if (!pConn)
   {
      C_STRING Catalog;

      Catalog = dir;
      Catalog += _T('\\');
      Catalog += GEOTEMPSESSIONDIR;
      Catalog += _T('\\');
      Catalog += GEOWRKSESSIONDB;
      C_2STR_LIST UDLProp(_T("Data Source"), Catalog.get_name());

      pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &UDLProp);
   }

   return pConn; 
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_pLS                           */
/*+
  Questa funzione inizializza il LS da usare nella sessione di lavoro.

  Ritorna il puntatore al LS
-*/  
/*********************************************************/
CAseLinkSel *C_WRK_SESSION::get_pLS(void)
{
   if (!pLS)
   {
      // GEOsimAppl::ASE deve essere gi� inizializzato
      if ((pLS = new CAseLinkSel(GEOsimAppl::ASE)) == NULL)
         { GS_ERR_COD = eGSInvalidLS; return NULL; }
   }

   return pLS;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::Term_pLS                                  */
/*+
  Questa funzione termina il LS da usare nella sessione di lavoro.
-*/  
/*********************************************************/
void C_WRK_SESSION::Term_pLS(void)
{
   if (pLS)
   {
      delete pLS;
      pLS = NULL;
   }
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_pCacheClsAttribValuesList     */
/*+
  Questa funzione restituisce il puntatore alla cache dei valori 
  degli attributi delle classi letti da TAB e REF.
-*/  
/*********************************************************/
C_CACHE_CLS_ATTRIB_VALUES_LIST *C_WRK_SESSION::get_pCacheClsAttribValuesList(void)
{
   return &CacheClsAttribValuesList;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_pClsCodeList                 */
/*+
  Questa funzione restituisce il puntatore alla lista delle
  classi usate nella sessione.
-*/  
/*********************************************************/
C_INT_LIST *C_WRK_SESSION::get_pClsCodeList(void)
{
   return &ClsCodeList;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_SQLParameters                 */
/*+
  Questa funzione restituisce il puntatore alla lista dei
  parametri SQL uati per l'estrazione dei dati.
-*/  
/*********************************************************/
C_STRING *C_WRK_SESSION::get_SQLParameters(void)
{
   return SQLParameters;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_CmdSelectLinkWhereCls                */
/*+
  Questa funzione restituisce il comando SQL per la ricerca di link
  di una classe.
-*/  
/*********************************************************/
_CommandPtr C_WRK_SESSION::get_CmdSelectLinkWhereCls(void)
{
   if (CmdSelectLinkWhereCls.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pDBConn;
      C_STRING       TableRef;
      
      if (getLinksTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;
      if (gsc_PrepareSelLinkWhereCls(pDBConn, TableRef,
                                     CmdSelectLinkWhereCls) == GS_BAD) return NULL;
   }

   return CmdSelectLinkWhereCls;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_CmdSelectLinkWhereKey                */
/*+
  Questa funzione restituisce il comando SQL per la ricerca di link
  di un record di una classe.
-*/  
/*********************************************************/
_CommandPtr C_WRK_SESSION::get_CmdSelectLinkWhereKey(void)
{
   if (CmdSelectLinkWhereKey.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pDBConn;
      C_STRING       TableRef;
      
      if (getLinksTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;
      if (gsc_PrepareSelLinkWhereKey(pDBConn, TableRef,
                                     CmdSelectLinkWhereKey) == GS_BAD) return NULL;
   }

   return CmdSelectLinkWhereKey;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_CmdSelectLinkWhereHandle             */
/*+
  Questa funzione restituisce il comando SQL per la ricerca di link
  di una entit� grafica di una classe.
-*/  
/*********************************************************/
_CommandPtr C_WRK_SESSION::get_CmdSelectLinkWhereHandle(void)
{
   if (CmdSelectLinkWhereHandle.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pDBConn;
      C_STRING       TableRef;
      
      if (getLinksTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;
      if (gsc_PrepareSelLinkWhereHandle(pDBConn, TableRef,
                                        CmdSelectLinkWhereHandle) == GS_BAD) return NULL;
   }

   return CmdSelectLinkWhereHandle;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_RsInsertLink                         */
/*+
  Questa funzione restituisce un recordset per l'inserimento di link.
-*/  
/*********************************************************/
_RecordsetPtr C_WRK_SESSION::get_RsInsertLink(void)
{
   if (RsInsertLink.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pDBConn;
      C_STRING       TableRef;
      
      if (getLinksTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;
      if (pDBConn->InitInsRow(TableRef.get_name(), RsInsertLink) == GS_BAD) return NULL;
   }

   return RsInsertLink;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_RsSelectLink                         */
/*+
  Questa funzione restituisce un recordset set per la ricerca di link.
-*/  
/*********************************************************/
_RecordsetPtr C_WRK_SESSION::get_RsSelectLink(void)
{
   if (isRsSelectLinkSupported == -1) // non ancora inizializzato
   {
      if (RsSelectLink.GetInterfacePtr() == NULL)
      {
         C_DBCONNECTION *pDBConn;
         C_STRING       TableRef;
         bool           RsOpened = false;
      
         // istanzio un recordset
         if (FAILED(RsSelectLink.CreateInstance(__uuidof(Recordset))))
            { GS_ERR_COD = eGSOutOfMem; return NULL; }

         if (getLinksTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;

	      try
         {  
            RsSelectLink->CursorLocation = adUseServer;
            // apro il recordset
            RsSelectLink->Open(_T("GS_LINK"), pDBConn->get_Connection().GetInterfacePtr(), 
                               adOpenDynamic, adLockOptimistic, adCmdTableDirect);
            RsOpened = true;
            
            // roby per baco di Jet il seek sfonda su tabelle modificate
            //if (RsSelectLink->Supports(adIndex) && RsSelectLink->Supports(adSeek))
            //   isRsSelectLinkSupported = 1; // supportato
            //else
            {
               isRsSelectLinkSupported = 0; // non supportato
               gsc_DBCloseRs(RsSelectLink); 
            }
         }

	      catch (_com_error)
	      {
            isRsSelectLinkSupported = 0; // non supportato
            if (RsOpened) gsc_DBCloseRs(RsSelectLink);
            else RsSelectLink.Release();
         }
      }
   }
   else // gi� inizializzato
      if (RsSelectLink.GetInterfacePtr() != NULL) // se esisteva il recordset
         if (gsc_DBIsClosedRs(RsSelectLink)) // se � stato chiuso (baco di Jet)
         {
            gsc_DBCloseRs(RsSelectLink); // Cerco di chiuderlo e rilasciarlo regolarmente
            isRsSelectLinkSupported = 0; // non supportato
         }

   return (isRsSelectLinkSupported == 1) ? RsSelectLink : NULL;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::get_CmdInsertDeleted                     */
/*+
  Questa funzione restituisce un record set per l'inserimento di 
  entit� cancellate.
-*/  
/*********************************************************/
_RecordsetPtr C_WRK_SESSION::get_RsInsertDeleted(void)
{
   if (RsInsertDeleted.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pDBConn;
      C_STRING       TableRef;
      
      if (getDeletedTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;
      if (pDBConn->InitInsRow(TableRef.get_name(), RsInsertDeleted) == GS_BAD) return NULL;
   }

   return RsInsertDeleted;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::set_name                                 */
/*+
  Questa funzione setta il nome della sessione
    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::set_name(const TCHAR *in)
{
   if (in != NULL && (gsc_strlen(in) < MAX_LEN_WRKSESSION_NAME))
      gsc_strcpy(name, in, MAX_LEN_WRKSESSION_NAME);
   else
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::set_dir                                 */
/*+
  Questa funzione setta il direttorio della sessione
    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::set_dir(const TCHAR *in)
{
   if (!in) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   
   dir = in;
   dir.alltrim();
   if (gsc_validdir(dir) == GS_BAD) { dir.clear(); return GS_BAD; }
   return GS_GOOD;
}      


/*****************************************************************************/
/*.doc  C_WRK_SESSION::isReadyToUpd                                                 */
/*+
  Restituisce GS_GOOD nel caso in cui l'area sia attiva e aggiornabile.
-*/  
/*****************************************************************************/
int C_WRK_SESSION::isReadyToUpd(int *WhyNot)
{
   if (get_status() != WRK_SESSION_ACTIVE && get_status() != WRK_SESSION_EXTR &&
       get_status() != WRK_SESSION_SAVE)
      { if (WhyNot) *WhyNot = eGSInvSessionStatus; return GS_BAD; }

   if (get_level() != GSUpdateableData)
      { if (WhyNot) *WhyNot = eGSReadOnlySession; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc  C_WRK_SESSION::to_rb <external> */
/*+
  Questa funzione restituisce una lista resbuf
  Restituisce una lista resbuf:
  <code><nome><dir><type><status><usr><coordinate><data/ora creazione>(<lista classi>)
  dove:
  (<lista classi>) = lista di codici delle classi usate nella sessione (<cls 1><cls 2>...<cls n>)
    
  Restituisce il puntatore alla lista in caso di successo altrimenti 
  restituisce NULL.
-*/  
/*********************************************************/
presbuf C_WRK_SESSION::to_rb(void)
{
   C_RB_LIST List;
   
   if ((List << acutBuildList(RTREAL, (double) code,
                              RTSTR,   name,
                              RTSTR,   (dir.len() == 0) ? GS_EMPTYSTR : dir.get_name(),
                              RTSHORT, (int) level,
                              RTSHORT, status,
                              RTSHORT, usr,
                              RTSTR,   coordinate,
                              RTSTR,   (CreationDateTime.len() == 0) ? GS_EMPTYSTR : CreationDateTime.get_name(),
                              0)) == NULL)
      return NULL;
   if ((List += ClsCodeList.to_rb()) == NULL) return NULL;
   List.ReleaseAllAtDistruction(GS_BAD);

   return List.get_head();                       
} 


/*********************************************************/
/*.doc C_WRK_SESSION::get_class_list <external> */
/*+
  Questa funzione restituisce una lista delle classi (che sono visibili
  all'utente corrente) nella sessione di lavoro.
  (<code><name><level><cat><type><sub_list><sel>)
  
  se category = CAT_EXTERN
     sub_list = (<sub_code><name><level><cat><type>nil<sel>)
  altrimenti 
     sub_list = nil

  Parametri:
  C_AREACLASS_LIST &lista;          Lista classi
  bool             OnlyExtracted;   Se true la lista comprende solo le classi
                                    gi� estratte nella sessione (default = true).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::get_class_list(C_AREACLASS_LIST &lista, bool OnlyExtracted)
{
   C_AREACLASS *p_class, *p_sub;
   
   lista.remove_all();
   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }
               
   // Se si tratta della sessione corrente
   if (GS_CURRENT_WRK_SESSION && gsc_strcmp(GS_CURRENT_WRK_SESSION->get_dir(), get_dir()) == 0)
   { // classi gi� caricate in memoria
      C_CLASS *pclass, *pSub;
      C_ID    *p_id;

      pclass = (C_CLASS *) pPrj->ptr_classlist()->get_head();
      while (pclass)
      {
	      p_id = pclass->ptr_id();

         // Salto le classi deselezionate
         if (p_id->sel == DESELECTED)
            { pclass = (C_CLASS *) pPrj->ptr_classlist()->get_next(); continue; }

         // Salto le classi non ancora estratte
         if (OnlyExtracted)
            if (p_id->sel != EXTRACTED && p_id->sel != EXTR_SEL)
               { pclass = (C_CLASS *) pPrj->ptr_classlist()->get_next(); continue; }

         if ((p_class = new C_AREACLASS) == NULL) 
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         if (p_class->set(p_id->code, p_id->name,
                           p_id->abilit, p_id->category,
                           p_id->type, p_id->sel) == GS_BAD)
            { delete p_class; return GS_BAD; }
            if (lista.add_tail(p_class) == GS_BAD)
            { delete p_class; return GS_BAD; }
         if (p_id->category == CAT_EXTERN)
         {
            pSub = (C_CLASS *) pclass->ptr_sub_list()->get_head();
            while (pSub != NULL)
            {
	            p_id = pSub->ptr_id();
               if ((p_sub = new C_AREACLASS) == NULL) 
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
               if (p_sub->set(p_id->sub_code, p_id->name,
                              p_id->abilit, p_id->category,
                              p_id->type, p_id->sel) == GS_BAD)
                  { delete p_sub; return GS_BAD; }
               if (p_class->ptr_sub_list()->add_tail(p_sub) == GS_BAD)
                  { delete p_class; return GS_BAD; }
               pSub = (C_CLASS *) pclass->ptr_sub_list()->get_next();
            }
         }

         pclass = (C_CLASS *) pPrj->ptr_classlist()->get_next();
      }
   }
   else                                
   { // leggo da file
      C_STRING pathfile;
      TCHAR    entry[ID_PROFILE_LEN];
      int      cls, i = 1, prev_GS_ERR_COD = GS_ERR_COD;
      C_ID     id;
      C_PROFILE_SECTION_BTREE ProfileSections;
      C_BPROFILE_SECTION      *ProfileSection;
      C_B2STR                 *pProfileEntry;

      get_TempInfoFilePath(pathfile);
      if (gsc_read_profile(pathfile, ProfileSections) == GS_BAD)
         if (gsc_drive_exist(pathfile.get_name()) == GS_GOOD)
            return GS_GOOD; // la macchina era accesa (il file � stato cancellato)
         else
            return GS_BAD; // la macchina era spenta o sconnessa
   
      swprintf(entry, ID_PROFILE_LEN, _T("%d"), i);
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(CLASSES_LIST_SEZ);
      while (ProfileSection && (pProfileEntry = (C_B2STR *) ProfileSection->get_ptr_EntryList()->search(entry)))
      {
         swscanf(pProfileEntry->get_name2(), _T("%d"), &cls);      
         swprintf(entry, ID_PROFILE_LEN, _T("%d.0"), cls);

         get_TempInfoFilePath(pathfile, cls);
         if (id.load(pathfile, entry) == GS_BAD) return GS_BAD;

         // Salto le classi deselezionate
         if (id.sel == DESELECTED) continue;

         // Salto le classi non ancora estratte
         if (OnlyExtracted)
            if (id.sel != EXTRACTED && id.sel != EXTR_SEL) continue;

         if ((p_class = new C_AREACLASS) == NULL) 
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         lista.add_tail(p_class);
            
         if (p_class->set(id.code, id.name, id.abilit, id.category,
                          id.type, id.sel) == GS_BAD)
            return GS_BAD;

         // Incremento entry
         swprintf(entry, ID_PROFILE_LEN, _T("%d"), ++i);
      }
      GS_ERR_COD = prev_GS_ERR_COD;

      // Se non si tratta di superutente devo filtrare solo le classi
      // visibili all'utente corrente
      if (gsc_superuser() == GS_BAD)
      {
         C_INT_INT_LIST ClsAbilitList;
         C_INT_INT      *pClsAbilit;

         // Leggo le abilitazioni alle classi
         if (gsc_getClassPermissions(GEOsimAppl::GS_USER.code, pPrj->get_key(), ClsAbilitList) == GS_BAD)
            return GS_BAD;

         p_class = (C_AREACLASS *) lista.get_head();
         while (p_class)
            // Se non � una classe abilitata in visib o modifica
            if (!(pClsAbilit = (C_INT_INT *) ClsAbilitList.search_key(p_class->get_key())) || 
                pClsAbilit->get_type() == (int) GSInvisibleData) // salto la classe se invisibile
            {
               lista.remove_at(); // cancella e va al successivo
               p_class = (C_AREACLASS *) lista.get_cursor();
            }
            else
               p_class = (C_AREACLASS *) lista.get_next();
      }
   }
 
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_WRK_SESSION::load_classes <external> */
/*+
  Questa funzione carica la lista delle classi nella sessione di lavoro inoltre
  ripristina le strutture di appoggio alla lista degli attributi.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::load_classes(void)
{                                                       
   int              cls, sub, sec, i = 1;
   C_STRING         pathfile;
   TCHAR            entry[ID_PROFILE_LEN];
   C_ID	           id;
   C_CLASS          *pclass;
   C_SECONDARY      *pSec;
   C_SECONDARY_LIST *plist_sec;
   C_DBCONNECTION   *pConn;
   C_PROFILE_SECTION_BTREE ProfileSections, ClsProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_B2STR                 *pProfileEntry;
   
   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }

   get_TempInfoFilePath(pathfile);
   if (gsc_read_profile(pathfile, ProfileSections) == GS_BAD) return GS_BAD;

   // carico definizioni classi
   swprintf(entry, ID_PROFILE_LEN, _T("%d"), i);
   ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(CLASSES_LIST_SEZ);
   while (ProfileSection && (pProfileEntry = (C_B2STR *) ProfileSection->get_ptr_EntryList()->search(entry)))
   {
      swscanf(pProfileEntry->get_name2(), _T("%d"), &cls);      
      swprintf(entry, ID_PROFILE_LEN, _T("%d.0"), cls);

      get_TempInfoFilePath(pathfile, cls);
      if (gsc_read_profile(pathfile, ClsProfileSections) == GS_BAD) return GS_BAD;
      if (id.load(ClsProfileSections, entry) == GS_BAD) return GS_BAD;
                            
      // verifico se la classe � gi� stata caricata
      if ((pclass = (C_CLASS *) pPrj->ptr_classlist()->C_LIST::search_key(id.code)) == NULL)
      {
         if ((pclass = gsc_alloc_new_class(id.category)) == NULL) return GS_BAD;
         pPrj->ptr_classlist()->add_tail(pclass); // inserimento in lista
      }

	  if (pclass->load(ClsProfileSections, id.code) == GS_BAD) return GS_BAD;
      pclass->ptr_id()->pPrj = pPrj;

      // inizializzo i tipi di dato ADO per gli attributi
      if (pclass->get_category() == CAT_EXTERN)
      {
         C_SUB *pSub = (C_SUB *) pclass->ptr_sub_list()->get_head();

         while (pSub)
         {
            pSub->ptr_id()->pPrj = pPrj;
            // inizializzo i tipi di dato ADO per gli attributi della sotto-classe
            if ((pConn = pSub->ptr_info()->getDBConnection(OLD)) == NULL ||
                pSub->ptr_attrib_list()->init_ADOType(pConn) == GS_BAD)
               return GS_BAD;
            
            pSub = (C_SUB *) pSub->get_next();
         }
      }
      else
         if (pclass->ptr_attrib_list())
            // inizializzo i tipi di dato ADO per gli attributi della classe
            if ((pConn = pclass->ptr_info()->getDBConnection(OLD)) == NULL ||
                pclass->ptr_attrib_list()->init_ADOType(pConn) == GS_BAD)
               return GS_BAD;

      // Incremento entry
      swprintf(entry, ID_PROFILE_LEN, _T("%d"), ++i);
   }

   // carico definizioni tabelle secondarie
   if ((ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(SECONDARIES_LIST_SEZ)))
   {
      i = 1;
      swprintf(entry, ID_PROFILE_LEN, _T("%d"), i);
      while ((pProfileEntry = (C_B2STR *) ProfileSection->get_ptr_EntryList()->search(entry)))
      {
         swscanf(pProfileEntry->get_name2(), _T("%d,%d,%d"), &cls, &sub, &sec);      

         // Ritorna il puntatore alla classe cercata
         if ((pclass = pPrj->find_class(cls, sub)) == NULL) return GS_BAD;

         // lista tabelle secondarie
         if ((plist_sec = (C_SECONDARY_LIST *) pclass->ptr_secondary_list) == NULL)
            { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

         // verifico se la tabella secondaria � gi� stata caricata
         if ((pSec = (C_SECONDARY *) plist_sec->C_LIST::search_key(sec)) == NULL)
         {
            if ((pSec = new C_SECONDARY) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            plist_sec->add_tail(pSec); // inserimento in lista
         }

         get_TempInfoFilePath(pathfile, cls);
         if (gsc_read_profile(pathfile, ClsProfileSections) == GS_BAD) return GS_BAD;
         if (pSec->load(ClsProfileSections, pclass, sec) == GS_BAD) return GS_BAD;

         swprintf(entry, ID_PROFILE_LEN, _T("%d"), ++i);
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_WRK_SESSION::get_spatial_area_list          <external> */
/*+
  Questa funzione restituisce una lista delle aree spaziali 
  in cui sono state estratte le classi della sessione di lavoro.
  Si tratta di rettangoli che racchiudono le zone estratte.
  Parametri:
  C_RECT_LIST &SpatialAreaList;  Lista rettangoli delle aree

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::get_spatial_area_list(C_RECT_LIST &SpatialAreaList)
{
   C_STRING    pathfile;
   C_RECT      *pRect;
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_B2STR                 *pProfileEntry;

   SpatialAreaList.remove_all();
   get_TempInfoFilePath(pathfile);

   if (gsc_read_profile(pathfile, ProfileSections) == GS_BAD) return GS_GOOD;

   ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(WRK_SESSIONS_AREAS_SEZ);
   if (!ProfileSection) return GS_GOOD;
   pProfileEntry = (C_B2STR *) ProfileSection->get_ptr_EntryList()->go_top();
   while (pProfileEntry)
   {
      if ((pRect = new C_RECT(pProfileEntry->get_name2())) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      SpatialAreaList.add_tail(pRect);

      pProfileEntry = (C_B2STR *) ProfileSection->get_ptr_EntryList()->go_next();
   }

   return GS_GOOD;
}                                   

/*********************************************************/
/*.doc  C_WRK_SESSION::from_rb <external> */
/*+
  Questa funzione inizializza un'area da una lista resbuf.
  Parametri:
  resbuf *rb; Lista (<codice>[<nome>[<dir>[<type>[<usr>[<status>[<coordinate>[<key_dwg>]]]]]]]
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::from_rb(resbuf *rb)
{
   if (rb == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (rb->restype != RTLB)
      if (rb->restype == RTNIL) return GS_GOOD;
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   // code
   if ((rb = rb->rbnext) == NULL || rb->restype == RTLE) return GS_GOOD;
   if (gsc_rb2Lng(rb, &code) == GS_BAD)
      if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   // name
   if ((rb = rb->rbnext) == NULL || rb->restype == RTLE) return GS_GOOD;
   if (rb->restype == RTSTR)
   {
      if (set_name(rb->resval.rstring) == GS_BAD) return GS_BAD;
   }
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   // dir
   if ((rb = rb->rbnext) == NULL || rb->restype == RTLE) return GS_GOOD;
   if (rb->restype == RTSTR)
   {
      if (set_dir(rb->resval.rstring) == GS_BAD) return GS_BAD;
   }
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   // level
   if ((rb = rb->rbnext) == NULL || rb->restype == RTLE) return GS_GOOD;
   if (rb->restype == RTSHORT) level = (GSDataPermissionTypeEnum) rb->resval.rint;
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   // usr
   if ((rb = rb->rbnext) == NULL || rb->restype == RTLE) return GS_GOOD;
   if (rb->restype == RTSHORT) usr = rb->resval.rint;  
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   // status
   if ((rb = rb->rbnext) == NULL || rb->restype == RTLE) return GS_GOOD;
   if (rb->restype == RTSHORT) status = rb->resval.rint;     
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   // coordinate
   if ((rb = rb->rbnext) == NULL || rb->restype == RTLE) return GS_GOOD;
   if (rb->restype == RTSTR)
   {
      if (set_coordinate(rb->resval.rstring) == GS_BAD) return GS_BAD;
   }
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   // key_dwg
   if ((rb = rb->rbnext) == NULL || rb->restype == RTLE) return GS_GOOD;
   if (rb->restype == RTSTR)
   {
      if (set_KeyDwg(rb->resval.rstring) == GS_BAD) return GS_BAD;
   }
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   // non setto la data e l'ora di creazione (solo la "C_WRK_SESSION::create" li pu� settare)
   // non setto la lista delle classi (solo la "C_WRK_SESSION::extract" la pu� settare)

   return GS_GOOD;
} 
   
  
/*********************************************************/
/*.doc C_WRK_SESSION::create <internal> */
/*+
  Questa funzione crea una sessione di lavoro nei database.
  
  Restituisce il codice della sessione creata in caso di successo altrimenti
  restituisce 0.
-*/  
/*********************************************************/
long C_WRK_SESSION::create(void)
{
   C_RB_LIST     DescrArea;
   presbuf       p;
   C_STRING      pathfile, SourceDB;
   int           file, result = GS_BAD;
   long          new_code;
   C_INT_INT_STR usr;

   // verifico abilitazione
   if (gsc_check_op(opCreateArea) == GS_BAD) return 0;
   if (GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSSessionsFound; return 0; }

   // setto la data e l'ora di creazione
   gsc_current_DateTime(CreationDateTime);

   // setto il proprietario (l'utente corrente)
   if (gsc_whoami(&usr) == GS_BAD || set_usr(usr.get_key()) == GS_BAD) return 0;

   // verifico correttezza valori
   if (is_valid() == GS_BAD) return 0;
   // verifico se il direttorio gi� esisteva
   if (gsc_path_exist(dir) == GS_GOOD) { GS_ERR_COD = eGSPathAlreadyExisting; return 0; }

   // apro il file degli utenti bloccandolo agli altri (semaforo)
   if ((file = gsc_open_geopwd()) == -1) { GS_ERR_COD = eGSOpNotAble; return 0; }

   do
   {
      if ((DescrArea << acutBuildList(RTLB, 0)) == NULL) break;
      if (to_rb_db(&p) == GS_BAD) break;
      DescrArea += p;
      if ((DescrArea += acutBuildList(RTLE, 0)) == NULL) break;

      // inserisco nuova area in WRKSESSIONS_TABLE_NAME
      if ((new_code = ins_session_to_db(DescrArea)) == 0) break;
      code = new_code;

      if (gsc_mkdir(dir.get_name()) == GS_BAD) break;
      
      // creo il direttorio TEMP
      pathfile = dir;
      pathfile += _T('\\');
      pathfile += GEOTEMPSESSIONDIR;
      if (gsc_mkdir(pathfile.get_name()) == GS_BAD) break;
      pathfile += _T('\\');
      pathfile += GEOWRKSESSIONDB;

      SourceDB = GEOsimAppl::GEODIR + _T('\\') + GEOSAMPLEDIR + _T('\\') + ACCESSGEOWRKSESSIONSAMPLEDB;
      // creo database per tabelle sessione di lavoro
      if (gsc_copyfile(SourceDB, pathfile) == GS_BAD) break;

      // registro l'applicazione di GEOsim GEO_APP_ID nel disegno corrente
      if (gsc_regapp()==GS_BAD) break; 

      // setto il sistema di coordinate (che fa uno zoom per i fatti suoi)
      if (gsc_projsetwscode(coordinate) == GS_BAD) break;

      // se esiste un sinottico lo inserisco
      if (KeyDwg.len() > 0)
      {
         // Leggo il sinottico dal dwg roby
         if (gsc_attachExtractDetach(KeyDwg) == GS_BAD) break;

         // se � stato inserito un sinottico per visualizzarlo devo fare zoom esteso
         // posticipato a quello di gsc_projsetwscode (che lo fa quando autocad riprende il controllo)
         AcApDocument* pDoc = acDocManager->curDocument();
         acDocManager->activateDocument(pDoc);
         acDocManager->sendStringToExecute(pDoc, _T("_.ZOOM\n_E\n"));
      }

      // setto area corrente
      GS_CURRENT_WRK_SESSION = this;

      // inizializzo area
      if (init() == GS_BAD)
      {
         GS_CURRENT_WRK_SESSION = NULL;
         break;
      }

      result = GS_GOOD;
   }
   while (0);

   // libero il file delle password
   _close(file);

   if (result == GS_BAD)
   {
      int prevErr = GS_ERR_COD;

      del((TCHAR *) GEOsimAppl::GS_USER.pwd);
      GS_ERR_COD = prevErr;
      return 0;
   }

   // inizializzo le variabili d'ambiente ADE per GEOsim
   C_MAP_ENV AdeEnv;
   AdeEnv.SetEnv4GEOsim();

   // imposto il direttorio di default per il catalogo di query da salvare
   // quando si � in una sessione di lavoro
   pathfile = pPrj->get_dir();
   pathfile += _T('\\');
   pathfile += GEOQRYDIR;
   if ((p = acutBuildList(RTSTR, pathfile.get_name(), 0)) != NULL) 
   {
      ade_prefsetval(_T("QueryFileDirectory"), p);
      acutRelRb(p);
   }

   // carico eventuale file GS_LISP_FILE dal direttorio del progetto corrente
   gs_gsl_prj_reload();

   // Commentato perch� questa funzione viene chiamata mentre � aperta
   // una finestra DCL e quindi non posso chiamare UndefineAcadCmds
   // che a sua volta chiama degli acedCommand
   // Eseguo undefine dei comandi primitivi di AutoCAD per usare quelli di GEOsim
   //GEOsimAppl::CMDLIST.UndefineAcadCmds();

   // Notifico in file log
   TCHAR Msg[MAX_LEN_MSG];
   swprintf(Msg, MAX_LEN_MSG, _T("Session %ld named <%s> created."), get_id(), get_name());
   gsc_write_log(Msg);

   return new_code;
}


/*********************************************************/
/*.doc C_WRK_SESSION::setLastExtractedClassesToFile <external> */
/*+
  Funzione che scrive/legge i codici delle classi estratte nella sessione.
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::setLastExtractedClassesToINI(void)
{
   C_STRING        pathfile, entry(_T("LastExtractedClasses_PRJ")), value;
   C_CLS_PUNT_LIST extr_cls_list;   

   if (status != WRK_SESSION_ACTIVE) return GS_GOOD;

   // scrivo la lista dei codici delle classi in GEOSIM.INI
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;

   entry += get_PrjId();

   if (get_pPrj()->extracted_class(extr_cls_list) == GS_GOOD)
   {
      C_CLS_PUNT *p = (C_CLS_PUNT *) extr_cls_list.get_head();
      while (p)
      {
         value += ((C_CLASS *) (p->get_class()))->ptr_id()->code;
	      if ((p = (C_CLS_PUNT *) extr_cls_list.get_next()))
            value += _T(',');
      }
   }

   return gsc_setInfoToINI(entry.get_name(), value.get_name(), &pathfile);
}
int C_WRK_SESSION::getLastExtractedClassesFromINI(C_INT_LIST &ClsList)
   { return gsc_getLastExtractedClassesFromINI(get_PrjId(), ClsList); }


/*********************************************************/
/*.doc C_WRK_SESSION::is_valid <external> */
/*+
  Questa funzione controlla i valori di un'area.
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::is_valid(void)
{
   C_INT_INT_STR_LIST lista_utenti;
   C_USER             temp;
   int                file;

   // verifico validit� name
   if (wcslen(name) == 0) { GS_ERR_COD = eGSInvSessionName; return GS_BAD; }
   // verifico validit� dir
   if (gsc_validdir(dir) == NULL) return GS_BAD;
   // verifico validit� level
   if (level != GSUpdateableData && level != GSReadOnlyData)
      { GS_ERR_COD = eGSInvSessionType; return GS_BAD; }
   // verifico validit� usr
   if (gsc_getusrlist(&lista_utenti) == GS_BAD) return GS_BAD;
   if (lista_utenti.search_key(usr) == NULL)
      { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }
   // verifico che l'utente esista ancora
   if ((file = gsc_open_geopwd(READONLY)) == -1) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
   if (temp.alloc_from_geopwd(file)==GS_BAD) { _close(file); return GS_BAD; }
   if (gsc_get_log(file, usr, LOG_ID, temp.log) == GS_BAD)
      { _close(file); return GS_BAD; }
   _close(file);
   // verifico che anche la login sia uguale
   if (gsc_strcmp((TCHAR *)temp.log, (TCHAR *)GEOsimAppl::GS_USER.log) != 0)
      { GS_ERR_COD = eGSInvalidUser; return GS_BAD; }
   // verifico validit� status
   if (status != WRK_SESSION_SAVE && status != WRK_SESSION_EXTR &&
       status != WRK_SESSION_ACTIVE && status != WRK_SESSION_FREEZE &&
       status != WRK_SESSION_WAIT)
      { GS_ERR_COD = eGSInvSessionStatus; return GS_BAD; }
   // verifico validit� coordinate
   if (gsc_validcoord(coordinate) == GS_BAD) return GS_BAD;
   
   return GS_GOOD;
}
   

/*********************************************************/
/*.doc C_WRK_SESSION::to_rb_db <internal> */
/*+
  Questa funzione scarica i dati di una C_WRK_SESSION in un resbuf per scrivere
  nella tabella WRKSESSIONS_TABLE_NAME.
  Parametri:
  presbuf *colvalues;    lista colonna-valore della riga di GS_AREA
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::to_rb_db(presbuf *colvalues)
{
   C_STRING dummy;

   dummy.paste(ClsCodeList.to_str());

   if ((*colvalues = acutBuildList(RTLB, RTSTR, _T("GS_ID"), RTLONG, code, RTLE, 
                                   RTLB, RTSTR, _T("NAME"), RTSTR, name, RTLE,
                                   RTLB, RTSTR, _T("DIR"), RTSTR, (dir.len() == 0) ? GS_EMPTYSTR : dir.get_name(), RTLE,
                                   RTLB, RTSTR, _T("TYPE"), RTSHORT, (int) level, RTLE,
                                   RTLB, RTSTR, _T("USR"), RTSHORT, usr, RTLE,
                                   RTLB, RTSTR, _T("STATUS"), RTSHORT, status, RTLE,
                                   RTLB, RTSTR, _T("COORDINATE"), RTSTR, coordinate, RTLE,
                                   RTLB, RTSTR, _T("CREATION_DATETIME"), RTSTR, (CreationDateTime.len() == 0) ? GS_EMPTYSTR : CreationDateTime.get_name(), RTLE,
                                   RTLB, RTSTR, _T("CLASS_CODE_LIST"), RTSTR, (dummy.len() == 0) ? GS_EMPTYSTR : dummy.get_name(), RTLE,
                                   0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   return GS_GOOD;
}  
   

/*********************************************************/
/*.doc C_WRK_SESSION::from_rb_db <internal> */
/*+
  Questa funzione carica i dati di una C_WRK_SESSION da un resbuf letto
  dalla tabella WRKSESSIONS_TABLE_NAME.
  Parametri:
  presbuf *colvalues;    lista colonna-valore della riga di WRKSESSIONS_TABLE_NAME
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::from_rb_db(C_RB_LIST &ColValues)
{
   presbuf  p;
   C_STRING m_dir, m_CreationDateTime;
   TCHAR    m_name[MAX_LEN_WRKSESSION_NAME], m_coordinate[MAX_LEN_COORDNAME];
   int      m_type, m_status, m_usr;
   long     m_code;

   if ((p = ColValues.CdrAssoc(_T("GS_ID"))) == NULL || gsc_rb2Lng(p, &m_code) == GS_BAD)
      return GS_BAD;

   if ((p = ColValues.CdrAssoc(_T("NAME"))) == NULL) return GS_BAD;
   gsc_strcpy(m_name, gsc_rtrim(p->resval.rstring), MAX_LEN_WRKSESSION_NAME);

   if ((p = ColValues.CdrAssoc(_T("DIR"))) == NULL) return GS_BAD;
   m_dir = gsc_rtrim(p->resval.rstring);
   // traduco dir assoluto in dir relativo
   if (gsc_nethost2drive(m_dir) == GS_BAD) return GS_BAD;

   if ((p = ColValues.CdrAssoc(_T("TYPE"))) == NULL || gsc_rb2Int(p, &m_type) == GS_BAD)
      return GS_BAD;

   if ((p = ColValues.CdrAssoc(_T("USR"))) == NULL || gsc_rb2Int(p, &m_usr) == GS_BAD)
      return GS_BAD;

   if ((p = ColValues.CdrAssoc(_T("STATUS"))) == NULL || gsc_rb2Int(p, &m_status) == GS_BAD)
      return GS_BAD;

   if ((p = ColValues.CdrAssoc(_T("COORDINATE"))) == NULL) return GS_BAD;
   if (p->restype != RTSTR) wcscpy_s(m_coordinate, MAX_LEN_COORDNAME, GS_EMPTYSTR);
   else gsc_strcpy(m_coordinate, gsc_rtrim(p->resval.rstring), MAX_LEN_COORDNAME);

   if ((p = ColValues.CdrAssoc(_T("CREATION_DATETIME"))) == NULL) return GS_BAD;
   if (p->restype == RTSTR)
      m_CreationDateTime = gsc_alltrim(p->resval.rstring);

   if (set_name(m_name) == GS_BAD || set_id(m_code) == GS_BAD ||
       set_dir(m_dir.get_name()) == GS_BAD || set_status(m_status) == GS_BAD ||
       set_usr(m_usr) == GS_BAD || set_level((GSDataPermissionTypeEnum) m_type) == GS_BAD ||
       set_coordinate(m_coordinate) == GS_BAD) return GS_BAD;
   CreationDateTime = m_CreationDateTime;

   if ((p = ColValues.CdrAssoc(_T("CLASS_CODE_LIST"))) == NULL) return GS_BAD;
   if (p->restype == RTSTR) ClsCodeList.from_str(p->resval.rstring);
   else ClsCodeList.remove_all();

   return GS_GOOD;
}  


/*********************************************************/
/*.doc C_WRK_SESSION::ins_session_to_db              <internal> */
/*+
  Questa funzione inserisce una nuova sessione di lavoro
  in WRKSESSIONS_TABLE_NAME.
  La lista resbuf viene modificata.
  Parametri:
  C_RB_LIST &ColValues; Lista colonna-valore
  
  Restituisce il codice della nuova sessione in caso di successo altrimenti restituisce 0. 
-*/  
/*********************************************************/
long C_WRK_SESSION::ins_session_to_db(C_RB_LIST &ColValues)
{
   presbuf        p;
   C_STRING       TableRef;
   int            n_test = 0;
   long           new_code;
   C_DBCONNECTION *pDBConn;
       
   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return 0; }
   if (pPrj->getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return 0;

   // traduco dir relativo in dir assoluto
   if ((p = ColValues.CdrAssoc(_T("DIR"))) != NULL)
   {
      if (p->resval.rstring != NULL)
      {
         C_STRING path;
         
         path = p->resval.rstring;
         if (gsc_drive2nethost(path) == GS_BAD) return 0;
         free(p->resval.rstring);
         if ((p->resval.rstring = gsc_tostring(path.get_name())) == NULL) return 0;
      }
   }

   // ciclo di GS_NUM_TEST tentativi per inserire una nuova sessione di lavoro
   do
   {
      // cerco nuovo codice per l'area
      if ((new_code = pPrj->getNextWrkSessionId()) == 0) return 0;

      // modifico codice in resbuf per scrittura in WRKSESSIONS_TABLE_NAME
      if (ColValues.CdrAssocSubst(_T("GS_ID"), new_code) == GS_BAD) return 0;

      // inserisco nuova riga in WRKSESSIONS_TABLE_NAME
      if (pDBConn->InsRow(TableRef.get_name(), ColValues, ONETEST) == GS_GOOD) break;
       
      n_test++;
      if (n_test >= GEOsimAppl::GLOBALVARS.get_NumTest()) return 0;
      
      // se l'errore non era dovuto perch� il codice esisteva gi�, attendi
      if (GS_ERR_COD != eGSIntConstr) gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime());
   }
   while (1);

   // salvo il prossimo listelcodice per la sessione di lavoro
   pPrj->setNextWrkSessionId(new_code + 1);

   return new_code;
}


/*********************************************************/
/*.doc C_WRK_SESSION::upd_ClsCodeList_to_db   <internal> */
/*+
  Questa funzione aggiorna la lista delle classi usate dalla sessione
  in WRKSESSIONS_TABLE_NAME.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_WRK_SESSION::upd_ClsCodeList_to_db(void)
{
   C_RB_LIST      ColValues;
   C_STRING       TableRef, dummy, statement;
   C_DBCONNECTION *pDBConn;
   long           RecordsAffected;

   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }
   if (pPrj->getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;

   dummy.paste(ClsCodeList.to_str());
   // Correggo la stringa secondo la sintassi SQL
   if (pDBConn->Str2SqlSyntax(dummy) ==  GS_BAD) return GS_BAD;

   statement = _T("UPDATE ");
   statement += TableRef;
   statement += _T(" SET CLASS_CODE_LIST=");
   statement += dummy;
   statement += _T(" WHERE GS_ID=");
   statement += code;

   if (pDBConn->ExeCmd(statement, &RecordsAffected) == GS_BAD) return GS_BAD;
   if (RecordsAffected == 0) return GS_BAD;
    
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_WRK_SESSION::del                            <external> */
/*+
  Questa funzione cancella una area di GEOsim.
  Parametri:
  const TCHAR *Password;  Password dell'utente corrente (per controllo)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::del(const TCHAR *Password)
{
   C_RB_LIST        ColValues;
   _RecordsetPtr    pRs;
   C_INT_LIST       LockedCodeClassList;
   TCHAR            Msg[MAX_LEN_MSG];
   C_INT            *pExtrCls;

   // verifico abilitazione
   if (gsc_check_op(opDelArea) == GS_BAD) return GS_BAD;
   // verifico che la password sia corretta
   if (gsc_strcmp(Password, (TCHAR *) GEOsimAppl::GS_USER.pwd) != 0)
      { GS_ERR_COD = eGSInvalidPwd; return GS_BAD; }

   if (gsc_superuser() == GS_BAD)  // non � un SUPER USER 
      // se non � "loggato" nessuno o l'utente corrente non sta cancellando le sue areee
      if (usr != GEOsimAppl::GS_USER.code || GEOsimAppl::GS_USER.code == 0)
         { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // controllo che l'area da cancellare NON sia la corrente
   if (GS_CURRENT_WRK_SESSION && 
       GS_CURRENT_WRK_SESSION->get_PrjId() == get_pPrj()->get_key() && // stesso progetto
       GS_CURRENT_WRK_SESSION->get_id() == get_id()) // stessa area
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // Notifico in file log
   swprintf(Msg, MAX_LEN_MSG, _T("Session %ld named <%s> erasing started."), code, get_name());
   gsc_write_log(Msg);

   // se la sessione era in stato di salvataggio
   if (status == WRK_SESSION_SAVE)
   {
      int         _Mode, _State;
      C_LONG_LIST _LockedBy;

      // Verifico quali classi erano in fase di salvataggio
      pExtrCls = (C_INT *) ClsCodeList.get_head();
      while (pExtrCls)
      {
         if (gsc_get_class_lock(get_pPrj()->get_key(), pExtrCls->get_key(),
                                _LockedBy, &_Mode, &_State) == GS_GOOD &&
             _State == WRK_SESSION_SAVE && _LockedBy.search(code))
         {
            C_DWG_LIST DwgList;
            C_CLASS    *pCls = find_class(pExtrCls->get_key());

            // inizializzo la lista dei dwg della classe
            if (DwgList.load(get_pPrj()->get_key(), pExtrCls->get_key()) == GS_GOOD)
               // Sblocca tutti i DWG della classe (cancella i *.DWK, *.DWL?, *.MAP).
               if (pCls && pCls->DWGsUnlock() == GS_GOOD)
                  // Sblocca tutti gli oggetti rimasti bloccati per errore nei DWG attaccati.
                  DwgList.unlockObjs();
 
            // Stacco la sorgente grafica della classe
            if (pCls) pCls->GphDetach();
         }

         pExtrCls = (C_INT *) pExtrCls->get_next();
      }
   }

   if (ClassesUnlock() == GS_BAD) return GS_BAD;

   // setta le classi della sessione di lavoro corrente in stato di salvataggio in modo che
   // nessuna altra sessione possa effettuare il salvataggio nello stesso momento
   // per non bloccare i dwg ad altri utenti in fase di salvataggio (che possono aver
   // gi� salvato le schede nei database ma non gli oggetti grafici).
   if (ClassesLock(&LockedCodeClassList) == GS_BAD) 
   {
      if (gsc_superuser() == GS_GOOD && LockedCodeClassList.get_count() > 0)  // se sono un SUPER USER 
      {
         C_INT    *pLockedCodeClass = (C_INT *) LockedCodeClassList.get_head();
         int      res;
         C_CLASS  *pCls;
         C_STRING Msg(gsc_msg(263)); 

         // La cancellazione della sessione di lavoro ha rilevato alcune classi di entit� bloccate da altre sessioni.\n
         // Procedere comunque al loro sblocco ?
         // Classi di entit� bloccate:
         
         while (pLockedCodeClass)
         {
            if ((pCls = pPrj->find_class(pLockedCodeClass->get_key())))
            {
               Msg += _T('\n');
               Msg += pCls->ptr_id()->name;
            }
            pLockedCodeClass = (C_INT *) LockedCodeClassList.get_next();
         }

         if (gsc_ddgetconfirm(Msg.get_name(), &res, GS_BAD, GS_BAD, GS_BAD, GS_GOOD) == GS_BAD ||
             res == GS_BAD)
            return GS_BAD;

         pLockedCodeClass = (C_INT *) LockedCodeClassList.get_head();
         while (pLockedCodeClass)
         {
            if ((pCls = pPrj->find_class(pLockedCodeClass->get_key())))
               // Forzo lo sblocco della classe per tutte le sessioni senza notificare
               if (pCls->set_unlocked(0, GS_BAD) == GS_BAD)
                  return GS_BAD;

            pLockedCodeClass = (C_INT *) LockedCodeClassList.get_next();
         }
      }
      else
         return GS_BAD;
   }

   // blocco riga in WRKSESSIONS_TABLE_NAME
   if (Lock(pRs) == GS_BAD)
      { ClassesUnlock(); return GS_BAD; }

   // rilascio tutti gli oggetti che avevo bloccato
   if (gsc_unlockallObjs(pPrj, this) == GS_BAD) 
      { Unlock(pRs, READONLY); ClassesUnlock(); return GS_BAD; }

   // sblocco eventuali classi bloccate dalla sessione di lavoro corrente
   if (ClassesUnlock() == GS_BAD)
      { Unlock(pRs, READONLY); return GS_BAD; }

   // cancello caratteristiche sessioni sbloccando la riga in WRKSESSIONS_TABLE_NAME
   if (del_session_from_db(pRs) == GS_BAD) return GS_BAD;

   // cancello eventuali DWK per le classi che non sono in uso
   DWGsUnlock();

   // cancello tutto il direttorio della sessione e tutti i suoi 
   // sottodirettori (la cancellazione dei direttori pu� avvenire solo se non ci
   // sono cursori aperti)
   // Chiudo la connessione con GEOWRKSESSIONDB perch� questa blocca il database (ACCESS 97)
   GEOsimAppl::TerminateSQL(getDBConnection());
   TerminateSQL();

   // A volte ci sono dei tempi di latenza per cui il file di lock del database di access 
   // viene rimosso in ritardo e la cancellazione del direttorio fallisce 
   // quindi riprovo per 10 volte con tempo di attesa di 1 secondo tra un tentativo e l'altro
   int OldNumTest = GEOsimAppl::GLOBALVARS.get_NumTest(), OldWaitTime = GEOsimAppl::GLOBALVARS.get_WaitTime();
   GEOsimAppl::GLOBALVARS.set_NumTest(10);
   GEOsimAppl::GLOBALVARS.set_WaitTime(1);
   gsc_delall(get_dir(), RECURSIVE);
   GEOsimAppl::GLOBALVARS.set_NumTest(OldNumTest);
   GEOsimAppl::GLOBALVARS.set_WaitTime(OldWaitTime);

   // Termino istruzioni usate dal progetto per la sessione
   pPrj->TerminateSQL();

   // Se alcune classi sono state estratte notifico agli altri
   NotifyWaitingUserOnErase();

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pPrj->get_key());
   
   // Notifico in file log
   swprintf(Msg, MAX_LEN_MSG, _T("Session %ld named <%s> erased."), code, get_name());
   gsc_write_log(Msg);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_WRK_SESSION::destroy                        <external> */
/*+
  Questa funzione cancella tutto quello che � possibile cancellare
  di una sessione di lavoro di GEOsim. Questa funzione � da usare solo nel 
  caso non sia possibile cancellare correttamente la sessione.
  Parametri:
  int prj;              codice progetto
  int session;          codice sessione
  const TCHAR *Password; Password dell'utente corrente (per controllo)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Questa funzione potrebbe lasciare sporco il disco o tabelle di GEOsim.
       (es. distruzione di una sessione le cui tabelle si trovano su una 
        macchina spenta...)
-*/  
/*********************************************************/
int C_WRK_SESSION::destroy(const TCHAR *Password)
{
   _RecordsetPtr    pRs;
   C_INT_LIST       LockedCodeClassList;
   TCHAR            Msg[MAX_LEN_MSG];
   C_INT            *pExtrCls;

   // verifico abilitazione
   if (gsc_check_op(opDelArea) == GS_BAD) return GS_BAD;
   // verifico che la password sia corretta
   if (gsc_strcmp(Password, (TCHAR *) GEOsimAppl::GS_USER.pwd) != 0)
      { GS_ERR_COD = eGSInvalidPwd; return GS_BAD; }

   if (gsc_superuser() == GS_BAD)  // non � un SUPER USER 
      // se non � "loggato" nessuno o l'utente corrente non sta cancellando le sue areee
      if (get_usr() != GEOsimAppl::GS_USER.code || GEOsimAppl::GS_USER.code == 0)
         { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // controllo che l'area da cancellare NON sia la corrente
   if (GS_CURRENT_WRK_SESSION && 
       GS_CURRENT_WRK_SESSION->get_PrjId() == get_pPrj()->get_key() && // stesso progetto
       GS_CURRENT_WRK_SESSION->get_id() == get_id()) // stessa sessione
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // Notifico in file log
   swprintf(Msg, MAX_LEN_MSG, _T("Session %ld named <%s> destruction started."), code, get_name());
   gsc_write_log(Msg);

   // se la sessione era in stato di salvataggio
   if (get_status() == WRK_SESSION_SAVE)
   {
      int         _Mode, _State;
      C_LONG_LIST _LockedBy;

      // Verifico quali classi erano in fase di salvataggio
      pExtrCls = (C_INT *) ClsCodeList.get_head();
      while (pExtrCls)
      {
         if (gsc_get_class_lock(get_pPrj()->get_key(), pExtrCls->get_key(),
                                _LockedBy, &_Mode, &_State) == GS_GOOD &&
             _State == WRK_SESSION_SAVE && _LockedBy.search(code))
         {
            C_DWG_LIST DwgList;
            C_CLASS    *pCls = find_class(pExtrCls->get_key());

            // inizializzo la lista dei dwg della classe
            if (DwgList.load(get_pPrj()->get_key(), pExtrCls->get_key()) == GS_GOOD)
               // Sblocca tutti i DWG della classe (cancella i *.DWK, *.DWL?, *.MAP).
               if (pCls && pCls->DWGsUnlock() == GS_GOOD)
                  // Sblocca tutti gli oggetti rimasti bloccati per errore nei DWG attaccati.
                  DwgList.unlockObjs();

            // Stacco la sorgente grafica della classe
            if (pCls) pCls->GphDetach();
         }

         pExtrCls = (C_INT *) pExtrCls->get_next();
      }
   }

   ClassesUnlock(); // sblocco classi bloccate dalla sessione

   // setta le classi della sessione di lavoro corrente in stato di salvataggio in modo che
   // nessuna altra sessione possa effettuare il salvataggio nello stesso momento
   // per non bloccare i dwg ad altri utenti in fase di salvataggio (che possono aver
   // gi� salvato le schede nei database ma non gli oggetti grafici).
   if (ClassesLock(&LockedCodeClassList) == GS_BAD) 
   {
      if (gsc_superuser() == GS_GOOD)  // se sono un SUPER USER 
      {
         C_INT   *pLockedCodeClass;
         C_CLASS *pCls;
            
         pLockedCodeClass = (C_INT *) LockedCodeClassList.get_head();
         while (pLockedCodeClass)
         {  
            if ((pCls = pPrj->find_class(pLockedCodeClass->get_key())))
               // Forzo lo sblocco della classe per tutte le sessioni senza notificare
               pCls->set_unlocked(0, GS_BAD);

            pLockedCodeClass = (C_INT *) LockedCodeClassList.get_next();
         }
      }
   }

   // blocco riga in WRKSESSIONS_TABLE_NAME
   Lock(pRs);

   // rilascio tutti gli oggetti che avevo bloccato
   gsc_unlockallObjs(pPrj, this);

   // sblocco eventuali classi bloccate dalla sessione di lavoro corrente
   ClassesUnlock();

   // cancello caratteristiche sessioni sbloccando la riga in WRKSESSIONS_TABLE_NAME
   del_session_from_db(pRs);

   // cancello eventuali DWK per le classi che non sono in uso
   DWGsUnlock();

   // cancello tutto il direttorio della sessione e tutti i suoi 
   // sottodirettori (la cancellazione dei direttori pu� avvenire solo se non ci
   // sono cursori aperti)
   // Chiudo la connessione con GEOWRKSESSIONDB perch� questa blocca il database (ACCESS 97)
   GEOsimAppl::TerminateSQL(getDBConnection());
   TerminateSQL();

   // A volte ci sono dei tempi di latenza per cui il file di lock del database di access 
   // viene rimosso in ritardo e la cancellazione del direttorio fallisce 
   // quindi riprovo per 10 volte con tempo di attesa di 1 secondo tra un tentativo e l'altro
   int OldNumTest = GEOsimAppl::GLOBALVARS.get_NumTest(), OldWaitTime = GEOsimAppl::GLOBALVARS.get_WaitTime();
   GEOsimAppl::GLOBALVARS.set_NumTest(10);
   GEOsimAppl::GLOBALVARS.set_WaitTime(1);
   gsc_delall(get_dir(), RECURSIVE);
   GEOsimAppl::GLOBALVARS.set_NumTest(OldNumTest);
   GEOsimAppl::GLOBALVARS.set_WaitTime(OldWaitTime);

   // Termino istruzioni usate dal progetto per la sessione
   pPrj->TerminateSQL();

   // Se alcune classi sono state estratte notifico agli altri
   NotifyWaitingUserOnErase();

   // Notifico in file log
   swprintf(Msg, MAX_LEN_MSG, _T("Session %ld named <%s> destroyed."), code, get_name());
   gsc_write_log(Msg);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_WRK_SESSION::NotifyWaitingUserOnErase       <internal> */
/*+
  Questa funzione notifica agli utenti che avevano qualche classe
  bloccata da questa sessione la cancellazione della sessione.
  Parametri:
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_WRK_SESSION::NotifyWaitingUserOnErase(void)
{   
   // "Messaggio GEOsim: La sessione n�.%ld � stata cancellata."
   TCHAR Msg[256];
   swprintf(Msg, 256, gsc_msg(411), code);
   // Notifico chi doveva salvare ed estrarre
   gsc_NotifyWaitForExtraction(get_pPrj()->get_key(), ClsCodeList, Msg);
   gsc_NotifyWaitForSave(get_pPrj()->get_key(), ClsCodeList, Msg);

   return GS_GOOD;
}

  
/*********************************************************/
/*.doc C_WRK_SESSION::Lock <internal> */
/*+
  Questa funzione blocca la riga della sessione in WRKSESSIONS_TABLE_NAME.
  Crea una transazione che va chiusa al rilascio della riga.
  Parametri:
  _RecordsetPtr &pRs;   Recordset di lettura
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_WRK_SESSION::Lock(_RecordsetPtr &pRs)
{   
   C_DBCONNECTION *pDBConn;
   C_STRING       statement, TableRef;

   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }

   if (pPrj->getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE GS_ID=");
   statement += code;
   if (pDBConn->get_RowLockSuffix_SQLKeyWord()) // se esiste un suffisso da usare per bloccare la riga
      { statement += _T(" "); statement += pDBConn->get_RowLockSuffix_SQLKeyWord(); }
   pDBConn->BeginTrans();
   if (pDBConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
   {
      if (GS_ERR_COD == eGSLocked) GS_ERR_COD = eGSSessionInUse;
      pDBConn->RollbackTrans();
      return GS_BAD;
   }
   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      gsc_DBCloseRs(pRs);
      GS_ERR_COD = eGSInvSessionCode;
      pDBConn->RollbackTrans();
      return GS_BAD;
   }

   return gsc_DBLockRs(pRs);
}


/*********************************************************/
/*.doc C_WRK_SESSION::Unlock                  <internal> */
/*+
  Questa funzione sblocca la riga della sessione in WRKSESSIONS_TABLE_NAME.
  Chiude una transazione aperta con "Lock" e chiude anche il recordset.
  Parametri:
  _RecordsetPtr &pRs;   Recordset di lettura
  int           Mode;   Flag se = UPDATEABLE viene aggiornato il 
                        recordset altrimenti vengono abbandonate
                        le modifiche (default = UPDATEABLE)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_WRK_SESSION::Unlock(_RecordsetPtr &pRs, int Mode)
{
   C_DBCONNECTION *pDBConn;

   if (pPrj->getWrkSessionsTabInfo(&pDBConn) == GS_BAD) return GS_BAD;

   if (gsc_DBUnLockRs(pRs, Mode, true) == GS_BAD)
   {
      pDBConn->RollbackTrans();
      return GS_BAD;
   }

   if (Mode != UPDATEABLE)
      pDBConn->RollbackTrans();
   else
      pDBConn->CommitTrans();

   return GS_GOOD;
}
   
  
/*********************************************************/
/*.doc C_WRK_SESSION::del_session_from_db            <internal> */
/*+
  Questa funzione cancella la sessione da'area da WRKSESSIONS_TABLE_NAME.
  Parametri:
  _RecordsetPtr &pRs;    Recordset
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_WRK_SESSION::del_session_from_db(_RecordsetPtr &pRs)
{   
   C_DBCONNECTION *pDBConn;

   if (pPrj->getWrkSessionsTabInfo(&pDBConn) == GS_BAD) return GS_BAD;

   // cancello le righe del cursore vedi "Lock"
   if (gsc_DBDelRow(pRs) == GS_BAD)
   {
      Unlock(pRs, READONLY);
      return GS_BAD;
   }

   return Unlock(pRs);
}   
   
  
/******************************************************************************/
/*.doc C_WRK_SESSION::freeze <internal>                                        */
/*+
  Questa funzione congela la sessione di lavoro.
  In particolare copia i file da TEMP ad OLD e 
  deattiva i DWG attaccati salva il .dwg temporaneo (controllando se esiste gi�)
  e riattiva i DWG (per baco MAP 2004).
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************************/
int C_WRK_SESSION::freeze(void)
{   
   TCHAR    *drawing, buf[ID_PROFILE_LEN];
   C_STRING dummy, source_dir, dest_dir, pathfile;
   long     i, len_sel_set;
   C_SELSET ss_del;
   resbuf   rb;
   C_CLASS  *pCls;
   C_PROFILE_SECTION_BTREE ClsProfileSections, ProfileSections;
   bool                    Unicode = false;

   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }

   if (get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // verifico abilitazione
   if (gsc_check_op(opFreezeArea) == GS_BAD) return GS_BAD;

   // controllo che l'area da congelare sia la corrente
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION != this)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // Per tutte le classi modificate 
   pCls = (C_CLASS *) pPrj->ptr_classlist()->get_head();
   while (pCls)
   {
       // se classe modificata
      if (pCls->isDataModified() == GS_GOOD)
         if (pCls->get_category() == CAT_EXTERN)
         {
            GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile, pCls->ptr_id()->code);
            if (gsc_path_exist(pathfile) == GS_GOOD)
               if (gsc_read_profile(pathfile, ClsProfileSections, &Unicode) == GS_BAD) return GS_BAD;

            C_SUB *pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();
            
            while (pSub)
            {
               swprintf(buf, ID_PROFILE_LEN, _T("%d.%d"), pSub->ptr_id()->code, pSub->ptr_id()->sub_code);
               if (pSub->ptr_info()->ToFile(ClsProfileSections, buf) == GS_BAD)
                  return GS_BAD;
               pSub = (C_SUB *) pSub->get_next();
            }

            if (gsc_write_profile(pathfile, ClsProfileSections, Unicode) == GS_BAD) return GS_BAD; 
         }
         else
         if (pCls->ptr_info())
         {
            GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile, pCls->ptr_id()->code);

            swprintf(buf, ID_PROFILE_LEN, _T("%d.0"), pCls->ptr_id()->code);
            if (pCls->ptr_info()->ToFile(pathfile, buf) == GS_BAD) return GS_BAD;
         }

      pCls = (C_CLASS *) pCls->get_next();
   }

   // segno l'ultima classe che � stata gestita da un comando GEOsim
   swprintf(buf, ID_PROFILE_LEN, _T("%d,%d"), GEOsimAppl::LAST_CLS, GEOsimAppl::LAST_SUB);
   GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile);
   if (gsc_path_exist(pathfile) == GS_GOOD)
      if (gsc_read_profile(pathfile, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   if (ProfileSections.set_entry(LAST_SITUATION, _T("1"), buf) == GS_BAD) return GS_BAD;
   if (gsc_write_profile(pathfile, ProfileSections, Unicode) == GS_BAD) return GS_BAD; 

   // scrivo il contenuto del gruppo di selezione GS_LSFILTER
   gsc_save_LastFilterResult();

   // Scrivo su file il gruppo SAVE_SS
   pathfile = get_dir();
   pathfile += _T('\\');
   pathfile += GEOTEMPSESSIONDIR;
   pathfile += _T('\\');
   pathfile += GEO_SAVESS_FILE;
   if (GEOsimAppl::SAVE_SS.save(pathfile.get_name()) == GS_BAD) return GS_BAD;

   // tutti gli oggetti cancellati vanno rispristinati e memorizzati in un file
	if ((len_sel_set = GEOsimAppl::SAVE_SS.length()) > 0) // se c'� qualcosa nel gruppo di selezione GEOsimAppl::SAVE_SS
	{
      ads_name  ent;
      FILE      *f_handle;
      int       result = GS_GOOD;

      pathfile = dir;
      pathfile += _T('\\');
      pathfile += GEOTEMPSESSIONDIR;
      pathfile += _T('\\');
      pathfile += GEO_SSDEL_FILE;
      if ((f_handle = gsc_fopen(pathfile, _T("w"))) == NULL) return GS_BAD;

      for (i = 0; i < len_sel_set; i++)
	   {
         if (GEOsimAppl::SAVE_SS.entname(i, ent) == GS_BAD)
            { GS_ERR_COD = eGSAdsCommandErr; result = GS_BAD; break; }

		   if (gsc_IsErasedEnt(ent) == GS_GOOD)  	// se l'entit� non esiste
	      {
            if (gsc_UnEraseEnt(ent) != GS_GOOD) // la rispristino
               { result = GS_BAD; break; }
            ss_del.add(ent, GS_GOOD, f_handle);
         }
		}
      if (gsc_fclose(f_handle) == GS_BAD) return GS_BAD;
      if (result == GS_BAD) return GS_BAD;
	}

   // Disattivo tutti i DWG attualmente attivi
   C_DWG_LIST DwgList;

   pCls = (C_CLASS *) pPrj->ptr_classlist()->get_head();
   while (pCls)
   {
      if (pCls->ptr_id()->sel != DESELECTED && pCls->ptr_fas())
         // inizializzo la lista dei dwg della classe
         if (DwgList.load(pPrj->get_key(), pCls->ptr_id()->code) == GS_GOOD)
            // Stacco tutti i DWG della classe
            DwgList.deactivate();

      pCls = (C_CLASS *) pCls->get_next();
   }

   // congelo il disegno da TEMP
   source_dir = dir;
   source_dir += _T('\\');
   source_dir += GEOTEMPSESSIONDIR;
   if (gsc_nethost2drive(source_dir) == GS_BAD) return GS_BAD;
   dummy = source_dir; 
   dummy += _T("\\DRAW"); 
   if ((drawing = gsc_strtran(dummy.get_name(), _T("\\"), _T("/"))) == NULL) return GS_BAD; 

   if (acedGetVar(_T("DWGTITLED"), &rb) != RTNORM)
      { free(drawing);  GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (rb.restype == RTSHORT && rb.resval.rint == 1)
   {  // il disegno ha gi� nome quindi QSAVE non richiede un nome di DWG
      if (gsc_callCmd(_T("_.QSAVE"), 0) != RTNORM) { free(drawing); return GS_BAD; }
   }
   else // il disegno non ha nome quindi QSAVE richiede un nome di DWG
      if (gsc_callCmd(_T("_.QSAVE"), RTSTR, drawing, 0) != RTNORM)
         { free(drawing); return GS_BAD; }

   free(drawing);

   // Riattivo i DWG delle classi estratte
   pCls = (C_CLASS *) pPrj->ptr_classlist()->get_head();
   while (pCls)
   {
      if (pCls->ptr_id()->sel != DESELECTED && pCls->ptr_fas())
         // inizializzo la lista dei dwg della classe
         if (DwgList.load(pPrj->get_key(), pCls->ptr_id()->code) == GS_GOOD)
            // Stacco tutti i DWG della classe
            DwgList.activate();

      pCls = (C_CLASS *) pCls->get_next();
   }

   // creo il direttorio OLD
   dest_dir = dir;
   dest_dir += _T('\\');
   dest_dir += GEOOLDSESSIONDIR;
   if (gsc_mkdir(dest_dir) == GS_BAD) return GS_BAD;

   // copio tutti i files del direttorio TEMP (e sottodirettori)
   // nel direttorio OLD della sessione scartando i files *.dwl?
   if (gsc_copydir(source_dir.get_name(), _T("*.*"), dest_dir.get_name(),
                   RECURSIVE, _T("*.DWL?")) == GS_BAD) return GS_BAD;

   // se � stato ripristinato qualcosa lo ricancello
   if (ss_del.length() > 0)
      // tutti gli oggetti che devono essere cancellati vanno cancellati veramente
      if (ss_del.Erase() != GS_GOOD) return GS_BAD;

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_WRK_SESSION::exit <internal> */
/*+
  Questa funzione esce dalla sessione di lavoro.
  In particolare elimina la directory relativa all'area se l'operazione eseguita
  immediatamente prima era stata di salvataggio nella banca dati centrale, 
  mentre cambia lo stato all'area, mettendolo a WRK_SESSION_FREEZE, se l'operazione eseguita 
  prima era stata di congelamento.
  Se il motivo non � quello di uscire da ACAD (for_quit=GS_BAD) allora la funzione
  ripulisce lo schermo aprendo un nuovo disegno.
  Parametri:
  int for_quit;      flag, se = GS_GOOD la funzione � chiamata durante la notifica
                     di evento kQuitMsg (default = GS_BAD)
  TCHAR *NextCmd;    Stringa che identifica una eventuale chiamata ad un comando
                     da lanciare a fine sessione (dopo il comando di "NEW") se
                     il parametro for_quit = GS_BAD; default = NULL

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B.: Tutti i link Path Name vengono automaticamente cancellati da ACAD.
-*/  
/*****************************************************************************/
int C_WRK_SESSION::exit(int for_quit, TCHAR *NextCmd)
{
   C_RB_LIST      ColValues;
   C_CLASS        *p_class;
   C_CLASS_LIST   *class_list;
   _RecordsetPtr  pRs;

   // verifico l'abilitazione
   if (gsc_check_op(opCloseArea) == GS_BAD) return GS_BAD;

   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }

//gsc_ddalert(_T("funzione exit segno 1"));
   // controllo che l'area da cui si vuole uscire sia la corrente
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION->get_id() != get_id())
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   if (get_status() == WRK_SESSION_FREEZE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // blocco riga in WRKSESSIONS_TABLE_NAME
   if (Lock(pRs) == GS_BAD) return GS_BAD;

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pPrj->get_key());
   // memorizzo le classi estratte nella sessione in GEOSIM.INI
   setLastExtractedClassesToINI();

   // se esiste il direttorio OLD significa che 
   // era stato fatto un congelamento da mantenere
   if (HasBeenFrozen() == GS_GOOD)
   {
      // modifico la riga del cursore vedi "Lock"
      if ((ColValues << acutBuildList(RTLB, RTLB, 
                                      RTSTR, _T("STATUS"),
                                      RTSHORT, WRK_SESSION_FREEZE,
                                      RTLE, RTLE, 0)) == NULL)
         { Unlock(pRs, READONLY); return GS_BAD; }

      // dealloca le strutture usate dall'area
      if (terminate() == GS_BAD) { Unlock(pRs, READONLY); return GS_BAD; }

      // Chiudo la connessione con GEOWRKSESSIONDB perch� questa blocca il database (ACCESS 97)
      GEOsimAppl::TerminateSQL(getDBConnection());
      TerminateSQL();
      
      if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
         { Unlock(pRs, READONLY); return GS_BAD; }

      // sblocco la riga in WRKSESSIONS_TABLE_NAME
      if (Unlock(pRs) == GS_BAD) return GS_BAD;

      // Termino istruzioni usate dal progetto per la sessione
      pPrj->TerminateSQL();
   }
   else // non era stato fatto un congelamento
   {
      // rilascio tutti gli oggetti che avevo bloccato
      if (gsc_unlockallObjs(pPrj, this) == GS_BAD)
         { Unlock(pRs, READONLY); return GS_BAD; }

      // sblocco eventuali classi bloccate dalla sessione di lavoro corrente
      if (ClassesUnlock() == GS_BAD)
         { Unlock(pRs, READONLY); return GS_BAD; }

      // dealloca le strutture usate dall'area
      if (terminate() == GS_BAD)
         { Unlock(pRs, READONLY); return GS_BAD; }

      // cancello la sessione dalla tabella GS_WRKSESSION
      if (del_session_from_db(pRs) == GS_BAD) return GS_BAD;

      // cancello tutto il direttorio della sessione e tutti i suoi 
      // sottodirettori (la cancellazione dei direttori pu� avvenire solo se non ci
      // sono cursori aperti)
      // Chiudo la connessione con GEOWRKSESSIONDB perch� questa blocca il database (ACCESS 97)
      GEOsimAppl::TerminateSQL(getDBConnection());

      TerminateSQL();

      // Termino istruzioni usate dal progetto per la sessione
      pPrj->TerminateSQL();

      // se la sessione corrente sta utilizzando un disegno esistente che si chiama
      // "DRAW.DWG" nel direttorio "TEMP" sotto il direttorio della sessione e non 
      // esiste il direttorio "OLD" sotto il direttorio della sessione:
      // la sessione era stata precedentemente congelata, salvando e quindi uscendo, 
      // non veniva cancellato il direttorio della sessione perch� il 
      // disegno (DRAW.DWG) era ancora usato da AutoCAD.
      if (ActiveButPreviouslyFrozen() == GS_BAD)
      {
         // A volte ci sono dei tempi di latenza per cui il file di lock del database di access 
         // viene rimosso in ritardo e la cancellazione del direttorio fallisce 
         // quindi riprovo per 10 volte con tempo di attesa di 1 secondo tra un tentativo e l'altro
         int OldNumTest = GEOsimAppl::GLOBALVARS.get_NumTest(), OldWaitTime = GEOsimAppl::GLOBALVARS.get_WaitTime();
         GEOsimAppl::GLOBALVARS.set_NumTest(10);
         GEOsimAppl::GLOBALVARS.set_WaitTime(1);
         gsc_delall(get_dir(), RECURSIVE);
         GEOsimAppl::GLOBALVARS.set_NumTest(OldNumTest);
         GEOsimAppl::GLOBALVARS.set_WaitTime(OldWaitTime);
      }
      else
         gsc_delall(get_dir(), RECURSIVE, TRUE, GS_BAD); // rimuovere la cartella senza riprovare

      // Se alcune classi sono state estratte notifico agli altri
      NotifyWaitingUserOnErase();
   }

   // disattivo e stacco i DWG delle classi estratte e
   // le cancello dalla memoria perch� nella sessione potrei 
   // aver modificato qualche loro caratteristica (valori di default, FAS...)
   C_DWG_LIST DwgList;

   class_list = pPrj->ptr_classlist();
   p_class = (C_CLASS *) class_list->get_head();
   while (p_class != NULL)
   {
      if (p_class->ptr_id()->sel != DESELECTED)
      {  
         // Stacco la sorgente grafica  della classe
         p_class->GphDetach();

         class_list->remove_at(); // cancella e va al successivo
         p_class = (C_CLASS *) class_list->get_cursor();
      }
      else
         p_class = (C_CLASS *) class_list->get_next();
   }

   // rilascio alcune variabili globali che hanno senso solo in una sessione di lavoro
   GEOsimAppl::SAVE_SS.clear();
   GEOsimAppl::REFUSED_SS.clear();
   if (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET) 
   {
      delete GEOsimAppl::ACTIVE_VIS_ATTRIB_SET;
      GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = NULL;
   }
   GS_ALTER_FAS_OBJS.clear();
   GS_LSFILTER.clear();
   gsc_PutSym_ssfilter();           // Esporta in Autolisp il gruppo di selez. dl filtro
   LPN_USER_STATUS.remove_all();    // Memorizza la scelta di utilizzo LPN nell' import
   gsc_reactors_ClearObjs();        // Pulisco da eventuali oggetti rimasti nei reattori
   GEOsimAppl::OD_TABLENAME_LIST.remove_all(); // Lista delle tabelle OD correntemente definite

   // Ripristino alcuni comandi di ACAD per GEOsim
   GEOsimAppl::CMDLIST.RedefineAcadCmds();

   SEL_EXT              = NULL;
   SEL_CLS              = NULL;
   GEOsimAppl::LAST_CLS = 0;
   GEOsimAppl::LAST_SUB = 0;

   // Notifico in file log
   TCHAR Msg[MAX_LEN_MSG];
   swprintf(Msg, MAX_LEN_MSG, _T("Session %ld named <%s> left."), code, get_name());
   gsc_write_log(Msg);

   // se la funzione non � chiamata durante la notifica di evento kQuitMsg
   if (for_quit == GS_BAD)
   {
      C_STRING  ScriptFileName;

      // riabilito i reattori (se erano stati disabilitati dalle funzioni
      // "gsc_reactors_off" o da "gsc_disable_reactors")
      gsc_enable_reactors();

      // pulisco lo stack degli errori ADE per evitare che MAP visualizzi la lista
      // di errori ADE della sessione precedente
      ade_errclear();
 
      // Scrivo uno script da lanciare a fine funzione
      if (ScriptFileName.paste(WriteScriptToExit(NextCmd)) == NULL) return GS_BAD;

      // lancio lo script scritto in precedenza
      gsc_callCmd(_T("_.SCRIPT"), RTSTR, ScriptFileName.get_name(), 0);
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_WRK_SESSION::WriteScriptToExit                           <internal> */
/*+
  Questa funzione di ausilio alla "exit"  scrive un file di script per
  l'uscita da una sessione di lavoro nel caso che non sia un QUIT di AutoCAD
  Parametri:
  TCHAR *NextCmd;    Stringa che identifica una eventuale chiamata ad un comando
                     da lanciare a fine sessione (dopo il comando di "NEW") se
                     il parametro for_quit = GS_BAD; default = NULL

  Restituisce la path del file script generato in di successo altrimenti NULL. 
-*/  
/*****************************************************************************/
TCHAR *C_WRK_SESSION::WriteScriptToExit(TCHAR *NextCmd)
{
   FILE     *file;
   C_STRING filename, str, dir2Compare(dir), Template, ScriptDir;

   // scrivo file script      
   ScriptDir = GEOsimAppl::CURRUSRDIR;
   ScriptDir += _T('\\');
   ScriptDir += GEOTEMPDIR;
   if (gsc_get_tmp_filename(ScriptDir.get_name(), GS_SCR_EXIT, _T(".SCR"), filename) == GS_BAD)
      return NULL;

   if ((file = gsc_fopen(filename, _T("w"))) == NULL) return NULL;

   // Se � stato modificato qualcosa nella sessione corrente di autocad      
   if (gsc_isCurrentDWGModified())
      str = _T("_.QNEW\n_Y\n"); // Il comando chiede se si vuole abbandonare le modifiche
   else
      str = _T("_.QNEW\n");

   // Se NON � stato impostato un modello che deve essere usato dal comando QNEW
   // (vedi Strumenti-Opzioni-Impostazioni del modello-Nome del file modello di default per CNUOVO) 
   // allora viene richiesto un modello
   if (gsc_getGEOsimQNEWTEMPLATERegistryKey(Template) == GS_BAD)
      str += GS_LFSTR;
     
   // se la sessione corrente sta utilizzando un disegno esistente che si chiama
   // "DRAW.DWG" nel direttorio "TEMP" sotto il direttorio della sessione e non 
   // esiste il direttorio "OLD" sotto il direttorio della sessione:
   // la sessione era stata precedentemente congelata, salvando e quindi uscendo, 
   // non veniva cancellato il direttorio della sessione perch� il 
   // disegno (DRAW.DWG) era ancora usato da AutoCAD.
   if (ActiveButPreviouslyFrozen() == GS_GOOD)
   {
      C_STRING Dir4Lisp(dir);

      Dir4Lisp.strtran(_T("\\"), _T("/"));

      str += _T("(GS_DELALL \"");
      str += Dir4Lisp;
      str += _T("\" 1)\n");
   }

   if (fwprintf(file, str.get_name()) < 0)
   {
      gsc_fclose(file);
      GS_ERR_COD = eGSWriteFile;
      return NULL;
   }

   // Se esiste la chiamata ad un comando supplementare 
   // viene richiamato a fine script
   if (NextCmd)
      if (fwprintf(file, _T("%s\n"), NextCmd) < 0)
      {
         gsc_fclose(file);
         GS_ERR_COD = eGSWriteFile;
         return NULL;
      }

   if (gsc_fclose(file) == GS_BAD) return NULL;

   return filename.cut();
}

 
/*****************************************************************************/
/*.doc C_WRK_SESSION::save <internal>                                        */
/*+
  Questa funzione salva la sessione di lavoro nella banca dati centrale.
  Ricontrollare.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*****************************************************************************/
int C_WRK_SESSION::save(void)
{
   C_INT_LIST    class_list_to_save;
   C_INT    	  *pExtrCls, *p_class_to_save;
   C_STRING      dest_dir, LayerConfigFile;
   int			  result, LayerSaved = FALSE, ToSaveAnyway;
   bool          LayerFrozen = false, LayerLocked = false, LayerOff = false;
   C_CLASS       *pCls, *pMotherCls;
   C_FAMILY_LIST family_list;
   C_FAMILY      *p_family;
   
   // verifico abilitazione
   if (gsc_check_op(opSaveArea) == GS_BAD) return GS_BAD;

   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }

   // Se la sessione � in sola lettura oppure non si trova nello stato di
   // "sessione attiva" o "sessione in salvataggio" (potrebbe essersi 
   // piantato un salvataggio precedente senza aver ripristinato lo stato della sessione)
   if (get_level() == GSReadOnlyData || 
       (get_status() != WRK_SESSION_ACTIVE && get_status() != WRK_SESSION_SAVE))
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // controllo che la sessione da salvare sia la corrente
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION != this)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // leggo le famiglie delle classi del progetto
   if (gsc_getfamily(pPrj, &family_list) == GS_BAD) return GS_BAD;
   
   result = GS_GOOD;
   
   // ottengo la lista delle classi da salvare
   pExtrCls = (C_INT *) ClsCodeList.get_head();
   while (pExtrCls)
   {  
      if (!(pCls = pPrj->find_class(pExtrCls->get_key(), 0)))
         { result = GS_BAD; break; }

      ToSaveAnyway = FALSE;

      // se la classe ha legami con altre classi verifico se � una figlia della famiglia
      if (p_family = family_list.search_list(pCls->ptr_id()->code))
         if (p_family->get_key() != pCls->ptr_id()->code)
         {  // se la mamma � da salvare allora anche la figlia � da salvare   
            if ((pMotherCls = pPrj->find_class(p_family->get_key())) == NULL)
               { result = GS_BAD; break; }
            // se c'� stato almeno un cambiamento della banca dati della classe madre
            if (pMotherCls->isDataModified() == GS_GOOD) ToSaveAnyway = TRUE;
         }
      
      // se l'eventuale classe madre � da salvare oppure
      // se c'� stato almeno un cambiamento della banca dati della classe
      if (ToSaveAnyway || pCls->isDataModified() == GS_GOOD)
	   {
         if ((p_class_to_save = new C_INT(pExtrCls->get_key())) == NULL) 
            { result = GS_BAD; GS_ERR_COD = eGSOutOfMem; break; }

         class_list_to_save.add_tail(p_class_to_save);
	   }
      pExtrCls = (C_INT *) pExtrCls->get_next();
   }

   if (result == GS_BAD) return GS_BAD;
   if (class_list_to_save.get_count() == 0)
   {
      // cancello il contenuto del sottodirettorio OLD della sessione di lavoro corrente
      // N.B. : questo serve alla funzione C_WRK_SESSION::exit che testa l'esistenza 
      // del sottodirettorio OLD della sessione di lavoro corrente
      dest_dir = dir;
      dest_dir += _T('\\');
      dest_dir += GEOOLDSESSIONDIR;
      if (gsc_path_exist(dest_dir) == GS_GOOD)
         gsc_delall(dest_dir.get_name(), RECURSIVE);
      return GS_GOOD;
   }

   // salvo configurazione attuale dei layer
   LayerConfigFile = GEOsimAppl::CURRUSRDIR;
   LayerConfigFile += _T('\\');
   LayerConfigFile += GEOTEMPDIR;
   LayerConfigFile += _T('\\');
   LayerConfigFile += _T("LAYER.LYR");
   if (gsc_save_layer_status(LayerConfigFile.get_name()) == GS_BAD) return GS_BAD;
   
   result = GS_BAD;
   do
   {
      // setto tutti i layer come ON, UNLOCK, THAW
      if (gsc_set_charact_layer(_T("*"), NULL, NULL, 
                                &LayerFrozen, &LayerLocked, &LayerOff) == GS_BAD) break;

      if (GEOsimAppl::GLOBALVARS.get_AlignHighlightedFASOnSave() == GS_GOOD)
      {
         C_STRING path;
         C_SELSET ss;

         GS_ALTER_FAS_OBJS.copy(ss);

         // reallineamento dei dati con FAS variata in FAS di default
         // solo per gli oggetti che devono essere salvati
         ss.intersect(GEOsimAppl::SAVE_SS);
         GEOsimAppl::REFUSED_SS.clear();
         //              selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
         if (gsc_class_align(ss, GS_GOOD, GS_GOOD, &(GEOsimAppl::REFUSED_SS), GS_GOOD) == -1) 
            break;
        
         // Gli altri oggetti vengono trattati solo per l'aspetto grafico
         C_CLASS  *pClass;
         long     BitForFAS;
         ads_name ent;
         C_EED    eed;
         C_SELSET ClassSS;

         GS_ALTER_FAS_OBJS.subtract(ss);

         while (GS_ALTER_FAS_OBJS.entname(0, ent) == GS_GOOD)
   	   {
            // leggo identificatore della classe di appartenenza
            if (eed.load(ent) == GS_BAD) { GS_ALTER_FAS_OBJS.subtract_ent(ent); continue; }

            // Ritorna il puntatore alla classe cercata
            if (!(pClass = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)))
               { GS_ALTER_FAS_OBJS.subtract_ent(ent); continue; }

            if (pClass->get_SelSet(ClassSS) == GS_GOOD)
            {
               ClassSS.intersect(GS_ALTER_FAS_OBJS);

               if ((BitForFAS = pClass->what_is_graph_updateable()) != GSNoneSetting)
               {
                  // La rotazione e l'elevazione non devono essere variate
                  if (BitForFAS & GSRotationSetting) BitForFAS -= GSRotationSetting;
                  if (BitForFAS & GSElevationSetting) BitForFAS -= GSElevationSetting;
                  // Il riempimento con le sue caratteristiche non deve essere variato
                  if (BitForFAS & GSHatchNameSetting) BitForFAS -= GSHatchNameSetting;
                  if (BitForFAS & GSHatchLayerSetting) BitForFAS -= GSHatchLayerSetting;
                  if (BitForFAS & GSHatchColorSetting) BitForFAS -= GSHatchColorSetting;
                  if (BitForFAS & GSHatchScaleSetting) BitForFAS -= GSHatchScaleSetting;
                  if (BitForFAS & GSHatchRotationSetting) BitForFAS -= GSHatchRotationSetting;

                  gsc_modifyEntToFas(ClassSS, pClass->ptr_fas(), BitForFAS);
               }
               GS_ALTER_FAS_OBJS.subtract(ClassSS);
            }
            else
               GS_ALTER_FAS_OBJS.subtract_ent(ent);
         }

         // cancello il file del gruppo di selezione degli oggetti con FAS modificata
         path = get_dir();
         path += _T('\\');
         path += GEOTEMPSESSIONDIR;
         path += _T('\\');
         path += GS_ALTER_FAS_OBJS_FILE;
         if (gsc_path_exist(path) == GS_GOOD) gsc_delfile(path);
      }

      int OldStatus = get_status();
      // Setto lo stato della sessione anche nel database
      if (set_status(WRK_SESSION_SAVE, GS_GOOD) == GS_BAD) break;

      // Salvataggio classi
      if (gsc_save(class_list_to_save) == GS_BAD)
         { set_status(OldStatus, GS_GOOD); break; }

      // Setto lo stato della sessione anche nel database
      if (set_status(OldStatus, GS_GOOD) == GS_BAD) break;
      
      result = GS_GOOD;
   }
   while (0);

   // ripristino configurazione dei layer
   gsc_load_layer_status(LayerConfigFile.get_name());

   if (result == GS_BAD) return GS_BAD;

   gsc_reset_SAVE_SS();

   // cancello il contenuto del sottodirettorio OLD della sessione di lavoro corrente
   // N.B. : questo serve alla funzione C_WRK_SESSION::exit che testa l'esistenza 
   // del sottodirettorio OLD della sessione di lavoro corrente
   dest_dir = dir;
   dest_dir += _T('\\');
   dest_dir += GEOOLDSESSIONDIR;
   if (gsc_path_exist(dest_dir) == GS_GOOD)
      if (gsc_delall(dest_dir.get_name(), RECURSIVE) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_WRK_SESSION::thaw_1part                                   <internal> */
/*+
  Questa funzione, dopo aver copiato i file da OLD a TEMP, prepara uno SCRIPT 
  per l'apertura dell'opportuno file .dwg e per l'esecuzione della funzione 
  <gs_thaw_WrkSession2> passando gli adeguati parametri.
  Parametri:
  bool RecoverDwg;   Flag opzionale; se = TRUE il disegno deve essere recuperato
                     percui il comando OPEN chieder� se si vuole recuperare il
                     disegno e si deve rispondere _Y (Yes)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/************************************************************************************************************/
int C_WRK_SESSION::thaw_1part(bool RecoverDwg)
{
   C_STRING      source_dir, dest_dir, dummy, filename;
   TCHAR         *drawing;
   FILE          *file = NULL;
   int           result = GS_BAD;
   _RecordsetPtr pRs;

   // verifico abilitazione
   if (gsc_check_op(opSelArea) == GS_BAD) return GS_BAD;

   if (GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
   if (get_status() != WRK_SESSION_FREEZE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // verifico esistenza DWG di congelamento nel direttorio OLD
   dummy = dir;
   dummy += _T("\\");
   dummy += GEOOLDSESSIONDIR;
   dummy += _T("\\DRAW.DWG");
   if (gsc_path_exist(dummy) == GS_BAD)
      { GS_ERR_COD = eGSFileNoExisting; return GS_BAD; }

   // blocco riga in GS_AREA
   if (Lock(pRs) == GS_BAD) return GS_BAD;

   do
   {
      source_dir = dir;
      source_dir += _T('\\');
      source_dir += GEOOLDSESSIONDIR;
      
      dest_dir = dir;
      dest_dir += _T('\\');
      dest_dir += GEOTEMPSESSIONDIR;

      // pulisco direttorio di destinazione
      gsc_delall(dest_dir.get_name(), RECURSIVE);

      // copio tutti i files del direttorio OLD nel direttorio TEMP della sessione
      if (gsc_copydir(source_dir.get_name(), _T("*.*"), dest_dir.get_name()) == GS_BAD) break;

      // scrivo file script
      filename = GEOsimAppl::CURRUSRDIR;
      filename += _T('\\');
      filename += GEOTEMPDIR;
      filename += _T('\\');
      filename += GS_SCR_UNFREEZE;
      if ((file = gsc_fopen(filename, _T("w"))) == NULL)  break;

      // apro il disegno congelato (dest_dir convertito da gsc_copydir)
      dummy = dest_dir;
      dummy += _T("/DRAW");
      if ((drawing = gsc_strtran(dummy.get_name(), _T("\\"), _T("/"))) == NULL) break;
   
      // chiamo il comando open (che verr� ridefinito da GEOsim) e non il nativo .open perch� una volta scongelato
      // se si preme invio parte il comando .open che chiede di salvare ma geosim abortisce il salvataggio
      // mandando in crash il comando open
      dummy = (gsc_isCurrentDWGModified()) ? _T("_OPEN\n_Y") : _T("_OPEN");

      if (RecoverDwg) dummy += _T("\n\"%s\"\n_Y\n\n(GS_THAW_WRKSESSION2 %d %ld T)\n");
      else dummy += _T("\n\"%s\"\n(GS_THAW_WRKSESSION2 %d %ld)\n");

      if (fwprintf(file, dummy.get_name(), drawing, pPrj->get_key(), code) < 0)
      {
         free(drawing);
         GS_ERR_COD = eGSWriteFile;
         break;
      }
      free(drawing);

      RS_LOCK_WRKSESSION = pRs;

      result = GS_GOOD;
   }
   while (0);

   if (file)
      if (gsc_fclose(file) == GS_BAD) { Unlock(pRs, READONLY); return GS_BAD; }

   if (result == GS_BAD) { Unlock(pRs, READONLY); return GS_BAD; }

   // setto area corrente
   GS_CURRENT_WRK_SESSION = this;

   if (gsc_callCmd(_T("_.SCRIPT"), RTSTR, filename.get_name(), 0) != RTNORM)
      { Unlock(pRs, READONLY); return GS_BAD; }

   return GS_GOOD;
}


/**************************************************************************************/
/*.doc C_WRK_SESSION::thaw_2part <internal>                                           */
/*+
  Questa funzione setta lo status della sessione ad WRK_SESSION_ACTIVE e imposta
  alcune varibili globali (proprie di gs_graph.h) a valori recuperati dal file 
  avente macro GS_CLASS_FILE, esegue infine la funzione <gs_sel_area_for_sel>.
  Parametri:
  bool RecoverDwg;   Flag che indica se lo scongelamento � dovuto al recupero di
                     una sessione non congelata che era stata interrotta generando
                     un file di recover (default = false).
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/**************************************************************************************/
int C_WRK_SESSION::thaw_2part(bool RecoverDwg)
{
   C_RB_LIST    ColValues;
   presbuf      p;
   C_STRING     pathfile, Buffer;
   TCHAR        entry[ID_PROFILE_LEN];
   int          cls, sub, result = GS_GOOD;

   if (RS_LOCK_WRKSESSION.GetInterfacePtr() == NULL)
      { GS_ERR_COD = eGSInvSessionStatus; GS_CURRENT_WRK_SESSION = NULL; return GS_BAD; }

   // setto area corrente
   GS_CURRENT_WRK_SESSION = this;

   do
   {
      // leggo da file il gruppo SAVE_SS
      pathfile = get_dir();
      pathfile += _T('\\');
      pathfile += GEOTEMPSESSIONDIR;
      pathfile += _T('\\');
      pathfile += GEO_SAVESS_FILE;
      if (GEOsimAppl::SAVE_SS.load(pathfile.get_name()) == GS_BAD) { result = GS_BAD; break; }

   	if (GEOsimAppl::SAVE_SS.length() > 0) // se c'� qualcosa nel gruppo di selezione GEOsimAppl::SAVE_SS
   	{
         C_SELSET ss_del;

         // carico il gruppo ss_del
         pathfile = get_dir();
         pathfile += _T('\\');
         pathfile += GEOTEMPSESSIONDIR;
         pathfile += _T('\\');
         pathfile += GEO_SSDEL_FILE;
         if (ss_del.load(pathfile.get_name()) == GS_BAD) { result = GS_BAD; break; }
         
         if (ss_del.length() > 0)
            if (ss_del.Erase() != GS_GOOD) { result = GS_BAD; break; }
	   }

      // carico il gruppo di selezione GS_LSFILTER
      if (gsc_load_LastFilterResult() == GS_BAD) { result = GS_BAD; break; }
      // carico il gruppo di selezione GS_ALTER_FAS_OBJS
      pathfile = get_dir();
      pathfile += _T('\\');
      pathfile += GEOTEMPSESSIONDIR;
      pathfile += _T('\\');
      pathfile += GS_ALTER_FAS_OBJS_FILE;
      if (GS_ALTER_FAS_OBJS.load(pathfile.get_name()) == GS_BAD) { result = GS_BAD; break; }

      // leggo la lista delle classi gi� estratte nella sessione
      if (load_classes() == GS_BAD) { result = GS_BAD; break; }

      // Se si era interrotta la sessione generando un file di recover
      if (RecoverDwg)
      {
         // Aggiorno gli elast delle classi che erano state modificate
         // perch� il salvataggio ne tiene conto e perch� queste informazioni
         // vengono salvate su file in fase di congelamento e non pi� in fase
         // di inserimento
         C_CLASS *pCls = (C_CLASS *) pPrj->ptr_classlist()->get_head();
         while (pCls)
         {
            if (pCls->isDataModified() == GS_GOOD) // se classe modificata
               pCls->RefreshTempLastId();

            pCls = (C_CLASS *) pCls->get_next();
         }
      }

      // modifico la riga del cursore vedi "Lock"
      if ((ColValues << acutBuildList(RTLB, RTLB, 
                                      RTSTR, _T("STATUS"),
                                      RTSHORT, WRK_SESSION_ACTIVE,
                                      RTLE, RTLE, 0)) == NULL)
         { result = GS_BAD; break; }

      if (gsc_DBUpdRow(RS_LOCK_WRKSESSION, ColValues) == GS_BAD)
         { result = GS_BAD; break; }
      
      if (set_status(WRK_SESSION_ACTIVE) == GS_BAD)
         { result = GS_BAD; break; }

      get_TempInfoFilePath(pathfile);
      if (gsc_get_profile(pathfile, LAST_SITUATION, entry, Buffer) == GS_GOOD)
         swscanf(Buffer.get_name(), _T("%d,%d"), &cls, &sub);      
      else
      {
         cls = 0;
         sub = 0;
      }

      GEOsimAppl::LAST_CLS = cls;
      GEOsimAppl::LAST_SUB = sub;

      if (GEOsimAppl::LAST_CLS != 0)
      {
         if (GEOsimAppl::LAST_SUB == 0)
         {
            if ((SEL_CLS = (C_CLASS *) ((pPrj->ptr_classlist())->search_key(GEOsimAppl::LAST_CLS))) == NULL)
               { result = GS_BAD; break; }
         }
         else
         {
            if ((SEL_EXT=(C_CLASS*)((pPrj->ptr_classlist())->search_key(GEOsimAppl::LAST_CLS))) == NULL)
               { result = GS_BAD; break; }
            if ((SEL_CLS=(C_CLASS*)((SEL_EXT->ptr_sub_list())->search_key(GEOsimAppl::LAST_SUB))) == NULL)
               { result = GS_BAD; break; }
         }
      }
   }
   while (0);

   // sblocco la riga in WRKSESSIONS_TABLE_NAME
   if (Unlock(RS_LOCK_WRKSESSION) == GS_BAD)
      result = GS_BAD;

   if (result == GS_BAD)
      { GS_CURRENT_WRK_SESSION = NULL; return GS_BAD; }
   
   // inizializzo area:

   // setto il sistema di coordinate
   if (gsc_projsetwscode(coordinate) == GS_BAD)
      { GS_CURRENT_WRK_SESSION = NULL; return GS_BAD; }

   if (init() == GS_BAD || gsc_set_auxiliary_for_thaw_class() == GS_BAD)
      { GS_CURRENT_WRK_SESSION = NULL; return GS_BAD; }

   // inizializzo le variabili d'ambiente ADE per GEOsim
   C_MAP_ENV AdeEnv;
   AdeEnv.SetEnv4GEOsim();

   // imposto il direttorio di default per il catalogo di query da salvare
   // quando si � in una sessione di lavoro
   pathfile = pPrj->get_dir();
   pathfile += _T('\\');
   pathfile += GEOQRYDIR;
   if ((p = acutBuildList(RTSTR, pathfile.get_name(), 0)) != NULL) 
   {
      ade_prefsetval(_T("QueryFileDirectory"), p);
      acutRelRb(p);
   }

   // carico eventuale file GS_LISP_FILE dal direttorio del progetto corrente
   gs_gsl_prj_reload();

   // registro l'applicazione di GEOsim GEO_APP_ID nel disegno corrente
   gsc_regapp(); 

   // Eseguo undefine dei comandi primitivi di AutoCAD per usare quelli di GEOsim
   GEOsimAppl::CMDLIST.UndefineAcadCmds();

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pPrj->get_key());

   // Notifico in file log
   TCHAR Msg[MAX_LEN_MSG];
   swprintf(Msg, MAX_LEN_MSG, _T("Session %ld named <%s> thawed."), code, get_name());
   gsc_write_log(Msg);

   return GS_GOOD;
}

   
/*********************************************************/
/*.doc C_WRK_SESSION::extract <internal> */
/*+
  Questa funzione estrae le classi selezionate nella sessione.
  Parametri:
  int mode;                   Modo estrazione
  int Exclusive;              Flag di estrazione se TRUE la funzione estrae le classi
                              rendendole, agli altri utenti, in sola lettura.
                              In caso di fallimento le classi saranno estratte 
                              in sola lettura (default = FALSE)
  C_INT_INT_LIST *pErrClsCodeList; Opzionale, puntatore ad una lista che verr� riempita
                                   con i codici delle classi che non sono state estratte e codice di errore
                                   (default = NULL).
  C_SELSET *pExtractedSS;     Opzionale; puntatore a gruppo di selezione 
                              oggetti estratti (default = NULL).
  int retest;                 se MORETESTS -> in caso di errore riprova n volte 
                              con i tempi di attesa impostati poi ritorna GS_BAD,
                              ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                              (default = MORETESTS).
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD
  in caso di errore o GS_CAN nel caso di interruzione voluta dall'utente (CTRL-C).
-*/  
/*********************************************************/
int C_WRK_SESSION::extract(int mode, int Exclusive, C_INT_INT_LIST *pErrClsCodeList,
                           C_SELSET *pExtractedSS, int retest)
{
   C_CLASS        *pCls;
	C_SUB          *p_sub;
   C_ID           *p_id;
   C_STRING       pathfile, TableRef;
   int            result = GS_GOOD, UndoState, times = 0, conf = GS_BAD;
   GSDataPermissionTypeEnum abilit_state;
   int            LockResult, ExtrRes;
   C_DBCONNECTION *pClsConn;
   C_RB_LIST      ColValues;
   C_INT_INT_LIST ClassList_to_extract;
   C_INT_INT      *pClass_to_extract;
   C_INT_LIST     extractedClsCodeList; // lista dei codici delle classi estratte
   C_INT          *pExtractedClsCode;
   C_INT_INT_LIST ErrClsCodeList; // lista dei codici delle classi non estratte e relativo codice di errore
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1089)); // "Visualizzazione del territorio"
   long                      i;

   if (get_status() != WRK_SESSION_ACTIVE && get_status() != WRK_SESSION_WAIT && 
       get_status() != WRK_SESSION_EXTR)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
   if (mode != EXTRACTION && mode != PREVIEW) 
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // controllo che la sessione da estrarre sia la corrente
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION != this)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // N.B. dopo aver fatto un congelamento non � possibile eseguire altre estrazioni
   // questo � fatto perch� dopo uno scongelamento se si eseguisse una estrazione
   // di un'area gi� estratta si duplicherebbero gli oggetti coincidenti (infatti, in
   // questa condizione, non esistono pi� i link tra oggetti e DWG di origine)
   if (HasBeenFrozen() == GS_GOOD) { GS_ERR_COD = eGSOpNotAbleInFrozenSession; return GS_BAD; }

   // inizializzo gruppo di selezione GS_SELSET variabile pubblica dichiarata in GS_GRAPH
   GS_SELSET.clear();

   // Preparo la lista delle classi da estrarre

   // se c'� solo una classe da estrarre
   int nClsToExtract = 0;
   pCls = (C_CLASS *) pPrj->ptr_classlist()->get_head();
   while (pCls)
   {
      p_id = pCls->ptr_id();
      // se la classe � da estrarre
      if (p_id->sel == SELECTED || p_id->sel == EXTR_SEL)
      {
         nClsToExtract++;
         if (nClsToExtract == 1)
            ClassList_to_extract.values_add_tail(p_id->code, DESELECTED); // Non ancora estratta
         else
         {
            ClassList_to_extract.remove_all();
            break;
         }
      }
      // classe successiva
      pCls = (C_CLASS *) pCls->get_next();
   }

   if (nClsToExtract > 1) // se si devono estrarre pi� di una classi
   {
      // ordino le classi per livello di complessit� e tipologia
      get_pPrj()->ptr_classlist()->sort_for_extraction();

      pCls = (C_CLASS *) pPrj->ptr_classlist()->get_head();
      while (pCls)
      {
         p_id = pCls->ptr_id();
         // se la classe � da estrarre
         if (p_id->sel == SELECTED || p_id->sel == EXTR_SEL)
         {
            if ((pClass_to_extract = new C_INT_INT()) == NULL) 
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   		   pClass_to_extract->set_key(p_id->code);
   		   pClass_to_extract->set_type(DESELECTED); // Non ancora estratta

            ClassList_to_extract.add_tail(pClass_to_extract);
         }

         // classe successiva
         pCls = (C_CLASS *) pCls->get_next();
      }
   }

   // Setto lo stato della sessione anche nel database
   if (set_status(WRK_SESSION_EXTR, GS_GOOD) == GS_BAD) return GS_BAD;

   // disabilito UNDO (velocizza l'estrazione)
   UndoState = gsc_SetUndoRecording(FALSE);

   if (UndoState == TRUE) // undo era abilitato
      // "\nLe operazioni effettuate fino ad ora non saranno pi� annullabili."
      acutPrintf(gsc_msg(597)); 

   if (pErrClsCodeList) pErrClsCodeList->remove_all();
   GEOsimAppl::REFUSED_SS.clear();

   do
   {
      // ciclo su classi da estrarre
      while (gsc_is_completed_extracted(ClassList_to_extract) != TRUE &&
             ++times <= GEOsimAppl::GLOBALVARS.get_NumTest())
      {
         pClass_to_extract = (C_INT_INT *) ClassList_to_extract.get_head();
   
         if (nClsToExtract > 1) // se si dovevano estrarre pi� di una classi
         {
            StatusBarProgressMeter.Init(ClassList_to_extract.get_count());
            i = 0;
         }

         while (pClass_to_extract)
         {
            ErrClsCodeList.remove_key(pClass_to_extract->get_key());

            if (gsc_is_extractable(ClassList_to_extract, pClass_to_extract) != GS_GOOD)
            {  // classe successiva
               ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
               pClass_to_extract = (C_INT_INT *) pClass_to_extract->get_next();
               continue;
            }

            // Ritorna il puntatore alla classe cercata
            if ((pCls = find_class(pClass_to_extract->get_key())) == NULL)
            {
               ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
               result = GS_BAD;
               break;
            }

            p_id = pCls->ptr_id();

            // Se la classe non era gi� stata estratta
            if (pCls->is_extracted() == GS_BAD)
               // Verifico compatibilit� di versione
               if (gsc_isCompatibGEOsimVersion(p_id->Version.get_name()) == GS_BAD)
               {
                  ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), eGSInvVersion);                     
                  // "\n*Attenzione* La classe <%s> non � compatibile con la versione attuale di GEOsim."
                  acutPrintf(gsc_msg(65), p_id->name);
                  pClass_to_extract = (C_INT_INT *) pClass_to_extract->get_next(); // classe successiva
                  continue;
               }

            // Se la classe non era gi� stata estratta
            if (pCls->is_extracted() == GS_BAD)
            {
               if (p_id->category == CAT_EXTERN || p_id->category == CAT_GRID ||
                   Exclusive)
                  // se simulazione o griglia o se bisogna forzare la classe in uso esclusivo
                  if (pCls->set_exclusive_use(code, &LockResult) == GS_BAD ||
                      LockResult == GS_BAD)
                  {
                     ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                     pClass_to_extract = (C_INT_INT *) pClass_to_extract->get_next(); // classe successiva
                     continue;
                  }             

               // se si tratta di classe semplice o spaghetti
               if (p_id->category == CAT_SIMPLEX || p_id->category == CAT_SPAGHETTI)
                  // setto la tabella interna (se non esiste viene creata)
                  if (gsc_setODTable(pPrj->get_key(), p_id->code, p_id->sub_code) == GS_BAD)
                  {
                     ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                     result = GS_BAD;
                     break;
                  }

               if (pCls->ptr_info())
                  if (pCls->ptr_info()->AdjTableParams() != GS_GOOD) 
                  { 
                     ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                     result = GS_BAD;
                     break;
                  }

               if (pCls->ptr_GphInfo() &&
                   pCls->ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource)
                  if (((C_DBGPH_INFO *) pCls->ptr_GphInfo())->AdjTableParams() != GS_GOOD) 
                  {
                     ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                     result = GS_BAD;
                     break;
                  }
            }

            // se simulazione
            if (p_id->category == CAT_EXTERN)
            {
               C_DBCONNECTION *pOldConn, *pTempConn;
               C_STRING       TempTableRif;

               abilit_state = p_id->abilit;

               p_sub = (C_SUB *) pCls->ptr_sub_list()->get_head();
               while (p_sub != NULL)
               {
                  p_sub->ptr_id()->abilit = abilit_state;

                  // se era non era ancora stata estratta
                  if (p_sub->is_extracted() == GS_BAD)
                  {
                     acutPrintf(_T("%s%s..."), gsc_msg(332), p_sub->ptr_id()->name); // "\nPreparazione sotto-classe "

                     // setto la tabella interna (se non esiste viene creata)
                     if (gsc_setODTable(pPrj->get_key(), 
                                        p_sub->ptr_id()->code,
                                        p_sub->ptr_id()->sub_code) == GS_BAD)
                     {
                        ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                        result = GS_BAD;
                        break;
                     }

                     // solo per la prima sottoclasse
                     if (p_sub == pCls->ptr_sub_list()->get_head())
                     {
                        C_TOPOLOGY topo;

                        // Creo la topologia temporanea (se non gi� esistente)
                        topo.set_type(TYPE_POLYLINE);  // tipologia di tipo rete
                        topo.set_cls(pCls);
                        if (topo.OLD2TEMP() == GS_BAD)
                        {
                           ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                           result = GS_BAD;
                           break;
                        }
                     }
            
                     // ricavo nome tabella temporanea senza crearla
                     if (p_sub->getTempTableRef(TempTableRif, GS_BAD) == GS_BAD)
                     {
                        ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                        result = GS_BAD;
                        break;
                     }
                     // connessioni OLE-DB
                     if ((pOldConn = p_sub->ptr_info()->getDBConnection(OLD)) == NULL ||
                         (pTempConn = p_sub->ptr_info()->getDBConnection(TEMP)) == NULL)
                     {
                        ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                        result = GS_BAD;
                        break;
                     }

                     // creo e copio intera tabella schede OLD in TEMP (se non esistente)
                     if (pTempConn->ExistTable(TempTableRif) == GS_BAD)
                        if (pOldConn->CopyTable(p_sub->ptr_info()->OldTableRef.get_name(),
                                                *pTempConn, TempTableRif.get_name(),
                                                GSStructureAndDataCopyType,
                                                GS_GOOD, GS_GOOD) == GS_BAD) // CounterToVideo
                        {
                           ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);                     
                           result = GS_BAD;
                           break;
                        }
                  }
           
                  // inizializzo i tipi di dato ADO per gli attributi della sotto-classe
                  if ((pClsConn = p_sub->ptr_info()->getDBConnection(OLD)) == NULL ||
                      p_sub->ptr_attrib_list()->init_ADOType(pClsConn) == GS_BAD)
                  {
                     ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
                     result = GS_BAD;
                     break;
                  }

                  p_sub = (C_SUB *) p_sub->get_next();
               }
         
               if (result != GS_GOOD) break;
            
               // Estraggo gli elementi grafici della classe
               //if ((ExtrRes = ClsExtract(p_id->code, mode, pExtractedSS)) == GS_GOOD)
               if ((ExtrRes = PROFILE_INT_FUNC(ClsExtract, p_id->code, mode, pExtractedSS)) == GS_GOOD)
               {
                  pClass_to_extract->set_type(SELECTED);
                  extractedClsCodeList.add_tail_int(p_id->code);
                  if (nClsToExtract > 1) // se si dovevano estrarre pi� di una classi
                     StatusBarProgressMeter.Set(++i);
               }
               else if (ExtrRes == GS_CAN) // Interruzione voluta dall'utente
                  { result = GS_CAN; break; }
               else
                  ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
            }
            else // se non � simulazione
            {
               if (pCls->ptr_attrib_list())
                  // inizializzo i tipi di dato ADO per gli attributi della classe
                  if ((pClsConn = pCls->ptr_info()->getDBConnection(OLD)) == NULL ||
                      pCls->ptr_attrib_list()->init_ADOType(pClsConn) == GS_BAD)
                  {
                     ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
                     result = GS_BAD;
                     break;
                  }

               if (pCls->ptr_fas() != NULL) // classe con rappresentazione grafica
               {
                  // Estraggo gli elementi grafici della classe
                  //if ((ExtrRes = ClsExtract(p_id->code, mode, pExtractedSS)) == GS_GOOD)
                  if ((ExtrRes = PROFILE_INT_FUNC(ClsExtract, p_id->code, mode, pExtractedSS)) == GS_GOOD)
                  {
                     pClass_to_extract->set_type(SELECTED);
                     extractedClsCodeList.add_tail_int(p_id->code);
                     if (nClsToExtract > 1) // se si dovevano estrarre pi� di una classi
                        StatusBarProgressMeter.Set(++i);
                  }
                  else if (ExtrRes == GS_CAN) // Interruzione voluta dall'utente
                     { result = GS_CAN; break; }
                  else
                     ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
               }
               else
               {
                  if (p_id->category == CAT_GRID)
                  {
                     C_DBCONNECTION *pOldConn, *pTempConn;
                     C_STRING       TempTableRif;

                     // ricavo nome tabella temporanea senza crearla
                     if (pCls->getTempTableRef(TempTableRif, GS_BAD) == GS_BAD)
                     {
                        ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
                        result = GS_BAD;
                        break;
                     }
                     // connessioni OLE-DB
                     if ((pOldConn = pCls->ptr_info()->getDBConnection(OLD)) == NULL ||
                         (pTempConn = pCls->ptr_info()->getDBConnection(TEMP)) == NULL)
                     {
                        ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
                        result = GS_BAD;
                        break;
                     }

                     // creo e copio intera tabella schede OLD in TEMP (se non esistente)
                     if (pTempConn->ExistTable(TempTableRif) == GS_BAD)
                        if (pOldConn->CopyTable(pCls->ptr_info()->OldTableRef.get_name(),
                                                *pTempConn, TempTableRif.get_name(), 
                                                GSStructureAndDataCopyType,
                                                GS_GOOD, GS_GOOD) == GS_BAD) // CounterToVideo
                        {
                           ErrClsCodeList.values_add_tail(pClass_to_extract->get_key(), GS_ERR_COD);
                           result = GS_BAD;
                           break;
                        }
                  }

                  // Classe senza grafica
                  pClass_to_extract->set_type(SELECTED);
                  extractedClsCodeList.add_tail_int(p_id->code);
                  if (nClsToExtract > 1) // se si dovevano estrarre pi� di una classi
                     StatusBarProgressMeter.Set(++i);
               }
            }

            // classe successiva
            pClass_to_extract = (C_INT_INT *) pClass_to_extract->get_next();
         }

         if (nClsToExtract > 1) // se si dovevano estrarre pi� di una classi
            StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

         if (result != GS_GOOD) break;
      }

      if (result != GS_GOOD) break;
      result = GS_BAD;

      // se alcune classi non sono state estratte chiedere se si vuole riprovare
      if (gsc_is_completed_extracted(ClassList_to_extract) == TRUE) 
         { result = GS_GOOD; break; }

      // se si deve fare un solo test esco senza chiedere la ripetizione
      if (retest == ONETEST) { result = GS_GOOD; break; }

      // "Alcune classi non sono state estratte. Ripetere l'estrazione ?"
      if (gsc_ddgetconfirm(gsc_msg(266), &conf) == GS_BAD) { result = GS_BAD; break; }
      if (conf != GS_GOOD) { result = GS_GOOD; break; }
      times = 0;
   }
   while (1);

   gsc_SetUndoRecording(UndoState); // Ripristina situazione UNDO precedente

   if (pErrClsCodeList) ErrClsCodeList.copy(pErrClsCodeList); // roby 2014

   // Setto lo stato della sessione anche nel database
   if (set_status(WRK_SESSION_ACTIVE, GS_GOOD) == GS_BAD) return GS_BAD;

   // Per tutte le classi estratte
   if ((pExtractedClsCode = (C_INT *) extractedClsCodeList.get_head()) != NULL)
   {
      C_PROFILE_SECTION_BTREE ClsProfileSections;
      bool                    Unicode = false;

      while (pExtractedClsCode)
      {
         if (ClsCodeList.search_key(pExtractedClsCode->get_key()) == NULL)
            ClsCodeList.add_tail_int(pExtractedClsCode->get_key());

         pCls = find_class(pExtractedClsCode->get_key());
         p_id = pCls->ptr_id();

         // apro il file profile per scrivere la classe estratta
         get_TempInfoFilePath(pathfile, pExtractedClsCode->get_key());
         if (gsc_path_exist(pathfile) == GS_GOOD)
            if (gsc_read_profile(pathfile, ClsProfileSections, &Unicode) == GS_BAD) result = GS_BAD;

         // la segno come estratta e selezionata per la prossima estrazione
         p_id->sel = EXTR_SEL;
         // scrivo su file
         if (pCls->ToFile_id(ClsProfileSections) == GS_BAD) result = GS_BAD;

         // se la tabella � dichiarata come "collegata"
         if (pCls->ptr_info() && pCls->ptr_info()->LinkedTable)
            // scrivo su file eventuali parametri di collegamento alla tabella
            // come per esempio nella storicizzazione in cui si memorizza la data dell'immagine storica
            if (pCls->ToFile_info(ClsProfileSections) == GS_BAD) result = GS_BAD;

         // se esiste anche una sola delle tabelle dichiarata come "collegata"
         if (pCls->ptr_GphInfo() && pCls->ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource && 
             (((C_DBGPH_INFO *) pCls->ptr_GphInfo())->LinkedTable ||
              ((C_DBGPH_INFO *) pCls->ptr_GphInfo())->LinkedLblGrpTable ||
              ((C_DBGPH_INFO *) pCls->ptr_GphInfo())->LinkedLblTable))
            // scrivo su file eventuali parametri di collegamento alla tabella
            // come per esempio nella storicizzazione in cui si memorizza la data dell'immagine storica
            if (pCls->ToFile_gphInfo(ClsProfileSections) == GS_BAD) result = GS_BAD;

         // se la classe � una simulazione e non � ancora stata estratta
	      if (p_id->category == CAT_EXTERN)
         {
            p_sub = (C_SUB *) pCls->ptr_sub_list()->get_head();
            while (p_sub != NULL)
            {
               // la segno come estratta e selezionata per la prossima estrazione
               p_sub->ptr_id()->sel = EXTR_SEL;           
               // scrivo su file
               if (p_sub->ToFile_id(ClsProfileSections) == GS_BAD) result = GS_BAD;
               p_sub = (C_SUB *) p_sub->get_next();
            }
         }

         result = gsc_write_profile(pathfile, ClsProfileSections, Unicode);
         
         pExtractedClsCode = (C_INT *) extractedClsCodeList.get_next();
      }

      upd_ClsCodeList_to_db(); // aggiorno la lista delle classi estratte in DB
   }

   // Se ci sono stati dei problemi
   if (ErrClsCodeList.get_count() > 0 ) // roby 2014
   {
      if (retest == MORETESTS) DisplayMsgsForClassNotExtracted(ErrClsCodeList);
      GS_ERR_COD = eGSNotClassExtr;
      result = GS_BAD;
   }

   return result;
}


/*****************************************************************************/
/*.doc C_WRK_SESSION::DisplayMsgsForClassNotExtracted             <internal> */
/*+
  Questa funzione visualizza a rga di comando le classi che non sono state estratte e 
  il morivo dell'errore. Per le classi che non sono state estratte perch� bloccate
  da altri utenti, la funzione chiede se si vuole ricevere notifica da parte di chi bloccava
  le classi.
  Parametri:
  C_INT_INT_LIST &ErrClsCodeList; lista di codici delle classi non estratte e codice dell'errore

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD
  in caso di errore o GS_CAN in caso di interruzione voluta dall'utente (CTRL-C). 
-*/  
/*****************************************************************************/
void C_WRK_SESSION::DisplayMsgsForClassNotExtracted(C_INT_INT_LIST &ErrClsCodeList)
{
   C_INT_INT *pErrClsCode = (C_INT_INT *) ErrClsCodeList.get_head();

   if (!pErrClsCode) return;

   // Se alcune classi non sono state estratte visualizzo un warning nella riga di comando
   C_INT_LIST NotifyClsList;
   bool       first = true;

   // ricavo una lista di classi il cui errore di estrazione � dovuto a problemi di multi-utenza
   while (pErrClsCode)
   {
      if (pErrClsCode->get_type() == eGSLockBySession || pErrClsCode->get_type() == eGSSessionsFound)
         NotifyClsList.add_tail_int(pErrClsCode->get_key());

      pErrClsCode = (C_INT_INT *) ErrClsCodeList.get_next();
   }

   pErrClsCode = (C_INT_INT *) ErrClsCodeList.get_head();
   while (pErrClsCode)
   {
      if (NotifyClsList.search_key(pErrClsCode->get_key()) == NULL) // se non � un errore di multi-utenza
      {
         if (first)
         {
            first = false;
            acutPrintf(gsc_msg(1053)); // "\nLe seguenti classi di entit� non sono state estratte per i seguenti motivi:"
         }
         acutPrintf(_T("\n%s\n%s"), pPrj->find_class(pErrClsCode->get_key())->ptr_id()->name, gsc_err(pErrClsCode->get_type()));
      }
      pErrClsCode = (C_INT_INT *) ErrClsCodeList.get_next();
   }

   if (NotifyClsList.get_count() > 0)
   {
      C_INT *pNotifyCls = (C_INT *) NotifyClsList.get_head();

      acutPrintf(gsc_msg(268)); // "\nLe seguenti classi di entit� non sono state estratte perch� bloccate da altre sessioni di lavoro:"
      while (pNotifyCls)
      {
         acutPrintf(_T("\n%s"), pPrj->find_class(pNotifyCls->get_key())->ptr_id()->name);

         pNotifyCls = (C_INT *) NotifyClsList.get_next();
      }

      // "Alcune classi di entit� non sono state estratte perch� bloccate da altre sessioni di lavoro."
      gsc_ddalert(gsc_msg(69));     

      // Se le classi NON sono gi� in attesa di notifica
      if (gsc_isWaitingForExtraction(get_pPrj()->get_key(), NotifyClsList) == GS_BAD)
      {
         int conf;
         // "Si vuole ricevere un avviso dalle altre sessioni di lavoro ?"
         gsc_ddgetconfirm(gsc_msg(269), &conf);
         if (conf == GS_GOOD) gsc_setWaitForExtraction(get_pPrj()->get_key(), NotifyClsList);
      }
   }

   return;
}


/*****************************************************************************/
/*.doc C_WRK_SESSION::ClsExtract                     <internal> */
/*+
  Questa funzione estrae gli oggetti grafici di una classe.
  Viene settata la query spaziale, SQL e effettuato un eventuale
  riallineamento se le variabili di ambiente di GEOsim lo richiedono.
  Parametri:
  int cls:                 codice classe
  int mode;                modo estrazione (EXTRACTION, PREVIEW)
  C_SELSET *pExtractedSS;  Opzionale; puntatore a gruppo di selezione 
                           oggetti estratti (default = NULL).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD
  in caso di errore o GS_CAN in caso di interruzione voluta dall'utente (CTRL-C). 
-*/  
/*****************************************************************************/
int C_WRK_SESSION::ClsExtract(int cls, int mode, C_SELSET *pExtractedSS)
{
   C_CLASS  *pCls;
   long     nExtracted;

   if ((pCls = find_class(cls)) == NULL) return GS_BAD;

   if ((nExtracted = pCls->extract_GraphData(mode, pExtractedSS)) == -1) return GS_BAD;
   else if (nExtracted == -2) // Interruzione voluta dall'utente
      return GS_CAN;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_WRK_SESSION::init <internal> */
/*+
  Questa funzione inizializza alcune variabili e strutture
  (dichiarate globali) che saranno usate nella sessione di lavoro.
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_WRK_SESSION::init(void)
{
   // resetto il set di visibilit� corrente
   if (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET)
   {
      delete GEOsimAppl::ACTIVE_VIS_ATTRIB_SET;
      GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = NULL;
   }

   return GS_GOOD;
}

  
/*********************************************************/
/*.doc C_WRK_SESSION::terminate <internal> */
/*+
  Questa funzione distrugge le variabili e strutture inizializzate
  da alloc_var.
 
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_WRK_SESSION::terminate(void)
{
   if (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET) // resetto il set di visibilit� corrente
   {
      delete GEOsimAppl::ACTIVE_VIS_ATTRIB_SET;
      GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = NULL;
   }
  
   return GS_GOOD;
}


/******************************************************************************/
/*.doc get_GS_CURRENT_WRK_SESSION() <internal> */
/*+
   Questa funzione � stata introdotta perch� nelle librerie non si vede direttamente la variabile 
   GS_CURRENT_WRK_SESSION, nel file .h � una funzione DllExport.
-*/  
/******************************************************************************/
C_WRK_SESSION* get_GS_CURRENT_WRK_SESSION()
{
   return GS_CURRENT_WRK_SESSION;
}


/****************************************************************************/
/*.doc C_WRK_SESSION::ClassesLock                                       <internal> */
/*+
  Questa funzione blocca tutte le classi della sessione come se fossero in 
  stato di salvataggio.
  Parametri:
  C_INT_LIST *pLockedCodeClassList; opzionale; se passato ritorna l'elenco 
                                    dei codici delle classi che erano bloccate da
                                    altre sessioni di lavoro (default = NULL) e che
                                    quindi non sono state bloccate.
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int C_WRK_SESSION::ClassesLock(C_INT_LIST *pLockedCodeClassList)
{
   C_INT   *pExtrCls;
   C_CLASS *pCls;
   int     result;
   C_INT   *pLockedCodeClass;

   if (pLockedCodeClassList) pLockedCodeClassList->remove_all();

   pExtrCls = (C_INT *) ClsCodeList.get_head();
   while (pExtrCls)
   {
      if ((pCls = pPrj->find_class(pExtrCls->get_key())) == GS_BAD) return GS_BAD;
      if (pCls->set_locked_on_save(code, &result) == GS_BAD) return GS_BAD;
      
      if (result == GS_BAD)
         if (!pLockedCodeClassList) return GS_BAD;
         else
         {
            if ((pLockedCodeClass = new C_INT(pCls->ptr_id()->code)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            pLockedCodeClassList->add_tail(pLockedCodeClass);
         }

      pExtrCls = (C_INT *) pExtrCls->get_next();
   }

   if (pLockedCodeClassList && pLockedCodeClassList->get_count() > 0) return GS_BAD;

   return GS_GOOD;
}


/****************************************************************************/
/*.doc C_WRK_SESSION::ClassesUnlock                                     <internal> */
/*+
  Questa funzione sblocca tutte le classi bloccate dalla sessione perch�:
  1) � stato settato il flag OWNER in GS_CLASS (uso esclusivo)
  2) � stato settato il flag LOCKED in GS_CLASS (classe in fase di 
     estrazione o di salvataggio) se la sessione si fosse piantata
  C_INT_LIST *pLockedCodeClassList; opzionale; se passato ritorna l'elenco 
                                    dei codici delle classi che erano bloccate da
                                    altre sessioni di lavoro (default = NULL) e che
                                    quindi non sono state sbloccate.
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int C_WRK_SESSION::ClassesUnlock(C_INT_LIST *pLockedCodeClassList)
{
   C_INT   *pExtrCls;
   C_CLASS *pCls;
   C_INT   *pLockedCodeClass;

   pExtrCls = (C_INT *) ClsCodeList.get_head();
   while (pExtrCls)
   {
      if ((pCls = pPrj->find_class(pExtrCls->get_key())) == GS_BAD) return GS_BAD;
      if (pCls->set_unlocked(code) == GS_BAD || pCls->set_share_use() == GS_BAD)
         if (!pLockedCodeClassList)
            return GS_BAD;
         else
         {
            if ((pLockedCodeClass = new C_INT(pCls->ptr_id()->code)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            pLockedCodeClassList->add_tail(pLockedCodeClass);
         }

      pExtrCls = (C_INT *) pExtrCls->get_next();
   }

   if (pLockedCodeClassList && pLockedCodeClassList->get_count() > 0) return GS_BAD;

   return GS_GOOD;
}


/****************************************************************************/
/*.doc C_WRK_SESSION::DWGsUnlock <internal> */
/*+
  Questa funzione sblocca tutti i DWG delle classi della sessione che non sono
  usate da nessuno (cancella i *.DWK ...).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int C_WRK_SESSION::DWGsUnlock(void)
{
   C_INT   *pExtrCls;
   int     unlocked;
   C_CLASS *pClass;
   
   // "\nVerifica disegni delle classi di entit� della sessione <%s>..."
   acutPrintf(gsc_msg(660), get_name());

   // cancello eventuali DWK per le classi che non sono in uso
   pExtrCls = (C_INT *) ClsCodeList.get_head();
   while (pExtrCls)
   {
      // Ritorna il puntatore alla classe cercata
      if ((pClass = pPrj->find_class(pExtrCls->get_key())) == NULL)
         return GS_BAD; 

      if (pClass->DWGsUnlock(&unlocked) == GS_GOOD)
      {  
         acutPrintf(_T("\n%s%s"), gsc_msg(230), pClass->get_name());  // "Classe: "
         if (unlocked) acutPrintf(_T("; %s"), gsc_msg(661)); // "Disegni sbloccati."
      }
       
      pExtrCls = (C_INT *) pExtrCls->get_next();
   }

   return GS_GOOD;
}


/*********************************************************
/*.doc C_WRK_SESSION::getLinksTabInfo               <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente la lista dei link della sessione di lavoro.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              la lista dei link della sessione di lavoro (default = NULL)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::getLinksTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
   {
      C_STRING Catalog;

      Catalog = dir;
      Catalog += _T('\\');
      Catalog += GEOTEMPSESSIONDIR;
      Catalog += _T('\\');
      Catalog += GEOWRKSESSIONDB;
      pTableRef->paste(pConn->get_FullRefTable(Catalog.get_name(), NULL, GS_LINK));
   }

   return GS_GOOD;
}


/*********************************************************
/*.doc C_WRK_SESSION::getDeletedTabInfo             <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente la lista delle entit� cancellate 
  nella sessione di lavoro.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              la lista delle entit� cancellate nella sessione di lavoro (default = NULL)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::getDeletedTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
   {
      C_STRING Catalog;

      Catalog = dir;
      Catalog += _T('\\');
      Catalog += GEOTEMPSESSIONDIR;
      Catalog += _T('\\');
      Catalog += GEOWRKSESSIONDB;
      pTableRef->paste(pConn->get_FullRefTable(Catalog.get_name(), NULL, GS_DELETED));
   }

   return GS_GOOD;
}


/*********************************************************
/*.doc C_WRK_SESSION::getVerifiedTabInfo            <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella temporanea usata dal comando di filtro.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              la lista delle entit� cancellate nella sessione di lavoro (default = NULL)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::getVerifiedTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
   {
      C_STRING Catalog;

      Catalog = dir;
      Catalog += _T('\\');
      Catalog += GEOTEMPSESSIONDIR;
      Catalog += _T('\\');
      Catalog += GEOWRKSESSIONDB;
      pTableRef->paste(pConn->get_FullRefTable(Catalog.get_name(), NULL, GS_VERIFIED));
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_WRK_SESSION::SelClasses                     <external> */
/*+
  Questa funzione seleziona una lista di classi da estrarre con 
  la prossima estrazione nella sessione di lavoro.
  Parametri:
  C_INT_LIST &lista;    lista delle classi da selezionare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::SelClasses(C_INT_LIST &lista)
{
   C_INT      *p;
   C_CLASS    *pCls;
   int        cls, i = 1, qty_in_file_class;
   C_STRING   pathfile, ClsPathFile;
   TCHAR      buf[ID_PROFILE_LEN], entry[ID_PROFILE_LEN];
   C_ID       *pId;
   TCHAR      *read_buffer=NULL;
   C_INT_LIST in_file_class_list;
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_B2STR                 *pProfileEntry;
   bool                    Unicode = false;

   // verifico abilitazione
   if (gsc_check_op(opSelAreaClass) == GS_BAD) return GS_BAD;
     
   // controllo che l'area in cui si vuole selezionare le classi sia la corrente
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION != this)
      { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   if (!pPrj) { GS_ERR_COD = eGSNotCurrentPrj; return GS_BAD; }

   // ricavo le classi gi� scritte in CLASS.GEO
   get_TempInfoFilePath(pathfile);
   
   if (gsc_path_exist(pathfile) == GS_GOOD)
   {
      if (gsc_read_profile(pathfile, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;
      swprintf(entry, ID_PROFILE_LEN, _T("%d"), i);
      if ((ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(CLASSES_LIST_SEZ)))
         while ((pProfileEntry = (C_B2STR *) ProfileSection->get_ptr_EntryList()->search(entry)))
         {
            swscanf(pProfileEntry->get_name2(), _T("%d"), &cls);      
            in_file_class_list.add_tail_int(cls);
            swprintf(entry, ID_PROFILE_LEN, _T("%d"), ++i);
         }
   }

   qty_in_file_class = i - 1;
   i = 1;

   // deseleziono le classi da estrarre (funzione che apre e chiude GS_CLASS_FILE)
   //if (gsc_desel_all_area_class() == GS_BAD) return GS_BAD;
   if (PROFILE_INT_FUNC(gsc_desel_all_area_class) == GS_BAD) return GS_BAD;

   p = (C_INT *) lista.get_head();
   while (p != NULL)
   {
      cls = p->get_key();
   
      // Ritorna il puntatore alla classe cercata
      if ((pCls = pPrj->find_class(cls)) == NULL) return GS_BAD;

      pId = pCls->ptr_id();

      if (gsc_isCompatibGEOsimVersion(pId->Version.get_name()) == GS_BAD)
      {
         // "\n*Attenzione* La classe <%s> non � compatibile con la versione attuale di GEOsim."
         acutPrintf(gsc_msg(65), pId->name);
         p = (C_INT *) lista.get_next(); // classe successiva
         continue;
      }

      switch (pId->sel)
      {
         case DESELECTED: pId->sel = SELECTED; break;
         case EXTRACTED : pId->sel = EXTR_SEL; break;
         default: break;
      }      

      if (get_level() == GSReadOnlyData) pId->abilit = GSReadOnlyData;

      if (pId->category == CAT_EXTERN)
      {
         C_SUB *pSub;
         
         pSub = (C_SUB *) pCls->ptr_sub_list()->get_head();

         while (pSub)
         {
            pId  = pSub->ptr_id();

            switch (pId->sel)
            {
               case DESELECTED: pId->sel = SELECTED; break;
               case EXTRACTED : pId->sel = EXTR_SEL; break;
               default: break;
            } 

            pSub = (C_SUB *) pSub->get_next();
         }
      }

      get_TempInfoFilePath(ClsPathFile, pCls->ptr_id()->code);

      if (in_file_class_list.search_key(pCls->ptr_id()->code))
      {  // scrivo la nuova selezione su file
         if (pCls->ToFile_id(ClsPathFile) == GS_BAD) return GS_BAD;
      }
      else
      {  // scrivo la classe su file
         if (pCls->ToFile(ClsPathFile) == GS_BAD) return GS_BAD;
         // aggiungo la classe nella sezione CLASSES_LIST_SEZ
         swprintf(buf, ID_PROFILE_LEN, _T("%d"), pCls->ptr_id()->code);
         swprintf(entry, ID_PROFILE_LEN, _T("%d"), ++qty_in_file_class);
         ProfileSections.set_entry(CLASSES_LIST_SEZ, entry, buf);
      }

      p = (C_INT *) lista.get_next();
   }

   return gsc_write_profile(pathfile, ProfileSections, Unicode);
}


/*********************************************************/
/*.doc C_WRK_SESSION::TerminateSQL                   <internal> */
/*+
  Questa funzione termina i recordset e i comandi eventualmente 
  inizializzati e pone la connessione OLE-DB nulla.
  C_DBCONNECTION *pConnToTerminate; Opzionale; connessione da terminare,
                                    se non passata termina tutte le connessioni
                                    (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_WRK_SESSION::TerminateSQL(C_DBCONNECTION *pConnToTerminate)
{
   bool Terminate = true;

   if (pConnToTerminate && pConnToTerminate != pConn) Terminate = false;

   if (Terminate)
   {
      gsc_DBCloseRs(RsInsertLink);
      gsc_DBCloseRs(RsSelectLink);
      gsc_DBCloseRs(RsInsertDeleted);

      if (CmdSelectLinkWhereCls.GetInterfacePtr()) CmdSelectLinkWhereCls.Release();
      if (CmdSelectLinkWhereKey.GetInterfacePtr()) CmdSelectLinkWhereKey.Release();
      if (CmdSelectLinkWhereHandle.GetInterfacePtr()) CmdSelectLinkWhereHandle.Release();

      pConn = NULL;
   }
}


/*********************************************************/
/*.doc C_WRK_SESSION::HasBeenFrozen                  <internal> */
/*+
  Questa funzione determina se la sessione � stata congelata.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::HasBeenFrozen(void)
{
   C_STRING OldPath;

   // per sapere se la sessione � stata congelata � sufficiente verificare
   // l'esistenza del sottodirettorio OLD del direttorio della sessione
   OldPath = dir;
   OldPath += _T('\\');
   OldPath += GEOOLDSESSIONDIR;

   return gsc_path_exist(OldPath);
}


/*********************************************************/
/*.doc C_WRK_SESSION::ActiveButPreviouslyFrozen                  <internal> */
/*+
  Questa funzione determina se la sessione corrente sta utilizzando 
  un disegno esistente che si chiama "DRAW.DWG" nel direttorio "TEMP" 
  sotto il direttorio della sessione e non esiste il direttorio "OLD" 
  sotto il direttorio della sessione:
  la sessione era stata precedentemente congelata, salvando e quindi uscendo, 
  non veniva cancellato il direttorio della sessione perch� il 
  disegno (DRAW.DWG) era ancora usato da AutoCAD.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::ActiveButPreviouslyFrozen(void)
{
   resbuf   rbDWGTITLED, rbDWGPREFIX, rbDWGNAME;
   C_STRING dir2Compare(dir);

   if (acedGetVar(_T("DWGTITLED"), &rbDWGTITLED) != RTNORM ||
       acedGetVar(_T("DWGPREFIX"), &rbDWGPREFIX) != RTNORM ||
       acedGetVar(_T("DWGNAME"), &rbDWGNAME) != RTNORM)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }

   dir2Compare += _T("\\");
   dir2Compare += GEOTEMPSESSIONDIR;
   dir2Compare += _T("\\");

   // se la sessione corrente sta utilizzando un disegno esistente che si chiama
   // "DRAW.DWG" nel direttorio "TEMP" sotto il direttorio della sessione e non 
   // esiste il direttorio "OLD" sotto il direttorio della sessione:
   // la sessione era stata precedentemente congelata, salvando e quindi uscendo, 
   // non veniva cancellato il direttorio della sessione perch� il 
   // disegno (DRAW.DWG) era ancora usato da AutoCAD.
   if (HasBeenFrozen() == GS_BAD &&
       (rbDWGTITLED.restype == RTSHORT && rbDWGTITLED.resval.rint == 1) && // sto usando un disegno esistente
       (rbDWGPREFIX.restype == RTSTR && dir2Compare.comp(rbDWGPREFIX.resval.rstring, FALSE) == 0) && // nella path TEMP della sessione
       (rbDWGNAME.restype == RTSTR && gsc_strcmp(rbDWGNAME.resval.rstring, _T("DRAW.DWG"), FALSE) == 0)) // che si chiama DRAW.DWG
      return GS_GOOD;
   else
      return GS_BAD;
}


/*********************************************************/
/*.doc C_WRK_SESSION::UnlockWaitingClsList           <external> */
/*+
  Questa funzione prova a sbloccare tutte le classi che erano
  in attesa di essere sbloccate ma che per motivi legati alla 
  multiutenza non � stato possibile sbloccare.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::UnlockWaitingClsList(void)
{
   C_INT *pClsCode;
   int   res = GS_GOOD;

   pClsCode = (C_INT *) WaitingClsToUnlockList.get_head();
   while (pClsCode)
   {  // Sblocco della classe
      if (pPrj->find_class(pClsCode->get_key())->set_unlocked(0, GS_GOOD) == GS_GOOD)
      {
         WaitingClsToUnlockList.remove_at(); // cancella e va al successivo
         pClsCode = (C_INT *) WaitingClsToUnlockList.get_cursor();
      }
      else
      {
         pClsCode = (C_INT *) pClsCode->get_next();
         res      = GS_BAD;
      }
   }

   return res;
}


/*********************************************************/
/*.doc C_WRK_SESSION::AddClsToUnlockWaitingClsList           <external> */
/*+
  Questa funzione aggiunge una classe alla lista delle classi da 
  sbloccare.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_WRK_SESSION::AddClsToUnlockWaitingClsList(int cls)
{
   C_INT *pClsCode;

   if (WaitingClsToUnlockList.search_key(cls)) return GS_GOOD;

   if ((pClsCode = new C_INT(cls)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   WaitingClsToUnlockList.add_head(pClsCode);

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////
///////////        FINE   FUNZIONI C_WRK_SESSION         ///////////
/////////////////////////////////////////////////////////////


/****************************************************************************/
/*.doc gsc_extract_classes                                       <external> */
/*+
  Questa funzione crea una sessione di lavoro ed estrae le classi selezionate
  nella posizione desiderata (se non specificata sar� "tutto").
  Parametri:
  C_PROJECT &Prj;          Oggetto C_PROJECT
  C_WRK_SESSION &session;         Oggetto C_WRK_SESSION
  C_INT_LIST &ClsList;     Lista di codici delle classi da estrarre
  int Exclusive;           Flag di estrazione se TRUE la funzione estrae le classi
                           rendendole, agli altri utenti, in sola lettura.
                           In caso di fallimento le classe saranno estratte 
                           in sola lettura (default = FALSE)
  presbuf SpatialCond;     Condizione spaziale (default = NULL -> "tutto")
  presbuf PropertyCond;    Condizione di propriet� (default = NULL)
  int     PropertyNotMode; Se TRUE la condizione property viene valutata in
                           forma NOT (default = FALSE).
  int mode;                Modo estrazione (EXTRACTION o PREVIEW); default = EXTRACTION
  C_SELSET *pExtractedSS;  Opzionale; puntatore a gruppo di selezione 
                           oggetti estratti (default = NULL).
  int retest;              Se MORETESTS -> in caso di errore riprova n volte 
                           con i tempi di attesa impostati poi ritorna GS_BAD,
                           ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                           (default = MORETESTS).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int gsc_extract_classes(C_PROJECT *pPrj, C_WRK_SESSION &session, C_INT_LIST &ClsList,
                        int Exclusive, presbuf SpatialCond, presbuf PropertyCond,
                        int PropertyNotMode, int mode, C_SELSET *pExtractedSS, int retest)
{
   C_RB_LIST     p_estract_cond;
   int           result = GS_BAD;
   C_WRK_SESSION *pCurrSession;
   C_INT_INT_LIST ErrClsCodeList;

   if ((pCurrSession = new C_WRK_SESSION(pPrj)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   session.copy(pCurrSession);

   // creazione di una sessione di lavoro
   if (pCurrSession->create() == 0)
      { delete pCurrSession; return GS_BAD; }

   do
   {
      if (!SpatialCond)
      {
         // imposto condizione spaziale = "tutto"
         if ((p_estract_cond << acutBuildList(RTLB, RTSTR, ALL_SPATIAL_COND, RTLE, 0)) == NULL) break;
         if (ade_qrydefine(GS_EMPTYSTR, GS_EMPTYSTR, GS_EMPTYSTR, _T("Location"), 
                           p_estract_cond.get_head(), GS_EMPTYSTR) == ADE_NULLID)
            { GS_ERR_COD = eGSQryCondNotDef; break; }
      }
      else
         if (ade_qrydefine(GS_EMPTYSTR, GS_EMPTYSTR, GS_EMPTYSTR, _T("Location"), SpatialCond, GS_EMPTYSTR) == ADE_NULLID)
            { GS_ERR_COD = eGSQryCondNotDef; break; }

      if (PropertyCond)
         if (ade_qrydefine(_T("and"), GS_EMPTYSTR,
                           (PropertyNotMode) ? _T("not") : GS_EMPTYSTR,
                           _T("Property"), PropertyCond, GS_EMPTYSTR) == ADE_NULLID)
            { GS_ERR_COD = eGSQryCondNotDef; break; }

      // salvo la condizione chiamandola con la categoria e il nome di default
      // (rispettivamente "estrazione" e "spaz_estr")
      if (gsc_save_qry() == GS_BAD) break;

      // seleziono classe da estrarre
      if (pCurrSession->SelClasses(ClsList) == GS_BAD) break;

      // effettuo estrazione rendendo la classe in sola lettura alle altre sessioni
      if (pCurrSession->extract(mode, Exclusive, &ErrClsCodeList, pExtractedSS, retest) == GS_BAD ||
          ErrClsCodeList.get_count() > 0) break;

      if (Exclusive) // se si voleva le classi in esclusiva
      {
         C_INT *pCls = (C_INT *) ClsList.get_head();
         GSDataPermissionTypeEnum abilit;

         result = GS_GOOD;
         while (pCls)
         {
            abilit = pPrj->find_class(pCls->get_key())->ptr_id()->abilit;

            // se la classe non � stata estratta in modifica
            // (perch� bloccata da un'altra sessione di lavoro)
            if (abilit != GSUpdateableData) 
               { result = GS_BAD; GS_ERR_COD = eGSClassLocked; break; }
   
            pCls = (C_INT *) pCls->get_next();
         }
         if (result == GS_BAD) break;
         result = GS_BAD;
      }

      result = GS_GOOD;
   }
   while (0);

   // uscita dalla sessione di lavoro
   if (result == GS_BAD)
   {  // rilascio tutti i DWG altrimenti non si potrebbe
      // ristorare il backup dei DWG che risulterebbero in uso
      C_INT   *pClsCode = (C_INT *) ClsList.get_head();
      C_CLASS *pCls;

      while (pClsCode)
      {
         if ((pCls = pPrj->find_class(pClsCode->get_key())) != NULL)
            pCls->GphDetach();

         pClsCode = (C_INT *) pClsCode->get_next();
      }

      if (gsc_ExitCurrSession() == GS_BAD) return GS_BAD;
   }

   return result;
}


/*********************************************************/
/*.doc gsc_is_extractable                     <internal> */
/*+                                                                       
  Verifica che la classe sia nella condizione in cui si 
  possa estrarre.
  Parametri:
  C_INT_INT_LIST &ClassList_to_extracts;  lista classi da salvare
  C_INT_INT      *cls;                    classe da salvare (codice e tipo)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_is_extractable(C_INT_INT_LIST &ClassList_to_extract, C_INT_INT *cls)
{
   C_INT_INT 	 *subcls, *subcls_lista_cls;
   C_CLASS   	 *pCls, *pComponentCls;
   C_GROUP_LIST *p_group_list;
   C_LONG_LIST   LockedBy;
   int           Mode, State;

   if (cls->get_type() == SELECTED) return GS_BAD;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls->get_key())) == NULL) return GS_BAD;

   if (gsc_get_class_lock(GS_CURRENT_WRK_SESSION->get_PrjId(), cls->get_key(),
                          LockedBy, &Mode, &State) == GS_BAD)
      return GS_BAD;

   // Se era in uso in modo esclusivo o in fase di estrazione da parte di un altro utente
   if ((Mode == EXCLUSIVEBYANOTHER || State == WRK_SESSION_SAVE) && 
       LockedBy.search(GS_CURRENT_WRK_SESSION->get_id()) == NULL)
   {
      pCls->ptr_id()->abilit = GSExclusiveUseByAnother;
      GS_ERR_COD = eGSClassLocked;
      return GS_BAD;
   }
   else
      // Se era in uso in modo esclusivo da parte di un altro utente
      if (pCls->ptr_id()->abilit == EXCLUSIVEBYANOTHER)      
         // rileggo il giusto livello di abilitazione
         pCls->ptr_id()->refresh_abilit();   // classi modificabili 

   if ((p_group_list = pCls->ptr_group_list()) != NULL)
   { // verifico che le classi che compongono il gruppo siano in lista
      subcls = (C_INT_INT *) p_group_list->get_head();
      while (subcls != NULL)
      {
         if ((pComponentCls = GS_CURRENT_WRK_SESSION->find_class(subcls->get_key())) == NULL)
            return GS_BAD;
         // Se la classe componente il gruppo NON � gi� stata estratta
         if (pComponentCls->is_extracted() == GS_BAD)
         {
            if ((subcls_lista_cls = (C_INT_INT *) ClassList_to_extract.search_key(subcls->get_key())) == NULL)
               { GS_ERR_COD = eGSMemberClassNotAlreadyExtracted; return GS_BAD; }
            // se non � gi� stata estratta
            if (subcls_lista_cls->get_type() != SELECTED)
               { GS_ERR_COD = eGSMemberClassNotAlreadyExtracted; return GS_BAD; }
         }

         if (pComponentCls->ptr_id()->abilit == EXCLUSIVEBYANOTHER)
            { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

         subcls = (C_INT_INT *) subcls->get_next();
	   }
   }
  
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_is_completed_extracted             <internal> */
/*+                                                                       
  Verifica che tutte le classi siano state estratte.
  Parametri:
  C_INT_INT_LIST &ClassList_to_extract;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_is_completed_extracted(C_INT_INT_LIST &ClassList_to_extract)
{
   C_INT_INT *cls;

   cls = (C_INT_INT *) ClassList_to_extract.get_head();
   while (cls != NULL)
   {
      if (cls->get_type() == DESELECTED) return FALSE;
      cls = (C_INT_INT *) cls->get_next();
   }

   return TRUE;
}


/*********************************************************/
/*.doc gs_destroysession                      <external> */
/*+
  Questa funzione � la versione funzione LISP della "C_WRK_SESSION::destroy".
  Parametri:
  Lista di resbuf (<Password><codice progetto><codice sessione>)
    
  Restituisce TRUE in caso di successo (anche parziale) altrimenti 
  restituisce nil.
-*/  
/*********************************************************/
int gs_destroysession(void)
{
   presbuf       arg = acedGetArgs();
   int           prj;
   long          SessionId;
   TCHAR         *Password;
   C_WRK_SESSION *pSession;

   acedRetNil();
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   Password = arg->resval.rstring;
   if (!(arg = arg->rbnext) || gsc_rb2Int(arg, &prj) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (!(arg = arg->rbnext) || gsc_rb2Lng(arg, &SessionId) == GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // Ritorna il puntatore all'area cercata
   if ((pSession = gsc_find_wrk_session(prj, SessionId)) == NULL) return RTERROR;
   if (pSession->destroy(Password) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_getLastExtractedClassesFromINI                      <external> */
/*+
  Questa funzione � la versione funzione LISP della "gsc_getLastExtractedClassesFromINI".
  Parametri:
  Lista di resbuf ([<codice progetto>]) // se <codice progetto> non indicato viene usato il progetto corrente
    
  Restituisce la lista dei codici delle classi in caso di successo altrimenti restituisce nil.
-*/  
/*********************************************************/
int gs_getLastExtractedClassesFromINI(void)
{
   presbuf    arg = acedGetArgs();
   int        prj;
   C_INT_LIST ClsList;
   C_RB_LIST  ret;

   acedRetNil();
   if (!arg) // uso il progetto della sessione corrente
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
      else prj = GS_CURRENT_WRK_SESSION->get_PrjId();
   else
      if (gsc_rb2Int(arg, &prj) == GS_BAD)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_getLastExtractedClassesFromINI(prj, ClsList) != GS_GOOD) return RTERROR;
   //ret << acutBuildList(RTSHORT, prj, RTLB, 0);
   ret << ClsList.to_rb();
   //ret += acutBuildList(RTLE, 0);
   ret.LspRetList();

   return RTNORM;
}

/*********************************************************/
/*.doc gsc_getLastExtractedClassesFromINI     <external> */
/*+
  Funzione che legge i codici delle classi estratte l'ultima volta nel progetto.
  Parametri:
  int prj;              codice progetto (input)
  C_INT_LIST &ClsList;  lista dei codici delle classi (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getLastExtractedClassesFromINI(int prj, C_INT_LIST &ClsList)
{
   C_STRING pathfile, entry(_T("LastExtractedClasses_PRJ")), value;

   ClsList.remove_all();
   // scrivo la lista dei codici delle classi in GEOSIM.INI
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;

   entry += prj;

   if (gsc_getInfoFromINI(entry.get_name(), value, &pathfile) == GS_GOOD)
      return ClsList.from_str(value.get_name());

   return GS_GOOD;
}
