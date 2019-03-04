/**********************************************************
Name: GS_SEC
                                   
Module description: File funzioni di base per la gestione
                    delle tabelle secondarie delle classi di entita' 
            
Author: Roberto Poltini, Maria Paola De Prati, Paolo De Sole. 

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

#include "resource.h"

#include "rxdefs.h"   
#include "adslib.h"   
#include "adsdlg.h"       // gestione DCL
#include "AcExtensionModule.h" // per CAcModuleResourceOverride

#include "gs_opcod.h"     // codici delle operazioni
#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori
#include "gs_utily.h"
#include "gs_resbf.h"     // gestione resbuf
#include "gs_list.h"      // gestione liste C++ 
#include "gs_ase.h"
#include "gs_user.h"      // gestione utenti     
#include "gs_lisp.h"
#include "gs_init.h"      // direttori globali
#include "gs_class.h"     // gestione classi di entita' 
#include "gs_sec.h"
#include "gs_prjct.h"
#include "gs_netw.h"      // funzioni di rete
#include "gs_area.h"
#include "gs_graph.h"
#include "gs_query.h"
#include "gs_evid.h"
#include "gs_filtr.h"

#include "d2hMap.h" // doc to help


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


static C_ATTRIB_LIST       *ATTRIB_LIST;    // PUNTATORE LISTA ATTRIBUTI CLASSE ENTITA'


/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/

int gsc_get_reldata(_CommandPtr &pCmd, int Cls, int Sub, long Key, C_RB_LIST &ColValues);
int gsc_get_reldata(_RecordsetPtr &pRs, int Cls, int Sub, long Key, C_RB_LIST &ColValues);

// operazioni della DCL "SecValueTransfer"
#define LOCATION_SEL  DLGOK + 1

// vedi anche GS_FILTR.CPP
static TCHAR *AGGR_FUN_LIST[] = {_T("Avg"),
                                 _T("Count"), 
                                 _T("Count *"), 
                                 _T("Count distinct"), 
                                 _T("Max"),
                                 _T("Min"),
                                 _T("Sum")};

// struttura usata per scambiare dati nella DCL "SecValueTransfer"
struct Common_Dcl_SecValueTransfer_Struct
{
   bool      LocationUse;
   C_STRING  SpatialSel;
   int       Inverse;
   bool      LastFilterResultUse;
   int       Logical_op;
   C_CLASS_SQL_LIST sql_list;
   C_SINTH_SEC_TAB_LIST SinthSecList;
   int       Cls;
   int       Sub;
   int       Sec;
   C_STRING  Source;    // attributo della tab. sec.
   int       AggrUse;
   C_STRING  AggrFun;   // funzione di aggregazione su <Source> (opzionale)
   C_STRING  Dest;      // attributo della classe principale
};

// struttura usata per scambiare dati nella DCL "DynSegmentationAnalyzer"
struct Common_Dcl_DynSegmentationAnalyzer_Struct
{
   bool      LocationUse;
   C_STRING  SpatialSel;
   int       Inverse;
   bool      LastFilterResultUse;
   int       Logical_op;
   C_STRING  SQLCond;
   C_SINTH_SEC_TAB_LIST SinthSecList;
   C_STR_LIST           SecAttribNameList;
   int       Cls;
   int       Sub;
   int       Sec;

   bool      FASConvertion;
   long      EnabledFas;    // Caratteristiche grafiche che possono essere impostate
   long      flag_set;      // Caratteristiche grafiche da variare
   C_FAS     NewFAS;        // caratteristiche grafiche dei nuovi oggetti grafici

   bool      LblCreation;   // Flag di creazione etichette
   C_STRING  LblFun;        // Nome attributo o funzione geolisp per generare il valore delle etichette
   long      Lbl_flag_set;  // Caratteristiche grafiche da variare per le etichette 
   C_FAS     LblFAS;        // caratteristiche grafiche delle etichette

   bool      AddODValues;   // flag per aggiungere o meno i dati oggetto
};


/*********************************************************/
/*.doc gs_getdefaultattrsec <external> */
/*+
  Restituisce la lista degli attributi di default di una tabella secondaria.
  Parametri:
  (("UDL_FILE" <file UDL> || <stringa di connessione>) ("UDL_PROP" <value>))
-*/  
/*********************************************************/
int gs_getdefaultattrsec(void)
{   
   C_DBCONNECTION *pConn;
   presbuf        rb = acedGetArgs();
   C_RB_LIST      ret;

   acedRetNil();

   // Ottengo la connessione OLE-DB
   if ((pConn = gsc_getConnectionFromLisp(rb)) == NULL) return RTERROR;
   
   rb = NULL;
   if (gsc_getdefaultattr(pConn, CAT_SECONDARY, GSInternalSecondaryTable, &rb) == GS_GOOD)
   {
      C_RB_LIST ret(rb);
      ret.LspRetList();
   }

   return RTNORM;
}


/*********************************************************/
/*.doc gs_get_tab_sec                         <external> */
/*+
  Questa funzione lisp legge le caratteristiche di una tabella secondaria.
  Parametri:
  Lista RESBUF (<secj><prj><class>[<sub>])
  out:
  (<id><info><attrib_list>)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_get_tab_sec(void)
{
   C_SECONDARY *pSec;
   presbuf     arg;
   C_RB_LIST   ret;
   int         prj, cls, sub, sec;

   acedRetNil();
   // Legge nella lista dei parametri: progetto classe e sub //
   arg = acedGetArgs();
      
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   // Ritorna il puntatore alla tabella secondaria cercata
   if ((pSec = gsc_find_sec(prj, cls, sub, sec)) == NULL) return RTERROR;

   // Ritorna lista resbuf
   if ((ret << pSec->to_rb()) == NULL) return RTERROR;
   ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_get_sec_attrib_list <external> */
/*+
  Questa funzione lisp legge la struttura di una tabella secondaria.
  Parametri:
  Lista RESBUF (<secj><prj><class>[<sub>])

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_get_sec_attrib_list(void)
{
   C_SECONDARY   *pSec;
   presbuf       arg;
   C_RB_LIST     ret;
   int           prj, cls, sub, sec;
   C_ATTRIB_LIST *attrib;

   acedRetNil();
   // Legge nella lista dei parametri: progetto classe e sub //
   arg = acedGetArgs();
      
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   // Ritorna il puntatore alla tabella secondaria cercata
   if ((pSec = gsc_find_sec(prj, cls, sub, sec)) == NULL) return RTERROR;

   // Ritorna il puntatore alla ATTRIBLIST
   if ((attrib = pSec->ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return RTERROR; }

   // Ritorna lista resbuf
   if ((ret << attrib->to_rb()) == NULL) return RTERROR;
   ret.LspRetList();
   
   return RTNORM;
}


/*********************************************************/
/*.doc gs_get_sec_info <external> */
/*+
  Questa funzione lisp legge la struttura C_INFO_SEC di una tabella
  secondaria.
  Parametri:
  Lista RESBUF (<secj><prj><class>[<sub>])

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_get_sec_info(void)
{
   C_SECONDARY   *pSec;
   presbuf       arg;
   C_RB_LIST     ret;
   int           prj, cls, sub, sec;
   C_INFO_SEC    *pinfo;

   acedRetNil();
   // Legge nella lista dei parametri: progetto classe e sub //
   arg = acedGetArgs();
      
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   // Ritorna il puntatore alla tabella secondaria cercata
   if ((pSec = gsc_find_sec(prj, cls, sub, sec)) == NULL) return RTERROR;

   // Ritorna il puntatore alla INFO_SEC
   if ((pinfo = pSec->ptr_info()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return RTERROR; }

   // Ritorna lista resbuf
   if ((ret << pinfo->to_rb()) == NULL) return RTERROR;
   ret.LspRetList();
   
   return RTNORM;
}


/*********************************************************/
/*.doc gs_mod_sec_link <external> */
/*+
  Questa funzione lisp modifica il link tra una classe GEOSIM ed una tabella
  secondaria esterna.
  Parametri:
  Lista RESBUF (<secj><prj><class><sub><key_pri><key_attrib>)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_mod_sec_link(void)
{
   presbuf arg, pVal;
   int     prj, cls, sub, sec;
   TCHAR   *key_pri, *key_attrib;

   acedRetNil();
   // Legge nella lista dei parametri: progetto classe e sub //
   arg = acedGetArgs();
      
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   // key_pri
   if (!(pVal = gsc_CdrAssoc(_T("KEY_PRI"), arg, FALSE)) || pVal->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   key_pri = pVal->resval.rstring;
   // key_attrib
   if (!(pVal = gsc_CdrAssoc(_T("KEY_ATTRIB"), arg, FALSE)) || pVal->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   key_attrib = pVal->resval.rstring;

   if (gsc_mod_sec_link(prj, cls, sub, sec, key_pri, key_attrib) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_mod_sec_vis <external> */
/*+
  Questa funzione lisp modifica la chiave di visualizzazione delle schede di una
  tabella secondaria.
  Parametri:
  Lista RESBUF (<secj><prj><class><sub>(("VIS_ATTRIB" <value>)[("VIS_ATTRIB_DESC_ORDER_TYPE" <value>)]))

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_mod_sec_vis(void)
{
   presbuf  arg, pVal;
   int      prj, cls, sub, sec;
   C_STRING vis_attrib;
   bool     vis_attrib_desc_order_type = false;

   acedRetNil();
   // Legge nella lista dei parametri: progetto classe e sub //
   arg = acedGetArgs();
      
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   // Vis_attrib
   if (!(pVal = gsc_CdrAssoc(_T("VIS_ATTRIB"), arg, FALSE)))
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (pVal == NULL || (pVal->restype != RTSTR && pVal->restype != RTNIL))
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (pVal->restype == RTNIL || pVal->restype == RTNONE) vis_attrib = GS_EMPTYSTR;
   else vis_attrib = pVal->resval.rstring;
   
   if (vis_attrib.len() > 0)
      // vis_attrib_desc_order_type
      if ((pVal = gsc_CdrAssoc(_T("VIS_ATTRIB_DESC_ORDER_TYPE"), arg, FALSE)) && pVal->restype != RTNIL)
         vis_attrib_desc_order_type = true; // ordine descrescente

   if (gsc_mod_sec_vis(prj, cls, sub, sec, 
                       vis_attrib, vis_attrib_desc_order_type) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_mod_sec_dynamic_segmentation        <external> */
/*+
  Questa funzione lisp modifica le informazioni per la gestione della 
  segmentazione dinamica di una tabella secondaria.
  Parametri:
  Lista RESBUF (<secj><prj><class><sub>
                  ([("REAL_INIT_DISTANCE_ATTRIB" <valore>)]
                   [("REAL_FINAL_DISTANCE_ATTRIB" <valore>)]
                   [("NOMINAL_INIT_DISTANCE_ATTRIB" <valore>)]
                   [("NOMINAL_FINAL_DISTANCE_ATTRIB" <value>)]
                   [("REAL_OFFSET_ATTRIB" <value>)])

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_mod_sec_dynamic_segmentation(void)
{
   presbuf    arg;
   int        prj, cls, sub, sec;
   C_INFO_SEC InfoSec;

   acedRetNil();
   // Legge nella lista dei parametri: progetto classe e sub //
   arg = acedGetArgs();
      
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   if (InfoSec.from_rb_dynamic_segmentation_info(arg) == GS_BAD) return RTERROR;

   if (gsc_mod_sec_dynamic_segmentation(prj, cls, sub, sec,
                                        InfoSec.real_init_distance_attrib,
                                        InfoSec.real_final_distance_attrib,
                                        InfoSec.nominal_init_distance_attrib, 
                                        InfoSec.nominal_final_distance_attrib,
                                        InfoSec.real_offset_attrib) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_mod_sec_name <external> */
/*+
  Questa funzione lisp modifica il nome di una tabella secondaria.
  Parametri:
  Lista RESBUF (<secj><prj><class><sub><name>)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_mod_sec_name(void)
{
   presbuf arg;
   int     prj, cls, sub, sec;
   TCHAR   *name;

   acedRetNil();
   // Legge nella lista dei parametri: progetto classe e sub
   arg = acedGetArgs();
      
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   // nome secondaria
   if (arg == NULL || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   name = arg->resval.rstring;

   if (gsc_mod_sec_name(prj, cls, sub, sec, name) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_mod_sec_descr                       <external> */
/*+
  Questa funzione lisp modifica la descrizione di una tabella secondaria.
  Parametri:
  Lista RESBUF (<secj><prj><class><sub><Descr>)

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR.
-*/  
/*********************************************************/
int gs_mod_sec_descr(void)
{
   presbuf arg;
   int     prj, cls, sub, sec;
   TCHAR   *NewDescr;

   acedRetNil();
   // Legge nella lista dei parametri: progetto classe e sub //
   arg = acedGetArgs();
      
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;

   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   // nome secondaria
   if (arg == NULL || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   NewDescr = arg->resval.rstring;

   if (gsc_mod_sec_descr(prj, cls, sub, sec, NewDescr) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_create_tab_sec                      <external> */
/*+
  Questa funzione LISP crea una tabella secondaria di GEOsim.
  Parametri:
  Lista RESBUF (<prj><class><sub><nome><descr><info>[<dbms>[attrib[<vis>]]])
  
  Restituisce il codice della tabella secondaria in caso di successo
  altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_create_tab_sec(void)
{
   C_SECONDARY *pSec = NULL;
   C_RB_LIST   list;
   presbuf     arg, p = NULL;
   int         prj, cls, sub, new_code, res = GS_BAD;

   acedRetNil();
   arg = acedGetArgs();

   // Alloca un' oggetto SECONDARY
   if ((pSec = new C_SECONDARY) == NULL) { GS_ERR_COD = eGSOutOfMem; return RTERROR; }
   
   do
   {
      // Codice progetto
      if ((p = gsc_nth(0, arg)) == NULL) break;
      if (p->restype == RTSHORT)
         prj = p->resval.rint;
      else { GS_ERR_COD = eGSInvRBType; break; }
      // Codice classe
      if ((p = gsc_nth(1, arg)) == NULL) break;
      if (p->restype == RTSHORT)
         cls = p->resval.rint;
      else { GS_ERR_COD = eGSInvRBType; break; }
      // Codice sotto classe
      if ((p = gsc_nth(2, arg)) == NULL) break;
      if (p->restype == RTSHORT)
         sub = p->resval.rint;
      else { GS_ERR_COD = eGSInvRBType; break; }
      // Nome della secondaria
      if ((p = gsc_nth(3, arg)) == NULL) break;
      if (p->restype == RTSTR)
         gsc_strcpy(pSec->name, p->resval.rstring, MAX_LEN_CLASSNAME);
      else { GS_ERR_COD = eGSInvRBType; break; }
      // Descrizione della secondaria
      if ((p = gsc_nth(4, arg)) == NULL) break;
      if (p->restype == RTSTR) pSec->Descr = p->resval.rstring;
      // Carico la INFO della secondaria
      if ((p = gsc_nth(5, arg)) == NULL) break;
      if (p->restype == RTLB)
         if (pSec->ptr_info()->from_rb(p) == GS_BAD) break; 
      // Carico anche la struttura della tabella secondaria di GEOsim
      if ((p = gsc_nth(6, arg)) == NULL) break;
      if (p->restype == RTLB)
         if (pSec->ptr_attrib_list()->from_rb(p) == GS_BAD) break;
      // Creo la secondaria
      if ((new_code = gsc_create_tab_sec(prj, cls, sub, pSec)) == GS_BAD) break;
      
      res = GS_GOOD;
   }
   while (0);

   if (res == GS_BAD) { delete pSec; return RTERROR; }

   acedRetInt(new_code);

   return RTNORM;
}


/*********************************************************/
/*.doc gs_link_tab_sec <external> */
/*+
  Questa funzione LISP collega una tabella secondaria esterna ad
  una classe di GEOsim.
  Parametri:
  Lista RESBUF (<prj><class><sub><nome><descr><dir><key_pri><key_attrib>
                                            [vis_attrib[<dbms>]])
  
  Restituisce il codice della tabella secondaria in caso di successo
  altrimenti restituisce NIL.
-*/  
// (gs_link_tab_sec 1 1 0 "Cinghiate" )
/*********************************************************/
int gs_link_tab_sec(void)
{
   C_SECONDARY *pSec = NULL;
   presbuf     arg, p = NULL;
   int         prj, cls, sub, res = GS_BAD, new_code;
   
   acedRetNil();
   arg = acedGetArgs();

   // Alloca un' oggetto SECONDARY
   if ((pSec = new C_SECONDARY) == NULL) { GS_ERR_COD = eGSOutOfMem; return RTERROR; }

   do
   {
      // Codice progetto
      if ((p = gsc_nth(0, arg)) == NULL) break;
      if (p->restype == RTSHORT)
         prj = p->resval.rint;
      else { GS_ERR_COD = eGSInvRBType; break; }
      // Codice classe
      if ((p = gsc_nth(1, arg)) == NULL) break;
      if (p->restype == RTSHORT)
         cls = p->resval.rint;
      else { GS_ERR_COD = eGSInvRBType; break; }
      // Codice sotto classe
      if ((p = gsc_nth(2, arg)) == NULL) break;
      if (p->restype == RTSHORT)
         sub = p->resval.rint;
      else { GS_ERR_COD = eGSInvRBType; break; }
      // Nome della secondaria
      if ((p = gsc_nth(3, arg)) == NULL) break;
      if (p->restype == RTSTR)
         gsc_strcpy(pSec->name, p->resval.rstring, MAX_LEN_CLASSNAME);
      else { GS_ERR_COD = eGSInvRBType; break; }
      // Descrizione della secondaria
      if ((p = gsc_nth(4, arg)) == NULL) break;
      if (p->restype == RTSTR) pSec->Descr = p->resval.rstring;
      // Carico la INFO della secondaria
      if ((p = gsc_nth(5, arg)) == NULL) break;
      if (p->restype == RTLB)
         if (pSec->ptr_info()->from_rb(p) == GS_BAD) break; 
      // Creo la secondaria
      if ((new_code = gsc_create_extern_secondary(prj, cls, sub, pSec)) == GS_BAD) break; 

      res = GS_GOOD;
   }
   while (0);

   if (res == GS_BAD) { delete pSec; return RTERROR; }

   acedRetInt(new_code);

   return RTNORM;
}

                                                  
/*********************************************************/
/*.doc gs_del_sec <external> */
/*+
  Questa funzione LISP cancella una tabella secondaria di entità di GEOsim.
  Parametri:
  Lista RESBUF (<Password><sec><prj><class>[<sub>])
    
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_del_tab_sec(void)
{
   presbuf arg = acedGetArgs();
   int     prj, cls, sub, sec;
   TCHAR   *Password;

   acedRetNil();

   if (!arg || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   Password = arg->resval.rstring;

   if (!(arg = arg->rbnext) || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   sec = (int) arg->resval.rint;
   arg = arg->rbnext;
   
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   
   // Ritorna il puntatore alla tabella secondaria cercata
   if (gsc_del_tab_sec(prj, cls, sub, sec, Password) == NULL) return RTERROR;

   acedRetT();
   return RTNORM;
}
 
                         

/*********************************************************/
/*.doc gsc_del_tab_sec <external> */
/*+
  Questa funzione cancella una tabella secondaria di GEOsim.
  Parametri:
  int prj;               codice progetto
  int cls;               codice classe
  int sub;               codice sotto-classe
  int sec;               codice tabella secondaria
  const TCHAR *Password; Password dell'utente corrente (per controllo)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_del_tab_sec(int prj, int cls, int sub, int sec, const TCHAR *Password)
{
   C_CLASS      *pCls;
   C_SECONDARY  *pSec;

   // Ricavo il puntatore alla classe
   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   // Cerca la secondaria solo in memoria
   if ((pSec = (C_SECONDARY *) pCls->find_sec(sec)) == NULL) 
      return GS_BAD;

   // cancello secondaria                  
   if (pSec->del(Password) == NULL) return GS_BAD;

   // cancello la tabella secondaria dalla lista
   if ((pCls->ptr_secondary_list->remove(pSec)) == GS_BAD) return GS_BAD;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_mod_sec_link <internal> */
/*+
  Questa funzione modifica i valori della C_ID di una classe.
  Parametri:
  int prj;                 codice progetto             
  int cls;                 codice classe
  int sub;                 codice sottoclasse
  int sec;                 codice secondaria
  const TCHAR *key_pri;    nome attributo classe
  const TCHAR *key_attrib; nome attributo tabella secondaria         
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_mod_sec_link(int prj, int cls, int sub, int sec, 
                     const TCHAR *key_pri, const TCHAR *key_attrib)
{
   C_CLASS     *pCls;
   C_SECONDARY *pSec;
   C_INFO_SEC  *pinfo;
   long        SessionId;

   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   // verifico se la classe madre fa parte di una sessione di lavoro
   if (pCls->is_inarea(&SessionId) == GS_BAD || SessionId > 0)
      { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
   
   if ((pSec = (C_SECONDARY *) pCls->find_sec(sec)) == NULL) return GS_BAD;

   if ((pinfo = new C_INFO_SEC) == NULL) { GS_ERR_COD = eGSOutOfMem; return NULL; }
   
   pinfo->key_attrib = key_attrib;
   pinfo->key_pri    = key_pri;
   
   // modifico info secondaria                  
   if (pSec->mod_link(pinfo) == GS_BAD)
      { delete pinfo; return GS_BAD; }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_mod_sec_vis <internal> */
/*+
  Questa funzione modifica il valore di vis_attrib di una tabella
  secondaria.
  Parametri:
  int prj;              codice progetto             
  int cls;              codice classe
  int sub;              codice sottoclasse
  int sec;              codice secondaria
  C_STRING &vis_attrib; nome attributo tabella secondaria
  bool vis_attrib_desc_order_type;  Flag opzionale. Se vero l'ordine deve essere
                                    decrescente (default = falso)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_mod_sec_vis(int prj, int cls, int sub, int sec, C_STRING &vis_attrib,
                    bool vis_attrib_desc_order_type)
{
   C_CLASS     *pCls;
   C_SECONDARY *pSec;
   long        SessionId;

   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   // verifico se la classe madre fa parte di un'area di lavoro
   if (pCls->is_inarea(&SessionId) == GS_BAD || SessionId > 0)
      { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
   
   if ((pSec = (C_SECONDARY *) pCls->find_sec(sec)) == NULL) return GS_BAD;
   
   // modifico info secondaria                  
   if (pSec->mod_vis(vis_attrib, vis_attrib_desc_order_type) == GS_BAD)
      return GS_BAD;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_mod_sec_vis <internal> */
/*+
  Questa funzione modifica il valore di vis_attrib di una tabella
  secondaria.
  Parametri:
  int prj;              codice progetto             
  int cls;              codice classe
  int sub;              codice sottoclasse
  int sec;              codice secondaria
  C_STRING &_real_init_dist_attrib; 
  C_STRING &_real_final_dist_attrib;
  C_STRING &_nominal_init_dist_attrib;
  C_STRING &_nominal_final_dist_attrib;
  C_STRING &_real_offset_attrib;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_mod_sec_dynamic_segmentation(int prj, int cls, int sub, int sec, 
                                     C_STRING &_real_init_dist_attrib,
                                     C_STRING &_real_final_dist_attrib, 
                                     C_STRING &_nominal_init_dist_attrib, 
                                     C_STRING &_nominal_final_dist_attrib,
                                     C_STRING &_real_offset_attrib)
{
   C_CLASS     *pCls;
   C_SECONDARY *pSec;
   long        SessionId;

   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   // verifico se la classe madre fa parte di un'area di lavoro
   if (pCls->is_inarea(&SessionId) == GS_BAD || SessionId > 0)
      { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
   
   if ((pSec = (C_SECONDARY *) pCls->find_sec(sec)) == NULL) return GS_BAD;
   
   // modifico secondaria                  
   if (pSec->mod_dynamic_segmentation(_real_init_dist_attrib,
                                      _real_final_dist_attrib,
                                      _nominal_init_dist_attrib,
                                      _nominal_final_dist_attrib,
                                      _real_offset_attrib) == GS_BAD)
      return GS_BAD;
     
   pSec->info.real_init_distance_attrib     = _real_init_dist_attrib;
   pSec->info.real_final_distance_attrib    = _real_final_dist_attrib;
   pSec->info.nominal_init_distance_attrib  = _nominal_init_dist_attrib;
   pSec->info.nominal_final_distance_attrib = _nominal_final_dist_attrib;
   pSec->info.real_offset_attrib            = _real_offset_attrib;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_mod_sec_name <internal> */
/*+
  Questa funzione modifica il valore di name di una tabella
  secondaria.
  Parametri:
  int prj;             codice progetto             
  int cls;             codice classe
  int sub;             codice sottoclasse
  int sec;             codice secondaria
  TCHAR *name;          nome della tabella secondaria         
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_mod_sec_name(int prj, int cls, int sub, int sec, TCHAR *name)
{
   C_CLASS     *pCls;
   C_SECONDARY *pSec;
   long        SessionId;

   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   // verifico se la classe madre fa parte di una sessione di lavoro
   if (pCls->is_inarea(&SessionId) == GS_BAD || SessionId > 0)
      { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
   
   if ((pSec = (C_SECONDARY *) pCls->find_sec(sec)) == NULL) return GS_BAD;
   
   // modifico info secondaria                  
   if (pSec->mod_name(name) == GS_BAD) return GS_BAD;
     
   wcscpy(pSec->name, name);
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_mod_sec_descr                      <internal> */
/*+
  Questa funzione modifica il valore di descrizione di una tabella
  secondaria.
  Parametri:
  int prj;             codice progetto             
  int cls;             codice classe
  int sub;             codice sottoclasse
  int sec;             codice secondaria
  TCHAR *NewDescr;     Descrizione della tabella secondaria         
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_mod_sec_descr(int prj, int cls, int sub, int sec, TCHAR *NewDescr)
{
   C_CLASS     *pCls;
   C_SECONDARY *pSec;
   long        SessionId;

   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   // verifico se la classe madre fa parte di una sessione di lavoro
   if (pCls->is_inarea(&SessionId) == GS_BAD || SessionId > 0)
      { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }
   
   if ((pSec = (C_SECONDARY *) pCls->find_sec(sec)) == NULL) return GS_BAD;
   
   // modifico info secondaria                  
   if (pSec->mod_Descr(NewDescr) == GS_BAD) return GS_BAD;
     
   pSec->Descr = NewDescr;
   
   return GS_GOOD;
}


/*********************************************************/
/*  INIZIO   FUNZIONI DI C_INFO_SEC                      */
/*********************************************************/


// Costruttore
C_INFO_SEC::C_INFO_SEC() : C_INFO()
{ 
   vis_attrib_desc_order_type = false; 
}


/*********************************************************/
/*.doc C_INFO_SEC::from_rb <external> */
/*+
  Questa funzione inizializza i valori della C_INFO_SEC leggendoli
  da una lista di resbuf.
  Parametri:
  resbuf *rb;      Lista di resbuf
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_INFO_SEC::from_rb(resbuf *rb)
{
   C_STRING       Catalog, Schema, Table;
   presbuf        pVal;

   // Lista in input: (("UDL_FILE" <Connection>)
   //                  ("UDL_PROP" <Properties>)
   //                  ("TABLE_REF" <TableRef>)
   //                  [("KEY_ATTRIB" <value>)]
   //                  [("LAST" <value>)]
   //                  [("ORIGINAL_LAST" <value>)]
   //                  [("KEY_PRI" <value>)]
   //                  [("VIS_ATTRIB" <value>)]
   //                  [("VIS_ATTRIB_DESC_ORDER_TYPE" <value>)]
   //                  [("REAL_INIT_DISTANCE_ATTRIB" <valore>)]
   //                  [("REAL_FINAL_DISTANCE_ATTRIB" <valore>)]
   //                  [("NOMINAL_INIT_DISTANCE_ATTRIB" <valore>)]
   //                  [("NOMINAL_FINAL_DISTANCE_ATTRIB" <value>)]
   //                  [("REAL_OFFSET_ATTRIB" <value>)])
   // dove
   // <Connection> = <file UDL> | <stringa di connessione>
   // <Properties> = stringa delle proprietà | ((<prop1><value>)(<prop1><value>)...)
   // <TableRef>   = riferimento completo tabella | (<cat><schema><tabella>)

   // Questa funzione legge: ConnStrUDLFile, UDLProperties, OldTableRef, 
   //                        key_attrib, last, original_last
   if (C_INFO::from_rb(rb) == GS_BAD) return GS_BAD;
   
   // key_pri (opzionale)
   if ((pVal = gsc_CdrAssoc(_T("KEY_PRI"), rb, FALSE)))
      if (pVal->restype == RTSTR)
      {
         key_pri = pVal->resval.rstring;
         key_pri.alltrim();
      }
      else if (pVal->restype == RTNIL || pVal->restype == RTNONE) key_pri.clear();
      else { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   // vis_attrib (opzionale)
   if ((pVal = gsc_CdrAssoc(_T("VIS_ATTRIB"), rb, FALSE)))
      if (pVal->restype == RTSTR)
      {
         vis_attrib = pVal->resval.rstring;
         vis_attrib.alltrim();
      }
      else if (pVal->restype == RTNIL || pVal->restype == RTNONE) vis_attrib.clear();
      else { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   // vis_attrib_desc_order_type (opzionale)
   if ((pVal = gsc_CdrAssoc(_T("VIS_ATTRIB_DESC_ORDER_TYPE"), rb, FALSE)))
      if (gsc_rb2Bool(pVal, &vis_attrib_desc_order_type) == GS_BAD)
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   ////////////////////////////////////////////////////////////////////////////
   // Dynamic segmentation
   if (from_rb_dynamic_segmentation_info(rb) == GS_BAD) return GS_BAD;

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_INFO_SEC::from_rb_dynamic_segmentation_info <external> */
/*+
  Questa funzione inizializza i valori relativi alla segmentazione dinamica 
  della C_INFO_SEC leggendoli da una lista di resbuf.
  Parametri:
  resbuf *rb;      Lista di resbuf
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_INFO_SEC::from_rb_dynamic_segmentation_info(resbuf *rb)
{
   presbuf pVal;

   // Lista in input: ([("REAL_INIT_DISTANCE_ATTRIB" <valore>)]
   //                  [("REAL_FINAL_DISTANCE_ATTRIB" <valore>)]
   //                  [("NOMINAL_INIT_DISTANCE_ATTRIB" <valore>)]
   //                  [("NOMINAL_FINAL_DISTANCE_ATTRIB" <value>)]
   //                  [("REAL_OFFSET_ATTRIB" <value>)])

   if ((pVal = gsc_CdrAssoc(_T("REAL_INIT_DISTANCE_ATTRIB"), rb, FALSE)) != NULL)
      if (pVal->restype == RTSTR) real_init_distance_attrib = gsc_alltrim(pVal->resval.rstring);
      else real_init_distance_attrib.clear();
   if ((pVal = gsc_CdrAssoc(_T("REAL_FINAL_DISTANCE_ATTRIB"), rb, FALSE)) != NULL)
      if (pVal->restype == RTSTR) real_final_distance_attrib = gsc_alltrim(pVal->resval.rstring);
      else real_final_distance_attrib.clear();
   if ((pVal = gsc_CdrAssoc(_T("NOMINAL_INIT_DISTANCE_ATTRIB"), rb, FALSE)) != NULL)
      if (pVal->restype == RTSTR) nominal_init_distance_attrib = gsc_alltrim(pVal->resval.rstring);
      else nominal_init_distance_attrib.clear();
   if ((pVal = gsc_CdrAssoc(_T("NOMINAL_FINAL_DISTANCE_ATTRIB"), rb, FALSE)) != NULL)
      if (pVal->restype == RTSTR) nominal_final_distance_attrib = gsc_alltrim(pVal->resval.rstring);
      else nominal_final_distance_attrib.clear();
   if ((pVal = gsc_CdrAssoc(_T("REAL_OFFSET_ATTRIB"), rb, FALSE)) != NULL)
      if (pVal->restype == RTSTR) real_offset_attrib = gsc_alltrim(pVal->resval.rstring);
      else real_offset_attrib.clear();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_INFO_SEC::to_rb <external> */
/*+
  Questa funzione legge i valori di una C_INFO_SEC convertendoli
  in una lista di resbuf.
  Lista in uscita: (("UDL_FILE" <Connection>)
                    ("UDL_PROP" <Properties>)
                    ("TABLE_REF" <TableRef>)
                    [("KEY_ATTRIB" <value>)]
                    [("LAST" <value>)]
                    [("ORIGINAL_LAST" <value>)]
                    [("KEY_PRI" <value>)]
                    [("VIS_ATTRIB" <value>)]
                    [("VIS_ATTRIB_DESC_ORDER_TYPE" <value>)]
                    [("REAL_INIT_DISTANCE_ATTRIB" <valore>)]
                    [("REAL_FINAL_DISTANCE_ATTRIB" <valore>)]
                    [("NOMINAL_INIT_DISTANCE_ATTRIB" <valore>)]
                    [("NOMINAL_FINAL_DISTANCE_ATTRIB" <value>)]
                    [("REAL_OFFSET_ATTRIB" <value>)])
  Parametri:
  bool ConvertDrive2nethost; Se = TRUE le path vengono convertite
                             con alias di rete (default = false)
  bool ToDB;                 Se = true vengono considerate anche le
                             informazioni per la scrittura in DB (default = false)
        
  Restituisce la lista di resbuf in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
presbuf C_INFO_SEC::to_rb(bool ConvertDrive2nethost, bool ToDB)
{
   C_RB_LIST List;

   // Questa funzione scrive: ConnStrUDLFile, UDLProperties, OldTableRef, 
   //                         key_attrib, last, original_last
   if ((List << C_INFO::to_rb(ConvertDrive2nethost, ToDB)) == NULL) return GS_BAD;

   // devo eliminare LINKED_TABLE, SQL_CON_ON_TABLE che non sono usati per le secondarie
   List.link_head(acutBuildList(RTLB, 0)); // mi serve per far funzionare il metodo Assoc
   if (List.Assoc(_T("LINKED_TABLE")))
      { List.remove_at(); List.remove_at(); List.remove_at(); List.remove_at(); }
   if (List.Assoc(_T("SQL_COND_ON_TABLE")))
      { List.remove_at(); List.remove_at(); List.remove_at(); List.remove_at(); }
   List.remove_head();

   if ((List += acutBuildList(RTLB, RTSTR, _T("KEY_PRI"), 0)) == NULL) return NULL;
   if ((List += gsc_str2rb(key_pri)) == NULL) return NULL;
   if ((List += acutBuildList(RTLE, RTLB, RTSTR, _T("VIS_ATTRIB"), 0)) == NULL) return NULL;
   if ((List += gsc_str2rb(vis_attrib)) == NULL) return NULL;

   if (vis_attrib_desc_order_type)
      { if ((List += acutBuildList(RTLE, RTLB, RTSTR, _T("VIS_ATTRIB_DESC_ORDER_TYPE"), RTT, RTLE, 0)) == NULL) return NULL; }
   else
      { if ((List += acutBuildList(RTLE, RTLB, RTSTR, _T("VIS_ATTRIB_DESC_ORDER_TYPE"), RTNIL, RTLE, 0)) == NULL) return NULL; }

   ////////////////////////////////////////////////////////////////////////////
   // Dynamic segmentation
   if ((List += to_rb_dynamic_segmentation_info()) == NULL) return NULL;

   List.ReleaseAllAtDistruction(GS_BAD);
   return List.get_head();
}


/*********************************************************/
/*.doc C_INFO_SEC::to_rb_dynamic_segmentation_info <external> */
/*+
  Questa funzione legge i valori relativi alla segmentazione dinamica 
  della C_INFO_SEC convertendoli in una lista di resbuf.
  Lista in uscita: ([("REAL_INIT_DISTANCE_ATTRIB" <valore>)]
                    [("REAL_FINAL_DISTANCE_ATTRIB" <valore>)]
                    [("NOMINAL_INIT_DISTANCE_ATTRIB" <valore>)]
                    [("NOMINAL_FINAL_DISTANCE_ATTRIB" <value>)]
                    [("REAL_OFFSET_ATTRIB" <value>)])
        
  Restituisce la lista di resbuf in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
presbuf C_INFO_SEC::to_rb_dynamic_segmentation_info(void)
{
   C_RB_LIST List;

   if ((List += acutBuildList(RTLB, RTSTR, _T("REAL_INIT_DISTANCE_ATTRIB"), 0)) == NULL) return NULL;
   if ((List += gsc_str2rb(real_init_distance_attrib)) == NULL) return NULL;
   if ((List += acutBuildList(RTLE, RTLB, RTSTR, _T("REAL_FINAL_DISTANCE_ATTRIB"), 0)) == NULL) return NULL;
   if ((List += gsc_str2rb(real_final_distance_attrib)) == NULL) return NULL;
   if ((List += acutBuildList(RTLE, RTLB, RTSTR, _T("NOMINAL_INIT_DISTANCE_ATTRIB"), 0)) == NULL) return NULL;
   if ((List += gsc_str2rb(nominal_init_distance_attrib)) == NULL) return NULL;
   if ((List += acutBuildList(RTLE, RTLB, RTSTR, _T("NOMINAL_FINAL_DISTANCE_ATTRIB"), 0)) == NULL) return NULL;
   if ((List += gsc_str2rb(nominal_final_distance_attrib)) == NULL) return NULL;
   if ((List += acutBuildList(RTLE, RTLB, RTSTR, _T("REAL_OFFSET_ATTRIB"), 0)) == NULL) return NULL;
   if ((List += gsc_str2rb(real_offset_attrib)) == NULL) return NULL;
   if ((List += acutBuildList(RTLE, 0)) == NULL) return NULL;

   List.ReleaseAllAtDistruction(GS_BAD);
   return List.get_head();
} 


/*********************************************************/
/*.doc C_INFO_SEC::from_rb_db <internal> */
/*+
  Questa funzione carica i dati di una C_INFO_SEC da un resbuf
  generato dalla lettura su tabella GS_SEC.
  Parametri:
  presbuf colvalues;    lista colonna-valore della riga di GS_CLASS
  int cod_level;        livello di abilitazione per la classe
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_INFO_SEC::from_rb_db(C_RB_LIST &ColValues)
{
   presbuf pRb;
   
   // Questa funzione legge: ConnStrUDLFile, UDLProperties, OldTableRef, 
   //                        key_attrib, OldLastId
   if (C_INFO::from_rb_db(ColValues) == GS_BAD) return GS_BAD;

   // Key_pri
   if ((pRb = ColValues.CdrAssoc(_T("KEY_PRI"))) == NULL || pRb->restype != RTSTR)
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   else key_pri = gsc_alltrim(pRb->resval.rstring);
   // Vis_attrib
   if ((pRb = ColValues.CdrAssoc(_T("VIS_ATTRIB"))) == NULL)
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   if (pRb->restype == RTNIL || pRb->restype == RTNONE) vis_attrib = GS_EMPTYSTR;
   else vis_attrib = gsc_alltrim(pRb->resval.rstring);
   
   if (vis_attrib.len() > 0) // vis_attrib_desc_order_type
      if ((pRb = ColValues.CdrAssoc(_T("VIS_ATTRIB_DESC_ORDER_TYPE"))) != NULL)
         vis_attrib_desc_order_type = (pRb->restype == RTNIL) ? false : true;

   // REAL_INIT_DISTANCE_ATTRIB
   if ((pRb = ColValues.CdrAssoc(_T("REAL_INIT_DISTANCE_ATTRIB"))) != NULL)
      if (pRb->restype == RTNIL || pRb->restype == RTNONE) real_init_distance_attrib.clear();
      else real_init_distance_attrib = gsc_alltrim(pRb->resval.rstring);
   // REAL_FINAL_DISTANCE_ATTRIB
   if ((pRb = ColValues.CdrAssoc(_T("REAL_FINAL_DISTANCE_ATTRIB"))) != NULL)
      if (pRb->restype == RTNIL || pRb->restype == RTNONE) real_final_distance_attrib.clear();
      else real_final_distance_attrib = gsc_alltrim(pRb->resval.rstring);
   // NOMINAL_INIT_DISTANCE_ATTRIB
   if ((pRb = ColValues.CdrAssoc(_T("NOMINAL_INIT_DISTANCE_ATTRIB"))) != NULL)
      if (pRb->restype == RTNIL || pRb->restype == RTNONE) nominal_init_distance_attrib.clear();
      else nominal_init_distance_attrib = gsc_alltrim(pRb->resval.rstring);
   // NOMINAL_FINAL_DISTANCE_ATTRIB
   if ((pRb = ColValues.CdrAssoc(_T("NOMINAL_FINAL_DISTANCE_ATTRIB"))) != NULL)
      if (pRb->restype == RTNIL || pRb->restype == RTNONE) nominal_final_distance_attrib.clear();
      else nominal_final_distance_attrib = gsc_alltrim(pRb->resval.rstring);
   // REAL_OFFSET_ATTRIB
   if ((pRb = ColValues.CdrAssoc(_T("REAL_OFFSET_ATTRIB"))) != NULL)
      if (pRb->restype == RTNIL || pRb->restype == RTNONE) real_offset_attrib.clear();
      else real_offset_attrib = gsc_alltrim(pRb->resval.rstring);
   
   return GS_GOOD;
}  


/*********************************************************/
/*.doc C_INFO_SEC::ToFile                     <internal> */
/*+
  Questa funzione scarica i dati di una C_INFO_SEC in una sezione di
  un file.
  Parametri:
  C_STRING &filename; nome del file
  const TCHAR *sez;   nome della sezione
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_INFO_SEC::ToFile(C_STRING &filename, const TCHAR *sez)
{  
   C_PROFILE_SECTION_BTREE ProfileSections;
   bool                    Unicode = false;

   if (gsc_path_exist(filename) == GS_GOOD)
      if (gsc_read_profile(filename, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   if (ToFile(ProfileSections, sez) == GS_BAD) return GS_BAD;

   return gsc_write_profile(filename, ProfileSections, Unicode);
}
int C_INFO_SEC::ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{  
   TCHAR    buf[INFO_PROFILE_LEN];
   C_STRING Buffer;
   C_BPROFILE_SECTION *ProfileSection;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez)))
   {
      if (ProfileSections.add(sez) == GS_BAD) return GS_BAD;
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();
   }

   ProfileSection->set_entry(_T("INFO_TABLEREF"), 
                             (OldTableRef.len() == 0) ? GS_EMPTYSTR : OldTableRef.get_name());
   ProfileSection->set_entry(_T("INFO_CONNSTRUDLFILE"), 
                             (ConnStrUDLFile.len() == 0) ? GS_EMPTYSTR : ConnStrUDLFile.get_name());

   Buffer.paste(gsc_PropListToConnStr(UDLProperties));
   if (Buffer.len() > 0)
   {
      C_DBCONNECTION *pConn;

      // Ricavo connessione OLE-DB per tabella
      if ((pConn = getDBConnection(OLD)) == NULL) return GS_BAD;
      // Conversione path UDLProperties da dir relativo in assoluto
      if (pConn->UDLProperties_drive2nethost(Buffer) == GS_BAD) return GS_BAD;
   }
   ProfileSection->set_entry(_T("INFO_UDLPROPERTIES"), 
                             (Buffer.len() == 0) ? GS_EMPTYSTR : Buffer.get_name());

   swprintf(buf, INFO_PROFILE_LEN, _T("%s,%s,%s,%d,%ld"),
            key_attrib.get_name(), key_pri.get_name(), 
            vis_attrib.get_name(), vis_attrib_desc_order_type,
            TempLastId);
   
   ProfileSection->set_entry(_T("INFO"), buf);
   ProfileSection->set_entry(_T("INFO_REAL_INIT_DISTANCE_ATTRIB"),
                             (real_init_distance_attrib.len() == 0) ? GS_EMPTYSTR : real_init_distance_attrib.get_name());
   ProfileSection->set_entry(_T("INFO_REAL_FINAL_DISTANCE_ATTRIB"),
                             (real_final_distance_attrib.len() == 0) ? GS_EMPTYSTR : real_final_distance_attrib.get_name());
   ProfileSection->set_entry(_T("INFO_NOMINAL_INIT_DISTANCE_ATTRIB"),
                             (nominal_init_distance_attrib.len() == 0) ? GS_EMPTYSTR : nominal_init_distance_attrib.get_name());
   ProfileSection->set_entry(_T("INFO_NOMINAL_FINAL_DISTANCE_ATTRIB"),
                             (nominal_final_distance_attrib.len() == 0) ? GS_EMPTYSTR : nominal_final_distance_attrib.get_name());
   ProfileSection->set_entry(_T("INFO_REAL_OFFSET_ATTRIB"),
                             (real_offset_attrib.len() == 0) ? GS_EMPTYSTR : real_offset_attrib.get_name());

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_INFO_SEC::load <internal> */
/*+
  Questa funzione carica i dati di una C_INFO_SEC in una sezione di un file.
  Parametri:
  TCHAR *filename;    nome del file
  const TCHAR *sez;   nome della sezione
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_INFO_SEC::load(TCHAR *filename, const TCHAR *sez)
{
   C_PROFILE_SECTION_BTREE ProfileSections;

   if (gsc_read_profile(filename, ProfileSections) == GS_BAD) return GS_BAD;
   return load(ProfileSections, sez);
}
int C_INFO_SEC::load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_2STR_BTREE       *pProfileEntries;
   C_B2STR            *pProfileEntry;
   TCHAR              *str;
   int                n_campi;
   C_STRING           buffer;
   
   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez)))
      return GS_CAN;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   // riferimento a tabella (obbligatorio)
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO_TABLEREF")))) return GS_CAN;
   OldTableRef = pProfileEntry->get_name2();

   // UDL_FILE (obbligatorio)
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO_CONNSTRUDLFILE")))) return GS_CAN;
   ConnStrUDLFile = pProfileEntry->get_name2();
   ConnStrUDLFile.alltrim();
   if (gsc_path_exist(ConnStrUDLFile) == GS_GOOD)
      // se si tratta di un file e NON di una stringa di connessione
      gsc_nethost2drive(ConnStrUDLFile); // lo converto

   // UDL_PROP (obbligatorio)
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO_UDLPROPERTIES")))) return GS_CAN;
   buffer = pProfileEntry->get_name2();
   buffer.alltrim();
   if (buffer.len() > 0)
   {
      // Conversione path UDLProperties da assoluto in dir relativo
      if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(), buffer) == GS_BAD)
         return GS_BAD;
      if (gsc_PropListFromConnStr(buffer.get_name(), UDLProperties) == GS_BAD)
         return GS_BAD;
   }

   // INFO obbligatorio
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO")))) return GS_CAN;
   buffer  = pProfileEntry->get_name2();
   str     = buffer.get_name();
   n_campi = gsc_strsep(str, _T('\0')) + 1;

   // if (n_campi != 4) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (n_campi > 0)
   {
      // Key_attrib
      key_attrib = str;
      if (n_campi > 1)
      {
         while (*str != _T('\0')) str++; str++;
         // Key_pri
         key_pri = str;
         if (n_campi > 2)
         {
            while (*str != _T('\0')) str++; str++;
            // Vis_attr
            vis_attrib = str;
            if (n_campi > 3)
            {
               while (*str != _T('\0')) str++; str++;
               // vis_attrib_desc_order_type
               vis_attrib_desc_order_type = (_wtoi(str) == 0) ? false : true;
               if (n_campi > 4)
               {
                  while (*str != _T('\0')) str++; str++;
                  // Last
                  TempLastId = _wtol(str);
               }
            }
         }
      }
   }

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO_REAL_INIT_DISTANCE_ATTRIB"))))
   real_init_distance_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO_REAL_FINAL_DISTANCE_ATTRIB"))))
   real_final_distance_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO_NOMINAL_INIT_DISTANCE_ATTRIB"))))
   nominal_init_distance_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO_NOMINAL_FINAL_DISTANCE_ATTRIB"))))
   nominal_final_distance_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("INFO_REAL_OFFSET_ATTRIB"))))
   real_offset_attrib = pProfileEntry->get_name2();

   return GS_GOOD;
}


int C_INFO_SEC::copy(C_INFO_SEC *out)
{
	if (!out) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   
	C_INFO::copy(out);

   out->key_pri    = key_pri;
   out->vis_attrib = vis_attrib;
   out->vis_attrib_desc_order_type = vis_attrib_desc_order_type;

   out->real_init_distance_attrib     = real_init_distance_attrib;
   out->real_final_distance_attrib    = real_final_distance_attrib;
   out->nominal_init_distance_attrib  = nominal_init_distance_attrib;
   out->nominal_final_distance_attrib = nominal_final_distance_attrib;
   out->real_offset_attrib            = real_offset_attrib;

	return GS_GOOD;
}  

 
/****************************************************************************/
/*.doc C_INFO_SEC:reportHTML                                     <external> */
/*+
  Questa funzione stampa su un file HTML i dati della C_INFO_SEC.
  Parametri:
  FILE *file;     Puntatore a file
  bool SynthMode; Opzionale. Flag di modalità di report.
                  Se = true attiva il report sintetico (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_INFO_SEC::reportHTML(FILE *file, bool SynthMode)
{  
   C_STRING Buffer;
   C_STRING TitleBorderColor("#808080"), TitleBgColor("#c0c0c0");
   C_STRING BorderColor("#00CCCC"), BgColor("#99FFFF");

   if (SynthMode) return GS_GOOD;

   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" bgcolor=\"%s\" width=\"100%%\" border=\"1\">"),
                TitleBorderColor.get_name(), TitleBgColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Collegamento a Database"
   if (fwprintf(file, _T("\n<tr><td align=\"center\"><b><font size=\"3\">%s</font></b></td></tr></table><br>"),
                gsc_msg(753)) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // intestazione tabella
   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" cellspacing=\"2\" cellpadding=\"2\" border=\"1\">"), 
                BorderColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   Buffer.paste(gsc_PropListToConnStr(UDLProperties));

   // "Riferimento tabella"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\" width=\"30%%\"><b>%s:</b></td><td width=\"70%%\">%s</td></tr>"),
                BgColor.get_name(), gsc_msg(727),
                (OldTableRef.len() == 0) ? _T("&nbsp;") : OldTableRef.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Connessione UDL"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(728),
                (ConnStrUDLFile.len() == 0) ? _T("&nbsp;") : ConnStrUDLFile.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Proprietà UDL"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(790),
                (Buffer.len() == 0) ? _T("&nbsp;") : Buffer.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Attributo chiave"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(729),
                (key_attrib.len() == 0) ? _T("&nbsp;") : key_attrib.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Ultimo codice"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%ld</td></tr>"),
                BgColor.get_name(), gsc_msg(730), OldLastId) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Attributo di relazione"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(271),
                (key_pri.len() == 0) ? _T("&nbsp;") : key_pri.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (vis_attrib.len() == 0)
      Buffer = _T("&nbsp;");
   else
   {
      Buffer = vis_attrib;
      Buffer += _T(" (");
      if (vis_attrib_desc_order_type) Buffer += gsc_msg(283); // "descrescente"
      else Buffer += gsc_msg(280); // "crescente"
      Buffer += _T(")");
   }

   // "Ordine di visualizzazione"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(275), Buffer.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (real_init_distance_attrib.len() > 0)
      // "Attributo per la distanza reale (inizio)"
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                   BgColor.get_name(), gsc_msg(313), real_init_distance_attrib.get_name()) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   if (real_final_distance_attrib.len() > 0)
      // "Attributo per la distanza reale (fine)"
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                   BgColor.get_name(), gsc_msg(325), real_final_distance_attrib.get_name()) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   if (nominal_init_distance_attrib.len() > 0)
      // "Attributo per la distanza nominale (inizio)"
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                   BgColor.get_name(), gsc_msg(331), nominal_init_distance_attrib.get_name()) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   if (nominal_final_distance_attrib.len() > 0)
      // "Attributo per la distanza nominale (fine)"
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                   BgColor.get_name(), gsc_msg(368), nominal_final_distance_attrib.get_name()) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   if (real_offset_attrib.len() > 0)
      // "Attributo per l'offset reale"
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                   BgColor.get_name(), gsc_msg(402), real_offset_attrib.get_name()) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   // fine tabella
   if (fwprintf(file, _T("\n</table><br><br>")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*  FINE   FUNZIONI DI C_INFO_SEC                        */
/*  INIZIO FUNZIONI DI C_SECONDARY                       */
/*********************************************************/


C_SECONDARY::C_SECONDARY() : C_NODE()
{
   pCls     = NULL;
   code     = 0;
   type     = GSInternalSecondaryTable;
   name[0]  = _T('\0');
   abilit   = GSInvisibleData;
   sel      = DESELECTED;
   info.key_attrib = DEFAULT_KEY_ATTR;
}

TCHAR *C_SECONDARY::get_name(void) { return name; }
bool C_SECONDARY::isValidName(TCHAR *_name)
{
   gsc_alltrim(_name);
   if (gsc_strlen(_name) == 0 || gsc_strlen(_name) >= MAX_LEN_CLASSNAME)
      { GS_ERR_COD = eGSInvSecName; return false; }
   else
      return true;
}

TCHAR *C_SECONDARY::get_Descr(void) { return Descr.get_name(); }
bool C_SECONDARY::isValidDescr(TCHAR *_Descr)
{
   gsc_alltrim(_Descr);

   if (gsc_strlen(_Descr) >= MAX_LEN_CLASSNAME)
      { GS_ERR_COD = eGSInvSecDescr; return false; }
   else
      return true;
}

int C_SECONDARY::get_type(void) { return type; }

int C_SECONDARY::get_key(void) { return code; }


/*********************************************************/
/*.doc C_SECONDARY::get_Bitmap                 <external> */
/*+
  Funzione che restituisce la bitmap della tabella secondaria.
  Parametri:
  bool LargeSize;    Se vero la dimensione della bitmap sarà di 32x16
                     altrimenti sarà di 16x16
  CBitmap &CBMP;     Oggetto bitmap (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::get_Bitmap(bool LargeSize, CBitmap &CBMP)
{
   return gsc_getSecTabBitmap(LargeSize, CBMP);
}


/*********************************************************/
/*.doc gsc_getSecTabBitmap                    <external> */
/*+
  Funzione che restituisce la bitmap della tabella secondaria.
  Parametri:
  bool LargeSize;    Se vero la dimensione della bitmap sarà di 32x16
                     altrimenti sarà di 16x16
  CBitmap &CBMP;     Oggetto bitmap (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getSecTabBitmap(bool LargeSize, CBitmap &CBMP)
{
   UINT nIDResource = (LargeSize) ? IDB_SECOND_32X16 : IDB_SECOND_16X16;
   // When resource from this ARX app is needed, just
   // instantiate a local CAcModuleResourceOverride
   CAcModuleResourceOverride myResources;

   HBITMAP hBitmap=LoadBitmap(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(nIDResource));
   HGDIOBJ obj = CBMP.Detach();
   if (obj) ::DeleteObject(obj);
   if (CBMP.Attach(hBitmap) == false) return GS_BAD;

   return GS_GOOD;
}

/*********************************************************/
/*.doc gsc_getSecLnkTableRef                  <internal> */
/*+
  Questa funzione ricava il nome della tabella dei link di
  una secondaria partendo dal nome della secondaria, e quindi
  senza dover caricare la secondaria stessa.
  Parametri:
  C_DBCONNECTION    *pConn          puntatore alla connessione OLE-DB
  C_STRING          &TableRef       nome della secondaria
  C_STRING          &LinkTableRef   nome della tabella dei link
                                    della secondaria
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getSecLnkTableRef(C_DBCONNECTION *pConn, C_STRING &TableRef, C_STRING &LinkTableRef)
{
   C_STRING Catalog, Schema, Table;

   if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;
   Table += _T("L");  
   LinkTableRef.paste(pConn->get_FullRefTable(Catalog, Schema, Table));
   if (!(LinkTableRef.get_name())) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::getOldLnkTableRef         <internal> */
/*+
  Questa funzione ottiene il riferimento completo della tabella
  dei link della secondaria.
  Parametri:
  C_STRING &LinkTableRef;
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::getOldLnkTableRef(C_STRING &OldLnkTableRef)
{
   C_DBCONNECTION   *pConn;
   C_STRING          Catalog, Schema, Table;
   C_INFO_SEC       *pInfo = ptr_info();

   if (!pInfo) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (!(pInfo->OldLnkTableRef.get_name()))
   {
      // Ricavo connessione OLE-DB per tabella
      if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;
      if (gsc_getSecLnkTableRef(pConn, pInfo->OldTableRef, pInfo->OldLnkTableRef) == GS_BAD) 
         return GS_BAD;
   }
   OldLnkTableRef = pInfo->OldLnkTableRef;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::getTempTableRef           <internal> */
/*+
  Questa funzione ottiene il riferimento completo della tabella
  temporanea della secondaria.
  Parametri:
  C_STRING &TempTableRef;
  int      Create;        Se = GS_GOOD crea la tabella TEMP (default = GS_GOOD)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::getTempTableRef(C_STRING &TempTableRef, int Create)
{
   C_DBCONNECTION   *pConn;
   C_STRING          Table;
   C_INFO_SEC       *pInfo = ptr_info();

   if (!pInfo) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // Ricavo connessione OLE-DB per tabella TEMP
   if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;

   if (!(pInfo->TempTableRef.get_name()))
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

      Table = _T("CLS");
      Table += pCls->ptr_id()->code;
      if (pCls->is_subclass() == GS_GOOD)
      {
         Table += _T("SUB");
         Table += pCls->ptr_id()->sub_code;
      }
      Table += _T("SEC");
      Table += code;

      pInfo->TempTableRef.paste(pConn->get_FullRefTable(GS_CURRENT_WRK_SESSION->get_dir(), NULL, Table.get_name()));
      if (!(pInfo->TempTableRef.get_name())) return GS_BAD;
   }
   TempTableRef = pInfo->TempTableRef;

   // Se Create = GS_GOOD devo verificare l' esistenza e nel caso non esista la creo
   if (Create == GS_GOOD)
   {
      // Se non esiste la tabella la creo
      if (pConn->ExistTable(TempTableRef) == GS_BAD)
         if (create_tab(TEMP) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::getTempLnkTableRef        <internal> */
/*+
  Questa funzione ottiene il riferimento completo della tabella
  temporanea dei link della secondaria.
  Parametri:
  C_STRING &TempTableRef;
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::getTempLnkTableRef(C_STRING &TempLnkTableRef)
{
   C_DBCONNECTION   *pConn;
   C_STRING          Table;
   C_INFO           *pInfo = ptr_info();

   if (!pInfo) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (!(pInfo->TempLnkTableRef.get_name()))
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

      Table = _T("CLS");
      Table += pCls->ptr_id()->code;
      if (pCls->is_subclass() == GS_GOOD)
      {
         Table += _T("SUB");
         Table += pCls->ptr_id()->sub_code;
      }
      Table += _T("SEC");
      Table += code;
      Table += _T("L");        // "L" per Link

      // ricavo connessione OLE-DB per tabella TEMP
      if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;
      pInfo->TempLnkTableRef.paste(pConn->get_FullRefTable(GS_CURRENT_WRK_SESSION->get_dir(), NULL, Table.get_name()));
      if (!(pInfo->TempLnkTableRef.get_name())) return GS_BAD;
   }
   TempLnkTableRef = pInfo->TempLnkTableRef;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::getSupportTableRef        <internal> */
/*+
  Questa funzione ottiene il riferimento completo della tabella
  di appoggio temporanea della secondaria.
  Parametri:
  C_STRING &TempTableRef;
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::getSupportTableRef(C_STRING &SupportTableRef)
{
   C_DBCONNECTION   *pConn;
   C_STRING          Table;
   C_INFO_SEC       *pInfo = ptr_info();

   if (!pInfo) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (!(pInfo->SupportTableRef.get_name()))
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

      Table = _T("CLS");
      Table += pCls->ptr_id()->code;
      if (pCls->is_subclass() == GS_GOOD)
      {
         Table += _T("SUB");
         Table += pCls->ptr_id()->sub_code;
      }
      Table += _T("SEC");
      Table += code;
      Table += _T("X");        // "X" sta per tabella appoggio

      // ricavo connessione OLE-DB per tabella TEMP
      if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;
      pInfo->SupportTableRef.paste(pConn->get_FullRefTable(GS_CURRENT_WRK_SESSION->get_dir(), NULL, Table.get_name()));
      if (!(pInfo->SupportTableRef.get_name())) return GS_BAD;
   }
   SupportTableRef = pInfo->SupportTableRef;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::getSupportLnkTableRef     <internal> */
/*+
  Questa funzione ottiene il riferimento completo della tabella
  temporanea dei link della secondaria.
  Parametri:
  C_STRING &TempTableRef;
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::getSupportLnkTableRef(C_STRING &SupportLnkTableRef)
{
   C_DBCONNECTION   *pConn;
   C_STRING          Table;
   C_INFO_SEC       *pInfo = ptr_info();

   if (!pInfo) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (!(pInfo->SupportLnkTableRef.get_name()))
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

      Table = _T("CLS");
      Table += pCls->ptr_id()->code;
      if (pCls->is_subclass() == GS_GOOD)
      {
         Table += _T("SUB");
         Table += pCls->ptr_id()->sub_code;
      }
      Table += _T("SEC");
      Table += code;
      Table += _T("X");        // "X" sta per tabella appoggio
      Table += _T("L");        // "L" per Link

      // ricavo connessione OLE-DB per tabella TEMP
      if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;
      pInfo->SupportLnkTableRef.paste(pConn->get_FullRefTable(GS_CURRENT_WRK_SESSION->get_dir(), NULL, Table.get_name()));
      if (!(pInfo->SupportLnkTableRef.get_name())) return GS_BAD;
   }
   SupportLnkTableRef = pInfo->SupportLnkTableRef;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::ToFile                    <internal> */
/*+
  Questa funzione scarica i dati di una C_SECONDARY di una classe nota,
  di una sotto-classe nota in un file.
  Parametri:
  C_STRING *filename;    nome del file
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::ToFile(C_STRING &filename)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   bool                    Unicode = false;

   if (gsc_path_exist(filename) == GS_GOOD)
      if (gsc_read_profile(filename, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   if (ToFile(ProfileSections) == GS_BAD) return GS_BAD;

   return gsc_write_profile(filename, ProfileSections, Unicode);
}
int C_SECONDARY::ToFile(C_PROFILE_SECTION_BTREE &ProfileSections)
{
   TCHAR sez[SEZ_PROFILE_LEN];

   swprintf(sez, SEZ_PROFILE_LEN, _T("%d.%d.%d"), pCls->ptr_id()->code, pCls->ptr_id()->sub_code, code);
   if (ToFile_id(ProfileSections, sez) == GS_BAD )    return GS_BAD;
   if (info.ToFile(ProfileSections, sez) == GS_BAD )  return GS_BAD;
   wcscat(sez, ATTRIB_PROFILE);
   if (attrib_list.ToFile(ProfileSections, sez) == GS_BAD ) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::load <internal> */
/*+
  Questa funzione carica i dati di una C_SECONDARY di una classe nota,
  di una sotto-classe nota da un file.
  Parametri:
  TCHAR *filename;   nome del file
  int cls;           codice classe madre
  int sub;           codice sotto-classe madre
  int sec;           codice secondaria
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::load(TCHAR *filename, C_CLASS *pClass, int sec)
{
   C_PROFILE_SECTION_BTREE ProfileSections;

   if (gsc_read_profile(filename, ProfileSections) == GS_BAD) return GS_BAD;
   return load(ProfileSections, pClass, sec);
}
int C_SECONDARY::load(C_PROFILE_SECTION_BTREE &ProfileSections, C_CLASS *pClass, int sec)
{
   TCHAR sez[SEZ_PROFILE_LEN];
   int   Result;
   
   pCls = pClass;
   swprintf(sez, SEZ_PROFILE_LEN, _T("%d.%d.%d"), pCls->ptr_id()->code, pCls->ptr_id()->sub_code, sec);
   if ((Result = load_id(ProfileSections, sez)) != GS_GOOD) return Result;
   if (info.load(ProfileSections, sez) != GS_GOOD) return GS_BAD;
   wcscat(sez, ATTRIB_PROFILE);
   return attrib_list.load(ProfileSections, sez, info.getDBConnection(OLD));
}


/*********************************************************/
/*.doc C_SECONDARY::ToFile_id <internal> */
/*+
  Questa funzione scarica i dati di identificazione di una C_SECONDARY
  in un file (codice secondaria, codice classe madre, codice sotto-classe
  madre, tipo secondaria, nome secondaria).
  Parametri:
  C_STRING &filename; nome del file
  const TCHAR *sez;   nome della sezione
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::ToFile_id(C_STRING &filename, const TCHAR *sez)
{  
   C_PROFILE_SECTION_BTREE ProfileSections;
   bool                    Unicode = false;

   if (gsc_path_exist(filename) == GS_GOOD)
      if (gsc_read_profile(filename, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   if (ToFile_id(ProfileSections, sez) == GS_BAD) return GS_BAD;

   return gsc_write_profile(filename, ProfileSections, Unicode);
}
int C_SECONDARY::ToFile_id(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{  
   C_BPROFILE_SECTION *ProfileSection;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez)))
   {
      if (ProfileSections.add(sez) == GS_BAD) return GS_BAD;
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();
   }

   ProfileSection->set_entry(_T("ID.CODE"),       code);
   ProfileSection->set_entry(_T("ID.CLS_CODE"),   pCls->ptr_id()->code);
   ProfileSection->set_entry(_T("ID.SUB_CODE"),   pCls->ptr_id()->sub_code);
   ProfileSection->set_entry(_T("ID.TYPE"),       type);
   ProfileSection->set_entry(_T("ID.NAME"),       name);
   ProfileSection->set_entry(_T("ID.DESCRIPTION"),Descr.get_name() ? Descr.get_name(): GS_EMPTYSTR);
   ProfileSection->set_entry(_T("ID.ABILIT"), abilit);
   ProfileSection->set_entry(_T("ID.SEL"), sel);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::load_id <internal> */
/*+
  Questa funzione carica i dati di identificazione di una C_SECONDARY
  da un file (codice secondaria, codice classe madre, codice sotto-classe
  madre, tipo secondaria, nome secondaria).
  Parametri:
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  const TCHAR *sez;  nome della sezione
    
  Restituisce GS_GOOD in caso di successo, GS_CAN se non esiste la sezione 
  altrimenti restituisce GS_BAD in caso di errore.
-*/  
/*********************************************************/
int C_SECONDARY::load_id(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_2STR_BTREE       *pProfileEntries;
   C_B2STR            *pProfileEntry;
   
   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez)))
      return GS_CAN;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ID.CODE"))))
      code = _wtoi(pProfileEntry->get_name2());
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ID.TYPE"))))
      type = (_wtoi(pProfileEntry->get_name2()) == GSInternalSecondaryTable) ? GSInternalSecondaryTable : GSExternalSecondaryTable;
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ID.NAME"))))
      gsc_strcpy(name, pProfileEntry->get_name2(), MAX_LEN_CLASSNAME);
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ID.DESCRIPTION"))))
      Descr = pProfileEntry->get_name2();
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ID.ABILIT"))))
      abilit = (GSDataPermissionTypeEnum) _wtoi(pProfileEntry->get_name2());
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("ID.SEL"))))
      sel = _wtoi(pProfileEntry->get_name2());

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::from_db <internal> */
/*+
  Questa funzione carica la definizione di una tabella secondaria dai database.
  Parametri:
  C_RB_LIST &ColValues; lista colonna-valore della riga di GS_SEC
  int cod_level;        livello di abilitazione per la tabella secondaria
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::from_db(C_RB_LIST &colvalues, GSDataPermissionTypeEnum cod_level)
{  
   if (from_rb_db(colvalues, cod_level) == GS_BAD) return GS_BAD;
   
   // ATTRIBUTE LIST
   if (restore_attriblist() == GS_BAD)
      // e' un problema solo se secondaria di GEOSIM
      if (type == GSInternalSecondaryTable) return GS_BAD;

   return GS_GOOD;
}   


/*********************************************************/
/*.doc C_SECONDARY::from_rb_db <internal> */
/*+
  Questa funzione carica i dati di una tabella secondaria da un resbuf
  generato dalla lettura su tabella GS_SEC.
  Parametri:
  presbuf colvalues;    lista colonna-valore della riga di GS_SEC
  int cod_level;        livello di abilitazione per la tabella secondaria
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::from_rb_db(C_RB_LIST &colvalues, GSDataPermissionTypeEnum cod_level)
{
   presbuf p;
   int     dummy;

   if ((p = colvalues.CdrAssoc(_T("GS_ID"))) == NULL ||
       gsc_rb2Int(p, &code) == GS_BAD) return GS_BAD; 

   if ((p = colvalues.CdrAssoc(_T("TYPE"))) == NULL)
      return GS_BAD;
   if (gsc_rb2Int(p, &dummy) == GS_BAD) return GS_BAD; 
   type = (dummy == GSInternalSecondaryTable) ? GSInternalSecondaryTable : GSExternalSecondaryTable;

   if ((p = colvalues.CdrAssoc(_T("NAME"))) == NULL || p->restype != RTSTR)
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   wcscpy(name, gsc_alltrim(p->resval.rstring));

   if ((p = colvalues.CdrAssoc(_T("DESCRIPTION"))) == NULL)
      { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   if (p->restype == RTSTR) { Descr = p->resval.rstring; Descr.alltrim(); }
   else Descr.clear();
  
   abilit = cod_level;

   if (info.from_rb_db(colvalues) == NULL) return GS_BAD;

   if (type == GSInternalSecondaryTable)
      // Leggo il massimo codice direttamente dalla tabella
      if (ptr_info()->RefreshOldLastId() == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::to_rb <external> */
/*+
  Questa funzione legge i valori di una tabella secondaria convertendoli
  in una lista di resbuf.
  (<id><info><attrib_list>)
        
  Restituisce la lista di resbuf in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
presbuf C_SECONDARY::to_rb(void)
{
   presbuf lista, p;
   
   if ((p = lista =  acutBuildList(RTLB,
                                      RTLB,
                                         RTSTR, _T("CODE"),
                                         RTSHORT, code,
                                      RTLE,
                                      RTLB,
                                         RTSTR, _T("TYPE"),
                                         RTSHORT, type,
                                      RTLE,
                                      RTLB,
                                         RTSTR, _T("NAME"),
                                         RTSTR, name,
                                      RTLE,
                                      RTLB,
                                         RTSTR, _T("DESCRIPTION"),
                                         RTSTR, (Descr.get_name()) ? Descr.get_name() : GS_EMPTYSTR,
                                      RTLE,
                                      RTLB,
                                         RTSTR, _T("ABILIT"),
                                         RTSHORT, abilit,
                                      RTLE,
                                   RTLE,
                                   RTLB, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   while (p->rbnext != NULL) p = p->rbnext;

   if ((p->rbnext = info.to_rb((type) == 0 ? false : true)) == NULL) return GS_BAD;
   while (p->rbnext != NULL) p = p->rbnext;

   // Attrib_list
   if ((p->rbnext = acutBuildList(RTLE, RTLB, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   p = p->rbnext->rbnext;
   p->rbnext = attrib_list.to_rb();
   while (p->rbnext != NULL) p = p->rbnext;
   if ((p->rbnext = acutBuildList(RTLE, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   return lista;
} 

/*********************************************************/
/*.doc C_SECONDARY::from_rb <external> */
/*+
  Questa funzione inizializza i valori di una tabella secondaria leggendoli
  da una lista di resbuf.
  Parametri:
  resbuf *rb;      Lista di resbuf
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::from_rb(resbuf *rb)
{
   presbuf p;
   int     dummyInt;

   if (rb == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (rb->restype == RTNIL) return GS_GOOD;

   rb = gsc_nth(0, rb); // ID
   if ((p = gsc_CdrAssoc(_T("CODE"), rb, FALSE)))
      if (gsc_rb2Int(p, &code) == GS_BAD) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   if ((p = gsc_CdrAssoc(_T("TYPE"), rb, FALSE)))
      if (gsc_rb2Int(p, &dummyInt) == GS_BAD) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      else
         type = (dummyInt == GSInternalSecondaryTable) ? GSInternalSecondaryTable : GSExternalSecondaryTable;
   if ((p = gsc_CdrAssoc(_T("NAME"), rb, FALSE)))
      if (p->restype == RTSTR)
      {
         if (!isValidName(p->resval.rstring)) return GS_BAD;
         gsc_strcpy(name, p->resval.rstring, MAX_LEN_CLASSNAME);
      }
      else
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   if ((p = gsc_CdrAssoc(_T("DESCRIPTION"), rb, FALSE)))
      if (p->restype == RTSTR)
      {
         if (!isValidDescr(p->resval.rstring)) return GS_BAD;
         Descr = p->resval.rstring;
      }
      else
         Descr.clear();

   if ((p = gsc_CdrAssoc(_T("ABILIT"), rb, FALSE)))
      if (gsc_rb2Int(p, (int *) &abilit) == GS_BAD) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   rb = gsc_nth(1, rb); // INFO
   if ((rb = rb->rbnext) != NULL)
      if (info.from_rb(rb) == NULL) return GS_BAD;

   rb = gsc_nth(2, rb); // ATTRIB_LIST

   if (type == GSInternalSecondaryTable)
      if (attrib_list.from_rb(rb) == GS_BAD) return GS_BAD;

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SECONDARY::baseval_to_rb_db <internal> */
/*+
  Questa funzione scarica i dati base di una C_SECONDARY in un resbuf per 
  scrivere nella tabella GS_SEC.
  Parametri:
  presbuf *colvalues;    lista colonna-valore della riga di GS_SEC.
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::baseval_to_rb_db(C_RB_LIST &ColValues)
{
   if ((ColValues += acutBuildList(RTLB,
                                      RTSTR, _T("GS_ID"), RTSHORT, code,
                                   RTLE, 
                                   RTLB,
                                      RTSTR, _T("CLASS_ID"), RTSHORT, pCls->ptr_id()->code,
                                   RTLE,
                                   RTLB,
                                      RTSTR, _T("SUB_CL_ID"), RTSHORT, pCls->ptr_id()->sub_code,
                                   RTLE,
                                   RTLB,
                                      RTSTR, _T("TYPE"), RTSHORT, type,
                                   RTLE,
                                   RTLB,
                                      RTSTR, _T("NAME"), RTSTR, name,
                                   RTLE,
                                   RTLB,
                                      RTSTR, _T("DESCRIPTION"), RTSTR, (Descr.get_name()) ? Descr.get_name() : GS_EMPTYSTR,
                                   RTLE,
                                   0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   
   return GS_GOOD;
}  


/*********************************************************/
/*.doc gsc_find_sec                           <external> */
/*+
  Questa funzione cerca una tabella secondaria in memoria e se non la
  trova cerca su disco.
  Parametri:
  int prj;       codice progetto
  int cls;       codice classe
  int sub;       codice sottoclasse
  int sec;       codice tabella secondaria

  Restituisce il puntatore alla tabella secondaria in caso di successo
  altrimenti restituisce NULL.
-*/  
/*********************************************************/
C_SECONDARY *gsc_find_sec(int prj, int cls, int sub, int sec)
{
   C_CLASS     *pCls;

   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return NULL;

   return (C_SECONDARY *) pCls->find_sec(sec);
}


/*********************************************************/
/*.doc restore_attriblist <internal> */
/*+
  Questa funzione carica la lista degli attributi della tabella secondaria
  dai database.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B.: Prima di utilizzare questa funzione devono essere caricati i dati
        base della tabella secondaria.
-*/  
/*********************************************************/
int C_SECONDARY::restore_attriblist(void)
{  
   C_PROJECT      *pFatherPrj = (C_PROJECT *) pCls->ptr_id()->pPrj;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   C_STRING       statement, TableRef;
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();

   if (type == GSExternalSecondaryTable) return read_sec_extattriblist();

   if (gsc_stru_valdef2attriblist(this, (type == GSInternalSecondaryTable) ? GS_GOOD : GS_BAD) == GS_BAD)
      return GS_BAD;

   // ottengo la connessione OLE-DB il riferimento alla tabella GS_ATTR
   if (pFatherPrj->getAttribsTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += pCls->ptr_id()->code;
   statement += _T(" AND SUB_CL_ID=");
   statement += pCls->ptr_id()->sub_code;
   statement += _T(" AND SEC_ID=");
   statement += code;
   
   // Leggo le righe della tabella senza bloccarla
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;

   if (gsc_NoGraphCharact2attriblist(pRs, p_attrib_list) == GS_BAD)
      { gsc_DBCloseRs(pRs); p_attrib_list->remove_all(); return GS_BAD; }

   if (gsc_DBCloseRs(pRs) == GS_BAD)
      { p_attrib_list->remove_all(); return GS_BAD; }

   if (p_attrib_list->init_ADOType(ptr_info()->getDBConnection(OLD)) == GS_BAD)
      { p_attrib_list->remove_all(); return GS_BAD; }

   return GS_GOOD;
}

  
/*********************************************************/
/*.doc C_SECONDARY::create_tab <internal> */
/*+
   Questa funzione crea una tabella secondaria.
   Parametri:
      int Type    Flag che specifica se OLD - TEMP
  
  Restituisce GS_GOOD in caso di successo altrimenti 
  restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::create_tab(int Type)
{
   C_ATTRIB_LIST    *p_attrib_list;
   C_STRING          TempTable;
   C_DBCONNECTION   *pTempConn, *pOldConn;

   if (ptr_info() == NULL || (p_attrib_list = ptr_attrib_list()) == NULL) 
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (Type == OLD)
   {
      C_STRING PrimaryKey;

      // Verifico abilitazione solo per creazione tabella OLD
      if (gsc_check_op(opCreateClass) == GS_BAD) return GS_BAD;

      if (ptr_info()->get_PrimaryKeyName(PrimaryKey) == GS_BAD) return GS_BAD;

      // Creo la tabella OLD della secondaria
      if (gsc_create_tab(ptr_info()->OldTableRef, ptr_info()->getDBConnection(OLD),
                         p_attrib_list, PrimaryKey, ptr_info()->key_attrib, INDEX_UNIQUE) == GS_BAD)
         return GS_BAD; 
   }
   else
   {
      // Ricavo le due connessioni 
      if ((pTempConn = ptr_info()->getDBConnection(TEMP)) == GS_BAD) return GS_BAD;
      if ((pOldConn = ptr_info()->getDBConnection(OLD)) == GS_BAD) return GS_BAD;
      
      // Se è TEMP sufficiente ricopiare l' old nel temp
      // Ricavo il riferimento completo alla tabella temporanea della secondaria
      if (getTempTableRef(TempTable, GS_BAD) == GS_BAD) return GS_BAD;
      // Ricopio la struttura dell' old nel temp
      if (pOldConn->CopyTable(ptr_info()->OldTableRef.get_name(), *pTempConn, 
                              TempTable.get_name(), GSStructureOnlyCopyType) == GS_BAD) return GS_BAD; 
   }
   
   return GS_GOOD;
}   


/*********************************************************/
/*.doc C_SECONDARY::create_tab_link <external> */
/*+
  Questa funzione crea la tabella dei link per le tabelle secondarie.
  Parametri:
   int Status;    Flag che determina se inserire il campo Status
   int Type;      parametro che determina OLD o TEMP (default = OLD)
   int isSupport; (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.:Da usare dopo che e' gia' stata creata la tabella delle entita'.
-*/  
/*********************************************************/
int C_SECONDARY::create_tab_link(int Status, int Type, int isSupport)
{   
   C_DBCONNECTION *pConn;
   C_STRING        TableRef, ProviderDescrUInt, ProviderDescrSmallInt, DestProviderDescr;
   C_STRING        Catalog, Schema, Table, TempRif, Index1, Index2;
   C_ATTRIB       *p_attrib = NULL, *p_attrib_cls = NULL;
   long            lenUInt, lenSmallInt, DestSize;
   int             decUInt, decSmallInt, DestPrec;

   if (Type == OLD)  
   {
      // Ricavo la connessione OLE-DB
      if ((pConn = ptr_info()->getDBConnection(OLD)) == GS_BAD) return GS_BAD;
      // Ricavo la tabella link temporanea 
      if (getOldLnkTableRef(TableRef) == GS_BAD) return GS_BAD;
   }
   else
   {
      // Ricavo la connessione OLE-DB
      if ((pConn = ptr_info()->getDBConnection(TEMP)) == GS_BAD) return GS_BAD;
      if (isSupport == GS_BAD)
      {
         // Ricavo la tabella link temporanea 
         if (getTempLnkTableRef(TableRef) == GS_BAD) return GS_BAD;
      }
      else
      {
         // Ricavo la tabella di support dei link temporanei 
         if (getSupportLnkTableRef(TableRef) == GS_BAD) return GS_BAD;
      }
   }
   
   // Se la tabella non esiste la creo
   if (pConn->ExistTable(TableRef) == GS_BAD)
   {
      C_RB_LIST stru;
      
      // Attributo della secondaria
      if ((p_attrib = (C_ATTRIB *) ptr_attrib_list()->search_name(ptr_info()->key_attrib.get_name(), FALSE)) == NULL)
         return GS_BAD; 

      // Attributo della classe 
      if ((p_attrib_cls = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(ptr_info()->key_pri.get_name(), FALSE)) == NULL)
         return GS_BAD; 
      if (pCls->ptr_info()->getDBConnection(OLD)->Descr2ProviderDescr(
                          p_attrib_cls->type, p_attrib_cls->len, p_attrib_cls->dec,
                          *pConn, DestProviderDescr, &DestSize, &DestPrec) == GS_BAD)
         return GS_BAD;

      // Converto campi da codice ADO in Provider dipendente (numero intero)
      // Conversione tipologia per campi CLASS_ID, SUB_CL_ID
      if (pConn->Type2ProviderType(KEY_TYPE_ENUM,           // DataType per campo sottoclasse
                                   FALSE,                   // IsLong
                                   FALSE,                   // IsFixedPrecScale
                                   RTT,                     // IsFixedLength
                                   TRUE,                    // IsWrite
                                   LEN_KEY_ATTR,            // Size
                                   0,                       // Prec
                                   ProviderDescrSmallInt,   // ProviderDescr
                                   &lenSmallInt, &decSmallInt) == GS_BAD) return GS_BAD;

      // Conversione tipologia per campi STATUS
      if (pConn->Type2ProviderType(adUnsignedTinyInt,       // DataType per campo sottoclasse
                                   FALSE,                   // IsLong
                                   FALSE,                   // IsFixedPrecScale
                                   RTT,                     // IsFixedLength
                                   TRUE,                    // IsWrite
                                   3,                       // Size
                                   0,                       // Prec
                                   ProviderDescrUInt,       // ProviderDescr
                                   &lenUInt, &decUInt) == GS_BAD) return GS_BAD;

      // Costruisco la lista della struttura tabella
      if ((stru << acutBuildList(RTLB,
                                 RTLB,
                                 RTSTR, _T("CLASS_ID"),                    // CLASS_ID
                                 RTSTR, ProviderDescrSmallInt.get_name(),  // <tipo>
                                 RTSHORT, lenSmallInt,                     // <len>
                                 RTSHORT, decSmallInt,                     // <dec>
                                 RTLE,

                                 RTLB,
                                 RTSTR, _T("SUB_CL_ID"),                // SUB_CL_ID
                                 RTSTR, ProviderDescrUInt.get_name(),   // <tipo>
                                 RTSHORT, lenUInt,                      // <len>
                                 RTSHORT, decUInt,                      // <dec>
                                 RTLE, 

                                 RTLB,
                                 RTSTR, _T("KEY_PRI"),                  // KEY_PRI
                                 RTSTR, DestProviderDescr.get_name(),   // <tipo>
                                 RTSHORT, DestSize,                     // <len>
                                 RTSHORT, DestPrec,                     // <dec>
                                 RTLE,  
   
                                 RTLB,
                                 RTSTR, _T("KEY_ATTRIB"),               // KEY_ATTRIB
                                 RTSTR, DestProviderDescr.get_name(),   // <tipo>
                                 RTSHORT, DestSize,                     // <len>
                                 RTSHORT, DestPrec,                     // <dec>
                                 RTLE, 0)) == NULL) return GS_BAD;
      if (Status == FALSE)
      {
         if ((stru += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;
      }
      else
      {
         if ((stru += acutBuildList(RTLB,
                                    RTSTR, _T("STATUS"),                   // STATUS
                                    RTSTR, ProviderDescrUInt.get_name(),   // <tipo>
                                    RTSHORT, lenUInt,                      // <len>
                                    RTSHORT, decUInt,                      // <dec>
                                    RTLE, 
                                    RTLE, 0)) == NULL) return GS_BAD;
      }

      // Creo la tabella
      if (pConn->CreateTable(TableRef.get_name(), stru.get_head()) == GS_BAD) return GS_BAD;

      // Ricavo ora catalogo schema e nome della tabella
      if (pConn->split_FullRefTable(TableRef, Catalog, Schema, Table) == GS_BAD)
      {  
         // Se fallisce cancello tabella link creata precedentemente
         pConn->DelTable(TableRef.get_name()); 
         return GS_BAD;
      }
      // Creo l'indice per CLASS_ID, SUB_CL_ID, KEY_PRI
      Index1 = Table;
      Index1 += _T("L");
      // Creo l' indice
      if (Index1.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                 Schema.get_name(),
                                                 Index1.get_name(),
                                                 _T("CLASS_ID, SUB_CL_ID, KEY_PRI"))) == NULL)
         return GS_BAD;
      if (pConn->CreateIndex(Index1.get_name(), TableRef.get_name(), 
                             _T("CLASS_ID, SUB_CL_ID, KEY_PRI"), INDEX_NOUNIQUE) == GS_BAD) 
      {
         // Se fallisce cancello tabella link creata precedentemente
         pConn->DelTable(TableRef.get_name()); 
         return GS_BAD;
      }
      // Creo indice per LINK_ID
      Index2 = Table;
      Index2 += _T("G");
      // Creo l' indice
      if (Index2.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                 Schema.get_name(),
                                                 Index2.get_name(),
                                                 _T("KEY_ATTRIB"))) == NULL)
         return GS_BAD;
      if (pConn->CreateIndex(Index2.get_name(), TableRef.get_name(), 
                             _T("KEY_ATTRIB"), INDEX_NOUNIQUE) == GS_BAD) 
      {         
         // Se fallisce cancello tabella link creata precedentemente
         pConn->DelTable(TableRef.get_name()); 
         pConn->DelTable(Index1.get_name()); 
         return GS_BAD;
      }
   }

   return GS_GOOD;  
}

  
/*********************************************************/
/*.doc C_SECONDARY::default_to_db <internal> */
/*+
  Questa funzione modifica i valori di default della tabella secondaria.
  Parametri:
  C_ATTRIB_LIST *p_attrib_list; lista attributi
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::default_to_db(C_ATTRIB_LIST *p_attrib_list)
{
   C_DBCONNECTION *pConn;

   // ricavo connessione OLE-DB
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   
   if (gsc_default_to_db(pConn, ptr_info()->OldTableRef.get_name(), ptr_info()->key_attrib.get_name(), 
                         ptr_attrib_list(), p_attrib_list) == GS_BAD)
      return GS_BAD;
    
   return GS_GOOD;   
}


/*********************************************************/
/*.doc C_SECONDARY::carattattr_to_db <internal> */
/*+
  Questa funzione scarica la lista delle caratteristiche degli attributi
  della tabella secondaria nei database.
  Parametri:
  C_ATTRIB_LIST *p_attrib_list;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::carattattr_to_db(C_ATTRIB_LIST *p_attrib_list)
{
   if (gsc_carattattr_to_db((C_NODE *) pCls->ptr_id()->pPrj, pCls->ptr_id()->code,
                            pCls->ptr_id()->sub_code, code, p_attrib_list) == GS_BAD) return GS_BAD;
   
   return GS_GOOD;
}                  
   
 
/*********************************************************/
/*.doc create <internal> */
/*+
  Questa funzione crea una tabella secondaria nei database.
    
  Restituisce codice secondaria in caso di successo altrimenti
  restituisce 0.
-*/  
/*********************************************************/
int C_SECONDARY::create()
{
   int             new_code, result;
   C_PROJECT      *pFatherPrj = (C_PROJECT *) pCls->ptr_id()->pPrj;
   C_DBCONNECTION *pConn;
   C_RB_LIST       pSec;

   // verifico abilitazione
   if (gsc_check_op(opCreateSec) == GS_BAD) return 0;

   // Inizializzazioni
   abilit = GSUpdateableData;

   // verifico dati base della secondaria
   if (is_validsec(pFatherPrj->get_key()) == GS_BAD) return 0;

   // ricavo connessione OLE-DB
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return 0;
   if (attrib_list.init_ADOType(pConn) == GS_BAD) return 0;

   // carico eventuali funzioni di calcolo in GSL.GSL del progetto della classe
   gsc_load_gsl(((C_PROJECT *) pCls->ptr_id()->pPrj)->get_dir());
   // verifico attrib list
   if (attrib_list.is_valid(pConn) == GS_BAD) { gs_gsl_reload(); return 0; }
   if (attrib_list.is_valid(CAT_SECONDARY, GSInternalSecondaryTable, pConn,
                            ptr_info()->key_attrib) == GS_BAD)
      { gs_gsl_reload(); return 0; }
	// ricarico GSL.GSL giusto
	gs_gsl_reload();

   pSec << acutBuildList(RTLB, 0);
   // Valori base
   if (baseval_to_rb_db(pSec) == GS_BAD) return 0;
   // C_INFO
   if ((pSec += info.to_rb(true, true)) == NULL) return 0;
   pSec += acutBuildList(RTLE, 0);

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return 0;

   // creo directory della tabella, creo tabella e indice 
   if (create_tab(OLD) == GS_BAD) { pCls->set_share_use(); return 0; }
   // creo tabella di link e indice 
   if (create_tab_link(FALSE, OLD) == GS_BAD)
      { pConn->DelTable(ptr_info()->OldTableRef.get_name()); pCls->set_share_use(); return 0; }
   
   // inserisco nuova classe in GS_CLASS
   if ((new_code = ins_sec_to_gs_sec(pSec)) == GS_BAD)
   {
      C_STRING LnkTableRef;
      pConn->DelTable(ptr_info()->OldTableRef.get_name());
      // Cancello tabella link
      getOldLnkTableRef(LnkTableRef);
      pConn->DelTable(LnkTableRef.get_name());
      pCls->set_share_use();
      return 0;
   }
      
   // scarico valori di default in tabella
   if (default_to_db(&attrib_list) == GS_BAD)
      { del((TCHAR *) GEOsimAppl::GS_USER.pwd); pCls->set_share_use(); return 0; } 
   
   // scarico su tabella GS_ATTR le caratteristiche degli attributi
   if (carattattr_to_db(&attrib_list) == GS_BAD)
      { del((TCHAR *) GEOsimAppl::GS_USER.pwd); pCls->set_share_use(); return 0; }

   pCls->set_share_use(); // sblocco classe

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pFatherPrj->get_key());

   return new_code;
}   

 
/*********************************************************/
/*.doc link <internal> */
/*+
  Questa funzione crea un link tra una tabella secondaria esterna ed
  una classe GEOSIM (nei database).
  Parametri:

  Restituisce codice secondaria in caso di successo altrimenti 0.
-*/  
/*********************************************************/
int C_SECONDARY::link()
{
   int            new_code, result;
   C_PROJECT      *pFatherPrj = (C_PROJECT *) pCls->ptr_id()->pPrj;
   C_RB_LIST      pSec;
   C_DBCONNECTION *pConn;

   // verifico abilitazione
   if (gsc_check_op(opCreateLinkSec) == GS_BAD) return 0;

   // Si tratta di una secondaria esterna a GEOsim
   type = GSExternalSecondaryTable;
   // leggo ATTRIBUTE LIST
   if (read_sec_extattriblist() == GS_BAD) return 0;
   // verifico dati base della secondaria
   if (is_validsec(pFatherPrj->get_key()) == GS_BAD) return 0;

   // ricavo connessione OLE-DB
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return 0;
   if (attrib_list.init_ADOType(pConn) == GS_BAD) return 0;

   // carico eventuali funzioni di calcolo in GSL.GSL del progetto della classe
   gsc_load_gsl(((C_PROJECT *) pCls->ptr_id()->pPrj)->get_dir());
   // verifico attrib list
   if (attrib_list.is_valid(pConn) == GS_BAD) { gs_gsl_reload(); return 0; }
	// ricarico GSL.GSL giusto
	gs_gsl_reload();

   pSec << acutBuildList(RTLB, 0);
   // Valori base 
   if (baseval_to_rb_db(pSec) == GS_BAD) return 0; 
   // C_INFO
   if ((pSec += info.to_rb(true, true)) == NULL) return 0;
   pSec += acutBuildList(RTLE, 0);

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return 0;
   
   // Inserisco nuova classe in GS_CLASS
   if ((new_code = ins_sec_to_gs_sec(pSec)) == GS_BAD)
      { del((TCHAR *) GEOsimAppl::GS_USER.pwd); pCls->set_share_use(); return 0; }

   pCls->set_share_use(); // sblocco classe

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pFatherPrj->get_key());

   return new_code;
}   
                           
  
/*********************************************************/
/*.doc del <internal> */
/*+
  Questa funzione cancella una secondaria.
  Parametri:
  const TCHAR *Password;   Password dell'utente corrente (per controllo)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::del(const TCHAR *Password)
{
   C_DBCONNECTION *pConn;
   C_STRING       LnkTableRef, SecTableRef, statement;
   int            result;

   // Verifico abilitazione
   if (gsc_check_op(opDelSec) == GS_BAD) return GS_BAD;
   // Verifico che la password sia corretta
   if (gsc_strcmp(Password, (TCHAR *) GEOsimAppl::GS_USER.pwd) != 0)
      { GS_ERR_COD = eGSInvalidPwd; return GS_BAD; }

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return GS_BAD;

   // Si puo' cancellare solo una secondaria di GEOSIM
   if (type == GSInternalSecondaryTable)
   { 
     // Ricavo la connessione OLE-DB dell' OLD
      if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL)
         { pCls->set_share_use(); return GS_BAD; }
      // Cancello tabella principale
      if (pConn->DelTable(ptr_info()->OldTableRef.get_name()) == GS_BAD)
         { pCls->set_share_use(); return GS_BAD; }
      // Cancello tabella link
      if (getOldLnkTableRef(LnkTableRef) == GS_BAD)
         { pCls->set_share_use(); return GS_BAD; }
      if (pConn->DelTable(LnkTableRef.get_name()) == GS_BAD)
         { pCls->set_share_use(); return GS_BAD; }
   }

   // cancello abilitazioni alla tabella secondaria per tutti gli utenti 
   gsc_delSecPermissions(pCls->get_PrjId(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code, code);

   // Cancello caratteristiche attributi
   if (del_db_carattattr() == GS_BAD)
      { pCls->set_share_use(); return GS_BAD; }

   if (pCls->get_pPrj()->getSecsTabInfo(&pConn, &SecTableRef) == GS_BAD)
      { pCls->set_share_use(); return GS_BAD; }
   // cancello la riga in GS_SEC
   statement = _T("DELETE FROM ");
   statement += SecTableRef;
   statement += _T(" WHERE GS_ID=");
   statement += code;
   statement += _T(" AND CLASS_ID=");
   statement += pCls->ptr_id()->code;
   statement += _T(" AND SUB_CL_ID=");
   statement += pCls->ptr_id()->sub_code;
   if (pConn->ExeCmd(statement) == GS_BAD) return GS_BAD;

   // Cancellazione files di supporto
   DelSupportFiles();

   pCls->set_share_use(); // sblocco classe

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pCls->get_PrjId());

   return GS_GOOD;
}
/* Funzioni base della classe C_SECONDARY */


/*************************************************************/
/*.doc C_SECONDARY::isDataModified                <external> */
/*+                                                                       
  Ritorna GS_GOOD se c'è stato almeno un cambiamento della 
  banca dati della classe (inserimento, cancellazione, modifica) 
  altrimenti GS_BAD.
-*/  
/*************************************************************/
int C_SECONDARY::isDataModified(void)
{
   C_DBCONNECTION *pConn;
   C_STRING        TempTable;
   int             result;

   if (type == GSInternalSecondaryTable) return GS_BAD;
   // se modificabile ed estratta
   if (pCls->ptr_id()->abilit == GSUpdateableData && pCls->is_extracted() == GS_GOOD)
   {
      // Ricavo connessione OLE - DB con tabella TEMP
      if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
      // Ricavo il riferimento completo alla tabella temporanea della secondaria
      if (getTempTableRef(TempTable, GS_BAD) == GS_BAD) return GS_BAD;
      // Verifico l' esistenza della tabella
      if (pConn->ExistTable(TempTable) == GS_GOOD) 
         result = GS_GOOD;
      else
         result = GS_BAD;
   }
   else
      result = GS_BAD;

   return result;
}


/*************************************************************/
/*.doc C_SECONDARY::DynSegmentationType           <external> */
/*+                                                                       
  Ritorna il tipo di eventi supportati dalla segmentazione dinamica.
  GSNoneDynSegmentation = se la segmentazione dinamica non è supportata
  GSPunctualDynSegmentation = se gli eventi supportati dalla segmentazione 
                              dinamica sono puntuali (es. incidenti lungo una strada)
  GSLinearDynSegmentation = se gli eventi supportati dalla segmentazione 
                            dinamica sono lineari (es. stato di un tubo lungo la condotta)
-*/  
/*************************************************************/
DynamicSegmentationTypeEnum C_SECONDARY::DynSegmentationType(void)
{
   if (info.real_init_distance_attrib.len() == 0) return GSNoneDynSegmentation;
   if (info.real_final_distance_attrib.len() == 0) return GSPunctualDynSegmentation;
   return GSLinearDynSegmentation;
}


/*********************************************************/
/*.doc C_SECONDARY::is_NewEntity                <external> */
/*+                                                                       
  Funzione che verifica se una scheda secondaria di GEOsim è nuova. 
  Parametri:
  long Key;          Codice scheda secondaria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::is_NewEntity(long Key)
{
   if (type != GSInternalSecondaryTable) return GS_BAD;
   return (Key < 0) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc gsc_destroy_tab_sec <external> */
/*+
  Questa funzione cancella tutto quello che è possibile cancellare di una
  tabella secondaria di GEOsim. Questa funzione è da usare solo nel caso non 
  sia possibile caricare correttamente la tabella secondaria e quindi ottenere
  un oggetto C_SECONDARY o suo derivato e perciò applicare la funzione del().
  Parametri:
  int prj;               codice progetto
  int cls;               codice classe
  int sub;               codice sottoclasse
  int sec                codice tabella secondaria
  const TCHAR *Password; Password dell'utente corrente (per controllo)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Questa funzione potrebbe lasciare sporco il disco o tabelle di GEOsim.
       (es. distruzione di una secondaria la cui tabella si trova su una 
        macchina spenta...)
-*/  
/*********************************************************/
int gsc_destroy_tab_sec(int prj, int cls, int sub, int sec, const TCHAR *Password)
{
   C_PROJECT      *pPrj;
   presbuf        colvalues = NULL, p;
   int            result, type;
   C_CLASS        *pCls;
   C_NODE         *pSec;
   C_DBCONNECTION *pConn, *pSecConn;
   C_STRING       TableRef, ClsTableRef, LnkTableRef, ConnStrUDLFile, UDLProperties, statement;
   C_RB_LIST      ColValues;
   _RecordsetPtr  pRs;
   C_2STR_LIST    PropList;
   long           SessionCode;

   // verifico abilitazione
   if (gsc_superuser() != GS_GOOD)
      { GS_ERR_COD = eGSOpNotAble; return RTERROR; } // non è un SUPER USER
   // verifico che la password sia corretta
   if (gsc_strcmp(Password, (TCHAR *) GEOsimAppl::GS_USER.pwd) != 0)
      { GS_ERR_COD = eGSInvalidPwd; return GS_BAD; }

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT*) GEOsimAppl::PROJECTS.search_key(prj)) == NULL) return GS_BAD;

   if (gsc_superuser() != GS_GOOD)  // non è un SUPER USER
   {  
      C_INT_INT_LIST usr_listclass;    // lista classi utente
      C_INT_INT      *pusr_class;

      // Lista classi utente
      if (gsc_getClassPermissions(GEOsimAppl::GS_USER.code, prj, usr_listclass) == GS_BAD) return GS_BAD;
      
      if (!(pusr_class = (C_INT_INT *) usr_listclass.search_key(cls)) ||  // nessuna abilitazione   
          pusr_class->get_type() != (int) GSUpdateableData) // abilitazione non in modifica
         { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
   }
   
   // Verifico se la classe fa parte di una sessione di lavoro
   if (gsc_is_inarea(prj, cls, &SessionCode) == GS_BAD || SessionCode > 0)
      { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

   // Ricavo connessione e riferimento alla tabella GS_SEC
   if (pPrj->getSecsTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE GS_ID=");
   statement += sec;
   statement += _T(" AND CLASS_ID=");
   statement += cls;
   statement += _T(" AND SUB_CL_ID=");
   statement += sub;

   if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD)
       return GS_BAD;

   do
   {
      // Leggo il record
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) break;
      
      // Memorizzo il tipo della secondaria
      if ((p = ColValues.CdrAssoc(_T("TYPE"))) == NULL ||
          gsc_rb2Int(p, &type) == GS_BAD) type = 0;
      
      // Memorizzo l' UDL
      if ((p = ColValues.CdrAssoc(_T("UDL_FILE"))) == NULL || p->restype != RTSTR)
         { GS_ERR_COD = eGSInvRBType; result = GS_BAD; break; }
      ConnStrUDLFile = gsc_alltrim(p->resval.rstring);
      if (gsc_path_exist(ConnStrUDLFile) == GS_GOOD)
         // traduco dir assoluto in dir relativo
         if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD)
            { result = GS_BAD; break; }

      // Memorizzo le proprietà dell' UDL
      if ((p = ColValues.CdrAssoc(_T("UDL_PROP"))) != NULL)
      {
         if (p->restype == RTSTR)
         {
            UDLProperties = gsc_alltrim(p->resval.rstring);
            // Conversione path UDLProperties da assoluto in dir relativo
            if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(), UDLProperties) == GS_BAD)
            if (gsc_PropListFromConnStr(UDLProperties.get_name(), PropList) == GS_BAD)
                  { result = GS_BAD; break; }
         }
         else
            PropList.remove_all();

         if ((pSecConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name(), &PropList)) == NULL) break;
            
         // TABLE_REF
         TableRef = GS_EMPTYSTR;
         if ((p = ColValues.CdrAssoc(_T("TABLE_REF"))) != NULL && p->restype == RTSTR)
            // Conversione riferimento tabella da dir assoluto in relativo
            if (TableRef.paste(pSecConn->FullRefTable_nethost2drive(gsc_alltrim(p->resval.rstring))) == NULL)
               { result = GS_BAD; break; }
      
         // Cancelliamo solo se secondaria di GEOsim
         if (type == GSInternalSecondaryTable)
         { 
            // Cancello tabella principale
            pSecConn->DelTable(TableRef.get_name());
            // Ricavo il nome della tabella Link dal nome originale
            if (gsc_getSecLnkTableRef(pSecConn, TableRef, LnkTableRef) == GS_GOOD)
               // Cancello tabella link
               pSecConn->DelTable(LnkTableRef.get_name());

            if (pConn->get_CatalogResourceType() == DirectoryRes)
            {
               C_STRING Catalog, Schema, Table, Path;

               if (pConn->split_FullRefTable(TableRef, Catalog,
                                             Schema, Table) == GS_GOOD &&
                   gsc_RefTable2Path(pConn->get_DBMSName(), Catalog, Schema, Table, Path) == GS_GOOD)
                  // Cancello direttorio
                  gsc_rmdir_from_path(Path.get_name());
            }
            
            // cancello caratteristiche attributi
            gsc_del_db_carattattr(pPrj, cls, sub, sec);
         }
      }
   }
   while (0);

   // cancello caratteristiche classe sbloccando la riga in GS_SEC
   gsc_DBDelRow(pRs);
   gsc_DBCloseRs(pRs);

   // se la classe madre era caricata in memoria
   if (pCls = (C_CLASS *) pPrj->ptr_classlist()->C_LIST::search_key(cls))
      // se è stata caricata in memoria almeno una secondaria
      if (pCls->ptr_secondary_list != NULL)
         // la cerco solo in memoria senza caricarla da DB
         if (pSec = pCls->ptr_secondary_list->C_LIST::search_key(sec))
            // se era caricata in memoria la cancello dalla lista
            pCls->ptr_secondary_list->remove(pSec);
 
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_stru_valdef2attriblist <external> */
/*+
  Questa funzione carica su una C_ATTRIB_LIST precedentemente gia'
  allocata, la lista delle colonne di una tabella e per ciascun attributo
  il valore di default.
  Parametri:
  C_SECONDARY *pSec;            puntatore alla classe
  int         def;              flag se GS_GOOD -> carica anche i valori di default
    
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_stru_valdef2attriblist(C_SECONDARY *pSec, int def)
{
   C_ATTRIB_LIST  *p_attrib_list;
   C_INFO         *p_info;
   C_DBCONNECTION *pConn;

   if (!pSec) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if ((p_info = pSec->ptr_info()) == NULL ||(p_attrib_list = pSec->ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // Ricavo connessione OLE-DB
   if ((pConn = p_info->getDBConnection(OLD)) == NULL) return GS_BAD;

   if (gsc_stru_valdef2attriblist(p_info->OldTableRef, pConn, p_attrib_list,
                                  p_info->key_attrib.get_name(), def) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::mod_link <internal> */
/*+
  Questa funzione modifica key_pri e key_attrib di una 
  tabella secondaria esterna.
  Parametri:
  C_INFO_SEC *pinfo 

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::mod_link(C_INFO_SEC *pinfo)
{             
   C_ATTRIB_LIST  *p_attrib_list;
   C_ATTRIB       *p_attrib1, *p_attrib2;
   C_RB_LIST      pSec;
   int            result;
   C_DBCONNECTION *pConn;

   // verifico abilitazione
   if (gsc_check_op(opModSec) == GS_BAD) return GS_BAD;

   if (type == GSInternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }

   if ((p_attrib_list = ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   // verifico dati della C_INFO_SEC
   if (gsc_valid_attrib_name(pinfo->key_attrib, pConn, p_attrib_list) == GS_BAD)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   p_attrib2 = (C_ATTRIB *) p_attrib_list->search_name(pinfo->key_attrib.get_name(), FALSE);

   if ((pConn = pCls->ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   if (gsc_valid_attrib_name(pinfo->key_pri, pConn, pCls->ptr_attrib_list()) == GS_BAD)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   p_attrib1 = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(pinfo->key_pri.get_name(), FALSE);
   // verifico compatibilità tra attributi
   // Restituisce GS_GOOD in caso di piena compatibilità, GS_CAN se i tipi sono
   // compatibili ma potrebbe esserci una perdita di dati, GS_BAD se i tipi non 
   // sono compatibili.

   // Scommentare la seguente funzione
   // if (gsc_compatib_Fields(pCls->ptr_info()->driver, p_attrib1->type, 
   //                         p_attrib1->len, p_attrib1->dec,
   //                         info.driver, p_attrib2->type, 
   //                         p_attrib2->len, p_attrib2->dec) == GS_BAD)
   //    { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   if ((pSec << acutBuildList(RTLB,
                                 RTLB, RTSTR, _T("KEY_ATTRIB"), RTSTR, pinfo->key_attrib.get_name(), RTLE,
                                 RTLB, RTSTR, _T("KEY_PRI"), RTSTR, pinfo->key_pri.get_name(), RTLE,
                              RTLE, 0)) == NULL) return GS_BAD; 

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return GS_BAD;

   // Modifico riga in GS_SEC
   if (mod_sec_to_gs_sec(pSec) == GS_BAD)
      { pCls->set_share_use(); return GS_BAD; }

   // sblocco classe
   if (pCls->set_share_use() == GS_BAD) return GS_BAD;
     
   info.key_attrib = pinfo->key_attrib;
   info.key_pri    = pinfo->key_pri;

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pCls->get_PrjId());

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::mod_vis                   <internal> */
/*+
  Questa funzione modifica vis_attrib di una tabella secondaria.
  Parametri:
  C_STRING &vis;        Attributo per l'ordine di visualizzazione
  bool desc_order_type; Flag opzionale. Se vero l'ordine deve essere
                        decrescente (default = falso)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::mod_vis(C_STRING &vis, bool desc_order_type)
{
   C_ATTRIB_LIST  *p_attrib_list;
   C_RB_LIST      pSec;
   C_DBCONNECTION *pConn;
   int            result;

   // verifico abilitazione
   if (gsc_check_op(opModSec) == GS_BAD) return GS_BAD;

   if ((p_attrib_list = ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   // Se esiste vis, verifico la correttezza; vis mi può arrivare vuoto 
   if (vis.len() > 0)
   {
      if (pConn->IsValidFieldName(vis) == GS_BAD) return GS_BAD;
      // verifico la sua validità 
      if (gsc_valid_attrib_name(vis, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidVis_attrib; return GS_BAD; }
   }
   else
      desc_order_type = false;

   if ((pSec << acutBuildList(RTLB,
                              RTLB, RTSTR, _T("VIS_ATTRIB"), RTSTR, vis.get_name(), RTLE,
                              0)) == NULL) return GS_BAD; 
   if (desc_order_type)
   {
      if ((pSec += acutBuildList(RTLB, RTSTR, _T("VIS_ATTRIB_DESC_ORDER_TYPE"), RTT, RTLE,
                                 0)) == NULL) return GS_BAD; 
   }
   else
      if ((pSec += acutBuildList(RTLB, RTSTR, _T("VIS_ATTRIB_DESC_ORDER_TYPE"), RTNIL, RTLE,
                                 0)) == NULL) return GS_BAD; 

   if ((pSec += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return GS_BAD;

   // Modifico riga in GS_SEC
   if (mod_sec_to_gs_sec(pSec) == GS_BAD)
      { pCls->set_share_use(); return GS_BAD; }

   // sblocco classe
   if (pCls->set_share_use() == GS_BAD) return GS_BAD;

   info.vis_attrib = vis;
   if (vis.len() > 0) 
      info.vis_attrib_desc_order_type = desc_order_type;

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pCls->get_PrjId());

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::mod_dynamic_segmentation  <internal> */
/*+
  Questa funzione modifica la segmentazione dinamica di una tabella secondaria.
  Parametri:
  C_STRING &_real_init_dist_attrib;
  C_STRING &_real_final_dist_attrib;
  C_STRING &_nominal_init_dist_attrib;
  C_STRING &_nominal_final_dist_attrib;
  C_STRING &_real_offset_attrib;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::mod_dynamic_segmentation(C_STRING &_real_init_dist_attrib,
                                          C_STRING &_real_final_dist_attrib,
                                          C_STRING &_nominal_init_dist_attrib,
                                          C_STRING &_nominal_final_dist_attrib,
                                          C_STRING &_real_offset_attrib)
{         
   C_ATTRIB_LIST  *p_attrib_list;
   C_RB_LIST      pSec;
   C_DBCONNECTION *pConn;
   int            result;

   // verifico abilitazione
   if (gsc_check_op(opModSec) == GS_BAD) return GS_BAD;

   if ((p_attrib_list = ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   // Se esiste _real_init_dist_attrib, verifico la correttezza, mi può arrivare vuoto
   if (_real_init_dist_attrib.len() > 0)
   {
      // verifico la sua validità 
      if (gsc_valid_attrib_name(_real_init_dist_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
   }
   // Se esiste _real_final_dist_attrib, verifico la correttezza, mi può arrivare vuoto
   if (_real_final_dist_attrib.len() > 0)
   {
      // verifico la sua validità 
      if (gsc_valid_attrib_name(_real_final_dist_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
   }
   // Se esiste _nominal_init_dist_attrib, verifico la correttezza, mi può arrivare vuoto
   if (_nominal_init_dist_attrib.len() > 0)
   {
      // verifico la sua validità 
      if (gsc_valid_attrib_name(_nominal_init_dist_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
   }
   // Se esiste _nominal_final_dist_attrib, verifico la correttezza, mi può arrivare vuoto
   if (_nominal_final_dist_attrib.len() > 0)
   {
      // verifico la sua validità 
      if (gsc_valid_attrib_name(_nominal_final_dist_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
   }

   if ((pSec << acutBuildList(RTLB, RTLB, RTSTR, _T("REAL_INIT_DISTANCE_ATTRIB"), 0)) == NULL) return GS_BAD; 
   if ((pSec += gsc_str2rb(_real_init_dist_attrib)) == NULL) return GS_BAD;
   if ((pSec += acutBuildList(RTLE, RTLB, RTSTR, _T("REAL_FINAL_DISTANCE_ATTRIB"), 0)) == NULL) return GS_BAD; 
   if ((pSec += gsc_str2rb(_real_final_dist_attrib)) == NULL) return GS_BAD;
   if ((pSec += acutBuildList(RTLE, RTLB, RTSTR, _T("NOMINAL_INIT_DISTANCE_ATTRIB"), 0)) == NULL) return GS_BAD; 
   if ((pSec += gsc_str2rb(_nominal_init_dist_attrib)) == NULL) return GS_BAD;
   if ((pSec += acutBuildList(RTLE, RTLB, RTSTR, _T("NOMINAL_FINAL_DISTANCE_ATTRIB"), 0)) == NULL) return GS_BAD; 
   if ((pSec += gsc_str2rb(_nominal_final_dist_attrib)) == NULL) return GS_BAD;
   if ((pSec += acutBuildList(RTLE, RTLB, RTSTR, _T("REAL_OFFSET_ATTRIB"), 0)) == NULL) return GS_BAD; 
   if ((pSec += gsc_str2rb(_real_offset_attrib)) == NULL) return GS_BAD;
   if ((pSec += acutBuildList(RTLE, RTLE, 0)) == NULL) return GS_BAD;

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return GS_BAD;

   // Modifico riga in GS_SEC
   if (mod_sec_to_gs_sec(pSec) == GS_BAD)
      { pCls->set_share_use(); return GS_BAD; }

   // sblocco classe
   if (pCls->set_share_use() == GS_BAD) return GS_BAD;

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pCls->get_PrjId());
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::mod_name <internal> */
/*+
  Questa funzione modifica name di una tabella secondaria
  esterna.
  Parametri:
  TCHAR *name;       Nome tabella secondaria

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::mod_name(TCHAR *name_sec)
{                   
   C_RB_LIST pSec;
   int       result;

   // Verifico abilitazione
   if (gsc_check_op(opModSec) == GS_BAD) return GS_BAD;
   if (!isValidName(name_sec)) { GS_ERR_COD = eGSInvSecName; return GS_BAD; }

   if ((pSec << acutBuildList(RTLB,
                                 RTLB, RTSTR, _T("NAME"), RTSTR, name_sec, RTLE,
                              RTLE, 0)) == NULL)
      return GS_BAD;

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return GS_BAD;

   // Modifico riga in GS_SEC
   if (mod_sec_to_gs_sec(pSec) == GS_BAD)
      { pCls->set_share_use(); return GS_BAD; }

   // sblocco classe
   if (pCls->set_share_use() == GS_BAD) return GS_BAD;

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pCls->get_PrjId());

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::mod_Descr                 <internal> */
/*+
  Questa funzione modifica la descrizione di una tabella secondaria
  esterna.
  Parametri:
  TCHAR *NewDescr;      Descrizione della tabella secondaria

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::mod_Descr(TCHAR *NewDescr)
{          
   C_RB_LIST pSec;
   int       result;

   // Verifico abilitazione
   if (gsc_check_op(opModSec) == GS_BAD) return GS_BAD;

   if (!isValidDescr(NewDescr)) return GS_BAD;

   if ((pSec << acutBuildList(RTLB,
                                 RTLB, RTSTR, _T("DESCRIPTION"), 0)) == NULL)
      return GS_BAD;
   if ((pSec += gsc_str2rb(NewDescr)) == NULL) return GS_BAD;
   if ((pSec += acutBuildList(RTLE, RTLE, 0)) == NULL) return GS_BAD;  

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return GS_BAD;

   // Modifico riga in GS_SEC
   if (mod_sec_to_gs_sec(pSec) == GS_BAD)
      { pCls->set_share_use(); return GS_BAD; }

   // sblocco classe
   if (pCls->set_share_use() == GS_BAD) return GS_BAD;

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pCls->get_PrjId());

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::is_validsec <internal> */
/*+
  Questa funzione controlla la validita' dei dati di base della
  tabella secondaria.
  Parametri:
  int prj;   Codice del progetto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::is_validsec(int prj)
{                   
   C_STRING         Catalog, Schema, Table;
   C_ATTRIB_LIST   *p_attrib_list;
   C_ATTRIB        *p_attrib1, *p_attrib2;
   C_DBCONNECTION  *pConn;

   if (!isValidName(name)) return GS_BAD;
   if (!isValidDescr(Descr.get_name())) return GS_BAD;

   if ((p_attrib_list = ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   // Ricavo connessione OLE-DB per tabella
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   // In OldTableRef ho il riferimento completo alla tabella OLD ricavo il nome della tabella
   if (pConn->split_FullRefTable(ptr_info()->OldTableRef, Catalog, Schema, Table) == GS_BAD)
      return GS_BAD;
   // Verifico correttezza nome file (solo alfanumerici)
   if (pConn->IsValidTabName(Table) == GS_BAD) return GS_BAD;
   // Tolgo 1 perchè aggiungero un carattere io per discriminare tab e lnk
   if (type == GSInternalSecondaryTable)
      if (Table.len() > (size_t) pConn->get_MaxTableNameLen() - 1) 
         { GS_ERR_COD = eGSInvTabName; return GS_BAD; }
   // Verifico la correttezza dell' attributo chiave
   if (gsc_valid_attrib_name(info.key_attrib, pConn, p_attrib_list) == GS_BAD)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   // Verifico attributo per ordine di sfogliamento schede secondarie
   if (info.vis_attrib.len() > 0)
      if (gsc_valid_attrib_name(info.vis_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidVis_attrib; return GS_BAD; }
   // Ricavo connessione OLE-DB per tabella
   if ((pConn = pCls->ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   // Verifico attributo chiave della principale
   if (info.key_pri.len() == 0)
      info.key_pri = pCls->ptr_info()->key_attrib; 
   if (gsc_valid_attrib_name(info.key_pri, pConn, pCls->ptr_attrib_list()) == GS_BAD)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   
   p_attrib2 = (C_ATTRIB *) p_attrib_list->search_name(info.key_attrib.get_name(), FALSE);
   p_attrib1 = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(info.key_pri.get_name(), FALSE);
   // verifico compatibilità tra attributi
   // Restituisce GS_GOOD in caso di piena compatibilità, GS_CAN se i tipi sono
   // compatibili ma potrebbe esserci una perdita di dati, GS_BAD se i tipi non 
   // sono compatibili.
   
   // Scommentare la seguente funzione
   // if (gsc_compatib_Fields(pCls->ptr_info()->driver, p_attrib1->type, 
   //                         p_attrib1->len, p_attrib1->dec,
   //                         info.driver, p_attrib2->type, 
   //                         p_attrib2->len, p_attrib2->dec) == GS_BAD) return GS_BAD;

   // Verifico attributi per segmentazione dinamica
   if (info.real_init_distance_attrib.len() > 0)
      if (gsc_valid_attrib_name(info.real_init_distance_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
   if (info.real_final_distance_attrib.len() > 0)
   {  // se c'è il finale deve esserci anche quello iniziale
      if (info.real_init_distance_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
      if (gsc_valid_attrib_name(info.real_final_distance_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
   }
   if (info.nominal_init_distance_attrib.len() > 0)
      if (gsc_valid_attrib_name(info.nominal_init_distance_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
   if (info.nominal_final_distance_attrib.len() > 0)
   {  // se c'è il finale deve esserci anche quello iniziale
      if (info.nominal_init_distance_attrib.len() == 0)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
      if (gsc_valid_attrib_name(info.nominal_final_distance_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }
   }
   if (info.real_offset_attrib.len() > 0)
      if (gsc_valid_attrib_name(info.real_offset_attrib, pConn, p_attrib_list) == GS_BAD)
         { GS_ERR_COD = eGSInvalidDynamSegment_attrib; return GS_BAD; }

   return GS_GOOD;
}
                                         

/*********************************************************/
/*.doc C_SECONDARY::ins_sec_to_gs_sec        <internal> */
/*+
  Questa funzione inserisce una nuova classe in GS_SEC.
  La lista resbuf viene modificata cosi come la C_ID della secondaria.
  Parametri:
  C_RB_LIST &ColValues;       lista colonna-valore
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::ins_sec_to_gs_sec(C_RB_LIST &ColValues)
{
   C_DBCONNECTION  *pConn;
   C_STRING         TableSec, Path, AbsUDLProperties;
   presbuf          p = NULL;
   int              newcode, n_test = 0;
         
   // Ricavo il riferimento completo alla tabella GS_SEC del progetto
   ((C_PROJECT *)pCls->ptr_id()->pPrj)->getSecsTabInfo(&pConn, &TableSec);
      
   if ((p = ColValues.CdrAssoc(_T("UDL_FILE"))) != NULL)
   {
      if (ptr_info() && p->restype == RTSTR && wcslen(p->resval.rstring) > 0)
      {  
         // traduco dir relativo in dir assoluto
         Path = p->resval.rstring;
         if (gsc_path_exist(Path) == GS_GOOD)
            if (gsc_drive2nethost(Path) == GS_BAD) return GS_BAD;
         if (Path.len() > ACCESS_MAX_LEN_FIELDCHAR)
            { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }
         if (gsc_RbSubst(p, Path.get_name()) == GS_BAD) return GS_BAD;
      }
      else
         if (gsc_RbSubst(p, GS_EMPTYSTR) == GS_BAD) return GS_BAD;

      if ((p = ColValues.CdrAssoc(_T("UDL_PROP"))) != NULL)
      {
         if (ptr_info() && p->restype == RTSTR && wcslen(p->resval.rstring) > 0)
         {
            AbsUDLProperties = p->resval.rstring;
            if (AbsUDLProperties.len() > 0)
            {
               C_DBCONNECTION *pConn;

               // Conversione path UDLProperties da dir relativo in assoluto
               if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
               if (pConn->UDLProperties_drive2nethost(AbsUDLProperties) == GS_BAD) return GS_BAD;
               if (gsc_RbSubst(p, AbsUDLProperties.get_name()) == GS_BAD) return GS_BAD;
            }
         }
      }

      if ((p = ColValues.CdrAssoc(_T("TABLE_REF"))) != NULL)
      {
         if (ptr_info() && p->restype == RTSTR && wcslen(p->resval.rstring) > 0)
         {
            C_STRING AbsUDLProperties(p->resval.rstring);

            if (AbsUDLProperties.len() > 0)
            {
               C_DBCONNECTION *pConn;

               // Conversione riferimento tabella da dir relativo in assoluto
               if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
               if (AbsUDLProperties.paste(pConn->FullRefTable_drive2nethost(p->resval.rstring)) == NULL)
                  return GS_BAD;
               if (gsc_RbSubst(p, AbsUDLProperties.get_name()) == GS_BAD) return GS_BAD;
            }
         }
      }
   }
   // Inseriamo anche la versione del programma
   if ((p = ColValues.CdrAssoc(_T("VERSION"))) == NULL)
   { // aggiungo lista resbuf
      ColValues.get_tail()->restype = RTLB;
      if ((ColValues += (p = acutBuildList(RTSTR, _T("VERSION"), RTSTR, GS_EMPTYSTR,
                                           RTLE, RTLE, 0))) == NULL)
         return GS_BAD;
      p = p->rbnext;
   }
   if (p->restype != RTSTR) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   if (p->resval.rstring) free(p->resval.rstring);
   // Versione GEOsim
   if ((p->resval.rstring = gsc_tostring(gsc_msg(130))) == NULL) return GS_BAD; 

   // Ciclo di GS_NUM_TEST tentativi per inserire una nuova secondaria
   do
   {
      if (n_test == 0)
      {
         // Cerco nuovo codice per la secondaria
         if (new_code(&newcode) == GS_BAD) return GS_BAD; 

         if ((p = ColValues.CdrAssoc(_T("GS_ID"))) != NULL)
            gsc_RbSubst(p, newcode);
      }
      // Prova ad inserire nuova riga in GS_SEC
      if (pConn->InsRow(TableSec.get_name(), ColValues) == GS_GOOD) break;
      n_test++;
      if (n_test >= GEOsimAppl::GLOBALVARS.get_NumTest()) 
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      // se l'errore non era dovuto perchè il codice esisteva già, attendi
      if (GS_ERR_COD != eGSIntConstr) 
         gsc_wait(GEOsimAppl::GLOBALVARS.get_WaitTime());
   }
   while (1);
                           
   code = newcode;
   if (ptr_info() != NULL) ptr_info()->TempLastId = 0;

   return newcode;
}   


/*********************************************************/
/*.doc C_SECONDARY::new_code <external> */
/*+
  Questa funzione cerca nella tabella indicata da tablerif nella colonna
  nominata GS_ID qual'è il codice successivo, nuovo, per poter inserire 
  una nuova riga nella tabella stessa.
  Parametri:
  int *new_code             nuovo codice progressivo
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::new_code(int *new_code)
{
   C_DBCONNECTION *pConn;
   C_STRING       statement, TableRef;
   C_RB_LIST      ColValues;
   _RecordsetPtr  pRs;
   presbuf        gs_id;
   int            max_cod;
   
   *new_code = 1;
   
   ((C_PROJECT *)pCls->ptr_id()->pPrj)->getSecsTabInfo(&pConn, &TableRef);
   statement = _T("SELECT GS_ID FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += pCls->ptr_id()->code;
   statement += _T(" AND SUB_CL_ID=");
   statement += pCls->ptr_id()->sub_code;
   statement += _T(" ORDER BY GS_ID");

   // Eseguo lo statement
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD; 
   
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) break;
      if ((gs_id = gsc_nth(1, gsc_nth(0, ColValues.get_head()))) == NULL) break;
      if (gsc_rb2Int(gs_id, &max_cod) == GS_BAD) break;
      *new_code = ++max_cod; 
      gsc_Skip(pRs);
   }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::del_db_carattattr <internal> */
/*+
  Questa funzione cancella la lista delle caratteristiche degli attributi
  della classe nei database
  Parametri:
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::del_db_carattattr()
{
   if (gsc_del_db_carattattr(pCls->ptr_id()->pPrj, pCls->ptr_id()->code, pCls->ptr_id()->sub_code, code) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_SECONDARY::read_sec_extattriblist <internal> */
/*+
  Questa funzione carica la lista degli attributi di una tabella
  secondaria esterna.
  Parametri:
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B.: Prima di utilizzare questa funzione deve essere caricata la 
        C_INFO_SEC.
-*/  
/*********************************************************/
int C_SECONDARY::read_sec_extattriblist(void)
{  
   C_ATTRIB_LIST *p_attrib_list;
   C_INFO_SEC    *p_info;

   if ((p_info = ptr_info()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((p_attrib_list = ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (gsc_stru_valdef2attriblist(this, GS_BAD) == GS_BAD)
      return GS_BAD;
   
   return GS_GOOD;
}
    

/*********************************************************/
/*.doc C_SECONDARY::mod_sec_to_gs_sec <internal> */
/*+
  Questa funzione modifica i dati di una secondaria già 
  esistente in GS_SEC.
  Parametri:
  C_RB_LIST &ColValues;    lista colonna - valore con dati da
                           aggiornare.   
  _RecordsetPtr &RsCls;    Recordset che blocca in GS_CLASS
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::mod_sec_to_gs_sec(C_RB_LIST &ColValues)
{
   C_DBCONNECTION *pConn;
   C_STRING       statement, SecTableRef;
   _RecordsetPtr  pRs;

   // modifico la riga in GS_SEC
   if (pCls->get_pPrj()->getSecsTabInfo(&pConn, &SecTableRef) == GS_BAD) return GS_BAD;
   statement = _T("SELECT * FROM ");
   statement += SecTableRef;
   statement += _T(" WHERE GS_ID=");
   statement += code;
   statement += _T(" AND CLASS_ID=");
   statement += pCls->ptr_id()->code;
   statement += _T(" AND SUB_CL_ID=");
   statement += pCls->ptr_id()->sub_code;

   if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD)
      return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   // Modifico riga in GS_SEC
   if (mod_sec_to_gs_sec(ColValues, pRs) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   
   return gsc_DBCloseRs(pRs);
}
int C_SECONDARY::mod_sec_to_gs_sec(C_RB_LIST &ColValues, _RecordsetPtr &pRs)
{
   presbuf p;
   C_INFO_SEC *p_info = ptr_info();
    
   // Alcuni valori non possono essere modificati in GS_SEC
   if ((p = ColValues.CdrAssoc(_T("GS_ID"))) != NULL)
      if (gsc_RbSubst(gsc_nth(1, p), code) == GS_BAD) return GS_BAD;

   if ((p = ColValues.CdrAssoc(_T("CLASS_ID"))) != NULL)
      if (gsc_RbSubst(gsc_nth(1, p), pCls->ptr_id()->code) == GS_BAD) return GS_BAD;

   if ((p = ColValues.CdrAssoc(_T("SUB_CL_ID"))) != NULL)
      if (gsc_RbSubst(gsc_nth(1, p), pCls->ptr_id()->sub_code) == GS_BAD) return GS_BAD;

   if ((p = ColValues.CdrAssoc(_T("TYPE"))) != NULL)
      if (gsc_RbSubst(gsc_nth(1, p), type) == GS_BAD) return GS_BAD;
   
   if ((p = ColValues.CdrAssoc(_T("UDL_FILE"))) != NULL)
   {
      if (ptr_info() && p->restype == RTSTR && wcslen(p->resval.rstring) > 0)
      {  // traduco dir relativo in dir assoluto
         C_STRING Path(p->resval.rstring);

         if (gsc_path_exist(Path) == GS_GOOD)
            if (gsc_drive2nethost(Path) == GS_BAD) return GS_BAD;
         if (Path.len() > ACCESS_MAX_LEN_FIELDCHAR)
            { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }
         if (gsc_RbSubst(p, Path.get_name()) == GS_BAD) return GS_BAD;
      }
      else
         if (gsc_RbSubst(p, GS_EMPTYSTR) == GS_BAD) return GS_BAD;

      if ((p = ColValues.CdrAssoc(_T("UDL_PROP"))) != NULL)
      {
         if (ptr_info() && p->restype == RTSTR && wcslen(p->resval.rstring) > 0)
         {
            C_STRING AbsUDLProperties(p->resval.rstring);

            if (AbsUDLProperties.len() > 0)
            {
               C_DBCONNECTION *pConn;

               // Conversione path UDLProperties da dir relativo in assoluto
               if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
               if (pConn->UDLProperties_drive2nethost(AbsUDLProperties) == GS_BAD) return GS_BAD;
               if (gsc_RbSubst(p, AbsUDLProperties.get_name()) == GS_BAD) return GS_BAD;
            }
         }
      }

      if ((p = ColValues.CdrAssoc(_T("TABLE_REF"))) != NULL)
      {
         if (ptr_info() && p->restype == RTSTR && wcslen(p->resval.rstring) > 0)
         {
            C_STRING AbsUDLProperties(p->resval.rstring);

            if (AbsUDLProperties.len() > 0)
            {
               C_DBCONNECTION *pConn;

               // Conversione riferimento tabella da dir relativo in assoluto
               if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
               if (AbsUDLProperties.paste(pConn->FullRefTable_drive2nethost(p->resval.rstring)) == NULL)
                  return GS_BAD;
               if (gsc_RbSubst(p, AbsUDLProperties.get_name()) == GS_BAD) return GS_BAD;
            }
         }
      }
   }

   // modifico la riga
   if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
   

/*********************************************************/
/*.doc C_SECONDARY::modi_tab <internal> */
/*+
  Questa funzione modifica la struttura di una tabella secondaria.
  La variabile globale GS_RESBUF deve contenere una lista così composta:
  ((definizione nuova struttura) (struct link list))
  Vedere funzione gsc_updstruct e gs_alloc_mod_stru().
  Parametri:
  C_ATTRIB_LIST *p_attrib_list; nuova struttura
  presbuf link;                 lista di link con vecchia struttura
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::modi_tab(C_ATTRIB_LIST *p_attrib_list, presbuf link)
{
   C_RB_LIST      Values, def_stru;
   C_STRING       value, temp_val;
   C_ATTRIB       *p_attrib;
   C_DBCONNECTION *pConn;
   C_INFO         *p_info = ptr_info();
   _RecordsetPtr  pRs;
   C_STRING       statement, SecTableRef;
   int            result;

   // Verifico abilitazione
   if (gsc_check_op(opModClass) == GS_BAD) return GS_BAD;

   if (type == GSExternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }
 
   if (p_attrib_list->is_empty() == TRUE || link == NULL)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; } 

   // Creo la lista dei valori da inserire nelle colonne della nuova tabella
   if ((Values << gsc_rblistcopy(link)) == NULL) return GS_BAD;
   // Elimino l'ultima parentesi chiusa
   if (Values.remove_tail() == GS_BAD) return GS_BAD;

   p_attrib = (C_ATTRIB *) p_attrib_list->get_head();
   while (p_attrib)
   {
      if (gsc_assoc(p_attrib->get_name(), link) == NULL)
      { 
         // Inserisco i valori di default per le nuove colonne
         if (p_attrib->def)
         {
            if ((Values += acutBuildList(RTLB,
                                         RTSTR, p_attrib->get_name(), 0)) == GS_BAD)
               return GS_BAD;

            if (p_attrib->def->restype == RTSTR)
            {
               value = _T("'");
               if ((temp_val.paste(gsc_rb2str(p_attrib->def))) == NULL) return GS_BAD;
               value += temp_val;
               value += _T("'");
               if ((Values += acutBuildList(RTSTR, value.get_name(), 0)) == GS_BAD)
                  return GS_BAD;
            }
            else
               if ((Values += gsc_copybuf(p_attrib->def)) == GS_BAD) return GS_BAD;

            if ((Values += acutBuildList(RTLE, 0)) == GS_BAD) return GS_BAD;
         }
      }
      p_attrib = (C_ATTRIB *) p_attrib->get_next();
   }
   if ((Values += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   // Ricavo connessione OLE-DB
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   // Ricavo la struttura della nuova tabella
   if ((def_stru << p_attrib_list->to_rb_db()) == NULL)
      return GS_BAD;

   // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
   // il codice della sessione attiva, se esistente, altrimenti il codice utente 
   long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
   if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
      return GS_BAD;
   // Modifico struttura tabella                                  
   if (pConn->UpdStruct(p_info->OldTableRef.get_name(), def_stru.get_head(), Values.get_head()) == GS_BAD)
      { pCls->set_share_use(); return GS_BAD; }
   // sblocco classe
   if (pCls->set_share_use() == GS_BAD) return GS_BAD;
   
   return GS_GOOD;
}   


/*****************************************************************************/
/*.doc C_SECONDARY::get_default_values                                                                      */
/*+
   Questa funzione restituisce una lista di resbuf così costruita
   ((<nome colonna><valore di default>) ... )
   Parametri:
   C_RB_LIST &ColValues;

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int C_SECONDARY::get_default_values(C_RB_LIST &ColValues)
{
   C_ATTRIB_LIST *p_attrib_list;
   
   // Ritorna il puntatore alla ATTRIBLIST della classe
   if ((p_attrib_list = ptr_attrib_list()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return NULL; }
  
   if (p_attrib_list->get_StaticDefValues(ColValues) == GS_BAD) return GS_BAD;
   
   // Ciclo per calcolo eventuali valori di default calcolati
   // validazione e ricalcolo dati
   if (p_attrib_list->is_DefCalculated() == GS_GOOD)
      if (CalcValidData(ColValues, NONE) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}                  


/****************************************************/
/*.doc GetNewSecCode                                */
/*+
   Restituisce un nuovo identificatore
-*/  
/****************************************************/
int C_SECONDARY::GetNewSecCode()
{
   if (ptr_info() == NULL) { GS_ERR_COD = eGSInvClassType; return 0; }
   // ricavo nuovo key_attrib (codici progressivi negativi)
   return -1 * (abs(ptr_info()->TempLastId) + 1);
}


/*******************************************************/
/*.doc C_SECONDARY::CalcValidData                      */
/*+
   Valida i dati se devono essere validati, 
   calcola i dati se devono essere calcolati.
      
   Parametri
   input: C_RB_LIST &ColValues  buffer da cui ricavare i valori 
                                per il record, avente struttura
                                ((<nome colonna><valore di default>) ... )
   int Op;                  Operazione attuale (NONE, INSERT, MODIFY; default = MODIFY,
                            se = NONE l'operazione riguarda altre situazioni,
                            es. calcolo dei valori di default)

                                                        
   Restituisce GS_GOOD in caso di successo altrimenti 
   restituisce GS_BAD.
-*/  
/********************************************************/
int C_SECONDARY::CalcValidData(C_RB_LIST &ColValues, int Op)
{
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();

   if (p_attrib_list->init_ADOType(ptr_info()->getDBConnection(OLD)) == NULL)
      return GS_BAD;

   // inizializzo puntatore globale alla classe per calcolo da grafica
   GS_CALC_CLASS = NULL;
   // ricalcolo (con controllo valore dati per gli attributi calcolati)
   if (p_attrib_list->calc_all(ColValues, Op) == GS_BAD) return GS_BAD;
   // controllo di tutti i valori
   if (p_attrib_list->CheckValues(ColValues) == GS_BAD) return GS_BAD;
   // validazione dei dati
   if (p_attrib_list->validate_all(ColValues) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::reindex <internal> */
/*+
  Questa funzione reindicizza le tabelle della secondaria.
  Parametri:
  bool OnlyTemp;        Flag, se true indica che devono essere considerate solo
                        le tabelle temporanee (default = false)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::reindex(bool OnlyTemp)
{
   C_PROJECT      *pFatherPrj = (C_PROJECT *) pCls->ptr_id()->pPrj;
   C_DBCONNECTION *pOldConn, *pTempConn;
   C_STRING       Tabella;
   int            result;

   // Se secondaria esterna non si può reindicizzare
   if (type == GSExternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }

   if (!OnlyTemp)
   {
      // blocco la classe in modo esclusivo GS_CLASS usando come semaforo
      // il codice della sessione attiva, se esistente, altrimenti il codice utente 
      long OwnerCode = (GS_CURRENT_WRK_SESSION) ? GS_CURRENT_WRK_SESSION->get_id() : GEOsimAppl::GS_USER.code * -1;
      if (pCls->set_exclusive_use(OwnerCode, &result) == GS_BAD || result == GS_BAD)
         return GS_BAD;

      // Ricavo la connessione OLD-DB per le tabelle OLD
      if ((pOldConn = info.getDBConnection(OLD)) == NULL)
         { pCls->set_share_use(); return GS_BAD; }
      // Eseguo la reindicizzazione dell' OLD della secondaria
      if (pOldConn->Reindex(info.OldTableRef.get_name()) == GS_BAD)
         { pCls->set_share_use(); return GS_BAD; }
      // Ricavo il riferimento completo alla tabella OLD dei link della secondaria
      if (getOldLnkTableRef(Tabella) == GS_BAD)
         { pCls->set_share_use(); return GS_BAD; }
      // Eseguo la reindicizzazione
      if (pOldConn->Reindex(Tabella.get_name()) == GS_BAD)
         { pCls->set_share_use(); return GS_BAD; }
      // aggiorno ENT_LAST in GS_SEC
      if (ptr_info()->RefreshOldLastId() == GS_GOOD)
      {
         C_RB_LIST pSec;

         if ((pSec << acutBuildList(RTLB,
                                        RTLB, RTSTR, _T("LAST_ENT"), RTLONG, info.OldLastId, RTLE,
                                    RTLE, 0)) == NULL)
            { pCls->set_share_use(); return GS_BAD; }

         // Modifico riga in GS_SEC
         if (mod_sec_to_gs_sec(pSec) == GS_BAD)
            { pCls->set_share_use(); return GS_BAD; }
      }

      // sblocco classe
      if (pCls->set_share_use() == GS_BAD) return GS_BAD;
   }

   // Se sono in una sessione di lavoro allora reindicizzo anche i files temporanei
   if (GS_CURRENT_WRK_SESSION && 
       (GS_CURRENT_WRK_SESSION->get_status() == WRK_SESSION_ACTIVE || GS_CURRENT_WRK_SESSION->get_status() == WRK_SESSION_SAVE) &&
       GS_CURRENT_WRK_SESSION->get_PrjId() == pFatherPrj->get_key())
   { 
      // Ricavo la connessione OLD-DB per le tabelle OLD
      if ((pTempConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
      // Ricavo il riferimento completo alla tabella TEMP della secondaria
      // Ma non devo crearla se non esiste
      if (getTempTableRef(Tabella, GS_BAD) == GS_BAD) return GS_BAD;
      // Se esiste allora la reindicizzo
      if (pTempConn->ExistTable(Tabella) == GS_GOOD)
         pTempConn->Reindex(Tabella.get_name());

      // Ricavo il riferimento completo alla tabella TEMP dei link della secondaria
      if (getTempLnkTableRef(Tabella) == GS_BAD) return GS_BAD;
      // Se esiste allora la reindicizzo
      if (pTempConn->ExistTable(Tabella) == GS_GOOD)
         pTempConn->Reindex(Tabella.get_name());

      // Ricavo il riferimento completo alla tabella TEMP di support della secondaria
      if (getSupportTableRef(Tabella) == GS_BAD) return GS_BAD;
      // Se esiste allora la reindicizzo
      if (pTempConn->ExistTable(Tabella) == GS_GOOD)
         pTempConn->Reindex(Tabella.get_name());

      // Ricavo il riferimento completo alla tabella TEMP di support dei link della secondaria
      if (getSupportLnkTableRef(Tabella) == GS_BAD) return GS_BAD;
      // Se esiste allora la reindicizzo
      if (pTempConn->ExistTable(Tabella) == GS_GOOD)
         pTempConn->Reindex(Tabella.get_name());
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::RefreshTempLastId         <internal> */
/*+
  Questa funzione aggiorna il membro TempLastId della secondaria interna.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::RefreshTempLastId(void)
{
   C_STRING       TempTableRef;
   C_DBCONNECTION *pConn;
   double         Value;

   if (ptr_info() == NULL) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // ricavo connessione OLE-DB per tabella TEMP
   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   // senza creare la tabella
   if (getTempTableRef(TempTableRef, GS_BAD) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TempTableRef) == GS_GOOD)
   // ricavo il codice minore
   if (pConn->GetNumAggrValue(TempTableRef.get_name(),
                              ptr_info()->key_attrib.get_name(),
                              _T("MIN"), &Value) == GS_BAD) return GS_BAD;
   ptr_info()->TempLastId = (long) Value;

   return GS_GOOD;
}

    
/*********************************************************/
/*.doc (new 2) C_SECONDARY::CheckValidFuncOnData <internal> */
/*+
  Questa funzione verifica le funzioni di validità dei dati della tabella
  secondaria prendendole da una struttura passata come parametro. Verifica che
  eventuali dati già presenti soddisfino le funzioni.
  Parametri:
  C_ATTRIB_LIST *p_attrib_list;  lista attributi da cui leggere le funzioni
                                 di validità

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::CheckValidFuncOnData(C_ATTRIB_LIST *p_attrib_list)
{
   C_DBCONNECTION   *pConn;
   int               Valid;

   // Ricavo la connessione OLD-DB per le tabelle OLD
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   
   // carico eventuali funzioni di calcolo in GSL.GSL del progetto della classe
   gsc_load_gsl(((C_PROJECT *) pCls->ptr_id()->pPrj)->get_dir());

   Valid = gsc_CheckValidFuncOnData(pConn, ptr_info()->OldTableRef.get_name(), 
                                    p_attrib_list, ptr_info()->key_attrib.get_name());

	// ricarico GSL.GSL giusto
	gs_gsl_reload();
   
   return Valid;
}

    
/*********************************************************/
/*.doc (new 2) C_SECONDARY::ChangeCalcFuncOnData <internal> */
/*+
  Questa funzione ricalcola i campi calcolati con le funzioni di calcolo
  prendendole da una struttura passata come parametro.
  Parametri:
  C_ATTRIB_LIST *p_attrib_list;  lista attributi da cui leggere le funzioni
                                 di validità

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::ChangeCalcFuncOnData(C_ATTRIB_LIST *p_attrib_list)
{
   C_DBCONNECTION *pConn;
	int            res;
   
   if (type == GSExternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }
   if (!p_attrib_list) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // Ricavo la connessione OLD-DB per le tabelle OLD
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;

   // carico eventuali funzioni di calcolo in GSL.GSL del progetto della classe
   gsc_load_gsl(((C_PROJECT *) pCls->ptr_id()->pPrj)->get_dir());

   res = gsc_ChangeCalcFuncOnData(pConn, ptr_info()->OldTableRef.get_name(),
                                  p_attrib_list, ptr_info()->key_attrib.get_name());

	// ricarico GSL.GSL giusto
	gs_gsl_reload();

   return res;
}

    
/*********************************************************/
/*.doc (new 2) C_SECONDARY::CheckMandatoryOnData <internal> */
/*+
  Questa funzione verifica che i campi con il flag di obbligatorietà
  attivo non abbiano valore vuoto.
  Parametri:
  C_ATTRIB_LIST *p_attrib_list;  lista attributi da cui leggere gli
                                 attributi obbligatori
  bool           AssignDefault;  Flag, se = TRUE se viene incontrato un 
                                 attributo che non soddisfa l'obbligatorietà 
                                 viene settato il valore di default
                                 (default = FALSE)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::CheckMandatoryOnData(C_ATTRIB_LIST *p_attrib_list, bool AssignDefault)
{
   C_DBCONNECTION *pConn;
   int            Mandatory;
   
   if (type == GSExternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }
   if (!p_attrib_list) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   
   // Ricavo la connessione OLD-DB per le tabelle OLD
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;

   Mandatory = gsc_CheckMandatoryOnData(pConn, ptr_info()->OldTableRef.get_name(), 
                                        p_attrib_list, AssignDefault);

   return Mandatory;
}


/*********************************************************/
/*.doc C_SECONDARY::read_stru <internal> */
/*+
  Questa funzione carica la struttura della tabella secondaria
  di GEOsim leggendola da file.
  Parametri:
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  C_CLASS *pCls;  puntatore alla classe madre
  int sec;        codice tabella secondaria
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::read_stru(C_PROFILE_SECTION_BTREE &ProfileSections, C_CLASS *pCls, int sec)
{
   TCHAR      sez[SEZ_PROFILE_LEN];
   C_STRING   dir_file, drive;
   C_ID       *p_id = pCls->ptr_id();
   C_INFO_SEC *p_info_sec = ptr_info();
   C_INFO     *p_info;
   C_RB_LIST  attr_def_list;
   int        Result;

   if ((p_info = pCls->ptr_info()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   swprintf(sez, SEZ_PROFILE_LEN, _T("%d.%d"), p_id->sub_code, sec);
   if ((Result = load_id(ProfileSections, sez)) != GS_GOOD) return Result;
   type = GSInternalSecondaryTable; // tipo secondaria GEOsim
   if (info.load(ProfileSections, sez) != GS_GOOD) return GS_BAD;

   p_info_sec->ConnStrUDLFile = p_info->ConnStrUDLFile; 
   p_info->UDLProperties.copy(p_info_sec->UDLProperties);
   
   p_info_sec->OldTableRef = _T("S");
   if (p_id->sub_code != 0) // sottoclasse di una simulazione
   {
      p_info_sec->OldTableRef += p_id->sub_code;
      p_info_sec->OldTableRef += _T('_');
      p_info_sec->OldTableRef += sec;
   }
   else
      p_info_sec->OldTableRef += sec;

   p_info_sec->key_pri = p_info->key_attrib; // attributo chiave della classe

   swprintf(sez, SEZ_PROFILE_LEN, _T("%d.%d%s"), p_id->sub_code, sec, ATTRIB_PROFILE);
   if ((attr_def_list << read_attrib(ProfileSections, sez)) == NULL)
      return GS_BAD;
   if (attrib_list.from_rb(attr_def_list.get_head()) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::read_attrib <internal> */
/*+
  Questa funzione carica la struttura della tabella di una
  tabella secondaria leggendola da file.
  Parametri:
  C_PROFILE_SECTION_BTREE &ProfileSections; File di profilo letto in memoria
  const TCHAR *sez;   nome della sezione

  Restituisce una lista di resbuf in caso di successo 
  altrimenti restituisce NULL.
-*/  
/*********************************************************/
presbuf C_SECONDARY::read_attrib(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_DBCONNECTION *pConn;   
   C_ATTRIB_LIST  attrib_list;
   C_RB_LIST      list;
   
   // Ricavo la connessione OLD-DB per le tabelle OLD
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;

   // l'ultimo paramtero = GS_GOOD per avvisare che si sta creando una tabella esterna
   if (attrib_list.load(ProfileSections, sez, pConn) == GS_BAD) return NULL;

   if ((list << acutBuildList(RTLB, 0)) == NULL) return NULL;
   list += attrib_list.to_rb();
   if ((list += acutBuildList(RTLE, 0)) == NULL) return NULL;
   list.ReleaseAllAtDistruction(GS_BAD);

   return list.get_head();
}


/*********************************************************/
/*.doc C_SECONDARY::CheckValue                <internal> */
/*+
  Questa funzione controlla la correttezza del valore
  verificando lunghezza, decimali, tipo, obbligatorietà. 
  Il valore viene modificato con quello corretto.
  Parametri:
  C_ATTRIB *pAttrib;       Puntatore all'attributo 
  presbuf   Value;      Nuovo valore dell'attributo

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::CheckValue(C_ATTRIB *pAttrib, presbuf Value)
{
   if (!ptr_attrib_list() || !ptr_info()) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }

   if (ptr_attrib_list()->init_ADOType(ptr_info()->getDBConnection(OLD)) == GS_BAD)
      return GS_BAD;

   return pAttrib->CheckValue(Value);
}


/*********************************************************/
/*  FINE   FUNZIONI DI C_SECONDARY                       */
/*********************************************************/


/************************************************************/
/*.doc gsc_checkValue                           < external> */
/*+
  Questa funzione controlla un valore letto verificando le sue 
  caratteristiche rispetto ad un attibuto, di una lista Resbuf
  di attributi e di un Drv.
  Parametri:
  C_SECONDARY  *pSec;      puntatore a secondaria GEOsim
  presbuf      pRbAttrib   puntatore a resbuf dell'attributo corrente
  C_ATTRIB     *pAttrib    Attributo di riferimento
  TCHAR        *Value      Valore da verificare.

  Restituisce in caso di errore il messaggio da stampare, altrimenti NULL.
  N.B. Alloca memoria.
-*/  
/************************************************************/
TCHAR *gsc_checkValue(C_SECONDARY *pSec, presbuf pRbAttrib, C_ATTRIB *pAttrib, TCHAR *Value)
{
   C_DBCONNECTION *pOldConn;
   presbuf        pRB;

   if (!pSec->ptr_attrib_list() || !pSec->ptr_info()) { GS_ERR_COD = eGSInvSecType; return NULL; }

   if ((pOldConn = pSec->ptr_info()->getDBConnection(OLD)) == NULL) return NULL;

   if (pSec->ptr_attrib_list()->init_ADOType(pOldConn) == GS_BAD) return NULL;

   // converto valore in resbuf
   if ((pRB = pOldConn->ptr_DataTypeList()->DataValueStr2Rb(pAttrib->type, Value)) == NULL)
      return NULL;

   if (pAttrib->CheckValue(pRB) == GS_BAD)
   {
      switch (GS_ERR_COD)
      {
         case eGSInvAttribLen:
            return gsc_tostring(gsc_msg(222));  // *Errore* Lunghezza attributo non valida.
         case eGSInvAttribType:
            return gsc_tostring(gsc_msg(223));  // *Errore* Tipo attributo non valido.
         case eGSObbligFound:
            return gsc_tostring(gsc_msg(78));   // *Errore* Obbligatorità attributo non soddisfatta.
         default:
            return gsc_tostring(gsc_msg(224));  // *Errore* Valore attributo non valido.
      }
   }

   if (pRbAttrib->rbnext == NULL || 
       gsc_sostitutebuf(pRB, pRbAttrib->rbnext) == GS_BAD)
   {
      acutRelRb(pRB);
      return gsc_tostring(gsc_msg(225));        // *Errore* Assegnazione fallita.
   }

   acutRelRb(pRB);
   
   return NULL;
}


/*********************************************************/
/*.doc C_SECONDARY::mod_stru                  <external> */
/*+
  Questa funzione modifica i valori della C_ATTRIB_LIST 
  di una tabella secondaria.
    
  Restituisce GS_GOOD in caso di successo, GS_CAN se 
  l'operazione viene abortita altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::mod_stru(void)
{
   C_RB_LIST      new_stru, link, new_defvalues_list;
   C_ATTRIB_LIST  attrib_list, *p_attrib_list = ptr_attrib_list(), OldStruct;
   C_INFO_SEC     *p_info = ptr_info();
   int            stru, charact, def, dummy, calc, valid, mand, result = GS_BAD;
   long           SessionId;
   C_DBCONNECTION *pConn;

   // Verifico abilitazione
   if (gsc_check_op(opModSec) == GS_BAD) return GS_BAD;

   // vedi gsc_alloc_mod_stru
   if (GS_RESBUF == NULL) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }
   
   // Ricavo la struttura della nuova tabella
   if ((new_stru << gsc_nthcopy(1, GS_RESBUF)) == NULL)
      { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }
   if (new_stru.GetCount() <= 0) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }
                                                         
   // Ricavo la lista di link della nuova tabella
   if ((link << gsc_nthcopy(2, GS_RESBUF)) == NULL)
      { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }
   if (link.GetCount() <= 0)
      { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   // Verifico se la classe fa parte di una sessione di lavoro
   if (pCls->is_inarea(&SessionId) == GS_BAD || SessionId > 0)
      { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

   // copia di sicurezza
   p_attrib_list->copy(&OldStruct);

   if (attrib_list.from_rb(new_stru.get_head()) == GS_BAD) return GS_BAD;

   // Ricavo connessione OLE-DB
   if ((pConn = p_info->getDBConnection(OLD)) == NULL) return GS_BAD;

   // carico eventuali funzioni di calcolo in GSL.GSL del progetto della classe
   gsc_load_gsl(pCls->get_pPrj()->get_dir());

   // Verifico l' Attrib_list
   if (attrib_list.is_valid(pConn) == GS_BAD) { gs_gsl_reload(); return GS_BAD; }
   if (attrib_list.is_valid(CAT_SECONDARY, type, pConn, p_info->key_attrib) == GS_BAD)
		{ gs_gsl_reload(); return GS_BAD; }

	// ricarico GSL.GSL giusto
	gs_gsl_reload();

   // Paolino
   // if (get_default_values(new_defvalues_list) == GS_BAD) return GS_BAD;
   if (attrib_list.get_StaticDefValues(new_defvalues_list) == GS_BAD) return GS_BAD;
   if (attrib_list.init_ADOType(pConn) == GS_BAD) return GS_BAD;

   if (attrib_list.CheckValues(new_defvalues_list) == GS_BAD) return GS_BAD;
   // Setto i valori di default corretti
   if (attrib_list.set_default_values(new_defvalues_list) == GS_BAD) return GS_BAD;

   // Verifico cosa è cambiato 
   if (p_attrib_list->WhatIsChanged(&attrib_list, link.get_head(),
                                    &stru, &charact, &def, &dummy,
                                    &calc, &valid, &mand) == GS_BAD) return GS_BAD;

   // Faccio un Backup di sicurezza della secondaria
   if (Backup(GSCreateBackUp) == GS_BAD) return GS_BAD;

   do
   {     
      // è stata modificata la struttura della tabella
      if (stru == GS_GOOD)
      {
         // modifico struttura tabella secondaria
         if (modi_tab(&attrib_list, link.get_head()) == GS_BAD) break;
      }
      // è stata modificata almeno una caratteristica degli attributi della tabella
      if (stru == GS_GOOD || charact == GS_GOOD ||
          calc == GS_GOOD || valid == GS_GOOD || mand == GS_GOOD)
      {
         // modifico caratteristiche degli attributi
         if (carattattr_to_db(&attrib_list) == GS_BAD) break;
      }
      // è stata modificato almeno un valore di default degli attributi della tabella
      if (def == GS_GOOD)
      {
	      if (default_to_db(&attrib_list) == GS_BAD) break;
      }
      // è stata modificata la funzione di calcolo di almeno un attributo della tabella
      if (calc == GS_GOOD)
      { // ricalcolo campi tabella
         if (ChangeCalcFuncOnData(&attrib_list) == GS_BAD)
         {
            acutPrintf(gsc_msg(306)); // "\nAlcune schede non soddisfano la funzione di calcolo."
            break;
         }
      }
      // è stata modificata la funzione di validità di almeno un attributo della tabella
      if (valid == GS_GOOD)
      { // verifico validità
         if (CheckValidFuncOnData(&attrib_list) == GS_BAD)
         {
            acutPrintf(gsc_msg(305)); // "\nAlcune schede non soddisfano la funzione di validità."
            break;
         }
      }
      // è stata modificata l'obbligatorietà di almeno un attributo della tabella
      if (mand == GS_GOOD)
      { // verifico obbligatorietà
         if (CheckMandatoryOnData(&attrib_list) == GS_BAD)
         {
            int AssignDefault = GS_BAD;

            // "Alcune schede non soddisfano la regola di obbligatorietà, assegnare il valore di default ?"
            if (gsc_ddgetconfirm(gsc_msg(307), &AssignDefault) == GS_BAD ||
                AssignDefault == GS_BAD)
               break;
            // Assegno i valori di default agli attributi obblicgatori con valore nullo
            if (CheckMandatoryOnData(&attrib_list, TRUE) == GS_BAD) break;
         }
      }

      // devo settare la nuova struttura e i valori di default corretti
      if (p_attrib_list->from_rb(new_stru.get_head()) == GS_BAD) break;
      if (p_attrib_list->set_default_values(new_defvalues_list) == GS_BAD) break;

      result = GS_GOOD;
   }
   while (0);

   switch (result)
   {
      case GS_GOOD:
      {
         // se esiste un file SQL che deve essere eseguito
         C_STRING SQLFile;
         SQLFile = GEOsimAppl::GEODIR; // Directory server di GEOsim
         SQLFile += _T('\\');
         SQLFile += GEOCUSTOMDIR;
         SQLFile += _T("\\PRJ");
         SQLFile += pCls->get_PrjId();
         SQLFile += _T("_CLS");
         SQLFile += pCls->ptr_id()->code;
         if (pCls->ptr_id()->sub_code > 0)
         {
            SQLFile += _T("_SUB");
            SQLFile += pCls->ptr_id()->sub_code;
         }
         SQLFile += _T("_SEC");
         SQLFile += code;
         SQLFile += _T(".SQL");
         if (gsc_path_exist(SQLFile) == GS_GOOD)
            gsc_ExeCmdFromFile(SQLFile);

         // Se andato tutto bene allora rilascio strutture e cancello il Backup
         if (gsc_dealloc_GS_RESBUF() == GS_BAD) return GS_BAD;
         // Eseguo il commit perchè è andato tutto OK
         // pConn->CommitTrans();
         // Cancello il Backup
         if (Backup(GSRemoveBackUp) == GS_BAD) return GS_BAD;

         break;
      }
      case GS_CAN:
      case GS_BAD:
      {
         int error_code = GS_ERR_COD;

         // Se c'è stato un malfunzionamento eseguo il RollbackTrans e
         // ripristino il backup.
         if (Backup(GSRestoreBackUp) == GS_BAD) return GS_BAD;

         Backup(GSRemoveBackUp); // cancello il backup
         OldStruct.copy(p_attrib_list); // rispristino immagine in memoria

         GS_ERR_COD = error_code;
         break;
      }
   }
   if (p_attrib_list->init_ADOType(pConn) == GS_BAD) return GS_BAD;

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(pCls->get_PrjId());
 
   return result;
}


/*****************************************************************************/
/*.doc gsc_setTileDclSecValueTransfer                                        */
/*+
  Questa funzione setta tutti i controlli della DCL "SecValueTransfer" in modo 
  appropriato secondo il tipo di selezione.
  Parametri:
  ads_hdlg dcl_id;     
  Common_Dcl_SecValueTransfer_Struct *CommonStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_setTileDclSecValueTransfer(ads_hdlg dcl_id, 
                                          Common_Dcl_SecValueTransfer_Struct *CommonStruct)
{
   C_STRING        JoinOp, Boundary, SelType, LocSummary, Trad;
   C_RB_LIST       CoordList;
   bool            Inverse;
   C_SECONDARY     *pSec;
   C_CLASS         *pClass;
   C_ATTRIB_LIST   *pAttrList;
   C_ATTRIB        *pAttr;
   TCHAR           StrPos[4], *key_attrib;
   C_SINTH_SEC_TAB *pSinthSec;
   C_CLASS_SQL     *pClsSQL;
   C_SEC_SQL       *pSecSQL;
   int             i;

   // scompongo la condizione impostata
   if (gsc_SplitSpatialSel(CommonStruct->SpatialSel, JoinOp, &Inverse,
                           Boundary, SelType, CoordList) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   // Traduzioni per SelType
   if (gsc_trad_SelType(SelType, Trad) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   LocSummary = Trad;
   LocSummary += _T(" ");
   // Traduzioni per Boundary
   if (gsc_trad_Boundary(Boundary, Trad) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   LocSummary += Trad;
   ads_set_tile(dcl_id, _T("Location_Descr"), LocSummary.get_name());

   // Location use
   if (CommonStruct->LocationUse)
   {
      ads_set_tile(dcl_id, _T("LocationUse"), _T("1"));
      ads_mode_tile(dcl_id, _T("Location"), MODE_ENABLE); 
   }
   else
   {
      ads_set_tile(dcl_id, _T("LocationUse"), _T("0"));
      ads_mode_tile(dcl_id, _T("Location"), MODE_DISABLE); 
   }

   // logical operator
   switch (CommonStruct->Logical_op)
   {
      case UNION:
         ads_set_tile(dcl_id, _T("Logical_op"), _T("Union"));
         break;
      case SUBTRACT_A_B:
         ads_set_tile(dcl_id, _T("Logical_op"), _T("Subtr_Location_SSfilter"));
         break;
      case SUBTRACT_B_A:
         ads_set_tile(dcl_id, _T("Logical_op"), _T("Subtr_SSfilter_Location"));
         break;
      case INTERSECT:
         ads_set_tile(dcl_id, _T("Logical_op"), _T("Intersect"));
         break;
   }

   // LastFilterResult use
   if (CommonStruct->LastFilterResultUse)
      ads_set_tile(dcl_id,  _T("SSFilterUse"),  _T("1"));
   else
      ads_set_tile(dcl_id,  _T("SSFilterUse"),  _T("0"));

   // controllo su operatori logici (abilitato solo se è stata scelta una area 
   // tramite "Location" ed è stato scelto anche l'uso di LastFilterResult)
   if (CommonStruct->LocationUse && Boundary.len() > 0 && CommonStruct->LastFilterResultUse)
      ads_mode_tile(dcl_id,  _T("Logical_op"), MODE_ENABLE); 
   else
      ads_mode_tile(dcl_id,  _T("Logical_op"), MODE_DISABLE); 

   // Nome Tabella Secondaria
   pSinthSec = (C_SINTH_SEC_TAB *) CommonStruct->SinthSecList.get_head();
   ads_start_list(dcl_id,  _T("SecTable"), LIST_NEW, 0);
   while (pSinthSec)
   {
      gsc_add_list(pSinthSec->get_name());
      pSinthSec = (C_SINTH_SEC_TAB *) CommonStruct->SinthSecList.get_next();
   }
   ads_end_list();
   swprintf(StrPos, 4, _T("%d"), CommonStruct->SinthSecList.getpos_key(CommonStruct->Sec) - 1);
   ads_set_tile(dcl_id, _T("SecTable"), StrPos);

   // check uso funzione di aggregazione
   if (CommonStruct->AggrUse == FALSE)
   {
      ads_set_tile(dcl_id, _T("Aggr"), _T("0"));
      ads_mode_tile(dcl_id, _T("aggrSQL"), MODE_DISABLE);
   }
   else
   {
      ads_set_tile(dcl_id, _T("Aggr"), _T("1"));
      ads_mode_tile(dcl_id, _T("aggrSQL"), MODE_ENABLE);
   }

   // list box delle funzioni di aggregazione
   swprintf(StrPos, 4, GS_EMPTYSTR);
   ads_start_list(dcl_id, _T("aggrSQL"), LIST_NEW, 0);
   for (i = 0; i < ELEMENTS(AGGR_FUN_LIST); i++)
   {
      gsc_add_list(AGGR_FUN_LIST[i]);
      if (gsc_strcmp(CommonStruct->AggrFun.get_name(), AGGR_FUN_LIST[i], FALSE) == 0)
          swprintf(StrPos, 4, _T("%d"), i);
   }   
   ads_end_list();
   if (wcslen(StrPos) > 0) ads_set_tile(dcl_id , _T("aggrSQL"), StrPos);

   // Attributo Sorgente della Tabella Secondaria
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls, CommonStruct->Sub,
                            CommonStruct->Sec)) == NULL)
      return GS_BAD;
   pAttrList = pSec->ptr_attrib_list();
   pAttr = (C_ATTRIB *) pAttrList->get_head();
   ads_start_list(dcl_id, _T("SecAttr"), LIST_NEW, 0);
   while (pAttr)
   {
      gsc_add_list(pAttr->get_name());
      pAttr = (C_ATTRIB *) pAttrList->get_next();
   }
   ads_end_list();
 
   swprintf(StrPos, 4, _T("%d"), pAttrList->getpos_name(CommonStruct->Source.get_name()) - 1);
   ads_set_tile(dcl_id, _T("SecAttr"), StrPos);

   // Condizione SQL Secondaria
   if ((pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.search(CommonStruct->Cls,
                                                                CommonStruct->Sub)) == NULL)
      return GS_BAD;
   if ((pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->search_type(CommonStruct->Sec)) == NULL)
      return GS_BAD;
   if (pSecSQL->get_sql()) ads_set_tile(dcl_id, _T("editsql"), pSecSQL->get_sql());
   else ads_set_tile(dcl_id, _T("editsql"), GS_EMPTYSTR);

   // Attributo Destinazione della Tabella principale
   if ((pClass = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls, 
                                CommonStruct->Sub)) == NULL)
      return GS_BAD;
   
   key_attrib = pClass->ptr_info()->key_attrib.get_name();
   pAttrList = pClass->ptr_attrib_list();
   pAttr = (C_ATTRIB *) pAttrList->get_head();
   ads_start_list(dcl_id, _T("ClassAttr"), LIST_NEW, 0);
   swprintf(StrPos, 4, GS_EMPTYSTR);
   i = 0;
   while (pAttr)
   {
      // scarto l'attributo chiave e attributi calcolati
      if (gsc_strcmp(key_attrib, pAttr->get_name(), FALSE) != 0 &&
          pAttr->is_calculated() == GS_BAD)
      {
         gsc_add_list(pAttr->get_name());
         if (CommonStruct->Dest.comp(pAttr->get_name(), FALSE) == 0)
            swprintf(StrPos, 4, _T("%d"), i);
         else i++;
      }
      pAttr = (C_ATTRIB *) pAttr->get_next();
   }
   ads_end_list();
   if (wcslen(StrPos) > 0) ads_set_tile(dcl_id, _T("ClassAttr"), StrPos);

   return GS_GOOD;
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" e "DclDynSegmentationAnalyzer" su controllo "Location"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SpatialSel(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, LOCATION_SEL);         
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "LocationUse"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_LocationUse(ads_callback_packet *dcl)
{
   TCHAR val[10];

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   if (ads_get_tile(dcl->dialog, _T("LocationUse"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      ((Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data))->LocationUse = true;
   else
      ((Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data))->LocationUse = false;

   gsc_setTileDclSecValueTransfer(dcl->dialog, 
                                  (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "Logical_op"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_Logical_op(ads_callback_packet *dcl)
{
   TCHAR val[50];

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   if (ads_get_tile(dcl->dialog, _T("Logical_op"), val, 50) == RTNORM)
   {
      // case insensitive
      if (gsc_strcmp(val, _T("Union")) == 0)
         ((Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data))->Logical_op = UNION;
      else
      if (gsc_strcmp(val, _T("Subtr_Location_SSfilter")) == 0)
         ((Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data))->Logical_op = SUBTRACT_A_B;
      else
      if (gsc_strcmp(val, _T("Subtr_SSfilter_Location")) == 0)
         ((Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data))->Logical_op = SUBTRACT_B_A;
      else
      if (gsc_strcmp(val, _T("Intersect")) == 0)
         ((Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data))->Logical_op = INTERSECT;
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "LastFilterResultUse"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_LastFilterResultUse(ads_callback_packet *dcl)
{
   TCHAR val[10];

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   if (ads_get_tile(dcl->dialog, _T("SSFilterUse"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      ((Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data))->LastFilterResultUse = true;
   else
      ((Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data))->LastFilterResultUse = false;

   gsc_setTileDclSecValueTransfer(dcl->dialog, 
                                  (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "SecTable"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_SecTable(ads_callback_packet *dcl)
{
   TCHAR           val[10];
   C_SINTH_SEC_TAB *pSinthSec;
   C_SECONDARY     *pSec;
   C_ATTRIB        *pAttr;
   C_CLASS_SQL     *pClsSQL;
   C_SEC_SQL       *pSecSQL;
   Common_Dcl_SecValueTransfer_Struct *CommonStruct;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data);
   if (ads_get_tile(dcl->dialog, _T("SecTable"), val, 10) != RTNORM) return;
   
   if ((pSinthSec = (C_SINTH_SEC_TAB *) CommonStruct->SinthSecList.getptr_at(_wtoi(val) + 1)) == NULL)
      { GS_ERR_COD = eGSNoSecondary; return; }
   CommonStruct->Sec = pSinthSec->get_key();

   // Attributo Sorgente della Tabella Secondaria
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls, CommonStruct->Sub,
                            CommonStruct->Sec)) == NULL)
      return;
   pAttr = (C_ATTRIB *) pSec->ptr_attrib_list()->get_head();
   CommonStruct->Source = pAttr->get_name();

   // Condizione SQL Secondaria
   if ((pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.search(CommonStruct->Cls,
                                                                CommonStruct->Sub)) == NULL)
      return;
   if ((pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->search_type(CommonStruct->Sec)) == NULL)
      return;
   if (pSecSQL->set_sql(GS_EMPTYSTR) == GS_BAD) return;

   gsc_setTileDclSecValueTransfer(dcl->dialog, 
                                  (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "Aggr"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_Aggr(ads_callback_packet *dcl)
{
   TCHAR                              val[10];
   Common_Dcl_SecValueTransfer_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data);
   if (ads_get_tile(dcl->dialog, _T("Aggr"), val, 10) != RTNORM) return;

   if (gsc_strcmp(val, _T("0")) == 0)
   {
      ads_mode_tile(dcl->dialog, _T("aggrSQL"), MODE_DISABLE);
      CommonStruct->AggrUse = FALSE;
   }
   else
   {
      ads_mode_tile(dcl->dialog, _T("aggrSQL"), MODE_ENABLE);
      CommonStruct->AggrUse = TRUE;
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "aggrSQL"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_aggrSQL(ads_callback_packet *dcl)
{
   Common_Dcl_SecValueTransfer_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data);
   CommonStruct->AggrFun = AGGR_FUN_LIST[_wtoi(dcl->value)];
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "SecAttr"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_SecAttr(ads_callback_packet *dcl)
{
   TCHAR         val[10];
   C_SECONDARY   *pSec;
   C_ATTRIB      *pAttr;
   Common_Dcl_SecValueTransfer_Struct *CommonStruct;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data);
   if (ads_get_tile(dcl->dialog, _T("SecAttr"), val, 10) != RTNORM) return;
   
   // Attributo Sorgente della Tabella Secondaria
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls, 
                            CommonStruct->Sub, CommonStruct->Sec)) == NULL)
      return;

   pAttr = (C_ATTRIB *) pSec->ptr_attrib_list()->getptr_at(_wtoi(val) + 1);
   CommonStruct->Source = pAttr->get_name();
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "SecSQL"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_SecSQL(ads_callback_packet *dcl)
{
   Common_Dcl_SecValueTransfer_Struct *CommonStruct;
   C_SECONDARY      *pSec;
   C_INFO_SEC       *pInfo;
   C_STRING          SQLCond, str;
   C_CLASS_SQL      *pClsSQL;
   C_SEC_SQL        *pSecSQL;
   C_DBCONNECTION   *pConn;

   if (!GS_CURRENT_WRK_SESSION) return;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data);

   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls,
                            CommonStruct->Sub, CommonStruct->Sec)) == NULL) return;

   if ((pInfo = pSec->ptr_info()) == NULL) return;
   // Condizione SQL Secondaria
   if ((pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.search(CommonStruct->Cls,
                                                                CommonStruct->Sub)) == NULL)
      return;
   if ((pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->search_type(CommonStruct->Sec)) == NULL)
      return;
   SQLCond = pSecSQL->get_sql();
   
   str = gsc_msg(309); // "Tabella secondaria: "
   str += pSec->get_name();

   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return;

   if (gsc_ddBuildQry(str.get_name(), pConn, pInfo->OldTableRef, SQLCond,
                      GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls,
                      CommonStruct->Sub, CommonStruct->Sec) != GS_GOOD) return;
   if (pSecSQL->set_sql(SQLCond.get_name()) == GS_BAD) return;

   // Condizione SQL Secondaria
   if (SQLCond.get_name())
      ads_set_tile(dcl->dialog, _T("editsql"), SQLCond.get_name());
   else
      ads_set_tile(dcl->dialog, _T("editsql"), SQLCond.get_name());
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "editsql"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_editsql(ads_callback_packet *dcl)
{
   TCHAR       val[TILE_STR_LIMIT], *err;
   C_SECONDARY *pSec;
   C_INFO_SEC  *pInfo;
   C_CLASS_SQL *pClsSQL;
   C_SEC_SQL   *pSecSQL;
   Common_Dcl_SecValueTransfer_Struct *CommonStruct;
   C_DBCONNECTION   *pConn;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data);
   if (ads_get_tile(dcl->dialog, _T("editsql"), val, TILE_STR_LIMIT) != RTNORM) return;
   
   // Attributo Sorgente della Tabella Secondaria
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls,
                            CommonStruct->Sub, CommonStruct->Sec)) == NULL)
      return;
   if ((pInfo = pSec->ptr_info()) == NULL) return;

   // Ricavo la connessione OLE - DB all' OLD
   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return;

   if ((err = pConn->CheckSql(val)) != NULL)
   {  // ci sono errori
      ads_set_tile(dcl->dialog, _T("error"), err);
      free(err);
      return;
   }
   
   // Condizione SQL Secondaria
   if ((pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.search(CommonStruct->Cls,
                                                                CommonStruct->Sub)) == NULL)
      return;
   if ((pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->search_type(CommonStruct->Sec)) == NULL)
      return;
   if (pSecSQL->set_sql(val) == GS_BAD) return;

   // Condizione SQL Secondaria
   ads_set_tile(dcl->dialog, _T("editsql"), val);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "ClassAttr"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_ClassAttr(ads_callback_packet *dcl)
{
   TCHAR         val[10], *key_attrib;
   C_CLASS       *pClass;
   C_ATTRIB      *pAttr;
   int           i = 0;
   Common_Dcl_SecValueTransfer_Struct *CommonStruct;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data);
   if (ads_get_tile(dcl->dialog, _T("ClassAttr"), val, 10) != RTNORM) return;
   
   // Attributo destinazione della classe principale
   if ((pClass = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls, CommonStruct->Sub)) == NULL)
      return;

   key_attrib = pClass->ptr_info()->key_attrib.get_name();
   pAttr = (C_ATTRIB *) pClass->ptr_attrib_list()->get_head();
   while (pAttr)
   {
      // scarto l'attributo chiave e attributi calcolati
      if (gsc_strcmp(key_attrib, pAttr->get_name(), FALSE) != 0 &&
          pAttr->is_calculated() == GS_BAD)
         if (i == _wtoi(val)) break; 
         else i++;
      
      pAttr = (C_ATTRIB *) pAttr->get_next();
   }

   if (pAttr)
      CommonStruct->Dest = pAttr->get_name();
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "SecValueTransfer" su controllo "accept"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_SecValueTransfer_accept(ads_callback_packet *dcl)
{
   TCHAR         val[10];
   C_CLASS       *pClass;
   C_SECONDARY   *pSec;
   C_ATTRIB      *pAttrSource, *pAttrDest;
   C_CLASS_SQL   *pClsSQL;
   C_SEC_SQL     *pSecSQL;
   Common_Dcl_SecValueTransfer_Struct *CommonStruct;
   C_DBCONNECTION *pConn, *pClsConn;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_SecValueTransfer_Struct *) (dcl->client_data);

   if (ads_get_tile(dcl->dialog, _T("editsql"), val, 10) != RTNORM) return;

   // Attributo Sorgente della Tabella Secondaria
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls, CommonStruct->Sub,
                            CommonStruct->Sec)) == NULL)
      return;

   // Condizione SQL Secondaria
   if ((pClsSQL = (C_CLASS_SQL *) CommonStruct->sql_list.search(CommonStruct->Cls,
                                                                CommonStruct->Sub)) == NULL)
      return;
   if ((pSecSQL = (C_SEC_SQL *) pClsSQL->ptr_sec_sql_list()->search_type(CommonStruct->Sec)) == NULL)
      return;

   // Ricavo la connessione OLE - DB all' OLD
   if ((pConn = pSec->ptr_info()->getDBConnection(OLD)) == NULL) return;

   if (CommonStruct->AggrUse)
   { // si trasferisce il risultato di una funzione di aggregazione
      TCHAR    *err;
      C_STRING SQLAggrCond, Source, Operator, Value;

      Source = CommonStruct->Source;
      if (gsc_fullSQLAggrCond(CommonStruct->AggrFun, Source, Operator, Value, TRUE,
                              SQLAggrCond) == GS_BAD)
         return;

      if ((err = pConn->CheckSql(SQLAggrCond.get_name())) != NULL)
      {  // ci sono errori
         ads_set_tile(dcl->dialog, _T("error"), err);
         free(err);
         return;
      }

      pSecSQL->set_grp(SQLAggrCond.get_name());
      ads_done_dialog(dcl->dialog, DLGOK);
   }
   else // si trasferisce il valore esplicito di un attributo
   {
      if ((pAttrSource = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(CommonStruct->Source.get_name(), FALSE)) == NULL)
         return;

      // Attributo destinazione della classe principale
      if ((pClass = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls, CommonStruct->Sub)) == NULL)
         return;
      if ((pAttrDest = (C_ATTRIB *) pClass->ptr_attrib_list()->search_name(CommonStruct->Dest.get_name(), FALSE)) == NULL)
         return;

      if ((pClsConn = pClass->ptr_info()->getDBConnection(OLD)) == NULL) return;

      switch (pConn->IsCompatibleType(pAttrSource->type, pAttrSource->len, pAttrSource->dec,
                                  *pClsConn, pAttrDest->type, pAttrDest->len, pAttrDest->dec))
      {
         case GS_GOOD: // tipi compatibili
            ads_done_dialog(dcl->dialog, DLGOK);
            break;
         case GS_CAN:  // possibile perdita di dati 
         {
            int result;

            // "Attenzione, possibile perdita di dati (tipi/dimensioni differenti). Confermare l'operazione."
            if (gsc_ddgetconfirm(gsc_msg(369), &result) != GS_GOOD || result != GS_GOOD)
               return;
            else
               ads_done_dialog(dcl->dialog, DLGOK);
            break;
         }
         default:      // tipi incompatibili
            ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSNoCompatibFields)); // "*Errore* Attributi non compatibili"
            return;
      }

      pSecSQL->set_grp(CommonStruct->Source.get_name());
   }
}
// ACTION TILE : click su tasto Help
static void CALLB dcl_SecValueTransfer_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Trasferimentovaloridatabellasecondaria); } 


/*****************************************************************************/
/*.doc gsddSecValueTransfer                                                  */
/*+
  Questa comando effettua il trasferimento di un valore di un attributo
  di una scheda secondaria in un attributo della relativa entità GEOsim principale
  della banca dati caricata nella sessione di lavoro corrente.
-*/  
/*****************************************************************************/
void gsddSecValueTransfer(void)
{
   int             ret = GS_GOOD, status = DLGOK, dcl_file;
   C_LINK_SET      ResultLS;
   bool            UseLS;
   ads_hdlg        dcl_id;
   C_STRING        path;
   TCHAR           *key_attrib;   
   C_SINTH_SEC_TAB *pSinthSec;
   C_SECONDARY     *pSec;
   C_CLASS         *pClass;
   C_ATTRIB        *pAttr;
   Common_Dcl_SecValueTransfer_Struct CommonStruct;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION)
      { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }   

   if (gsc_check_op(opModEntity) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // seleziona la classe o sottoclasse
   if ((ret = gsc_ddselect_class(NONE, &pClass)) != GS_GOOD)
      if (ret == GS_CAN) return GEOsimAppl::CMDLIST.CancelCmd();
      else return GEOsimAppl::CMDLIST.ErrorCmd();

   // prima inizializzazione
   CommonStruct.SpatialSel  = _T("\"\" \"\" \"\" \"Location\" (\"all\") \"\"");
   CommonStruct.LocationUse = true;
   CommonStruct.Inverse     = FALSE;
   CommonStruct.LastFilterResultUse = false;
   CommonStruct.Logical_op  = UNION;
   CommonStruct.Cls = pClass->ptr_id()->code;
   CommonStruct.Sub = pClass->ptr_id()->sub_code;

   // leggo la lista delle tabelle secondarie
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->getSinthClsSecondaryTabList(CommonStruct.Cls, CommonStruct.Sub, CommonStruct.SinthSecList) == GS_BAD)
      return GEOsimAppl::CMDLIST.ErrorCmd();
   
   if ((pSinthSec = (C_SINTH_SEC_TAB *) CommonStruct.SinthSecList.get_head()) == NULL)
      { GS_ERR_COD = eGSNoSecondary; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   CommonStruct.Sec = pSinthSec->get_key();

   CommonStruct.AggrUse = FALSE;
   CommonStruct.AggrFun = AGGR_FUN_LIST[0];

   // Attributo Sorgente della Tabella Secondaria
   if ((pSec = (C_SECONDARY *) pClass->find_sec(CommonStruct.Sec)) == NULL)
      return GEOsimAppl::CMDLIST.ErrorCmd();
   pAttr = (C_ATTRIB *) pSec->ptr_attrib_list()->get_head();
   CommonStruct.Source = pAttr->get_name();

   if (CommonStruct.sql_list.initialize(pClass) != GS_GOOD)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   // Attributo destinazione della classe principale
   key_attrib = pClass->ptr_info()->key_attrib.get_name();
   pAttr = (C_ATTRIB *) pClass->ptr_attrib_list()->get_head();
   while (pAttr)
   {
      // scarto l'attributo chiave e attributi calcolati
      if (gsc_strcmp(key_attrib, pAttr->get_name(), FALSE) != 0 &&
          pAttr->is_calculated() == GS_BAD) break; 
      
      pAttr = (C_ATTRIB *) pAttr->get_next();
   }

   if (pAttr)
      CommonStruct.Dest = pAttr->get_name();   // attributo della classe principale

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_SEC.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   while (1)
   {
      ads_new_dialog(_T("SecValueTransfer"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ret = GS_BAD; break; }

      if (gsc_setTileDclSecValueTransfer(dcl_id, &CommonStruct) == GS_BAD)
         { ret = GS_BAD; ads_term_dialog(); break; }

      ads_action_tile(dcl_id, _T("Location"), (CLIENTFUNC) dcl_SpatialSel);
      ads_action_tile(dcl_id, _T("LocationUse"), (CLIENTFUNC) dcl_SecValueTransfer_LocationUse);
      ads_client_data_tile(dcl_id, _T("LocationUse"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Logical_op"), (CLIENTFUNC) dcl_SecValueTransfer_Logical_op);
      ads_client_data_tile(dcl_id, _T("Logical_op"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SSFilterUse"), (CLIENTFUNC) dcl_SecValueTransfer_LastFilterResultUse);
      ads_client_data_tile(dcl_id, _T("SSFilterUse"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecTable"), (CLIENTFUNC) dcl_SecValueTransfer_SecTable);
      ads_client_data_tile(dcl_id, _T("SecTable"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Aggr"), (CLIENTFUNC) dcl_SecValueTransfer_Aggr);
      ads_client_data_tile(dcl_id, _T("Aggr"), &CommonStruct);
      ads_action_tile(dcl_id, _T("aggrSQL"), (CLIENTFUNC) dcl_SecValueTransfer_aggrSQL);
      ads_client_data_tile(dcl_id, _T("aggrSQL"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecAttr"), (CLIENTFUNC) dcl_SecValueTransfer_SecAttr);
      ads_client_data_tile(dcl_id, _T("SecAttr"), &CommonStruct);
      ads_action_tile(dcl_id, _T("editsql"), (CLIENTFUNC) dcl_SecValueTransfer_editsql);
      ads_client_data_tile(dcl_id, _T("editsql"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecSQL"), (CLIENTFUNC) dcl_SecValueTransfer_SecSQL);
      ads_client_data_tile(dcl_id, _T("SecSQL"), &CommonStruct);
      ads_action_tile(dcl_id, _T("ClassAttr"), (CLIENTFUNC) dcl_SecValueTransfer_ClassAttr);
      ads_client_data_tile(dcl_id, _T("ClassAttr"), &CommonStruct);
      ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) dcl_SecValueTransfer_accept);
      ads_client_data_tile(dcl_id, _T("accept"), &CommonStruct);
      ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_SecValueTransfer_help);

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      {  
         break;
      }
      else if (status == DLGCANCEL) { ret = GS_CAN; break; } // uscita con tasto CANCEL

      switch (status)
      {
         case LOCATION_SEL:  // definizione di area di selezione
         {
            gsc_ddSelectArea(CommonStruct.SpatialSel);
            break;
         }
	   }
   }
   ads_unload_dialog(dcl_file);

   if (ret != GS_GOOD)
      if (ret == GS_CAN) return GEOsimAppl::CMDLIST.CancelCmd();
      else return GEOsimAppl::CMDLIST.ErrorCmd();

   // ricavo il LINK_SET da usare per il filtro
   ret = gsc_get_LS_for_filter(pClass, CommonStruct.LocationUse,
                               CommonStruct.SpatialSel,
                               CommonStruct.LastFilterResultUse, 
                               CommonStruct.Logical_op, 
                               ResultLS);

   // se c'è qualcosa che non va
   if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // se devono essere considerate tutte le entità della classe che sono state estratte
   if (ret == GS_CAN) UseLS = false;
   else UseLS = true;
 
   if (gsc_do_SecValueTransfer(pClass->ptr_id()->code, pClass->ptr_id()->sub_code,
                               CommonStruct.sql_list, (UseLS) ? &ResultLS : NULL,
                               CommonStruct.Dest) != GS_GOOD)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*****************************************************************/
/*.doc gsc_create_tab_sec                                        */
/*+
  Questa funzione crea una secondaria dal progetto classe 
  sottoclasse.
    
  Restituisce GS_GOOD in caso di successo altrimenti 
  restituisce GS_BAD.
-*/  
/*****************************************************************/
int gsc_create_tab_sec(int prj, int cls, int sub, C_SECONDARY *pSec)
{
   C_CLASS *pCls = NULL;

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   return pCls->CreateSecondary(pSec); 
}



int C_CLASS::CreateSecondary(C_NODE *pSec)
{
   // Verifico che esista la INFO e la secondary list
   if (!ptr_info() || !ptr_secondary_list) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // Setto la chiave primaria della secondaria uguale alla chiave della classe madre
   ((C_SECONDARY *) pSec)->ptr_info()->key_pri = ptr_info()->key_attrib;

   ((C_SECONDARY *) pSec)->pCls = this;

   // Creo la secondaria
   if (((C_SECONDARY *) pSec)->create() == GS_BAD) return GS_BAD; 
   
   // Aggiungo la secondaria alla lista delle sec. della classe
   if (ptr_secondary_list->add_tail(pSec) == GS_BAD) return GS_BAD; 

   return GS_GOOD;
}


/*************************************************************/
/*.doc gsc_create_extern_secondary                           */
/*+
  Questa funzione crea una secondaria esterna a GEOsim 
  dal progetto classe sottoclasse.
    
  Restituisce GS_GOOD in caso di successo altrimenti 
  restituisce GS_BAD.
-*/  
/*************************************************************/
int gsc_create_extern_secondary(int prj, int cls, int sub, C_SECONDARY *pSec)
{
   C_CLASS *pCls = NULL;

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;

   return pCls->CreateExternSecondary(pSec); 
}


int C_CLASS::CreateExternSecondary(C_NODE *pSec)
{
   int new_code;

   // Verifico che esista la INFO e la secondary list
   if (!ptr_info() || !ptr_secondary_list) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   ((C_SECONDARY *) pSec)->pCls = this;
  
   // Creo la secondaria esterna a GEOsim
   if ((new_code = ((C_SECONDARY *) pSec)->link()) == GS_BAD) return GS_BAD; 

   // Aggiungo la secondaria alla lista delle sec. della classe
   if (ptr_secondary_list->add_tail(pSec) == GS_BAD) return GS_BAD; // inserimento in lista

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::prepare_data              <external> */
/*+                                                                       
  Preparo istruzione per la selezione delle schede della tabella
  secondaria dal temporaneo o dall'old delle schede.
  Parametri:
  C_PREPARED_CMD_LIST &CmdList; Comandi per TEMP (primo) e OLD (secondo) da preparare
  const TCHAR *what;            Eventuale espressione da ritornare (default = NULL)
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
  const TCHAR *what;       Eventuale espressione da ritornare (default = NULL)
  const TCHAR *WhereSql;   Eventuale condizione di filtro (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::prepare_data(C_PREPARED_CMD_LIST &CmdList, const TCHAR *what, 
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
int C_SECONDARY::prepare_data(C_PREPARED_CMD &pCmd, int Type, const TCHAR *what, 
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
int C_SECONDARY::prepare_data(_CommandPtr &pCmd, int Type, const TCHAR *what,
                              const TCHAR *WhereSql)
{                 
   C_INFO_SEC       *p_info = ptr_info();
   C_STRING          statement, TableRef;
   C_DBCONNECTION   *pConn;
   _ParameterPtr     pParam;

   if (!p_info) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if ((pConn = p_info->getDBConnection(Type)) == NULL) return GS_BAD;

   if (Type == OLD) // Connessione alla tabella OLD
      TableRef = p_info->OldTableRef;
   else
   {
      if (getTempTableRef(TableRef) == GS_BAD) return GS_BAD;
   }
   // leggo record in TEMP
   statement = _T("SELECT ");
   statement += (what != NULL && wcslen(what) > 0) ? what : _T("*");
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += p_info->key_attrib;
   statement += _T("=?");
   if (WhereSql != NULL && wcslen(WhereSql) > 0)
   {
      statement += _T(" AND (");
      statement += WhereSql;
      statement += _T(")");
   }
   // Preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;
   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), adDouble) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::prepare_data                  <internal> */
/*+                                                                       
  Preparo recordset per la selezione delle schede della tab. secondaria
  dal temporaneo o dall'old delle schede in modalità seek.
  Parametri:
  int Type; Tipo di dato se = OLD da tabella OLD
            se = TEMP da tabella temporanea
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
_RecordsetPtr C_SECONDARY::prepare_data(int Type)
{                 
   C_INFO         *p_info = ptr_info();
   C_STRING       TableRef, InitQuotedId, FinalQuotedId;
   C_STRING       IndexName;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   bool           RsOpened = false;

   if (!p_info) { GS_ERR_COD = eGSInvClassType; return NULL; }

   if ((pConn = p_info->getDBConnection(OLD)) == NULL) return NULL;
   TableRef = p_info->OldTableRef;

   // nome indice per campo chiave di ricerca
   if (p_info->get_PrimaryKeyName(IndexName) == GS_BAD) return NULL;

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
         return NULL;
      }
      // Setto l'indice di ricerca
      if (gsc_DBSetIndexRs(pRs, IndexName.get_name(), GS_BAD) == GS_BAD)
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
/*.doc C_SECONDARY::prepare_reldata_where_keyAttrib    <internal> */
/*+                                                                       
  Preparo comando per la selezione delle relazioni di una tabella secondaria
  attraverso il codice della secondaria.
  Parametri:
  C_PREPARED_CMD_LIST &CmdList; Comandi per TEMP (primo) e OLD (secondo) da preparare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int C_SECONDARY::prepare_reldata_where_keyAttrib(C_PREPARED_CMD_LIST &CmdList)
{
   C_PREPARED_CMD *pCmd;
   
   CmdList.remove_all();

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_reldata_where_keyAttrib(*pCmd, TEMP) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { CmdList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_reldata_where_keyAttrib(*pCmd, OLD) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   return GS_GOOD;
}


/******************************************************************/
/*.doc C_SECONDARY::prepare_reldata_where_keyAttrib    <internal> */
/*+                                                                       
  Preparo comando per la selezione delle relazioni di una tabella secondaria
  attraverso il codice della secondaria.
  entità.
  Parametri:
  C_PREPARED_CMD &pCmd; Comando da preparare
  int            Type;  Tipo di relazioni se = OLD relazioni da tabella OLD
                        se = TEMP relazioni temporanee

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/******************************************************************/
int C_SECONDARY::prepare_reldata_where_keyAttrib(C_PREPARED_CMD &pCmd, int Type)
{
   // verifico se si può usare il metodo seek con i recordset
   if ((pCmd.pRs = prepare_reldata_where_keyAttrib(Type)) == NULL)
      // Se non è possibile usare la seek uso il comando preparato
      if (prepare_reldata_where_keyAttrib(pCmd.pCmd, Type) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
_RecordsetPtr C_SECONDARY::prepare_reldata_where_keyAttrib(int Type)
{
   C_INFO_SEC     *p_info = ptr_info();
   C_STRING       LinkTableRef, InitQuotedId, FinalQuotedId;
   C_STRING       Catalog, Schema, IndexRef;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   bool           RsOpened = false;

   if (!p_info || type != GSInternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return NULL; }

   if ((pConn = p_info->getDBConnection(OLD)) == NULL) return NULL;
   if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return NULL;

   if (Type == TEMP)
   {
      if ((pConn = p_info->getDBConnection(TEMP)) == NULL) return NULL;
      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return NULL;
      // se non esiste la tabella temporanea (creata solo se modificato qualcosa)
      if (pConn->ExistTable(LinkTableRef) == GS_BAD) return NULL;
   }

   // 1) il metodo seek è attualmente supportato solo da JET
   // 2) se si prova ad aprire un recordset con la modalità adCmdTableDirect
   //    mentre si è in una transazione questa viene automaticamente abortita
   //    con PostgreSQL
   if (gsc_strcmp(pConn->get_DBMSName(), ACCESS_DBMSNAME, FALSE) != 0)
      return NULL;

   // nome indice per campo chiave di ricerca
   if (pConn->split_FullRefTable(LinkTableRef, Catalog, Schema, IndexRef) == GS_BAD)
      return NULL;
   IndexRef += _T('G');

   // Se la connessione ole-db non supporta l'uso del catalogo e dello schema
   if (pConn->get_CatalogUsage() == 0 && pConn->get_SchemaUsage() == 0)
   {
      // rimuove se esiste il prefisso e il suffisso (es. per ACCESS = [ ])
      InitQuotedId = pConn->get_InitQuotedIdentifier();
      FinalQuotedId = pConn->get_FinalQuotedIdentifier();
      LinkTableRef.removePrefixSuffix(InitQuotedId.get_name(), FinalQuotedId.get_name());
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
int C_SECONDARY::prepare_reldata_where_keyAttrib(_CommandPtr &pCmd, int Type)
{                 
   C_INFO_SEC     *p_info = ptr_info();
   C_STRING        statement, LinkTableRef, Field;
   C_DBCONNECTION *pConn;
   _ParameterPtr   pParam;

   if (!p_info || type != GSInternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }

   if ((pConn = p_info->getDBConnection(Type)) == NULL) return GS_BAD;

   if (Type == OLD) // Connessione alla tabella OLD
   {
      if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD;
      // leggo record in OLD
      statement = _T("SELECT * FROM ");
   }
   else
   {
      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD;

      // se non esiste la tabella temporanea (creata solo se modificato qualcosa)
      if (pConn->ExistTable(LinkTableRef) == GS_BAD) return GS_CAN;

      // leggo record in TEMP
      statement = _T("SELECT * FROM ");
   }
   statement += LinkTableRef;
   statement += _T(" WHERE ");

   Field = _T("KEY_ATTRIB");
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
   statement += Field;

   statement += _T("=?");

   // Preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;
   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), adDouble) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/******************************************************************/
/*.doc C_SECONDARY::prepare_reldata_where_keyPri       <internal> */
/*+                                                                       
  Preparo comando per la selezione delle relazioni di una tabella secondaria
  attraverso il codice dell'entità della classe madre.
  Parametri:
  C_PREPARED_CMD_LIST &CmdList; Comandi per TEMP (primo) e OLD (secondo) da preparare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int C_SECONDARY::prepare_reldata_where_keyPri(C_PREPARED_CMD_LIST &CmdList)
{
   C_PREPARED_CMD *pCmd;
   
   CmdList.remove_all();

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_reldata_where_keyPri(*pCmd, TEMP) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   if ((pCmd = new C_PREPARED_CMD()) == NULL)
      { CmdList.remove_all(); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   if (prepare_reldata_where_keyPri(*pCmd, OLD) == GS_BAD) return GS_BAD;
   CmdList.add_tail(pCmd);

   return GS_GOOD;
}


/******************************************************************/
/*.doc C_SECONDARY::prepare_reldata_where_keyPri       <internal> */
/*+                                                                       
  Preparo comando per la selezione delle relazioni di una tabella secondaria
  attraverso il codice dell'entità della classe madre.
  Parametri:
  C_PREPARED_CMD &pCmd; Comando da preparare
  int            Type;  Tipo di relazioni se = OLD relazioni da tabella OLD
                        se = TEMP relazioni temporanee

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************/
int C_SECONDARY::prepare_reldata_where_keyPri(C_PREPARED_CMD &pCmd, int Type)
{
   if ((pCmd.pRs = prepare_reldata_where_keyPri(Type)) == NULL)
      // Se non è possibile usare la seek uso il comando preparato
      if (prepare_reldata_where_keyPri(pCmd.pCmd, Type) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}
_RecordsetPtr C_SECONDARY::prepare_reldata_where_keyPri(int Type)
{                 
   C_INFO_SEC     *p_info = ptr_info();
   C_STRING       LinkTableRef, InitQuotedId, FinalQuotedId;
   C_STRING       Catalog, Schema, IndexRef;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   bool           RsOpened = false;

   if (!p_info || type != GSInternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return NULL; }

   if ((pConn = p_info->getDBConnection(OLD)) == NULL) return NULL;
   if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return NULL;

   if (Type == TEMP)
   {
      if ((pConn = p_info->getDBConnection(TEMP)) == NULL) return NULL;
      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return NULL;
      // se non esiste la tabella temporanea (creata solo se modificato qualcosa)
      if (pConn->ExistTable(LinkTableRef) == GS_BAD) return NULL;
   }

   // 1) il metodo seek è attualmente supportato solo da JET
   // 2) se si prova ad aprire un recordset con la modalità adCmdTableDirect
   //    mentre si è in una transazione questa viene automaticamente abortita
   //    con PostgreSQL
   if (gsc_strcmp(pConn->get_DBMSName(), ACCESS_DBMSNAME, FALSE) != 0)
      return NULL;

   // nome indice per campo chiave di ricerca
   if (pConn->split_FullRefTable(LinkTableRef, Catalog, Schema, IndexRef) == GS_BAD)
      return NULL;
   IndexRef += _T('L');

   // Se la connessione ole-db non supporta l'uso del catalogo e dello schema
   if (pConn->get_CatalogUsage() == 0 && pConn->get_SchemaUsage() == 0)
   {
      // rimuove se esiste il prefisso e il suffisso (es. per ACCESS = [ ])
      InitQuotedId = pConn->get_InitQuotedIdentifier();
      FinalQuotedId = pConn->get_FinalQuotedIdentifier();
      LinkTableRef.removePrefixSuffix(InitQuotedId.get_name(), FinalQuotedId.get_name());
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
int C_SECONDARY::prepare_reldata_where_keyPri(_CommandPtr &pCmd, int Type)
{                 
   C_INFO_SEC     *p_info = ptr_info();
   C_STRING        statement, LinkTableRef, Field;
   C_DBCONNECTION *pConn;
   _ParameterPtr   pParam;

   if (!p_info || type != GSInternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }

   if ((pConn = p_info->getDBConnection(Type)) == NULL) return GS_BAD;

   if (Type == OLD) // Connessione alla tabella OLD
   {
      if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD; // leggo record in OLD      
   }
   else
   {
      if (getTempLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD; // leggo record in TEMP
      // se non esiste la tabella temporanea (creata solo se modificato qualcosa)
      if (pConn->ExistTable(LinkTableRef) == GS_BAD) return GS_CAN;    
   }
   statement = _T("SELECT * FROM ");
   statement += LinkTableRef;
   // WHERE CLASS_ID=? AND SUB_CL_ID=? AND KEY_PRI=?
   statement += _T(" WHERE ");

   Field = _T("CLASS_ID");
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
   statement += Field;

   statement += _T("=? AND ");

   Field = _T("SUB_CL_ID");
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
   statement += Field;

   statement += _T("=? AND ");

   Field = _T("KEY_PRI");
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
   statement += Field;

   statement += _T("=?");

   // Preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("CLASS_ID"), adDouble) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);
   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("SUB_CL_ID"), adDouble) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);
   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("KEY_PRI"), adDouble) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);
   
   return GS_GOOD;
}


/*****************************************************************/
/*.doc C_SECONDARY::Backup <internal>                            */
/*+
  Questa funzione esegue il backup dei file di una secondaria.
  Parametri:
  BackUpModeEnum Mode; Flag di modalità: Creazione , restore o cancellazione
                       del backup esistente (default = GSCreateBackUp).
  TCHAR *Dest;  direttorio di destinazione (default = NULL). 
                Se = NULL e la tabella è un file allora viene creato 
                un sottodirettorio  della classe chiamato "BACKUP".

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Le tabelle GS_CLASS e GS_ATTR non vengono trattate perchè, essendo in ACCESS,
       utilizzano la tecnica del commit/rollback.
-*/  
/******************************************************************************/
int C_SECONDARY::Backup(BackUpModeEnum Mode, TCHAR *Dest)
{
   int     prj = pCls->get_pPrj()->get_key(), result = GS_GOOD;
   int     cls = pCls->ptr_id()->code, sub = pCls->ptr_id()->sub_code;
   TCHAR   *cod36prj = NULL, *cod36cls = NULL, *cod36sub = NULL, *cod36sec = NULL;
   C_INFO  *pInfo = ptr_info();

   // Se si tratta di una secondaria esterna non è corretto chiamare il backup
   if (type == GSExternalSecondaryTable) return GS_GOOD;
   // Progetto
   if (gsc_long2base36(&cod36prj, prj, 2) == GS_BAD || !cod36prj) return GS_BAD;
   // Classe
   if (gsc_long2base36(&cod36cls, cls, 3) == GS_BAD || !cod36cls)
      { free(cod36prj); return GS_BAD; }
   // Se la classe di appartenenza e una simulazione allora Sottoclasse
   if (pCls->get_category() == CAT_EXTERN)
      if (gsc_long2base36(&cod36sub, sub, 3) == GS_BAD || !cod36sub)
         { free(cod36prj); free(cod36cls); return GS_BAD; }
   // Secondaria
   if (gsc_long2base36(&cod36sec, code, 3) == GS_BAD || !cod36sec)
      { free(cod36prj); free(cod36cls); if (cod36sub) free(cod36sub); return GS_BAD; }
   
   // Backup delle tabelle 
   if (pInfo)
   {
      C_STRING       Catalog, Schema, Table, BackUpTable, BackUpTableLink, OldLinkTableRef;
      C_DBCONNECTION *pConn;
      int            conf = GS_BAD;

      // Ricavo la connessione all' OLD
      pConn = pInfo->getDBConnection(OLD);
      // Ricavo il nome della tabella dei link della secondaria
      if (getOldLnkTableRef(OldLinkTableRef) == GS_BAD) return GS_BAD;

      int IsTransactionSupported = pConn->BeginTrans();
      do
      {
         if (pConn->split_FullRefTable(pInfo->OldTableRef, Catalog, Schema, Table) == GS_BAD)
            { result = GS_BAD; break; }

         // Se il riferimento completo è una path di un file (il catalogo è il direttorio)
         if (pConn->get_CatalogResourceType() == DirectoryRes)
         {
            BackUpTable = Table.get_name();
            if (pConn->split_FullRefTable(OldLinkTableRef, Catalog, Schema, Table) == GS_BAD)
               { result = GS_BAD; break; }
            BackUpTableLink = Table.get_name();
            Catalog += _T("\\BACKUP"); 
         }
         else
         {
            BackUpTable = cod36prj;
            BackUpTable += cod36cls;
            if (cod36sub) 
               BackUpTable += cod36sub;
            else
               BackUpTable += _T("000");
            BackUpTable += cod36sec;
            BackUpTable += _T('B');
         
            BackUpTableLink = BackUpTable.get_name();
            BackUpTableLink += _T('L');
         }

         // Tabella secondaria
         if (BackUpTable.paste(pConn->get_FullRefTable(Catalog, Schema, BackUpTable)) == NULL)
            { result = GS_BAD; break; }
         // Tabella dei link della secondaria
         if (BackUpTableLink.paste(pConn->get_FullRefTable(Catalog, Schema, BackUpTableLink)) == NULL)
            { result = GS_BAD; break; }

         switch (Mode)
         {
            case GSCreateBackUp: // Crea backup
               if (pConn->get_CatalogResourceType() == DirectoryRes && Catalog.get_name())
                  if (gsc_mkdir(Catalog.get_name()) == GS_BAD) 
                     { result = GS_BAD; break; }
               if (pConn->ExistTable(BackUpTable) == GS_GOOD)
               {
                  if (conf == GS_BAD)
                  {  // nel caso di ciclo viene chiesto solo la prima volta                
                     // "Tabella di backup già esistente. Sovrascrivere la tabella ?"
                     if (gsc_ddgetconfirm(gsc_msg(793), &conf) == GS_BAD) 
                        { result = GS_BAD; break; }
                     if (conf != GS_GOOD) { result = GS_CAN; break; }
                  }
                  // Cancello il backup già presente
                  if (pConn->DelTable(BackUpTable.get_name()) == GS_BAD)
                     { result = GS_BAD; break; }
                  // Verifico anche l' esistenza del file dei link e se esiste lo cancello
                  if (pConn->ExistTable(BackUpTableLink) == GS_GOOD)
                     if (pConn->DelTable(BackUpTableLink.get_name()) == GS_BAD)
                        { result = GS_BAD; break; }
               }
         
               // Eseguo la copia delle due tabelle (struttura e dati senza indici)
               if (pConn->CopyTable(pInfo->OldTableRef.get_name(), BackUpTable.get_name(),
                                    GSStructureAndDataCopyType, GS_BAD) == GS_BAD)
                  { result = GS_BAD; break; }
               if (pConn->CopyTable(OldLinkTableRef.get_name(), BackUpTableLink.get_name(),
                                    GSStructureAndDataCopyType, GS_BAD) == GS_BAD)
                  { result = GS_BAD; break; }
               break;

            case GSRemoveBackUp: // Cancella backup
               if (pConn->ExistTable(BackUpTable) == GS_GOOD)
                  if (pConn->DelTable(BackUpTable.get_name()) == GS_BAD)
                     { result = GS_BAD; break; }
               if (pConn->ExistTable(BackUpTableLink) == GS_GOOD)
                  if (pConn->DelTable(BackUpTableLink.get_name()) == GS_BAD)
                     { result = GS_BAD; break; }
               if (pConn->get_CatalogResourceType() == DirectoryRes && Catalog.get_name())
                  gsc_rmdir(Catalog);
               break;

            case GSRestoreBackUp: // ripristina backup della Tabella secondaria
               if (pConn->ExistTable(BackUpTable) == GS_GOOD)
                  if (pConn->DelTable(pInfo->OldTableRef.get_name()) == GS_BAD)
                     { result = GS_BAD; break; }
               if (pConn->CopyTable(BackUpTable.get_name(), pInfo->OldTableRef.get_name()) == GS_BAD)
                  { result = GS_BAD; break; }
               if (pConn->DelTable(BackUpTable.get_name()) == GS_BAD) 
                  { result = GS_BAD; break; }
               // Ricavo il nome della tabella dei link della secondaria
               if (getOldLnkTableRef(OldLinkTableRef) == GS_BAD) { result = GS_BAD; break; }
               if (pConn->ExistTable(BackUpTableLink) == GS_GOOD)
                  if (pConn->DelTable(OldLinkTableRef.get_name()) == GS_BAD)
                     { result = GS_BAD; break; }
               if (pConn->CopyTable(BackUpTableLink.get_name(), OldLinkTableRef.get_name()) == GS_BAD)
                  { result = GS_BAD; break; }
               if (pConn->DelTable(BackUpTableLink.get_name()) == GS_BAD)
                  { result = GS_BAD; break; }

               // reindicizzo
               if (reindex() == GS_BAD)
                  { result = GS_BAD; break; }

               break;

            default:
               break;  
         }
         if (result == GS_BAD) break;

         // gestione backup per GS_ATTRIB
         if (gsc_Backup_gs_attrib(Mode, pCls->get_pPrj(), cls, sub, code) == GS_BAD)
            { result = GS_BAD; break; }

         break;
      }
      while (1);
      
      if (result != GS_GOOD)
      {
         if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans();
         free(cod36prj);
         free(cod36cls);
         if (cod36sub) free(cod36sub);
         free(cod36sec);
            
         return GS_BAD;
      }
      else
      {
         if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();
         if (Mode == GSRestoreBackUp) // ripristina backup
            if (pConn->get_CatalogResourceType() == DirectoryRes && Catalog.get_name())
               gsc_rmdir(Catalog);
      }
   }
   if (cod36sub) { free(cod36sub); cod36sub = NULL; }
   free(cod36prj);
   free(cod36cls);
   free(cod36sec);

   return result;
}


int C_SECONDARY::copy(C_SECONDARY *out, C_CLASS *pNewCls)
{
	if (out==NULL) { GS_ERR_COD=eGSNotAllocVar; return GS_BAD; }
   
	if (pNewCls)
		out->pCls = pNewCls;
	else
		out->pCls = pCls;

	out->code = code;
	out->type = type;
	wcscpy(out->name, name);
   out->Descr = Descr;
   out->abilit = abilit;
	out->sel   = sel;

	info.copy(out->ptr_info());
	attrib_list.copy(out->ptr_attrib_list());

	return GS_GOOD;
}


/**************************************************************/
/*.doc C_SECONDARY::CopyToCls                                 */
/*+
   Funzione di copia di una secondaria.
   Parametri:
   int PrjDest;               progetto di destinazione   
   int ClsDest;               classe di destinazione
   int SubDest;               sottoclasse di destinazione
   TCHAR *Name;               Nome della nuova secondaria
   const TCHAR *NewDescr;     Descrizione della nuova secondaria
   TCHAR *TableRef;           Riferimento completo alla nuova tabella secondaria
   TCHAR *ConnStrUDLFile;     Stringa di connessione o file .UDL
   C_2STR_LIST &PropList      Lista delle proprietà UDL da variare
   DataCopyTypeEnum CopyType; Tipo di copia (GSStructureOnlyCopyType = solo struttura,
                                             GSStructureAndDataCopyType = tutto,
                                             GSDataLinkCopyType = solo link se la classe origine era esterna)

   Restituisce il puntatore alla nuova secondaria in caso di successo, altrimenti GS_BAD;
-*/  
/**************************************************************/
C_SECONDARY* C_SECONDARY::CopyToCls(int PrjDest, int ClsDest, int SubDest, DataCopyTypeEnum CopyType,
                                    const TCHAR *Name, const TCHAR *NewDescr,
                                    const TCHAR *TableRef,
                                    const TCHAR *ConnStrUDLFile, C_2STR_LIST &PropList)
{
   C_CLASS     *pNewCls = NULL;
   C_SECONDARY *pNewSec = NULL;
   int         res;

   // Verifico abilitazione alla copia di secondarie
   if (gsc_check_op(opCopySec) == GS_BAD) return GS_BAD;

   // Ricavo il puntatore alla classe del progetto di destinazione
   if ((pNewCls = gsc_find_class(PrjDest, ClsDest, SubDest)) == NULL) return GS_BAD;
   // Alloco un' oggetto C_SECONDARY
   if ((pNewSec = new C_SECONDARY) == NULL) return GS_BAD;

   // copio la vecchia secondaria nella nuova
   if (copy(pNewSec, pNewCls) == GS_BAD) { delete pNewSec; return GS_BAD; }

   // Inizializzo il nome della secondaria
   gsc_strcpy(pNewSec->name, Name, MAX_LEN_CLASSNAME);
   pNewSec->Descr = NewDescr;

   // Inizializo la info della secondaria
   pNewSec->ptr_info()->OldTableRef    = TableRef;
   pNewSec->ptr_info()->ConnStrUDLFile = ConnStrUDLFile;
   PropList.copy(pNewSec->ptr_info()->UDLProperties);

   res = GS_BAD;
   if (type == GSExternalSecondaryTable) // Secondaria esterna
   {
      do
      {
         // Creo il  collegamento alla secondaria esterna
         if (((C_SECONDARY *) pNewSec)->link() == 0) break;
         // Aggiungo la secondaria alla lista delle sec. della classe
         if (pNewCls->ptr_secondary_list->add_tail(pNewSec) == GS_BAD) break;
         res = GS_GOOD;
      }
      while (0);
   }
   else // Secondaria di GEOsim
   {
      C_ATTRIB       *pKeyAttrib;
      C_STRING       ProviderDescr;
      C_DBCONNECTION *pSourceConn, *pDestConn;

      pSourceConn = ptr_info()->getDBConnection(OLD);
      pDestConn   = pNewSec->ptr_info()->getDBConnection(OLD);

      do
      {
         // Conversione struttura tabella
         if (pNewSec->ptr_attrib_list()->Convert2ProviderType(pSourceConn, pDestConn) == GS_BAD)
            break;

         if (!(pKeyAttrib = (C_ATTRIB *) pNewSec->ptr_attrib_list()->search_name(ptr_info()->key_attrib.get_name(), FALSE)))
            break;
         // Il valore chiave viene forzato al tipo di default del DBMS della classe (numero intero)
         if (pDestConn->Type2ProviderType(CLASS_KEY_TYPE_ENUM, // DataType per campo chiave
                                          FALSE,               // IsLong
                                          FALSE,               // IsFixedPrecScale
                                          RTT,                 // IsFixedLength
                                          TRUE,                // IsWrite
                                          CLASS_LEN_KEY_ATTR,  // Size
                                          0,                   // Prec
                                          ProviderDescr,       // ProviderDescr
                                          &(pKeyAttrib->len),
                                          &(pKeyAttrib->dec)) == GS_BAD)
            break;
         gsc_strcpy(pKeyAttrib->type, ProviderDescr.get_name(), MAX_LEN_FIELDTYPE);

         // Imposto il nome dell'attributo di collegamento della classe madre
         pNewSec->ptr_info()->key_pri = pNewCls->ptr_info()->key_attrib;

         // Creo la secondaria
         if (((C_SECONDARY *) pNewSec)->create() == 0) break;
         
         if (CopyType == GSStructureAndDataCopyType)  // copio struttura e dati
         {
            _RecordsetPtr  pReadRs;
            _CommandPtr    pInsCmd;
            C_STRING       statement;

            // Cancello il contenuto della tabella
            if (gsc_DBDelRows(pDestConn->get_Connection(),
                              pNewSec->ptr_info()->OldTableRef.get_name()) == GS_BAD)
               break;
               
            // copio il contenuto da una tabella all'altra
            if (pDestConn->InitInsRow(pNewSec->ptr_info()->OldTableRef.get_name(),
                                      pInsCmd) == GS_BAD)
               break;

            statement = _T("SELECT * FROM ");
            statement += ptr_info()->OldTableRef;
            if (pSourceConn->OpenRecSet(statement, pReadRs, adOpenForwardOnly, adLockReadOnly, ONETEST) == GS_BAD)
               break;

            // gsc_DBInsRowSet è implementato in una transaction
            if (gsc_DBInsRowSet(pInsCmd, pReadRs, ONETEST) == GS_BAD)
            {
               gsc_DBCloseRs(pReadRs);
               break;
            }

            gsc_DBCloseRs(pReadRs);

            // copio le relazioni
            C_STRING OldSecLinkTableRef, NewSecLinkTableRef;

            if (getOldLnkTableRef(OldSecLinkTableRef) == GS_BAD ||
                pNewSec->getOldLnkTableRef(NewSecLinkTableRef) == GS_BAD)
               break;

            // Cancello il contenuto della tabella
            if (gsc_DBDelRows(pDestConn->get_Connection(),
                              NewSecLinkTableRef.get_name()) == GS_BAD)
               break;
            
            // copio il contenuto da una tabella all'altra
            if (pDestConn->InitInsRow(NewSecLinkTableRef.get_name(), pInsCmd) == GS_BAD)
               break;

            statement = _T("SELECT * FROM ");
            statement += OldSecLinkTableRef;
            if (pSourceConn->OpenRecSet(statement, pReadRs, adOpenForwardOnly, adLockReadOnly, ONETEST) == GS_BAD)
               break;

            if (gsc_DBInsRowSet(pInsCmd, pReadRs, ONETEST) == GS_BAD)
            {
               gsc_DBCloseRs(pReadRs);
               break;
            }

            gsc_DBCloseRs(pReadRs);

            // Aggiorno il codice classe e sottoclasse
            statement = _T("UPDATE ");
            statement += NewSecLinkTableRef;
            statement += _T(" SET CLASS_ID=");
            statement += ClsDest;
            statement += _T(", SUB_CL_ID=");
            statement += SubDest;
            if (pDestConn->ExeCmd(statement) == GS_BAD)
               break;
         }

         // Aggiungo la secondaria alla lista delle sec. della classe
         if (pNewCls->ptr_secondary_list->add_tail(pNewSec) == GS_BAD) break;
         res = GS_GOOD;
      }
      while (0);
   }
   
   if (res == GS_BAD)
   {
      delete (pNewSec);
      pNewSec = NULL;
   }
   else
      CopySupportFiles(pNewSec);

   return pNewSec;
}


/*********************************************************/
/*.doc gsc_get_reldata                        <external> */
/*+                                                                       
  Legge i dati usando il comando preparato nella
  funzione <prepare_reldata_where_keyPri>.
  Parametri:
  C_PREPARED_CMD_LIST &TempOldCmdList;
  int                 Cls;
  int                 Sub;
  long 		          Key;
  C_RB_LIST           &ColValues;     (output)
  oppure
  _RecordsetPtr &pRs;         Apre il recordset e lo lascia aperto in modifica

  int         *Source;        Provenienza dei dati (TEMP o OLD; default = NULL);
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_get_reldata(C_PREPARED_CMD_LIST &TempOldCmdList, int Cls, int Sub, long Key, 
                    C_RB_LIST &ColValues, int *Source)
{
   // Cerco nel temporaneo
   if (gsc_get_reldata(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(1)), Cls, Sub, Key, ColValues) == GS_GOOD)
      { if (Source) *Source = TEMP; return GS_GOOD; }

   // Cerco nell'old
   if (gsc_get_reldata(*((C_PREPARED_CMD *)TempOldCmdList.getptr_at(2)), Cls, Sub, Key, ColValues) == GS_GOOD)
      { if (Source) *Source = OLD; return GS_GOOD; }

   return GS_BAD;
}
int gsc_get_reldata(C_PREPARED_CMD &pCmd, int Cls, int Sub, long Key, C_RB_LIST &ColValues)
{
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {  // Uso il recordset preparato per la seek
      if (gsc_get_reldata(pCmd.pRs, Cls, Sub, Key, ColValues) == GS_GOOD) return GS_GOOD;
   }
   else
      if (pCmd.pCmd != NULL && pCmd.pCmd.GetInterfacePtr())
         if (gsc_get_reldata(pCmd.pCmd, Cls, Sub, Key, ColValues) == GS_GOOD) return GS_GOOD;

   return GS_BAD;
}
int gsc_get_reldata(_RecordsetPtr &pRs, int Cls, int Sub, long Key, C_RB_LIST &ColValues)
{
   C_RB_LIST      Buffer;
   presbuf        pCls, pSub, pKey, pChildKey, pStatus;
   long           ChildKey, _Key;
   int            Status = UNMODIFIED, result = GS_GOOD, _Cls, _Sub;
   SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 3 elementi
   SAFEARRAYBOUND rgsabound;
   _variant_t     var, KeyForSeek;
   long           ix;

   rgsabound.lLbound   = 0;
   rgsabound.cElements = 3;
   psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

   // CODICE CLASSE
   ix  = 0;
   gsc_set_variant_t(var, (short) Cls);
   SafeArrayPutElement(psa, &ix, &var);

   // CODICE SOTTOCLASSE
   ix  = 1;
   gsc_set_variant_t(var, (short) Sub);
   SafeArrayPutElement(psa, &ix, &var);

   // CODICE CHIAVE
   ix  = 2;
   gsc_set_variant_t(var, Key);
   SafeArrayPutElement(psa, &ix, &var);

   KeyForSeek.vt = VT_ARRAY|VT_VARIANT;
   KeyForSeek.parray = psa;  

   if (gsc_DBSeekRs(pRs, KeyForSeek, adSeekFirstEQ) == GS_BAD) return GS_BAD;

   if (gsc_isEOF(pRs) == GS_GOOD)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
   
   if (gsc_InitDBReadRow(pRs, Buffer) == GS_BAD) return GS_BAD;

   pCls      = Buffer.CdrAssoc(_T("CLASS_ID"));
   pSub      = Buffer.CdrAssoc(_T("SUB_CL_ID"));
   pKey      = Buffer.CdrAssoc(_T("KEY_PRI"));
   pChildKey = Buffer.CdrAssoc(_T("KEY_ATTRIB"));
   pStatus   = Buffer.CdrAssoc(_T("STATUS"));

   // ciclo di lettura
   if ((ColValues << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, Buffer) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_rb2Int(pCls, &_Cls) == GS_BAD || _Cls != Cls ||
          gsc_rb2Int(pSub, &_Sub) == GS_BAD || _Sub != Sub ||
          gsc_rb2Lng(pKey, &_Key) == GS_BAD || _Key != Key)
         break;

      if (gsc_rb2Lng(pChildKey, &ChildKey) == GS_BAD) ChildKey = 0;   // Codice dell'entità figlia collegata
      if (pStatus) // presente solo nelle relazioni temporanee
         gsc_rb2Int(pStatus, &Status); // Stato della relazione

      if ((ColValues += acutBuildList(RTLB,
								                 RTLONG,  ChildKey,
								                 RTSHORT, Status,
								              RTLE, 0)) == NULL)
         { result = GS_BAD; break; }
      gsc_Skip(pRs);
   }

   if ((ColValues += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   return GS_GOOD;
}
int gsc_get_reldata(_CommandPtr &pCmd, int Cls, int Sub, long Key, C_RB_LIST &ColValues)
{
   _RecordsetPtr pInternalRs;
   C_RB_LIST     Buffer;
   presbuf       pChildKey, pStatus;
   long          ChildKey;
   int           Status = UNMODIFIED, result = GS_GOOD;

   if (gsc_SetDBParam(pCmd, 0, Cls) == GS_BAD) return GS_BAD;
   if (gsc_SetDBParam(pCmd, 1, Sub) == GS_BAD) return GS_BAD;
   if (gsc_SetDBParam(pCmd, 2, Key) == GS_BAD) return GS_BAD;

   if (gsc_ExeCmd(pCmd, pInternalRs) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pInternalRs) == GS_GOOD)
   {
      gsc_DBCloseRs(pInternalRs);
      GS_ERR_COD = eGSInvalidKey;
      return GS_BAD;
   }
   
   if (gsc_InitDBReadRow(pInternalRs, Buffer) == GS_BAD)
      { gsc_DBCloseRs(pInternalRs); return GS_BAD; }

   pChildKey = Buffer.CdrAssoc(_T("KEY_ATTRIB"));
   pStatus   = Buffer.CdrAssoc(_T("STATUS"));

   // ciclo di lettura
   if ((ColValues << acutBuildList(RTLB, 0)) == NULL)
      { gsc_DBCloseRs(pInternalRs); return GS_BAD; }

   while (gsc_isEOF(pInternalRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pInternalRs, Buffer) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_rb2Lng(pChildKey, &ChildKey) == GS_BAD) ChildKey = 0;   // Codice dell'entità figlia collegata
      if (pStatus) // presente solo nelle relazioni temporanee
         gsc_rb2Int(pStatus, &Status); // Stato della relazione

      if ((ColValues += acutBuildList(RTLB,
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
int gsc_get_reldata(_CommandPtr &pCmd, int Cls, int Sub, long Key, _RecordsetPtr &pRs)
{
   // cerco nel temp
   if (gsc_SetDBParam(pCmd, 0, Cls) == GS_BAD) return GS_BAD;
   if (gsc_SetDBParam(pCmd, 1, Sub) == GS_BAD) return GS_BAD;
   if (gsc_SetDBParam(pCmd, 2, Key) == GS_BAD) return GS_BAD;

   if (gsc_ExeCmd(pCmd, pRs) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      gsc_DBCloseRs(pRs);
      GS_ERR_COD = eGSInvalidKey;
      return GS_BAD;
   }
   
   return GS_GOOD;
}
int gsc_get_reldata(C_PREPARED_CMD &pCmd, int Cls, int Sub, long Key, 
                    _RecordsetPtr &pRs, int *IsRsCloseable)
{
   if (pCmd.pRs != NULL && pCmd.pRs.GetInterfacePtr())
   {  // Uso il recordset preparato per la seek
      SAFEARRAY FAR* psa = NULL; // Creo un safearray che prende 3 elementi
      SAFEARRAYBOUND rgsabound;
      _variant_t     var, KeyForSeek;
      long           ix;

      *IsRsCloseable = GS_BAD;

      rgsabound.lLbound   = 0;
      rgsabound.cElements = 3;
      psa = SafeArrayCreate(VT_VARIANT, 1, &rgsabound);

      // CODICE CLASSE
      ix  = 0;
      gsc_set_variant_t(var, (short) Cls);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE SOTTOCLASSE
      ix  = 1;
      gsc_set_variant_t(var, (short) Sub);
      SafeArrayPutElement(psa, &ix, &var);

      // CODICE CHIAVE
      ix  = 2;
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
         if (gsc_get_reldata(pCmd.pCmd, Cls, Sub, Key, pRs) == GS_GOOD) return GS_GOOD;
      }

   return GS_BAD;
}


/*********************************************************/
/*.doc gs_destroysec                          <external> */
/*+
  Questa funzione è la versione funzione LISP della "gsc_destroysec".
  Parametri:
  Lista di resbuf (<Password><codice progetto><codice classe><codice sottoclasse><sec>)
    
  Restituisce TRUE in caso di successo (anche parziale) altrimenti 
  restituisce nil.
-*/  
/*********************************************************/
int gs_destroysec(void)
{
   presbuf arg = acedGetArgs();
   int     prj, cls, sub, sec;
   TCHAR   *Password;

   acedRetNil();
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   Password = arg->resval.rstring;
   arg = arg->rbnext;
   // Legge nella lista dei parametri: progetto classe e sub
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   // sec
   if (arg == NULL || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   sec = arg->resval.rint;

   if (gsc_destroy_tab_sec(prj, cls, sub, sec, Password) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_copy_sec                            <external> */
/*+
  Questa funzione copia la struttura di una tabella secondaria di GEOsim
  oppure il link di una tabella secondaria esterna.
  Parametri:
  Lista RESBUF
  (<prj><cls><sub><sec><newprj><newcls><newsub><newsecname><newsecDescr><dblist>)

  dove:
  <UDLConnStr>          = <file UDL> || <stringa di connessione>
  <property list>       = nil || ((prop.UDL)(Val.prop.)...)
  <riferimento tabella> = (<cat><sch><tab>)
  <dblist>              = (<UDLConnStr> <property list> <riferimento tabella>)
  <UDLConnStr>          = <file UDL> || <stringa di connessione>
  <property list>       = nil || ((prop.UDL)(Val.prop.)...)
  <riferimento tabella> = (<cat><sch><tab>)

  Ritorna il codice della nuova classe in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
int gs_copy_sec(void)
{
   int         prj, cls, sub, sec;
	presbuf     arg;
	C_CLASS     *pNewCls;
   C_SECONDARY *pOldSec, *pNewSec;
   C_INFO      InfoConn;
   C_STRING    NewSecName, NewSecDescr;
              
   acedRetNil();

   // ricavo i valori impostati
   arg = acedGetArgs();

   // codice del progetto origine
   if (!arg || gsc_rb2Int(arg, &prj) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // codice classe origine
   arg = arg->rbnext;
   if (!arg || gsc_rb2Int(arg, &cls) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // sottoclasse origine
   arg = arg->rbnext;
   if (!arg || gsc_rb2Int(arg, &sub) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // secondaria origine
   arg = arg->rbnext;
   if (!arg || gsc_rb2Int(arg, &sec) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // Ricavo secondaria sorgente
   if (!(pOldSec = gsc_find_sec(prj, cls, sub, sec))) return RTERROR;

   arg = arg->rbnext;
   // codice del progetto destinazione
   if (!arg || gsc_rb2Int(arg, &prj) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // codice classe destinazione
   arg = arg->rbnext;
   if (!arg || gsc_rb2Int(arg, &cls) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // sottoclasse destinazione
   arg = arg->rbnext;
   if (!arg || gsc_rb2Int(arg, &sub) == GS_BAD) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if ((pNewCls = gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;
   if (!pNewCls->ptr_info() || !pNewCls->ptr_attrib_list())
      { GS_ERR_COD = eGSInvClassType; return RTERROR; }

   // nome secondaria destinazione 
   arg = arg->rbnext;
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   NewSecName = arg->resval.rstring;
   // nome secondaria destinazione 
   arg = arg->rbnext;
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   NewSecDescr = arg->resval.rstring;
   // dblist 
   arg = arg->rbnext;
   if (!arg || arg->restype != RTLB) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((InfoConn.from_rb(arg)) == GS_BAD) return RTERROR;
            
   // Copia solo struttura secondaria
   if ((pNewSec = pOldSec->CopyToCls(prj, cls, sub, GSStructureOnlyCopyType,
                                     NewSecName.get_name(), NewSecDescr.get_name(),
                                     InfoConn.OldTableRef.get_name(),
                                     InfoConn.ConnStrUDLFile.get_name(),
                                     InfoConn.UDLProperties)) == NULL) return RTERROR;

   acedRetInt(pNewSec->code);

   return RTNORM;
}


///////////////////////////////////////////////////////////////////////////////


/*************************************************************/
/*.doc C_SECONDARY::query_AllData                 <external> */
/*+                                                                       
  Interroga tutte le secondarie collegate all'entità GEOsim con
  codice passato come parametro.
  Parametri:
  long      key_pri;         Codice dell' entità GEOsim.
  C_RB_LIST &ColValues;      Lista di resbuf contenente i dati 
                             delle schede secondarie.
                             (((<attr><val>)...) ((<attr><val>)...) ...)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/  
/*************************************************************/
int C_SECONDARY::query_AllData(long key_pri, C_RB_LIST &ColValues)
{
   resbuf rb;

   rb.restype = RTLONG;
   rb.resval.rlong = key_pri;
   
   return query_AllData(&rb, ColValues);
}
int C_SECONDARY::query_AllData(presbuf key_pri, C_RB_LIST &ColValues)
{
   C_STRING       statement, TableSec, LnkTableSec, Field;
   C_DBCONNECTION *pConn;
   
   if (gsc_check_op(opQryEntity) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->get_status() != WRK_SESSION_ACTIVE) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   ColValues.remove_all();

   if (type == GSExternalSecondaryTable)
   {
      // Se chiave primaria nulla non cerco nulla
      if (key_pri->restype == RTNIL || key_pri->restype == RTNONE) return GS_GOOD;

      // Ricavo la connessione OLE - DB
      if ((pConn = info.getDBConnection(OLD)) == NULL) return GS_BAD;

      Field = info.key_attrib;
      if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                        pConn->get_FinalQuotedIdentifier(),
                        pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;

      // Ora compongo l' istruzione Sql
      statement = _T("SELECT * FROM ");
      statement += info.OldTableRef;
      statement += _T(" WHERE ");
      statement += Field;

      statement += _T("="); 
      if (key_pri->restype == RTSTR)
      {
         Field = key_pri;
         Field.strtran(_T("'"), _T("''")); // devo raddoppiare eventuali apici
         Field.addPrefixSuffix(_T("'"), _T("'"));

         statement += Field;
      }
      else
         statement += key_pri;

      if (info.vis_attrib.len() > 0)
      {
         Field = info.vis_attrib;
         statement += _T(" ORDER BY ");
         if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                           pConn->get_FinalQuotedIdentifier(),
                           pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
         statement += Field;


         if (info.vis_attrib_desc_order_type) // ordine decrescente
            statement += _T(" DESC");
      }

      // Leggo le righe
      return pConn->ReadRows(statement, ColValues);
   }

   // Ricavo il riferimento completo alla tabella temporanea della secondaria
   if (getTempTableRef(TableSec, GS_BAD) == GS_BAD) return GS_BAD;
   // Ricavo la connessione OLE - DB
   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;
   // Se esiste il temporaneo eseguo la query su questo
   if (pConn->ExistTable(TableSec) == GS_GOOD)
   {
      // Ricavo il riferimento completo alla tabella temporanea dei link della secondaria
      if (getTempLnkTableRef(LnkTableSec) == GS_BAD) return GS_BAD;
      // se non esiste allora creo la tabella dei link TEMPORANEA con 
      // aggiunta del campo STATUS (1° parametro TRUE)
      if (pConn->ExistTable(LnkTableSec) == GS_BAD)
         if (create_tab_link(TRUE, TEMP) == GS_BAD) return GS_BAD;

      // Prima se esistono relazioni sul TEMP senza controllare STATUS
      statement = _T("SELECT KEY_ATTRIB FROM ");
      statement += LnkTableSec;
      statement += _T(" WHERE KEY_PRI=");
      statement += key_pri;
      // Leggo le righe
      if (pConn->ReadRows(statement, ColValues) == GS_BAD) return GS_BAD;
      if (ColValues.GetCount() > 0)
      {
         statement = _T("SELECT * FROM ");
         statement += TableSec;
         statement += _T(" WHERE ");
         statement += ptr_info()->key_attrib;
         statement += _T(" IN (SELECT KEY_ATTRIB FROM ");
         statement += LnkTableSec;
         statement += _T(" WHERE KEY_PRI=");
         statement += key_pri;
         statement += _T(" AND STATUS<>");
         statement += ERASED;
         statement += _T(")");

         if (info.vis_attrib.len() > 0)
         {
            statement += _T(" ORDER BY ");
            Field = info.vis_attrib;
            if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                              pConn->get_FinalQuotedIdentifier(),
                              pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
            statement += Field;

            if (info.vis_attrib_desc_order_type) // ordine decrescente
               statement += _T(" DESC");
         }

         // Leggo le righe
         if (pConn->ReadRows(statement, ColValues) == GS_BAD) return GS_BAD;
         
         return GS_GOOD;
      }
   }

   // Ricavo il riferimento completo alla tabella OLD dei link della secondaria
   if (getOldLnkTableRef(LnkTableSec) == GS_BAD) return GS_BAD;
   // Ricavo la connessione OLE - DB
   if ((pConn = info.getDBConnection(OLD)) == NULL) return GS_BAD;

   // Poi verifichiamo sull'OLD
   statement = _T("SELECT * FROM ");
   statement += ptr_info()->OldTableRef;
   statement += _T(" WHERE ");
   statement += ptr_info()->key_attrib;
   statement += _T(" IN (SELECT ");

   Field = _T("KEY_ATTRIB");
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
   statement += Field;

   statement += _T(" FROM ");
   statement += LnkTableSec;
   statement += _T(" WHERE ");

   Field = _T("KEY_PRI");
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
   statement += Field;

   statement += _T("=");
   statement += key_pri;
   statement += _T(")");

   if (info.vis_attrib.len() > 0)
   {
      Field = info.vis_attrib;
      statement += _T(" ORDER BY ");
      if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(),
                        pConn->get_FinalQuotedIdentifier(),
                        pConn->get_StrCaseFullTableRef()) == GS_BAD) return GS_BAD;
      statement += Field;

      if (info.vis_attrib_desc_order_type) // ordine decrescente
         statement += _T(" DESC");
   }

   // Ricavo la connessione OLE - DB
   if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   // Leggo le righe
   return pConn->ReadRows(statement, ColValues);
}


/*************************************************************/
/*.doc C_SECONDARY::CopyDataToTemp                <external> */
/*+                                                                       
  Copia tutte le schede secondarie e i relativi collegamenti per un'entità 
  di GEOsim con codice passato come parametro nelle tabelle temporanee.
  La copia avviene solo se non è già stata fatta prima.
  Parametri:
  long                key_pri;             Codice dell' entità GEOsim.
  C_PREPARED_CMD      *pOldCmd;            Comando per lettura dati OLD (in caso
                                           di letture multiple); default = NULL.
  C_PREPARED_CMD_LIST *pTempOldRelCmdList; Lista di comandi per lettura relazioni TEMP (1 elemento)
                                           e OLD (2 elemento) (in caso di letture multiple)
                                           partendo dal codice entità; default = NULL.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/  
/*************************************************************/
int C_SECONDARY::CopyDataToTemp(long key_pri,
                                C_PREPARED_CMD *pOldCmd,
                                C_PREPARED_CMD_LIST *pTempOldRelCmdList)
{
   C_STRING       OldLnkTable, TempTable, TempLinkTable;
   C_DBCONNECTION *pConn;
   C_PREPARED_CMD TmpCmd, OldCmd;
   int            TempTableExist, IsSourceRsCloseable, result = GS_BAD, ClassId, SubClId;
   _RecordsetPtr  pSourceRs, pDestRs;
   C_LONG_BTREE   KeyList;
   C_BLONG        *pKey;
   long           Key, KeyPri;
   C_RB_LIST      RelColValues, ColValues;
   presbuf        prb_key, prb_ClassId, prb_SubClId, prb_KeyPri;

   if (type != GSInternalSecondaryTable) return GS_BAD;

   // Ricavo la connessione OLE-DB alla tabella TEMP
   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;

   int IsTransactionSupported = pConn->BeginTrans();
   do
   {
      // Verifico l'esistenza della tabella temporanea dei link
      getTempLnkTableRef(TempLinkTable);
      TempTableExist = pConn->ExistTable(TempLinkTable);
      // Se esiste allora verifico se i dati sono già stati copiati nel TEMP
      if (TempTableExist == GS_GOOD)
      {
         if (pTempOldRelCmdList) // utilizzo i comandi preparati
         {
            if (gsc_get_reldata(*((C_PREPARED_CMD *) pTempOldRelCmdList->get_head()), 
                                pCls->ptr_id()->code, pCls->ptr_id()->sub_code, key_pri,
                                pSourceRs, &IsSourceRsCloseable) == GS_GOOD)
            {
               if (IsSourceRsCloseable == GS_GOOD) gsc_DBCloseRs(pSourceRs);
               result = GS_GOOD; // i dati erano già stati copiati
               break;
            }
         }
         else
         {
            // Preparo comando per accesso alle relazioni temporanee
            if (prepare_reldata_where_keyPri(TmpCmd, TEMP) == GS_BAD) break;

            if (gsc_get_reldata(TmpCmd, pCls->ptr_id()->code, pCls->ptr_id()->sub_code, key_pri,
                                pSourceRs, &IsSourceRsCloseable) == GS_GOOD)
            {
               if (IsSourceRsCloseable == GS_GOOD) gsc_DBCloseRs(pSourceRs);
               result = GS_GOOD; // i dati erano già stati copiati
               break;
            }
         }
      }

      if (pTempOldRelCmdList) // utilizzo i comandi preparati
      {
         // Se non esistono relazioni nell'OLD
         if (gsc_get_reldata(*((C_PREPARED_CMD *) pTempOldRelCmdList->getptr_at(2)), 
                             pCls->ptr_id()->code, pCls->ptr_id()->sub_code, key_pri,
                             pSourceRs, &IsSourceRsCloseable) == GS_BAD)
         {
            result = GS_GOOD; // i dati erano già stati copiati
            break;
         }
      }
      else
      {
         // Preparo comando per accesso alle relazioni OLD
         if (prepare_reldata_where_keyPri(OldCmd, OLD) == GS_BAD) break;
         // Se non esistono relazioni nell'OLD
         if (gsc_get_reldata(OldCmd, pCls->ptr_id()->code, pCls->ptr_id()->sub_code, key_pri, 
                             pSourceRs, &IsSourceRsCloseable) == GS_BAD)
         {
            result = GS_GOOD; // i dati erano già stati copiati
            break;
         }
      }

      // se non esiste la tabella temporanea dei link creo la tabella dei dati
      // e quella delle relazioni con aggiunta del campo STATUS (1° parametro TRUE)
      if (TempTableExist == GS_BAD)
      {
         if (getTempTableRef(TempTable) == GS_BAD || create_tab_link(TRUE, TEMP) == GS_BAD)
         {
            if (IsSourceRsCloseable == GS_GOOD) gsc_DBCloseRs(pSourceRs);
            break;
         }
      }
      else getTempTableRef(TempTable, GS_BAD); // inizializzo il nome della tabella TEMP

      if (gsc_InitDBReadRow(pSourceRs, RelColValues) == GS_BAD)
      {
         if (IsSourceRsCloseable == GS_GOOD) gsc_DBCloseRs(pSourceRs);
         break;
      }
      prb_key     = RelColValues.CdrAssoc(_T("KEY_ATTRIB"));
      prb_ClassId = RelColValues.CdrAssoc(_T("CLASS_ID"));
      prb_SubClId = RelColValues.CdrAssoc(_T("SUB_CL_ID"));
      prb_KeyPri  = RelColValues.CdrAssoc(_T("KEY_PRI"));
      // Aggiungo alla lista la colonna STATUS = UNMODIFIED
      RelColValues.remove_tail();
      RelColValues += acutBuildList(RTLB, RTSTR, _T("STATUS"), RTSHORT, UNMODIFIED, RTLE, RTLE, 0);

      // Preparo il record set di inserimento in tabella TEMP dei link
      if (pConn->InitInsRow(TempLinkTable.get_name(), pDestRs) == GS_BAD)
      {
         if (IsSourceRsCloseable == GS_GOOD) gsc_DBCloseRs(pSourceRs);
         break;
      }
      // Copio le relazioni memorizzando il codice delle schede
      result = GS_GOOD;
      while (gsc_isEOF(pSourceRs) == GS_BAD)
      {  // leggo il record
         if (gsc_DBReadRow(pSourceRs, RelColValues) != GS_GOOD) { result = GS_BAD; break; }

         if (gsc_rb2Int(prb_ClassId, &ClassId) == GS_BAD || 
             gsc_rb2Int(prb_SubClId, &SubClId) == GS_BAD || 
             gsc_rb2Lng(prb_KeyPri, &KeyPri) == GS_BAD)
            { result = GS_BAD; break; }
         if (ClassId != pCls->ptr_id()->code || 
             SubClId != pCls->ptr_id()->sub_code || 
             KeyPri != key_pri)
            break;

         // memorizzo il codice della scheda        
         if (gsc_rb2Lng(prb_key, &Key) == GS_BAD || KeyList.add(&Key) == GS_BAD)
            { result = GS_BAD; break; }
         // inserisco li record nella tabella TEMP
         if (gsc_DBInsRow(pDestRs, RelColValues, ONETEST) == GS_BAD)
            { result = GS_BAD; break; }

         gsc_Skip(pSourceRs);
      }
      if (IsSourceRsCloseable == GS_GOOD) gsc_DBCloseRs(pSourceRs);
      gsc_DBCloseRs(pDestRs);

      if (result == GS_BAD) break;
      result = GS_BAD;
       
      // Preparo il record set di inserimento in tabella TEMP
      if (pConn->InitInsRow(TempTable.get_name(), pDestRs) == GS_BAD) break;
      
      if (pOldCmd) // utilizzo i comandi preparati
      {
         result = GS_GOOD;
         pKey = (C_BLONG *) KeyList.go_top();
         while (pKey)
         {
            // leggo e copio i dati
            if (gsc_get_data(*pOldCmd, pKey->get_key(), ColValues) == GS_BAD ||
                gsc_DBInsRow(pDestRs, ColValues, ONETEST) == GS_BAD)
               { result = GS_BAD; break; }

            pKey = (C_BLONG *) KeyList.go_next();
         }
      }
      else
      {
         // Preparo comando per accesso alle dati OLD
         if (prepare_data(OldCmd, OLD) == GS_BAD) break;
         result = GS_GOOD;
         pKey = (C_BLONG *) KeyList.go_top();
         while (pKey)
         {
            // leggo e copio i dati
            if (gsc_get_data(OldCmd, pKey->get_key(), ColValues) == GS_BAD ||
                gsc_DBInsRow(pDestRs, ColValues, ONETEST) == GS_BAD)
               { result = GS_BAD; break; }

            pKey = (C_BLONG *) KeyList.go_next();
         }
      }

      gsc_DBCloseRs(pDestRs);

      if (result == GS_BAD) break;

      result = GS_GOOD;
   }
   while (0);

   if (result != GS_GOOD)
   {
      if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans();
      return GS_BAD;
   }

   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) C_SECONDARY::is_updateable <internal> */
/*+                                                                       
  Verifica la possibilità di modificare un'entità.
  Parametri:
  int *WhyNot; Flag rappresentante il motivo per cui non è aggiornabile (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::is_updateable(int *WhyNot)
{
   if (WhyNot) *WhyNot = eGSUnknown;

   // verifico abilitazione
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(WhyNot) == GS_BAD) return GS_BAD;
   if (abilit != GSUpdateableData)
   {
      GS_ERR_COD = (abilit == GSReadOnlyData) ? eGSSecIsReadOnly : eGSSecLocked;
      if (WhyNot) *WhyNot = GS_ERR_COD;
      return GS_BAD;
   }
   if (type != GSInternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }

   return GS_GOOD;
}


/****************************************************/
/*.doc C_SECONDARY::upd_data
/*+
  Aggiorna il record avente identificatore <gs_id> nella tabella secondaria.
  Parametri
  long      key_pri;        Codice dell'entità GEOsim.
  long      gs_id;		    Codice scheda secondaria
  C_RB_LIST &ColValues;	    Lista (colonna-valore)
  C_PREPARED_CMD_LIST *pTempCmd;    Lista di comandi per lettura dati TEMP
                                    (in caso di letture multiple);
                                    default = NULL.
                                                        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B.: Se viene passato il parametro <pTempCmdList> allora non viene 
        verificata la modificabilità nè viene aggiornata l'entità madre nè
        vengono copiate le schede secondarie nel TEMP.
-*/  
/****************************************************/
int C_SECONDARY::upd_data(long key_pri, long gs_id, C_RB_LIST &ColValues,
                          C_PREPARED_CMD *pTempCmd)
{   
   C_PREPARED_CMD TempCmd, TempRelCmd;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   int            status, dummy = 0, IsRsCloseable, result = GS_BAD, WhyNotUpd;
   C_RB_LIST      _ColValues;

   if (gsc_check_op(opModSecondary) == GS_BAD) return GS_BAD;
   if (is_updateable(&WhyNotUpd) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Se ci sono entrambi i parametri è sottinteso che la funzione si a chiamata
   // ciclicamente e quindi questo controllo è delegato alla funzione chiamante
   if (!pTempCmd)
   {
      // Verifico modificabilità entità madre se NON si tratta di entità nuova
      // (provando a bloccarla e ad estrarla se estratta parzialmente e ponendo il
      // record nella tabella temporanea)
      if (pCls->is_NewEntity(key_pri) != GS_GOOD)
      {
         C_PREPARED_CMD_LIST TempOldCmdList;

         // Preparo i comandi di lettura dei dati della classe dal temp/old
         if (pCls->prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;
         if (pCls->query_data(key_pri, _ColValues, &TempOldCmdList) == GS_BAD) return GS_BAD;
         if (pCls->upd_data(key_pri, _ColValues,
                           (C_PREPARED_CMD *) TempOldCmdList.get_head()) == GS_BAD) return GS_BAD;
         _ColValues.remove_all();
      }

      // Copio le schede e le relazioni dalle tabelle OLD   
      if (CopyDataToTemp(key_pri) == GS_BAD) return GS_BAD;
   }

   
   // Ricavo la connessione OLE-DB alla tabella TEMP
   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;

   int IsTransactionSupported = pConn->BeginTrans();
   do
   {
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

      // Lettura relazione temporanea
      if (prepare_reldata_where_keyAttrib(TempRelCmd, TEMP) == GS_BAD) break;
      if (gsc_get_data(TempRelCmd, gs_id, pRs, &IsRsCloseable) == GS_BAD) break;
      // non si deve modificare il link relativo ad un record <Inserted>
      if (gsc_DBReadRow(pRs, _ColValues) == GS_BAD)
         { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }

      // se STATUS è nullo viene considerato UNMODIFIED
      if (gsc_rb2Int(_ColValues.CdrAssoc(_T("STATUS")), &status) == GS_BAD)
         status = UNMODIFIED;
      if (status & ERASED) // secondaria cancellata
      {
         if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
         GS_ERR_COD = eGSInvalidKey;
         break;
      }
      // non si deve modificare il link relativo ad un record <Inserted>
      // ma solo se risulatava <Unmodified>
      if (status & UNMODIFIED)
      {
         // Se non era già da salvare verifico la variabile "AddEntityToSaveSet"
         if (status & NOSAVE)
            // Poichè il file delle relazioni temporanee fa fede per sapere quali secondarie
            // salvare (come GEOsimAppl::SAVE_SS per le classi con rappresentazione grafica diretta)
            // riporto anche il il bit n. 6 (100000 = 32 = NOSAVE)
            if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD) dummy = NOSAVE;
         
         _ColValues << acutBuildList(RTLB,
                                     RTLB, RTSTR, _T("STATUS"), RTSHORT, MODIFIED | dummy, RTLE,
                                     RTLE, 0);

         if (gsc_DBUpdRow(pRs, _ColValues) == GS_BAD)
            { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
      }
      
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

      // validazione e ricalcolo dati
      if (CalcValidData(ColValues, MODIFY) == GS_BAD) break;

      if (pTempCmd) // utilizzo i comandi preparati
      {
         // lettura dei dati della secondaria dal TEMP
         if (gsc_get_data(*pTempCmd, gs_id, pRs, &IsRsCloseable) == GS_BAD) break;
      }
      else
      {
         // Preparo i comandi di lettura dei dati della tab. secondaria dal TEMP
         if (prepare_data(TempCmd, TEMP) == GS_BAD) break;      
         // lettura dei dati della secondaria dal temp
         if (gsc_get_data(TempCmd, gs_id, pRs, &IsRsCloseable) == GS_BAD) break;
      }
      // aggiorno il record
      if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
         { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); break; }
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);


      result = GS_GOOD;
   }
   while (0);

   if (result != GS_GOOD)
   {
      if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans();
      return GS_BAD;
   }

   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_SECONDARY::upd_data                  <external> */
/*+                                                                       
  Aggiorna i record delle scheda secondarie dell'entita' della classe 
  con una nuova serie di valori.
  Parametri:
  long      key_pri;        Codice dell'entità GEOsim.
  C_LONG_BTREE &KeyList;    Lista di codici delle entità da aggiornare
  C_RB_LIST &ColValues;     Lista ((<colonna><valore>[<operatore>[<Perc>]])(...)) 
                            dei SOLI attributi da aggiornare

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
-*/  
/*********************************************************/
int C_SECONDARY::upd_data(long key_pri, C_LONG_BTREE &KeyList, C_RB_LIST &ColValues)
{
   C_BLONG             *pKey = (C_BLONG *) KeyList.go_top();
   long                Qty = 0, j;
   presbuf             rbField, pOperat, pPerc, pVal, pPrevVal;
   TCHAR               *pName;
   C_RB_LIST           OldColValues;
   C_PREPARED_CMD_LIST TempOldCmdList, TempOldRelCmdList;
   int                 WhyNotUpd;

   if (gsc_check_op(opModSecondary) == GS_BAD) return GS_BAD;
   if (is_updateable(&WhyNotUpd) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Verifico modificabilità entità madre
   // (provando a bloccarla e ad estrarla se estratta parzialmente e ponendo il
   // record nella tabella temporanea)

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (pCls->prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;
   if (pCls->query_data(key_pri, OldColValues, &TempOldCmdList) == GS_BAD) return GS_BAD;
   if (pCls->upd_data(key_pri, OldColValues,
                     (C_PREPARED_CMD *) TempOldCmdList.get_head()) == GS_BAD) return GS_BAD;
   OldColValues.remove_all();

   // Preparo il comando per accesso ai dati in TEMP e OLD
   if (prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;

   // Preparo comando per accesso alle relazioni in TEMP e OLD
   if (prepare_reldata_where_keyAttrib(TempOldRelCmdList) == GS_BAD) return GS_BAD;

   // Copio le schede e le relazioni dalle tabelle OLD   
   if (CopyDataToTemp(key_pri, (C_PREPARED_CMD *) TempOldCmdList.getptr_at(2)) == GS_BAD)
      return GS_BAD;

   while (pKey)
   {
      if (query_data(pKey->get_key(), OldColValues, 
                     &TempOldCmdList, &TempOldRelCmdList) == GS_BAD)
         { pKey = (C_BLONG *) KeyList.go_next(); continue; }

      j = 0;
      // Modifico i valori dell'entità
      while ((rbField = ColValues.nth(j++)) != NULL)
      {
         pName   = gsc_nth(0, rbField)->resval.rstring;
         pVal    = gsc_nth(1, rbField);
         pOperat = gsc_nth(2, rbField);
         pPerc   = gsc_nth(3, rbField);

         if (pOperat && pOperat->restype == RTSTR) // bisogna eseguire un'operazione
         {
            pPrevVal = OldColValues.CdrAssoc(pName);
            gsc_rbExeMathOp(pPrevVal, pVal, pOperat->resval.rstring,
                            (pPerc->restype == RTT) ? TRUE : FALSE);
         }
         else
            OldColValues.CdrAssocSubst(pName, pVal);
      }

      if (upd_data(key_pri, pKey->get_key(), OldColValues,
                   (C_PREPARED_CMD *) TempOldCmdList.get_head()) == GS_GOOD)
         Qty++;

      pKey = (C_BLONG *) KeyList.go_next();
   }

   return GS_GOOD;
}


/*************************************************************/
/*.doc C_SECONDARY::query_data                    <external> */
/*+                                                                       
  Interroga la scheda della tabella secondaria con chiave nota.
  Parametri:
  long         gs_id;      gs_id del record secondario.
  C_RB_LIST   &ColValues   Lista ((colonna valore)..) risultato
                           della query.
  C_PREPARED_CMD_LIST *pTempOldCmdList;    Lista di comandi per lettura dati OLD  (1 elemento)
                                           e OLD (2 elemento) (in caso di letture multiple);
                                           default = NULL.
  C_PREPARED_CMD_LIST *pTempOldRelCmdList; Lista di comandi per lettura relazioni TEMP (1 elemento)
                                           e OLD (2 elemento) (in caso di letture multiple);
                                           partendo dal codice scheda secondaria;
                                           default = NULL.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.  
-*/  
/*************************************************************/
int C_SECONDARY::query_data(long gs_id, C_RB_LIST &ColValues,
                            C_PREPARED_CMD_LIST *pTempOldCmdList,
                            C_PREPARED_CMD_LIST *pTempOldRelCmdList)

{
   C_STRING            OldLnkTable, TempTable, TempLinkTable;
   C_DBCONNECTION      *pConn;
   C_PREPARED_CMD_LIST TempOldCmdList;
   C_PREPARED_CMD      Cmd;
   int                 TempTableExist, IsRsCloseable, result = GS_BAD, IsInTemp;
   _RecordsetPtr       pRs;
   
   if (gsc_check_op(opQrySecondary) == GS_BAD) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (type != GSInternalSecondaryTable) return GS_BAD;

   // Ricavo la connessione OLE-DB alla tabella TEMP
   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;

   // Verifico l'esistenza della tabella temporanea dei link
   getTempLnkTableRef(TempLinkTable);
   TempTableExist = pConn->ExistTable(TempLinkTable);

   // Se esiste allora verifico se i dati sono già stati copiati nel TEMP
   if (TempTableExist == GS_GOOD)
   {
      if (pTempOldRelCmdList) // utilizzo i comandi preparati
         IsInTemp = gsc_get_reldata(*((C_PREPARED_CMD *) pTempOldRelCmdList->get_head()), 
                                    gs_id, pRs, &IsRsCloseable);
      else
      {
         // Preparo comando per accesso alle relazioni temporanee
         if (prepare_reldata_where_keyAttrib(Cmd, TEMP) == GS_BAD) return GS_BAD;
         IsInTemp = gsc_get_reldata(Cmd, gs_id, pRs, &IsRsCloseable);
      }

      if (IsInTemp == GS_GOOD)
      {
         C_RB_LIST RelColValues;
         int       status;

         if (gsc_DBReadRow(pRs, RelColValues) == GS_BAD)
            { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
         if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

         // se STATUS è nullo viene considerato UNMODIFIED
         if (gsc_rb2Int(RelColValues.CdrAssoc(_T("STATUS")), &status) == GS_BAD) 
            status = UNMODIFIED;
         if (status & ERASED) // secondaria cancellata
            { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }
      }
   }
   else
      IsInTemp = GS_BAD;

   if (IsInTemp == GS_GOOD)
   {
      if (pTempOldCmdList) // utilizzo i comandi preparati
         // lettura dei dati della secondaria dal TEMP
         return gsc_get_data(*((C_PREPARED_CMD *) pTempOldCmdList->get_head()),
                             gs_id, ColValues);
      else
      {
         // Preparo i comandi di lettura dei dati della tab. secondaria dal TEMP
         if (prepare_data(Cmd, TEMP) == GS_BAD) return GS_BAD;      
         // lettura dei dati della secondaria dal temp
         return gsc_get_data(Cmd, gs_id, ColValues);
      }      
   }
   else
   {
      if (pTempOldCmdList) // utilizzo i comandi preparati
         // lettura dei dati della secondaria dall'OLD
         return gsc_get_data(*((C_PREPARED_CMD *) pTempOldCmdList->getptr_at(2)),
                             gs_id, ColValues);
      else
      {
         // Preparo i comandi di lettura dei dati della tab. secondaria dal OLD
         if (prepare_data(Cmd, OLD) == GS_BAD) return GS_BAD;      
         // lettura dei dati della secondaria dal temp
         return gsc_get_data(Cmd, gs_id, ColValues);
      }      
   }
}


/*******************************************************/
/*.doc C_SECONDARY::ins_data                           */
/*+
   Determinato l'identificatore da assegnare al nuovo record, chiama la funzione di 
   inserimento passandogli il buffer <col_values> e tale nuovo identificatore.
   Restituisce il nuovo identificatore.
   Parametri
   int       key_pri;       Codice dell' entita principale
   C_RB_LIST &ColValues;    Buffer da cui ricavare il record da inserire
   long      *gs_id;        Nuovo identificatore del record inserito (output)
   bool      CheckEnt;      Flag; se = TRUE la funzione verifica la modificabilità 
                            dell'entità madre qualora non sia nuova (default = TRUE)
                                                        
   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SECONDARY::ins_data(long key_pri, C_RB_LIST &ColValues, long *gs_id,
                          bool CheckEnt)
{                 
   C_DBCONNECTION *pConn;
   C_STRING       TempTable, TempLinkTable, pathfile;
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();
   C_INFO         *p_info = ptr_info();
   long           new_key;
   int            state, result = GS_BAD, WhyNotUpd;
   C_RB_LIST      LinkList;

   if (gsc_check_op(opInsSecondary) == GS_BAD) return GS_BAD;
   if (is_updateable(&WhyNotUpd) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Verifico modificabilità entità madre se NON si tratta di entità nuova
   // (provando a bloccarla e ad estrarla se estratta parzialmente e ponendo il
   // record nella tabella temporanea)
   if (pCls->is_NewEntity(key_pri) != GS_GOOD && CheckEnt)
   {
      C_PREPARED_CMD_LIST TempOldCmdList;
      C_RB_LIST           _ColValues;

      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (pCls->prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;
      if (pCls->query_data(key_pri, _ColValues, &TempOldCmdList) == GS_BAD) return GS_BAD;
      if (pCls->upd_data(key_pri, _ColValues,
                        (C_PREPARED_CMD *) TempOldCmdList.get_head(),
                        NULL, NO_EXTERN_ACTION) == GS_BAD) return GS_BAD;
   }

   // Ricavo la connessione OLE-DB alla tabella TEMP
   if ((pConn = info.getDBConnection(TEMP)) == NULL) return GS_BAD;

   // Copio le schede e le relazioni dalle tabelle OLD   
   if (CopyDataToTemp(key_pri) == GS_BAD) return GS_BAD;

   if (getTempTableRef(TempTable) == GS_BAD) return GS_BAD; // Se non c'è la creo

   // Verifico l' esistenza della tabella dei link temporanea,
   if (getTempLnkTableRef(TempLinkTable) == GS_BAD) return GS_BAD;
   // se non esiste allora creo la tabella dei link TEMPORANEA con 
   // aggiunta del campo STATUS (1° parametro TRUE)
   if (pConn->ExistTable(TempLinkTable) == GS_BAD)
      if (create_tab_link(TRUE, TEMP) == GS_BAD) return GS_BAD;

   // Ricavo codice per prossimo inserimento
   if ((new_key = GetNewSecCode()) >= 0) return GS_BAD;

   // Modifico (key_attrib)
   if (ColValues.CdrAssocSubst(info.key_attrib.get_name(), new_key) == GS_BAD)
      return GS_BAD;

   // validazione e ricalcolo dati
   if (CalcValidData(ColValues, INSERT) == GS_BAD) return GS_BAD;

   int IsTransactionSupported = pConn->BeginTrans();
   do
   {
      // Aggiungo la riga nella tabella secondaria TEMPORANEA o APPOGGIO (MORETEST di default)
      if (pConn->InsRow(TempTable.get_name(), ColValues) != GS_GOOD) break;

      // Poichè il file delle relazioni temporanee fa fede per sapere quali secondarie
      // salvare (come GEOsimAppl::SAVE_SS per le classi con rappresentazione grafica diretta)
      // riporto anche il il bit n. 6 (100000 = 32 = NOSAVE)
      state = (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD) ? NOSAVE : 0;
      if ((LinkList << acutBuildList(RTLB,
                                     RTLB, RTSTR, _T("CLASS_ID"), RTLONG, pCls->ptr_id()->code, RTLE,
                                     RTLB, RTSTR, _T("SUB_CL_ID"), RTLONG, pCls->ptr_id()->sub_code, RTLE,
                                     RTLB, RTSTR, _T("KEY_PRI"), RTLONG, key_pri, RTLE,
                                     RTLB, RTSTR, _T("KEY_ATTRIB"), RTLONG, new_key, RTLE,
                                     RTLB, RTSTR, _T("STATUS"), RTSHORT, INSERTED | state, RTLE,
                                     RTLE, 0)) == NULL)
         break;
      // Aggiungo la riga nella tabella secondaria TEMPORANEA o APPOGGIO (MORETEST di default)
      if (pConn->InsRow(TempLinkTable.get_name(), LinkList) != GS_GOOD) break;

      info.TempLastId = new_key;

      // Salvo modifica in CLASS.GEO
      GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile, ptr_class()->ptr_id()->code);
      if (ToFile(pathfile) == GS_BAD) break;

      result = GS_GOOD;
   }
   while (0);

   if (result != GS_GOOD)
   {
      if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans();
      return GS_BAD;
   }

   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();
   if (gs_id) *gs_id = new_key;

   return GS_GOOD;
}


/******************************************************/
/*.doc C_SECONDARY::del_data
/*+
   Marca come cancellata la scheda secondaria avente identificatore <gs_id>
   aggiornando il campo STATUS ad "ERASED" per il relativo record 
   nella tabella dei link.
   Parametri:
   long key_pri;  Codice dell' entita principale
   long gs_id;    Identificatore del record da cancellare
                                                        
   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/******************************************************/
int C_SECONDARY::del_data(long key_pri, long gs_id)
{                 
   C_DBCONNECTION *pConn;
   C_PREPARED_CMD Cmd;
   int            status, dummy, IsRsCloseable, WhyNotUpd;
   C_RB_LIST      RelColValues;
   _RecordsetPtr  pRs;

   if (gsc_check_op(opDelSecondary) == GS_BAD) return GS_BAD;
   if (is_updateable(&WhyNotUpd) == GS_BAD)
      { if (WhyNotUpd != eGSUnknown) GS_ERR_COD = WhyNotUpd; return GS_BAD; }

   // Verifico modificabilità entità madre se NON si tratta di entità nuova
   // (provando a bloccarla e ad estrarla se estratta parzialmente e ponendo il
   // record nella tabella temporanea)
   if (pCls->is_NewEntity(key_pri) != GS_GOOD)
   {
      C_PREPARED_CMD_LIST TempOldCmdList;
      C_RB_LIST           _ColValues;

      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (pCls->prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;
      if (pCls->query_data(key_pri, _ColValues, &TempOldCmdList) == GS_BAD) return GS_BAD;
      if (pCls->upd_data(key_pri, _ColValues,
                        (C_PREPARED_CMD *) TempOldCmdList.get_head()) == GS_BAD) return GS_BAD;
   }

   // Ricavo la connessione OLE-DB alla tabella TEMP
   if ((pConn = info.getDBConnection(TEMP)) == NULL) return GS_BAD;

   // Copio le schede e le relazioni dalle tabelle OLD   
   if (CopyDataToTemp(key_pri) == GS_BAD) return GS_BAD;

   // Preparo comando per accesso alle relazioni temporanee
   if (prepare_reldata_where_keyAttrib(Cmd, TEMP) == GS_BAD) return GS_BAD;
   if (gsc_get_reldata(Cmd, gs_id, pRs, &IsRsCloseable) == GS_BAD) return GS_BAD;

   // Si rende <UNMODIFIED> (ovvero da non considerarsi) il link relativo
   // ad un record inserito e cancellato durante una stessa sessione 
   // (prima di dare il CLOSE)
   if (gsc_DBReadRow(pRs, RelColValues) == GS_BAD)
      { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }

   // se STATUS è nullo viene considerato UNMODIFIED
   if (gsc_rb2Int(RelColValues.CdrAssoc(_T("STATUS")), &status) == GS_BAD) 
      status = UNMODIFIED;

   dummy = ERASED;
   // Se non era già da salvare verifico la variabile "AddEntityToSaveSet"
   if (status & NOSAVE)
      // Poichè il file delle relazioni temporanee fa fede per sapere quali secondarie
      // salvare (come GEOsimAppl::SAVE_SS per le classi con rappresentazione grafica diretta)
      // riporto anche il il bit n. 6 (100000 = 32 = NOSAVE)
      if (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_BAD)
         dummy = dummy | NOSAVE;

   RelColValues << acutBuildList(RTLB, 
                                 RTLB, RTSTR, _T("STATUS"), RTSHORT, dummy, RTLE,
                                 RTLE, 0);

   if (gsc_DBUpdRow(pRs, RelColValues) == GS_BAD)
      { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }

   if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
   
   return GS_GOOD;
}


/****************************************************************************/
/*.doc C_SECONDARY::synchronize                                   <external> */
/*+
  Questa funzione sincronizza i database con quello della classe madre.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int C_SECONDARY::synchronize(void)
{
   C_STRING       FldName, statement, LinkTableRef;
   C_RB_LIST      ColValues, RelColValues;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs, pRelRs, pClsRs;
   presbuf        pKey = NULL, pKeyPri = NULL;
   long           Key, KeyPri, qty;
   int            IsRelRsCloseable, IsClsRsCloseable, IsRsCloseable;
   C_PREPARED_CMD RelCmd, ClsCmd, SecCmd;
   TCHAR          Msg[MAX_LEN_MSG];
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(410), get_name()); // "Sincronizzazione tabella secondaria <%s>"
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(442), 1, 2); // "Fase %d/%d..."
   C_STATUSLINE_MESSAGE StatusLineMsg2(gsc_msg(442), 2, 2); // "Fase %d/%d..."

   // verifico abilitazione
   if (gsc_check_op(opSynchronizeClass) == GS_BAD) return GS_BAD;

   if (type != GSInternalSecondaryTable) { GS_ERR_COD = eGSInvSecType; return GS_BAD; }

   StatusBarProgressMeter.Init(2);
   StatusBarProgressMeter.Set(1);
   StatusLineMsg.Init(gsc_msg(390), LITTLE_STEP); // ogni 10 "%ld schede secondarie elaborate."

   swprintf(Msg, MAX_LEN_MSG, _T("Synchronization secondary table %d, prj %d, class %d, sub %d started."),
            code, ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
   gsc_write_log(Msg);

   // Preparo i comandi di lettura dei dati della classe dal old
   if (pCls->prepare_data(ClsCmd, OLD) == GS_BAD) return GS_BAD;

   // sincronizzazione database
   if ((pConn = info.getDBConnection(OLD)) == NULL) return GS_BAD;

   if (prepare_reldata_where_keyAttrib(RelCmd, OLD) == GS_BAD) return GS_BAD;

   FldName = info.key_attrib;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   statement = _T("SELECT ");
   statement += FldName;
   statement += _T(" FROM ");
   statement += info.OldTableRef;
   statement += _T(" WHERE ");
   statement += FldName;
   statement += _T("<>0 OR ");
   statement += FldName;
   statement += _T(" IS NULL");

   // creo record-set
   // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
   // in una transazione fa casino (al secondo recordset che viene aperto)
   if (pConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD)
      return GS_BAD;
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   pKey = ColValues.CdrAssoc(info.key_attrib.get_name());

   // per ogni record della tabella secondaria verifico il collegamento alla classe madre
   qty = 0;
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      gsc_DBReadRow(pRs, ColValues);
      if (gsc_rb2Lng(pKey, &Key) == GS_BAD)
      { 
         // Segnalo l'errore su file .LOG e a video
         swprintf(Msg, MAX_LEN_MSG, _T("Wrong link between secondary data and class entity: prj %d, class %d, sub %d."),
                  ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
         gsc_write_log(Msg);
         // "\nCollegamento tra scheda secondaria e principale non più valido.\n"
         acutPrintf(gsc_msg(414));

         gsc_DBDelRow(pRs);
         gsc_Skip(pRs);
         
         continue;
      }

      // Attraverso la relazione leggo il codice della principale
      if (gsc_get_data(RelCmd, Key, pRelRs, &IsRelRsCloseable) == GS_BAD)
      { 
         // Segnalo l'errore su file .LOG e a video
         swprintf(Msg, MAX_LEN_MSG, _T("Wrong link between secondary data and class entity: id_sec %ld, prj %d, class %d, sub %d."),
                  Key, ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
         gsc_write_log(Msg);
         // "\nCollegamento tra scheda secondaria e principale non più valido per la scheda secondaria: %ld.\n"
         acutPrintf(gsc_msg(415), Key);

         gsc_DBDelRow(pRs);
         gsc_Skip(pRs);
         
         continue;
      }
      gsc_DBReadRow(pRelRs, RelColValues);

      if (!pKeyPri)
         pKeyPri = RelColValues.CdrAssoc(_T("KEY_PRI"));

      if (gsc_rb2Lng(pKeyPri, &KeyPri) == GS_BAD)
      { 
         // Segnalo l'errore su file .LOG e a video
         swprintf(Msg, MAX_LEN_MSG, _T("Wrong link between secondary data and class entity: id_sec %ld, prj %d, class %d, sub %d."),
                  Key, ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
         gsc_write_log(Msg);
         // "\nCollegamento tra scheda secondaria e principale non più valido per la scheda secondaria: %ld.\n"
         acutPrintf(gsc_msg(415), Key);

         gsc_DBDelRow(pRelRs);
         if (IsRelRsCloseable == GS_GOOD) gsc_DBCloseRs(pRelRs);
         gsc_DBDelRow(pRs);
         gsc_Skip(pRs);
         
         continue;
      }

      // leggo record classe madre
      if (gsc_get_data(ClsCmd, KeyPri, pClsRs, &IsClsRsCloseable) == GS_BAD)
      { 
         // Segnalo l'errore su file .LOG e a video
         swprintf(Msg, MAX_LEN_MSG, _T("Wrong link between secondary data and class entity: id_sec %ld, prj %d, class %d, sub %d."),
                  Key, ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
         gsc_write_log(Msg);
         // "\nCollegamento tra scheda secondaria e principale non più valido per la scheda secondaria: %ld.\n"
         acutPrintf(gsc_msg(415), Key);

         gsc_DBDelRow(pRelRs);
         if (IsRelRsCloseable == GS_GOOD) gsc_DBCloseRs(pRelRs);
         gsc_DBDelRow(pRs);
         gsc_Skip(pRs);
         
         continue;
      }

      // leggo record tabella secondaria
      if (gsc_get_data(ClsCmd, KeyPri, pClsRs, &IsClsRsCloseable) == GS_BAD)
      { 
         // Segnalo l'errore su file .LOG e a video
         swprintf(Msg, MAX_LEN_MSG, _T("Wrong link between secondary data and class entity: id_sec %ld, prj %d, class %d, sub %d."),
                 Key, ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
         gsc_write_log(Msg);
         // "\nCollegamento tra scheda secondaria e principale non più valido per la scheda secondaria: %ld.\n"
         acutPrintf(gsc_msg(415), Key);

         gsc_DBDelRow(pRelRs);
         if (IsRelRsCloseable == GS_GOOD) gsc_DBCloseRs(pRelRs);
         gsc_DBDelRow(pRs);
         gsc_Skip(pRs);
         
         continue;
      }

      if (IsRelRsCloseable == GS_GOOD) gsc_DBCloseRs(pRelRs);
      if (IsClsRsCloseable == GS_GOOD) gsc_DBCloseRs(pClsRs);

      gsc_Skip(pRs);

      StatusLineMsg.Set(++qty); // "%ld schede secondarie elaborate."
   }
   gsc_DBCloseRs(pRs);

   StatusLineMsg.End(gsc_msg(390), qty); // "%ld schede secondarie elaborate."

   //////////////////////////////////
   StatusBarProgressMeter.Set(2);
   StatusLineMsg2.Init(gsc_msg(390), LITTLE_STEP); // ogni 10 "%ld schede secondarie elaborate."

   // Preparo i comandi di lettura dei dati della tabella secondaria
   if (prepare_data(SecCmd, OLD) == GS_BAD) return GS_BAD;

   if (getOldLnkTableRef(LinkTableRef) == GS_BAD) return GS_BAD;
   statement = _T("SELECT * FROM ");
   statement += LinkTableRef;

   // creo record-set
   // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
   // in una transazione fa casino (al secondo recordset che viene aperto)
   if (pConn->OpenRecSet(statement, pRelRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD)
      return GS_BAD;
   if (gsc_InitDBReadRow(pRelRs, RelColValues) == GS_BAD) { gsc_DBCloseRs(pRelRs); return GS_BAD; }
   pKeyPri = RelColValues.CdrAssoc(_T("KEY_PRI"));
   pKey    = RelColValues.CdrAssoc(_T("KEY_ATTRIB"));

   acutPrintf(gsc_msg(559), get_name()); // "\nSincronizzazione link tabella secondaria <%s>:\n"
   qty = 0;
   while (gsc_isEOF(pRelRs) == GS_BAD)
   {
      gsc_DBReadRow(pRelRs, RelColValues);
      if (gsc_rb2Lng(pKey, &Key) == GS_BAD || gsc_rb2Lng(pKeyPri, &KeyPri) == GS_BAD)
      { 
         // Segnalo l'errore su file .LOG e a video
         swprintf(Msg, MAX_LEN_MSG, _T("Wrong link between secondary data and class entity: prj %d, class %d, sub %d."),
                  ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
         gsc_write_log(Msg);
         // "\nCollegamento tra scheda secondaria e principale non più valido.\n"
         acutPrintf(gsc_msg(414));

         gsc_DBDelRow(pRelRs);
         gsc_Skip(pRelRs);
         
         continue;
      }

      // leggo record classe madre
      if (gsc_get_data(ClsCmd, KeyPri, pClsRs, &IsClsRsCloseable) == GS_BAD)
      { 
         // Segnalo l'errore su file .LOG e a video
         swprintf(Msg, MAX_LEN_MSG, _T("Wrong link between secondary data and class entity: id_sec %ld, prj %d, class %d, sub %d."),
                  Key, ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
         gsc_write_log(Msg);
         // "\nCollegamento tra scheda secondaria e principale non più valido per la scheda secondaria: %ld.\n"
         acutPrintf(gsc_msg(415), Key);

         gsc_DBDelRow(pRelRs);
         gsc_Skip(pRelRs);
         
         continue;
      }
      if (IsClsRsCloseable == GS_GOOD) gsc_DBCloseRs(pClsRs);

      // leggo record tabella secondaria
      if (gsc_get_data(SecCmd, Key, pRs, &IsRsCloseable) == GS_BAD)
      { 
         // Segnalo l'errore su file .LOG e a video
         swprintf(Msg, MAX_LEN_MSG, _T("Wrong link between secondary data and class entity: id_sec %ld, prj %d, class %d, sub %d."),
                  Key, ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(), pCls->ptr_id()->code, pCls->ptr_id()->sub_code);
         gsc_write_log(Msg);
         // "\nCollegamento tra scheda secondaria e principale non più valido per la scheda secondaria: %ld.\n"
         acutPrintf(gsc_msg(415), Key);

         gsc_DBDelRow(pRelRs);
         gsc_Skip(pRelRs);
         
         continue;
      }
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

      gsc_Skip(pRelRs);

      StatusLineMsg2.Set(++qty); // "%ld schede secondarie elaborate."
   }
   gsc_DBCloseRs(pRelRs);

   acutPrintf(gsc_msg(390), qty); // "%ld schede secondarie elaborate."

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc C_SECONDARY::data_to_html                                             */
/*+
  Questa funzione esporta il contenuto della scheda in un file HTML.
  Parametri
  C_STRING &Path;                          Path completa file html
  long gs_id;                              Codice scheda secondaria
  C_PREPARED_CMD_LIST *pTempOldCmdList;    Lista di comandi per lettura dati OLD  (1 elemento)
                                           e OLD (2 elemento) (in caso di letture multiple);
                                           default = NULL.
  C_PREPARED_CMD_LIST *pTempOldRelCmdList; Lista di comandi per lettura relazioni TEMP (1 elemento)
                                           e OLD (2 elemento) (in caso di letture multiple);
                                           partendo dal codice scheda secondaria;
                                           default = NULL.
  C_STR_LIST *PrintableAttrNameList;       Lista con i nomi dei soli attributi da stampare
                                           (default = NULL cioè tutti gli attributi)
  const TCHAR *Mode;                       Modo di apertura del file html ("w" distrugge 
                                           il contenuto, "a" aggiunge al contenuto)
                                           (default = _T("w"))
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/************************************************************************************************************/
int C_SECONDARY::data_to_html(const TCHAR *Path, long gs_id, 
                              C_PREPARED_CMD_LIST *pTempOldCmdList,
                              C_PREPARED_CMD_LIST *pTempOldRelCmdList,
                              C_STR_LIST *PrintableAttrNameList,
                              const TCHAR *Mode)
{
   C_STRING      TitleBorderColor("#808080"), TitleBgColor("#c0c0c0");
   C_STRING      BorderColor("#00CCCC"), BgColor("#99FFFF");
   C_STRING      Msg, PrjName, ClassName, FieldName, _Mode(Mode);
   C_ATTRIB_LIST *pAttribList;
   C_ATTRIB      *pAttrib;
   presbuf       pName, pVal;
   int           i = 0, Result = GS_BAD;
   FILE          *file;
   C_RB_LIST     ColValues;

   if (!(pAttribList = ptr_attrib_list())) return GS_BAD;
   if (query_data(gs_id, ColValues, pTempOldCmdList, pTempOldRelCmdList) == GS_BAD) 
      return GS_BAD;

   PrjName = ((C_PROJECT *) ptr_class()->ptr_id()->pPrj)->get_name();
   ptr_class()->get_CompleteName(ClassName);

   PrjName.toHTML();
   ClassName.toHTML();

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

      Msg = name;
      Msg.toHTML();
      // Tabella secondaria:
      if (fwprintf(file, _T("<br>Tabella secondaria: %s"), Msg.get_name()) < 0)
         break;

      if (fwprintf(file, _T("</font></b></td></tr></table><br>")) < 0)
         break;
      
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
         pAttrib->ParseToString(pVal, Msg, &ColValues, 
                                ptr_class()->ptr_id()->code, 
                                ptr_class()->ptr_id()->sub_code,
                                code);
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

      if (_Mode.comp(_T("w")) == 0) // creazione html
         // fine html
         if (fwprintf(file, _T("\n</body></html>")) < 0) break;

      Result = GS_GOOD;
   }
   while (0);

   gsc_fclose(file);

   return Result;
}


/****************************************************************************/
/*.doc C_SECONDARY::reportHTML                                   <external> */
/*+
  Questa funzione stampa su un file HTML i dati della C_SECONDARY.
  FILE *file;        Puntatore a file
  bool SynthMode;    Opzionale. Flag di modalità di report.
                     Se = true attiva il report sintetico (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/******************************************************************************/
int C_SECONDARY::reportHTML(FILE *file, bool SynthMode)
{
   C_STRING Buffer, StrType, StrCategory;
   C_STRING TitleBorderColor("#808080"), TitleBgColor("#c0c0c0");
   C_STRING BorderColor("#00CCCC"), BgColor("#99FFFF");

   if (fwprintf(file, _T("\n<br><br><table bordercolor=\"%s\" bgcolor=\"%s\" width=\"100%%\" border=\"1\">"),
                TitleBorderColor.get_name(), TitleBgColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Dati Identificativi Tabella secondaria"
   if (fwprintf(file, _T("\n<tr><td align=\"center\"><b><font size=\"4\">%s<br><u>%s</u></font></b></td></tr></table><br>"),
                gsc_msg(294), name) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // intestazione tabella
   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" cellspacing=\"2\" cellpadding=\"2\" border=\"1\">"), 
                BorderColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Nome"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(628), name) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (!SynthMode)
   {
      // "Descrizione"
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                   BgColor.get_name(), gsc_msg(794),
                   (Descr.len() > 0) ? Descr.get_name() : _T("&nbsp;")) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      // "Progetto"
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\" width=\"30%%\"><b>%s:</b></td><td width=\"70%%\">%s</td></tr>"),
                   BgColor.get_name(), gsc_msg(326), ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_name()) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      // "Classe"
      pCls->get_CompleteName(Buffer);
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\" width=\"30%%\"><b>%s:</b></td><td width=\"70%%\">%s</td></tr>"),
                   BgColor.get_name(), gsc_msg(297), Buffer.get_name()) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

      // "Codice"
      if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%d</td></tr>"),
                   BgColor.get_name(), gsc_msg(624), code) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }

   if (gsc_getSecTypeDescr(type, StrType) == GS_BAD) StrType = GS_EMPTYSTR;

   // "Tipo"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(627), StrType.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // fine tabella
   if (fwprintf(file, _T("\n</table><br><br>")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   
   if (info.reportHTML(file, SynthMode) == GS_BAD) return GS_BAD;

   if (attrib_list.reportHTML(file, ((C_PROJECT *) pCls->ptr_id()->pPrj)->get_key(),
                              pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                              code, SynthMode) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/**************************************************************/
/*.doc C_SECONDARY::CopySupportFiles                          */
/*+
   Funzione che copia i files di supporto per lista di valori degli 
   attributi per renderli disponibili per una altra tabella secondaria.
   Parametri:
   C_SECONDARY *pDstCls;   puntatore alla tabella secondaria di destinazione

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int C_SECONDARY::CopySupportFiles(C_SECONDARY *pDstSec)
{
   return pDstSec->ptr_attrib_list()->CopySupportFiles(pCls->get_PrjId(),         // progetto
                                                       pCls->ptr_id()->code,      // classe
                                                       pCls->ptr_id()->sub_code,  // sottoclasse
                                                       code,                      // sec
                                                       pDstSec->pCls->get_PrjId(),
                                                       pDstSec->pCls->ptr_id()->code,
                                                       pDstSec->pCls->ptr_id()->sub_code,
                                                       pDstSec->code);
}


/**************************************************************/
/*.doc C_SECONDARY::DelSupportFiles                           */
/*+
   Funzione che cancella i files di supporto per lista di valori degli 
   attributi di una tabella secondaria.
   Parametri:

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/**************************************************************/
int C_SECONDARY::DelSupportFiles(void)
{
   return ptr_attrib_list()->DelSupportFiles(pCls->get_PrjId(),          // progetto
                                             pCls->ptr_id()->code,       // classe
                                             pCls->ptr_id()->sub_code,   // sottoclasse
                                             code);                      // sec
}


/*********************************************************/
/*  FINE   FUNZIONI DI C_SECONDARY                       */
/*  INIZIO FUNZIONI DI C_SECONDARY_LIST                  */
/*********************************************************/

C_SECONDARY_LIST::C_SECONDARY_LIST(C_CLASS *pCls) : C_LIST()
{
   mother_class = pCls;
}

C_CLASS *C_SECONDARY_LIST::get_mother_class(void)
   { return mother_class; }

int C_SECONDARY_LIST::set_mother_class(C_NODE *p_class)
{
   mother_class = (C_CLASS *) p_class;
   return GS_GOOD;
}

/****************************************************************************/
/*.doc C_SECONDARY_LIST:reportHTML                               <external> */
/*+
  Questa funzione stampa su un file HTML i dati della C_SECONDARY_LIST.
  Parametri:
  FILE *file;     Puntatore a file
  bool SynthMode; Opzionale. Flag di modalità di report.
                  Se = true attiva il report sintetico (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_SECONDARY_LIST::reportHTML(FILE *file, bool SynthMode)
{
   C_SECONDARY *pSec = (C_SECONDARY *) get_head();

   while (pSec)
   {
      if (pSec->reportHTML(file, SynthMode) == GS_BAD) return GS_BAD;
      pSec = (C_SECONDARY *) get_next();
   }

   return GS_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
// SEGMENTAZIONE DINAMICA   -  INIZIO
/////////////////////////////////////////////////////////////////////////////


/****************************************************************************/
/*.doc gsc_get_dynamic_segmentation_intersections                <external> */
/*+
  Questa funzione riceve due segmenti ognuno dei quali è identificato
  da una distanza dall'origine della linea e una lunghezza.
  Restituisce un segmento contenente il tratto di segmento in comune tra i due
  (intersezione).
  Parametri:
  C_2REAL *pSegment1;  Prima segmento
  C_2REAL *pSegment2;  Secondo segmento

  Restituisce il segmento di intersezione in caso di successo altrimenti restituisce NULL. 
  N.B. Alloca memoria
-*/
/****************************************************************************/
C_2REAL *gsc_get_dynamic_segmentation_intersections(C_2REAL *pSegment1, C_2REAL *pSegment2)
{
   C_2REAL *pResult = NULL;
   double DistanceFromStart1, Length1, DistanceFromStart2, Length2, DistanceFromStart3, Length3;

   DistanceFromStart1 = pSegment1->get_key_double();
   Length1            = pSegment1->get_key_2_double();

   DistanceFromStart2 = pSegment2->get_key_double();
   Length2            = pSegment2->get_key_2_double();

   if (DistanceFromStart1 + Length1 < DistanceFromStart2) return NULL;
   if (DistanceFromStart2 + Length2 < DistanceFromStart1) return NULL;

   DistanceFromStart3 = max(DistanceFromStart1, DistanceFromStart2);
   if (DistanceFromStart1 + Length1 < DistanceFromStart2 + Length2)
      Length3 = (DistanceFromStart1 + Length1) - DistanceFromStart3;
   else
      Length3 = (DistanceFromStart2 + Length2) - DistanceFromStart3;

   if ((pResult = new C_2REAL(DistanceFromStart3, Length3)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   return pResult;
}


/****************************************************************************/
/*.doc gsc_get_dynamic_segmentation_intersections                <external> */
/*+
  Questa funzione riceve due liste di segmenti ognuno dei quali è identificato
  da una distanza dall'origine della linea e una lunghezza.
  Restituisce una lista contenente i tratti di segmenti in comune tra le due liste
  (intersezione).
  Parametri:
  C_2REAL_LIST &List1;  Prima lista di segmenti
  C_2REAL_LIST &List2;  Seconda lista di segmenti
  C_2REAL_LIST &Result; Lista dei tratti di segmenti in comune tra le 2 liste

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int gsc_get_dynamic_segmentation_intersections(C_2REAL_LIST &List1, C_2REAL_LIST &List2,
                                               C_2REAL_LIST &Result)
{
   C_2REAL *pSegment1 = (C_2REAL *) List1.get_head();
   C_2REAL *pSegment2, IntersectionSegment, *pNewSegment;

   Result.remove_all();
   while (pSegment1)
   {
      pSegment1->copy(&IntersectionSegment);
      pSegment2 = (C_2REAL *) List2.get_head();

      while (pSegment2)
      {
         if ((pNewSegment = gsc_get_dynamic_segmentation_intersections(&IntersectionSegment, pSegment2)) == NULL)
            break;
         IntersectionSegment.set_key(pNewSegment->get_key_double());
         IntersectionSegment.set_key_2(pNewSegment->get_key_2_double());
         delete pNewSegment;

         pSegment2 = (C_2REAL *) List2.get_next();
      }

      if (pSegment2 == NULL) // è uscito dal ciclo correttamente
         Result.add_tail_2double(IntersectionSegment.get_key_double(),
                                 IntersectionSegment.get_key_2_double());

      pSegment1 = (C_2REAL *) List1.get_next();
   }

   return GS_GOOD;
}

///////////////////////////////////////////////////////////////////////////////
// GESTIONE DCL PER SEGMENTAZIONE DINAMICA  -  INIZIO
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gsc_setTileDclDynSegmentationAnalyzer                                 */
/*+
  Questa funzione setta tutti i controlli della DCL "DynSegmentationAnalyzer" in modo 
  appropriato secondo il tipo di selezione.
  Parametri:
  ads_hdlg dcl_id;     
  Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static int gsc_setTileDclDynSegmentationAnalyzer(ads_hdlg dcl_id, 
                                                 Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct)
{
   C_STRING        JoinOp, Boundary, SelType, LocSummary, Trad, StrPos;
   C_RB_LIST       CoordList;
   bool            Inverse;
   C_SINTH_SEC_TAB *pSinthSec;
   int             i;

   // scompongo la condizione impostata
   if (gsc_SplitSpatialSel(CommonStruct->SpatialSel, JoinOp, &Inverse,
                           Boundary, SelType, CoordList) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   // Traduzioni per SelType
   if (gsc_trad_SelType(SelType, Trad) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   LocSummary = Trad;
   LocSummary += _T(" ");
   // Traduzioni per Boundary
   if (gsc_trad_Boundary(Boundary, Trad) == GS_BAD)
   {
      ads_set_tile(dcl_id, _T("error"), gsc_err(eGSBadLocationQry)); // "*Errore* Condizione spaziale non valida"
      return GS_BAD;
   }
   LocSummary += Trad;
   ads_set_tile(dcl_id, _T("Location_Descr"), LocSummary.get_name());

   // Location use
   if (CommonStruct->LocationUse)
   {
      ads_set_tile(dcl_id, _T("LocationUse"), _T("1"));
      ads_mode_tile(dcl_id, _T("Location"), MODE_ENABLE); 
   }
   else
   {
      ads_set_tile(dcl_id, _T("LocationUse"), _T("0"));
      ads_mode_tile(dcl_id, _T("Location"), MODE_DISABLE); 
   }

   // logical operator
   switch (CommonStruct->Logical_op)
   {
      case UNION:
         ads_set_tile(dcl_id, _T("Logical_op"), _T("Union"));
         break;
      case SUBTRACT_A_B:
         ads_set_tile(dcl_id, _T("Logical_op"), _T("Subtr_Location_SSfilter"));
         break;
      case SUBTRACT_B_A:
         ads_set_tile(dcl_id, _T("Logical_op"), _T("Subtr_SSfilter_Location"));
         break;
      case INTERSECT:
         ads_set_tile(dcl_id, _T("Logical_op"), _T("Intersect"));
         break;
   }

   // LastFilterResult use
   if (CommonStruct->LastFilterResultUse)
      ads_set_tile(dcl_id,  _T("SSFilterUse"),  _T("1"));
   else
      ads_set_tile(dcl_id,  _T("SSFilterUse"),  _T("0"));

   // controllo su operatori logici (abilitato solo se è stata scelta una area 
   // tramite "Location" ed è stato scelto anche l'uso di LastFilterResult)
   if (CommonStruct->LocationUse && Boundary.len() > 0 && CommonStruct->LastFilterResultUse)
      ads_mode_tile(dcl_id,  _T("Logical_op"), MODE_ENABLE); 
   else
      ads_mode_tile(dcl_id,  _T("Logical_op"), MODE_DISABLE); 

   // Nome Tabella Secondaria
   pSinthSec = (C_SINTH_SEC_TAB *) CommonStruct->SinthSecList.get_head();
   ads_start_list(dcl_id,  _T("SecTable"), LIST_NEW, 0);
   i = 0;
   while (pSinthSec)
   {
      if (pSinthSec->get_key() == CommonStruct->Sec) StrPos = i;
      gsc_add_list(pSinthSec->get_name());
      pSinthSec = (C_SINTH_SEC_TAB *) CommonStruct->SinthSecList.get_next();
      i++;
   }
   ads_end_list();
   if (StrPos.len() > 0) ads_set_tile(dcl_id, _T("SecTable"), StrPos.get_name());

   // Condizione SQL Secondaria
   if (CommonStruct->SQLCond.len() > 0) ads_set_tile(dcl_id, _T("editsql"), CommonStruct->SQLCond.get_name());
   else ads_set_tile(dcl_id, _T("editsql"), GS_EMPTYSTR);

   // keyConvertingFas
   if (CommonStruct->FASConvertion)
   {
      ads_set_tile(dcl_id,  _T("keyConvertingFas"),  _T("1"));
      ads_mode_tile(dcl_id, _T("keyFas"), MODE_ENABLE); 
   }
   else
   {
      ads_set_tile(dcl_id,  _T("keyConvertingFas"),  _T("0"));
      ads_mode_tile(dcl_id, _T("keyFas"), MODE_DISABLE); 
   }

   // label
   C_SECONDARY *pSec;
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls,
                            CommonStruct->Sub, CommonStruct->Sec)) == NULL)
      return GS_BAD;
   CommonStruct->SecAttribNameList.remove_all();
   C_ATTRIB *pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      CommonStruct->SecAttribNameList.add_tail_str(pAttrib->get_name());
      pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->get_next();
   }
   CommonStruct->SecAttribNameList.sort_name();

   C_STR *pAttribName = (C_STR *) CommonStruct->SecAttribNameList.get_head();
   ads_start_list(dcl_id,  _T("keyLblFun"), LIST_NEW, 0);
   i = 0;
   while (pAttribName)
   {
      if (CommonStruct->LblFun.comp(pAttribName->get_name(), FALSE) == 0) StrPos = i;
      gsc_add_list(pAttribName->get_name());
      pAttribName = (C_STR *) CommonStruct->SecAttribNameList.get_next();
      i++;
   }
   ads_end_list();
   if (StrPos.len() > 0) ads_set_tile(dcl_id, _T("keyLblFun"), StrPos.get_name());

   if (CommonStruct->LblCreation)
   {
      ads_set_tile(dcl_id,  _T("keyLblCreation"),  _T("1"));
      ads_mode_tile(dcl_id, _T("keyLblFun"), MODE_ENABLE); 
      ads_mode_tile(dcl_id, _T("keyLblFas"), MODE_ENABLE); 
   }
   else
   {
      ads_set_tile(dcl_id,  _T("keyLblCreation"),  _T("0"));
      ads_mode_tile(dcl_id, _T("keyLblFun"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("keyLblFas"), MODE_DISABLE); 
   }

   // AddObjectData
   if (CommonStruct->AddODValues)
      ads_set_tile(dcl_id,  _T("AddObjectData"),  _T("1"));
   else
      ads_set_tile(dcl_id,  _T("AddObjectData"),  _T("0"));

   return GS_GOOD;
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "LocationUse"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_LocationUse(ads_callback_packet *dcl)
{
   TCHAR val[10];

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   if (ads_get_tile(dcl->dialog, _T("LocationUse"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      ((Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data))->LocationUse = true;
   else
      ((Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data))->LocationUse = false;

   gsc_setTileDclDynSegmentationAnalyzer(dcl->dialog, 
                                         (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "Logical_op"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_Logical_op(ads_callback_packet *dcl)
{
   TCHAR val[50];

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   if (ads_get_tile(dcl->dialog, _T("Logical_op"), val, 50) == RTNORM)
   {
      // case insensitive
      if (gsc_strcmp(val, _T("Union")) == 0)
         ((Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data))->Logical_op = UNION;
      else
      if (gsc_strcmp(val, _T("Subtr_Location_SSfilter")) == 0)
         ((Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data))->Logical_op = SUBTRACT_A_B;
      else
      if (gsc_strcmp(val, _T("Subtr_SSfilter_Location")) == 0)
         ((Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data))->Logical_op = SUBTRACT_B_A;
      else
      if (gsc_strcmp(val, _T("Intersect")) == 0)
         ((Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data))->Logical_op = INTERSECT;
   }
}
void dcl_DynSegmentationAnalyzer_set_EnabledFas(Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct)
{
   C_SECONDARY *pSec;

   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls, CommonStruct->Sub,
                            CommonStruct->Sec)) == NULL) return;
   switch (pSec->DynSegmentationType())
   {
      case GSLinearDynSegmentation:
         // piano tipo-linea larghezza colore thickness elevazione scala-tipo-linea
         // layer-blocchi-DA
         CommonStruct->EnabledFas = GSLayerSetting + GSLineTypeSetting + GSWidthSetting + 
                                    GSColorSetting + GSThicknessSetting + GSElevationSetting +
                                    GSLineTypeScaleSetting;
         break;
      case GSPunctualDynSegmentation:
            // piano blocco scala-blocco colore elevazione rotazione layer-blocchi-DA
         CommonStruct->EnabledFas = GSLayerSetting + GSBlockNameSetting + GSBlockScaleSetting + 
                                    GSColorSetting + GSElevationSetting + GSRotationSetting;
         break;
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "LastFilterResultUse"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_LastFilterResultUse(ads_callback_packet *dcl)
{
   TCHAR val[10];

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   if (ads_get_tile(dcl->dialog, _T("SSFilterUse"), val, 10) != RTNORM) return;
   
   if (gsc_strcmp(val, _T("1")) == 0)
      ((Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data))->LastFilterResultUse = true;
   else
      ((Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data))->LastFilterResultUse = false;

   gsc_setTileDclDynSegmentationAnalyzer(dcl->dialog, 
                                         (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "SecTable"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_SecTable(ads_callback_packet *dcl)
{
   TCHAR           val[10];
   C_SINTH_SEC_TAB *pSinthSec;
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);
   if (ads_get_tile(dcl->dialog, _T("SecTable"), val, 10) != RTNORM) return;
   
   if ((pSinthSec = (C_SINTH_SEC_TAB *) CommonStruct->SinthSecList.getptr_at(_wtoi(val) + 1)) != NULL)
   {
      CommonStruct->Sec = pSinthSec->get_key();
      dcl_DynSegmentationAnalyzer_set_EnabledFas(CommonStruct);
      gsc_setTileDclDynSegmentationAnalyzer(dcl->dialog, CommonStruct);
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "editsql"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_editsql(ads_callback_packet *dcl)
{
   TCHAR       val[TILE_STR_LIMIT], *err;
   C_SECONDARY *pSec;
   C_INFO_SEC  *pInfo;
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;
   C_DBCONNECTION   *pConn;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);
   if (ads_get_tile(dcl->dialog, _T("editsql"), val, TILE_STR_LIMIT) != RTNORM) return;
   
   // Attributo Sorgente della Tabella Secondaria
   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls,
                            CommonStruct->Sub, CommonStruct->Sec)) == NULL)
      return;
   if ((pInfo = pSec->ptr_info()) == NULL) return;

   // Ricavo la connessione OLE - DB all' OLD
   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return;

   if ((err = pConn->CheckSql(val)) != NULL)
   {  // ci sono errori
      ads_set_tile(dcl->dialog, _T("error"), err);
      free(err);
      return;
   }
   
   // Condizione SQL Secondaria
   CommonStruct->SQLCond = val;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "SecSQL"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_SecSQL(ads_callback_packet *dcl)
{
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;
   C_SECONDARY *pSec;
   C_INFO_SEC  *pInfo;
   C_STRING    str;

   if (!GS_CURRENT_WRK_SESSION) return;

   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);

   if ((pSec = gsc_find_sec(GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls,
                            CommonStruct->Sub, CommonStruct->Sec)) == NULL)
      return;

   if ((pInfo = pSec->ptr_info()) == NULL) return;
   
   str = gsc_msg(309); // "Tabella secondaria: "
   str += pSec->get_name();

   if (gsc_ddBuildQry(str.get_name(), pInfo->getDBConnection(OLD), pInfo->OldTableRef,
                      CommonStruct->SQLCond, GS_CURRENT_WRK_SESSION->get_PrjId(), CommonStruct->Cls,
                      CommonStruct->Sub, CommonStruct->Sec) != GS_GOOD) return;
   gsc_setTileDclDynSegmentationAnalyzer(dcl->dialog, 
                                         (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data));
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "keyConvertingFas"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_keyConvertingFas(ads_callback_packet *dcl)
{
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);
   CommonStruct->FASConvertion = (gsc_strcmp(dcl->value, _T("1")) == 0) ? true : false;

   gsc_setTileDclDynSegmentationAnalyzer(dcl->dialog, CommonStruct);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "keyFas"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_keyFas(ads_callback_packet *dcl)
{
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;
   double  dummyDbl;
   int     dummyInt;

   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);
   gsc_ddChooseGraphSettings(CommonStruct->EnabledFas, false, &(CommonStruct->flag_set), CommonStruct->NewFAS,
                             &dummyDbl, &dummyInt);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "keyLblCreation"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_keyLblCreation(ads_callback_packet *dcl)
{
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);
   CommonStruct->LblCreation = (gsc_strcmp(dcl->value, _T("1")) == 0) ? true : false;

   gsc_setTileDclDynSegmentationAnalyzer(dcl->dialog, CommonStruct);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "keyLblFun"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_keyLblFun(ads_callback_packet *dcl)
{
   TCHAR val[10];
   C_STR *p;
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);
   if (ads_get_tile(dcl->dialog, _T("keyLblFun"), val, 10) != RTNORM) return;
   
   if ((p = (C_STR *) CommonStruct->SecAttribNameList.getptr_at(_wtoi(val) + 1)) != NULL)
      CommonStruct->LblFun = p->get_name();
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "keyLblFas"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_keyLblFas(ads_callback_packet *dcl)
{
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;
   double  dummyDbl;
   int     dummyInt;

   // piano stile-testo colore altezza-testo thickness elevazione rotazione
   long Flag = GSLayerSetting + GSTextStyleSetting + GSColorSetting + 
               GSTextHeightSetting + GSThicknessSetting + GSElevationSetting + 
               GSRotationSetting;

   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);
   gsc_ddChooseGraphSettings(Flag, false, &(CommonStruct->Lbl_flag_set), CommonStruct->LblFAS,
                             &dummyDbl, &dummyInt);
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "AddObjectData"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_AddObjectData(ads_callback_packet *dcl)
{
   Common_Dcl_DynSegmentationAnalyzer_Struct *CommonStruct;

   CommonStruct = (Common_Dcl_DynSegmentationAnalyzer_Struct *) (dcl->client_data);
   CommonStruct->AddODValues = (gsc_strcmp(dcl->value, _T("1")) == 0) ? true : false;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "DynSegmentationAnalyzer" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_DynSegmentationAnalyzer_help(ads_callback_packet *dcl)
{
   gsc_help(IDH_Analisisegmentazionedinamica);
} 


/*****************************************************************************/
/*.doc gsddSecDynamicSegmentationAnalyzer                                    */
/*+
  Questa comando effettua l'analisi della segmentazione dinamica di una
  tabella secondaria di una classe della banca dati caricata nella sessione 
  di lavoro corrente.
-*/  
/*****************************************************************************/
void gsddSecDynamicSegmentationAnalyzer(void)
{
   int             ret = GS_GOOD, status = DLGOK, dcl_file;
   C_LINK_SET      ResultLS;
   bool            UseLS;
   ads_hdlg        dcl_id;
   C_STRING        path;
   C_SINTH_SEC_TAB *pSinthSec;
   C_SECONDARY     *pSec;
   C_CLASS         *pClass;
   C_CLS_PUNT_LIST ClassList;
   Common_Dcl_DynSegmentationAnalyzer_Struct CommonStruct;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION)
      { GS_ERR_COD = eGSNotCurrentSession; return GEOsimAppl::CMDLIST.ErrorCmd(); }   

   if (gsc_check_op(opFilterEntity) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // Ottiene la lista delle classi estratte nella sessione corrente
   // che sono abilitate alla segmentazone dinamica
   if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(ClassList, 0, 0, GS_GOOD) == GS_BAD)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   // seleziona la classe o sottoclasse
   if ((ret = gsc_ddselect_class(NONE, &pClass, NULL, &ClassList)) != GS_GOOD)
      if (ret == GS_CAN) return GEOsimAppl::CMDLIST.CancelCmd();
      else return GEOsimAppl::CMDLIST.ErrorCmd();

   // prima inizializzazione
   CommonStruct.SpatialSel  = _T("\"\" \"\" \"\" \"Location\" (\"all\") \"\"");
   CommonStruct.LocationUse = true;
   CommonStruct.Inverse     = FALSE;
   CommonStruct.LastFilterResultUse = false;
   CommonStruct.Logical_op  = UNION;
   CommonStruct.Cls = pClass->ptr_id()->code;
   CommonStruct.Sub = pClass->ptr_id()->sub_code;

   // leggo la lista delle tabelle secondarie abilitate alla segmentazione dinamica
   if (pClass->get_pPrj()->getSinthClsSecondaryTabList(CommonStruct.Cls, CommonStruct.Sub, CommonStruct.SinthSecList) == GS_BAD)
      return GEOsimAppl::CMDLIST.ErrorCmd();
   CommonStruct.SinthSecList.FilterOnDynSegmentationSupportedOnly();
   
   if ((pSinthSec = (C_SINTH_SEC_TAB *) CommonStruct.SinthSecList.get_head()) == NULL)
      { GS_ERR_COD = eGSNoSecondary; return GEOsimAppl::CMDLIST.ErrorCmd(); }
   CommonStruct.Sec = pSinthSec->get_key();

   CommonStruct.FASConvertion = false;
   dcl_DynSegmentationAnalyzer_set_EnabledFas(&CommonStruct);
   CommonStruct.flag_set      = GSNoneSetting;
   pClass->ptr_fas()->copy(&CommonStruct.NewFAS);

   CommonStruct.LblCreation  = false;
   CommonStruct.Lbl_flag_set = GSNoneSetting;
   pClass->ptr_fas()->copy(&CommonStruct.LblFAS);

   CommonStruct.AddODValues = true;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_SEC.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR)
      return GEOsimAppl::CMDLIST.ErrorCmd();

   while (1)
   {
      ads_new_dialog(_T("DynSegmentationAnalyzer"), dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
      if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ret = GS_BAD; break; }

      if (gsc_setTileDclDynSegmentationAnalyzer(dcl_id, &CommonStruct) == GS_BAD)
         { ret = GS_BAD; ads_term_dialog(); break; }

      ads_action_tile(dcl_id, _T("Location"), (CLIENTFUNC) dcl_SpatialSel);
      ads_action_tile(dcl_id, _T("LocationUse"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_LocationUse);
      ads_client_data_tile(dcl_id, _T("LocationUse"), &CommonStruct);
      ads_action_tile(dcl_id, _T("Logical_op"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_Logical_op);
      ads_client_data_tile(dcl_id, _T("Logical_op"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SSFilterUse"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_LastFilterResultUse);
      ads_client_data_tile(dcl_id, _T("SSFilterUse"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecTable"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_SecTable);
      ads_client_data_tile(dcl_id, _T("SecTable"), &CommonStruct);
      ads_action_tile(dcl_id, _T("editsql"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_editsql);
      ads_client_data_tile(dcl_id, _T("editsql"), &CommonStruct);
      ads_action_tile(dcl_id, _T("SecSQL"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_SecSQL);
      ads_client_data_tile(dcl_id, _T("SecSQL"), &CommonStruct);

      ads_action_tile(dcl_id, _T("keyConvertingFas"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_keyConvertingFas);
      ads_client_data_tile(dcl_id, _T("keyConvertingFas"), &CommonStruct);
      ads_action_tile(dcl_id, _T("keyFas"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_keyFas);
      ads_client_data_tile(dcl_id, _T("keyFas"), &CommonStruct);

      ads_action_tile(dcl_id, _T("keyLblCreation"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_keyLblCreation);
      ads_client_data_tile(dcl_id, _T("keyLblCreation"), &CommonStruct);
      ads_action_tile(dcl_id, _T("keyLblFun"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_keyLblFun);
      ads_client_data_tile(dcl_id, _T("keyLblFun"), &CommonStruct);
      ads_action_tile(dcl_id, _T("keyLblFas"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_keyLblFas);
      ads_client_data_tile(dcl_id, _T("keyLblFas"), &CommonStruct);

      ads_action_tile(dcl_id, _T("AddObjectData"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_AddObjectData);
      ads_client_data_tile(dcl_id, _T("AddObjectData"), &CommonStruct);
      ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) dcl_DynSegmentationAnalyzer_help);

      ads_start_dialog(dcl_id, &status);

      if (status == DLGOK)                                // uscita con tasto OK
      {  
         break;
      }
      else if (status == DLGCANCEL) { ret = GS_CAN; break; } // uscita con tasto CANCEL

      switch (status)
      {
         case LOCATION_SEL:  // definizione di area di selezione
         {
            gsc_ddSelectArea(CommonStruct.SpatialSel);
            break;
         }
	   }
   }
   ads_unload_dialog(dcl_file);

   if (ret != GS_GOOD)
      if (ret == GS_CAN) return GEOsimAppl::CMDLIST.CancelCmd();
      else return GEOsimAppl::CMDLIST.ErrorCmd();

   // ricavo il LINK_SET da usare per il filtro
   ret = gsc_get_LS_for_filter(pClass, CommonStruct.LocationUse,
                               CommonStruct.SpatialSel,
                               CommonStruct.LastFilterResultUse, 
                               CommonStruct.Logical_op, 
                               ResultLS);

   // se c'è qualcosa che non va
   if (ret == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();

   // se devono essere considerate tutte le entità della classe che sono state estratte
   if (ret == GS_CAN) UseLS = false;
   else UseLS = true;
 
   if ((pSec = (C_SECONDARY *) pClass->find_sec(CommonStruct.Sec)) == NULL) return;

   GEOsimAppl::CMDLIST.StartCmd();
   C_SELSET outSS;

   if (!CommonStruct.LblCreation) CommonStruct.LblFun.clear();

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) return;
   if (gsc_do_SecDynamicSegmentationAnalyze(pSec, CommonStruct.SQLCond, 
                                            (UseLS) ? &ResultLS : NULL,
                                            CommonStruct.NewFAS,
                                            (CommonStruct.FASConvertion) ? CommonStruct.flag_set : 0,
                                            CommonStruct.LblFun, CommonStruct.LblFAS, CommonStruct.Lbl_flag_set,
                                            CommonStruct.AddODValues, outSS) != GS_GOOD)
   {
      gsc_abortTransaction();
      return GEOsimAppl::CMDLIST.ErrorCmd();
   }

   // End Undo
   gsc_endTransaction();

   return GEOsimAppl::CMDLIST.EndCmd();
}


///////////////////////////////////////////////////////////////////////////////
// GESTIONE DCL PER SEGMENTAZIONE DINAMICA  -  FINE
///////////////////////////////////////////////////////////////////////////////


/****************************************************************************/
/*.doc gsc_CalibrateDynSegmentationData                          <internal> */
/*+
  Questa funzione calibra una distanza in funzione di riferimenti nominali.
  Parametri:
  double GeomDist;                     Distanza da calibrare
  C_2REAL_LIST &NominalGeomDistList;   Lista delle coppie distanza nominale -
                                       geometrica dall'inizio della linea 
                                       ordinata per distanza nominale (ma anche geometrica)

  Restituisce la distanza calibrata in caso di successo altrimenti restituisce -1. 
-*/
/****************************************************************************/
double gsc_CalibrateDynSegmentationData(double GeomDist, C_2REAL_LIST &NominalGeomDistList)
{
   double Res, Geom1, Geom2, Nominal1, Nominal2;
   C_2REAL *p1 = (C_2REAL *) NominalGeomDistList.get_head(), *p2;

   if (!p1) return -1;

   // cerco l'intervallo in cui ricade Dist
   if (p1->get_key_2_double() > 0)
   {
      Nominal1 = 0; // distanza nominale del primo punto dell'intervallo
      Geom1    = 0; // distanza geometrica del primo punto dell'intervallo
      p2       = p1;
   }
   else
   {
      Nominal1 = p1->get_key_double();   // distanza nominale del primo punto dell'intervallo
      Geom1    = p1->get_key_2_double(); // distanza geometrica del primo punto dell'intervallo
      p2       = (C_2REAL *) p1->get_next();
   }

   while (p2 && GeomDist > p2->get_key_2_double())
   {
      p1       = p2;
      Nominal1 = p1->get_key_double(); // distanza nominale del primo punto dell'intervallo
      Geom1    = p1->get_key_2_double();  // distanza geometrica del primo punto dell'intervallo
      p2       = (C_2REAL *) p2->get_next();
   }

   Res = Nominal1;
   if (p2) // ci sono entrambi gli estremi dell'intervallo
   {
      Nominal2 = p2->get_key_double();
      Geom2    = p2->get_key_2_double();
      Res += ((Nominal2 - Nominal1) * (GeomDist - Geom1) / (Geom2 - Geom1));
   }
   else // c'è solo il primo estremo dell'intervallo
      Res += (GeomDist - Geom1);

   return Res;
}


/****************************************************************************/
/*.doc C_CLASS::CalibrateDynSegmentationData                     <external> */
/*+
  Questa funzione calibra le distanze nominali delle tabelle secondarie di un 
  gruppo di entità di una classe GEOsim.
  Parametri:
  C_SELSET &SelSet;     Oggetti grafici da calibrare (possono essere misti anche di altre classi)
  C_CLASS *pRefCls;     Puntatore alla classe di GEOsim da calibrare
  C_STRING &DistAttrib; Nome attributo contenente i valori delle distanze
  double Tolerance;     Tolleranza per trovare i riferimenti distanziometrici 
                        nell'intorno della linea da calibrare
  int CounterToVideo;   Flag, se = GS_GOOD stampa a video il numero di entità che si 
                        stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_CLASS::CalibrateDynSegmentationData(C_SELSET &SelSet, C_CLASS *pRefCls,
                                          C_STRING &DistAttrib, double Tolerance,
                                          int CounterToVideo)
{
   ads_name             ent, ref_ent;
   long                 i = 0, MySelSet_len, gs_id, j;
   C_SELSET             MySelSet, RefSelSet, entSelSet, entGrphSelSet;
   C_LINK_SET           RefLS;
   int                  Res = GS_GOOD;
   double               NominalDist, GeomDist;
   C_2REAL              *pNominalGeomDist;
   C_2REAL_LIST         NominalGeomDistList;
   C_RB_LIST            ColValues, RefColValues;
   presbuf              pRbDist;
   C_PREPARED_CMD_LIST  RefTempOldCmdList, TempOldCmdList;
   ads_point            InsPt;
   C_SINTH_SEC_TAB      *pSinthSec;
   C_SINTH_SEC_TAB_LIST SinthSecList;
   C_STRING             CompleteName;
   get_CompleteName(CompleteName);
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1072), CompleteName.get_name()); // "Calibrazione entità classe <%s>"

   SelSet.copyIntersectClsCode(MySelSet, id.code, id.sub_code);
   // se non ci sono oggetti nel gruppo di selezione
   if ((MySelSet_len = MySelSet.length()) <= 0) return GS_GOOD;

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

   // Leggo la lista sintetica delle secondarie che supportano la segmentazione dinamica
   if (get_pPrj()->getSinthClsSecondaryTabList(id.code, id.sub_code, SinthSecList) == GS_BAD) return GS_BAD;
   SinthSecList.FilterOnDynSegmentationSupportedOnly();

   if (SinthSecList.get_count() == 0) return GS_GOOD; // Non ci sono secondarie

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;

   // Preparo i comandi di lettura dei dati della classe di riferimento dal temp/old
   if (pRefCls->prepare_data(RefTempOldCmdList) == GS_BAD) return GS_BAD;

   while (MySelSet.entname(0, ent) == GS_GOOD)
   {
      if (CounterToVideo == GS_GOOD)
         StatusLineMsg.Set(++i); // "\r%ld entità GEOsim elaborate."

      // Estraggo totalmente l'entità senza bloccarla agli altri utenti
      if (get_Key_SelSet(ent, &gs_id, entSelSet) == GS_BAD) { Res = GS_BAD; break; }
      MySelSet.subtract(entSelSet);
      if (is_updateableSS(gs_id, entSelSet, NULL, GS_BAD, GS_GOOD) == GS_BAD)
         { Res = GS_BAD; break; }

      entSelSet.copyIntersectType(entGrphSelSet, GRAPHICAL); // copio scartando i blocchi DA
      // Ricavo le entità di riferimento distanziometrico
      if (Tolerance > 0)
      {
         if (gsc_selClsObjsBufferFenceSS(entGrphSelSet, Tolerance, INSIDE, pRefCls, RefLS) == GS_BAD)
            { Res = GS_BAD; break; }
      }
      else
         if (gsc_selClsObjsFenceSS(entGrphSelSet, pRefCls, RefLS) == GS_BAD)
            { Res = GS_BAD; break; }
      
      if (RefLS.ptr_SelSet()->length() == 0) continue; // non ci sono riferimenti

      entGrphSelSet.entname(0, ent); // prendo il primo oggetto grafico principale

      // mi ricavo una lista con le distanze dal punto iniziale della linea
      // e i valori di distanze di riferimento
      NominalGeomDistList.remove_all();
      j = 0;
      while (RefLS.ptr_SelSet()->entname(j++, ref_ent) == GS_GOOD)
      {
         // leggo la distanza nominale
         if (pRefCls->query_data(ref_ent, RefColValues, &RefTempOldCmdList) == GS_BAD)
            { Res = GS_BAD; break; }
         if (!(pRbDist = RefColValues.CdrAssoc(DistAttrib.get_name())))
            { GS_ERR_COD = eGSInvAttribName; Res = GS_BAD; break; }
         if (gsc_rb2Dbl(pRbDist, &NominalDist) == GS_BAD || NominalDist < 0) // riferimento non valido
            continue;

         // calcolo la distanza geometrica tra i punto e l'inizio della linea
         if (gsc_get_firstPoint(ref_ent, InsPt) == GS_BAD)
            { Res = GS_BAD; break; }
         if (Tolerance > 0)
            if (gsc_getClosestPointTo(ent, InsPt, InsPt) == GS_BAD) { Res = GS_BAD; break; }

         if ((GeomDist = gsc_getDistAtPoint(ent, InsPt)) < 0) { Res = GS_BAD; break; }

         if (NominalGeomDistList.add_tail_2double(NominalDist, GeomDist) == GS_BAD)
            { Res = GS_BAD; break; }
      }
      if (Res == GS_BAD) break;

      NominalGeomDistList.sort_key(); // ordine crescente per distanza nominale

      if (NominalGeomDistList.get_count() > 1)
      {
         // Analizzo la lista delle distanze (ordinata per distanza nominale)
         int    QtyAscOrder = 0;
         double PrevGeomDist;
         bool   AscOrder;

         pNominalGeomDist = (C_2REAL *) NominalGeomDistList.get_head();
         PrevGeomDist = pNominalGeomDist->get_key_2_double();
         while ((pNominalGeomDist = (C_2REAL *) NominalGeomDistList.get_next()) != NULL)
         {
            if (PrevGeomDist < pNominalGeomDist->get_key_2_double()) QtyAscOrder++;
            PrevGeomDist = pNominalGeomDist->get_key_2_double();
         }
         // stabilisco l'ordine delle distanze
         AscOrder = (QtyAscOrder >= NominalGeomDistList.get_count()) ? true : false;
         if (!AscOrder) // devo invertire l'ordine dei vertici della polilinea
         {
            presbuf rb_length;
            // calcolo la lunghezza della linea
            if ((rb_length = gsc_get_graphical_data(ent, GS_LISP_LENGTH, _T("real"))) == NULL)
               { Res = GS_BAD; break; }

            // ricalcolo le distanze geometriche
            pNominalGeomDist = (C_2REAL *) NominalGeomDistList.get_head();
            while (pNominalGeomDist)
            {
               pNominalGeomDist->set_key_2(rb_length->resval.rreal - pNominalGeomDist->get_key_2_double());
               pNominalGeomDist = (C_2REAL *) NominalGeomDistList.get_next();
            }

            acutRelRb(rb_length);

            // faccio il reverse dei vertici
            if (gsc_reverseCurve(ent) != GS_GOOD)
               { Res = GS_BAD; break; }

            // leggo scheda
            if (query_data(gs_id, ColValues, &TempOldCmdList) == GS_BAD)
               { Res = GS_BAD; break; }
            // Aggiorno database e video
            if (upd_data(gs_id, ColValues, (C_PREPARED_CMD *) TempOldCmdList.get_head(), 
                         &entSelSet) == GS_BAD) 
              { Res = GS_BAD; break; }
         }

         // scarto gli elementi che non hanno le distanze geometriche crescenti
         pNominalGeomDist = (C_2REAL *) NominalGeomDistList.get_head();
         PrevGeomDist = pNominalGeomDist->get_key_2_double();
         pNominalGeomDist = (C_2REAL *) NominalGeomDistList.get_next();
         while (pNominalGeomDist)
            if (PrevGeomDist >= pNominalGeomDist->get_key_2_double())
            {
               NominalGeomDistList.remove_at();
               pNominalGeomDist = (C_2REAL *) NominalGeomDistList.get_cursor();
            }
            else
               pNominalGeomDist = (C_2REAL *) NominalGeomDistList.get_next();
      }

      // calibro tutte le secondarie che supportano la segmentazione dinamica
      pSinthSec = (C_SINTH_SEC_TAB *) SinthSecList.get_head();
      while (pSinthSec)
         if (CalibrateDynSegmentationData(pSinthSec->get_key(), gs_id,
                                          NominalGeomDistList) == GS_BAD)
            { Res = GS_BAD; break; }
         else 
            pSinthSec = (C_SINTH_SEC_TAB *) SinthSecList.get_next();

      if (Res == GS_BAD) break;
   }

   StatusLineMsg.End(gsc_msg(310), i); // ogni 10 "%ld entità GEOsim elaborate."

   return Res;
}


/****************************************************************************/
/*.doc C_CLASS::CalibrateDynSegmentationData                     <external> */
/*+
  Questa funzione calibra le distanze nominali della tabella secondaria di una 
  entità di una classe GEOsim.
  Parametri:
  int  sec;                            Codice tabella secondaria
  long gs_id;                          Codice entità da calibrare
  C_2REAL_LIST &NominalGeomDistList;   Lista delle coppie distanza nominale -
                                       geometrica dall'inizio della linea

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_CLASS::CalibrateDynSegmentationData(int sec, long gs_id, C_2REAL_LIST &NominalGeomDistList)
{
   C_SECONDARY         *pSecondary;
   C_INFO_SEC          *pInfoSec;
   C_RB_LIST           AllColValues, ColValues;
   presbuf             pRb, pRbValue;
   double              GeomDist, NominalDist;
   bool                ToUpdate;
   long                Key;
   C_PREPARED_CMD_LIST TempOldCmdList;

   if ((pSecondary = (C_SECONDARY *) find_sec(sec)) == NULL)
      return GS_BAD;
   if (pSecondary->DynSegmentationType() == GSNoneDynSegmentation)
      return GS_BAD;
   pInfoSec = pSecondary->ptr_info();

   // Preparo il comando per accesso ai dati in TEMP e OLD
   if (pSecondary->prepare_data(TempOldCmdList) == GS_BAD) return GS_BAD;
   // Copio le schede e le relazioni dalle tabelle OLD   
   if (pSecondary->CopyDataToTemp(gs_id, (C_PREPARED_CMD *) TempOldCmdList.getptr_at(2)) == GS_BAD)
      return GS_BAD;

   if (pSecondary->query_AllData(gs_id, AllColValues) == GS_BAD) return GS_BAD;

   pRb = AllColValues.get_head();
   if (pRb) pRb = pRb->rbnext;
   while (pRb && pRb->restype == RTLB)
   {
      ColValues << gsc_listcopy(pRb);

      ToUpdate = false;
      if ((pRbValue = ColValues.CdrAssoc(pInfoSec->key_attrib.get_name())) == NULL ||
          gsc_rb2Lng(pRbValue, &Key) == GS_BAD)
         return GS_BAD;

      if ((pRbValue = ColValues.CdrAssoc(pInfoSec->real_init_distance_attrib.get_name())) != NULL &&
          gsc_rb2Dbl(pRbValue, &GeomDist) == GS_GOOD &&
          (NominalDist = gsc_CalibrateDynSegmentationData(GeomDist, NominalGeomDistList)) >= 0)
         if (ColValues.CdrAssocSubst(pInfoSec->nominal_init_distance_attrib.get_name(), NominalDist) == GS_GOOD)
            ToUpdate = true;

      if ((pRbValue = ColValues.CdrAssoc(pInfoSec->real_final_distance_attrib.get_name())) != NULL &&
          gsc_rb2Dbl(pRbValue, &GeomDist) == GS_GOOD &&
          (NominalDist = gsc_CalibrateDynSegmentationData(GeomDist, NominalGeomDistList)) >= 0)
         if (ColValues.CdrAssocSubst(pInfoSec->nominal_final_distance_attrib.get_name(), NominalDist) == GS_GOOD)
            ToUpdate = true;

      if (ToUpdate)
         if (pSecondary->upd_data(gs_id, Key, ColValues,
                                  (C_PREPARED_CMD *) TempOldCmdList.get_head()) != GS_GOOD)
            return GS_BAD;

      if ((pRb = gsc_scorri(pRb))) // vado alla chiusa tonda successiva
         pRb = pRb->rbnext; // vado alla aperta tonda successiva
   }

   return GS_GOOD;
}