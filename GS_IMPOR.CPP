/**********************************************************
Name: GS_IMPORT

Module description: File contenente le funzioni per 
                    l'importazione e l'esportazione dati GEOsim.
            
Author: Paolo De Sole, Poltini Roberto.
                                                       
(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.A.


Modification history:
              
Notes and restrictions on use: 

**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"     // MFC core and standard components

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
#include "gs_sql.h"     
#include "gs_evid.h"     
#include "gs_impor.h"     
#include "gs_sec.h"     


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
#endif


// Definizioni 
#define     SKIPHANDLETOONE // Se definito utilizzo il tapullo dell' handle - 1
                            // per ricavare i DA (Mtext) dagli oggetti di AutoCAD.

// V A R I A B I L I   G L O B A L I
// La seguente mi serve per memorizzare lo stato dei LPN coinvolti nelle operazioni di IMPORT
// C_2STR_INT_LIST  LPNACTIVATED;

C_2STR_INT_LIST LPN_USER_STATUS;


// D E F I N I Z I O N E    F U N Z I O N I
int gsc_isAcMapAnnotation(ads_name obj);
int gsc_queryAcadObject(ads_name ent, C_STRING &nameLPN, C_RB_LIST &ColValues,
                        int RemoveLPN, CAseLinkSel *pLS = NULL);
int analyzingLPN(C_STR_LIST *pLPNList);
int objectIsLinked(ads_name ent, C_STR_LIST *pLPNList, C_STRING &nameValidLPN);
int gsc_objectIsODLinked(ads_name ent, C_STR_LIST *pValidODList, C_STRING &ValidODTable);

int gsc_isValidExternalLPN(const TCHAR *NameLPN, CAseLinkPath &InternalLPN);
int is_enableLPN(C_STRING &lpName);

int gsc_ImportBlocksFromFile(FILE *pFile, bool Unicode, C_CLASS *pClass,
                             C_DBCONNECTION *pConn = NULL, C_STRING *pTable = NULL,
                             C_STRING *pKeyAttr = NULL);
int gsc_ImportTextsFromFile(FILE *pFile, bool Unicode, C_CLASS *pClass,
                            C_DBCONNECTION *pConn = NULL, C_STRING *pTable = NULL,
                            C_STRING *pKeyAttr = NULL);
int gsc_ImportPlinesFromFile(FILE *pFile, bool Unicode, C_CLASS *pClass,
                             C_DBCONNECTION *pConn = NULL, C_STRING *pTable = NULL,
                             C_STRING *pKeyAttr = NULL);


//////////////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc objectIsLinked()                       <internal> */
/*+
  Questa funzione determina se l' oggetto passato in input
  ha dei Lpn, e in casi positivo restituisce il LPN 
  valido. In caso di più Lpn restituisce il primo valido.
  Param: 
      ads_name    ent               Oggetto da controllare (input)
      C_STR_LIST *pValidLPNList;    Puntatore alla lista dei LPN validi (input)
      C_STRING   &ValidLPN;         Nome del LPN per la lettura (out)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int objectIsLinked(ads_name ent, C_STR_LIST *pValidLPNList, C_STRING &ValidLPN)
{
   int        is_linked = GS_BAD;
   C_STR_LIST LPNames;

   ValidLPN.clear();

   // Leggo i LPT usati da questa entità
   if (!pValidLPNList || gsc_getLinkNames(ent, LPNames) == GS_BAD) return GS_BAD;

   if (LPNames.get_count() > 0)
   {
      C_STR *pLPName = (C_STR *) LPNames.get_head();

      while (pLPName)
      {
         if (pValidLPNList->search_name(pLPName->get_name(), FALSE) != NULL)
            { is_linked = GS_GOOD; ValidLPN = pLPName->get_name(); break; }
         pLPName = (C_STR *) pLPName->get_next();
      }
   }

   return is_linked;
}


/*********************************************************/
/*.doc gsc_objectIsODLinked                   <internal> */
/*+
  Questa funzione determina se l' oggetto passato in input
  ha delle tabelle OD, e in caso positivo restituisce il nome della
  tabella valida. In caso di più tabelle restituisce la prima.
  Param: 
  ads_name    ent             Oggetto da controllare (input)
  C_STR_LIST *pValidODList;   Puntatore alla lista delle tabelle OD valide (input)
  C_STRING   &ValidODTable;   Nome della tabella OD per la lettura (out)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int gsc_objectIsODLinked(ads_name ent, C_STR_LIST *pValidODList, C_STRING &ValidODTable)
{
   int        is_linked = GS_BAD;
   C_STR_LIST ODTables;

   ValidODTable.clear();

   // Leggo i LPT usati da questa entità
   if (!pValidODList || gsc_getODTables(ent, ODTables) == GS_BAD) return GS_BAD;

   if (ODTables.get_count() > 0)
   {
      C_STR *pODTable = (C_STR *) ODTables.get_head();

      while (pODTable)
      {  // No sensitive
         if (pValidODList->search_name(pODTable->get_name(), GS_BAD) != NULL)
            { is_linked = GS_GOOD; ValidODTable = pODTable->get_name(); break; }
         pODTable = (C_STR *) pODTable->get_next();
      }
   }

   return is_linked;
}


/*******************************************************/
/*.doc int gsc_importFromFile(void)                    */
/*+                                                                   
  Funzione base che importa degli oggetti leggendo le 
  caratteristiche da un file specificato (compresa la path)
  nel parametro fileName.
   Parametri:
   C_CLASS *pCls              Puntatore alla classe di destinazione;
   C_STRING &fileName         Path completa del file delle specifiche;
   C_DBCONNECTION *pConn;     Opzionale, se presente indica il riferimento
                              ad un database da cui leggere gli attributi
                              (Default = NULL)
   C_STRING       *pTable;    Se pConn è valido, indica la tabella da cui
                              leggere gli attributi (Default = NULL)
   C_STRING       *pKeyAttr;  Se pConn è valido, indica l'attributo chiave
                              per leggere i valori degli attributi che trovano
                              corrispondenza con quelli della classe (Default = NULL)
                          

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_importFromFile(C_CLASS *pCls, C_STRING &fileName,
                       C_DBCONNECTION *pConn, C_STRING *pTable, C_STRING *pKeyAttr)
{
   FILE *pFile;
   long posiz;
   int  result = GS_BAD;
   bool Unicode;

   // Verifico l'abilitazione dell'utente;
   if (gsc_check_op(opImportEntity) == GS_BAD) return GS_BAD;

   // Apro il file in lettura
   if ((pFile = gsc_open_profile(fileName, READONLY, MORETESTS, &Unicode)) == NULL) return GS_BAD;

   // Cerco il profilo BLOCK, se non lo trovo cerco LINE, se non lo 
   // trovo cerco TEXT, se non trovo neanche quest' ultimo il file non va bene.
   if (gsc_find_sez(pFile, _T("BLOCK"), 5, &posiz, Unicode) == GS_GOOD)
   {
      if (pCls->get_category() == CAT_SPAGHETTI ||
          pCls->get_type() == TYPE_NODE)
         result = gsc_ImportBlocksFromFile(pFile, Unicode, pCls, pConn, pTable, pKeyAttr);
      else
         GS_ERR_COD = eGSInvClassType;
   }
   else if (gsc_find_sez(pFile, _T("LINE"), 4, &posiz) == GS_GOOD)
   {
      if (pCls->get_category() == CAT_SPAGHETTI ||
          pCls->get_type() == TYPE_POLYLINE)
         result = gsc_ImportPlinesFromFile(pFile, Unicode, pCls, pConn, pTable, pKeyAttr);
      else
         GS_ERR_COD = eGSInvClassType;
   }
   else if (gsc_find_sez(pFile, _T("TEXT"), 4, &posiz) == GS_GOOD)
   {
      if (pCls->get_category() == CAT_SPAGHETTI ||
          pCls->get_type() == TYPE_TEXT)
         result = gsc_ImportTextsFromFile(pFile, Unicode, pCls, pConn, pTable, pKeyAttr);
      else
         GS_ERR_COD = eGSInvClassType;
   }
   else
      GS_ERR_COD = eGSSezNotFound;

   if (gsc_fclose(pFile) == GS_BAD) return GS_BAD;

   return result;
}


/*******************************************************/
/*.doc int gsc_importFromGraph                         */
/*+                                                                   
  Funzione base che importa degli oggetti in GEOsim.
  Parametri:
  ads_name    SelSet         Gruppo di selezione degli oggetti da importare;
  C_CLASS    *pCls           Puntatore alla classe di destinazione;
  int         ObjGEOsimLoad  Flag per importare oggetti di GEOsim;
  long BitForChangeToNewFAS; Opzionale, Flag a bit per indicare quali caratteristiche
                             grafiche cambiare
  C_FAS *pNewFas;            Opzionale, i valori delle caratteristiche grafiche da cambiare 
  C_STR_LIST  *pLPNList      Lista nomi LPT esterni a GEOsim che possono essere
                             utilizzati per la lettura dei dati da DB esterno
  int         RemoveLPN      Flag per cancellare il LPN dall' oggetto;
  C_STR_LIST *pODTableList;
  int        RemoveODData;
  bool       ReadAttrBlk;    Flag di importazione; se = true e se non è stato scelto
                             di leggere i valori degli attributi da link a db esterno
                             vengono letti eventuali valori degli attributi dei blocchi 
                             che si stanno importando (default = false).

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_importFromGraph(ads_name SelSet, C_CLASS *pCls, int ObjGEOsimLoad,
                        long BitForChangeToNewFAS, C_FAS *pNewFas,
                        C_STR_LIST *pLPNList, int RemoveLPN, 
                        C_STR_LIST *pODTableList, int RemoveODData, bool ReadAttrBlk)
{
   _RecordsetPtr pInsRs;
   C_SELSET      SelectionSet, Considered, ssGraph, ssDA;
   C_CLASS       *pObjClass = NULL;
   C_STRING      nameLPN, ODTable, BlockName;
   C_RB_LIST     RbToIns, RbDefault, RbReaded;
   C_EED         eed;
   ads_name      ent;
   long          index = 0, numObj = 0, notLoaded = 0, Loaded = 0, tmp = 0;
   long          OBJloaded = 0, OBJnotloaded = 0, a = 0, b = 0, OBJlocked = 0; 
   int           result, flagLocked;
   CAseLinkSel   ls(GEOsimAppl::ASE);
   C_FAS         *pFas = (pNewFas) ? pNewFas : pCls->ptr_fas();

   // Verifico l'abilitazione dell'utente;
   if (gsc_check_op(opImportEntity) == GS_BAD) return GS_BAD;

   // Per comodità mi metto il gruppo di selezione passato come parametro
   // in un oggetto della classe C_SELSET.
   SelectionSet.add_selset(SelSet);

   // se non esistono i layer della classe li creo prima di gsc_startTransaction sennò autocad dà problemi
   if (BitForChangeToNewFAS & GSLayerSetting)
      if (gsc_crea_layer(pFas->layer) == GS_BAD) return GS_BAD;

   if (pCls->get_type() == TYPE_SURFACE &&
       BitForChangeToNewFAS & GSHatchLayerSetting)
      if (gsc_crea_layer(pFas->hatch_layer) == GS_BAD) return GS_BAD;
   
   // creo coumunque il layer perchè il blocco degli attributi potrebbe essere inserito nuovo
   if (BitForChangeToNewFAS & GSDABlockLayerSetting)
   {
      if (gsc_crea_layer(pFas->layer) == GS_BAD) return GS_BAD;
   }
   else
      if (gsc_crea_layer(pCls->ptr_fas()->layer) == GS_BAD) return GS_BAD;

   // Start Undo
   if (gsc_startTransaction() == GS_BAD) return GS_BAD;
   
   // Eseguo una prima scrematura degli oggetti che posso eliminare subito.
   if (gsc_RemoveInvalidObject(SelectionSet, pCls, ObjGEOsimLoad) == GS_BAD) return GS_BAD;
      
   // Per prima cosa, se la classe di destinazione ha una attrib_list
   // mi memorizzo la scheda di default, per passarla alla funzione di import,
   // e mi ricavo già il record set per l'inserimento nella classe di destinazione
   if (pCls->ptr_attrib_list() != NULL)
   {
      if (pCls->ptr_attrib_list()->get_StaticDefValues(RbDefault) == GS_BAD) return GS_BAD;
      if (pCls->prepareInsRs(pInsRs, TEMP) == GS_BAD) return GS_BAD;
   }  
   // Poi, se la lista dei LPN contiene almeno un LPN richiamo la funzione 
   // di verifica ed eventuale attivazione recordset.
   if (pLPNList && pLPNList->get_count() > 0)
      if (analyzingLPN(pLPNList) == GS_BAD) return GS_BAD;

   Considered.clear();

   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(344)); // "Importazione entità"
   StatusBarProgressMeter.Init(SelectionSet.length());

   index = 0;
   result = GS_GOOD;
   while (SelectionSet.entname(index++, ent) == GS_GOOD)
   {
      StatusBarProgressMeter.Set(index);

      // Se è un oggetto che ho già preso in considerazione lo salto
      if (Considered.is_member(ent) == GS_GOOD) continue;
      
      // Se è un blocco DA lo salto
      if (gsc_isAcMapAnnotation(ent) == GS_GOOD) continue;
      
      // Svuoto i due gruppi (Principali e Schede)
      ssGraph.clear();
      ssDA.clear();
      nameLPN.clear();
      ODTable.clear();
   
      pObjClass = NULL;

      // Devo verificare se l' oggetto preso in considerazione è linkato con
      // un record di un DB esterno      
      if (eed.load(ent) == GS_BAD)
      {
         if (pCls->ptr_attrib_list() != NULL)
         {
            if (pLPNList && pLPNList->get_count() > 0)
            {
               if (objectIsLinked(ent, pLPNList, nameLPN) == GS_BAD)
                  // Verifico se si devono leggere dati OD
                  if (pODTableList && pODTableList->get_count() > 0)
                     gsc_objectIsODLinked(ent, pODTableList, ODTable);
            }
            else // Verifico se si devono leggere dati OD
               if (pODTableList && pODTableList->get_count() > 0)
                  gsc_objectIsODLinked(ent, pODTableList, ODTable);
         }
      }
      else
         if ((pObjClass = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), eed.cls, eed.sub)) == NULL)
            { result = GS_BAD; break; }

      // Se pObjClass è NULL allora ent non è di GEOsim.
      // Se pObjClass è diverso da NULL allora vado a vedere se ha la
      // p_attrib_list che vuol dire che è da interrogare.
      // Devo tenere conto che in Colvalues ho già memorizzato la scheda di Default
      // Della classe di destinazione, se l' oggetto è interrogabile allora 
      // per l' importazione devo utilizzare l' RB_list che ricvavo dall' interrogazione.
      RbToIns.remove_all();

      if (pCls->ptr_attrib_list() != NULL) // Se classe con collegamento a DB
      { 
         // Ricavo da ent le entità legate (Principali e Schede)
         if (gsc_load_Princ_and_DABlk(SelectionSet, ent, nameLPN, ssGraph, ssDA, 
                                      &pObjClass, &ls) == GS_BAD)
            { result = GS_BAD; break; }

         Considered.add_selset(ssDA);

         if (pObjClass) // entità di GEOsim
         {
            if (pObjClass->ptr_attrib_list())
            {
               if (pObjClass->query_data(ent, RbReaded) == GS_BAD ||
                   RbToRbOnlyFieldCompatible(pCls, RbReaded, RbToIns) == GS_BAD)
                  RbDefault.copy(RbToIns);
            }
            else // Sono nel caso degli spaghetti
               RbDefault.copy(RbToIns);
         }
         else // entità non appartenente a GEOsim
         {
            if ((nameLPN.len() > 0) && (is_enableLPN(nameLPN) == GS_GOOD))
            {
               // Interrogo l' oggetto di Acad, se va male l' interrogazione,
               // oppure va bene ma fallisce la funzione che ricava i valori 
               // del resbuf compatibili mi ripristino la scheda di default
               if (gsc_queryAcadObject(ent, nameLPN, RbReaded, RemoveLPN, &ls) == GS_BAD ||
                   RbToRbOnlyFieldCompatible(pCls, RbReaded, RbToIns) == GS_BAD)
                  RbDefault.copy(RbToIns);
            }
            else
            {
               C_RB_LIST ColValues;

               RbDefault.copy(RbToIns);

               // Verifico se l'oggetto è colegato a tabelle OD
               if (ODTable.len() > 0)
               {
                  // Leggo i valori OD
                  if (gsc_getValuesFromODTable(ent, ODTable, ColValues) == GS_GOOD)
                     if (RemoveODData == GS_GOOD) // se richiesto cancello il dato OD apppena letto
                        gsc_delID2ODTable(ent, ODTable);
               }
               else
               {
                  // Se si vogliono leggere gli attributi dei blocchi e 
                  // si stanno importando dei blocchi verifico se 
                  // questi hanno degli attributi
                  if (ReadAttrBlk && gsc_isblock(ent) == GS_GOOD && 
                     pCls->get_category() == CAT_SIMPLEX && pCls->get_type() == TYPE_NODE)
                  {
                     C_ATTRIB_BLOCK AttrBlk(ent);
                     
                     AttrBlk.get_values_attrib(ColValues);
                  }
               }

               // Se sono stati letti dei valori li sostituisco a quelli di default
               if (ColValues.GetCount() > 0)
               {
                  int      i = 0;
                  presbuf  pRbAttr, pRbAttrName, pRbAttrValue;
                  C_ATTRIB *pAttrib;

                  while ((pRbAttr = ColValues.nth(i++)))
                  {
                     pRbAttrName  = gsc_nth(0, pRbAttr);
                     pRbAttrValue = gsc_nth(1, pRbAttr);
                     pAttrib = (C_ATTRIB *) pCls->ptr_attrib_list()->search_name(pRbAttrName->resval.rstring, FALSE);

                     if (pAttrib && pAttrib->CheckValue(pRbAttrValue) == GS_GOOD)
                        RbToIns.CdrAssocSubst(pAttrib->name.get_name(), // nome attributo
                                             pRbAttrValue);             // valore attributo
                  }
               }
            }
         }
      }
      else
         ssGraph.add(ent);

      Considered.add_selset(ssGraph);

      flagLocked = GS_BAD;
      numObj = a + b;
      tmp = notLoaded = Loaded = 0;

      // Richiamo la funzione di importazione
      if ((tmp = pCls->import_data(ssGraph, ssDA, ObjGEOsimLoad,
                                   BitForChangeToNewFAS, pFas,
                                   RbToIns, pObjClass, pInsRs, &flagLocked)) == GS_BAD) 
      {
         notLoaded = numObj - tmp;
         if (flagLocked == GS_GOOD)
            OBJlocked++;
      }
      else
      {
         C_SINTH_SEC_TAB_LIST SrcSinthSecList, DstSinthSecList;
         long                 EntKey;

         if (pObjClass)
            // leggo la lista delle tabelle secondarie
            pObjClass->get_pPrj()->getSinthClsSecondaryTabList(pObjClass->ptr_id()->code, pObjClass->ptr_id()->sub_code,
                                                               SrcSinthSecList);
         
         pCls->get_pPrj()->getSinthClsSecondaryTabList(pCls->ptr_id()->code, pCls->ptr_id()->sub_code,
                                                       DstSinthSecList);
         DstSinthSecList.FilterOnType(GSInternalSecondaryTable);

         // Se la classe sorgente ha delle tabelle secondarie e
         // la classe destinazione ha delle tabelle secondarie interne
         if (SrcSinthSecList.get_count() > 0 && DstSinthSecList.get_count() > 0)
         {
            C_SINTH_SEC_TAB *pSrcSinthSec, *pDstSinthSec;

            // Per ogni secondaria interna di destinazione
            pDstSinthSec = (C_SINTH_SEC_TAB *) DstSinthSecList.get_head();
            while (pDstSinthSec)
            {
               // Se esiste una secondaria sorgente con lo stesso nome
               if ((pSrcSinthSec = (C_SINTH_SEC_TAB *) SrcSinthSecList.search_name(pDstSinthSec->get_name(), FALSE)))
               {
                  C_SECONDARY *pSrcSec, *pDstSec;
                  presbuf     pSrckeyPri, pSrcColValues;
                  C_RB_LIST   SrcSecColValues, SrcSecColValuesList, DstSecColValues;
      
                  if (!(pSrcSec = (C_SECONDARY *) pObjClass->find_sec(pSrcSinthSec->get_key())) ||
                      !(pDstSec = (C_SECONDARY *) pCls->find_sec(pSrcSinthSec->get_key())))
                     { pDstSinthSec = (C_SINTH_SEC_TAB *) DstSinthSecList.get_next(); continue; }
                  // Cerco il valore della chiave dell'entità sorgente
                  if (!(pSrckeyPri = RbReaded.CdrAssoc(pSrcSec->ptr_info()->key_pri.get_name())))
                     { pDstSinthSec = (C_SINTH_SEC_TAB *) DstSinthSecList.get_next(); continue; }
                  // leggo tutte le schede secondarie dell'entità sorgente
                  if (pSrcSec->query_AllData(pSrckeyPri, SrcSecColValuesList) == GS_BAD)
                     { pDstSinthSec = (C_SINTH_SEC_TAB *) DstSinthSecList.get_next(); continue; }
                  // Cerco il valore della chiave dell'entità destinazione
                  if (gsc_rb2Lng(RbToIns.CdrAssoc(pCls->ptr_info()->key_attrib.get_name()), &EntKey) == GS_BAD)
                     { pDstSinthSec = (C_SINTH_SEC_TAB *) DstSinthSecList.get_next(); continue; }

                  SrcSecColValuesList.remove_head(); // cancello la prima tonda
                  SrcSecColValuesList.remove_tail(); // cancello l'ultima tonda

                  pSrcColValues = SrcSecColValuesList.get_head();
                  while (pSrcColValues)
                  {
                     SrcSecColValues << gsc_listcopy(pSrcColValues);

                     if (RbToRbOnlyFieldCompatible(pDstSec, SrcSecColValues, DstSecColValues) == GS_GOOD)
                        pDstSec->ins_data(EntKey, DstSecColValues, NULL, FALSE);

                     if ((pSrcColValues = gsc_scorri(pSrcColValues))) // vado alla chiusa tonda successiva
                        pSrcColValues = pSrcColValues->rbnext;
                  }
               }

               pDstSinthSec = (C_SINTH_SEC_TAB *) DstSinthSecList.get_next();
            }
         }

         Loaded = tmp;
      }

      OBJloaded = OBJloaded + Loaded;
      OBJnotloaded = OBJnotloaded + notLoaded;
   }
   StatusBarProgressMeter.End(gsc_msg(70), --index); // "%ld oggetti grafici elaborati."

   // Stampo se ho trovato oggetti bloccati da un' altro utente.
   if (OBJlocked > 0)
      // "\nIl comando non è stato eseguito su %d oggetto(i) perchè bloccato(i) da un'altro utente."
      acutPrintf(gsc_msg(76), OBJlocked);

   // "\nIl comando è stato eseguito correttamente su %ld oggetti grafici."
   acutPrintf(gsc_msg(562), OBJloaded);

   if (result == GS_GOOD)
   {
      gsc_endTransaction();
      ade_errclear();
   }
   else gsc_abortTransaction();

   return result;
}


/*******************************************************/
/*.doc int gsc_isValidExternalLPN()                    */
/*+                                                                   
   Funzione base per verificare ed inizializzare un LPN.
   Parametri:
   const TCHAR  *NameLPN        Nome del LPN da inizializzare;
   CAseLinkPath &InternalLPN     

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_isValidExternalLPN(const TCHAR *NameLPN, CAseLinkPath &InternalLPN)
{
   int Status;

   if (InternalLPN.initName(NameLPN) != kAsiGood) // non è utilizzabile
      return GS_BAD;
   Status = InternalLPN.getStatus();
   if (!(Status & ASE_DO_REG) ||   // The DO is registered (has at least one Link Path).
       !(Status & ASE_DO_EXIST) || // The DO exists in the SQL database (can be 
                                   // assigned if the Environment is connected in ASE).
       !(Status & ASE_DO_CON))     // The DO belongs to the connected environment
   {
      // provo a collegare il LPN con l'ambiente ASE se necessario
      if (gsc_LPNConnectAse(InternalLPN, (TCHAR *) GEOsimAppl::GS_USER.log,
                            (TCHAR *) GEOsimAppl::GS_USER.pwd) == GS_BAD)
         return GS_BAD;
      // Check whether the table already exists
      if (!(InternalLPN.getStatus() & ASE_DO_EXIST))
         { GS_ERR_COD = eGSTableNotExisting; return GS_BAD; }  
   }

   return GS_GOOD;
}


/*******************************************************/
/*.doc int gsc_queryAcadObject()                       */
/*+                                                                   
   Funzione base interroga un oggetto di AutoCAD.
   Parametri:
   ads_name      ent         Oggetto da interrogare;
   CAseLinkPath &nameLPN         LinkPathName;
   C_RB_LIST    &ColValues   Rb_list risultato della query;
   int          RemoveLPN
   CAseLinkSel  *pLS;          (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_queryAcadObject(ads_name ent, C_STRING &nameLPN, C_RB_LIST &ColValues,
                        int RemoveLPN, CAseLinkSel *pLS)
{
   CAsiRow      *pKeyCols = NULL, *pRowBuffer = NULL;
   CAsiUcStr    AsiUcrStr;
   TCHAR        FldName[MAX_LEN_FIELDNAME];
   C_STRING     CorrectedFldName;
   CAseLinkPath InternalLPN(GEOsimAppl::ASE);
   CAseLink     Link(GEOsimAppl::ASE);
   LinkID       ID;
   CAsiSession  *pTempSession = NULL;
   CAsiCsr      *pCsr = NULL;
   CAseLinkSel  *pInternalLS;

   if (InternalLPN.initName(nameLPN.get_name()) != kAsiGood)
      { GS_ERR_COD = eGSInvalidLPN; return GS_BAD; }

   if (InternalLPN.getKeyDsc(&pKeyCols) != kAsiGood)
      { GS_ERR_COD = eGSInvalidLPN; return GS_BAD; }
   if (InternalLPN.getTableDsc(&pRowBuffer) != kAsiGood)
      { GS_ERR_COD = eGSInvalidLPN; return GS_BAD; }

   if (pLS)
      pInternalLS = pLS;
   else 
      if ((pInternalLS = new CAseLinkSel(GEOsimAppl::ASE)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

   if (pInternalLS->initLpEnt(nameLPN.get_name(), ent) != kAsiGood)
      { if (!pLS) delete pInternalLS; GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
   if ((ID = pInternalLS->getId(0)) == LINKID_NULL)  // primo link del link set
      { if (!pLS) delete pInternalLS; GS_ERR_COD = eGSInvalidLink; return GS_BAD; }

   // Inizializzo il Link Descriptor attraverso ent
   if (Link.initId(ID) != kAsiGood)
      { if (!pLS) delete pInternalLS; GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
   if (Link.getKey(pKeyCols) != kAsiGood)
      { if (!pLS) delete pInternalLS; GS_ERR_COD = eGSInvalidLink; return GS_BAD; }

   // commentato perchè dopo circa 480 volte si inchioda (baco MAP)
//   if (InternalLPN.getRow(pKeyCols, pRowBuffer) != kAsiGood)
   {
//    #if defined(GSDEBUG) // se versione per debugging
//       gsc_printASEErr(&InternalLPN);
//    #endif

      // se fallisce questa funzione (se i tipi tra LPN e DB non corrispondono 
      // la funzione fa casino) provo a fare la select SQL (valido solo per campo
      // chiave numerico o carattere)
      C_STRING statement;

      statement = _T("SELECT * FROM ");

      TCHAR Table[256];
      InternalLPN.getName(Table, 255, kAseSQLCode);
      statement += Table;
      statement += _T(" WHERE ");

      AsiUcrStr = GS_EMPTYSTR;
      (*pKeyCols)[0].Name().ToString(&AsiUcrStr); // fa strcat
      AsiUcrStr.getToChar(FldName, MAX_LEN_FIELDNAME - 1);

      statement += FldName;

      if ((*pKeyCols)[0].getData()->Type() == kAsiCharVar ||
         (*pKeyCols)[0].getData()->Type() == kAsiChar)
      {
         TCHAR    Value[256];
         C_STRING dummy;

         statement += _T("='");
         if ((*pKeyCols)[0].getData()->getValue(Value, 255) == kAsiBad)
            { if (!pLS) delete pInternalLS; GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
         dummy = Value;
         dummy.strtran(_T("'"), _T("''"));
         statement += dummy.get_name();
         statement += _T("'");
      }
      else
      {
         real Value;

         statement += _T("=");
         if ((*pKeyCols)[0].getData()->getValue(&Value) == kAsiBad)
            { if (!pLS) delete pInternalLS; GS_ERR_COD = eGSInvalidLink; return GS_BAD; }
         statement += Value;
      }

      TCHAR       UDL[256];
      CAsiExecStm *pTempStm;

      InternalLPN.getName(UDL, 255, kAseEnvCode);
      if ((pTempSession = gsc_ASICreateSession(UDL)) == NULL)
         { if (!pLS) delete pInternalLS; return GS_BAD; }
      if (gsc_ASIPrepareSql(pTempSession, statement.get_name(), UDL,
                            &pTempStm, &pCsr) == GS_BAD)
         { if (!pLS) delete pInternalLS; return GS_BAD; }
      if (pCsr->Open() != kAsiGood) 
      {
         if (!pLS) delete pInternalLS;
         delete pCsr;
         gsc_ASITermStm(&pTempStm);
         gsc_ASITermSession(&pTempSession);
         GS_ERR_COD = eGSInvalidLink;
         return GS_BAD;
      }

      if (pCsr->Fetch() != kAsiGood ||
          (pRowBuffer = pCsr->getCurrentRow()) == NULL)
      {
         if (!pLS) delete pInternalLS;
         pCsr->Close();
         delete pCsr;
         gsc_ASITermStm(&pTempStm);
         gsc_ASITermSession(&pTempSession);
         GS_ERR_COD = eGSInvalidLink;
         return GS_BAD;
      }
      gsc_ASITermStm(&pTempStm);
   } 

   ColValues << acutBuildList(RTLB, 0);
   for (int i = 0; i < pRowBuffer->ColNum(); i++)
   {
      AsiUcrStr = GS_EMPTYSTR;
      (*pRowBuffer)[i].Name().ToString(&AsiUcrStr); // fa strcat
      AsiUcrStr.getToChar(FldName, MAX_LEN_FIELDNAME - 1);

      CorrectedFldName = FldName;
      if (CorrectedFldName.get_chr(0) == _T('"')) // tolgo il primo e l'ultimo "       
         CorrectedFldName.removePrefixSuffix(_T("\""), _T("\""));

      ColValues += acutBuildList(RTLB, RTSTR, CorrectedFldName.get_name(), 0);

      if ((*pRowBuffer)[i].getData()->isNull() == kAsiGood)
         ColValues += acutBuildList(RTNIL, RTLE, 0);
      else
         switch ((*pRowBuffer)[i].getData()->Type())
         {
            case kAsiDateTime:
            case kAsiInterval:
            {
               DBTIMESTAMP  Value;
               CString      dummyStr;

               if ((*pRowBuffer)[i].getData()->getValue(&Value) == kAsiBad)
               {
                  DBDATE Value;

                  if ((*pRowBuffer)[i].getData()->getValue(&Value) == kAsiBad)
                  {
                     DBTIME Value;

                     if ((*pRowBuffer)[i].getData()->getValue(&Value) == kAsiBad)
                     {
                        if (!pLS) delete pInternalLS;
                        if (pCsr) { pCsr->Close(); delete pCsr; }
                        if (pTempSession) gsc_ASITermSession(&pTempSession);
                        GS_ERR_COD = eGSInvalidLink;
                        return GS_BAD;
                     }
      
                     COleDateTime dummyOleDateTime(0, 0, 0, Value.hour, Value.minute, Value.second);
                     // "%c"  = Date and time representation for current locale
                     dummyStr = dummyOleDateTime.Format(_T("%c"));
                  }
                  else
                  {
                     COleDateTime dummyOleDateTime(Value.year, Value.month, Value.day, 0, 0, 0);
                     // "%c"  = Date and time representation for current locale
                     dummyStr = dummyOleDateTime.Format(_T("%c"));
                  }
               }
               else
               {
                  COleDateTime dummyOleDateTime(Value.year, Value.month, Value.day,
                                                Value.hour, Value.minute, Value.second);
                  // "%c"  = Date and time representation for current locale
                  dummyStr = dummyOleDateTime.Format(_T("%c"));
               }
               ColValues += acutBuildList(RTSTR, dummyStr, RTLE, 0);
               break;
            }
            case kAsiChar:
            case kAsiCharVar:
            case kAsiBit:
            case kAsiBitVar:
            {
               CAsiUcStr Buffer;

               if ((*pRowBuffer)[i].getData()->getValue(&Buffer) == kAsiBad)
               {
                  if (!pLS) delete pInternalLS;
                  if (pCsr) { pCsr->Close(); delete pCsr; }
                  if (pTempSession) gsc_ASITermSession(&pTempSession);
                  GS_ERR_COD = eGSInvalidLink;
                  return GS_BAD;
               }
               ColValues += acutBuildList(RTSTR, Buffer.UcStr(), RTLE, 0);
               break;
            }
            case kAsiNumeric:
            case kAsiDecimal:
            case kAsiFloat:
            case kAsiReal:
            case kAsiDouble:
            {
               real Value;
               if ((*pRowBuffer)[i].getData()->getValue(&Value) == kAsiBad)
               {
                  if (!pLS) delete pInternalLS;
                  if (pCsr) { pCsr->Close(); delete pCsr; }
                  if (pTempSession) gsc_ASITermSession(&pTempSession);
                  GS_ERR_COD = eGSInvalidLink;
                  return GS_BAD;
               }
               ColValues += acutBuildList(RTREAL, Value, RTLE, 0);
               break;
            }
            case kAsiInteger:
            {
               integer Value;
               if ((*pRowBuffer)[i].getData()->getValue(&Value) == kAsiBad)
               {
                  if (!pLS) delete pInternalLS;
                  if (pCsr) { pCsr->Close(); delete pCsr; }
                  if (pTempSession) gsc_ASITermSession(&pTempSession);
                  GS_ERR_COD = eGSInvalidLink;
                  return GS_BAD;
               }
               ColValues += acutBuildList(RTLONG, Value, RTLE, 0);
               break;
            } 
            case kAsiSmallInt:
            {
               smallint Value;
               if ((*pRowBuffer)[i].getData()->getValue(&Value) == kAsiBad)
               {
                  if (!pLS) delete pInternalLS;
                  if (pCsr) { pCsr->Close(); delete pCsr; }
                  if (pTempSession) gsc_ASITermSession(&pTempSession);
                  GS_ERR_COD = eGSInvalidLink;
                  return GS_BAD;
               }
               ColValues += acutBuildList(RTSHORT, Value, RTLE, 0);
               break;
            } 
            default:
               ColValues += acutBuildList(RTNIL, RTLE, 0);
         }
   }
   ColValues += acutBuildList(RTLE, 0);

   if (pCsr) { pCsr->Close(); delete pCsr; }
   if (pTempSession) gsc_ASITermSession(&pTempSession);

   if (RemoveLPN == GS_GOOD)
   {
      // La seguente istruzione serve a cancellare il LPN esterno dall'oggetto.
      pInternalLS->erase();
   }

   if (!pLS) delete pInternalLS;

   return GS_GOOD;
}


/*****************************************************/
/*.doc int gsc_load_Princ_and_DABlk()                */
/*+                                                                   
   Funzione che, dato un oggetto e il nome di un LPN
   ricava due gruppi di selezione, uno che rappresenta 
   le principali eventualmente legate allo stesso record 
   ed un' altro gruppo che rappresenta gli eventuali 
   blocchi DA.
  
   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*****************************************************/
int gsc_load_Princ_and_DABlk(C_SELSET &ssAll, ads_name ent, C_STRING &lpnName,
                             C_SELSET &ssGraph, C_SELSET &ssDA, C_CLASS **pCObj,
                             CAseLinkSel *pLS)
{
   C_EED     eed;
   C_SELSET  tmp, ssLeader;
   int       result;
   long      key;
   //long      index, a, b;
   //ads_name  obj;
   C_RB_LIST Descr;

   // Devo stare attento ai seguenti casi:
   // 1. caso di entità di GEOsim con DB
   // 2. caso di entità di GEOsim senza DB
   // 3. caso di entità non di GEOsim con DB
   // 4. caso di entità non di GEOsim senza DB
   if (eed.load(ent) == GS_GOOD)
   {
      do
      {
         result = GS_BAD;
         // Se è un' entità di GEOsim allora per prima cosa ricavo il 
         // il puntatore alla classe originale
         if (((*pCObj) = gsc_find_class(GS_CURRENT_WRK_SESSION->get_PrjId(), eed.cls, eed.sub)) == NULL) break;
         // Se poi non è un' entità spaghetti vado a ricavare gli oggetti linkati
         if ((*pCObj)->get_category() != CAT_SPAGHETTI)
         {
            // Ricavo il codice chiave 
            if ((*pCObj)->getKeyValue(ent, &key) == GS_BAD) break; 
            // Ricavo il gruppo di selezione degli oggetti principali
            if ((*pCObj)->get_SelSet(key, ssGraph, GRAPHICAL) == GS_BAD) break;
            // Ricavo il gruppo di selezione dei blocchi degli attributi visibili
            if ((*pCObj)->get_SelSet(key, ssDA, DA_BLOCK) == GS_BAD) break;
         }
         else
            ssGraph.add(ent);
         result = GS_GOOD;
      }
      while (0);
   }
   else // entità non di GEOsim
      if (lpnName.get_name() == NULL)
      {
         ssGraph.add(ent);
         result = GS_GOOD;
      }
      else
      {
         CAseLinkSel *pInternalLS;
         result = GS_BAD;

         do
         {
            if (pLS)
               pInternalLS = pLS;
            else 
               if ((pInternalLS = new CAseLinkSel(GEOsimAppl::ASE)) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; break; }

            if (lpnName.get_name() &&
                pInternalLS->initLpEnt(lpnName.get_name(), ent) == kAsiGood) 
            {
               LinkID ID;

               if ((ID = pInternalLS->getId(0)) != LINKID_NULL)  // primo link del link set
               {
                  CAseLink     Link(GEOsimAppl::ASE);
                  CAseLinkPath LPN(GEOsimAppl::ASE);
                  CAsiRow      *pKeyCols;
                  ads_name     dummy_ssDA, dummy_ssGraph;

                  // Inizializzo il Link Descriptor attraverso ent
                  if (Link.initId(ID) != kAsiGood) break; 
                  // Leggo il valore chiave
                  if (LPN.initName(lpnName.get_name()) != kAsiGood) break;
                  if (LPN.getKeyDsc(&pKeyCols) != kAsiGood) break;
                  if (Link.getKey(pKeyCols) != kAsiGood) break;
                  if (pInternalLS->initLpKey(lpnName.get_name(), pKeyCols, kAseEntityLink) != kAsiGood)
                     break;
                  if (pInternalLS->getEntSel(dummy_ssGraph) != kAsiGood) break;
                  ssGraph << dummy_ssGraph;
                  ssDA.clear();
                  if (pInternalLS->initLpKey(lpnName.get_name(), pKeyCols, kAseLeaderLink) == kAsiGood)
                     if (pInternalLS->getEntSel(dummy_ssDA) == kAsiGood) ssDA.add_selset(dummy_ssDA);
                  if (pInternalLS->initLpKey(lpnName.get_name(), pKeyCols, kAseDALink) == kAsiGood)
                     if (pInternalLS->getEntSel(dummy_ssDA) == kAsiGood) ssDA.add_selset(dummy_ssDA);

                  // Per le etichette dinamiche accade che gli oggetti grafici principali
                  // abbiano anche un link a etichetta e quindi compaiano sia in
                  // ssGraph che in ssDA. In questo caso elimino da ssDA tutti
                  // gli oggetti presenti anche in ssGraph.
                  ssDA.subtract(ssGraph);
               }
            }

            result = GS_GOOD;
         }
         while (0);

         if (!pLS && pInternalLS) delete pInternalLS;
      }
   
   return result;
}


/******************************************************************************/
/*.doc int analyzingLPN()                            */
/*+                                                                   
   Funzione che, dato l' elenco dei LPT che possono essere usati verifica che tutti
   possano essere utilizzabili in caso contrario scrive un messaggio di avvertimento.
   Parametri:
   C_STR_LIST *pLPNList;    Lista dei nomi dei Link Path Template

   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*****************************************************/
int analyzingLPN(C_STR_LIST *pLPNList) 
{
   C_STR        *pNameLPN = NULL;
   CAseLinkPath InternalLPN(GEOsimAppl::ASE);
   C_STRING     statement;
   
   // Se non esistono LPN la funzione restituisce GS_GOOD
   if (!pLPNList || pLPNList->get_count() <= 0) return GS_GOOD;

   pNameLPN = (C_STR *) pLPNList->get_head();
   while (pNameLPN)
   {
      // Verifico l'utilizzo del LPT
      if (gsc_isValidExternalLPN(pNameLPN->get_name(), InternalLPN) == GS_BAD)
         // "\nIl LPT %s non può essere utilizzato."
         acutPrintf(gsc_msg(856), pNameLPN->get_name()); 

      pNameLPN = (C_STR *) pLPNList->get_next();
   }
   
   return GS_GOOD;
}


/**************************************************************/
/*.doc RbToRbOnlyFieldCompatible                  <internal > */
/*+                                                                       
   Funzione per passa i valori compatibili dei campi passati 
   nel resbuf in input rispetto alla lista attributi pAttrib_list.
   Parametri: 
   C_ATTRIB_LIST *pAttrib_list;  lista attributi di riferimento;
   C_RB_LIST      &RbInput;      resbuf da controllare.
   C_RB_LIST      &RbOut;        Risultato
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/**************************************************************/
int RbToRbOnlyFieldCompatible(C_ATTRIB_LIST *pAttribList, C_RB_LIST &RbInput, C_RB_LIST &RbOut)
{
   C_ATTRIB *pAttrib;
   presbuf  p, RbFound;
   long     i = 0;
   int      result= GS_GOOD;
   
   if (!pAttribList) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // prendo la scheda di default della classe di entità
   if (RbInput.get_head() == NULL)
      return pAttribList->get_StaticDefValues(RbOut);

   pAttrib = (C_ATTRIB *) pAttribList->get_head();

   if ((RbOut << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

   while (pAttrib)
   {
      if ((RbOut += acutBuildList(RTLB, RTSTR, pAttrib->get_name(), 0)) == NULL)
         { result = GS_BAD; break; }
      // mi trovo con punt sul nome dell'attributo
      if ((RbFound = RbInput.CdrAssoc(pAttrib->get_name())) != NULL) 
      {
         if ((p = gsc_copybuf(RbFound)) == NULL) { result = GS_BAD; break; }
         
         // Prima cosa verifico se il valore del rb è corretto
         if (pAttrib->CheckValue(p) == GS_BAD)
         {
            if (p) acutRelRb(p);
            if ((p = gsc_copybuf(pAttrib->def)) == NULL)
               { result = GS_BAD; break; }
         }
      }
      else
         if ((p = gsc_copybuf(pAttrib->def)) == NULL) { result = GS_BAD; break; }

      RbOut += p;
      if ((RbOut += acutBuildList(RTLE, 0)) == NULL) { result = GS_BAD; break; }

      pAttrib = (C_ATTRIB *) pAttrib->get_next();
   }
   if (result == GS_BAD) return GS_BAD;

   if ((RbOut += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   return GS_GOOD;
}


/**************************************************************/
/*.doc RbToRbOnlyFieldCompatible                  <internal > */
/*+                                                                       
   Funzione per passa i valori compatibili dei campi passati 
   nel resbuf in input rispetto alla lista attributi pAttrib_list.
   Parametri: 
   C_CLASS   *pCls;         Puntatore a classe GEOsim
   C_RB_LIST &RbInput;      Resbuf da controllare.
   C_RB_LIST &RbOut;        Risultato
        
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/**************************************************************/
int RbToRbOnlyFieldCompatible(C_CLASS *pCls, C_RB_LIST &RbInput, C_RB_LIST &RbOut)
{
   C_ATTRIB_LIST *pAttribList = pCls->ptr_attrib_list();
   
   if (!pAttribList) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (pAttribList->init_ADOType(pCls->ptr_info()->getDBConnection(OLD)) == GS_BAD)
      return GS_BAD;

   return RbToRbOnlyFieldCompatible(pAttribList, RbInput, RbOut);
}
int RbToRbOnlyFieldCompatible(C_SECONDARY *pSec, C_RB_LIST &RbInput, C_RB_LIST &RbOut)
{
   C_ATTRIB_LIST *pAttribList = pSec->ptr_attrib_list();
   
   if (!pAttribList) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (pAttribList->init_ADOType(pSec->ptr_info()->getDBConnection(OLD)) == GS_BAD)
      return GS_BAD;

   return RbToRbOnlyFieldCompatible(pAttribList, RbInput, RbOut);
}


/****************************************************************/
/*.doc int gsc_RemoveInvalidObject()                            */
/*+                                                                   
  Funzione che elimina dal gruppo di selezione per l' importazione 
  gli oggetti che non sono compatibili graficamente, e in base 
  ai settaggi di GEOsim e quelli impostati.
  Parametri:
  C_SELSET &ss;   Gruppo di selezione da analizzare
  C_CLASS *pCls;  Classe di destinazione
  int flagEntGeo; Flag per importare oggetti di GEOsim
  
   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*****************************************************/
int gsc_RemoveInvalidObject(C_SELSET &ss, C_CLASS *pCls, int flagEntGeo)
{
   long        cont = 0, notCompat = 0, isGeo = 0, equal = 0, isGeoSimulation = 0;
   int         skippaDA = GS_BAD;
   ads_name    ent;
   C_EED       eed;
   C_SELSET    ToSubstract;
   C_CLASS    *pClsObj = NULL;
      
   // Scorro il gruppo di selezione per effettuare una prima selezione
   while (ss.entname(cont++, ent) == GS_GOOD)
   {
      if (eed.load(ent) == GS_GOOD)
      {
         // Innanzi tutto se è un oggetto di una classe simulazione lo elimino  
         if (eed.sub > 0)
            { ToSubstract.add(ent); isGeoSimulation++; continue; }
         // Se il flag di importazione di oggetti di GEOsim è disabilitato
         if (flagEntGeo == GS_BAD)
            { ToSubstract.add(ent); isGeo++; continue; }
         else
         {
            // Verifico che non siano della stessa classe            
            if ((pClsObj = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) != NULL)
            {
               if (pClsObj == pCls)
                  { ToSubstract.add(ent); equal++; continue; }
            }
            // A questo punto, se la classe di destinazione è uno 
            // spaghetti non procedo ad alcun tipo di controllo
            // sulla compatibilità dell'oggetto, al contrario nel 
            // caso di classe con tipologia unica allora lo verifico
            if (pCls->get_category() == CAT_SIMPLEX)
            {
               // Se è un blocco lo devo trattare individualmente
               if (gsc_is_DABlock(ent) == GS_GOOD)
               {
                  if (pClsObj->get_category() == CAT_SIMPLEX) 
                     continue;
                  else if (pClsObj->get_category() == CAT_SPAGHETTI) 
                     { ToSubstract.add(ent); notCompat++; continue; }
               }
               else
                  // Verifico la compatibilità grafica degli oggetti
                  if (pCls->check_graph_compatib(ent) == GS_BAD)
                  {
                     // Accetto le seguenti eccezioni:
                     // 1) points in classe semplice di tipo NODO
                     // 2) blocchi anche se con attributi che vengono scartati da check_graph_compatib in classe semplice di tipo NODO 
                     if (pCls->get_category() == CAT_SIMPLEX && pCls->get_type() == TYPE_NODE && 
                         (gsc_isPoint(ent) == GS_GOOD || gsc_isblock(ent) == GS_GOOD))
                     {}
                     else
                        { ToSubstract.add(ent); notCompat++; continue; }
                  }
            }
            else
            if (pCls->get_category() == CAT_SPAGHETTI)
            {
               C_SELSET ExplodedSS;

               // Se si vogliono importare degli oggetti non compatibili
               // in classe spaghetti provo ad esplodere gli oggetti finchè
               // si riducono in oggetti elementari (punti, linee)
               if (pCls->ptr_GphInfo()->HasCompatibleGeom(ent, true, &ExplodedSS) == false)
                  { ToSubstract.add(ent); notCompat++; continue; }
               if (ExplodedSS.length() > 0)
               {
                  ss.add_selset(ExplodedSS);
                  ToSubstract.add(ent);
               }
            }
         }
      }
      else // non era un oggetto di GEOsim
      {
         // Se è un blocco attributi di GEOsim lo escludo
         if (gsc_is_DABlock(ent) == GS_GOOD)
            { ToSubstract.add(ent); notCompat++; continue; }
         // Se è un blocco DA di AutoCAD, oppure un MTEXT (che non è compatibile).
         if (gsc_isAcMapAnnotation(ent) == GS_GOOD) continue;
         // Verifico la compatibilità grafica degli oggetti
         if (pCls->check_graph_compatib(ent) == GS_BAD)
         {
            // Accetto le seguenti eccezioni:
            // 1) points in classe semplice di tipo NODO
            // 2) blocchi anche se con attributi che vengono scartati da check_graph_compatib in classe semplice di tipo NODO 
            if (pCls->get_category() == CAT_SIMPLEX && pCls->get_type() == TYPE_NODE && 
                  (gsc_isPoint(ent) == GS_GOOD || gsc_isblock(ent) == GS_GOOD))
            {}
            else
            if (pCls->get_category() == CAT_SPAGHETTI)
            {
               C_SELSET ExplodedSS;

               // Se si vogliono importare degli oggetti non compatibili
               // in classe spaghetti provo ad esplodere gli oggetti finchè
               // si riducono in oggetti elementari (punti, linee)
               if (pCls->ptr_GphInfo()->HasCompatibleGeom(ent, true, &ExplodedSS) == false)
                  { ToSubstract.add(ent); notCompat++; continue; }
               if (ExplodedSS.length() > 0)
               {
                  ss.add_selset(ExplodedSS);
                  ToSubstract.add(ent);
               }
            }
            else
               { ToSubstract.add(ent); notCompat++; continue; }
         }
         else // se si tratta di classe semplice di tipo SUPERFICE scarto i riempimenti
            if (pCls->get_category() == CAT_SIMPLEX &&
                pCls->get_type() == TYPE_SURFACE &&
                gsc_ishatch(ent) == GS_GOOD)
               { ToSubstract.add(ent); notCompat++; continue; }
      }
   }

   // Se ci sono oggetti da scartare stampo il resoconto
   if (equal > 0)
      acutPrintf(gsc_msg(854), equal); // "\nEliminato(i) %d oggetto(i) perchè appartenente alla stessa classe di GEOsim."
   if (isGeo > 0)
      acutPrintf(gsc_msg(764), isGeo); // "\nEliminato(i) %d oggetto(i) perchè appartenente a GEOsim."
   if (notCompat > 0)
      acutPrintf(gsc_msg(855), notCompat); // "\nEliminato(i) %d oggetto(i) perchè non compatibile graficamente."
   if (isGeoSimulation > 0)
      acutPrintf(gsc_msg(77), isGeoSimulation); // "\nEliminato(i) %d oggetto(i) perchè appartenente(i) ad una classe simulazione."

   // Se esiste qualche oggetto da eliminare eseguo la sottrazione
   // dei due gruppi
   if (ToSubstract.length() > 0)
      ss.subtract(ToSubstract);

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc int gsc_isAcMapAnnotation()                                           */
/*+                                                                   
  Funzione che verifica, l' oggetto passato come parametro se è un MTEXT 
  e quindi è da considerare un blocco di visualizzazione degli attributi.
  Parametri:
  ads_name obj;  Oggetto da analizzare

  Restituisce GS_GOOD se si tratta di un MTEXT altrimenti GS_BAD. 
-*/  
/*****************************************************************************/
int gsc_isAcMapAnnotation(ads_name obj)
{
   C_RB_LIST   Rb;
   presbuf     pRb = NULL;
   int         result;

   result = GS_BAD;
   do
   {
      // Ricavo le caratteristiche grafiche dell' oggetto
      if ((Rb << ads_entgetx(obj, GEOsimAppl::ASE_APP_ID_LIST.get_head())) == NULL) break;
      if ((pRb = gsc_rbsearch(0, Rb.get_head())) == NULL) break;
      if (gsc_strcmp(pRb->resval.rstring, _T("MTEXT")) == 0)
      {
         // APPLICAZIONE ASE
         if ((pRb = gsc_EEDsearch(ASE_APP_ID, Rb.get_head())) == NULL) break;

         // se esiste un 1000 non seguito da un 1004 si tratta di un label template
         while (pRb)
         {
            if (pRb->restype == 1000)
               if (!pRb->rbnext || (pRb->rbnext && pRb->rbnext->restype != 1004))
               break;
            pRb = pRb->rbnext;
         }
         if (pRb) result = GS_GOOD;
      }
   }
   while (0);

   return result;
}


/*****************************************************/
/*.doc int gs_getLPNstatusFromMemory()               */
/*+                                                                   
   Funzione che legge la configurazione utente di un LPN
   dalla variabile in memoria.
  
   Restituisce lo stato del LPN. 
-*/  
/*****************************************************/
int gs_getLPNstatusFromMemory()
{
   C_STRING    nameLPN;
   C_2STR_INT *plp = NULL;
   presbuf     arg = NULL;
   int         Status = GS_BAD;

   acedRetNil();
   arg = acedGetArgs();
   // Il primo e unico parametro è nome del LPN  
   if (arg == NULL || arg->restype != RTSTR)  
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   nameLPN.set_name(arg->resval.rstring);
   
   if ((plp = (C_2STR_INT *) LPN_USER_STATUS.search_name(nameLPN.get_name())) != NULL)
      Status = plp->get_type();

   if (Status == GS_GOOD) acedRetT();
   else acedRetNil();

   return RTNORM;
}


/*****************************************************/
/*.doc int gs_setLPNstatusOnMemory()               */
/*+                                                                   
   Funzione che setta la configurazione utente dei LPN 
   nella variabile in memoria.
  
   Restituisce GS_GOOD se si tratta di un MTEXT altrimenti GS_BAD. 
-*/  
/*****************************************************/
int gs_setLPNstatusOnMemory()
{
   C_STRING    nameLPN, statusLPN;
   C_2STR_INT *plp = NULL;
   presbuf     arg = NULL;
   int         Status;

   acedRetNil();
   arg = acedGetArgs();
   // Il primo parametro è nome del LPN  
   if (arg == NULL || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   nameLPN.set_name(arg->resval.rstring);
   
   // Il secondo parametro è lo stato del LPN  
   if ((arg = arg->rbnext) && arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   statusLPN.set_name(arg->resval.rstring);
   
   if (wcsicmp(statusLPN.get_name(), _T("Si")) == 0)
      Status =  GS_GOOD;
   else if (wcsicmp(statusLPN.get_name(), _T("No")) == 0)
      Status =  GS_BAD;

   if ((plp = (C_2STR_INT *) LPN_USER_STATUS.search_name(nameLPN.get_name())) == NULL)
   {  
      // Alloco una C_2STR_INT
      if ((plp = new C_2STR_INT) == NULL) { GS_ERR_COD = eGSOutOfMem; return RTERROR; }
      plp->set_name(nameLPN.get_name());
      plp->set_type(Status);
      LPN_USER_STATUS.add_tail(plp);       
   }
   else
      plp->set_type(Status);

   acedRetT();

   return RTNORM;
}


/*****************************************************/
/*.doc int gs_setLPNstatusOnMemory()               */
/*+                                                                   
   Funzione che va a verificare nella lista in memoria
   se il nome del LPN passato come parametro è da 
   considerare per la lettura di DB esterni.
  
   Restituisce GS_GOOD se il LPN è abilitato
   altrimenti GS_BAD. 
-*/  
/*****************************************************/
int is_enableLPN(C_STRING &lpName)
{
   C_2STR_INT *plp = NULL;
   int         res = GS_BAD;
    
   if ((plp = (C_2STR_INT *) LPN_USER_STATUS.search_name(lpName.get_name())) != NULL)
      res = plp->get_type();

   return res;
}






/*
   Da ricordare !!!!!!!!

   
   La seguente funzione va richiamata all' inizio della procedura di import.  
      CAseLinkPath InternalLPN(GEOsimAppl::ASE);

   ---------------------------------------------------------
      
   GEOsimAppl::REFUSED_SS.clear();
   GEOsimAppl::REFUSED_SS.add();

   Le precedenti due funzioni servono ad evidenziare tutti gli oggetti scartati
   per poi poterli vedere !!!!
   Quindi ogni oggetto che non viene importato va aggiunto al gruppo REFUSED_SS.

*/


/*****************************************************/
/*.doc int gs_exportClass                            */
/*+                                                                   
   Funzione LISP che esporta in una cartella indicata 
   tutti i dati di una classe in modo tale che sia possibile 
   importarla in un'altra installazione di GEOsim.
   Parametri:
   (<prj><cls><percorso cartella export>)
  
   Restituisce T in caso di successo altrimenti nil.
-*/  
/*****************************************************/
/*
int gs_exportClass(void)
{
   presbuf arg = acedGetArgs();
   int     prj, cls;
   C_CLASS *pCls;

   acedRetNil();

   // Legge nella lista dei parametri: codice progetto e classe
   if (arg_to_prj_cls_sub(&arg, &prj, &cls) == GS_BAD) return RTERROR;
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((pCls = gsc_find_class(prj, cls)) == NULL) return RTERROR;

   if (pCls->ExportToDir(arg->resval.rstring) == GS_BAD) return RTERROR;

   acedRetT();
   
   return RTNORM;
}
*/


/*****************************************************/
/*.doc int C_SIMPLEX::ExportToDir                    */
/*+                                                                   
   Funzione che esporta in una cartella indicata 
   tutti i dati di una classe in modo tale che sia possibile 
   importarla in un'altra installazione di GEOsim.
   Parametri:
   const TCHAR *ExportDir; percorso cartella export

   Restituisce T in caso di successo altrimenti nil.
-*/  
/*****************************************************/
/*
int C_SIMPLEX::ExportToDir(const TCHAR *ExportDir)
{
   // Se non esiste creo la cartella di esportazione

   // Scrivo tutte le caratteristiche della classe in un file testo
   // usando la funzione di paolo (genera un file .acc "Automatic Creation Class")
   
   /////////////////
   // DATI DELLA FAS
   /////////////////

   // Copio i files DWG nella cartella
   
   // Se ci sono tematismi utente li copio nella cartella
   
   // Se ci sono dei blocchi di riferimento
   // Se esiste un file DWG contenente il blocco di riferimento lo copio nella cartella

   // Se ci sono delle funzioni di calcolo grafico
   // Se esiste un file GEOlisp contenente la funzione di calcolo grafico lo copio nella cartella
   // altrimenti se esiste GSL.GSL del progetto e dell'installazione server li copio nella cartella
   // infatti la funzione potrebbe essere stata definita in quei files)

   /////////////////
   // DATI DEL DB
   /////////////////
   
   // Creo un file MDB e inserisco i dati di database nel formato ACCESS
   // indipendentemente dal formato originale dei dati.
   
   // Se ci sono delle funzioni di validità
   // Se esiste un file GEOlisp contenente la funzione di validità lo copio nella cartella
   // altrimenti se esiste GSL.GSL del progetto e dell'installazione server li copio nella cartella
   // infatti la funzione potrebbe essere stata definita in quei files)

   // Se ci sono delle funzioni di calcolo
   // Se esiste un file GEOlisp contenente la funzione di calcolo lo copio nella cartella
   // altrimenti se esiste GSL.GSL del progetto e dell'installazione server li copio nella cartella
   // infatti la funzione potrebbe essere stata definita in quei files)

   ////////////////////////
   // DATI DELLE SECONDARIE
   ////////////////////////

   // per ogni secondaria 
   // Creo un file MDB e inserisco i dati e i link di database nel formato ACCESS
   // indipendentemente dal formato originale dei dati.
   
   // Se ci sono delle funzioni di validità
   // Se esiste un file GEOlisp contenente la funzione di validità lo copio nella cartella
   // altrimenti se esiste GSL.GSL del progetto e dell'installazione server li copio nella cartella
   // infatti la funzione potrebbe essere stata definita in quei files)

   // Se ci sono delle funzioni di calcolo
   // Se esiste un file GEOlisp contenente la funzione di calcolo lo copio nella cartella
   // altrimenti se esiste GSL.GSL del progetto e dell'installazione server li copio nella cartella
   // infatti la funzione potrebbe essere stata definita in quei files)

}
*/


/*****************************************************/
/*.doc int gs_importClass                            */
/*+                                                                   
   Funzione LISP che importa da una cartella indicata 
   tutti i dati di una classe che sarà creata nell'installazione corrente
   di GEOsim.
   Parametri:
   (<percorso cartella import> <prj> (<UDLConnStr> <property list>) <dir dwg>)
  
   Restituisce codice classe in caso di successo altrimenti restituisce NIL.    
/*****************************************************/
/*
int gs_importClass(void)
{
   presbuf        arg = acedGetArgs();
   int            prj;
   C_CLASS        *pCls;
   TCHAR          *dir;
   C_DBCONNECTION *pConn;

   acedRetNil();

   // direttorio di import
   if (!arg || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   dir = arg->resval.rstring;

   // codice progetto
   if (!(arg = arg->rbnext) || arg->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   prj = arg->resval.rint;

   // connessione OLE-DB (opzionale)
   if (!(arg = arg->rbnext) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype != RTNIL)
      // Legge nella lista dei parametri i riferimenti al DB
      if ((pConn = gsc_getConnectionFromLisp(&arg)) == NULL) return RTERROR;

   // direttorio dei disegni
   if (!(arg = arg->rbnext) || arg->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if ((pCls = gsc_importClassFromDir(dir, prj, pConn, arg->resval.rstring)) == GS_BAD) return RTERROR;

   acedRetInt(pCls->ptr_id()->code);
   
   return RTNORM;
}
*/


/*****************************************************/
/*.doc C_CLASS* gsc_importClassFromDir               */
/*+                                                                   
   Funzione che importa da una cartella indicata 
   tutti i dati di una classe che sarà creata nell'installazione corrente
   di GEOsim.
   Parametri:
   const TCHAR    *ImportDir;  Percorso cartella import
   int            prj;         Codice progetto in cui creare la classe
   C_DBCONNECTION *pConn;      Connessione OLE-DB da usare per creare la classe
   const TCHAR    *DirDwg;     Percorso cartella dei disegni della classe
  
   Restituisce il puntatore alla classe creata in caso di successo 
   altrimenti restituisce NIL.
-*/  
/*****************************************************/
/*
C_CLASS* gsc_importClassFromDir(const TCHAR *ImportDir, int prj, 
                                C_DBCONNECTION *pConn, const TCHAR *DirDwg)
{
   C_CLASS *pCls;

   // Creo la classe leggendo tutte le caratteristiche da un file testo
   if ((pCls = ...) == NULL) return GS_BAD;

   /////////////////
   // DATI DELLA FAS
   /////////////////

   // Copio i files DWG nella cartella DirDwg rinominandoli
    
   /////////////////
   // DATI DEL DB
   /////////////////

   // Copio i dati di database dal formato ACCESS nella tabella della classe

   ////////////////////////
   // DATI DELLE SECONDARIE
   ////////////////////////

   // per ogni secondaria 

   // Copio i dati e i link di database dal formato ACCESS nella tabella secondaria della classe
}
*/


/*******************************************************/
/*.doc gsc_prepareReadExtDbImportFile                  */
/*+                                                                   
  Funzione di ausilio all'importazione da file 
  che prepara il comando di lettura dei campi di una tabella
  esterna in fase di import da file.
  Parametri:
  C_DBCONNECTION *pConn;      Connessione OLE-DB
  C_STRING &TableRef;         Tabella
  C_STRING &KeyAttr;          Campo chiave
  _CommandPtr &pCmd;          Comando preparato
  _ParameterPtr &pParam;      Parametro del comando

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_prepareReadExtDbImportFile(C_DBCONNECTION *pConn, C_STRING &TableRef,
                                   C_STRING &KeyAttr, _CommandPtr &pCmd,
                                   _ParameterPtr &pParam)
{
   C_STRING     statement, dummy;
   DataTypeEnum DataType;
   long         Size;
   presbuf      pRb;
   C_RB_LIST    Struct;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");

   dummy = KeyAttr;
   if (gsc_AdjSyntax(dummy, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;
   statement += dummy;
   statement += _T("=?");

   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;

   // leggo la struttura della tabella per sapere il tipo del campo
   if ((Struct << pConn->ReadStruct(TableRef.get_name())) == NULL ||
       (pRb = Struct.CdrAssoc(KeyAttr.get_name())) == NULL)
      return GS_BAD;

   DataType = (DataTypeEnum) pRb->resval.rlong; // tipo
   Size     = pConn->ptr_DataTypeList()->search_Type(DataType, TRUE)->get_Size();

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), DataType, Size) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*******************************************************/
/*.doc gsc_fscanf_for_block                 <internal> */
/*+                                                                   
  Funzione di ausilio per importazione di blocchi da file testo.
  La funzione fa un fscanf in versione unicode o ansi con lettura 
  opzionale del ID di link a db esterno.
  Parametri:
  FILE *pFile;          Puntatore al file
  bool  Unicode;        Flag che determina se il contenuto del file è in 
                        formato UNICODE o ANSI (default = false)
  ads_point Pt;         Punto di inserimento 
  double *Scale;        Fattore di scala
  double *Rot;          Rotazione
  C_STRING *pId = NULL; Opzionale, puntatore a stringa on ID per link a DB esterno
                        (default = NULL)

  Restituisce EOF in caso di fallimento (es. fine file raggiunta)
  altrimenti valore diverso da EOF. 
-*/  
/*********************************************************/
int gsc_fscanf_for_block(FILE *pFile, bool Unicode, ads_point Pt, 
                         double *Scale, double *Rot, C_STRING *pId = NULL)
{
   int Res;

   if (Unicode)
   {
      if (pId)
      {
         TCHAR _Id[ACCESS_MAX_LEN_FIELDCHAR + 1];

         // %[^,] = leggi la stringa fino ad arrivare al carattere ,
         if ((Res = fwscanf(pFile, _T("%lf,%lf,%lf,%lf,%lf,%s"), 
                            &Pt[X], &Pt[Y], &Pt[Z], Scale, Rot, &_Id)) != 6)
            return EOF;
         if (pId) pId->set_name(_Id);
      }
      else
         if ((Res = fwscanf(pFile, _T("%lf,%lf,%lf,%lf,%lf"), 
                            &Pt[X], &Pt[Y], &Pt[Z], Scale, Rot)) != 5)
            return EOF;
   }
   else
   {
      if (pId)
      {
         char _Id[ACCESS_MAX_LEN_FIELDCHAR + 1];
         
         // %[^,] = leggi la stringa fino ad arrivare al carattere ,
         if ((Res = fscanf(pFile, "%lf,%lf,%lf,%lf,%lf,%s", 
                           &Pt[X], &Pt[Y], &Pt[Z], Scale, Rot, &_Id)) != 6)
            return EOF;
         if (pId) pId->set_name(_Id);
      }
      else
         if ((Res = fscanf(pFile, "%lf,%lf,%lf,%lf,%lf", 
                           &Pt[X], &Pt[Y], &Pt[Z], Scale, Rot)) != 5)
            return EOF;
   }

   return Res;
}


/*******************************************************/
/*.doc gsc_ImportBlocksFromFile                        */
/*+                                                                   
  Funzione di importazione di blocchi da file testo.
  Parametri:
  FILE *pFile;               Puntatore al file delle caratteristiche
  bool  Unicode;             Flag che determina se il contenuto del file è in 
                             formato UNICODE o ANSI
  C_CLASS *pClass;           Puntatore ad una classe di tipo nodo
  C_DBCONNECTION *pConn;     Opzionale, se presente indica il riferimento
                             ad un database da cui leggere gli attributi
                             (Default = NULL)
  C_STRING       *pTable;    Se pConn è valido, indica la tabella da cui
                             leggere gli attributi (Default = NULL)
  C_STRING       *pKeyAttr;  Se pConn è valido, indica l'attributo chiave
                             per leggere i valori degli attributi che trovano
                             corrispondenza con quelli della classe (Default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_ImportBlocksFromFile(FILE *pFile, bool  Unicode, C_CLASS *pClass,
                             C_DBCONNECTION *pConn, C_STRING *pTable, C_STRING *pKeyAttr)
{
   double        scale = 1.0, rotation = 0.0;
   ads_point     insPoint;
   ads_name      ent;
   C_RB_LIST     ColValues;
   C_SELSET      ss, ssDummy;
   int           flagLocked;
   _RecordsetPtr pInsRs;
   long          BitForChangeFAS;
   long                 i = 0;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(344)); // "Importa entità"
   StatusLineMsg.Init(gsc_msg(70), LITTLE_STEP); // ogni 10 "%ld oggetti grafici elaborati."

   BitForChangeFAS = pClass->what_is_graph_updateable();

   // Verifico ora la tipologia dei dati da importare e la classe di destinazione
   if (pClass->get_category() == CAT_SIMPLEX)
   {
      C_DBCONNECTION *pTempConn;
      C_STRING       TempTableRef;
      int            visibDA;

      if (pClass->get_type() != TYPE_NODE)
         { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

      visibDA = (GEOsimAppl::GLOBALVARS.get_InsPos() == AUTO) ? GS_GOOD : GS_BAD;

      // ricavo le connessioni ai database
      if ((pTempConn = pClass->ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;

      // ricavo la tabella temporanea
      if (pClass->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

      // preparo istruzione per l'inserimento di record nella tabella temp
      if (pTempConn->InitInsRow(TempTableRef.get_name(), pInsRs) == GS_BAD) return GS_BAD;

      // si deve leggere un campo chiave per il collegamento ad una tabella esterna
      if (pConn && pTable && pKeyAttr)
      {
         C_STRING      id;
         _CommandPtr   pCmd;
         _ParameterPtr pParam;
         resbuf        Value;
         C_RB_LIST     ExtColValues, Struct;
         _RecordsetPtr pRs;

         if (gsc_prepareReadExtDbImportFile(pConn, *pTable, *pKeyAttr, 
                                            pCmd, pParam) == GS_BAD)
            return GS_BAD;

         while (gsc_fscanf_for_block(pFile, Unicode, insPoint, &scale, &rotation, &id) != EOF)
         {
            StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

            if (gsc_DBIsNumeric(pParam->Type) == GS_GOOD)
               gsc_RbSubst(&Value, id.tof());
            else
               gsc_RbSubst(&Value, id.get_name());
               
            if (gsc_SetDBParam(pCmd, 0, &Value) == GS_BAD) continue;
            // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
            // in una transazione fa casino (al secondo recordset che viene aperto)
            if (gsc_ExeCmd(pCmd, pRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD) continue;
            if (gsc_isEOF(pRs) == GS_GOOD) { gsc_DBCloseRs(pRs); continue; }
            if (gsc_DBReadRow(pRs, ExtColValues) == GS_BAD)
               { gsc_DBCloseRs(pRs); continue; }
            gsc_DBCloseRs(pRs);
            // sostituisco gli attributi
            if (RbToRbOnlyFieldCompatible(pClass, ExtColValues, ColValues) == GS_BAD)
               continue;

            // Richiamo il comando di inserimento blocco
            if (gsc_insert_block(pClass->ptr_fas()->block, insPoint, scale, scale,
                                 rotation) != GS_GOOD) continue;
            acdbEntLast(ent);

            // Richiamo la funzione di importazione
            ss.clear();
            ss.add(ent);
            pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                ColValues, NULL, pInsRs, &flagLocked);
         }
      }
      else
      {
         // Se la classe di destinazione ha una attrib list ricavo la scheda di Default
         if (pClass->ptr_attrib_list()->get_StaticDefValues(ColValues) == GS_BAD)
            return GS_BAD;

         while (gsc_fscanf_for_block(pFile, Unicode, insPoint, &scale, &rotation) != EOF)
         {
            StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

            // Richiamo il comando di inserimento blocco
            if (gsc_insert_block(pClass->ptr_fas()->block, insPoint, scale, scale,
                                 rotation) != GS_GOOD) continue;
            acdbEntLast(ent);

            // Richiamo la funzione di importazione
            ss.clear();
            ss.add(ent);
            pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                ColValues, NULL, pInsRs, &flagLocked);
         }
      }
   }
   else if (pClass->get_category() == CAT_SPAGHETTI)
   {
      while (gsc_fscanf_for_block(pFile, Unicode, insPoint, &scale, &rotation) != EOF)
      {
         StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

         // Richiamo il comando di inserimento blocco
         if (gsc_insert_block(pClass->ptr_fas()->block, insPoint, scale, scale,
                              rotation) != GS_GOOD) continue;
         acdbEntLast(ent);

         // Richiamo la funzione di importazione
         ss.clear();
         ss.add(ent);
         pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                             ColValues, NULL, pInsRs, &flagLocked);
      }
   }
   else
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
  
   StatusLineMsg.End(gsc_msg(70), i); // "%ld oggetti grafici elaborati."

   return GS_GOOD;
}


/*******************************************************/
/*.doc gsc_fscanf_for_text                 <internal> */
/*+                                                                   
  Funzione di ausilio per importazione dei testi da file testo.
  La funzione fa un fscanf in versione unicode o ansi con lettura 
  opzionale del ID di link a db esterno.
  Parametri:
  FILE *pFile;          Puntatore al file
  bool  Unicode;        Flag che determina se il contenuto del file è in 
                        formato UNICODE o ANSI (default = false)
  ads_point Pt;         Punto di inserimento 
  double *HText;        Altezza testo
  double *Rot;          Rotazione
  C_STRING &Txt;        Valore del testo
  C_STRING *pId = NULL; Opzionale, puntatore a stringa on ID per link a DB esterno
                        (default = NULL)

  Restituisce EOF in caso di fallimento (es. fine file raggiunta)
  altrimenti valore diverso da EOF. 
-*/  
/*********************************************************/
int gsc_fscanf_for_text(FILE *pFile, bool Unicode, ads_point Pt, 
                        double *HText, double *Rot, C_STRING &Txt, C_STRING *pId = NULL)
{
   int Res;

   if (Unicode)
   {
      TCHAR _Txt[ACCESS_MAX_LEN_FIELDCHAR + 1];

      if (pId)
      {
         TCHAR _Id[ACCESS_MAX_LEN_FIELDCHAR + 1];

         // %[^,] = leggi la stringa fino ad arrivare al carattere ,
         if ((Res = fwscanf(pFile, _T("%lf,%lf,%lf,%lf,%lf,%[^,],%s"), 
                            &Pt[X], &Pt[Y], &Pt[Z], HText, Rot, &_Txt, &_Id)) != 7)
            return EOF;
         if (pId) pId->set_name(_Id);
      }
      else
         if ((Res = fwscanf(pFile, _T("%lf,%lf,%lf,%lf,%lf,%s"), 
                            &Pt[X], &Pt[Y], &Pt[Z], HText, Rot, &_Txt)) != 6)
            return EOF;

      Txt = _Txt;
   }
   else
   {
      char _Txt[ACCESS_MAX_LEN_FIELDCHAR + 1];

      if (pId)
      {
         char _Id[ACCESS_MAX_LEN_FIELDCHAR + 1];
         
         // %[^,] = leggi la stringa fino ad arrivare al carattere ,
         if ((Res = fscanf(pFile, "%lf,%lf,%lf,%lf,%lf,%[^,],%s", 
                           &Pt[X], &Pt[Y], &Pt[Z], HText, Rot, &_Txt, &_Id)) != 7)
            return EOF;
         if (pId) pId->set_name(_Id);
      }
      else
         if ((Res = fscanf(pFile, "%lf,%lf,%lf,%lf,%lf,%s", 
                           &Pt[X], &Pt[Y], &Pt[Z], HText, Rot, &_Txt)) != 6)
            return EOF;

      Txt = _Txt;
   }

   return Res;
}


/*******************************************************/
/*.doc gsc_ImportTextsFromFile                         */
/*+                                                                   
  Funzione di importazione di testi da file testo.
  Parametri:
  FILE *pFile;               Puntatore al file delle caratteristiche
  bool  Unicode;             Flag che determina se il contenuto del file è in 
                             formato UNICODE o ANSI
  C_CLASS *pClass;           Puntatore ad una classe di tipo nodo
  C_DBCONNECTION *pConn;     Opzionale, se presente indica il riferimento
                             ad un database da cui leggere gli attributi
                             (Default = NULL)
  C_STRING       *pTable;    Se pConn è valido, indica la tabella da cui
                             leggere gli attributi (Default = NULL)
  C_STRING       *pKeyAttr;  Se pConn è valido, indica l'attributo chiave
                             per leggere i valori degli attributi che trovano
                             corrispondenza con quelli della classe (Default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_ImportTextsFromFile(FILE *pFile, bool Unicode, C_CLASS *pClass,
                            C_DBCONNECTION *pConn, C_STRING *pTable, C_STRING *pKeyAttr)
{  
   double        scale = 1.0, rotation = 0.0, height = 1.0;
   C_STRING      string;
   ads_point     insPoint;
   ads_name      ent;
   C_RB_LIST     ColValues;
   C_SELSET      ss, ssDummy;
   int           flagLocked;
   _RecordsetPtr pInsRs;
   long          BitForChangeFAS;
   long                 i = 0;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(344)); // "Importa entità"
   StatusLineMsg.Init(gsc_msg(70), LITTLE_STEP); // ogni 10 "%ld oggetti grafici elaborati."

   BitForChangeFAS = pClass->what_is_graph_updateable();

   // Verifico ora la tipologia dei dati da importare e la classe di destinazione
   if (pClass->get_category()== CAT_SIMPLEX)
   {
      C_DBCONNECTION *pTempConn;
      C_STRING       TempTableRef;

      if (pClass->get_type() != TYPE_TEXT)
         { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

      // ricavo le connessioni ai database
      if ((pTempConn = pClass->ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;

      // ricavo la tabella temporanea
      if (pClass->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

      // preparo istruzione per l'inserimento di record nella tabella temp
      if (pTempConn->InitInsRow(TempTableRef.get_name(), pInsRs) == GS_BAD) return GS_BAD;

      // si deve leggere un campo chiave per il collegamento ad una tabella esterna
      if (pConn && pTable && pKeyAttr)
      {
         C_STRING      id;
         _CommandPtr   pCmd;
         _ParameterPtr pParam;
         resbuf        Value;
         _RecordsetPtr pRs;
         C_RB_LIST     ExtColValues, Struct;

         if (gsc_prepareReadExtDbImportFile(pConn, *pTable, *pKeyAttr, 
                                            pCmd, pParam) == GS_BAD)
            return GS_BAD;

         while (gsc_fscanf_for_text(pFile, Unicode, insPoint, &height, &rotation, string, &id) != EOF)
         {
            StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

            if (gsc_DBIsNumeric(pParam->Type) == GS_GOOD)
               gsc_RbSubst(&Value, id.tof());
            else
               gsc_RbSubst(&Value, id.get_name());
               
            if (gsc_SetDBParam(pCmd, 0, &Value) == GS_BAD) continue;
            // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
            // in una transazione fa casino (al secondo recordset che viene aperto)
            if (gsc_ExeCmd(pCmd, pRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD) continue;
            if (gsc_isEOF(pRs) == GS_GOOD) { gsc_DBCloseRs(pRs); continue; }
            if (gsc_DBReadRow(pRs, ExtColValues) == GS_BAD)
               { gsc_DBCloseRs(pRs); continue; }
            gsc_DBCloseRs(pRs);
            // sostituisco gli attributi
            if (RbToRbOnlyFieldCompatible(pClass, ExtColValues, ColValues) == GS_BAD)
               continue;

            // Richiamo il comando di inserimento testo
            if (gsc_insert_text(string.get_name(), insPoint, height, rotation,
                                pClass->ptr_fas()->style,
                                pClass->ptr_fas()->thickness) != GS_GOOD) continue;
            acdbEntLast(ent);

            // Richiamo la funzione di importazione
            ss.clear();
            ss.add(ent);
            pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                ColValues, NULL, pInsRs, &flagLocked);
         }
      }
      else
      {
         // Se la classe di destinazione ha una attrib list ricavo la scheda di Default
         if (pClass->ptr_attrib_list()->get_StaticDefValues(ColValues) == GS_BAD)
            return GS_BAD;

         while (gsc_fscanf_for_text(pFile, Unicode, insPoint, &height, &rotation, string) != EOF)
         {
            StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

            // Richiamo il comando di inserimento testo
            if (gsc_insert_text(string.get_name(), insPoint, height, rotation,
                                pClass->ptr_fas()->style,
                                pClass->ptr_fas()->thickness) != GS_GOOD) continue;
            acdbEntLast(ent);

            // Richiamo la funzione di importazione
            ss.clear();
            ss.add(ent);
            pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                ColValues, NULL, pInsRs, &flagLocked);
         }
      }
   }
   else if (pClass->get_category() == CAT_SPAGHETTI)
   {
      while (gsc_fscanf_for_text(pFile, Unicode, insPoint, &height, &rotation, string) != EOF)
      {
         StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

         // Richiamo il comando di inserimento testo
         if (gsc_insert_text(string.get_name(), insPoint, height, rotation,
                              pClass->ptr_fas()->style,
                              pClass->ptr_fas()->thickness) != GS_GOOD) continue;
         acdbEntLast(ent);

         // Richiamo la funzione di importazione
         ss.clear();
         ss.add(ent);
         pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                             ColValues, NULL, pInsRs, &flagLocked);
      }
   }
   else
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
  
   StatusLineMsg.End(gsc_msg(70), i); // "%ld oggetti grafici elaborati."

   return GS_GOOD;
}


/*******************************************************/
/*.doc gsc_fscanf_for_pline                 <internal> */
/*+                                                                   
  Funzione di ausilio per importazione delle polilinee da file testo.
  La funzione fa un fscanf in versione unicode o ansi.
  Parametri:
  FILE *pFile;          Puntatore al file
  bool  Unicode;        Flag che determina se il contenuto del file è in 
                        formato UNICODE o ANSI (default = false)
  ads_point Pt;         Vertice della polilinea
  C_STRING &Id;         ID della polilinea

  Restituisce EOF in caso di fallimento (es. fine file raggiunta)
  altrimenti valore diverso da EOF. 
-*/  
/*********************************************************/
int gsc_fscanf_for_pline(FILE *pFile, bool Unicode, ads_point Pt, C_STRING &Id)
{
   int Res;

   if (Unicode)
   {
      TCHAR _Id[ACCESS_MAX_LEN_FIELDCHAR + 1];

      if ((Res = fwscanf(pFile, _T("%%lf,%lf,%lf,%s"), 
                         &Pt[X], &Pt[Y], &Pt[Z], &_Id)) != 4)
         Id = _Id;
   }
   else
   {
      char _Id[ACCESS_MAX_LEN_FIELDCHAR + 1];

      if ((Res = fscanf(pFile, "%lf,%lf,%lf,%s", 
                        &Pt[X], &Pt[Y], &Pt[Z], &_Id)) != 4)
         Id = _Id;
   }

   return Res;
}


/*******************************************************/
/*.doc gsc_ImportPlinesFromFile                        */
/*+                                                                   
  Funzione di importazione di polilinee da file testo.
  Parametri:
  FILE *pFile;               Puntatore al file delle caratteristiche
  bool  Unicode;             Flag che determina se il contenuto del file è in 
                             formato UNICODE o ANSI
  C_CLASS *pClass;           Puntatore ad una classe di tipo nodo
  C_DBCONNECTION *pConn;     Opzionale, se presente indica il riferimento
                             ad un database da cui leggere gli attributi
                             (Default = NULL)
  C_STRING       *pTable;    Se pConn è valido, indica la tabella da cui
                             leggere gli attributi (Default = NULL)
  C_STRING       *pKeyAttr;  Se pConn è valido, indica l'attributo chiave
                             per leggere i valori degli attributi che trovano
                             corrispondenza con quelli della classe (Default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD. 
-*/  
/*********************************************************/
int gsc_ImportPlinesFromFile(FILE *pFile, bool Unicode, C_CLASS *pClass,
                             C_DBCONNECTION *pConn, C_STRING *pTable, C_STRING *pKeyAttr)
{  
   C_STRING      id, Prev_id;
   ads_point     currPoint; 
   ads_name      ent;
   C_RB_LIST     ColValues;
   C_SELSET      ss, ssDummy;
   int           flagLocked;
   _RecordsetPtr pInsRs;
   C_POINT_LIST  pointList;
   C_POINT       *pPoint;
   long          BitForChangeFAS;
   long                 i = 0;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(344)); // "Importa entità"
   StatusLineMsg.Init(gsc_msg(70), LITTLE_STEP); // ogni 10 "%ld oggetti grafici elaborati."

   BitForChangeFAS = pClass->what_is_graph_updateable();

   pointList.remove_all();

   // Verifico ora la tipologia dei dati da importare e la classe di destinazione
   if (pClass->get_category() == CAT_SIMPLEX)
   {
      C_DBCONNECTION *pTempConn;
      C_STRING       TempTableRef;

      if (pClass->get_type() != TYPE_POLYLINE)
         { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

      // ricavo le connessioni ai database
      if ((pTempConn = pClass->ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;

      // ricavo la tabella temporanea
      if (pClass->getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

      // preparo istruzione per l'inserimento di record nella tabella temp
      if (pTempConn->InitInsRow(TempTableRef.get_name(), pInsRs) == GS_BAD) return GS_BAD;

      // si deve leggere un campo chiave per il collegamento ad una tabella esterna
      if (pConn && pTable && pKeyAttr)
      {
         _CommandPtr   pCmd;
         _ParameterPtr pParam;
         resbuf        Value;
         _RecordsetPtr pRs;
         C_RB_LIST     ExtColValues, Struct;

         if (gsc_prepareReadExtDbImportFile(pConn, *pTable, *pKeyAttr, 
                                            pCmd, pParam) == GS_BAD)
            return GS_BAD;

         while (gsc_fscanf_for_pline(pFile, Unicode, currPoint, id) != EOF)
         {
            if (Prev_id.len() == 0) Prev_id = id; // Prima volta

            // La polilinea va disegnata se ha almeno 2 vertici
            if (id.comp(Prev_id) != 0 && pointList.get_count() >= 2)
            {
               StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

               if (gsc_DBIsNumeric(pParam->Type) == GS_GOOD)
                  gsc_RbSubst(&Value, Prev_id.tof());
               else
                  gsc_RbSubst(&Value, Prev_id.get_name());

               Prev_id = id;

               if (gsc_SetDBParam(pCmd, 0, &Value) == GS_BAD) 
                  { pointList.remove_all(); continue; }
               // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
               // in una transazione fa casino (al secondo recordset che viene aperto)
               if (gsc_ExeCmd(pCmd, pRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD)
                  { pointList.remove_all(); continue; }
               if (gsc_isEOF(pRs) == GS_GOOD)
                  { gsc_DBCloseRs(pRs); pointList.remove_all(); continue; }
               if (gsc_DBReadRow(pRs, ExtColValues) == GS_BAD)
                  { gsc_DBCloseRs(pRs); pointList.remove_all(); continue; }
               gsc_DBCloseRs(pRs);
               // sostituisco gli attributi
               if (RbToRbOnlyFieldCompatible(pClass, ExtColValues, ColValues) == GS_BAD)
                  { pointList.remove_all(); continue; }

               // Richiamo il comando di inserimento polilinea
               if (gsc_insert_pline(pointList) != GS_GOOD)
                  { pointList.remove_all(); continue; }
               acdbEntLast(ent);

               // Richiamo la funzione di importazione
               ss.clear();
               ss.add(ent);
               pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                   ColValues, NULL, pInsRs, &flagLocked);

               pointList.remove_all();
            }

            // Aggiungo il vertice alla lista dei vertici            
            if ((pPoint = new C_POINT(currPoint)) == NULL)   
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            pointList.add_tail(pPoint);
         }

         if (pointList.get_count() >= 2)
         {
            if (gsc_DBIsNumeric(pParam->Type) == GS_GOOD)
               gsc_RbSubst(&Value, Prev_id.tof());
            else
               gsc_RbSubst(&Value, Prev_id.get_name());
               
            if (gsc_SetDBParam(pCmd, 0, &Value) == GS_BAD) return GS_GOOD;
            // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
            // in una transazione fa casino (al secondo recordset che viene aperto)
            if (gsc_ExeCmd(pCmd, pRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD)
               return GS_GOOD;
            if (gsc_isEOF(pRs) == GS_GOOD)
               { gsc_DBCloseRs(pRs); return GS_GOOD; }
            if (gsc_DBReadRow(pRs, ExtColValues) == GS_BAD)
               { gsc_DBCloseRs(pRs); return GS_GOOD; }
            gsc_DBCloseRs(pRs);
            // sostituisco gli attributi
            if (RbToRbOnlyFieldCompatible(pClass, ExtColValues, ColValues) == GS_BAD)
               return GS_GOOD;

            // Richiamo il comando di inserimento polilinea
            if (gsc_insert_pline(pointList) != GS_GOOD) return GS_GOOD;
            acdbEntLast(ent);

            // Richiamo la funzione di importazione
            ss.clear();
            ss.add(ent);
            pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                ColValues, NULL, pInsRs, &flagLocked);
         }
      }
      else
      {
         // Se la classe di destinazione ha una attrib list ricavo la scheda di Default
         if (pClass->ptr_attrib_list()->get_StaticDefValues(ColValues) == GS_BAD)
            return GS_BAD;

         while (gsc_fscanf_for_pline(pFile, Unicode, currPoint, id) != EOF)
         {
            if (Prev_id.len() == 0) Prev_id = id; // Prima volta

            // La polilinea va disegnata se ha almeno 2 vertici
            if (id.comp(Prev_id) != 0 && pointList.get_count() >= 2)
            {
               StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

               Prev_id = id;

               // Richiamo il comando di inserimento polilinea
               if (gsc_insert_pline(pointList) != GS_GOOD)
                  { pointList.remove_all(); continue; }
               acdbEntLast(ent);

               // Richiamo la funzione di importazione
               ss.clear();
               ss.add(ent);
               pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                   ColValues, NULL, pInsRs, &flagLocked);

               pointList.remove_all();
            }

            // Aggiungo il vertice alla lista dei vertici            
            if ((pPoint = new C_POINT(currPoint)) == NULL)   
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
            pointList.add_tail(pPoint);
         }

         if (pointList.get_count() >= 2)
            // Richiamo il comando di inserimento polilinea
            if (gsc_insert_pline(pointList) == GS_GOOD)
            {
               acdbEntLast(ent);

               // Richiamo la funzione di importazione
               ss.clear();
               ss.add(ent);
               pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                   ColValues, NULL, pInsRs, &flagLocked);
            }
      }
   }
   else if (pClass->get_category() == CAT_SPAGHETTI)
   {
      while (gsc_fscanf_for_pline(pFile, Unicode, currPoint, id) != EOF)
      {
         StatusLineMsg.Set(++i); // "%ld oggetti grafici elaborati."

         if (Prev_id.len() == 0) Prev_id = id; // Prima volta

         // La polilinea va disegnata se ha almeno 2 vertici
         if (id.comp(Prev_id) != 0 && pointList.get_count() >= 2)
         {
            Prev_id = id;

            // Richiamo il comando di inserimento polilinea
            if (gsc_insert_pline(pointList) != GS_GOOD)
               { pointList.remove_all(); continue; }
            acdbEntLast(ent);
            pClass->ins_data(ent, ColValues);

            pointList.remove_all();
         }

         // Aggiungo il vertice alla lista dei vertici            
         if ((pPoint = new C_POINT(currPoint)) == NULL)   
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pointList.add_tail(pPoint);
      }

      if (pointList.get_count() >= 2)
         // Richiamo il comando di inserimento polilinea
         if (gsc_insert_pline(pointList) == GS_GOOD)
         {
            acdbEntLast(ent);

            // Richiamo la funzione di importazione
            ss.clear();
            ss.add(ent);
            pClass->import_data(ss, ssDummy, GS_BAD, BitForChangeFAS, NULL,
                                ColValues, NULL, pInsRs, &flagLocked);
         }
   }
   else
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
  
   StatusLineMsg.End(gsc_msg(70), i); // "%ld oggetti grafici elaborati."

   return GS_GOOD;
}