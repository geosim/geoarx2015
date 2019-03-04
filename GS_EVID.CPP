/**********************************************************
Name: GS_EVID.CPP

Module description: Gestione tramite finestre delle procedure
                    di evidenziazione degli oggetti grafici.
            
Author:  Roberto Poltini

(c) Copyright 1997-2015 by IREN ACQUA GAS S.P.A.

              
Modification history:
              
Notes and restrictions on use: 


**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <string.h>      /*  per strcat() gsc_strcmp()  */
#include <sys/timeb.h>   // per pausa lampeggio oggetti
#include <time.h>        // per pausa lampeggio oggetti

#include "rxdefs.h"
#include "rxmfcapi.h"
#include "adslib.h"   
#include "adsdlg.h"   

#include "GSresource.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori
#include "gs_init.h" 
#include "gs_thm.h"       // gestione tematismi e sistemi di coordinate
#include "gs_resbf.h"     // gestione resbuf
#include "gs_utily.h"
#include "gs_selst.h"     // gestione gruppi di selezione
#include "gs_class.h"
#include "gs_prjct.h"
#include "gs_area.h" 
#include "gs_filtr.h" 
#include "gs_lisp.h"      // per gsc_WhatIsGraphModified()
#include "gs_graph.h" 
#include "gs_query.h" 
#include "gs_evid.h" 

#include "d2hMap.h" // doc to help


/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/

// struttura usata per scambiare dati nella dcl di evidenziazione
struct Common_Dcl_Evid_Struct
{
   long  EnabledFas; // Flag che rappresenta quali caratteristiche grafiche possono essere
                     // settate
   long  *flag_set;  // Flag che rappresenta quali caratteristiche grafiche sono state
                     // settate
   C_FAS *pFAS;      // Caratteristiche grafiche
};


static void CALLB dcl_evid_help(ads_callback_packet *dcl);
static void gs_mod_tile(long value, TCHAR *tiletile, ads_hdlg dial, long *flag_set);
static void CALLB gs_op_1(ads_callback_packet *dcl);
static void CALLB gs_op_2(ads_callback_packet *dcl);
static void CALLB gs_op_4(ads_callback_packet *dcl);
static void CALLB gs_op_8(ads_callback_packet *dcl);
static void CALLB gs_op_16(ads_callback_packet *dcl);
static void CALLB gs_op_32(ads_callback_packet *dcl);
static void CALLB gs_op_color(ads_callback_packet *dcl);
static void CALLB gs_op_128(ads_callback_packet *dcl);
static void CALLB gs_op_256(ads_callback_packet *dcl);
static void CALLB gs_op_8192(ads_callback_packet *dcl);
static void CALLB gs_op_16384(ads_callback_packet *dcl);
static void CALLB gs_op_65536(ads_callback_packet *dcl);
static void CALLB gs_can_dlg(ads_callback_packet *dcl);
static void CALLB gs_acp_dlg(ads_callback_packet *dcl);
static void gs_s_layer(TCHAR *Value);
static void CALLB gs_s_MainEntLayer(ads_callback_packet *dcl);
static void CALLB gs_editscala(ads_callback_packet *dcl);
static void CALLB gs_radioscala(ads_callback_packet *dcl);
static void CALLB gs_acp_scala(ads_callback_packet *dcl);
static void gs_s_scala(double *pValue);
static void CALLB gs_s_BlkScale(ads_callback_packet *dcl);
static void CALLB gs_editlarg(ads_callback_packet *dcl);
static void CALLB gs_radiolarg(ads_callback_packet *dcl);
static void CALLB gs_acp_larg(ads_callback_packet *dcl);
static void CALLB gs_s_larg(ads_callback_packet *dcl);
static void CALLB gs_edithtext(ads_callback_packet *dcl);
static void CALLB gs_radiohtext(ads_callback_packet *dcl);
static void CALLB gs_acp_htext(ads_callback_packet *dcl);
static void CALLB gs_s_htext(ads_callback_packet *dcl);
static void CALLB gs_s_stile(ads_callback_packet *dcl);
static void CALLB gs_s_tipo(ads_callback_packet *dcl);

static void CALLB gs_s_HatchName(ads_callback_packet *dcl);
static void CALLB gs_s_HatchColor(ads_callback_packet *dcl);
static void CALLB gs_s_hatch(ads_callback_packet *dcl);

static void CALLB gs_s_color(ads_callback_packet *dcl);
static void CALLB gs_s_blocco(ads_callback_packet *dcl);
static void CALLB gs_editzoom(ads_callback_packet *dcl);
static void CALLB gs_acp_zoom(ads_callback_packet *dcl);
static void CALLB gs_op_zoom(ads_callback_packet *dcl);


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


/****************************************1*****************/
/*+
  Questa funzione abilita o disabilita la selezione del 
  tasto OK e del tasto di opzione
-*/  
/*********************************************************/
static void gs_mod_tile(long value, TCHAR *tiletile, ads_hdlg dial, long *flag_set)
{
   if (tiletile)
      if (value < 0) ads_mode_tile(dial, tiletile, MODE_DISABLE);
      else           ads_mode_tile(dial, tiletile, MODE_ENABLE);
 
   if (value < 0)
   {  // Se bit già settato
      if (*flag_set & abs(value)) *flag_set += value;
   }
   else *flag_set = *flag_set | value;

   if (*flag_set == 0) ads_mode_tile(dial, _T("accept"), MODE_DISABLE);
   else ads_mode_tile(dial, _T("accept"), MODE_ENABLE);
}

static void CALLB gs_op_1(ads_callback_packet *dcl)
{ // PIANO
    if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSLayerSetting, _T("opz_p"), dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSLayerSetting, _T("opz_p"), dcl->dialog, (long *) dcl->client_data);
}
static void CALLB gs_op_2(ads_callback_packet *dcl)
{ //BLOCCO
    if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSBlockNameSetting, _T("opz_b"), dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSBlockNameSetting, _T("opz_b"), dcl->dialog, (long *) dcl->client_data);
}
static void CALLB gs_op_4(ads_callback_packet *dcl)
{ // STILE
    if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSTextStyleSetting, _T("opz_st"), dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSTextStyleSetting, _T("opz_st"), dcl->dialog, (long *) dcl->client_data);
}
static void CALLB gs_op_8(ads_callback_packet *dcl)
{ // SCALA
    if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSBlockScaleSetting, _T("opz_sc"), dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSBlockScaleSetting, _T("opz_sc"), dcl->dialog, (long *) dcl->client_data);
}
static void CALLB gs_op_16(ads_callback_packet *dcl)
{ // TIPOLINEA
    if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSLineTypeSetting, _T("opz_tp"), dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSLineTypeSetting, _T("opz_tp"), dcl->dialog, (long *) dcl->client_data);
}
static void CALLB gs_op_32(ads_callback_packet *dcl)
{ // LARGHEZZA
    if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSWidthSetting, _T("opz_la"), dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSWidthSetting, _T("opz_la"), dcl->dialog, (long *) dcl->client_data);
}
static void CALLB gs_op_color(ads_callback_packet *dcl)
{ // COLORE
    if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSColorSetting, _T("opz_cl"), dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSColorSetting, _T("opz_cl"), dcl->dialog, (long *) dcl->client_data);
}
static void CALLB gs_op_256(ads_callback_packet *dcl)
{ // RIEMPIMENTO
    if (_wtoi(dcl->value) == 0)
    {  // deselezionato
       gs_mod_tile(-1 * GSHatchNameSetting, _T("opz_rm"), dcl->dialog, (long *) dcl->client_data);
       (* (long *) dcl->client_data) = (* (long *) dcl->client_data) & (~GSHatchLayerSetting);
       (* (long *) dcl->client_data) = (* (long *) dcl->client_data) & (~GSHatchColorSetting);
       (* (long *) dcl->client_data) = (* (long *) dcl->client_data) & (~GSHatchScaleSetting);
       (* (long *) dcl->client_data) = (* (long *) dcl->client_data) & (~GSHatchRotationSetting);
    }
    else
    {  // selezionato
       gs_mod_tile(GSHatchNameSetting, _T("opz_rm"), dcl->dialog, (long *) dcl->client_data);
       (* (long *) dcl->client_data) = (* (long *) dcl->client_data) | GSHatchLayerSetting;
       (* (long *) dcl->client_data) = (* (long *) dcl->client_data) | GSHatchColorSetting;
       (* (long *) dcl->client_data) = (* (long *) dcl->client_data) | GSHatchScaleSetting;
       (* (long *) dcl->client_data) = (* (long *) dcl->client_data) | GSHatchRotationSetting;
    }
}
static void CALLB gs_op_8192(ads_callback_packet *dcl)
{ // EVIDENZIA
   if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSHighlightSetting, NULL, dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSHighlightSetting, NULL, dcl->dialog, (long *) dcl->client_data);
}
static void CALLB gs_op_65536(ads_callback_packet *dcl)
{ // LAMPEGGIA
   if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSBlinkingSetting, NULL, dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSBlinkingSetting, NULL, dcl->dialog, (long *) dcl->client_data);
}

static void CALLB gs_op_16384(ads_callback_packet *dcl)
{ // ZOOM
    if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSZoomSetting, _T("opz_zoom"), dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSZoomSetting, _T("opz_zoom"), dcl->dialog, (long *) dcl->client_data);
}

static void CALLB gs_op_128(ads_callback_packet *dcl)
{ // ALTEZZA TESTO
    if (_wtoi(dcl->value) == 0)
	    // deselezionato
       gs_mod_tile(-1 * GSTextHeightSetting, _T("opz_th"), dcl->dialog, (long *) dcl->client_data);
    else
	   // selezionato
       gs_mod_tile(GSTextHeightSetting, _T("opz_th"), dcl->dialog, (long *) dcl->client_data);
}

static void CALLB gs_op_Obj2SaveSet(ads_callback_packet *dcl)
{ // INSERIMENTO NEL GRUPPO DI SELEZIONE
   if (_wtoi(dcl->value) == 0)
       // deselezionato
       gs_mod_tile(-1 * GSSaveAlteredObjs, NULL, dcl->dialog, (long *) dcl->client_data);
    else
       // selezionato
       gs_mod_tile(GSSaveAlteredObjs, NULL, dcl->dialog, (long *) dcl->client_data);
}

static void CALLB gs_can_dlg(ads_callback_packet *dcl)
{
    ads_done_dialog(dcl->dialog, DLGCANCEL);
}

static void CALLB gs_acp_dlg(ads_callback_packet *dcl)
{ // ACCEPT
   ads_hdlg hdlg = dcl->dialog;
   TCHAR    value[5];
   int      *pWhat = (int *) dcl->client_data;

   *pWhat = -1;

   ads_get_tile(hdlg, _T("main_ent"), value, 5 - 1);
   if (gsc_strcmp(value, _T("1")) == 0) *pWhat = GRAPHICAL;

   ads_get_tile(hdlg, _T("da_ent"), value, 5 - 1);
   if (gsc_strcmp(value, _T("1")) == 0)
      *pWhat = (*pWhat == GRAPHICAL) ? ALL : DA_BLOCK;

   ads_done_dialog(hdlg, DLGOK);
}

// Gestione layer generica inizio
static void CALLB gs_s_layer(TCHAR *Value)
{  // PIANO
   C_RB_LIST DescrLayer;

   if ((DescrLayer << gsc_ddsellayers(Value)) == NULL) return;

   if (DescrLayer.get_head() != NULL)
      gsc_strcpy(Value, DescrLayer.get_head()->resval.rstring, MAX_LEN_LAYERNAME);
}

// Gestione layer per entità principali
static void CALLB gs_s_MainEntLayer(ads_callback_packet *dcl)
{
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   gs_s_layer((TCHAR *) dcl->client_data);
   ads_set_tile(dcl->dialog, _T("val_p"), (TCHAR *) dcl->client_data);
}

// Gestione scala generica inizio
static void CALLB gs_editscala(ads_callback_packet *dcl)
{ 
   double valore;
   TCHAR  valori[10];
   
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   
   valore = _wtof(dcl->value);

   if (valore > 0)
   {
      swprintf(valori, 10, _T("%.3f"), valore);
      ads_set_tile(dcl->dialog, _T("editscala"), valori);
      if (valore == 2.0)
      {
         ads_set_tile(dcl->dialog, _T("radioscala"), _T("fact2"));
         ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
      }
      else if (valore == 5.0)
      {
         ads_set_tile(dcl->dialog, _T("radioscala"), _T("fact5"));
         ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
      }     
      else if (valore == 10.0)
      {
         ads_set_tile(dcl->dialog, _T("radioscala"), _T("fact10"));
         ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
      }
   }
   else
      ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidScale));  // "*Errore* Scala non valida"
}

static void CALLB gs_radioscala(ads_callback_packet *dcl)
{
  TCHAR valori[20];
 
  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
  wcscpy(valori, dcl->value);

  if (gsc_strcmp(valori, _T("fact2"))==0){
     ads_set_tile (dcl->dialog, _T("editscala"), _T("2.00"));
     ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori, _T("fact5"))==0){
     ads_set_tile (dcl->dialog, _T("editscala"), _T("5.00"));
     ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori, _T("fact10"))==0){
     ads_set_tile (dcl->dialog, _T("editscala"), _T("10.00"));
     ads_mode_tile(dcl->dialog, _T("editscala"), MODE_DISABLE);
  }
  else ads_mode_tile(dcl->dialog, _T("editscala"), MODE_ENABLE);
}

static void CALLB gs_acp_scala(ads_callback_packet *dcl)
{
  TCHAR  appo[TILE_STR_LIMIT];
  double value;
  double *evid_value = (double *) dcl->client_data;

  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

  if (ads_get_tile(dcl->dialog, _T("editscala"), appo, TILE_STR_LIMIT) != RTNORM) return;

  value = _wtof(appo);

  if (value > 0)
  {
     *evid_value = value;
     ads_done_dialog(dcl->dialog, DLGOK);
     
  }
  else ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidScale));  // "*Errore* Scala non valida"
}
static void CALLB gs_s_scala(double *pValue)
{
   ads_hdlg sscala;
   int      dbstatus, dcl_file;
   TCHAR    StrValue[10];
   C_STRING path;

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) != RTNORM) return;

   if (ads_new_dialog(_T("scala1"), dcl_file, NULLCB, &sscala) != RTNORM)
      { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return; }

   swprintf(StrValue, 10, _T("%.3f"), *pValue);
   ads_set_tile(sscala, _T("editscala"), StrValue);
   if (*pValue == 2.0)
   {
      ads_set_tile(sscala, _T("radioscala"), _T("fact2"));
      ads_mode_tile(sscala, _T("editscala"), MODE_DISABLE);
   }
   else if (*pValue == 5.0)
   {
      ads_set_tile(sscala, _T("radioscala"), _T("fact5"));
      ads_mode_tile(sscala, _T("editscala"), MODE_DISABLE);
   }
   else if (*pValue == 10.0)
   {
      ads_set_tile(sscala, _T("radioscala"), _T("fact10"));
      ads_mode_tile(sscala, _T("editscala"), MODE_DISABLE);
   }
   else ads_set_tile(sscala, _T("radioscala"), _T("factutente"));

   ads_action_tile(sscala, _T("scal_no"),    (CLIENTFUNC)gs_can_dlg);
   ads_action_tile(sscala, _T("editscala"),  (CLIENTFUNC)gs_editscala);
   ads_action_tile(sscala, _T("radioscala"), (CLIENTFUNC)gs_radioscala);
   ads_action_tile(sscala, _T("scal_si"),    (CLIENTFUNC)gs_acp_scala);
   ads_client_data_tile(sscala, _T("scal_si"), pValue);

   ads_start_dialog(sscala, &dbstatus);
   ads_unload_dialog(dcl_file);
}
// Gestione scala generica fine
// Gestione scala blocchi
static void CALLB gs_s_BlkScale(ads_callback_packet *dcl)
{
   TCHAR StrValue[10];

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   gs_s_scala((double *) dcl->client_data);
   swprintf(StrValue, 10, _T("%.3f"),  *((double *)dcl->client_data));
   ads_set_tile(dcl->dialog, _T("val_BlkScale"), StrValue);
}

//Gestione Larghezza
static void CALLB gs_editlarg(ads_callback_packet *dcl)
{
   double valore;
   TCHAR  valori[10];
 
   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   valore = _wtof(dcl->value);

   if (valore >= 0)
   {
      swprintf(valori, 10, _T("%.3f"), valore);
      ads_set_tile(dcl->dialog, _T("editlarg"), valori);
      if (valore == 2.0)
      {
         ads_set_tile(dcl->dialog, _T("radiolarg"),  _T("larg2"));
         ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
      }
      else if (valore == 5.0)
      {
         ads_set_tile(dcl->dialog, _T("radiolarg"), _T("larg5"));
         ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
      }     
      else if (valore == 10.0)
      {
         ads_set_tile(dcl->dialog, _T("radiolarg"), _T("larg10"));
         ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
      }
   }
   else ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidWidth));  // "*Errore* Larghezza non valida"
}

static void CALLB gs_radiolarg(ads_callback_packet *dcl)
{
  TCHAR valori[20];
 
  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
  wcscpy(valori, dcl->value);

  if (gsc_strcmp(valori, _T("larg2")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("editlarg"), _T("2.00"));
     ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori, _T("larg5")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("editlarg"), _T("5.00"));
     ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori, _T("larg10")) ==0 )
  {
     ads_set_tile (dcl->dialog, _T("editlarg"), _T("10.00"));
     ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_DISABLE);
  }
  else ads_mode_tile(dcl->dialog, _T("editlarg"), MODE_ENABLE);
}

static void CALLB gs_acp_larg(ads_callback_packet *dcl)
{
   TCHAR  appo[TILE_STR_LIMIT];
   double value;
   double *evid_value = (double *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   if (ads_get_tile(dcl->dialog, _T("editlarg"), appo, TILE_STR_LIMIT) != RTNORM) return;

   value = _wtof(appo);

   if (value >= 0)
   {
      *evid_value = value;
      ads_done_dialog(dcl->dialog, DLGOK);
   }
   else ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidWidth));  // "*Errore* Larghezza non valida"
}

static void CALLB gs_s_larg(ads_callback_packet *dcl)
{      // LARGHEZZA
   ads_hdlg llarg;
   int      dbstatus, dcl_file;
   TCHAR    valori[10];
   double   current;
   C_STRING path;
   double   *evid_value = (double *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   current = *evid_value;

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) != RTNORM) return;

   if (ads_new_dialog(_T("larghezza"), dcl_file, NULLCB, &llarg) != RTNORM)
      { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return; }

   swprintf(valori, 10, _T("%.3f"), current);
   ads_set_tile(llarg, _T("editlarg"), valori);
   if (current == 2.0)
   {
      ads_set_tile(llarg, _T("radiolarg"), _T("larg2"));
      ads_mode_tile(llarg, _T("editlarg"), MODE_DISABLE);
   }
   else if (current == 5.0)
   {
      ads_set_tile(llarg, _T("radiolarg"), _T("larg5"));
      ads_mode_tile(llarg, _T("editlarg"), MODE_DISABLE);
   }
   else if (current == 10.0)
   {
      ads_set_tile(llarg, _T("radiolarg"), _T("larg10"));
      ads_mode_tile(llarg, _T("editlarg"), MODE_DISABLE);
   }
   else ads_set_tile(llarg, _T("radiolarg"), _T("largutente"));

   ads_action_tile(llarg, _T("larg_no"),   (CLIENTFUNC)gs_can_dlg);
   ads_action_tile(llarg, _T("radiolarg"), (CLIENTFUNC)gs_radiolarg);
   ads_action_tile(llarg, _T("editlarg"),  (CLIENTFUNC)gs_editlarg);
   ads_action_tile(llarg, _T("larg_si"),   (CLIENTFUNC)gs_acp_larg);
   ads_client_data_tile(llarg, _T("larg_si"), evid_value);

   ads_start_dialog(llarg, &dbstatus);

   swprintf(valori, 10, _T("%.3f"), *evid_value);
   ads_set_tile(dcl->dialog, _T("val_la"), valori);

   ads_unload_dialog(dcl_file);
}

//Gestione Altezza testo
static void CALLB gs_edithtext(ads_callback_packet *dcl)
{
   double valore;
   TCHAR  valori[10];

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   valore = _wtof(dcl->value);

   if (valore > 0)
   {
      swprintf(valori, 10, _T("%.3f"), valore);
      ads_set_tile(dcl->dialog, _T("edithtext"), valori);
      if (valore == 2.0)
      {
         ads_set_tile(dcl->dialog, _T("radiohtext"), _T("htext2"));
         ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
      }
      else if (valore == 5.0)
      {
         ads_set_tile(dcl->dialog, _T("radiohtext"), _T("htext5"));
         ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
      }     
      else if (valore == 10.0)
      {
         ads_set_tile(dcl->dialog, _T("radiohtext"), _T("htext10"));
         ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
      }
   }
   else ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidH_text));  // "*Errore* Altezza testo non valida"
}

static void CALLB gs_radiohtext(ads_callback_packet *dcl)
{
  TCHAR valori[20];
 
  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
  wcscpy(valori, dcl->value);

  if (gsc_strcmp(valori, _T("htext2")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("edithtext"), _T("2.00"));
     ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori, _T("htext5")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("edithtext"), _T("5.00"));
     ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
  }
  else if (gsc_strcmp(valori, _T("htext10")) == 0)
  {
     ads_set_tile (dcl->dialog, _T("edithtext"), _T("10.00"));
     ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_DISABLE);
  }
  else ads_mode_tile(dcl->dialog, _T("edithtext"), MODE_ENABLE);
}

static void CALLB gs_acp_htext(ads_callback_packet *dcl)
{
  TCHAR  appo[TILE_STR_LIMIT];
  double value;
  double *evid_value = (double *) dcl->client_data;

  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

  if (ads_get_tile(dcl->dialog, _T("edithtext"), appo, TILE_STR_LIMIT) != RTNORM) return;

  value = _wtof(appo);

  if (value > 0)
  {
     *evid_value = value;
     ads_done_dialog(dcl->dialog,DLGOK);
  }
  else ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidH_text)); // "*Errore* Altezza testo non valida"
}

static void CALLB gs_s_htext(ads_callback_packet *dcl)
{      // ALTEZZA TESTO
   ads_hdlg lhtext;
   int      dbstatus, dcl_file;
   TCHAR    valori[10];
   double   current;
   C_STRING path;
   double   *evid_value = (double *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   current = *evid_value;

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) != RTNORM) return;

   if (ads_new_dialog(_T("altesto"), dcl_file, NULLCB, &lhtext) != RTNORM)
      { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return; }

   swprintf(valori, 10, _T("%.3f"), current);
   ads_set_tile(lhtext, _T("edithtext"), valori);

   if (current == 2.0)
   {
      ads_set_tile(lhtext, _T("radiohtext"), _T("htext2"));
      ads_mode_tile(lhtext, _T("edithtext"), MODE_DISABLE);
   }
   else if (current == 5.0)
   {
      ads_set_tile(lhtext, _T("radiohtext"), _T("htext5"));
      ads_mode_tile(lhtext, _T("edithtext"), MODE_DISABLE);
   }
   else if (current == 10.0)
   {
      ads_set_tile(lhtext, _T("radiohtext"), _T("htext10"));
      ads_mode_tile(lhtext, _T("edithtext"), MODE_DISABLE);
   }
   else ads_set_tile(lhtext, _T("radiohtext"), _T("altesto"));

   ads_action_tile(lhtext, _T("htext_no"),   (CLIENTFUNC)gs_can_dlg);
   ads_action_tile(lhtext, _T("radiohtext"), (CLIENTFUNC)gs_radiohtext);
   ads_action_tile(lhtext, _T("edithtext"),  (CLIENTFUNC)gs_edithtext);
   ads_action_tile(lhtext, _T("htext_si"),   (CLIENTFUNC)gs_acp_htext);
   ads_client_data_tile(lhtext, _T("htext_si"), evid_value);

   ads_start_dialog(lhtext, &dbstatus);

   swprintf(valori, 10, _T("%.3f"), *evid_value);
   ads_set_tile(dcl->dialog, _T("val_ht"), valori);

   ads_unload_dialog(dcl_file);
}


///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "evid" su controllo "opz_st"
///////////////////////////////////////////////////////////////////////////////
static void CALLB gs_s_stile(ads_callback_packet *dcl)
{      // STILE
   C_RB_LIST DescrTextStyle;
   TCHAR     *evid_value = (TCHAR *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   if ((DescrTextStyle << gsc_ddseltextstyles(evid_value)) == NULL)
      return;

   if (DescrTextStyle.get_head() != NULL)
   {
      gsc_strcpy(evid_value, DescrTextStyle.get_head()->resval.rstring, MAX_LEN_TEXTSTYLENAME);
      ads_set_tile(dcl->dialog, _T("val_st"), evid_value);
   }
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "evid" su controllo "opz_tp"
///////////////////////////////////////////////////////////////////////////////
static void CALLB gs_s_tipo(ads_callback_packet *dcl)
{  // TIPO LINEA
   C_RB_LIST DescrLineType;
   Common_Dcl_Evid_Struct *pCommonStru = (Common_Dcl_Evid_Struct *) dcl->client_data;
   TCHAR     *evid_value = (TCHAR *) pCommonStru->pFAS->line;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   // Richiedo il tipolinea
   if ((DescrLineType << gsc_ddsellinetype(evid_value)) == NULL) return;

   gsc_strcpy(evid_value, DescrLineType.get_head()->resval.rstring, MAX_LEN_LINETYPENAME);
   ads_set_tile(dcl->dialog, _T("val_tp"), evid_value);

   // Se abilitata anche la scala del tipo linea
   if (pCommonStru->EnabledFas & GSLineTypeScaleSetting)
   {
      int res;
      // "Si desidera impostare anche la scala del tipolinea ?"
      if (gsc_ddgetconfirm(gsc_msg(472), &res) == GS_GOOD && res == GS_GOOD)
      {
         C_STRING Msg(evid_value);

         gs_s_scala(&(pCommonStru->pFAS->line_scale));
         (* pCommonStru->flag_set) = (* pCommonStru->flag_set) | GSLineTypeScaleSetting;
         Msg += _T(" (");
         Msg += gsc_msg(467); // Scala
         Msg += _T("=");
         Msg += pCommonStru->pFAS->line_scale;
         Msg += _T(")");
         ads_set_tile(dcl->dialog, _T("val_tp"), Msg.get_name());
      }
      else // elimino l'opzione
         (* pCommonStru->flag_set) = (* pCommonStru->flag_set) & (~GSLineTypeScaleSetting);
   }
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "evid" su controllo "opz_rm" RIEMPIMENTO - INIZIO
///////////////////////////////////////////////////////////////////////////////
static void CALLB gs_s_HatchName(ads_callback_packet *dcl)
{  // Nome riempimento
   C_RB_LIST DescrHatch;
   TCHAR     Value[MAX_LEN_HATCHNAME];

   ads_get_tile(dcl->dialog, _T("e_fill"), Value, MAX_LEN_HATCHNAME); // edit box nome riempimento
   // Richiedo il riempimento
   if ((DescrHatch << gsc_ddselhatch(Value)) == NULL) return;

   gsc_strcpy(Value, DescrHatch.get_head()->resval.rstring, MAX_LEN_HATCHNAME);
   ads_set_tile(dcl->dialog, _T("e_fill"), Value);
}
static void CALLB gs_s_HatchColor(ads_callback_packet *dcl)
{  // Colore riempimento
   C_COLOR  Value, CurLayerColor;
   TCHAR    StrValue[TILE_STR_LIMIT];
   C_STRING StrColor;

   ads_get_tile(dcl->dialog, _T("e_fill_color"), StrValue, TILE_STR_LIMIT); // edit box nome riempimento
   Value.setString(StrValue);
   // colore del riempimento
   CurLayerColor.setForeground();
   if (gsc_SetColorDialog(Value, true, CurLayerColor) == GS_GOOD)
   {
      if (Value.getColorMethod() == C_COLOR::ByBlock)
         ads_set_tile(dcl->dialog, _T("e_fill_color"), gsc_msg(24)); // "DABLOCCO"
      else if (Value.getColorMethod() == C_COLOR::ByLayer)
         ads_set_tile(dcl->dialog, _T("e_fill_color"), gsc_msg(23)); // "DALAYER"
      else 
      {
         Value.getString(StrColor);
         ads_set_tile(dcl->dialog, _T("e_fill_color"), StrColor.get_name());
      }
   }
}
// Gestione layer per entità principali
static void CALLB gs_s_HatchLayer(ads_callback_packet *dcl)
{
   TCHAR Value[MAX_LEN_LAYERNAME];

   ads_get_tile(dcl->dialog, _T("e_fill_layer"), Value, MAX_LEN_LAYERNAME); // edit box layer riempimento
   gs_s_layer(Value);
   ads_set_tile(dcl->dialog, _T("e_fill_layer"), Value);
}

static void CALLB gs_acp_Hatch(ads_callback_packet *dcl)
{
   Common_Dcl_Evid_Struct *pCommonStru = (Common_Dcl_Evid_Struct *) dcl->client_data;
   TCHAR                  StrValue[TILE_STR_LIMIT];
   C_STRING               strColor;

   ads_get_tile(dcl->dialog, _T("e_fill"), StrValue, TILE_STR_LIMIT); // edit box nome riempimento
   gsc_strcpy(pCommonStru->pFAS->hatch, StrValue, MAX_LEN_HATCHNAME);

   ads_get_tile(dcl->dialog, _T("e_fill_rotaz"), StrValue, TILE_STR_LIMIT); // edit box rotazione
   pCommonStru->pFAS->hatch_rotation = _wtof(StrValue);

   ads_get_tile(dcl->dialog, _T("e_fill_scale"), StrValue, TILE_STR_LIMIT); // edit box scala
   pCommonStru->pFAS->hatch_scale = _wtof(StrValue);

   ads_get_tile(dcl->dialog, _T("e_fill_layer"), StrValue, TILE_STR_LIMIT); // edit box layer
   gsc_strcpy(pCommonStru->pFAS->hatch_layer, StrValue, MAX_LEN_LAYERNAME);

   ads_get_tile(dcl->dialog, _T("e_fill_color"), StrValue, TILE_STR_LIMIT); // edit box colore
   pCommonStru->pFAS->hatch_color.setString(StrValue);

   ads_done_dialog(dcl->dialog, DLGOK);
}
static void CALLB gs_s_hatch(ads_callback_packet *dcl)
{
   C_STRING               path;
   int                    dbstatus, dcl_file;
   ads_hdlg               hDlg;
   C_RB_LIST              DescrHatch;
   Common_Dcl_Evid_Struct *pCommonStru = (Common_Dcl_Evid_Struct *) dcl->client_data;
   C_STRING               StrValue;
   short                  ModeTile;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) != RTNORM) return;

   if (ads_new_dialog(_T("HatchGraphPropsChange"), dcl_file, NULLCB, &hDlg) != RTNORM)
      { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return; }
   // setto i valori
   ads_set_tile(hDlg, _T("e_fill"), pCommonStru->pFAS->hatch); // edit box nome riempimento
   StrValue = pCommonStru->pFAS->hatch_rotation;
   ads_set_tile(hDlg, _T("e_fill_rotaz"), StrValue.get_name()); // edit box rotazione
   StrValue = pCommonStru->pFAS->hatch_scale;
   ads_set_tile(hDlg, _T("e_fill_scale"), StrValue.get_name()); // edit box scala
   ads_set_tile(hDlg, _T("e_fill_layer"), pCommonStru->pFAS->hatch_layer); // edit box layer

   pCommonStru->pFAS->hatch_color.getString(StrValue);
   ads_set_tile(hDlg, _T("e_fill_color"), StrValue.get_name()); // edit box colore

   // Abilito i controlli
   ModeTile = (pCommonStru->EnabledFas & GSHatchNameSetting) ? MODE_ENABLE : MODE_DISABLE;
   ads_mode_tile(hDlg, _T("e_fill"), ModeTile);
   ads_mode_tile(hDlg, _T("le_fill"), ModeTile);
   ModeTile = (pCommonStru->EnabledFas & GSHatchRotationSetting) ? MODE_ENABLE : MODE_DISABLE;
   ads_mode_tile(hDlg, _T("e_fill_rotaz"), ModeTile);
   ModeTile = (pCommonStru->EnabledFas & GSHatchScaleSetting) ? MODE_ENABLE : MODE_DISABLE;
   ads_mode_tile(hDlg, _T("e_fill_scale"), ModeTile);
   ModeTile = (pCommonStru->EnabledFas & GSHatchLayerSetting) ? MODE_ENABLE : MODE_DISABLE;
   ads_mode_tile(hDlg, _T("e_fill_layer"), ModeTile);
   ads_mode_tile(hDlg, _T("le_fill_layer"), ModeTile);
   ModeTile = (pCommonStru->EnabledFas & GSHatchColorSetting) ? MODE_ENABLE : MODE_DISABLE;
   ads_mode_tile(hDlg, _T("e_fill_color"), ModeTile);
   ads_mode_tile(hDlg, _T("le_fill_color"), ModeTile);

   // Inizializzo eventi
   ads_action_tile(hDlg, _T("le_fill"), (CLIENTFUNC) gs_s_HatchName); // scelta riempimento
   ads_action_tile(hDlg, _T("le_fill_layer"), (CLIENTFUNC) gs_s_HatchLayer); // scelta layer
   ads_action_tile(hDlg, _T("le_fill_color"), (CLIENTFUNC) gs_s_HatchColor); // scelta colore
   ads_action_tile(hDlg, _T("help"),  (CLIENTFUNC) dcl_evid_help);
   ads_action_tile(hDlg, _T("accept"), (CLIENTFUNC)gs_acp_Hatch);
   ads_client_data_tile(hDlg, _T("accept"), pCommonStru);

   ads_start_dialog(hDlg, &dbstatus);
   ads_unload_dialog(dcl_file);
   if (dbstatus == DLGOK)
      ads_set_tile(dcl->dialog, _T("val_rm"), pCommonStru->pFAS->hatch);
}

///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "evid" su controllo "opz_rm" - FINE
///////////////////////////////////////////////////////////////////////////////

static void CALLB gs_s_color(ads_callback_packet *dcl)
{ // COLORE
   C_COLOR  *evid_value = (C_COLOR *) dcl->client_data, CurLayerColor;
   C_STRING StrColor;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);
   CurLayerColor.setForeground();
   if (gsc_SetColorDialog(*evid_value, true, CurLayerColor) == GS_GOOD)
   {
      if (evid_value->getColorMethod() == C_COLOR::ByBlock)
         ads_set_tile(dcl->dialog, _T("val_cl"), gsc_msg(24)); // "DABLOCCO"
      else if (evid_value->getColorMethod() == C_COLOR::ByLayer)
         ads_set_tile(dcl->dialog, _T("val_cl"), gsc_msg(23)); // "DALAYER"
      else 
      {
         evid_value->getString(StrColor);
         ads_set_tile(dcl->dialog, _T("val_cl"), StrColor.get_name());
      }
   }
}


static void CALLB gs_s_blocco(ads_callback_packet *dcl)
{  // BLOCCO
   C_RB_LIST DescrBlock;
   TCHAR     *evid_value = (TCHAR *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   if ((DescrBlock << gsc_uiSelBlock(evid_value)) == NULL) return;

   if (DescrBlock.get_head() != NULL)
   {
      gsc_strcpy(evid_value, DescrBlock.get_head()->resval.rstring, MAX_LEN_BLOCKNAME);
      ads_set_tile(dcl->dialog, _T("val_b"), evid_value);
   }
}


// Gestione finestra minima di zooom
static void CALLB gs_editzoom(ads_callback_packet *dcl)
{
  ads_real valore;
 
  ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

  valore = _wtof(dcl->value);
  // "*Errore* Argomento non valido"
  if (valore <= 0) ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidArg));
}

static void CALLB gs_acp_zoom(ads_callback_packet *dcl)
{
   TCHAR   appo[TILE_STR_LIMIT];
   double  value;
   double  *evid_value = (double *) dcl->client_data;

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   if (ads_get_tile(dcl->dialog, _T("editzoom"), appo, TILE_STR_LIMIT) != RTNORM) return;
   value = _wtof(appo);

   // "*Errore* Argomento non valido"
   if (value <= 0) ads_set_tile(dcl->dialog, _T("error"), gsc_err(eGSInvalidArg));
   else
   {
      *evid_value = value;
      ads_done_dialog(dcl->dialog, DLGOK);
   }
}

static void CALLB gs_op_zoom(ads_callback_packet *dcl)
{  // ZOOM
   ads_hdlg zoom_dlg;
   int      dbstatus, dcl_file;
   TCHAR    valori[10];
   ads_real current;
   C_STRING path;
   double   *evid_value = (double *) dcl->client_data;

   current = *evid_value;

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) != RTNORM) return;

   if (ads_new_dialog(_T("zoom"), dcl_file, NULLCB, &zoom_dlg) != RTNORM)
      { ads_unload_dialog(dcl_file); GS_ERR_COD = eGSAbortDCL; return; }

   ads_set_tile(dcl->dialog, _T("error"), GS_EMPTYSTR);

   swprintf(valori, 10, _T("%.3f"), current);
   ads_set_tile(zoom_dlg, _T("editzoom"), valori);

   ads_action_tile(zoom_dlg, _T("zoom_no"), (CLIENTFUNC)gs_can_dlg);
   ads_action_tile(zoom_dlg, _T("editzoom"), (CLIENTFUNC)gs_editzoom);
   ads_action_tile(zoom_dlg, _T("zoom_si"), (CLIENTFUNC)gs_acp_zoom);
   ads_client_data_tile(zoom_dlg, _T("zoom_si"), evid_value);

   ads_start_dialog(zoom_dlg, &dbstatus);

   swprintf(valori, 10, _T("%.3f"), *evid_value);
   ads_set_tile(dcl->dialog, _T("val_zoom"), valori);
   ads_unload_dialog(dcl_file);
}
// ACTION TILE : click su tasto HELP //
static void CALLB dcl_evid_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Evidenziazionerisultatifiltro); } 


/************************************************************/
/*.doc gsc_ddChooseGraphSettings                 <external> */
/*+
  Questa funzione chiede all'utente le caratteristiche grafiche
  da modificare per una classe (o sottoclasse) di GEOsim scelte 
  tramite interfaccia DCL.
  Parametri:
  C_CLASS *p_class;        puntatore alla classe
  bool    highlightOption; flag di modo;
                           se = TRUE si vogliono evidenziare degli oggetti nella
                           sessione di lavoro corrente per cui sarà abilitata 
                           l'opzione di zoom, evidenziazione, lampeggio,
                           ins. in salvataggio (altrimenti disabilitate)
                           e non verrà richiesto il layer su scelta riempimento.
  long *flag_set;          flag a bit che identifica quali azioni si vogliono 
                           intraprendere (vedi GraphSettingsEnum)
  C_FAS   &GraphSettings;  valori per le modifiche da intraprendere (settati da flag_set)
  double  *MinZoomWindow;  Finestra minima di zoom (usato se highlightOption = TRUE)
  int     *what;           Tipo di oggetti da trattare (usato se highlightOption = TRUE)
                           GRAPHICAL includi solo gli oggetti grafici principali
                           DA_BLOCK  includi solo i blocchi degli attributi visibili
                           ALL       includi entrambi
  bool DefaultInit;        Flag di inizializzazione della finestra. 
                           Se = TRUE inizializza la finestra con i seguenti
                           valori di default: 
                           1) caratteristiche grafiche della classe
                           2) nessuna opzione grafica abilitata tranne nel caso 
                              in cui highlightOption = TRUE che setta
                              GSHighlightSetting + GSZoomSetting.
                           3) MinZoomWindow = al massimo tra ciò che è impostato a
                              livello di zoom automatico X e Y nelle variabili di ambiente
                           4) what = GRAPHICAL
                           Se = FALSE inizializza la finestra come da valori 
                           nei parametri highlightOption, flag_set, GraphSettings,
                           MinZoomWindow, what.

  oppure per oggetti non appartenenti a GEOsim:

  long    FlagFas;
  bool    highlightOption;
  long    *flag_set;
  C_FAS   &GraphSettings;
  double  *MinZoomWindow;

  Restituisce GS_GOOD in caso di successo, GS_CAN in caso di annullamento
  da parte dell'utente, GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gsc_ddChooseGraphSettings(C_CLASS *p_class, bool highlightOption, 
                              long *flag_set, C_FAS &GraphSettings, 
                              double *MinZoomWindow, int *What, bool DefaultInit)
{
   C_STRING  path;
   long      FlagFas = GSNoneSetting;

   if (p_class)
   {
      // leggo quali caratteristiche grafiche si possono modificare:
      FlagFas = p_class->what_is_graph_updateable();
      // Se devo evidenziare
      if (highlightOption)
      {
         // Se si tratta di classe superficie i riempimenti sono inseriti come nuovi
         // oggetti autocad e non vengono modificati i riempimenti esistenti
         // per cui abilito tutte le caratteristiche dei riempimenti
         // anche se potrebbero essere disabilitate da funzioni di calcolo grafico
         if ((p_class->get_category() == CAT_SIMPLEX || p_class->get_category() == CAT_SUBCLASS) &&
             p_class->get_type() == TYPE_SURFACE)
         {
            FlagFas = FlagFas | GSHatchNameSetting;
            FlagFas = FlagFas | GSHatchLayerSetting;
            FlagFas = FlagFas | GSHatchColorSetting;
            FlagFas = FlagFas | GSHatchScaleSetting;
            FlagFas = FlagFas | GSHatchRotationSetting;
         }
      }
   }
   else
      FlagFas = *flag_set;

   if (DefaultInit) // Inizializzo con i default
   {
      if (highlightOption)
         *flag_set = GSHighlightSetting + GSZoomSetting; // zoom + evidenzia 
      else
         *flag_set = 0;

      if (p_class && p_class->ptr_fas())
         p_class->ptr_fas()->copy(&GraphSettings);
      else
         gsc_getdefaultFAS(GraphSettings); 

      // Dimensione min. finestra di zoom
      *MinZoomWindow = max(GEOsimAppl::GLOBALVARS.get_AutoZoomMinXDim(), 
                           GEOsimAppl::GLOBALVARS.get_AutoZoomMinYDim());
      *What = GRAPHICAL;
   }

   return gsc_ddChooseGraphSettings(FlagFas, highlightOption, 
                                    flag_set, GraphSettings, 
                                    MinZoomWindow, What);
}
int gsc_ddChooseGraphSettings(long EnabledFas, bool highlightOption, 
                              long *flag_set, C_FAS &GraphSettings, 
                              double *MinZoomWindow, int *What)
{
   int       dbstatus, dcl_file;
   ads_hdlg  d_evid;
   TCHAR     valori[60];
   C_STRING  path;
   Common_Dcl_Evid_Struct CommonStru;

   CommonStru.EnabledFas = EnabledFas; // caratteristiche settabili
   CommonStru.flag_set   = flag_set;
   CommonStru.pFAS       = &GraphSettings;

   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path.get_name(), &dcl_file) != RTNORM) return GS_BAD;

   if (highlightOption)
      dbstatus = ads_new_dialog(_T("evid"), dcl_file, NULLCB, &d_evid);
   else
      dbstatus = ads_new_dialog(_T("GraphPropsChangeNoEvid"), dcl_file, NULLCB, &d_evid);

   if (dbstatus != RTNORM)
   {
      ads_unload_dialog(dcl_file);
      GS_ERR_COD = eGSAbortDCL; 
      return GS_BAD;                                           
   }

   // Inizializzo la finestra
   
   // Inizializzo i toggle 
   ads_set_tile(d_evid, _T("togglepiano"), (*flag_set & GSLayerSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("toggleblocco"), (*flag_set & GSBlockNameSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("togglestile"), (*flag_set & GSTextStyleSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("togglealtesto"), (*flag_set & GSTextHeightSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("toggleriem"), (*flag_set & GSHatchNameSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("togglescala"), (*flag_set & GSBlockScaleSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("toggletipo"), (*flag_set & GSLineTypeSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("togglelarg"), (*flag_set & GSWidthSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("togglecolore"), (*flag_set & GSColorSetting) ? _T("1") : _T("0"));      
   ads_set_tile(d_evid, _T("togglezoom"), (*flag_set & GSZoomSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("toggleev"), (*flag_set & GSHighlightSetting) ? _T("1") : _T("0"));
   ads_set_tile(d_evid, _T("toggleblink"), (*flag_set & GSBlinkingSetting) ? _T("1") : _T("0"));

   ads_set_tile(d_evid, _T("val_p"), GraphSettings.layer);
   ads_set_tile(d_evid, _T("val_b"), GraphSettings.block);
   swprintf(valori, 60, _T("%.3f"), GraphSettings.block_scale);
   ads_set_tile(d_evid, _T("val_BlkScale"), valori);
   ads_set_tile(d_evid, _T("val_st"), GraphSettings.style);
   swprintf(valori, 60, _T("%.3f"), GraphSettings.h_text);
   ads_set_tile(d_evid, _T("val_ht"), valori);
   ads_set_tile(d_evid, _T("val_rm"), GraphSettings.hatch);
   ads_set_tile(d_evid, _T("val_tp"), GraphSettings.line);
   swprintf(valori, 60, _T("%.3f"), GraphSettings.width);
   ads_set_tile(d_evid, _T("val_la"), valori);

   if (GraphSettings.color.getColorMethod() == C_COLOR::ByBlock)
      ads_set_tile(d_evid, _T("val_cl"), gsc_msg(24)); // "DABLOCCO"
   else if (GraphSettings.color.getColorMethod() == C_COLOR::ByLayer)
      ads_set_tile(d_evid, _T("val_cl"), gsc_msg(23)); // "DALAYER"
   else
   {
      C_STRING StrColor;

      GraphSettings.color.getString(StrColor);
      ads_set_tile(d_evid, _T("val_cl"), StrColor.get_name());
   }

   if (highlightOption)
   {
      swprintf(valori, 60, _T("%.3f"), *MinZoomWindow);
      ads_set_tile(d_evid, _T("val_zoom"), valori);
   }

   // Inizializzo i toggle
   ads_callback_packet InitCallBackPacket;
   InitCallBackPacket.dialog = d_evid;
   InitCallBackPacket.client_data = flag_set;
   InitCallBackPacket.value = (TCHAR *) malloc(sizeof(TCHAR) * 2);
   
   // Se il piano è una caratteristica usabile per la classe
   if (CommonStru.EnabledFas & GSLayerSetting)
   {
      ads_mode_tile(d_evid, _T("togglepiano"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSLayerSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("togglepiano"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_1(&InitCallBackPacket);

   // Se il nome del blocco è una caratteristica usabile per la classe
   if (CommonStru.EnabledFas & GSBlockNameSetting)
   {
      ads_mode_tile(d_evid, _T("toggleblocco"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSBlockNameSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("toggleblocco"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_2(&InitCallBackPacket);

   // Se il nome dello stile testo è una caratteristica usabile per la classe
   if (CommonStru.EnabledFas & GSTextStyleSetting)
   {
      ads_mode_tile(d_evid, _T("togglestile"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSTextStyleSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("togglestile"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_4(&InitCallBackPacket);

   // Se il nome del riempimento è una caratteristica usabile per la classe
   if (CommonStru.EnabledFas & GSHatchNameSetting)
   {
      ads_mode_tile(d_evid, _T("toggleriem"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSHatchNameSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("toggleriem"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_256(&InitCallBackPacket);

   // Se la scala è una caratteristica usabile per la classe  
   if (CommonStru.EnabledFas & GSBlockScaleSetting)
   {
      ads_mode_tile(d_evid, _T("togglescala"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSBlockScaleSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("togglescala"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_8(&InitCallBackPacket);

   // Se il tipo linea è una caratteristica usabile per la classe  
   if (CommonStru.EnabledFas & GSLineTypeSetting)
   {
      ads_mode_tile(d_evid, _T("toggletipo"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSLineTypeSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("toggletipo"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_16(&InitCallBackPacket);

   // Se la larghezza della linea è una caratteristica usabile per la classe  
   if (CommonStru.EnabledFas & GSWidthSetting)
   {
      ads_mode_tile(d_evid, _T("togglelarg"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSWidthSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("togglelarg"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_32(&InitCallBackPacket);


   // Se il colore è una caratteristica usabile per la classe  
   if (CommonStru.EnabledFas & GSColorSetting)
   {
      ads_mode_tile(d_evid, _T("togglecolore"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSColorSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("togglecolore"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_color(&InitCallBackPacket);

   // Se l'altezza testo è una caratteristica usabile per la classe  
   if (CommonStru.EnabledFas & GSTextHeightSetting)
   {
      ads_mode_tile(d_evid, _T("togglealtesto"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSTextHeightSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("togglealtesto"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_128(&InitCallBackPacket);

   // Se lo zoom è una caratteristica usabile in questo contesto
   if (highlightOption)
   {
      ads_mode_tile(d_evid, _T("togglezoom"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSZoomSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("togglezoom"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_16384(&InitCallBackPacket);

   // Se l'evidenziazione è una caratteristica usabile in questo contesto
   if (highlightOption)
   {
      ads_mode_tile(d_evid, _T("toggleev"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSHighlightSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("toggleev"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_8192(&InitCallBackPacket);

   // Se il lampeggio è una caratteristica usabile in questo contesto
   if (highlightOption)
   {
      ads_mode_tile(d_evid, _T("toggleblink"), MODE_ENABLE);
      gsc_strcpy(InitCallBackPacket.value, (*flag_set & GSBlinkingSetting) ? _T("1") : _T("0"), 2);
   }
   else
   {
      ads_mode_tile(d_evid, _T("toggleblink"), MODE_DISABLE);
      gsc_strcpy(InitCallBackPacket.value, _T("0"), 2);
   }
   gs_op_65536(&InitCallBackPacket);

   // Se la tipologia di oggetti è una caratteristica usabile in questo contesto
   if (highlightOption)
   {
      ads_mode_tile(d_evid, _T("main_ent"), MODE_ENABLE);
      ads_set_tile(d_evid, _T("main_ent"), (*What == GRAPHICAL || *What == ALL) ? _T("1") : _T("0"));
      ads_mode_tile(d_evid, _T("da_ent"), MODE_ENABLE);
      ads_set_tile(d_evid, _T("da_ent"), (*What == DA_BLOCK || *What == ALL) ? _T("1") : _T("0"));
   }
   else
   {
      ads_mode_tile(d_evid, _T("main_ent"), MODE_DISABLE);
      ads_set_tile(d_evid, _T("main_ent"), _T("0"));
      ads_mode_tile(d_evid, _T("da_ent"), MODE_DISABLE);
      ads_set_tile(d_evid, _T("da_ent"), _T("0"));
   }

   if (highlightOption)
   {
      // Se la sessione non è in modifica oppure si vuole riallineare la fas degli 
      // oggetti evidenziati allora è inutile aggiungere gli oggetti al salvataggio
      if (GS_CURRENT_WRK_SESSION->get_level() != GSUpdateableData || 
          GEOsimAppl::GLOBALVARS.get_AlignHighlightedFASOnSave() == GS_GOOD)
      {
         ads_mode_tile(d_evid, _T("add_object2saveset"), MODE_DISABLE);
         ads_set_tile(d_evid, _T("add_object2saveset"), (*flag_set & GSSaveAlteredObjs) ? _T("1") : _T("0"));
      }
      else
      {
         ads_mode_tile(d_evid, _T("add_object2saveset"), MODE_ENABLE);
         ads_set_tile(d_evid, _T("add_object2saveset"), _T("0"));
      }
   }
   else
   {
      ads_mode_tile(d_evid, _T("add_object2saveset"), MODE_DISABLE);
      ads_set_tile(d_evid, _T("add_object2saveset"), _T("0"));
   }

   free(InitCallBackPacket.value);

   ads_mode_tile(d_evid,   _T("accept"), MODE_ENABLE);
   ads_mode_tile(d_evid,   _T("cancel"), MODE_ENABLE);
   ads_mode_tile(d_evid,   _T("help"),   MODE_ENABLE);
   ads_action_tile(d_evid, _T("help"),  (CLIENTFUNC) dcl_evid_help);

   // Inizializzo eventi
   ads_action_tile(d_evid, _T("togglepiano"), (CLIENTFUNC)gs_op_1);
   ads_client_data_tile(d_evid, _T("togglepiano"), flag_set);
   ads_action_tile(d_evid, _T("opz_p"), (CLIENTFUNC)gs_s_MainEntLayer);
   ads_client_data_tile(d_evid, _T("opz_p"), GraphSettings.layer);

   ads_action_tile(d_evid, _T("toggleblocco"), (CLIENTFUNC)gs_op_2);
   ads_client_data_tile(d_evid, _T("toggleblocco"), flag_set);
   ads_action_tile(d_evid, _T("opz_b"), (CLIENTFUNC)gs_s_blocco);
   ads_client_data_tile(d_evid, _T("opz_b"), GraphSettings.block);
   
   ads_action_tile(d_evid, _T("togglestile"), (CLIENTFUNC)gs_op_4);
   ads_client_data_tile(d_evid, _T("togglestile"), flag_set);
   ads_action_tile(d_evid, _T("opz_st"), (CLIENTFUNC)gs_s_stile);
   ads_client_data_tile(d_evid, _T("opz_st"), GraphSettings.style);
   
   ads_action_tile(d_evid, _T("togglealtesto"), (CLIENTFUNC)gs_op_128);
   ads_client_data_tile(d_evid, _T("togglealtesto"), flag_set);
   ads_action_tile(d_evid, _T("opz_th"), (CLIENTFUNC)gs_s_htext);
   ads_client_data_tile(d_evid, _T("opz_th"), &(GraphSettings.h_text));
   
   ads_action_tile(d_evid, _T("toggleriem"), (CLIENTFUNC)gs_op_256);
   ads_client_data_tile(d_evid, _T("toggleriem"), flag_set);
   ads_action_tile(d_evid, _T("opz_rm"), (CLIENTFUNC)gs_s_hatch);
   ads_client_data_tile(d_evid, _T("opz_rm"), &CommonStru);
   
   ads_action_tile(d_evid, _T("togglescala"), (CLIENTFUNC)gs_op_8);
   ads_client_data_tile(d_evid, _T("togglescala"), flag_set);
   ads_action_tile(d_evid, _T("opz_sc"), (CLIENTFUNC)gs_s_BlkScale);
   ads_client_data_tile(d_evid, _T("opz_sc"), &(GraphSettings.block_scale));
   
   ads_action_tile(d_evid, _T("toggletipo"), (CLIENTFUNC)gs_op_16);
   ads_client_data_tile(d_evid, _T("toggletipo"), flag_set);
   ads_action_tile(d_evid, _T("opz_tp"), (CLIENTFUNC)gs_s_tipo);
   ads_client_data_tile(d_evid, _T("opz_tp"), &CommonStru);
   
   ads_action_tile(d_evid, _T("togglelarg"), (CLIENTFUNC)gs_op_32);
   ads_client_data_tile(d_evid, _T("togglelarg"), flag_set);
   ads_action_tile(d_evid, _T("opz_la"), (CLIENTFUNC)gs_s_larg);
   ads_client_data_tile(d_evid, _T("opz_la"), &(GraphSettings.width));
   
   ads_action_tile(d_evid, _T("togglecolore"), (CLIENTFUNC)gs_op_color);
   ads_client_data_tile(d_evid, _T("togglecolore"), flag_set);
   ads_action_tile(d_evid, _T("opz_cl"), (CLIENTFUNC)gs_s_color);
   ads_client_data_tile(d_evid, _T("opz_cl"), &(GraphSettings.color));
   
   ads_action_tile(d_evid, _T("togglezoom"), (CLIENTFUNC)gs_op_16384);
   ads_client_data_tile(d_evid, _T("togglezoom"), flag_set);
   ads_action_tile(d_evid, _T("opz_zoom"), (CLIENTFUNC)gs_op_zoom);
   ads_client_data_tile(d_evid, _T("opz_zoom"), MinZoomWindow);
  
   ads_action_tile(d_evid, _T("toggleev"), (CLIENTFUNC)gs_op_8192);
   ads_client_data_tile(d_evid, _T("toggleev"), flag_set);

   ads_action_tile(d_evid, _T("toggleblink"), (CLIENTFUNC)gs_op_65536);
   ads_client_data_tile(d_evid, _T("toggleblink"), flag_set);

   ads_action_tile(d_evid, _T("add_object2saveset"), (CLIENTFUNC)gs_op_Obj2SaveSet);
   ads_client_data_tile(d_evid, _T("add_object2saveset"), flag_set);

   ads_action_tile(d_evid, _T("cancel"), (CLIENTFUNC)gs_can_dlg);
   ads_action_tile(d_evid, _T("accept"), (CLIENTFUNC)gs_acp_dlg);
   ads_client_data_tile(d_evid, _T("accept"), What);

   ads_start_dialog(d_evid, &dbstatus);
   ads_unload_dialog(dcl_file);

   // Se Si è scelto OK
   if (dbstatus == DLGOK)
   {
      if (highlightOption)
      {
         // Se non si è scelto di evidenziare qualcosa 
         // (oggetti grafici o blocchi DA o entrambi)
         if (*What == -1) return GS_CAN;
      
         // Se NON si è scelta qualche opzione di evidenziazione
         long dummy = *flag_set;   
         if (dummy & GSSaveAlteredObjs) dummy -= GSSaveAlteredObjs;

         return (dummy == GSNoneSetting) ? GS_CAN : GS_GOOD;
      }

      return GS_GOOD;
   }
   
   return GS_CAN;
}


/*********************************************************/
/*.doc gs_ddChooseGraphSettings               <external> */
/*+
  Questa funzione LISP viene usata per settare le caratteristiche grafiche 
  da cambiare.
  Lista resbuf: <prj><cls>[<sub>](flag_set(FAS))
    
  Restituisce una lista così composta in caso di successo:
  (flag_set(FAS))
  altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_ddChooseGraphSettings(void)
{
   resbuf    *arg;
   int       prj, cls, sub, dummyInt;
   C_CLASS   *pCls = NULL;
   C_FAS     FAS;
   double    dummyDbl;
   long      flag_set = GSNoneSetting;
   C_RB_LIST ResultList;

   acedRetNil();

   // Legge nella lista dei parametri il progetto
   arg = acedGetArgs();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_GOOD)
   {
      // Ritorna il puntatore alla classe cercata
      if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;
      if (pCls->ptr_fas() == NULL)
         { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   }

   if (arg) // Aperta tonda
   {
      if (!(arg = arg->rbnext)) return RTERROR;
      if (gsc_rb2Lng(arg, &flag_set) != GS_GOOD && pCls)
         // leggo quali caratteristiche grafiche si possono modificare dalla classe
         flag_set = pCls->what_is_graph_updateable();

      if (!(arg = arg->rbnext))
      {
         if (pCls && pCls->ptr_fas()) pCls->ptr_fas()->copy(&FAS);
      }
      else
         if (FAS.from_rb(arg) != GS_GOOD)
            if (pCls && pCls->ptr_fas()) pCls->ptr_fas()->copy(&FAS);
   }
   else
   {
      if (!pCls || pCls->ptr_fas() == NULL) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
      // leggo quali caratteristiche grafiche si possono modificare
      flag_set = pCls->what_is_graph_updateable();
      pCls->ptr_fas()->copy(&FAS);
   }

   if (gsc_ddChooseGraphSettings(pCls, FALSE, &flag_set, FAS, 
                                 &dummyDbl, &dummyInt, FALSE) != GS_GOOD)
      return RTNORM;

   if ((ResultList += acutBuildList(RTREAL, (double) flag_set, RTLB, 0)) == NULL) return GS_BAD;
   if ((ResultList += FAS.to_rb()) == NULL) return GS_BAD;
   if ((ResultList += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;
   ResultList.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_ddevid <external> */
/*+
  Questa funzione evidenzia gli oggetti grafici di una classe in base a criteri
  scelti tramite interfaccia DCL.
  Parametri:
  C_CLASS *p_class;  puntatore alla classe madre degli oggetti da evidenziare
  C_SELSET &selset;   gruppo di oggetti da evidenziare 

  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************/
int gsc_ddevid(C_CLASS *p_class, C_SELSET &selset)
{
   int      result, What;
   C_STRING path, HatchName;
   long     flag_set;
   C_FAS    GraphSettings;
   double   MinZoomWindow;
   bool     DefaultInit = true;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (selset.length() == 0) return GS_GOOD;

   // Provo a caricare le ultime opzioni di evidenziazione usate
   if (gsc_LoadEvidOptions(p_class->get_type(), &flag_set, GraphSettings) == GS_GOOD)
   {
      // ripropongo i valori di default della classe
      if (p_class && p_class->ptr_fas())
         p_class->ptr_fas()->copy(&GraphSettings);
      else
         gsc_getdefaultFAS(GraphSettings); 

      // Dimensione min. finestra di zoom
      MinZoomWindow = max(GEOsimAppl::GLOBALVARS.get_AutoZoomMinXDim(), 
                          GEOsimAppl::GLOBALVARS.get_AutoZoomMinYDim());
      What = GRAPHICAL;
      DefaultInit = false;
   }

   if ((result = gsc_ddChooseGraphSettings(p_class,
                                           TRUE, &flag_set, GraphSettings, 
                                           &MinZoomWindow, &What, DefaultInit)) == GS_BAD) return GS_BAD;

   // Salvo le ultime opzioni di evidenziazione usate
   gsc_SaveEvidOptions(p_class->get_type(), flag_set, GraphSettings);

   // Ripulisce gruppo di selezione AUTOCAD
   gsc_clear_pick_first();

   if ((flag_set > 0) && result == GS_GOOD)
   {  // faccio evidenziazioni
      C_SELSET ss_evid;

      if (selset.copy(ss_evid) == GS_BAD) return GS_BAD; 

      if (What == ALL || What == DA_BLOCK) // se si vogliono includere anche i blocchi DA
      {
         C_CLASS *pCls;
         long     i = 0;
         ads_name ent;
         C_EED    eed;
         C_SELSET DispAttribSS, entSS;

         while (ss_evid.entname(i++, ent) == GS_GOOD)
            if (eed.load(ent) == GS_GOOD &&
                (pCls = GS_CURRENT_WRK_SESSION->get_pPrj()->find_class(eed.cls, eed.sub)) &&
                pCls->get_SelSet(eed.gs_id, entSS, DA_BLOCK) == GS_GOOD)
               DispAttribSS.add_selset(entSS);

         ss_evid.add_selset(DispAttribSS);
      }

      if (What != ALL)
         if (ss_evid.intersectType(What) == GS_BAD) return GS_BAD; 

      // il gruppo e le loro sottoclassi hanno tutte la stessa abilitazione !
      if (GS_CURRENT_WRK_SESSION->get_level() == GSUpdateableData && 
          p_class->ptr_id()->abilit == GSUpdateableData)
      {
         // Se si evidenzia solo per zoom oppure per lampeggio allora
         // non si sta modificando la grafica
         long GraphMod = GSZoomSetting | GSHighlightSetting;
         GraphMod = ~GraphMod; // ne faccio il not

         if (flag_set & GraphMod)
         {
            // memorizzo in un gruppo di selezione gli oggetti con FAS modificata
            path = GS_CURRENT_WRK_SESSION->get_dir();
            path += _T('\\');
            path += GEOTEMPSESSIONDIR;
            path += _T('\\');
            path += GS_ALTER_FAS_OBJS_FILE;
            GS_ALTER_FAS_OBJS.add_selset(ss_evid, GS_GOOD, path.get_name()); 
         }
      }

      gsc_startTransaction();
      result = GS_BAD;
      do
      {
         long BitForFAS = flag_set;

         // se si vuole evidenziare per riempimento, questo non può essere effettuato
         // da "gsc_modifyEntToFas" che inserendo dei riempimenti associativi legati
         // a DB cambierebbero i fattori di aggregazione (con fattore di scala relativo
         // alla scala precedente)

         // Il riempimento con le sue caratteristiche non deve essere variato da gsc_modifyEntToFas
         if (BitForFAS & GSHatchNameSetting) BitForFAS -= GSHatchNameSetting;
         if (BitForFAS & GSHatchLayerSetting) BitForFAS -= GSHatchLayerSetting;
         if (BitForFAS & GSHatchColorSetting) BitForFAS -= GSHatchColorSetting;
         if (BitForFAS & GSHatchScaleSetting) BitForFAS -= GSHatchScaleSetting;
         if (BitForFAS & GSHatchRotationSetting) BitForFAS -= GSHatchRotationSetting;

         if (gsc_modifyEntToFas(ss_evid, &GraphSettings, BitForFAS, FALSE) == GS_BAD) break;

         // Cambio il tipo di riempimento (solo per polilinee chiuse)
         if (flag_set & GSHatchNameSetting)
         {        
            do
            {
               // Se non esiste il layer per il riempimento lo creo
               gsc_crea_layer(GraphSettings.hatch_layer);

               if (gsc_setHatchSS(ss_evid, GraphSettings.hatch, 
                                  GraphSettings.hatch_scale,
                                  GraphSettings.hatch_rotation,
                                  &GraphSettings.hatch_color,
                                  GraphSettings.hatch_layer) != GS_GOOD) break;
               result = GS_GOOD;
            }
            while (0);
         
            if (result == GS_BAD) break;
            result = GS_BAD;
         }

         if (flag_set & GSZoomSetting)
            // esegue lo zoom sull'area contenente gli oggetti filtrati
            // i blocchi $T vuoti non hanno oggetti visibili
            ss_evid.zoom(MinZoomWindow, MinZoomWindow);

         // evidenzia gli oggetti
         if (flag_set & GSHighlightSetting) ss_evid.redraw(3);

         // devo salvare gli oggetti modificati
         if (flag_set & GSSaveAlteredObjs)
         {
            int previous = GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet();

            GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(GS_GOOD);
            GEOsimAppl::REFUSED_SS.clear();
            //                  selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
            if (gsc_class_align(ss_evid, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), GS_GOOD, FAS_MOD) == -1)
               { GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(previous); break; }
            GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(previous);
         }

         // Il lampeggio termina con ESC da parte dell'utente
         if (flag_set & GSBlinkingSetting) gsc_BlinkObjs(ss_evid);

         result = GS_GOOD;
      }
      while (0);

      if (result == GS_BAD) gsc_abortTransaction();
      else
      {
         acutPrintf(GS_LFSTR);
         acutPrintf(gsc_msg(70), ss_evid.length()); // "%ld oggetti grafici elaborati."

         gsc_endTransaction();
      }
   }
   else result = GS_GOOD;

   return result;
}


/*********************************************************/
/*.doc gsc_ddGridEvid                         <external> */
/*+
  Questa funzione evidenzia gli oggetti di una classe griglia
  in base a criteri scelti tramite interfaccia DCL. La funzione non
  riceve un set di oggetti già trovati perchè potrebbero essere milioni
  (la griglia normalmente è molto grande) e l'occupazione di memoria 
  sarebbe gigantesca.
  Parametri:
  C_CLASS *p_class;      Puntatore alla classe madre degli oggetti da evidenziare
  C_LONG_BTREE &KeyList; Lista dei codici delle celle da evidenziare

  Restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************/
int gsc_ddGridEvid(C_CGRID *p_class, C_LONG_BTREE &KeyList)
{
   Common_Dcl_display_grid_Struct DisplayGridCommonStruct;
   bool                           _3D;

   // Interfaccia grafica per scegliere la modalità di evidenziazione
   // per le griglie
   DisplayGridCommonStruct.pGrid = p_class;
   gsc_load_last_DisplayGrid(&DisplayGridCommonStruct);
   if (gsc_dd_display_grid(DisplayGridCommonStruct, TRUE) != GS_GOOD)
      return GS_BAD;         
   gsc_save_last_DisplayGrid(&DisplayGridCommonStruct);

   _3D = (!DisplayGridCommonStruct.TwoDim && DisplayGridCommonStruct.AttribZ.len() > 0) ? TRUE : FALSE;

   // Se si è scelto di rappresentare la griglia per punti centroidi
   if (DisplayGridCommonStruct.ByPoints)
   {        
      if (p_class->DisplayPts(&KeyList,
                              (_3D) ? &(DisplayGridCommonStruct.AttribZ) : NULL,
                              &DisplayGridCommonStruct.FAS, TRUE,
                              DisplayGridCommonStruct.Mode) == GS_BAD)
         return GS_BAD;
   }
   else
      if (p_class->DisplayCells(&KeyList,
                                (_3D) ? &(DisplayGridCommonStruct.AttribZ) : NULL,
                                &DisplayGridCommonStruct.FAS, 
                                DisplayGridCommonStruct.Mode) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_WhatFas4Evid                                   <internal> */
/*+
  Questa funzione, dato il tipo di una classe o sotto-classe di GEOsim, 
  restituisce un flag contenente la codifica delle caratteristiche 
  grafiche modificabili secondo GraphSettingsEnum.
  Parametri:
  C_CLASS *p_class;  puntatore alla classe o sotto-classe
  long    *flag_set;  flag di modifica (out-put)

  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
-*/  
/*********************************************************************/
long gsc_WhatFas4Evid(C_CLASS *p_class)
{
	long      Flag, Partial;
   C_CLASS   *pCls;
   C_ID      *p_id = p_class->ptr_id();
   C_PROJECT *pPrj = (C_PROJECT *) p_id->pPrj;

   switch (p_id->category)
   {
      case CAT_GROUP:
      {
         C_INT_INT *p_group = NULL;

         // p_class potrebbe non avere la lista dei membri del gruppo
         // se si è in fase di creazione classe
         if (p_class->ptr_group_list())
            p_group = (C_INT_INT *) p_class->ptr_group_list()->get_head();

         // piano blocco stile-testo scala-blocco tipo-linea larghezza colore
         // altezza-testo riempimento thickness elevazione rotazione scala-tipo-linea 
         // piano-riempimento colore-riempimento scala-riempimento 
         // rotazione-riempimento layer-blocchi-DA
         Flag = GSLayerSetting + GSBlockNameSetting  + GSTextStyleSetting +
                GSBlockScaleSetting + GSLineTypeSetting   + GSWidthSetting + 
                GSColorSetting + GSTextHeightSetting + GSHatchNameSetting + 
                GSThicknessSetting + GSElevationSetting + GSRotationSetting +
                GSLineTypeScaleSetting + GSHatchLayerSetting + GSHatchColorSetting +
                GSHatchScaleSetting + GSHatchRotationSetting + GSDABlockLayerSetting;
         while (p_group)
         {  
            // Ritorna il puntatore alla classe cercata
            if ((pCls = pPrj->find_class(p_group->get_key())) == NULL) return -1;

            if ((Partial = gsc_WhatFas4Evid(pCls)) == -1) return -1;
            Flag = Flag & Partial; // solo quelli in comune

            p_group = (C_INT_INT *) p_group->get_next();     
         }
         break;
      }
      case CAT_SIMPLEX: 
      case CAT_SUBCLASS:
         switch (p_id->type)
         {
            case TYPE_POLYLINE:
               // piano tipo-linea larghezza colore thickness elevazione scala-tipo-linea
               // layer-blocchi-DA
               Flag = GSLayerSetting + GSLineTypeSetting + GSWidthSetting + 
                      GSColorSetting + GSThicknessSetting + GSElevationSetting +
                      GSLineTypeScaleSetting + GSDABlockLayerSetting;
               break;
            case TYPE_TEXT:
               // piano stile-testo colore altezza-testo thickness elevazione rotazione
               Flag = GSLayerSetting + GSTextStyleSetting + GSColorSetting + 
                      GSTextHeightSetting + GSThicknessSetting + GSElevationSetting + 
                      GSRotationSetting;
               break;
            case TYPE_NODE:
               // piano blocco scala-blocco colore elevazione rotazione layer-blocchi-DA
               Flag = GSLayerSetting + GSBlockNameSetting + GSBlockScaleSetting + 
                      GSColorSetting + GSElevationSetting + GSRotationSetting +
                      GSDABlockLayerSetting;
               break;
            case TYPE_SURFACE:
               // piano tipo-linea larghezza colore riempimento thickness elevazione 
               // scala-tipo-linea piano-riempimento colore-riempimento scala-riempimento
               // rotazione-riempimento layer-blocchi-DA
               Flag = GSLayerSetting + GSLineTypeSetting + GSWidthSetting +
                      GSColorSetting + GSHatchNameSetting + GSThicknessSetting +
                      GSElevationSetting + GSLineTypeScaleSetting +
                      GSHatchLayerSetting + GSHatchColorSetting + GSHatchScaleSetting +
                      GSHatchRotationSetting + GSDABlockLayerSetting;
               break;
            default:
               Flag = 0;
               break;
         }
         break;
      case CAT_GRID:
         // piano tipo-linea larghezza colore riempimento thickness elevazione 
         // scala-tipo-linea piano-riempimento colore-riempimento scala-riempimento
         // rotazione-riempimento
         Flag = GSLayerSetting + GSLineTypeSetting + GSWidthSetting +
                GSColorSetting + GSHatchNameSetting + GSThicknessSetting +
                GSElevationSetting + GSLineTypeScaleSetting +
                GSHatchLayerSetting + GSHatchColorSetting + GSHatchScaleSetting +
                GSHatchRotationSetting;
         break;
      case CAT_SPAGHETTI:
         // piano blocco stile-testo scala-blocco tipo-linea larghezza colore
         // altezza-testo riempimento thickness elevazione rotazione
         // scala-tipo-linea piano-riempimento colore-riempimento scala-riempimento
         // rotazione-riempimento stile-quotatura
         Flag = GSLayerSetting + GSBlockNameSetting + GSTextStyleSetting + 
                GSBlockScaleSetting + GSLineTypeSetting + GSWidthSetting + 
                GSColorSetting + GSTextHeightSetting + GSHatchNameSetting +
                GSThicknessSetting + GSElevationSetting + GSRotationSetting +
                GSLineTypeScaleSetting +
                GSHatchLayerSetting + GSHatchColorSetting + GSHatchScaleSetting +
                GSHatchRotationSetting + GSDimensionStyleSetting;
         break;
      default:
         Flag = 0;
         break;
   }
   
   return Flag;
}


/*********************************************************/
/*.doc gsc_ddgraphBrowse                      <external> */
/*+
  Questa funzione sfoglia il gruppo di selezione effettuando degli zoom
  sugli oggetti e mostrando la relativa scheda alfanumerica tramite interfaccia DCL.
  Parametri:
  C_SELSET &selset;   gruppo di oggetti da evidenziare
  int      DBOpCode;  Indica se si vuole effettuare operazioni di 
                      interrogazione, cancellazione (e quindi anche di modifica)
                      o di sola modifica sulle schede degli oggetti da sfogliare 
                      (MODIFY, ERASE o QUERY; default = QUERY)

  Restituisce RTERROR in caso di errore altrimenti RTNORM.
  N.B. Se <DBOpCode> = ERASE il gruppo di selezione <selset> può essere modificato
-*/  
/*********************************************************/
int gsc_ddgraphBrowse(C_SELSET &selset, int DBOpCode)
{
   C_STRING  msg, label;
   TCHAR     answer[MAX_COMMAND_LEN];
   int       res, Direction = 1; // 1 = successivo, 2 = precedente
   long      SelSetLen, i = 0;
   ads_name  ent, dummySS;
   C_SELSET  ss;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((SelSetLen = selset.length()) == 0) return GS_GOOD;

   selset.entname(0, ent); // prima entità
   ss.add(ent);
   // i blocchi $T vuoti non hanno oggetti visibili
   ss.zoom(GEOsimAppl::GLOBALVARS.get_AutoZoomMinXDim(),
           GEOsimAppl::GLOBALVARS.get_AutoZoomMinYDim());

   ss.get_selection(dummySS);
   if (acedSSSetFirst(dummySS, dummySS) != RTNORM) return GS_BAD;

   do
   {
      msg = gsc_msg(59);     // "Successivo"
      msg += _T(' ');
      msg += gsc_msg(60);    // "Precedente"
      msg += _T(' ');
      if (DBOpCode == ERASE)
      {
         msg += gsc_msg(61); // "cAncella"
         msg += _T(' ');
      }
      msg += gsc_msg(255);   // "Esci"
      acedInitGet(0, msg.get_name());

      msg = gsc_msg(93);     // "\nDigitare un'opzione "
      msg += _T('[');
      if (Direction == 1) // successivo 
      {
         msg += _T('<');       
         msg += gsc_msg(59);    // "Successivo"
         msg += _T(">/");
         msg += gsc_msg(60);    // "Precedente"
      }
      else // precedente
      {
         msg += gsc_msg(59);    // "Successivo"
         msg += _T("/<");
         msg += gsc_msg(60);    // "Precedente"
         msg += _T('>');
      }

      msg += _T('/');
      if (DBOpCode == ERASE)
      {
         msg += gsc_msg(61); // "cAncella"
         msg += _T('/');
      }
      msg += gsc_msg(255);   // "Esci"
      msg += _T("] (");
      msg += i + 1;
      msg += _T('/');
      msg += SelSetLen;
      msg += _T("): ");
      res = acedGetKword(msg.get_name(), answer);
      
      if (res == RTERROR) return RTERROR;   // Input Errato
      else if (res == RTCAN) return GS_CAN;      // Input ^C
      else if ((res == RTNONE && Direction == 1) || gsc_strcmp(answer, gsc_msg(59)) == 0) // Successivo
      {
         if (i + 1 < SelSetLen && selset.entname(i + 1, ent) == GS_GOOD)
         {  // esegue lo zoom sull'area contenente gli oggetti filtrati
            i++;
            ss.clear();
            ss.add(ent);
            // i blocchi $T vuoti non hanno oggetti visibili
            ss.zoom(GEOsimAppl::GLOBALVARS.get_AutoZoomMinXDim(),
                    GEOsimAppl::GLOBALVARS.get_AutoZoomMinYDim());

            ss.get_selection(dummySS);
            if (acedSSSetFirst(dummySS, dummySS) != RTNORM) break;

            Direction = 1; // successivo 
         }
         else
            Direction = 2; // precedente
      }
      else
      if ((res == RTNONE && Direction == 2) || gsc_strcmp(answer, gsc_msg(60)) == 0) // Precedente
      {
         if (i - 1 >= 0 && selset.entname(i - 1, ent) == GS_GOOD)
         {  // esegue lo zoom sull'area contenente gli oggetti filtrati
            i--;
            ss.clear();
            ss.add(ent);
            // i blocchi $T vuoti non hanno oggetti visibili
            ss.zoom(GEOsimAppl::GLOBALVARS.get_AutoZoomMinXDim(),
                    GEOsimAppl::GLOBALVARS.get_AutoZoomMinYDim());

            ss.get_selection(dummySS);
            if (acedSSSetFirst(dummySS, dummySS) != RTNORM) break;

            Direction = 2; // precedente
         }
         else
            Direction = 1; // successivo 
      }
      else
      if (gsc_strcmp(answer, gsc_msg(61)) == 0) // cAncella
      {
         C_SELSET PickSet;

         ss.get_selection(dummySS);
         if (acedSSSetFirst(dummySS, dummySS) != RTNORM) return GS_BAD;

         gserase();
         if (ss.entname(0, ent) == GS_GOOD && gsc_IsErasedEnt(ent) == GS_GOOD)
         {
            selset.subtract(ss);
            SelSetLen -= ss.length();
            i--;
         }
      }
      else
         break;
   }
   while (1);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_BlinkObjs                          <external> */
/*+
  Questa funzione fa lampeggiare un gruppo di selezione, il lampeggio viene 
  interrotto con CTRL-C.
  Parametri:
  C_SELSET &selset;   gruppo di oggetti da evidenziare
-*/  
/*********************************************************/
void gsc_BlinkObjs(C_SELSET &ss)
{
   struct _timeb t1, t2;
   double        tempo;
   int           Mode, WaitTime, MIN_WAIT_TIME = 500; // (1/2 secondo)

   acutPrintf(gsc_msg(299)); // "\nPremere ESC per qualche secondo per interrompere...\n"

   // Ridisegno la finestra perchè la DCL precedente può rimanere a video
   acedGetAcadFrame()->RedrawWindow();

   Mode = 3;
   while (!acedUsrBrk())
   {
      _ftime(&t1);
      ss.redraw(Mode);
      _ftime(&t2);

      // calcolo il tempo necessario per evidenziare
      tempo = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
      tempo *= 1000; // millisecondi
      
      // Attesa
      if (tempo < MIN_WAIT_TIME)
         if ((WaitTime = (int) (MIN_WAIT_TIME - tempo)) > 0) Sleep(WaitTime);

      Mode = (Mode == 3) ? 4 : 3;
   }

   if (Mode == 4) ss.redraw(4);
}


/*********************************************************/
/*.doc gsc_SaveEvidOptions e gsc_LoadEvidOptions <internal> */
/*+                                                                       
  Funzioni che salvano e caricano il valore del flag "tool" per il filtro
  e per i sensori in un file.
  Parametri:
  int ObjType;             Tipologia di oggetti da evidenziare
  long flag_set;           Flag a bit che identifica quali azioni si vogliono 
                           intraprendere (vedi GraphSettingsEnum)
  C_FAS   &GraphSettings;  Valori per le modifiche da intraprendere (settati da flag_set)
  C_STRING *pPath;         Opzionale, Puntatore alla path del file. Se non
                           specificato viene usato GS_INI_FILE nella cartella
                           della sessione di lavoro corrente.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_SaveEvidOptions(int ObjType, long flag_set, C_FAS &GraphSettings, C_STRING *pPath)
{
   C_STRING Path, EntryValue, EntryName, SezName;

   if (pPath) Path = pPath->get_name();
   else
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   if (gsc_getClsTypeDescr(ObjType, EntryValue) == GS_BAD) EntryValue = _T("Mixed");
   SezName = _T("LAST_FILTER_");
   SezName += EntryValue;
   SezName += _T("_EVID_OPTIONS");

   // Scrivo quali sono le caratteristiche grafiche usate per evidenziare
   EntryValue = flag_set;
   if (gsc_set_profile(Path, SezName.get_name(), _T("FLAG"), EntryValue.get_name()) == GS_BAD)
      return GS_BAD;

   // Scrivo i valori delle caratteristiche grafiche usate per evidenziare
   return GraphSettings.ToFile(Path, SezName.get_name());
}
int gsc_LoadEvidOptions(int ObjType, long *flag_set, C_FAS &GraphSettings, C_STRING *pPath)
{
   C_STRING Path, EntryValue, SezName;

   if (pPath) Path = pPath->get_name();
   else
   {
      if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

      Path = GS_CURRENT_WRK_SESSION->get_dir();
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   if (gsc_getClsTypeDescr(ObjType, EntryValue) == GS_BAD) EntryValue = _T("Mixed");
   SezName = _T("LAST_FILTER_");
   SezName += EntryValue;
   SezName += _T("_EVID_OPTIONS");

   // Leggo quali sono le caratteristiche grafiche usate per evidenziare
   if (gsc_get_profile(Path, SezName.get_name(), _T("FLAG"), EntryValue) == GS_BAD)
      return GS_BAD;
   *flag_set = EntryValue.tol();

   // Leggo i valori delle caratteristiche grafiche usate per evidenziare
   return GraphSettings.load(Path, SezName.get_name());
}


///////////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI DELL'IMPOSTAZIONE VISUALIZZAZIONE GRIGLIA
///////////////////////////////////////////////////////////////////////////////


/*****************************************************************************/
/*.doc gsc_load_last_DisplayGrid                                  <internal> */
/*+
  Questa funzione carica l'ultima configurazione di visualizzazione di una
  griglia se esiste, altrimenti setta una configurazione di default.
  Prima di chiamare questa funzione deve essere settato il membro "pGrid" della
  struttura "Common_Dcl_display_grid_Struct" per sapere a quale classe ci si deve
  riferire.
  Parametri:
  Common_Dcl_display_grid_Struct *pStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_load_last_DisplayGrid(Common_Dcl_display_grid_Struct *pStruct)
{
   C_STRING Path, Prefix, EntryName, EntryValue;
   int      Result = GS_BAD;

   if (!pStruct->pGrid)
   {
      pStruct->ByPoints = FALSE;
      pStruct->Mode     = PREVIEW;
      pStruct->TwoDim   = TRUE;
      pStruct->AttribZ.clear();
      
      return GS_GOOD;
   }

   Path = GS_CURRENT_WRK_SESSION->get_dir();
   Path += _T('\\');
   Path += GS_INI_FILE;

   Prefix = _T("PRJ");
   Prefix += GS_CURRENT_WRK_SESSION->get_PrjId();
   Prefix += _T("CLS");
   Prefix += pStruct->pGrid->ptr_id()->code;
   Prefix += _T("SUB");
   Prefix += pStruct->pGrid->ptr_id()->sub_code;
   
   do
   {
      EntryName = Prefix;
      EntryName += _T("ByPoints");
      if (gsc_get_profile(Path, _T("LAST_DISPLAY_GRID_CLASSES"), EntryName.get_name(), EntryValue) != GS_GOOD)
         break;
      pStruct->ByPoints = (EntryValue.comp(_T("1")) == 0) ? TRUE : FALSE;

      EntryName = Prefix;
      EntryName += _T("Mode");
      if (gsc_get_profile(Path, _T("LAST_DISPLAY_GRID_CLASSES"), EntryName.get_name(), EntryValue) != GS_GOOD)
         break;
      pStruct->Mode = EntryValue.toi();

      EntryName = Prefix;
      EntryName += _T("TwoDim");
      if (gsc_get_profile(Path, _T("LAST_DISPLAY_GRID_CLASSES"), EntryName.get_name(), EntryValue) != GS_GOOD)
         break;
      pStruct->TwoDim = (EntryValue.comp(_T("1")) == 0) ? TRUE : FALSE;

      if (!pStruct->TwoDim)
      {
         EntryName = Prefix;
         EntryName += _T("AttribZ");
         if (gsc_get_profile(Path, _T("LAST_DISPLAY_GRID_CLASSES"), EntryName.get_name(), EntryValue) != GS_GOOD)
            break;
         pStruct->AttribZ = EntryValue;
      }

      Result = GS_GOOD;
   }
   while (0);

   if (Result == GS_BAD)
   {
      pStruct->ByPoints = FALSE;
      pStruct->Mode     = PREVIEW;
      pStruct->TwoDim   = TRUE;
      pStruct->AttribZ.clear();
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_save_last_DisplayGrid                                  <internal> */
/*+
  Questa funzione salva la configurazione di visualizzazione di una
  griglia.
  Prima di chiamare questa funzione deve essere settato il membro "pGrid" della
  struttura "Common_Dcl_display_grid_Struct" per sapere a quale classe ci si deve
  riferire.
  Parametri:
  Common_Dcl_display_grid_Struct *pStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_save_last_DisplayGrid(Common_Dcl_display_grid_Struct *pStruct)
{
   C_STRING Path, Prefix, EntryName, EntryValue;
   int      Result = GS_BAD;

   Path = GS_CURRENT_WRK_SESSION->get_dir();
   Path += _T('\\');
   Path += GS_INI_FILE;

   Prefix = _T("PRJ");
   Prefix += GS_CURRENT_WRK_SESSION->get_PrjId();
   Prefix += _T("CLS");
   Prefix += pStruct->pGrid->ptr_id()->code;
   Prefix += _T("SUB");
   Prefix += pStruct->pGrid->ptr_id()->sub_code;
   
   do
   {
      EntryName = Prefix;
      EntryName += _T("ByPoints");
      EntryValue = (pStruct->ByPoints) ? _T("1") : _T("0");
      if (gsc_set_profile(Path, _T("LAST_DISPLAY_GRID_CLASSES"), EntryName.get_name(),
                          EntryValue.get_name()) != GS_GOOD)
         break;

      EntryName = Prefix;
      EntryName += _T("Mode");
      EntryValue = pStruct->Mode;
      if (gsc_set_profile(Path, _T("LAST_DISPLAY_GRID_CLASSES"), EntryName.get_name(),
                          EntryValue.get_name()) != GS_GOOD)
         break;

      EntryName = Prefix;
      EntryName += _T("TwoDim");
      EntryValue = (pStruct->TwoDim) ? _T("1") : _T("0");
      if (gsc_set_profile(Path, _T("LAST_DISPLAY_GRID_CLASSES"), EntryName.get_name(),
                          EntryValue.get_name()) != GS_GOOD)
         break;

      if (!pStruct->TwoDim)
      {
         EntryName = Prefix;
         EntryName += _T("AttribZ");
         if (gsc_set_profile(Path, _T("LAST_DISPLAY_GRID_CLASSES"), EntryName.get_name(),
                             pStruct->AttribZ.get_name()) != GS_GOOD)
            break;
      }

      Result = GS_GOOD;
   }
   while (0);

   return GS_GOOD;
}

/*****************************************************************************/
/*.doc gsc_setTileDcldisplay_grid                                            */
/*+
  Questa funzione setta tutti i controlli della DCL "display_grid" in modo 
  appropriato secondo il tipo di selezione.
  Parametri:
  ads_hdlg dcl_id;     
  Common_Dcl_display_grid_Struct *pCommonStruct;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
static void gsc_setTileDcldisplay_grid(ads_hdlg dcl_id, 
                                      Common_Dcl_display_grid_Struct *pCommonStruct)
{
   int      i = 0;
   C_ATTRIB *pAttrib;
   C_STRING KeyAttrib = pCommonStruct->pGrid->ptr_info()->key_attrib, iStr;

   ads_set_tile(dcl_id, _T("ObjectType"), (pCommonStruct->ByPoints) ? _T("Point") : _T("Cell"));
   if (pCommonStruct->TwoDim)
   {
      ads_set_tile(dcl_id, _T("2D"), _T("1"));
      ads_set_tile(dcl_id, _T("3D"), _T("0"));
   }
   else
   {
      ads_set_tile(dcl_id, _T("2D"), _T("0"));
      ads_set_tile(dcl_id, _T("3D"), _T("1"));
   }

   ads_set_tile(dcl_id, _T("Preview"), (pCommonStruct->Mode == PREVIEW) ? _T("1") : _T("0"));

   // Riempie la lista del controllo indicato con i nomi dei 
   // campi numerici della classe escluso il campo chiave
   ads_start_list(dcl_id, _T("AttribZ"), LIST_NEW, 0);
   pAttrib = (C_ATTRIB *) pCommonStruct->pGrid->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      if (KeyAttrib.comp(pAttrib->get_name(), FALSE) != 0)
      {
         pAttrib->init_ADOType(pCommonStruct->pGrid->ptr_info()->getDBConnection(OLD));

         if (gsc_DBIsNumeric(pAttrib->ADOType) == GS_GOOD)
         {
            gsc_add_list(pAttrib->get_name());
            if (pAttrib->name.comp(pCommonStruct->AttribZ) == 0) iStr = i;
            i++;
         }
      }

      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }
   ads_end_list();

   if (i == 0) // Non c'era neanche un attributo numerico
   {
      ads_mode_tile(dcl_id, _T("3D"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("AttribZ"), MODE_DISABLE); 
   }
   else
   {
      ads_mode_tile(dcl_id, _T("3D"), MODE_ENABLE); 
      ads_set_tile(dcl_id, _T("AttribZ"), (iStr.get_name()) ? iStr.get_name() : GS_EMPTYSTR);
      if (pCommonStruct->TwoDim) ads_mode_tile(dcl_id, _T("AttribZ"), MODE_DISABLE); 
      else ads_mode_tile(dcl_id, _T("AttribZ"), MODE_ENABLE); 
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "display_grid" su controllo "ObjectType"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_display_grid_ObjectType(ads_callback_packet *dcl)
{
   TCHAR val[TILE_STR_LIMIT];

   ads_get_tile(dcl->dialog, _T("ObjectType"), val, TILE_STR_LIMIT);
   if (gsc_strcmp(val, _T("Point")) == 0)
      ((Common_Dcl_display_grid_Struct*)(dcl->client_data))->ByPoints = TRUE;
   else
      ((Common_Dcl_display_grid_Struct*)(dcl->client_data))->ByPoints = FALSE;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "display_grid" su controllo "2D"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_display_grid_2D(ads_callback_packet *dcl)
{
   ((Common_Dcl_display_grid_Struct*)(dcl->client_data))->TwoDim = TRUE;
   ads_set_tile(dcl->dialog, _T("3D"), _T("0"));
   ads_mode_tile(dcl->dialog, _T("AttribZ"), MODE_DISABLE); 
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "display_grid" su controllo "AttribZ"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_display_grid_AttribZ(ads_callback_packet *dcl)
{
   TCHAR    val[TILE_STR_LIMIT];
   int      i = 0;
   C_ATTRIB *pAttrib;
   Common_Dcl_display_grid_Struct *pCommonStruct = (Common_Dcl_display_grid_Struct*) dcl->client_data;
   C_STRING KeyAttrib = pCommonStruct->pGrid->ptr_info()->key_attrib;

   ads_get_tile(dcl->dialog, _T("AttribZ"), val, TILE_STR_LIMIT);
   if (wcslen(val) == 0) return;

   pAttrib = (C_ATTRIB *) pCommonStruct->pGrid->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      if (KeyAttrib.comp(pAttrib->get_name(), FALSE) != 0)
      {
         pAttrib->init_ADOType(pCommonStruct->pGrid->ptr_info()->getDBConnection(OLD));

         if (gsc_DBIsNumeric(pAttrib->ADOType) == GS_GOOD)
         {
            if (i == _wtoi(val))
            { 
               pCommonStruct->AttribZ = pAttrib->get_name();
               break;
            }
            i++;
         }
      }

      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "display_grid" su controllo "3D"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_display_grid_3D(ads_callback_packet *dcl)
{
   ((Common_Dcl_display_grid_Struct*)(dcl->client_data))->TwoDim = FALSE;
   ads_set_tile(dcl->dialog, _T("2D"), _T("0"));
   ads_mode_tile(dcl->dialog, _T("AttribZ"), MODE_ENABLE); 
   dcl_display_grid_AttribZ(dcl);
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "display_grid" su controllo "Preview"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_display_grid_Preview(ads_callback_packet *dcl)
{
   TCHAR val[TILE_STR_LIMIT];

   ads_get_tile(dcl->dialog, _T("Preview"), val, TILE_STR_LIMIT);
   if (gsc_strcmp(val, _T("1")) == 0)
      ((Common_Dcl_display_grid_Struct*)(dcl->client_data))->Mode = PREVIEW;
   else
      ((Common_Dcl_display_grid_Struct*)(dcl->client_data))->Mode = EXTRACTION;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "display_grid" su controllo "FAS"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_display_grid_FAS(ads_callback_packet *dcl)
{
   Common_Dcl_display_grid_Struct *pCommonStruct = (Common_Dcl_display_grid_Struct*) dcl->client_data;
   long   flag_set;
   int    what;
   double dummy;

   if (gsc_ddChooseGraphSettings(pCommonStruct->pGrid, FALSE, &flag_set,
                                 pCommonStruct->FAS, &dummy, &what, FALSE) != GS_GOOD)
      return;
}
///////////////////////////////////////////////////////////////////////////////
// ACTION TILE per la DCL "display_grid" su controllo "help"
///////////////////////////////////////////////////////////////////////////////
static void CALLB dcl_display_grid_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Metodidirestituzionedeirisultatidiunfiltro); }


/*****************************************************************************/
/*.doc gsc_dd_display_grid                                                   */
/*+
  Questa funione permette di selezionare le caratteristiche di
  visualizzazione di una griglia.
  Parametri:
  Common_Dcl_display_grid_Struct &CommonStruct;  Struttura contenente le informazioni
                                                 per rappresentare una griglia
  bool FASselection;                             Se la finestra deve presentare la
                                                 scelta delle caratteristiche grafiche
                                                 (default = TRUE)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_dd_display_grid(Common_Dcl_display_grid_Struct &CommonStruct,
                        bool FASselection)
{
   C_STRING path;
   ads_hdlg dcl_id;
   int      status, dcl_file;
   Common_Dcl_display_grid_Struct _CommonStruct;

   _CommonStruct.pGrid    = CommonStruct.pGrid;
   _CommonStruct.TwoDim   = CommonStruct.TwoDim;
   _CommonStruct.AttribZ  = CommonStruct.AttribZ;
   _CommonStruct.ByPoints = CommonStruct.ByPoints;
   _CommonStruct.Mode     = CommonStruct.Mode;
   CommonStruct.FAS.copy(&_CommonStruct.FAS);

   // CARICA IL FILE DCL E INIZIALIZZA LA DIALOG-BOX
   path = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_FILTR.DCL");
   if (gsc_load_dialog(path, &dcl_file) == RTERROR) return GS_BAD;
   ads_new_dialog((FASselection) ? _T("GridDisplay") : _T("GridDisplayNoFAS"), 
                  dcl_file, (CLIENTFUNC) NULLCB, &dcl_id);
   if (!dcl_id) { GS_ERR_COD = eGSAbortDCL; ads_unload_dialog(dcl_file); }

   gsc_setTileDcldisplay_grid(dcl_id, &_CommonStruct);

   ads_action_tile(dcl_id, _T("ObjectType"), (CLIENTFUNC) dcl_display_grid_ObjectType);
   ads_client_data_tile(dcl_id, _T("ObjectType"), &_CommonStruct);
   ads_action_tile(dcl_id, _T("2D"), (CLIENTFUNC) dcl_display_grid_2D);
   ads_client_data_tile(dcl_id, _T("2D"), &_CommonStruct);
   ads_action_tile(dcl_id, _T("3D"), (CLIENTFUNC) dcl_display_grid_3D);
   ads_client_data_tile(dcl_id, _T("3D"), &_CommonStruct);
   ads_action_tile(dcl_id, _T("AttribZ"), (CLIENTFUNC) dcl_display_grid_AttribZ);
   ads_client_data_tile(dcl_id, _T("AttribZ"), &_CommonStruct);
   ads_action_tile(dcl_id, _T("Preview"), (CLIENTFUNC) dcl_display_grid_Preview);
   ads_client_data_tile(dcl_id, _T("Preview"), &_CommonStruct);
   ads_action_tile(dcl_id, _T("help"),   (CLIENTFUNC) dcl_display_grid_help);
   if (FASselection)
   {
      ads_action_tile(dcl_id, _T("FAS"), (CLIENTFUNC) dcl_display_grid_FAS);
      ads_client_data_tile(dcl_id, _T("FAS"), &_CommonStruct);
   }

   // LANCIA LA DIALOG-BOX
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(dcl_file);

   if (status == DLGCANCEL) return GS_CAN;

   CommonStruct.TwoDim   = _CommonStruct.TwoDim;
   CommonStruct.AttribZ  = _CommonStruct.AttribZ;
   CommonStruct.ByPoints = _CommonStruct.ByPoints;
   CommonStruct.Mode     = _CommonStruct.Mode;
   _CommonStruct.FAS.copy(&CommonStruct.FAS);

   return GS_GOOD;
}
