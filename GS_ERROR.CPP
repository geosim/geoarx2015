/**********************************************************
Name: GS_ERROR.CPP

Module description: File funzioni di per la gestione
                    delle situazioni di errore. 
            
Author: Poltini Roberto

(c) Copyright 1995-2014 by IREN ACQUA GAS

**********************************************************/


/*********************************************************/
/* INCLUDE */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "adslib.h"
#include "adsdlg.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h" 
#include "gs_list.h"   
#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_ade.h"
#include "gs_init.h"   


/*********************************************************/
/* GLOBAL VARIABILES */
/*********************************************************/


int  GS_ERR_COD = eGSNoError; // Codice ultimo errore verificatosi

static TCHAR GS_MSG[MAX_LEN_MSG]= GS_EMPTYSTR;


/*+
-------------------------------------------------------------
TCHAR *gsc_msg(int code)

  Funzione che ritorna il putatore alla stringa globale
  GS_MSG in cui ricopia la traduzione corrente del messagio
  corrispondente a 'code'.
  Richiama funzione "gs_traduction" del modulo "GEOLAN.ARX".
-------------------------------------------------------------
-*/
TCHAR *gsc_msg(int code)
{
   resbuf *command = NULL,*ret = NULL;
   int    rc = RTERROR;
   TCHAR  *punt;

   GS_MSG[0] = _T('\0');
   command = acutBuildList(RTSTR, _T("GS_TRADUCTION"), RTSHORT, code, 0);
   if (command)
   {
      if ((rc = acedInvoke(command,&ret))==RTERROR)
         acutPrintf(_T("\nGEOLAN.ARX module not found.\n")); 
      acutRelRb(command);
      if (ret) 
      {
         gsc_strcpy(GS_MSG, ret->resval.rstring, MAX_LEN_MSG);
         acutRelRb(ret);
      }
   }
   punt = GS_MSG;
 
   return punt;
}


/*+
-------------------------------------------------------------
TCHAR *gsc_err(int code)

  Funzione che ritorna il putatore alla stringa globale
  GS_MSG in cui ricopia la traduzione corrente del messagio
  di errore corrispondente a 'code'.
  Richiama funzione "gs_error_msg" del modulo "GEOLAN.ARX".
-------------------------------------------------------------
-*/
TCHAR *gsc_err(int code)
{
   resbuf *command = NULL, *ret = NULL;
   int    rc = RTERROR;

   GS_MSG[0] = _T('\0');
   command   = acutBuildList(RTSTR, _T("GS_ERROR_MSG"), RTSHORT, code, 0);
   if (command)
   {
      if ((rc = acedInvoke(command, &ret)) == RTERROR)
         acutPrintf(_T("\nGEOLAN.ARX module not found.\n"));
      acutRelRb(command);
      if(ret) 
      {
         gsc_strcpy(GS_MSG, ret->resval.rstring, MAX_LEN_MSG);
         acutRelRb(ret);
      }
   }

   return GS_MSG;
}


/*+
-------------------------------------------------------------
int gsc_print_error(void)

  Stampa il messagio di errore relativo al valore GS_ERR_COD.
  Richiama funzione "gs_error_msg" del modulo "GEOLAN.ARX".
-------------------------------------------------------------
-*/
int gsc_print_error(void)
{  
   TCHAR msg1[MAX_LEN_MSG];

   gsc_strcpy(msg1, gsc_msg(133), MAX_LEN_MSG); // "\nErrore GEOsim:"
   acutPrintf(_T("%s [%d] - %s\n"), msg1, GS_ERR_COD, gsc_err(GS_ERR_COD));
   
   return GS_GOOD;
}


/*+
-------------------------------------------------------------
	Definite per renderle DllExport
-------------------------------------------------------------
-*/

int get_GS_ERR_COD()
{
	return GS_ERR_COD;
}

void set_GS_ERR_COD(int err)
{
	GS_ERR_COD = err;
}


/**********************************************************************/
/*.doc (mod 2) gsc_get_GEOdiag                                        */
/*+	   
   Restituisce una lista di resbuf contenente le informazioni diagnostiche
   nel seguente formato:
   ((<message 1> (<param 1><param 2>...))(<message 2> (<param 1><param 2>...)) ...)

   Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
presbuf gsc_get_GEOdiag(void)
{
   int       jj = 0;
   TCHAR     message[512];
   C_RB_LIST list;

   if (list << acutBuildList(RTLB, 0) == NULL) return NULL;

   if (errno) // se è un errore segnalato da errno
   {
      // Store messages
      swprintf(message, 512, _T("%-5d %-12s"), jj, strerror(errno));
      if ((list += acutBuildList(RTLB, RTSTR, message, RTLB, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      swprintf(message, 512, _T("%s OPERATING SYSTEM"), gsc_msg(613));    // Tipo errore:
      if ((list += acutBuildList(RTSTR, message, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }
      swprintf(message, 512, _T("%s %d"), gsc_msg(614), errno); // Codice errore:
      if ((list += acutBuildList(RTSTR, message, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

      TCHAR SysMessage[256];
      _wcserror_s(SysMessage, 256, errno);
      swprintf(message, 512, _T("%s %s"), gsc_msg(615), SysMessage);  // Messaggio:
      if ((list += acutBuildList(RTSTR, message, RTLE, RTLE, 0)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

      jj++;
   }

   // Store messages
   swprintf(message, 512, _T("%-5d %-12s"), jj, gsc_err(GS_ERR_COD));
   if ((list += acutBuildList(RTLB, RTSTR, message, RTLB, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   swprintf(message, 512, _T("%s GEOsim"), gsc_msg(613));    // Tipo errore:
   if ((list += acutBuildList(RTSTR, message, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   swprintf(message, 512, _T("%s %d"), gsc_msg(614), GS_ERR_COD); // Codice errore:
   if ((list += acutBuildList(RTSTR, message, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   // su 2 righe distinte perchè gsc_msg e gsc_err utilizzando la stessa stringa
   swprintf(message, 512, _T("%s "), gsc_msg(615));  // Messaggio:
   wcscat(message, gsc_err(GS_ERR_COD));
   if ((list += acutBuildList(RTSTR, message, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }
   if ((list += acutBuildList(RTLE, RTLE, RTLE, 0)) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   list.ReleaseAllAtDistruction(GS_BAD);
   
   return list.get_head();
}


//-----------------------------------------------------------------------//
// INIZIO GESTIONE DCL PER ERRORI DI RETE                                  //
//-----------------------------------------------------------------------//


/*************************************************************************/
/*.doc (new 2) redraw_ErrorList() <internal>                   */
/*+
   Parametri:
   ads_hdlg dcl_id;  handle DCL attiva

   Visualizza nella DCL la lista dei parametri diagnostici 
-*/
/*************************************************************************/
int redraw_ErrorList(ads_hdlg dcl_id, presbuf ErrorList)
{
   presbuf p;
   int     i = 0, n, PrevPos;
   TCHAR   offset[5], *pStr;

   // posizione della riga corrente
   ads_get_tile(dcl_id, _T("error_list"), offset, 4);
   PrevPos = _wtoi(offset);
   
   // offset di spostamento dello slider
   ads_get_tile(dcl_id, _T("error_slid"), offset, 4);
   n = _wtoi(offset);

   // creazione lista messaggi di errore per listbox
   if ((ads_start_list(dcl_id, _T("error_list"), LIST_NEW, 0))!=RTNORM)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   while ((p = gsc_nth(i++, ErrorList)) != NULL)
   {
      pStr = gsc_nth(0, p)->resval.rstring;
      if (wcslen(pStr) > (unsigned int) n) gsc_add_list(pStr + n);
      else gsc_add_list(GS_EMPTYSTR);
   }
   if (ads_end_list() != RTNORM) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   // riposiziono la riga che era corrente
   _itow_s(PrevPos, offset, 5, 10);
   ads_set_tile(dcl_id, _T("error_list"), offset);
   
   ads_set_tile(dcl_id, _T("error"), GS_EMPTYSTR);

   return GS_GOOD;
}


// ACTION TILE : slider orizzontale lista errori
static void CALLB error_slid(ads_callback_packet *dcl)
{
   redraw_ErrorList(dcl->dialog, (presbuf) dcl->client_data);
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   return;
} 


/*************************************************************************/
/*.doc (new 2) redraw_DiagList() <internal>                   */
/*+
   Parametri:
   ads_hdlg dcl_id;  handle DCL attiva
   int message;      Codice messaggio

   Visualizza nella DCL la lista dei parametri diagnostici 
-*/
/*************************************************************************/
int redraw_DiagList(ads_hdlg dcl_id, int message, presbuf ErrorList)
{
   presbuf p, q;
   int     i = 0, n, PrevPos;
   TCHAR   offset[5], *pStr;

   // posizione della riga corrente
   ads_get_tile(dcl_id, _T("diag_list"), offset, 4);
   PrevPos = _wtoi(offset);

   // offset di spostamento dello slider
   ads_get_tile(dcl_id, _T("diag_slid"), offset, 4);
   n = _wtoi(offset);

   // creazione lista parametri di diagnosi per listbox
   if ((ads_start_list(dcl_id, _T("diag_list"), LIST_NEW, 0))!=RTNORM)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if ((p = gsc_nth(message, ErrorList)) != NULL &&
       (p = gsc_nth(1, p)) != NULL)
      while ((q = gsc_nth(i++, p)) != NULL)
      {
         pStr = q->resval.rstring;
         if (pStr && wcslen(pStr) > (unsigned int) n) gsc_add_list(pStr + n);
         else gsc_add_list(GS_EMPTYSTR);
      }

   if (ads_end_list() != RTNORM) { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   // riposiziono la riga che era corrente
   _itow_s(PrevPos, offset, 5, 10);
   ads_set_tile(dcl_id, _T("error_list"), offset);

   ads_set_tile(dcl_id, _T("error"), GS_EMPTYSTR);

   return GS_GOOD;
}


// ACTION TILE : slider orizzontale lista errori
static void CALLB diag_slid(ads_callback_packet *dcl)
{
   TCHAR message[5];                        

   ads_get_tile(dcl->dialog, _T("error_list"), message, 4);
   redraw_DiagList(dcl->dialog, _wtoi(message), (presbuf) dcl->client_data);
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   return;
} 


// ACTION TILE : click su tasto n. tentativi... //
static void CALLB error_attempts_num(ads_callback_packet *dcl)
{
   TCHAR new_value[5];

   if ((ads_get_tile(dcl->dialog, _T("attempts_num"), new_value, 4)) != RTNORM) return;

   if (_wtoi(new_value) <= 0)
   {
      ads_set_tile(dcl->dialog, _T("error"), gsc_msg(287)); // "*Errore* Valore non valido."
      _itow_s(GEOsimAppl::GLOBALVARS.get_NumTest(), new_value, 5, 10);
      ads_set_tile(dcl->dialog, _T("attempts_num"), new_value);
   }
   else
   {
      ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
      if (GEOsimAppl::GLOBALVARS.set_NumTest(_wtoi(new_value)) == GS_GOOD)
      {
         C_STRING pathfile;

         // scrivo variabili globali
         pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
         pathfile += _T('\\');
         pathfile += GS_INI_FILE;
         if (GEOsimAppl::GLOBALVARS.Save(pathfile.get_name()) == GS_BAD) return;
      }
   }

   return;
} 


// ACTION TILE : click su tasto tempo di attesa //
static void CALLB error_wait_time(ads_callback_packet *dcl)
{
   TCHAR new_value[5];

   if ((ads_get_tile(dcl->dialog, _T("wait_time"), new_value, 4)) != RTNORM)
      { ads_done_dialog(dcl->dialog, DLGOK); }

   if (_wtoi(new_value) < 0)
   {
      ads_set_tile(dcl->dialog, _T("error"), gsc_msg(287)); // "*Errore* Valore non valido."
      _itow_s(GEOsimAppl::GLOBALVARS.get_WaitTime(), new_value, 5, 10);
      ads_set_tile(dcl->dialog, _T("wait_time"), new_value);
   }
   else 
   {
      ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
      if (GEOsimAppl::GLOBALVARS.set_WaitTime(_wtoi(new_value)) == GS_GOOD)
      {
         C_STRING pathfile;

         // scrivo variabili globali
         pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
         pathfile += _T('\\');
         pathfile += GS_INI_FILE;
         if (GEOsimAppl::GLOBALVARS.Save(pathfile.get_name()) == GS_BAD) return;
      }
   }

   return;
} 


// ACTION TILE : click su riga lista errori //
static void CALLB error_msg_list(ads_callback_packet *dcl)
{
   TCHAR message[5];

   if (dcl->reason == 1)
   {
      ads_get_tile(dcl->dialog, _T("error_list"), message, 4);
      redraw_DiagList(dcl->dialog, _wtoi(message), (presbuf) dcl->client_data);
      ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   }

   return;
} 


// ACTION TILE : click su tasto Ritenta... //
static void CALLB error_retry(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, DLGOK);
   return;
} 


// ACTION TILE : click su tasto Annulla //
static void CALLB error_abort(ads_callback_packet *dcl)
{
   ads_done_dialog(dcl->dialog, DLGCANCEL);
   return;
} 


/*************************************************************************/
/*.doc (new 2) gsc_dderrordmin() <external>                               */
/*+
   Gestisce gli errori di rete con interfaccia DCL
   Parametri:
   const TCHAR *ErrType;       "ADE" per errore ADE, "GEOSIM" per errore gestito da GEOsim
   const TCHAR *OptionalDescr; Descrizione opzionale (default = NULL)

   Ritorna GS_GOOD se si vuole ritentare, GS_CAN se si vuole abortire,
   GS_BAD in caso di errore.
-*/
/*************************************************************************/
int gsc_dderroradmin(const TCHAR *ErrType, const TCHAR *OptionalDescr)
{
   C_RB_LIST ErrorList;

   if (!ErrType) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (gsc_strcmp(ErrType, _T("ADE"), FALSE) == 0) // è un errore di ADE (no sensitive)
   {
      if ((ErrorList << gsc_get_ADEdiag()) == NULL || ErrorList.GetCount() <= 0)
         return GS_BAD;
   }
   else
   if (gsc_strcmp(ErrType, _T("GEOSIM"), FALSE) == 0) // è un errore gestito da GEOsim (no sensitive)
   {
      if ((ErrorList << gsc_get_GEOdiag()) == NULL || ErrorList.GetCount() <= 0)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   }
   else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   return gsc_dderroradmin(ErrorList, OptionalDescr);
}
int gsc_dderroradmin(C_RB_LIST &ErrorList, const TCHAR *OptionalDescr)
{
   ads_hdlg  dcl_id;
   int       status, i = 0, DclFile; // usata per la dcl che gestisce gli errori
   TCHAR     new_value[5];
   C_STRING  pathfile;

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   pathfile = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\") + GS_GENERIC_DCL;
   if (gsc_load_dialog(pathfile.get_name(), &DclFile) != RTNORM) return GS_BAD;

   if (ads_new_dialog(_T("ErrorAdmin"), DclFile, (CLIENTFUNC)NULLCB, &dcl_id) != RTNORM ||
       dcl_id == NULL)
      { GS_ERR_COD = eGSAbortDCL; ads_unload_dialog(DclFile); return GS_BAD; }

   // se esiste inserisco una descrizione opzionale
   if (OptionalDescr)
   {
      TCHAR _OptionalDescr[80]; // vedi in "gs.dcl" nella dialog "ErrorAdmin" la width del tile "OptionalDescr"

      gsc_strcpy(_OptionalDescr, OptionalDescr, 80);
      ads_set_tile(dcl_id, _T("OptionalDescr"), _OptionalDescr);
   }

   if (redraw_ErrorList(dcl_id, ErrorList.get_head()) == GS_BAD)
      { ads_unload_dialog(DclFile); return GS_BAD; }
   ads_set_tile(dcl_id, _T("error_list"), _T("0"));
   redraw_DiagList(dcl_id, 0, ErrorList.get_head());
   _itow_s(GEOsimAppl::GLOBALVARS.get_NumTest(), new_value, 5, 10);
   ads_set_tile(dcl_id, _T("attempts_num"), new_value);
   _itow_s(GEOsimAppl::GLOBALVARS.get_WaitTime(), new_value, 5, 10);
   ads_set_tile(dcl_id, _T("wait_time"), new_value);

   if (ads_action_tile(dcl_id, _T("error_list"), error_msg_list) != RTNORM || // lista errori
       ads_client_data_tile(dcl_id, _T("error_list"), ErrorList.get_head()) != RTNORM ||
       ads_action_tile(dcl_id, _T("error_slid"), error_slid) != RTNORM ||     // slider orizzontale
       ads_client_data_tile(dcl_id, _T("error_slid"), ErrorList.get_head()) != RTNORM ||
       ads_action_tile(dcl_id, _T("diag_slid"), diag_slid) != RTNORM ||       // slider orizzontale
       ads_client_data_tile(dcl_id, _T("diag_slid"), ErrorList.get_head()) != RTNORM ||
       ads_action_tile(dcl_id, _T("attempts_num"), error_attempts_num) != RTNORM || // n. tentativi
       ads_action_tile(dcl_id, _T("wait_time"), error_wait_time) != RTNORM || // tempo tra 2 tentativi
       ads_action_tile(dcl_id, _T("retry"), error_retry) != RTNORM ||         // tasto Ritenta...
       ads_action_tile(dcl_id, _T("abort"), error_abort) != RTNORM)           // tasto Annulla
      { ads_unload_dialog(DclFile); GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   if (ads_start_dialog(dcl_id, &status) != RTNORM)
      { ads_unload_dialog(DclFile); GS_ERR_COD = eGSAbortDCL; return GS_BAD; }

   ads_unload_dialog(DclFile);

   if (status == DLGCANCEL) return GS_CAN;

   return GS_GOOD;
}


/**********************************************************************/
/*.doc printDBErrs                                         <internal> */
/*+	   
  Stampa gli errori sulla riga di comando.
  Parametri:
  C_RB_LIST &ErrorList;
-*/  
/*********************************************************/
void gsc_PrintError(C_RB_LIST &ErrorList)
{
   presbuf pMsg, pDiag;
   int     ii, jj = 0;

   while ((pMsg = ErrorList.nth(jj++)) != NULL)
   {
      acutPrintf(_T("\n--------------------\n%s"), gsc_nth(0, pMsg)->resval.rstring);
      if ((pMsg = gsc_nth(1, pMsg)) != NULL)
      {
         ii = 0;
         // Print diagnostics parameters
         while ((pDiag = gsc_nth(ii++, pMsg)) != NULL)
            acutPrintf(_T("\n%s"), pDiag->resval.rstring);  
      }
   }
}


//-----------------------------------------------------------------------//
// FINE GESTIONE DCL PER ERRORI IN RETE                                  //
//-----------------------------------------------------------------------//