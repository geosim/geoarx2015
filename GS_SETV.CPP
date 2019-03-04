/**********************************************************
Name: GS_SETV

Module description: File contenente le funzioni per la gestione 
                    dei set di visibilità di GEOsim 
            
Author: Paolo De Sole, Poltini Roberto
                                                       
(c) Copyright 2000-2012 by IREN ACQUA GAS S.p.A.

Modification history:
              
Notes and restrictions on use: 


**********************************************************/


/**********************************************************/
/*   INCLUDE                                              */
/**********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <fcntl.h>       /*  per _open()  */
#include <sys\stat.h>    /*  per _open()  */
#include "adslib.h"
#include "adsdlg.h"   

#include "..\gs_def.h" // definizioni globali
#include "gs_utily.h"     // (gsc_strcat, gsc_tostring)
#include "gs_resbf.h"     // (gsc_nth)
#include "gs_error.h"     // codici errori
#include "gs_list.h"
#include "gs_ase.h"       // gestione database
#include "gs_init.h"      // direttori globali
#include "gs_opcod.h"
#include "gs_user.h"   
#include "gs_dbref.h"   
#include "gs_cmd.h"       
#include "gs_class.h"     
#include "gs_prjct.h"     
#include "gs_area.h"     
#include "gs_graph.h"     
#include "gs_query.h"     
#include "gs_attbl.h"     
#include "gs_lisp.h"     
#include "gs_setv.h"     
#include "gs_dwg.h"       // gestione disegni

#include "d2hMap.h" // doc to help


/***********************************************************/
/*  DEFINE                                                 */
/***********************************************************/
#define SET_NEW    0
#define SET_UPD    1       


/*************************************************************************/
/* STRUTTURE PER IL PASSAGGIO VALORI FRA DCL                             */                                    
/*************************************************************************/
struct Common_Dcl_Activate_Param
{
   int active_on_memory;
   int add_object2saveset;
};
struct Common_Dcl_Visib_Set
{
   C_PROJECT          *pCurrPrj;                // progetto
   C_INT_INT_STR_LIST ClassList;                // lista di classi del progetto (filtrate)
   C_SET_VISIB        *pCurrent_set_visibility;
   C_STR_LIST         current_list_setv;        // Lista dei nomi dei set di visibilità
   C_STR_LIST         current_list_attrib;
   C_SET_VISIB_LIST   current_set_visibility_list;
};


/***********************************************************/
/*  DEFINITION FUNCTION                                    */
/***********************************************************/
int load_listSetOfProject(Common_Dcl_Visib_Set *CommonStruct);
static void CALLB manag_clicked_visAttrib(ads_callback_packet *dcl);


// N.B. !!!!!!
// Da fare un funzione per l' inserimento dell' ON-OFF nell' entità di GEOsim

/***********************************************************/
/*                 ACTION TILE                             */
/*+
   L' utente ha cliccato il tasto CHIUDI
-*/  
/***********************************************************/
static void CALLB chiudiTutto(ads_callback_packet *dcl)
{
   ads_hdlg hdlg = dcl->dialog;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);
   ads_done_dialog(hdlg, DLGOK); 
   
   // Azzero le variabili globali.
   pCommonStruct->pCurrPrj = NULL;
   pCommonStruct->ClassList.remove_all();
   pCommonStruct->pCurrent_set_visibility = NULL;
   pCommonStruct->current_list_setv.remove_all();
   
   return;
}
// ACTION TILE : click su tasto Help
static void CALLB clicked_set_help(ads_callback_packet *dcl)
   { gsc_help(IDH_Gestionesetdivisibilit); } 


/***************************************************************/
/*.doc int loadListClassOfProject()                            */
/*+
   Funzione che carica la lista del progetto escludendo
   le classi TEXT, GROUP, GROUP, SPAGHETTI.
   Parametri:
   C_PROJECT *pPrj;                 Puntatore a progetto
   C_INT_INT_STR_LIST &ClassList;   Lista classi (out)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int loadListClassOfProject(C_PROJECT *pPrj, C_INT_INT_STR_LIST &ClassList)
{
   C_SINTH_CLASS_LIST GlobalClassList;
   C_SINTH_CLASS      *pGlobalClass = NULL, *psubcls = NULL;
   C_CLASS            *pCls = NULL;
   C_ATTRIB_LIST      *p_attrib_list = NULL; 
   C_STRING           stringa, prefisso;
   C_INT_INT_STR      *pListcls = NULL;

   ClassList.remove_all();

   // Caricamento delle classi di entità per gestione Dcl
   if (pPrj->getSinthClassList(GlobalClassList, ONETEST) == GS_BAD) return GS_BAD;
   // Ordino per nome classe con sensitive = FALSE
   GlobalClassList.sort_name();
   
   // Carico la lista dei progetti
   pGlobalClass = (C_SINTH_CLASS *) GlobalClassList.get_head();
   while (pGlobalClass)
   {
      // Elimino subito le classi TEXT, GROUP, GROUP, SPAGHETTI
      if (pGlobalClass->get_category() == CAT_GROUP ||
          pGlobalClass->get_category() == CAT_SPAGHETTI ||
          (pGlobalClass->get_category() == CAT_SIMPLEX && pGlobalClass->get_type() == TYPE_TEXT))
      { 
         pGlobalClass = (C_SINTH_CLASS *) GlobalClassList.get_next(); 
         continue; 
      }
      
      // Verifico se simulazione
      if (pGlobalClass->get_category() == CAT_EXTERN)
      {
         if (pGlobalClass->ptr_sub_list() &&
             (psubcls = (C_SINTH_CLASS *) (pGlobalClass->ptr_sub_list())->get_head()) != NULL)
         {
            // In caso di simulazione devo comporre il nome princ-sub
            prefisso = pGlobalClass->get_name();
            prefisso += _T(" - ");

            while (psubcls)
            {
               stringa = prefisso;
               stringa += psubcls->get_name();

               if ((pListcls = new C_INT_INT_STR) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
               pListcls->set_key(pGlobalClass->get_key());
               pListcls->set_type(psubcls->get_key());
               pListcls->set_name(stringa.get_name());
               ClassList.add_tail(pListcls);         
               psubcls = (C_SINTH_CLASS *) pGlobalClass->ptr_sub_list()->get_next();
            }
         }
      }
      else
      {
         if ((pListcls = new C_INT_INT_STR) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pListcls->set_key(pGlobalClass->get_key());
         pListcls->set_type(0);
         pListcls->set_name(pGlobalClass->get_name());
         ClassList.add_tail(pListcls);         
      }
      pGlobalClass = (C_SINTH_CLASS *) GlobalClassList.get_next();
   }
   
   return GS_GOOD;
}


/***************************************************************/
/*.doc int loadSet4Dcl()                                       */
/*+
   Funzione che carica i nomi dei set di visibilità nella lista
   della DCL principale.
   Parametri:
   ads_hdlg dcl: handle della dcl aperta;
   Common_Dcl_Visib_Set *CommonStruct: struttura delle variabili 
                                       globali di passaggio.

  Restituisce GS_GOOD in caso di successo altrimenti  GS_BAD.
-*/  
/***************************************************************/
int loadListSet4Dcl(ads_hdlg dcl, Common_Dcl_Visib_Set *pCommonStruct)
{
   C_STR *pSet;

   // Carico la lista dei progetti
   ads_start_list(dcl, _T("set_visib_list"), LIST_NEW, 0);
   pSet = (C_STR *) pCommonStruct->current_list_setv.get_head();
   while (pSet)
   {
      gsc_add_list(pSet->get_name());
      pSet = (C_STR *) pCommonStruct->current_list_setv.get_next();
   }                             
   ads_end_list(); 
   if (pCommonStruct->current_list_setv.get_count() == 0)
   {
      ads_mode_tile(dcl, _T("set_visib_new"), MODE_ENABLE); 
      ads_mode_tile(dcl, _T("set_visib_mod"), MODE_DISABLE); 
      ads_mode_tile(dcl, _T("set_visib_del"), MODE_DISABLE); 
      ads_mode_tile(dcl, _T("set_visib_list"), MODE_DISABLE); 
      ads_mode_tile(dcl, _T("set_visib_act"), MODE_DISABLE); 
   }
   else
   {  
      ads_mode_tile(dcl, _T("set_visib_new"), MODE_ENABLE); 
      ads_mode_tile(dcl, _T("set_visib_mod"), MODE_ENABLE); 
      ads_mode_tile(dcl, _T("set_visib_del"), MODE_ENABLE); 
      ads_mode_tile(dcl, _T("set_visib_list"), MODE_ENABLE); 

      // Se sono in un' area del progetto scelto attivo il tasto attiva
      if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION->get_pPrj() != pCommonStruct->pCurrPrj)
         ads_mode_tile(dcl, _T("set_visib_act"), MODE_DISABLE); 
      else
         ads_mode_tile(dcl, _T("set_visib_act"), MODE_ENABLE); 
   }
   
   return GS_GOOD;
}


/*************************************************************/
/*   ACTION TILE                                             */
/*+
   L'utente ha cliccato sulla edit dell'inserimento nome prj.
-*/  
/*************************************************************/
static void CALLB clicked_prjedit(ads_callback_packet *dcl)
{
   TCHAR    buff[10];
   ads_hdlg hdlg = dcl->dialog;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   ads_get_tile(hdlg, _T("prj_name"), buff, 10); 
   if ((pCommonStruct->pCurrPrj = gsc_GetPopDwnPrjList(_wtoi(buff))) == NULL)
   {
      ads_set_tile(hdlg, _T("error"), gsc_msg(665));      // "Progetto inesistente..."  
      ads_mode_tile(hdlg, _T("currentprj"), MODE_SETFOCUS); 
   }
   else
   {
      // Carico la lista dei set del progetto
      if (load_listSetOfProject(pCommonStruct) == GS_BAD)
         ads_set_tile(hdlg, _T("error"), gsc_msg(663));      // "Errore nel caricamento set..."
      // Carico la lista nella dcl
      if (loadListSet4Dcl(hdlg, pCommonStruct) == GS_BAD)
         ads_set_tile(hdlg, _T("error"), gsc_msg(663));      // "Errore nel caricamento set..."
      // Dal progetto carico nella lista globale le classi dei progetto
      if (loadListClassOfProject(pCommonStruct->pCurrPrj, pCommonStruct->ClassList) == GS_BAD)
         ads_set_tile(hdlg, _T("error"), gsc_msg(663));      // "Errore nel caricamento set..."
   }

   return;
}


/***********************************************************/
/*       ACTION TILE                                       */
/*+
   Cancellazione di un set di visibilità.
-*/  
/***********************************************************/
static void CALLB clicked_set_visib_del(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   TCHAR       val[TILE_STR_LIMIT], *pch = NULL;
   int         Num = 0;
   double      ndouble = 0;
   C_STRING    pathDelFile;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 
   // Leggo il set selezionato nella lista
   ads_get_tile(hdlg, _T("set_visib_list"), val, TILE_STR_LIMIT);
   // Se non è stato selezionato nessun set errore
   if ((val == NULL) || (wcslen(val) == 0))
      ads_set_tile(hdlg, _T("error"), gsc_msg(666));      // "Nessun set di visibilità selezionato..." 
   else
   { 
      // Val = 0 .... significa primo elemento, Val = 1 secondo...
      Num = _wtoi(val) + 1;
      // Cancellazione del set di visibilità... cancellazione del file su disco
      pathDelFile = pCommonStruct->pCurrPrj->get_dir();
      pathDelFile += _T("\\Vis\\");
      pathDelFile += pCommonStruct->current_list_setv.getptr_at(Num)->get_name();
      pathDelFile += _T(".Vsb");
      if (gsc_delfile(pathDelFile) == GS_BAD)
      {
         ads_set_tile(hdlg, _T("error"), gsc_msg(667));      // "Errore nella cancellazione di un set..." 
         return;
      }
      // Cancella nella lista temporanea in memoria
      pCommonStruct->current_list_setv.remove_at(Num);
      if (pCommonStruct->current_list_setv.get_count() == 0)
      {
         // Disabilito i tasti di mod, del, etc
         ads_mode_tile(hdlg, _T("set_visib_mod"), MODE_DISABLE); 
         ads_mode_tile(hdlg, _T("set_visib_del"), MODE_DISABLE); 
         ads_mode_tile(hdlg, _T("set_visib_act"), MODE_DISABLE); 
         ads_mode_tile(hdlg, _T("set_visib_list"), MODE_DISABLE); 
      }
      // Ricarico la lista dei set nella Dcl
      if (loadListSet4Dcl(hdlg, pCommonStruct) == GS_BAD)
         ads_set_tile(hdlg, _T("error"), gsc_msg(667));      // "Errore nella cancellazione di un set..." 
   }

   return;
}


/***********************************************************/
/*        ACTION TILE                                      */
/*+
   Click su un elemento della lista dei set di visibilità.
-*/  
/***********************************************************/
static void CALLB clicked_set_visib_list(ads_callback_packet *dcl)
{
   ads_hdlg hdlg = dcl->dialog;

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);

   return;
}


/*************************************************************/
/*.doc int load_listSetOfProject                             */
/*+
   Funzione per il caricamento della lista dei nomi dei set di 
   visibilità del progetto corrente.
   Parametri:
   Common_Dcl_Visib_Set *CommonStruct: struttura delle variabili
                                       globali di passaggio.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************/
int load_listSetOfProject(Common_Dcl_Visib_Set *pCommonStruct)
{
   C_STRING strDir, mask, fname;
   int      result = GS_GOOD;
   long     nFileVsb = 0;
   presbuf  filename = NULL, pFilename = NULL;
   C_STR    *pstr = NULL;

   do
   {
      pCommonStruct->current_list_setv.remove_all();
      // Attraverso la path del prj verifico l'esistenza della directory
      // di memorizzazione dei set di visibilità
      strDir = pCommonStruct->pCurrPrj->get_dir();
      strDir += _T("\\Vis");
      // Se il path non esiste lo devo creare
      if (gsc_path_exist(strDir) == GS_BAD)
      {
         if (gsc_mkdir(strDir.get_name()) == GS_BAD) { result = GS_BAD; break; }
         break;
      }
      // Memorizzo la maschera
      mask = strDir; 
      mask += _T("\\*.Vsb");
      // Se esisteva la Dir devo memorizzare tutti i file *.Vsb
      nFileVsb = gsc_adir(mask.get_name(), &filename, NULL, NULL, NULL);
      if (nFileVsb == -1) { result = GS_BAD; break; }
      if (nFileVsb == 0) break;

      pFilename = filename;
      filename = filename->rbnext;
      while (filename && filename->restype == RTSTR)
      {
         if ((pstr = new C_STR) == NULL) { acutRelRb(pFilename); break; }
         // In filename->resval.rstring c'è il nome è l'estensione, tolgo l'estensione
         gsc_splitpath(filename->resval.rstring, NULL, NULL, &fname);
         pstr->set_name(fname.get_name());
         pCommonStruct->current_list_setv.add_tail(pstr);
         filename = filename->rbnext;
      }
      acutRelRb(pFilename);
      // Ordino la lista dei nomi dei set      
      pCommonStruct->current_list_setv.sort_name(TRUE, TRUE); // sensitive e ascending
      break;
   }
   while (0);

   return result;
}

/***********************************************************/
/*                       ACTION TILE                       */
/*+
   Inserimento del nome del set di visibilità.
   Lettura er verifica di correttezza.
-*/  
/***********************************************************/
static void CALLB manag_clicked_name_set(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   TCHAR       val[TILE_STR_LIMIT];
   Common_Dcl_Visib_Set *CommonStruct;

   CommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);
   
   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
   // Leggo il nome del set per verificare la correttezza
   ads_get_tile(hdlg, _T("name_set_visib"), val, TILE_STR_LIMIT);
   // Se non è stato selezionato nessun set errore
   if ((val == NULL) || (wcslen(val) == 0))
      ads_set_tile(hdlg, _T("error"), gsc_msg(668));      // "Nessun nome set di visibilità inserito..." 
   else
   { 
      ads_set_tile(hdlg, _T("name_set_visib"), val);
      if (CommonStruct->current_list_setv.search_name(val, FALSE) != NULL)
      {
         ads_set_tile(hdlg, _T("error"), gsc_msg(669));      // "Nome set di visibilità esistente..." 
         ads_mode_tile(hdlg, _T("name_set_visib"), MODE_SETFOCUS); 
      }
      gsc_strcpy(CommonStruct->current_set_visibility_list.name, val, MAX_LEN_NAMESETVISIB);
   }

   return;
}


/***********************************************************/
/*.doc int fromCurrentSet2DclList()                        */
/*+
   Funzione per il caricamento dell'elenco degli attributi della classe nella dcl.
   Parametri:
   ads_hdlg    hdlg;       handle della dcl aperta;
   C_SET_VISIB *pVisibSet; puntatore a l set di visibilità

  Restituisce GS_GOOD in caso di successo altrimenti  GS_BAD.
-*/  
/***********************************************************/
int fromCurrentSetAttribList2DclList(ads_hdlg hdlg, C_SET_VISIB *pVisibSet) 
{
   C_STRING stringa;
   C_ATTRIB *pAttrib;

   // Caricamento degli attributi
   ads_start_list(hdlg, _T("attrib_list"), LIST_NEW, 0);
   if (!pVisibSet || !pVisibSet->pclass)
   {
      ads_end_list();
      // Attivo i tasti selectAll deselectAll e tile di visibilità
      ads_mode_tile(hdlg, _T("selectall"),   MODE_DISABLE); 
      ads_mode_tile(hdlg, _T("deselectall"), MODE_DISABLE); 
      ads_mode_tile(hdlg, _T("vis_attr"),    MODE_DISABLE); 
      return GS_GOOD;
   }

   pAttrib = (C_ATTRIB *) pVisibSet->pclass->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      if (pAttrib->is_visible() == GS_GOOD)
      {
         if (pVisibSet->InvAttribList.search_name(pAttrib->name.get_name(), FALSE))
            stringa = gsc_msg(55); // "off"
         else
            stringa = gsc_msg(54); // "on"

         stringa += _T('\t');
         stringa += pAttrib->name;
         gsc_add_list(stringa);
      }
      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }                             
   ads_end_list(); 

   // Attivo i tasti selectAll deselectAll e tile di visibilità
   ads_mode_tile(hdlg, _T("selectall"),   MODE_ENABLE); 
   ads_mode_tile(hdlg, _T("deselectall"), MODE_ENABLE); 
   ads_mode_tile(hdlg, _T("vis_attr"),    MODE_ENABLE); 

   return GS_GOOD;
}


/***********************************************************/
/*                 ACTION TILE                             */
/*+
   Click su una classe nella lista. Ricavo gli attributi 
   della classe.
-*/  
/***********************************************************/
static void CALLB manag_clicked_class_list(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   TCHAR          val[TILE_STR_LIMIT];
   int            Num = 0;
   C_INT_INT_STR *plist = NULL;
   C_CLASS       *pCls = NULL;
   C_ATTRIB_LIST *p_attrib_list = NULL;
   C_ATTRIB      *p_attrib = NULL;
   C_STRING       stringa;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   ads_set_tile(hdlg, _T("error"), gsc_msg(360));      // "Attendere..." 
   // Ricavo dalla lista delle classi quella selezionata
   ads_get_tile(hdlg, _T("class_list"), val, TILE_STR_LIMIT);
   // Se non è stato selezionato nessun set errore
   if (wcslen(val) == 0)
      ads_set_tile(hdlg, _T("error"), gsc_msg(671)); // "Nessuna classe selezionata..." 
   else
   { 
      // Val = 0 .... significa primo elemento, Val = 1 secondo...
      Num = _wtoi(val) + 1;
      if ((plist = (C_INT_INT_STR *) pCommonStruct->ClassList.getptr_at(Num)) == NULL)
         ads_set_tile(hdlg, _T("error"), gsc_msg(672));      // "Selezione classe non valida..." 
      else
      {
         if (dcl->reason != CBR_DOUBLE_CLICK) // se non è doppio click
            // se la classe non aveva già il set di visibilità
            if (!pCommonStruct->current_set_visibility_list.get_VisibSet(plist->get_key(), plist->get_type()))
            {  // Cancello la lista degli attributi
               fromCurrentSetAttribList2DclList(hdlg, NULL);
               ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
               return;
            }

         // In plist ho il codice della classe in get_key()   
         if ((pCls = pCommonStruct->pCurrPrj->find_class(plist->get_key(), plist->get_type())) == NULL) 
         {
            ads_set_tile(hdlg, _T("error"), gsc_msg(673));      // "Classe non trovata..." 
            return;
         }

         // Cerco il set di visibilità per la classe scelta
         pCommonStruct->pCurrent_set_visibility = pCommonStruct->current_set_visibility_list.get_VisibSet(pCls);
         if (!pCommonStruct->pCurrent_set_visibility)
         {  // la classe non aveva un set di visibilità quindi lo aggiungo
            if ((pCommonStruct->pCurrent_set_visibility = new C_SET_VISIB) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return; }
            pCommonStruct->pCurrent_set_visibility->pclass = pCls;
            pCommonStruct->current_set_visibility_list.add_tail(pCommonStruct->pCurrent_set_visibility);
         }
         else if (dcl->reason == CBR_DOUBLE_CLICK) // se è doppio click
         {  // la classe aveva già un set di visibilità quindi lo cancello
            pCommonStruct->current_set_visibility_list.remove_at();
            pCommonStruct->pCurrent_set_visibility = NULL;
         }

         if (fromCurrentSetAttribList2DclList(hdlg, pCommonStruct->pCurrent_set_visibility) == GS_BAD)
         {
            ads_set_tile(hdlg, _T("error"), gsc_msg(672)); // "Selezione classe non valida..."
            return;
         }
      }
   }
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);

   // Visualizzo la lista delle classi del progetto
   ads_start_list(hdlg, _T("class_list"), LIST_NEW, 0);
   plist = (C_INT_INT_STR *) pCommonStruct->ClassList.get_head();
   while (plist)
   {  // se la classe è inclusa nel set di visibilità
      if (pCommonStruct->current_set_visibility_list.get_VisibSet(plist->get_key(), plist->get_type()))
         stringa = _T("*\t");
      else
         stringa = _T("\t");

      stringa += plist->get_name();
      gsc_add_list(stringa);
      plist = (C_INT_INT_STR *) pCommonStruct->ClassList.get_next();
   }                             
   ads_end_list(); 
   ads_set_tile(hdlg, _T("class_list"), val);

   return;
}


/***********************************************************/
/*      ACTION TILE                                        */
/*+
   L' utente ha cliccato sul tasto cancel.
-*/  
/***********************************************************/
static void CALLB manag_clicked_cancel(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   Common_Dcl_Visib_Set *CommonStruct;

   CommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 
   // Devo azzerare tutto !!
   CommonStruct->current_set_visibility_list.remove_all();
   CommonStruct->pCurrent_set_visibility = NULL;
   ads_done_dialog(hdlg, DLGCANCEL); 

   return;
}


/***********************************************************/
/*.doc int manag_clicked_accept()                          */
/*+
   L' utente ha cliccato sul tasto OK.
   Devo salvare le impostazioni su file.
-*/  
/***********************************************************/
static void CALLB manag_clicked_accept(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   TCHAR       val[TILE_STR_LIMIT];
   C_STR      *pStr = NULL;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 
   
   // Rileggo il nome del set
   ads_get_tile(hdlg, _T("name_set_visib"), val, TILE_STR_LIMIT);
   // Se non è stato selezionato nessun set errore
   if (val == NULL || wcslen(val) == 0)
   {
      ads_set_tile(hdlg, _T("error"), gsc_msg(668));      // "Nessun nome set di visibilità inserito..." 
      ads_mode_tile(hdlg, _T("name_set_visib"), MODE_SETFOCUS); 
   }
   else
   { 
      if (gsc_strcpy(pCommonStruct->current_set_visibility_list.name, val, MAX_LEN_NAMESETVISIB) == GS_BAD)
      {
         ads_set_tile(hdlg, _T("error"), gsc_msg(668)); // "Nessun nome set di visibilità inserito..."
         ads_mode_tile(hdlg, _T("name_set_visib"), MODE_SETFOCUS); 
      }

      if (pCommonStruct->current_set_visibility_list.save() == GS_BAD)
      {
         ads_set_tile(hdlg, _T("error"), gsc_msg(674)); // "Errore nel salvataggio del set di visibilità..." 
         return; 
      }
      
      // Se in pCommonStruct->current_list_setv non c'è un set con il nome del set 
      // corrente devo aggiungerlo perchè si tratta di nuovo set di visibilità
      if (pCommonStruct->current_list_setv.search_name(pCommonStruct->current_set_visibility_list.name) == NULL)
      {
         // Devo aggiornare pCommonStruct->current_list_setv
         if ((pStr = new C_STR(pCommonStruct->current_set_visibility_list.name)) == NULL)
            return;
         pCommonStruct->current_list_setv.add_tail(pStr);
      }

      // Temporaneo di prova
      pCommonStruct->current_set_visibility_list.remove_all();
      pCommonStruct->pCurrent_set_visibility = NULL;

      ads_done_dialog(hdlg, DLGOK); 
   }

   return;
}


/***********************************************************/
/*             ACTION TILE                                 */
/*+
   L' utente ha cliccato su un elemento della lista dei
   set di visibilità.
-*/  
/***********************************************************/
static void CALLB manag_clicked_selectAll(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   int         i = 0;
   C_STRING    str;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 

   C_ATTRIB *pAttrib = (C_ATTRIB *) pCommonStruct->pCurrent_set_visibility->pclass->ptr_attrib_list()->get_head();
   while (pAttrib)
   {
      if (pAttrib->is_visible() == GS_GOOD)
      {
         str = i++;
         ads_set_tile(hdlg, _T("attrib_list"), str.get_name());
      }
      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }

   return;
}

   
/***********************************************************/
/*                       ACTION TILE                       */
/*+
   L' utente ha cliccato su un elemento della lista dei
   set di visibilità.
-*/  
/***********************************************************/
static void CALLB manag_clicked_deselectAll(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 
   if (fromCurrentSetAttribList2DclList(hdlg, pCommonStruct->pCurrent_set_visibility) == GS_BAD)
      ads_set_tile(hdlg, _T("error"), gsc_msg(675));      // "Errore nel deselezionare attributi..." 

   return;
}


/***********************************************************/
/*                       ACTION TILE                       */
/*+
   L' utente ha cliccato su un elemento della lista dei
   set di visibilità.
-*/  
/***********************************************************/
static void CALLB manag_clicked_attrib_list(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   TCHAR          val[TILE_STR_LIMIT];
   int            Num, i = 0;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
   // Ricavo dalla lista delle classi quella selezionata
   ads_get_tile(hdlg, _T("attrib_list"), val, TILE_STR_LIMIT);
   // Se non è stato selezionato nessun set errore
   if (wcslen(val) == 0)
      ads_set_tile(hdlg, _T("error"), gsc_msg(676)); // "Nessun attributo selezionato..." 
   else
   { 
      // Val = 0 .... significa primo elemento, Val = 1 secondo...
      Num = _wtoi(val) + 1;
      C_ATTRIB *pAttrib = (C_ATTRIB *) pCommonStruct->pCurrent_set_visibility->pclass->ptr_attrib_list()->get_head();
      while (pAttrib)
      {
         if (pAttrib->is_visible() == GS_GOOD)
         {
            i++;
            if (i == Num) break;
         }
         pAttrib = (C_ATTRIB *) pAttrib->get_next();
      }

      if (!pAttrib)
         ads_set_tile(hdlg, _T("error"), gsc_msg(677));      // "Errore nella selezione attributi.." 
      else
      {
         if (dcl->reason == CBR_DOUBLE_CLICK) // doppio click
         { // toggle mode
            if (pCommonStruct->pCurrent_set_visibility->InvAttribList.search_name(pAttrib->name.get_name(), FALSE))
               ads_set_tile(hdlg, _T("vis_attr"), gsc_msg(54));      // "on"
            else                                   
               ads_set_tile(hdlg, _T("vis_attr"), gsc_msg(55));      // "off"
            
            manag_clicked_visAttrib(dcl);
         }
         else
         {
            // se è un attributo da nascondere
            if (pCommonStruct->pCurrent_set_visibility->InvAttribList.search_name(pAttrib->name.get_name(), FALSE))
               ads_set_tile(hdlg, _T("vis_attr"), gsc_msg(55));      // "off"
            else                                   
               ads_set_tile(hdlg, _T("vis_attr"), gsc_msg(54));      // "on"
         }
      }
   }

   return;
}


/***********************************************************/
/*                     ACTION TILE                         */
/*+
   L' utente ha cliccato su un elemento della lista dei
   set di visibilità.
-*/  
/***********************************************************/
static void CALLB manag_clicked_visAttrib(ads_callback_packet *dcl)
{
   ads_hdlg       hdlg = dcl->dialog;
   TCHAR          val[TILE_STR_LIMIT], valOnOff[TILE_STR_LIMIT], *punt = NULL;
   int            Num, ii, kk, cont, ToggleOnOff;
   C_STRING       buffer, stringa;
   C_INT_INT_STR *pAttrib = NULL;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 
   // Ricavo dalla lista delle classi quella selezionata
   ads_get_tile(hdlg, _T("attrib_list"), val, TILE_STR_LIMIT);
   // Se non è stato selezionato nessun set errore
   if ((val == NULL) || (wcslen(val) == 0))
      ads_set_tile(hdlg, _T("error"), gsc_msg(676));      // "Nessun attributo selezionato..." 
   else
   { 
      // Ricavo dal radio buttom il valore On/Off
      ads_get_tile(hdlg, _T("vis_attr"), valOnOff, TILE_STR_LIMIT);
      if (gsc_strcmp(valOnOff, gsc_msg(54)) == 0) // "on"
         ToggleOnOff = GS_GOOD;
      else
         ToggleOnOff = GS_BAD;

      buffer = val;
      // Devo stare attento perchè in val ci possono essere più valori,
      // perchè la selezione è multipla il formato di restituzione è "2 3 7 9 11"
      // Sostituisco gli spazi con \0 così ricavo il numero di elementi selezionati
      cont = gsc_strsep(buffer.get_name(), _T('\0'), _T(' '));
      // Incremento di uno perchè se gli spazi sono due i valori sono tre
      cont++; 
      if (cont == 1)
      {
         Num = _wtoi(val) + 1;

         C_ATTRIB *pAttrib = (C_ATTRIB *) pCommonStruct->pCurrent_set_visibility->pclass->ptr_attrib_list()->get_head();
         ii = 0;
         while (pAttrib)
         {
            if (pAttrib->is_visible() == GS_GOOD)
            {
               ii++;
               if (ii == Num) break;
            }
            pAttrib = (C_ATTRIB *) pAttrib->get_next();
         }

         if (!pAttrib)
            ads_set_tile(hdlg, _T("error"), gsc_msg(677));      // "Errore nella selezione attributi.." 
         else
            if (ToggleOnOff == GS_GOOD)
               pCommonStruct->pCurrent_set_visibility->DelInvAttrib(pAttrib->name.get_name());
            else
               pCommonStruct->pCurrent_set_visibility->AddInvAttrib(pAttrib->name.get_name());
      }
      else
      {
         // Attenzione anche alla restituzione di acad, il valore 2 corrisponde
         // al terzo elemento nella lista
         punt = buffer.get_name();
         for (ii = 1; ii <= cont; ii++)
         {
            if (ii != 1)
            {
               for (kk = 1; kk < ii; kk++)
                  while (*punt != _T('\0')) punt++; punt++; 
            }
            Num = _wtoi(punt) + 1;

            C_ATTRIB *pAttrib = (C_ATTRIB *) pCommonStruct->pCurrent_set_visibility->pclass->ptr_attrib_list()->get_head();
            kk = 0;
            while (pAttrib)
            {
               if (pAttrib->is_visible() == GS_GOOD)
               {
                  kk++;
                  if (kk == Num) break;
               }
               pAttrib = (C_ATTRIB *) pAttrib->get_next();
            }

            if (!pAttrib)
               ads_set_tile(hdlg, _T("error"), gsc_msg(677));      // "Errore nella selezione attributi.." 
            else
               if (ToggleOnOff == GS_GOOD)
                  pCommonStruct->pCurrent_set_visibility->DelInvAttrib(pAttrib->name.get_name());
               else
                  pCommonStruct->pCurrent_set_visibility->AddInvAttrib(pAttrib->name.get_name());
         }
      }
      if (fromCurrentSetAttribList2DclList(hdlg, pCommonStruct->pCurrent_set_visibility) == GS_BAD)
         ads_set_tile(hdlg, _T("error"), gsc_msg(675));      // "Errore nel deselezionare attributi..." 
   }
   
   return;
}


/***************************************************************/
/*.doc int set_visib_management()                              */
/*+
   Funzione per la gestione(creazione, modifica) di un set di 
   visibilità:
   Parametri: 
   int typeOperation: tipo di operazione;
                      SET_NEW -> Nuovo set di visibilità
                      SET_UPD -> Modifica di un set esistente;
                      int numSet: numero del set di visibilità;
   Common_Dcl_Visib_Set *pCommonStruct: struttura delle variabili
                         globali di passaggio.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/***************************************************************/
int set_visib_management(int typeOperation, int numSet, Common_Dcl_Visib_Set *pCommonStruct)
{
   int            status = 0, DclFile;
   ads_hdlg       dcl_id;
   C_INT_INT_STR *plist = NULL;
   C_CLASS       *psubcls = NULL;
   C_STRING       stringa, pth, pathDcl;

   pCommonStruct->current_set_visibility_list.pPrj = pCommonStruct->pCurrPrj;
   // In caso di aggiornamento di un set lo devo caricare in memoria
   if (typeOperation == SET_UPD)
   {
      // Attraverso la lista in memoria dei set del progetto ricavo 
      // il nome del set 
      gsc_strcpy(pCommonStruct->current_set_visibility_list.name,
                 pCommonStruct->current_list_setv.getptr_at(numSet)->get_name(),
                 MAX_LEN_NAMESETVISIB);
      pCommonStruct->current_set_visibility_list.load();      
   }
   else
   {
      pCommonStruct->current_set_visibility_list.remove_all();      
      pCommonStruct->current_set_visibility_list.name[0] = _T('\0');      
   }

   // Attivazione della DCL per scelta del progetto
   pathDcl = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_SETV.DCL");
   if (gsc_load_dialog(pathDcl, &DclFile) != RTNORM) return GS_BAD;

   if ((ads_new_dialog(_T("Management_Set"), DclFile, (CLIENTFUNC) NULLCB, &dcl_id) != RTNORM) || (dcl_id == NULL))
   {
      ads_unload_dialog(DclFile);
      GS_ERR_COD = eGSAbortDCL;
      return GS_BAD; 
   }

   // Se l' operazione è di modifica allora riempio l' edit del nome 
   if (typeOperation == SET_UPD)
   {
      ads_set_tile(dcl_id, _T("name_set_visib"),
                   pCommonStruct->current_list_setv.getptr_at(numSet)->get_name());
      ads_mode_tile(dcl_id, _T("name_set_visib"), MODE_DISABLE); 
   }
   else
      ads_action_tile(dcl_id, _T("name_set_visib"), (CLIENTFUNC) manag_clicked_name_set);
      ads_client_data_tile(dcl_id, _T("name_set_visib"), pCommonStruct);
   
   // Finchè non è stata scelta una classe i due tasti selectAll e deselect
   // e il tile di visibilità vanno disabilitati
   ads_mode_tile(dcl_id, _T("selectall"), MODE_DISABLE); 
   ads_mode_tile(dcl_id, _T("deselectall"), MODE_DISABLE); 
   ads_mode_tile(dcl_id, _T("vis_attr"), MODE_DISABLE); 

   // Visualizzo la lista delle classi del progetto
   ads_start_list(dcl_id, _T("class_list"), LIST_NEW, 0);
   plist = (C_INT_INT_STR *) pCommonStruct->ClassList.get_head();
   while (plist)
   {  // se la classe è inclusa nel set di visibilità
      if (pCommonStruct->current_set_visibility_list.get_VisibSet(plist->get_key(), plist->get_type()))
         stringa = _T("*\t");
      else
         stringa = _T("\t");

      stringa += plist->get_name();
      gsc_add_list(stringa);
      plist = (C_INT_INT_STR *) pCommonStruct->ClassList.get_next();
   }                             
   ads_end_list(); 
   
   // Abilitazione parte scelta progetto
   ads_action_tile(dcl_id, _T("class_list"),  (CLIENTFUNC) manag_clicked_class_list);
   ads_client_data_tile(dcl_id, _T("class_list"), pCommonStruct);

   ads_action_tile(dcl_id, _T("attrib_list"), (CLIENTFUNC) manag_clicked_attrib_list);
   ads_client_data_tile(dcl_id, _T("attrib_list"), pCommonStruct);

   // ads_action_tile(dcl_id, _T("is_active_set"), (CLIENTFUNC) manag_clicked_is_active);

   ads_action_tile(dcl_id, _T("selectall"), (CLIENTFUNC) manag_clicked_selectAll);
   ads_client_data_tile(dcl_id, _T("selectall"), pCommonStruct);

   ads_action_tile(dcl_id, _T("deselectall"), (CLIENTFUNC) manag_clicked_deselectAll);
   ads_client_data_tile(dcl_id, _T("deselectall"), pCommonStruct);

   ads_action_tile(dcl_id, _T("vis_attr"), (CLIENTFUNC) manag_clicked_visAttrib);
   ads_client_data_tile(dcl_id, _T("vis_attr"), pCommonStruct);

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) manag_clicked_accept);
   ads_client_data_tile(dcl_id, _T("accept"), pCommonStruct);

   ads_action_tile(dcl_id, _T("cancel"), (CLIENTFUNC) manag_clicked_cancel);
   ads_client_data_tile(dcl_id, _T("cancel"), pCommonStruct);

   // Partenza della DCL
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(DclFile);

   return GS_GOOD;
}

/***************************************************************/
/*                         ACTION TILE                         */
/*+
   Funzione per la creazione di un nuovo set di visibilità
   degli attributi.
-*/  
/***************************************************************/
static void CALLB clicked_set_visib_new(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   int         Num = 0;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   ads_set_tile(hdlg, _T("error"), gsc_msg(360));      // "Attendere..." 
   if (set_visib_management(SET_NEW, Num, pCommonStruct) == GS_BAD)
   {
      ads_set_tile(hdlg, _T("error"), gsc_msg(680));      // "Errore nella creazione di un nuovo set..."
      return;   
   }
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 
   // Aggiorno la lista della Dcl
   if (loadListSet4Dcl(hdlg, pCommonStruct) == GS_BAD) return;

   return;
}

/***************************************************************/
/*              ACTION TILE                                    */
/*+
   Funzione per la modifica di un set di visibilità esistente.
-*/  
/***************************************************************/
static void CALLB clicked_set_visib_mod(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   TCHAR       val[TILE_STR_LIMIT];
   int         Num = 0;
   Common_Dcl_Visib_Set *pCommonStruct;

   pCommonStruct = (Common_Dcl_Visib_Set*)(dcl->client_data);

   ads_set_tile(hdlg, _T("error"), gsc_msg(360));      // "Attendere..." 
    // Leggo il set selezionato nella lista
   ads_get_tile(hdlg, _T("set_visib_list"), val, TILE_STR_LIMIT);
   // Se non è stato selezionato nessun set errore
   if ((val == NULL) || (wcslen(val) == 0))
      ads_set_tile(hdlg, _T("error"), gsc_msg(671));      // "Nessun set di visibilità selezionato..." 
   else
   { 
      // Val = 0 .... significa primo elemento, Val = 1 secondo...
      Num = _wtoi(val) + 1;
      if (set_visib_management(SET_UPD, Num, pCommonStruct) == GS_BAD)
      {
         ads_set_tile(hdlg, _T("error"), gsc_msg(680));      // "Errore nella creazione di un nuovo set..."
         return;   
      }
      ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 
   }

   return;
}

/***************************************************************/
/*                ACTION TILE                                  */
/*+
   Funzione per la attivazione di un set di visibilità
   degli attributi.
-*/  
/***************************************************************/
static void CALLB clicked_set_visib_act(ads_callback_packet *dcl)
{
   ads_hdlg hdlg = dcl->dialog;
   TCHAR    val[TILE_STR_LIMIT];
   int      Num = 0;

   // Setto l' errore a null 
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR);
   ads_set_tile(hdlg, _T("error"), gsc_msg(360));      // "Attendere..." 
    // Leggo il set selezionato nella lista
   ads_get_tile(hdlg, _T("set_visib_list"), val, TILE_STR_LIMIT);
   // Se non è stato selezionato nessun set errore
   if ((val == NULL) || (wcslen(val) == 0))
      ads_set_tile(hdlg, _T("error"), gsc_msg(666));      // "Nessun set di visibilità selezionato..." 
   else
   { 
      // Val = 0 .... significa primo elemento, Val = 1 secondo...
      Num = _wtoi(val) + 1;
      ads_set_tile(hdlg, _T("error"), gsc_msg(681));      // "Attivazione del set di visibilità" 
            
      // Devo chiudere il dialog per attivare il set di visibilita
      ads_done_dialog(hdlg, DLGSTATUS + Num); 
   }

   return;
}


/***************************************************************/
/*             ACTION TILE                                     */
/*+
   Funzione per la lettura dei parametri di lancio del set.
-*/  
/***************************************************************/
static void CALLB clicked_accept4inputparam(ads_callback_packet *dcl)
{
   ads_hdlg    hdlg = dcl->dialog;
   TCHAR       val[TILE_STR_LIMIT];
   Common_Dcl_Activate_Param *pCommonStruct;

   pCommonStruct = (Common_Dcl_Activate_Param*)(dcl->client_data);
 
   // Setto l' errore a null e scrivo attendere
   ads_set_tile(hdlg, _T("error"), GS_EMPTYSTR); 
   ads_set_tile(hdlg, _T("error"), gsc_msg(360));      
   ads_get_tile(hdlg, _T("active_setonmemory"), val, TILE_STR_LIMIT);
   pCommonStruct->active_on_memory = (gsc_strcmp(val, _T("1")) == 0) ? GS_GOOD : GS_BAD;   
   
   ads_get_tile(hdlg, _T("add_object2saveset"), val, TILE_STR_LIMIT);
   pCommonStruct->add_object2saveset = (gsc_strcmp(val, _T("1")) == 0) ? GS_GOOD : GS_BAD;   

   ads_done_dialog(hdlg, DLGOK);

   return;
}

/***************************************************************/
/*.doc int get_param4activation(int *ActiveSetOnMemory,        */
/*                              int *AddObject2SaveSet)        */
/*+
   Funzione per la lettura da Dcl dei parametri di attivazione 
   del set di visibilità scelto dall' utente.
   La funzione ha due parametri di output:
      int *ActiveSetOnMemory  Flag di permanenza del set in memoria.
      int *AddObject2SaveSet  Flag di inserimento degli oggetti
                              nel gruppo di selezione del salvataggio.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/***************************************************************/
int get_param4activation(int *ActiveSetOnMemory, int *AddObject2SaveSet)
{
   ads_hdlg dcl_id;
   C_STRING pathDcl;
   int      status = 0, result = GS_GOOD, DclFile;
   Common_Dcl_Activate_Param CommonStruct;

   CommonStruct.active_on_memory = 0;
   CommonStruct.add_object2saveset = 0;

   // Attivazione della DCL per scelta del progetto
   pathDcl = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_SETV.DCL");
   if (gsc_load_dialog(pathDcl, &DclFile) != RTNORM) return GS_BAD;

   if ((ads_new_dialog(_T("Attivazione_Set"), DclFile, (CLIENTFUNC) NULLCB, &dcl_id) != RTNORM) || (dcl_id == NULL))
   {
      ads_unload_dialog(DclFile);
      GS_ERR_COD = eGSAbortDCL;
      return GS_BAD; 
   }

   if (GS_CURRENT_WRK_SESSION)
      if (GS_CURRENT_WRK_SESSION->get_level() != GSUpdateableData)
         ads_mode_tile(dcl_id, _T("add_object2saveset"), MODE_DISABLE);

   // Abilitazione parte scelta progetto
   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) clicked_accept4inputparam);
   ads_client_data_tile(dcl_id, _T("accept"), &CommonStruct);

   // Partenza della DCL
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(DclFile);

   if (status != DLGOK)
      result = GS_BAD;
   else
   {
      *ActiveSetOnMemory = CommonStruct.active_on_memory;
      *AddObject2SaveSet = CommonStruct.add_object2saveset;
   }

   return result;
}


/***************************************************************/
/*.doc void gsddsetvisib                                       */
/*+
   Comando con uso di interfaccia a finestra DCL per la gestione 
   dei set di visibilità.
-*/  
/***************************************************************/
void gsddsetvisib(void)
{
   int              status = 0, DclFile;
   ads_hdlg         dcl_id;
   C_STRING         pathFile, pathDcl;
   C_SET_VISIB_LIST *previous_setv = NULL;
   Common_Dcl_Visib_Set CommonStruct;  // nuova

   GEOsimAppl::CMDLIST.StartCmd();

   // verifico l'abilitazione dell' utente;
   if (gsc_check_op(opSetVisib) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
   
   CommonStruct.pCurrPrj = NULL;
   CommonStruct.pCurrent_set_visibility = NULL;

   // Se era attivo un set di visibilità lo devo memorizzare 
   if (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET)
      previous_setv = GEOsimAppl::ACTIVE_VIS_ATTRIB_SET;

   // Attivazione della DCL
   pathDcl = GEOsimAppl::WORKDIR + _T("\\") + GEODCLDIR + _T("\\GS_SETV.DCL");
   if (gsc_load_dialog(pathDcl, &DclFile) != RTNORM) return GEOsimAppl::CMDLIST.ErrorCmd();

   if ((ads_new_dialog(_T("Set_visibilita"), DclFile, (CLIENTFUNC) NULLCB, &dcl_id) != RTNORM) ||
       (dcl_id == NULL))
   {
      ads_unload_dialog(DclFile);
      GS_ERR_COD = eGSAbortDCL;
      return GEOsimAppl::CMDLIST.ErrorCmd(); 
   }

   gsc_SetPopDwnPrjList(dcl_id, _T("prj_name"));

   // Abilitazioni generali:
   // Se non sono in un' area attiva allora devo far scegliere il progetto
   // Altrimenti disabilito la scelta del progetto
   if (GS_CURRENT_WRK_SESSION) 
   {
      C_STRING StrPos;
      ads_callback_packet dcl;

      CommonStruct.pCurrPrj = GS_CURRENT_WRK_SESSION->get_pPrj();
      StrPos = GEOsimAppl::PROJECTS.getpos_name(CommonStruct.pCurrPrj->get_name());
      ads_set_tile(dcl_id, _T("prj_name"), StrPos.get_name()); 
      // Carico la lista dei nomi dei set di visibilità del progetto scelto
      dcl.dialog      = dcl_id;
      dcl.client_data = &CommonStruct;
      clicked_prjedit(&dcl);
   }

   ads_action_tile(dcl_id, _T("prj_name"), (CLIENTFUNC) clicked_prjedit);
   ads_client_data_tile(dcl_id, _T("prj_name"), &CommonStruct);

   // Abilitazione parte set di visibilità
   // Il tasto di attivazione del set si attiva solo se il prj
   // scelto dall' utente è uguale a GS_CURRENT_WRK_SESSION->get_pPrj()
   ads_action_tile(dcl_id, _T("set_visib_new"), (CLIENTFUNC) clicked_set_visib_new);
   ads_client_data_tile(dcl_id, _T("set_visib_new"), &CommonStruct); 

   ads_action_tile(dcl_id, _T("set_visib_mod"), (CLIENTFUNC) clicked_set_visib_mod);
   ads_client_data_tile(dcl_id, _T("set_visib_mod"), &CommonStruct);
   
   ads_action_tile(dcl_id, _T("set_visib_del"), (CLIENTFUNC) clicked_set_visib_del);
   ads_client_data_tile(dcl_id, _T("set_visib_del"), &CommonStruct);  

   ads_action_tile(dcl_id, _T("set_visib_act"), (CLIENTFUNC) clicked_set_visib_act);

   ads_action_tile(dcl_id, _T("accept"), (CLIENTFUNC) chiudiTutto);
   ads_client_data_tile(dcl_id, _T("accept"), &CommonStruct); 
   
   ads_action_tile(dcl_id, _T("set_visib_list"), (CLIENTFUNC) clicked_set_visib_list);
   ads_action_tile(dcl_id, _T("help"), (CLIENTFUNC) clicked_set_help);

   // Se non è stato ancora scelto un prj disabilito tutto
   if (CommonStruct.pCurrPrj == NULL)
   {
      ads_mode_tile(dcl_id, _T("set_visib_new"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("set_visib_mod"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("set_visib_del"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("set_visib_act"), MODE_DISABLE); 
      ads_mode_tile(dcl_id, _T("set_visib_list"), MODE_DISABLE); 
   }

   // Partenza della DCL
   ads_start_dialog(dcl_id, &status);
   ads_unload_dialog(DclFile);

   // Se la Dcl è stata chiusa dopo un' attivazione il set attivato, se scelto
   // dall' utente rimane permanente in memoria.
   if (status >= DLGSTATUS)
   {
      int resInput, ActiveSetOnMemory = GS_BAD, AddObject2SaveSet = GS_BAD;

      // Leggo tramite Dcl i parametri per l' attivazione del set
      resInput = get_param4activation(&ActiveSetOnMemory, &AddObject2SaveSet);
      
      // Se la lettura dei parametri è andata a buon fine 
      if (resInput == GS_GOOD)
      {
         acutPrintf(gsc_msg(360)); // "Attendere..."
         
         // Alloco lo spazio in memoria ed attivo il set
         if ((GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = new C_SET_VISIB_LIST) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }

         GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->pPrj = CommonStruct.pCurrPrj;
         gsc_strcpy(GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->name,
                    CommonStruct.current_list_setv.getptr_at(status - DLGSTATUS)->get_name(),
                    MAX_LEN_NAMESETVISIB);
   
         // Carico in memoria il set dal file
         if (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->load() == GS_GOOD)
         {
            C_SELSET SelSet;

            if (gsc_ssget(NULL, NULL, NULL, NULL, SelSet) == RTNORM)
               GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->activate(SelSet, AddObject2SaveSet);
         }
         // Devo verificare se il set rimane permanente in memoria
         if (ActiveSetOnMemory == GS_BAD)
         {
            if (previous_setv) delete (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET);
            GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = previous_setv;
         }
         else
           if (previous_setv) delete previous_setv;
      }
   }

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*************************************************************/
/* Inizio Funzioni della classe C_SET_VISIB                  */

// Costruttore
C_SET_VISIB::C_SET_VISIB():C_NODE()
{
   pclass = NULL;
}

// Distruttore
C_SET_VISIB::~C_SET_VISIB() {}


/***************************************************************/
/*.doc int C_SET_VISIB::AddInvAttrib                           */
/*+
  La funzione aggiunge un attributo che deve essere reso invisibile.
  Parametri:
  const TCHAR *AttrName; Nome dell'attributo da aggiungere

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB::AddInvAttrib(const TCHAR *AttrName)
{
   if (!pclass) return GS_BAD;

   // esiste l'attributo ed era dichiarato visibile
   if (pclass->ptr_attrib_list()->search_name(AttrName, FALSE) &&
      ((C_ATTRIB *) pclass->ptr_attrib_list()->get_cursor())->is_visible() == GS_GOOD)
      // se non era già presente
      if (!InvAttribList.search_name(AttrName, FALSE))
      {
         C_INT_STR *pAttrib;

         if ((pAttrib = new C_INT_STR()) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pAttrib->set_name(AttrName);
         InvAttribList.add_tail(pAttrib);
      }

   return GS_GOOD;
}


/***************************************************************/
/*.doc int C_SET_VISIB::DelInvAttrib                           */
/*+
  La funzione cancello un attributo che doveva essere reso invisibile.
  Parametri:
  const TCHAR *AttrName; Nome dell'attributo da cancellare

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB::DelInvAttrib(const TCHAR *AttrName)
{
   // se era già presente
   if (InvAttribList.search_name(AttrName, FALSE)) InvAttribList.remove_at();

   return GS_GOOD;
}


/***************************************************************/
/*.doc int C_SET_VISIB::save                                   */
/*+
   La funzione salva su file gli attributi che devono essere resi invisibili.
   Parametri:
   FILE *file; puntatore al file in cui scrivere il set

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB::save(FILE *file)
{
   C_STRING    sez;
   C_INT_STR   *pAttrib;
   C_2STR_LIST EntryList;
   C_2STR      *pEntry;

   if (!pclass) return GS_BAD;

   sez = _T("CLS");
   sez += pclass->ptr_id()->code;
   sez += _T("SUB");
   sez += pclass->ptr_id()->sub_code;

   pAttrib = (C_INT_STR *) InvAttribList.get_head();
   while (pAttrib)
   {
      if ((pEntry = new C_2STR(pAttrib->get_name(), INVISIBLE)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
      EntryList.add_tail(pEntry);

      pAttrib = (C_INT_STR *) InvAttribList.get_next();
   }

   if (gsc_set_profile(file, sez.get_name(), EntryList) == GS_BAD) return GS_BAD;

   return gsc_set_profile(file, _T("INFO"), sez.get_name(), GS_EMPTYSTR);
}


/***************************************************************/
/*.doc int C_SET_VISIB::load                                   */
/*+
   La funzione carica da file gli attributi che devono essere resi invisibili.
   Parametri:
   FILE *file; puntatore al file in cui scrivere il set

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB::load(FILE *file, bool Unicode)
{
   C_STRING    sez;
   C_INT_STR   *pAttrib;
   C_2STR_LIST EntryList;
   C_2STR      *pEntry;

   if (!pclass) return GS_BAD;

   InvAttribList.remove_all();

   sez = _T("CLS");
   sez += pclass->ptr_id()->code;
   sez += _T("SUB");
   sez += pclass->ptr_id()->sub_code;

   if (gsc_get_profile(file, sez.get_name(), EntryList, Unicode) == GS_BAD)
      return GS_GOOD; // tutti gli attributi visibili a ON

   pEntry = (C_2STR *) EntryList.get_head();
   while (pEntry)
   {  // esiste l'attributo ed era dichiarato visibile
      if (pclass->ptr_attrib_list()->search_name(pEntry->get_name(), FALSE) &&
          ((C_ATTRIB *) pclass->ptr_attrib_list()->get_cursor())->is_visible() == GS_GOOD)
      {
         if ((pAttrib = new C_INT_STR()) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pAttrib->set_name(pEntry->get_name());
         InvAttribList.add_tail(pAttrib);
      }
      pEntry = (C_2STR *) EntryList.get_next();
   }

   return GS_GOOD;
}


/***************************************************************/
/*.doc int C_SET_VISIB::del                                    */
/*+
   La funzione cancella da file gli attributi che devono essere resi invisibili.
   Parametri:
   FILE *file; puntatore al file in cui scrivere il set

   Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB::del(FILE *file)
{
   C_STRING sez;

   if (!pclass) return GS_BAD;

   sez = _T("CLS");
   sez += pclass->ptr_id()->code;
   sez += _T("SUB");
   sez += pclass->ptr_id()->sub_code;

   if (gsc_delete_entry(file, _T("INFO"), sez.get_name()) == GS_BAD) return GS_BAD;

   return gsc_delete_sez(file, sez.get_name());
}


/***************************************************************/
/*.doc int C_SET_VISIB::activate                               */
/*+
  Funzione della classe C_SET_VISIB per l' attivazione del set
  Parametri:
  C_SELSET &SelSet;       gruppo di selezione contenente le
                          entità da elaborare
  int AddObject2SaveSet   flag per l' inserimento nel gruppo
                          di selezione del salvataggio.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB::activate(C_SELSET &SelSet, int AddObject2SaveSet)
{
   int                 result = GS_GOOD, cls, sub, isGraphCalculated;
   long                cont = 0, k = 0, QtyTot = 0, spente;
   ads_name            graphEnt;
   C_ATTRIB_BLOCK      blk;
   C_EED               eed;
   C_SELSET            InternalSS, UpdSet;
   C_PREPARED_CMD_LIST CmdList;
   C_RB_LIST           ColValues;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(681)); // "Attivazione del set di visibilità"

   if (!pclass || !pclass->ptr_fas()) return GS_BAD;
   cls = pclass->ptr_id()->code;
   sub = pclass->ptr_id()->sub_code;

   SelSet.copyIntersectClsCode(InternalSS, cls, sub);
   // Ciclo il gruppo di selezione e se incontro un'entità grafica
   // che non sia un blocco DA aggiungo tutti i blocchi DA ad essa associati
   // ed elimino l'entità grafica in modo che il gruppo di selezione
   // contenga solo blocchi DA della classe interessata
   while (SelSet.entname(cont++, graphEnt) == GS_GOOD)
      if (eed.load(graphEnt) == GS_GOOD && eed.cls == cls && eed.sub == sub)
         if (gsc_is_DABlock(graphEnt) == GS_BAD)
         {
            if (pclass->get_SelSet(graphEnt, UpdSet, DA_BLOCK) == GS_GOOD)
               InternalSS.add_selset(UpdSet);
         }
         else
            InternalSS.add(graphEnt);

   UpdSet.clear();
   cont = 0;

   // A questo punto il mio gruppo di selezione contiene tutti blocchi scheda
   // e ho il puntatore alla classe di appartenenza, scorro il gruppo è spengo/accendo 
   // gli attributi
   if ((QtyTot = InternalSS.length()) <= 0) return GS_GOOD;

   if (pclass->ptr_fas()->is_graph_calculated() == GS_BAD)
      isGraphCalculated = FALSE;
   else
   {
      isGraphCalculated = TRUE;
      // Preparo i comandi di lettura dei dati della classe dal temp/old
      if (pclass->prepare_data(CmdList) == GS_BAD) return GS_BAD;
   }

   StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

   spente = 0;
   while (InternalSS.entname(cont, graphEnt) == GS_GOOD)
   {
      if (eed.load(graphEnt) == GS_GOOD && eed.cls == cls && eed.sub == sub &&
          gsc_is_DABlock(graphEnt) == GS_GOOD)
      {
         // Solo se visibile
         if (eed.vis == VISIBLE)
         {
            if (blk.set_ent_name(graphEnt) == GS_BAD) 
               { result = GS_BAD; break; }
            if (blk.apply_visib_set(InvAttribList) == GS_BAD)
               { result = GS_BAD; break; }
            if (AddObject2SaveSet == GS_GOOD)
            {
               if (UpdSet.add(graphEnt) == GS_BAD)
                  { result = GS_BAD; break; }
            }
            else // se esistono funzioni di calcolo grafico
                 // le applico perchè potrebbero variare gli attributi visibili
               if (isGraphCalculated)
                  if (pclass->query_data(graphEnt, ColValues, &CmdList) == GS_GOOD)
                     pclass->graph_calc(ColValues);
         }
         else
            spente++;
      }

      StatusLineMsg.Set(++cont); // "%ld entità GEOsim elaborate."
   }
   StatusLineMsg.End(gsc_msg(310), cont); // "%ld entità GEOsim elaborate."

   if (spente > 0)
      acutPrintf(gsc_msg(683), spente); // "\nTrovati %ld blocchi di attributi spenti."

   if (AddObject2SaveSet == GS_GOOD)
   {  
      GEOsimAppl::REFUSED_SS.clear();
      //                  selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
      if (gsc_class_align(UpdSet, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), GS_GOOD, FAS_MOD) == -1)
         return GS_BAD;
   }

   return result;
}


/*************************************************************/
/* Fine Funzioni della classe C_SET_VISIB                    */
/* Inizio Funzioni della classe C_SET_VISIB_LIST             */
/*************************************************************/

// Costruttore
C_SET_VISIB_LIST::C_SET_VISIB_LIST():C_LIST()
{
   name[0] = _T('\0');
}

// Distruttore
C_SET_VISIB_LIST::~C_SET_VISIB_LIST() {}


/***************************************************************/
/*.doc int C_SET_VISIB_LIST::get_Path                          */
/*+
  Restituisce la path del file contenente la lista dei set di visibilità.
-*/  
/***************************************************************/
int C_SET_VISIB_LIST::get_Path(C_STRING &Path)
{
   if (wcslen(name) == 0 || !pPrj) return GS_BAD;

   Path = pPrj->get_dir();
   Path += _T("\\Vis\\");
   Path += name;
   Path += _T(".Vsb");
   
   return GS_GOOD;
}


/***************************************************************/
/*.doc int C_SET_VISIB_LIST::del                               */
/*+
  Cancella il file contenente la lista dei set di visibilità.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB_LIST::del()
{
   C_STRING Path;

   if (get_Path(Path) == GS_BAD) return GS_BAD;
   if (gsc_path_exist(Path) == GS_GOOD) return gsc_delfile(Path);
   remove_all();
   
   return GS_GOOD;
}


/***************************************************************/
/*.doc int C_SET_VISIB_LIST::load                              */
/*+
  Caricamento della lista dei set di visibilità.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB_LIST::load(void)
{
   C_SET_VISIB *pVisibSet;
   FILE        *file;
   C_STRING    Path;
   C_2STR_LIST EntryList;
   C_2STR      *pEntry;
   C_CLASS     *pClass;
   int         cls, sub;
   bool        Unicode;

   if (get_Path(Path) == GS_BAD) return GS_BAD;
   if (gsc_path_exist(Path) == GS_BAD) { GS_ERR_COD = eGSPathNoExist; return GS_BAD; }

   // Apro il file in lettura
   if ((file = gsc_open_profile(Path, READONLY, MORETESTS, &Unicode)) == NULL)
      return GS_BAD;
   // Leggo la lista delle classi
   if (gsc_get_profile(file, _T("INFO"), EntryList, Unicode) == GS_BAD)
      { gsc_fclose(file); return GS_BAD; }

   pEntry = (C_2STR *) EntryList.get_head();
   while (pEntry)
   {
      cls = _wtoi(wcsstr(pEntry->get_name(), _T("CLS")) + wcslen(_T("CLS")));
      sub = _wtoi(wcsstr(pEntry->get_name(), _T("SUB")) + wcslen(_T("SUB")));
      
      if ((pClass = pPrj->find_class(cls, sub)) != NULL)
      {
         if ((pVisibSet = new C_SET_VISIB) == NULL)
            { gsc_fclose(file); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pVisibSet->pclass = pClass;
         if (pVisibSet->load(file, Unicode) == GS_BAD) delete pVisibSet;
         else add_tail(pVisibSet);
      }
      
      pEntry = (C_2STR *) EntryList.get_next();
   }

   if (gsc_fclose(file) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/***************************************************************/
/*.doc int C_SET_VISIB_LIST::save                              */
/*+
  Salvataggio della lista dei set di visibilità.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB_LIST::save(void)
{
   C_SET_VISIB *pVisibSet;
   FILE        *file;
   C_STRING    Path;

   if (get_Path(Path) == GS_BAD) return GS_BAD;
   if (gsc_path_exist(Path) == GS_GOOD)
      if (gsc_delfile(Path) == GS_BAD) return GS_BAD;

   // Apro il file in lettura
   if ((file = gsc_open_profile(Path)) == NULL) return GS_BAD;

   pVisibSet = (C_SET_VISIB *) get_head();
   while (pVisibSet)
   {
      if (pVisibSet->save(file) == GS_BAD) 
         { gsc_close_profile(file); return GS_BAD; }
      pVisibSet = (C_SET_VISIB *) get_next();
   }

   return gsc_close_profile(file);
}


/***************************************************************/
/*.doc int C_SET_VISIB_LIST::get_VisibSet                      */
/*+
  Ritorna il set divisibilità della classe data.
  Parametri:
  C_CLASS *pCls;     puntatore alla classe da cercare
  oppure
  int cls;           codice classe
  int sub;           codice sottoclasse (default = 0)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
C_SET_VISIB *C_SET_VISIB_LIST::get_VisibSet(C_CLASS *pCls)
{
   C_SET_VISIB *p;
      
   p = (C_SET_VISIB *) get_head();
   while (p)
   {
      if (p->pclass == pCls) return p;
      p = (C_SET_VISIB *) get_next();
   }                             
   
   return NULL;
}
C_SET_VISIB *C_SET_VISIB_LIST::get_VisibSet(int cls, int sub)
{
   C_SET_VISIB *p;
      
   p = (C_SET_VISIB *) get_head();
   while (p)
   {
      if (p->pclass &&
          p->pclass->ptr_id()->code == cls && p->pclass->ptr_id()->sub_code == sub)
         return p;

      p = (C_SET_VISIB *) get_next();
   }                             
   
   return NULL;
}


/***************************************************************/
/*.doc int C_SET_VISIB_LIST::activate                          */ 
/*+
  Funzione della classe C_SET_VISIB_LIST per l' attivazione del
  set di visibilità.
  Parametri:
  C_SELSET &SelSet;       gruppo di selezione contenente le
                          entità da elaborare
  int AddObject2SaveSet   flag per l' inserimento nel gruppo
                          di selezione del salvataggio.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***************************************************************/
int C_SET_VISIB_LIST::activate(C_SELSET &SelSet, int AddObject2SaveSet)
{
   C_SET_VISIB *pVisibSet;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   
   pVisibSet = (C_SET_VISIB *) get_head();
   while (pVisibSet)
   {     
      // Attivo il set per questa classe
      if (pVisibSet->activate(SelSet, AddObject2SaveSet) == GS_BAD) return GS_BAD;

      pVisibSet = (C_SET_VISIB *) get_next();
   }
   while (0);

   return GS_GOOD;
}  

/* Fine Funzioni della classe C_SET_VISIB_LIST               */
/*************************************************************/


/*************************************************************/
/* Inizio funzioni lisp per set di visibilità                */

/*********************************************************/
/*.doc gs_rsetv                               <external> */
/*+
  Questa funzione LISP restituisce l'elenco dei set di visibilità 
  di un progetto passato in input.
-*/  
/*********************************************************/
int gs_rsetv(void)
{
   presbuf     arg = NULL, filename = NULL, pFilename = NULL;
   C_STRING    strDir, maschera, strcomodo;
   C_PROJECT  *pPrj = NULL;
   C_RB_LIST   ret;
   long        nFileVsb = 0;
   int         prj;
   
   acedRetNil();
   arg = acedGetArgs();
   
   // Codice del progetto
   if (arg == NULL || arg->restype != RTSHORT)  
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   prj = arg->resval.rint;

   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      acedRetNil();
   else
   {
      do
      {
         // Attraverso la path del prj verifico l'esistenza della directory
         // di memorizzazione dei set di visibilità
         strDir = pPrj->get_dir();
         strDir += _T("\\Vis");
         // Se il path non esiste non esistono set di visibilità
         if (gsc_path_exist(strDir.get_name()) == GS_BAD)
            { GS_ERR_COD = eGSOutOfMem; break; } 
         // Memorizzo la maschera
         maschera = strDir.get_name();
         maschera += _T("\\*.Vsb");
         // Se esisteva la Dir devo memorizzare tutti i file *.Vsb
         nFileVsb = gsc_adir(maschera.get_name(), &filename, NULL, NULL, NULL);
         if(nFileVsb <= 0) break;  

         pFilename = filename;
         filename = filename->rbnext;
         while (filename && filename->restype == RTSTR)
         {
            // Compongo il nome con la path
            strcomodo = strDir;
            strcomodo += _T("\\");
            strcomodo += filename->resval.rstring;
            // Memorizzo il nome
            if ((ret += acutBuildList(RTSTR, strcomodo.get_name(), 0)) == NULL)
               { GS_ERR_COD = eGSOutOfMem; break; } 
            filename = filename->rbnext;
         }
         acutRelRb(pFilename);
         break;
      }
      while (0);
   }
   if (ret.get_head() != NULL) 
      ret.LspRetList();   
   else
      acedRetNil(); 

   return RTNORM;
}


/*********************************************************/
/*.doc gs_current_setv <external>                        */
/*+
  Questa funzione LISP restituisce il nome del set di
  visibilità corrente, se esiste.
-*/  
/*********************************************************/
int gs_current_setv(void)
{
   presbuf arg;

   acedRetNil();
   arg = acedGetArgs();

   // Se non sono in un' area esco
   if (!GS_CURRENT_WRK_SESSION) return RTNORM; 
   // Se non esiste alcun set attivo esco
   if (!GEOsimAppl::ACTIVE_VIS_ATTRIB_SET) return RTNORM;
   
   acedRetStr(GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->name);

   return RTNORM;
}


/*********************************************************/
/*.doc gs_activesetv <external>                        */
/*+
  Questa funzione LISP attiva un set di visibilità passato
  come parametro.
  Parametri:
  (<set name><selset><ActiveOnMemory><SaveObjectModified>)
  Il secondo parametro è il flag per far si che rimanga 
  attivo in memoria;
  Il terzo parametro è il flag aggiungere gli oggetti
  modificati nel gruppo di selezione del salvataggio. 
-*/  
/*********************************************************/
int gs_activesetv(void)
{
   presbuf          arg;
   TCHAR            *pSetName;
   C_SET_VISIB_LIST *pPrevSet;
   C_SELSET         SelSet;
   int              ActiveOnMemory, SaveObjectModified, result = GS_BAD;
   
   acedRetNil();
   arg = acedGetArgs();

   // Verifico l'abilitazione dell' utente;
   if (gsc_check_op(opSetVisib) == GS_BAD) return GS_BAD;

   // Nome del set di visibilità da attivare
   if (arg == NULL || arg->restype != RTSTR)  
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   pSetName = arg->resval.rstring;

   // Flag che stabilisce se il set rimane attivo in memoria
   if ((arg = arg->rbnext) == NULL || arg->restype != RTPICKS)  
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   SelSet.add_selset(arg->resval.rlname);

   // Flag che stabilisce se il set rimane attivo in memoria
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT)  
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   ActiveOnMemory = arg->resval.rint;

   // Flag che stabilisce se gli oggetti modificati saranno salvati
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSHORT)  
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   SaveObjectModified = arg->resval.rint;

   // Se non sono in un' area esco
   if (!GS_CURRENT_WRK_SESSION) 
      { GS_ERR_COD = eGSNotCurrentSession; return RTERROR; }
   
   // Se esiste già un set di visibilità attivo lo memorizzo in una 
   // C_SET_VISIB_LIST di comodo
   pPrevSet = GEOsimAppl::ACTIVE_VIS_ATTRIB_SET;

   // Alloco una nuova C_SET_VISIB_LIST
   if ((GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = new C_SET_VISIB_LIST) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return RTERROR; }

   GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->pPrj = GS_CURRENT_WRK_SESSION->get_pPrj();
   gsc_strcpy(GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->name, pSetName, MAX_LEN_NAMESETVISIB);
   
   do
   {   
      // Carico in memoria il set dal file
      if (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->load() == GS_BAD) break;

      // Attivo il set di visibilità
      if (GEOsimAppl::ACTIVE_VIS_ATTRIB_SET->activate(SelSet, SaveObjectModified) == GS_BAD)
         break;

      result = GS_GOOD;
   }
   while (0);

   // Se si è verificato un errore ripristino la situazione prima 
   // del lancio del nuovo set
   if (result == GS_BAD)
   {
      delete GEOsimAppl::ACTIVE_VIS_ATTRIB_SET;
      GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = pPrevSet;
   }
   else
   {
      // Se sono arrivato a questo punto è andato tutto bene e
      // dopo aver attivato il set di visibilità se il flag di permanenza
      // del set in memoria è attivo rilascio lo spazio in memoria del 
      // vecchio set
      if (ActiveOnMemory == GS_GOOD)
         if (pPrevSet) delete pPrevSet;
      else
      {
         delete GEOsimAppl::ACTIVE_VIS_ATTRIB_SET;
         GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = pPrevSet;
      }
   }  

   return RTNORM;
}


/* Fine funzioni lisp per set di visibilità                  */
/*************************************************************/

