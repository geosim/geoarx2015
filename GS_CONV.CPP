/**********************************************************
Name: GS_CONV

Module description: funzioni per le conversioni della banca dati di GEOsim
                    dalla versione 2013 alla 2015
            
Author: Roberto Poltini

(c) Copyright 1997-2014 by IREN ACQUA GAS  S.p.A.

              
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
#include "adslib.h"   
#include "adeads.h"
#include "adsdlg.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"     // codici errori
#include "gs_opcod.h"
#include "gs_utily.h"
#include "gs_resbf.h"     // gestione resbuf
#include "gs_list.h"      // gestione liste C++ 
#include "gs_ase.h"       
#include "gs_user.h"      // gestione utenti
#include "gs_dwg.h"       // gestione disegni
#include "gs_ade.h"
#include "gs_class.h"     // gestione classi di entità GEOsim
#include "gs_netw.h"
#include "gs_prjct.h"     // gestione progetti GEOsim
#include "gs_init.h"      // 
#include "gs_area.h"      // gestione sessioni di lavoro
#include "gs_graph.h"     
#include "gs_conv.h"      // conversione dati da versione di GEOsim precedente
#include "gs_topo.h"      // per topologia simulazioni


/*********************************************************/
/* PRIVATE FUNCTIONS                                     */
/*********************************************************/


static int gsc_Conv2ActualVer_GS_PRJ(void);
static int gsc_Conv2ActualVer_GS_SRID(void);
static int gsc_Conv2ActualVer_GS_SET_CLASS(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_SET_CLASS_TREE(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_CLASS(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_CLASS_GRAPH(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_CLASS_GRAPH_INFO(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_ATTR(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_ATTR_GRAPH(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_CONCT(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_GROUP(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_LAYER_MODEL(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_LOCK(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_SEC(C_PROJECT *pPrj);
static int gsc_Conv2ActualVer_GS_WRKSESSION(C_PROJECT *pPrj);

int gsc_MainTabsConv2DB(int prj, C_STRING &ConnStrUDLFile, C_2STR_LIST &UDLProperties);

int gsc_setActualGEOsimVersion(int prj, int cls);
int gsc_ConvDWG2ActualVer(TCHAR *Dwg);
int gsc_prepareDWGConv2ActualVer(TCHAR *DwgFile, FILE *ScriptFile,
                                 bool isCurrDWGChanged);
int gsc_prepareFilesConv2ActualVer(C_CLASS *pClass, FILE *ScriptFile, bool isCurrDWGChanged);


/*****************************************************************************/
/*.doc gs_ClsConv2ActualVer                                                  */
/*+
  Questa funzione LISP aggiorna una o più classi, da GEOsim 2013 a GEOsim 2015.
  Parametri:
  lista resbuf (<prj> (<cls1> <cls2> ...))

  Restituisce RTNORM in caso di successo altrimenti restituisce RTERROR (per il LISP
  rispettivamente T e NIL).
-*/  
/*************************************************************************************/
int gs_ClsConv2ActualVer(void)
{
   resbuf     *arg = acedGetArgs();
   int        prj;
   C_INT_LIST ClassList;

   acedRetNil();
   if (arg == NULL || arg->restype != RTSHORT) // non è stato passato
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   prj = arg->resval.rint; 
   if (ClassList.from_rb(arg->rbnext) == GS_BAD) return RTERROR;
   
   if (gsc_ClsConv2ActualVer(prj, ClassList) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*****************************************************************************/
/*.doc gsc_ClsConv2ActualVer                                                  */
/*+
  Questa funzione converte delle classi di entità di GEOsim 2013 a GEOsim 2015.
  Converte il formato dei DWG e per le classi con grafica in DB converte il campo COLOR e
  HATCH_COLOR in formato testo di 11 caratteri (per memorizzare al max "255,255,255").
  Parametri:
  int        prj;          codice progetto
  C_INT_LIST &ClassList;   Lista codici classi

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*************************************************************************************/
int gsc_ClsConv2ActualVer(int prj, C_INT_LIST &ClassList)
{
   C_CLASS  *pClass;
   FILE     *file;
   C_STRING Path;
   int      result;
   C_INT    *pClsCode;
   bool     isCurrDWGChanged;

   if (gsc_check_op(opConv2ActualVer) == GS_BAD) return GS_BAD;
   if (GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

   // "La conversione delle classi prevede che queste NON siano usate da nessun altro utente.
   //  Confermare l'operazione."
   if (gsc_ddgetconfirm(gsc_msg(792), &result) != GS_GOOD || result != GS_GOOD)
      return GS_CAN;

   // "Si consiglia di creare delle copie di backup dei disegni. Confermare l'operazione."
   if (gsc_ddgetconfirm(gsc_msg(53), &result) != GS_GOOD || result != GS_GOOD)
      return GS_CAN;

   isCurrDWGChanged = gsc_isCurrentDWGModified();

   // scrivo file script
   Path = GEOsimAppl::CURRUSRDIR;
   Path += _T('\\');
   Path += GEOTEMPDIR;
   Path += _T('\\');
   Path += GS_SCRIPT;
   if ((file = gsc_fopen(Path, _T("w"))) == NULL) return GS_BAD;

   pClsCode = (C_INT *) ClassList.get_head();
   while (pClsCode)
   {
      if ((pClass = gsc_find_class(prj, pClsCode->get_key())) == NULL)
         { gsc_fclose(file); return GS_BAD; }

      if (gsc_isCompatibGEOsimVersion(pClass->ptr_id()->Version.get_name()) == GS_GOOD)
         acutPrintf(gsc_msg(66), pClass->ptr_id()->name); // \n*Attenzione* La classe <%s> non necessita di essere convertita.
      else
      {
         // In GEOsim 2013 i campi contenente il colore sono di tipo carattere
         if (pClass->ptr_GphInfo() && pClass->ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource)
         {  // Se i dati geometrici sono in tabelle DB
            C_DBGPH_INFO   *pGphInfo = (C_DBGPH_INFO *) pClass->ptr_GphInfo();
            C_DBCONNECTION *pConn = pGphInfo->getDBConnection();
            C_STRING       TableRef;

            if (pGphInfo->LinkedTable == false)
               if (pClass->get_type() == TYPE_SPAGHETTI || pClass->get_type() == TYPE_SURFACE)
               {
                  if (pConn->ConvFieldToChar(pGphInfo->TableRef, _T("COLOR"), 11) == GS_BAD)
                     { gsc_fclose(file); return GS_BAD; }
                  if (pConn->ConvFieldToChar(pGphInfo->TableRef, _T("HATCH_COLOR"), 11) == GS_BAD)
                     { gsc_fclose(file); return GS_BAD; }
               }
               else if (pConn->ConvFieldToChar(pGphInfo->TableRef, _T("COLOR"), 11) == GS_BAD)
                     { gsc_fclose(file); return GS_BAD; }

            if (pGphInfo->LinkedLblGrpTable == false && pGphInfo->LblGroupingTableRef.len() > 0)
               if (pConn->ConvFieldToChar(pGphInfo->LblGroupingTableRef, _T("COLOR"), 11) == GS_BAD)
                  { gsc_fclose(file); return GS_BAD; }
               
            if (pGphInfo->LinkedLblTable == false && pGphInfo->LblTableRef.len() > 0)
            {
               if (pConn->ConvFieldToChar(pGphInfo->LblTableRef, _T("COLOR"), 11) == GS_BAD)
                  { gsc_fclose(file); return GS_BAD; }
               if (pConn->ConvFieldToBool(pGphInfo->LblTableRef, _T("HIDDEN")) == GS_BAD)
                  { gsc_fclose(file); return GS_BAD; }
            }
         }

         // se ha collegamento con database
         if (pClass->ptr_info()) pClass->reindexTab();

         if (gsc_prepareFilesConv2ActualVer(pClass, file, isCurrDWGChanged) == GS_BAD)
            { gsc_fclose(file); return GS_BAD; }


         // Poichè la funzione precedente termina con un "_.NEW" che 
         // forza il caricamento dei tematismi di geosim la sessione di AutoCAd risulta modificata
         // Non più valido perchè in fase di login prima venivano settate 
         // alcune variabili d'ambiente (DIMASSOC) che modificavano il dwg corrente, ora non più.
         //isCurrDWGChanged = true;
      }

      pClsCode = (C_INT *) ClassList.get_next();
   }

   if (gsc_fclose(file) == GS_BAD) return GS_BAD;

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(prj);

   // Avvio la conversione
   return gsc_callCmd(_T("_.SCRIPT"), RTSTR, Path.get_name(), 0);
}


/*****************************************************************************/
/*.doc gsc_prepareScriptForPurgeAll                                          */
/*+
  Questa funzione prepara i files necessari per convertire una classe di GEOsim 2013 
  a GEOsim 2015.
  Parametri:
  C_CLASS *pClass;      Classe da convertire
  FILE *ScriptFile;     File Script

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_prepareScriptForPurgeAll(FILE *ScriptFile)
{
   // purge ALL
   if (fwprintf(ScriptFile, _T("_.PURGE\n_ALL\n*\n_N\n")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   // cancello tutti i LPT che non hanno riferimento ad oggetti grafici
   if (fwprintf(ScriptFile, _T("(GS_PURGELPTS)\n")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   // cancello tutti le tabelle OD che non hanno riferimento ad oggetti grafici
   if (fwprintf(ScriptFile, _T("(GS_PURGEODTABLES)\n")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_ConvDWG2ActualVer                                          */
/*+
  Questa funzione aggiorna il disegno alla versione attuale di Autocad MAP.
  Parametri:
  TCHAR *DwgFile;                   Path completa del file DWG

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_ConvDWG2ActualVer(TCHAR *Dwg)
{
   AcDbDatabase *pExtDatabase = NULL;
   C_STRING     _Dwg(Dwg), TempDwg, OldDwg, Path, Dir, FileName, Ext;

   // Controlla Correttezza Path
   if (gsc_nethost2drive(_Dwg) == GS_BAD) return GS_BAD; 
   gsc_splitpath(_Dwg, &Path, &Dir, &FileName, &Ext);

   if (gsc_path_exist(_Dwg) == GS_BAD)
   {  
      if (Ext.len() > 0) return GS_GOOD;
      
      // Se il disegno non aveva estensione lo cerco aggiungendo .dwg"
      _Dwg += _T(".DWG");
      if (gsc_path_exist(_Dwg) == GS_BAD) return GS_GOOD;      
   }

   Path += Dir;

   // faccio una copia di backup del dwg
   if (gsc_get_tmp_filename(Path.get_name(), FileName.get_name(), _T(".DWG"), OldDwg) == GS_BAD)
      return GS_BAD;
   if (gsc_copyfile(_Dwg, OldDwg) == GS_BAD) return GS_BAD;

   try
   {
      // Ricavo un nome di file temporaneo
      if (gsc_get_tmp_filename(Path.get_name(), _T("GS2013_TO_GS2015_"), _T(".DWG"), TempDwg) == GS_BAD)
         AfxThrowUserException();

      // leggo il dwg
      if ((pExtDatabase = new AcDbDatabase(Adesk::kFalse)) == NULL)
         { GS_ERR_COD = eGSOutOfMem; AfxThrowUserException(); }
      if (pExtDatabase->readDwgFile(_Dwg.get_name(), _SH_DENYNO) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; AfxThrowUserException(); }
      // Scrivo un dwg temporaneo
      if (pExtDatabase->saveAs(TempDwg.get_name()) != Acad::eOk)
         { GS_ERR_COD = eGSInvalidPath; AfxThrowUserException(); }
      delete pExtDatabase; pExtDatabase = NULL;
      // Cancello il dwg originale
      if (gsc_delfile(_Dwg) == GS_BAD) AfxThrowUserException();
      // Rinomino il dwg temporaneo in quello originale
      if (gsc_renfile(TempDwg, _Dwg) == GS_BAD) AfxThrowUserException();
      // Cancello il dwg di backup
      gsc_delfile(OldDwg);
   }
   catch (...)
   {
      if (pExtDatabase) delete pExtDatabase;
      if (gsc_path_exist(_Dwg) == GS_BAD && gsc_path_exist(OldDwg) == GS_GOOD)
      {
         gsc_copyfile(OldDwg, _Dwg);
         gsc_delfile(OldDwg);
      }
      return GS_BAD;
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_prepareDWGConv2ActualVer                                          */
/*+
  Questa funzione scrive in uno script le istruzioni per aggiornare il disegno
  alla versione attuale di Autocad MAP.
  Parametri:
  TCHAR *DwgFile;          Path completa del file DWG
  FILE *ScriptFile;        File Script in cui scrivere le istruzioni
  bool isCurrDWGChanged;   Flag che determina se la sessione di acad è
                           stata modificata e di conseguenza se il
                           comando OPEN richiede la conferma di 
                           salvare le modifiche del disegno corrente

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_prepareDWGConv2ActualVer(TCHAR *DwgFile, FILE *ScriptFile, 
                                 bool isCurrDWGChanged)
{
   C_STRING TempDwg, Dir, dummy;

   if (gsc_strlen(DwgFile) == 0) return GS_GOOD;

   if (isCurrDWGChanged)
   {
      if (fwprintf(ScriptFile, _T("_.OPEN\n_Y\n\"%s\"\n"), DwgFile) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }
   else
      if (fwprintf(ScriptFile, _T("_.OPEN\n\"%s\"\n"), DwgFile) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   if (gsc_prepareScriptForPurgeAll(ScriptFile) == GS_BAD) return GS_BAD;

   // Devo fare un SAVEAS quindi cancellare il vecchio file
   // e rinominare quello generato
   gsc_splitpath(DwgFile, &Dir, &dummy);
   Dir += dummy;
   if (gsc_get_tmp_filename(Dir.get_name(), _T("GS2013_TO_GS2015_"), _T(".DWG"), TempDwg) == GS_BAD)
      return GS_BAD;
   if (fwprintf(ScriptFile, _T("_.SAVEAS\n\n\"%s\"\n"), TempDwg.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   if (fwprintf(ScriptFile, _T("_.NEW\n\n")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   dummy = DwgFile;
   dummy.strtran(_T("\\"), _T("/"));
   if (fwprintf(ScriptFile, _T("(VL-FILE-DELETE \"%s\")\n"), dummy.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   TempDwg.strtran(_T("\\"), _T("/"));
   if (fwprintf(ScriptFile, _T("(VL-FILE-RENAME \"%s\" \"%s\")\n"), 
                TempDwg.get_name(), dummy.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_prepareFilesConv2ActualVer                                        */
/*+
  Questa funzione prepara i files necessari per convertire una classe di GEOsim 2013 
  a GEOsim 2015.
  Parametri:
  C_CLASS *pClass;         Classe da convertire
  FILE *ScriptFile;        File Script
  bool isCurrDWGChanged;   Flag che determina se la sessione di acad è
                           stata modificata e di conseguenza se il
                           comando OPEN richiede la conferma di 
                           salvare le modifiche del disegno corrente

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_prepareFilesConv2ActualVer(C_CLASS *pClass, FILE *ScriptFile,
                                   bool isCurrDWGChanged)
{
   C_DWG_LIST DwgList;
   C_DWG      *pDwg;
   C_STRING   Dwg, TempDwg, Dir, dummy;
   C_GPH_INFO *pGphInfo;
   bool       Consider_isCurrDWGChanged;

   if (pClass->get_category() == CAT_EXTERN)
      pGphInfo = ((C_SUB *) pClass->ptr_sub_list()->get_head())->ptr_GphInfo();
   else 
      pGphInfo = pClass->ptr_GphInfo();

   if (pClass->ptr_fas() || pClass->get_category() == CAT_EXTERN)
   {
      // Se la grafica è in formato dwg
      if (pGphInfo && pGphInfo->getDataSourceType() == GSDwgGphDataSource)
         // inizializzo la lista dei dwg della classe
         if (DwgList.load(pClass->ptr_id()->pPrj->get_key(), pClass->ptr_id()->code) == GS_BAD)
            return GS_BAD;

      // scrivo file script
      if ((pDwg = (C_DWG *) DwgList.get_head()) != NULL)
      {
         if (gsc_ConvDWG2ActualVer(pDwg->get_name()) == GS_BAD)
            return GS_BAD;

         if (isCurrDWGChanged)
         {
            if (fwprintf(ScriptFile, _T("_.OPEN\n_Y\n\"%s\"\n"), pDwg->get_name()) < 0)
               { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
         }
         else
            if (fwprintf(ScriptFile, _T("_.OPEN\n\"%s\"\n"), pDwg->get_name()) < 0)
               { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

         // purge ALL
         if (gsc_prepareScriptForPurgeAll(ScriptFile) == GS_BAD) return GS_BAD;

         // Salva senza salvataggio incrementale
         if (fwprintf(ScriptFile, _T("(SETQ svprcnt (GETVAR \"ISAVEPERCENT\"))\n(SETVAR \"ISAVEPERCENT\" 0)")) < 0 ||
             fwprintf(ScriptFile, _T("\n_.SAVE\n\"%s\"\n"), pDwg->get_name()) < 0 ||
             fwprintf(ScriptFile, _T("(SETVAR \"ISAVEPERCENT\" svprcnt)\n")) < 0)
            { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

         pDwg = (C_DWG *) DwgList.get_next();
         while (pDwg)
         {
            if (gsc_ConvDWG2ActualVer(pDwg->get_name()) == GS_BAD)
               return GS_BAD;
            if (fwprintf(ScriptFile, _T("_.OPEN\n\"%s\"\n"), pDwg->get_name()) < 0)
               { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

            // purge ALL
            if (gsc_prepareScriptForPurgeAll(ScriptFile) == GS_BAD) return GS_BAD;

            // Salva senza salvataggio incrementale
            if (fwprintf(ScriptFile, _T("(SETQ svprcnt (GETVAR \"ISAVEPERCENT\"))\n(SETVAR \"ISAVEPERCENT\" 0)")) < 0 ||
                fwprintf(ScriptFile, _T("\n_.SAVE\n\"%s\"\n"), pDwg->get_name()) < 0 ||
                fwprintf(ScriptFile, _T("(SETVAR \"ISAVEPERCENT\" svprcnt)\n")) < 0)
               { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

            pDwg = (C_DWG *) DwgList.get_next();
         }
      }

      // Converto i blocchi di riferimento se esistenti
      if (pClass->get_category() == CAT_EXTERN)
      {
         C_SUB *pSub = (C_SUB *) pClass->ptr_sub_list()->get_head();

         while (pSub)
         {
            if (gsc_ConvDWG2ActualVer(pSub->ptr_fas()->file_ref_block) == GS_BAD)
               return GS_BAD;
            pSub = (C_SUB *) pSub->get_next();
         }
      }
      else
         if (gsc_ConvDWG2ActualVer(pClass->ptr_fas()->file_ref_block) == GS_BAD)
            return GS_BAD;
   }

   // aggiorno struttura sim gas bassa (2) e media (3)
   if (pClass->get_category() == CAT_EXTERN &&
       (pClass->get_type() == 2 || pClass->get_type() == 3))
   {
      // devo ricaricare i tematismi prima di aggiornare la simulazione
      // altrimenti la creazione della sottoclasse PRV del gas non funziona
      if (fwprintf(ScriptFile, _T("(gs_LoadGEOsimThm)\n(gs_UpdateGasSimStru %d %d)\n"),
                   pClass->ptr_id()->pPrj->get_key(), pClass->ptr_id()->code) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; } 
      if (fwprintf(ScriptFile, _T("(gs_setActualGEOsimVersion %d %d)\n"),
                   pClass->ptr_id()->pPrj->get_key(), pClass->ptr_id()->code) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      // Poichè il caricamento dei tematismi potrebbe modificare il disegno corrente
      // (se tra i tematismi c'è un riempimento associativo)
      Consider_isCurrDWGChanged = true;
   }
   else
   {
      if (fwprintf(ScriptFile, _T("(gs_setActualGEOsimVersion %d %d)\n"),
                   pClass->ptr_id()->pPrj->get_key(), pClass->ptr_id()->code) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      if (DwgList.get_head()) // Se sono stati elaborati dei DWG
         // Non rispondo Yes perchè l'ultima operazione è stata SAVE e 
         // il disegno corrente non è stato poi modificato
         Consider_isCurrDWGChanged = false;
      else
         // Non si è elaborato la grafica
         Consider_isCurrDWGChanged = true;
   }

   if (Consider_isCurrDWGChanged)
   {
      if (isCurrDWGChanged)
      {
         if (fwprintf(ScriptFile, _T("_.NEW\n_Y\n\n")) < 0)
            { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      }
      else
         if (fwprintf(ScriptFile, _T("_.NEW\n\n")) < 0)
            { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }
   else
      if (fwprintf(ScriptFile, _T("_.NEW\n\n")) < 0)
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gs_MainTabsConv2ActualVer                                             */
/*+                                                           
  Questo funzione LISP aggiorna le tabelle principali di GEOsim
  da GEOsim 2013 a GEOsim 2015.
  
  Ritorna TRUE in caso di successo altrimenti NIL.
-*/  
/*****************************************************************************/
int gs_MainTabsConv2ActualVer(void)
{
   acedRetNil();

   if (gsc_MainTabsConv2ActualVer() == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*****************************************************************************/
/*.doc gsc_MainTabsConv2ActualVer                                            */
/*+
  Questa funzione aggiorna le tabelle principali di GEOsim
  da GEOsim 2013 a GEOsim 2015.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_MainTabsConv2ActualVer(void)
{
   C_PROJECT  *pPrj;
   int        result = GS_GOOD;
   C_STRING   dir, OldFile, NewFile;
   C_STR_LIST FileNameList;

   // solo per superutenti
   if (gsc_check_op(opConv2ActualVer) == GS_BAD) return GS_BAD;

   if (gsc_Conv2ActualVer_GS_PRJ() == GS_BAD)
   {
      acutPrintf(gsc_msg(445), _T(" (GS_PRJ).")); // "\nConversione progetto %s fallita."
      return GS_BAD;
   }  

   if (gsc_Conv2ActualVer_GS_SRID() == GS_BAD)
   {
      acutPrintf(gsc_msg(445), _T(" (GS_SRID).")); // "\nConversione progetto %s fallita."
      return GS_BAD;
   }  

   pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.get_head();
   while (pPrj)
   { 
      // NON verifico esistenza sessioni di lavoro, mi fido perchè cambio da mdb a accdb
      if (pPrj->is_free() == GS_GOOD)
      {
         // conversione di GS_CLASS
         if (gsc_Conv2ActualVer_GS_CLASS(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_CLASS)."));
            result = GS_BAD;
         }
         // conversione di GS_CLASS_GRAPH
         if (gsc_Conv2ActualVer_GS_CLASS_GRAPH(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_CLASS_GRAPH)."));
            result = GS_BAD;
         }
         // conversione di GS_CLASS_GRAPH_INFO
         if (gsc_Conv2ActualVer_GS_CLASS_GRAPH_INFO(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_CLASS_GRAPH_INFO)."));
            result = GS_BAD;
         }
         // conversione di GS_SET_CLASS
         if (gsc_Conv2ActualVer_GS_SET_CLASS(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_SET_CLASS)."));
            result = GS_BAD;
         }
         // conversione di GS_SET_CLASS_TREE
         if (gsc_Conv2ActualVer_GS_SET_CLASS_TREE(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_SET_CLASS_TREE)."));
            result = GS_BAD;
         }
         // conversione di GS_ATTR
         if (gsc_Conv2ActualVer_GS_ATTR(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_ATTR)."));
            result = GS_BAD;
         }
         // conversione di gsc_Conv2ActualVer_GS_ATTR_GRAPH
         if (gsc_Conv2ActualVer_GS_ATTR(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_ATTR_GRAPH)."));
            result = GS_BAD;
         }
         // conversione di GS_CONCT
         if (gsc_Conv2ActualVer_GS_CONCT(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_CONCT)."));
            result = GS_BAD;
         }
         // conversione di GS_GROUP
         if (gsc_Conv2ActualVer_GS_GROUP(pPrj) == GS_BAD)
         {
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_GROUP)."));
            result = GS_BAD;
         }
         // conversione di GS_LAYER_MODEL 
         if (gsc_Conv2ActualVer_GS_LAYER_MODEL(pPrj) == GS_BAD)
         { 
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_LAYER_MODEL)."));
            result = GS_BAD; 
         }
         // conversione di GS_LOCK 
         if (gsc_Conv2ActualVer_GS_LOCK(pPrj) == GS_BAD)
         { 
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_LOCK)."));
            result = GS_BAD; 
         }
         // conversione di GS_SEC 
         if (gsc_Conv2ActualVer_GS_SEC(pPrj) == GS_BAD)
         { 
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_SEC)."));
            result = GS_BAD; 
         }
         // conversione di GS_WRKSESSION
         if (gsc_Conv2ActualVer_GS_WRKSESSION(pPrj) == GS_BAD)
         { 
            acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
            acutPrintf(_T(" (GS_WRKSESSION)."));
            result = GS_BAD; 
         }
      }
      else
      {
         acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
         acutPrintf(_T(" %s"), gsc_msg(607)); // "Trovate sessioni di lavoro..."
      }

      pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.get_next();
   }                          

   if (result == GS_BAD) return GS_BAD;

   // Aggiorno i files dei tematismi alla versione attuale

   // file standard di GEOsim
   OldFile = GEOsimAppl::GEODIR;
   OldFile += _T('\\');
   OldFile += GEOTHMDIR;
   OldFile += _T('\\');
   OldFile += GEOTHM;
   if (gsc_ConvDWG2ActualVer(OldFile.get_name()) == GS_BAD)
      return GS_BAD;

   // files utente opzionali (tutti i files DWG che iniziano per "USRTHM")
   OldFile = GEOsimAppl::GEODIR;
   OldFile += _T('\\');
   OldFile += GEOTHMDIR;
   OldFile += _T('\\');
   OldFile += USRTHMS;
   if (gsc_adir(OldFile.get_name(), &FileNameList, NULL, NULL, NULL, true) > 0)
   {
      C_STR *pFileName = (C_STR *) FileNameList.get_head();
   
      while (pFileName)
      {
         OldFile = GEOsimAppl::GEODIR;
         OldFile += _T('\\');
         OldFile += GEOTHMDIR;
         OldFile += _T('\\');
         OldFile += pFileName->get_name();

         if (gsc_ConvDWG2ActualVer(OldFile.get_name()) == GS_BAD) return GS_BAD;

         pFileName = (C_STR *) FileNameList.get_next();
      }
   }

   // Converto anche i sinottici dei progetti
   pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.get_head();
   while (pPrj)
   { 
      if (gsc_ConvDWG2ActualVer(pPrj->get_keydwg()) == GS_BAD)
         return GS_BAD;
      pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.get_next();
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_PRJ                                              */
/*+
  Questa funzione converte la tabella GS_PRJ da GEOsim 2013 a GEOsim 2015.
  Cambia il formato da MDB a ACCDB e aggiunto il campo DESCRIPTION.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_PRJ(void)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef, LongIntType_ProviderDescr;
   C_RB_LIST      Stru, NewStru, Values;

   if (gsc_getPrjTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // Se la tabella non ha il campo "LAST_SAVE_DATE"
   if ((Stru << pConn->ReadStruct(TableRef.get_name())) == NULL) return GS_BAD;
   if (!Stru.Assoc(_T("NEXT_SET_CLASS_ID")))
   {
      int      i = 0, IntType_Dec;
      presbuf  p;
      C_STRING IntType_ProviderDescr;
      long     IntType_Len;

      Values  << acutBuildList(RTLB, 0);
      while ((p = Stru.nth(i++)))
      {
         p = gsc_nth(0, p);
         Values += acutBuildList(RTLB, RTSTR, p->resval.rstring, RTSTR, p->resval.rstring, RTLE, 0);
      }
      Values += acutBuildList(RTLB, RTSTR, _T("NEXT_SET_CLASS_ID"), RTNIL, RTLE,
                              0);
      Values += acutBuildList(RTLE, 0);
      //((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
      // converto campo da codice ADO in Provider dipendente (numero intero)
      if (pConn->Type2ProviderType(CLASS_KEY_TYPE_ENUM,      // DataType per campo chiave
                                   FALSE,                    // IsLong
                                   FALSE,                    // IsFixedPrecScale
                                   RTNIL,                    // IsFixedLength
                                   TRUE,                     // IsWrite
                                   CLASS_LEN_KEY_ATTR,       // Size
                                   0,                        // Prec
                                   IntType_ProviderDescr,   // ProviderDescr
                                   &IntType_Len, &IntType_Dec) == GS_BAD)
         return GS_BAD;

      // trasformo i tipi da codici ADO in descrizioni
      if ((NewStru << pConn->getStruForCreatingTable(Stru.get_head())) == NULL)
         return GS_BAD; 

      NewStru.remove_tail(); // elimino ultima tonda chiusa
      if ((NewStru += acutBuildList(RTLB,
                                       RTSTR, _T("NEXT_SET_CLASS_ID"),
                                       RTSTR, IntType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, IntType_Len,                    // <len>
                                       RTSHORT, 0,                              // <dec>
                                    RTLE,
                                    RTLE, 0)) == NULL)
         return GS_BAD;

      // modifico struttura tabella
      if (pConn->UpdStruct(TableRef.get_name(), NewStru.get_head(), Values.get_head()) == GS_BAD)
         return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_SRID                                             */
/*+
  Questa funzione converte la tabella GS_SRID da GEOsim 2013 a GEOsim 2015.
  Se non c'è la crea.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_SRID(void)
{
   C_DBCONNECTION *pConn, *pSourceConn;
   C_STRING       TableRef, SourceDB;

   // Se la tabella GS_SRID esiste già la cancello per ricrearla (il modello è già compilato)
   if (gsc_getSRIDTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   if (pConn->ExistTable(TableRef) == GS_GOOD)
      if (pConn->DelTable(TableRef.get_name()) == GS_BAD) return GS_BAD;

   acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"
   
   int IsTransactionSupported = pConn->BeginTrans();

   // Creo una nuova tabella GS_SRID dal DB modello ACCESSGEOSIMMAINSAMPLEDB
   SourceDB = GEOsimAppl::GEODIR + _T('\\') + GEOSAMPLEDIR + _T('\\') +ACCESSGEOSIMMAINSAMPLEDB;
   C_2STR_LIST SourceUDLProp(_T("Data Source"), SourceDB.get_name());
   pSourceConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &SourceUDLProp);
   if (pSourceConn->CopyTable(SRID_TABLE_NAME, *pConn, TableRef.get_name()) == GS_BAD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); return GS_BAD; }
   
   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_SET_CLASS                                        */
/*+
  Questa funzione converte la tabella GS_SET_CLASS da GEOsim 2013 a GEOsim 2015.
  Crea la tabella GS_SET_CLASS se non esistente.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_SET_CLASS(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn, *pSourceConn;
   C_STRING       TableRef, SourceDB;

   if (pPrj->getClassSetTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // Se la tabella GS_SET_CLASS esiste già esco
   if (pConn->ExistTable(TableRef.get_name()) == GS_GOOD) return GS_GOOD;

   acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"
   
   int IsTransactionSupported = pConn->BeginTrans();

   // Creo una nuova tabella GS_SET_CLASS dal DB modello ACCESSGEOPRJSAMPLEDB
   SourceDB = GEOsimAppl::GEODIR + _T('\\') + GEOSAMPLEDIR + _T('\\') + ACCESSGEOPRJSAMPLEDB;
   C_2STR_LIST SourceUDLProp(_T("Data Source"), SourceDB.get_name());
   pSourceConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &SourceUDLProp);
   if (pSourceConn->CopyTable(SET_CLASS_TABLE_NAME, *pConn, TableRef.get_name()) == GS_BAD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); return GS_BAD; }
   
   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_SET_CLASS_TREE                                        */
/*+
  Questa funzione converte la tabella GS_CLASS da GEOsim 2013 a GEOsim 2015.
  Crea la tabella GS_SET_CLASS_TREE se non esistente.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_SET_CLASS_TREE(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn, *pSourceConn;
   C_STRING       TableRef, SourceDB;

   if (pPrj->getClassSetTreeTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // Se la tabella GS_SET_CLASS_TREE esiste già esco
   if (pConn->ExistTable(TableRef.get_name()) == GS_GOOD) return GS_GOOD;

   acutPrintf(_T("%s %s..."), gsc_msg(594), TableRef.get_name());  // "\nConversione"
   
   int IsTransactionSupported = pConn->BeginTrans();

   // Creo una nuova tabella GS_SET_CLASS_TREE dal DB modello ACCESSGEOPRJSAMPLEDB
   SourceDB = GEOsimAppl::GEODIR + _T('\\') + GEOSAMPLEDIR + _T('\\') + ACCESSGEOPRJSAMPLEDB;
   C_2STR_LIST SourceUDLProp(_T("Data Source"), SourceDB.get_name());
   pSourceConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &SourceUDLProp);
   if (pSourceConn->CopyTable(SET_CLASS_TREE_TABLE_NAME, *pConn, TableRef.get_name()) == GS_BAD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); return GS_BAD; }
   
   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_CLASS                                            */
/*+
  Questa funzione converte la tabella GS_CLASS da GEOsim 2013 a GEOsim 2015.
  Vengono aggiunti i campi:
  LAST_SAVE_DATE.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_CLASS(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;
   C_RB_LIST      Stru, NewStru, Values;
   bool           ToUpdate = false;

   if (pPrj->getClassesTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   if ((Stru << pConn->ReadStruct(TableRef.get_name())) == NULL) return GS_BAD;

   // Se la tabella non ha il campo "LAST_SAVE_DATE"
   if (!Stru.Assoc(_T("LAST_SAVE_DATE")))
   {
      int      i = 0, DateType_Dec;
      presbuf  p;
      C_STRING DateType_ProviderDescr;
      long     DateType_Len;

      Values  << acutBuildList(RTLB, 0);
      while ((p = Stru.nth(i++)))
      {
         p = gsc_nth(0, p);
         Values += acutBuildList(RTLB, RTSTR, p->resval.rstring, RTSTR, p->resval.rstring, RTLE, 0);
      }
      Values += acutBuildList(RTLB, RTSTR, _T("LAST_SAVE_DATE"), RTNIL, RTLE,
                              0);
      Values += acutBuildList(RTLE, 0);
      // ((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
      // converto campo da codice ADO in Provider dipendente (data e ora)
      if (pConn->Type2ProviderType(adDBTimeStamp,            // DataType per campo data e ora
                                   FALSE,                    // IsLong
                                   FALSE,                    // IsFixedPrecScale
                                   RTNIL,                    // IsFixedLength
                                   TRUE,                     // IsWrite
                                   ACCESS_MAX_LEN_FIELDCHAR, // Size
                                   0,                        // Prec
                                   DateType_ProviderDescr,   // ProviderDescr
                                   &DateType_Len, &DateType_Dec) == GS_BAD)
         return GS_BAD;

      // trasformo i tipi da codici ADO in descrizioni
      if ((NewStru << pConn->getStruForCreatingTable(Stru.get_head())) == NULL)
         return GS_BAD; 

      NewStru.remove_tail(); // elimino ultima tonda chiusa
      if ((NewStru += acutBuildList(RTLB,
                                       RTSTR, _T("LAST_SAVE_DATE"),
                                       RTSTR, DateType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, DateType_Len,                    // <len>
                                       RTSHORT, DateType_Dec,                    // <dec>
                                    RTLE,
                                    RTLE, 0)) == NULL)
         return GS_BAD;

      ToUpdate = true;
   }

   // Se la tabella non ha il campo "VIEW_ENABLED"
   if (!Stru.Assoc(_T("VIEW_ENABLED")))
   {
      int      i = 0, DateType_Dec;
      presbuf  p;
      C_STRING DateType_ProviderDescr;
      long     DateType_Len;

      Values  << acutBuildList(RTLB, 0);
      while ((p = Stru.nth(i++)))
      {
         p = gsc_nth(0, p);
         Values += acutBuildList(RTLB, RTSTR, p->resval.rstring, RTSTR, p->resval.rstring, RTLE, 0);
      }
      Values += acutBuildList(RTLB, RTSTR, _T("VIEW_ENABLED"), RTNIL, RTLE,
                              RTLB, RTSTR, _T("HISTORY_ENABLED"), RTNIL, RTLE,
                              0);
      Values += acutBuildList(RTLE, 0);
      // ((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
      // converto campo da codice ADO in Provider dipendente (data e ora)
      if (pConn->Type2ProviderType(adBoolean,                // DataType per campo data e ora
                                   FALSE,                    // IsLong
                                   FALSE,                    // IsFixedPrecScale
                                   RTT,                      // IsFixedLength
                                   TRUE,                     // IsWrite
                                   1,                        // Size
                                   0,                        // Prec
                                   DateType_ProviderDescr,   // ProviderDescr
                                   &DateType_Len, &DateType_Dec) == GS_BAD)
         return GS_BAD;

      // trasformo i tipi da codici ADO in descrizioni
      if ((NewStru << pConn->getStruForCreatingTable(Stru.get_head())) == NULL)
         return GS_BAD; 

      NewStru.remove_tail(); // elimino ultima tonda chiusa
      if ((NewStru += acutBuildList(RTLB,
                                       RTSTR, _T("VIEW_ENABLED"),
                                       RTSTR, DateType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, DateType_Len,                    // <len>
                                       RTSHORT, DateType_Dec,                    // <dec>
                                    RTLE,
                                    RTLB,
                                       RTSTR, _T("HISTORY_ENABLED"),
                                       RTSTR, DateType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, DateType_Len,                    // <len>
                                       RTSHORT, DateType_Dec,                    // <dec>
                                    RTLE,
                                    RTLE, 0)) == NULL)
         return GS_BAD;

      ToUpdate = true;
   }

   if (ToUpdate == true)
      // modifico struttura tabella                                  
      if (pConn->UpdStruct(TableRef.get_name(), NewStru.get_head(), Values.get_head()) == GS_BAD)
         return GS_BAD;

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_ATTR                                             */
/*+
  Questa funzione converte la tabella GS_ATTR da GEOsim 2013 a GEOsim 2015.
  Aggiunge il campo ORDER_POSITION se non c'è.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_ATTR(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;
   C_RB_LIST      Stru, NewStru, Values;

   if (pPrj->getAttribsTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // Se la tabella non ha il campo "ORDER_POS"
   if ((Stru << pConn->ReadStruct(TableRef.get_name())) == NULL) return GS_BAD;
   if (!Stru.Assoc(_T("ORDER_POS")))
   {
      int      i = 0, IntType_Dec, BackUp = GS_GOOD;
      presbuf  p;
      C_STRING IntType_ProviderDescr, BackupTableRef;
      long     IntType_Len;

      if (pPrj->getAttribsTabInfo(&pConn, &BackupTableRef, BackUp) == GS_BAD) return GS_BAD;
      if (pConn->ExistTable(BackupTableRef) == GS_GOOD)
         // cancello la tabella di backup
         if (pConn->DelTable(BackupTableRef.get_name()) == GS_BAD) return GS_BAD;

      Values  << acutBuildList(RTLB, 0);
      while ((p = Stru.nth(i++)))
      {
         p = gsc_nth(0, p);
         Values += acutBuildList(RTLB, RTSTR, p->resval.rstring, RTSTR, p->resval.rstring, RTLE, 0);
      }
      Values += acutBuildList(RTLB, RTSTR, _T("ORDER_POS"), RTNIL, RTLE, 0);
      Values += acutBuildList(RTLE, 0);
      //((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
      // converto campo da codice ADO in Provider dipendente (integer)
      if (pConn->Type2ProviderType(adSmallInt,               // DataType per campo integer
                                   FALSE,                    // IsLong
                                   TRUE ,                    // IsFixedPrecScale
                                   RTT,                      // IsFixedLength
                                   TRUE,                     // IsWrite
                                   3,                        // Size
                                   0,                        // Prec
                                   IntType_ProviderDescr,    // ProviderDescr
                                   &IntType_Len, &IntType_Dec) == GS_BAD)
         return GS_BAD;

      // trasformo i tipi da codici ADO in descrizioni
      if ((NewStru << pConn->getStruForCreatingTable(Stru.get_head())) == NULL)
         return GS_BAD; 

      NewStru.remove_tail(); // elimino ultima tonda chiusa
      if ((NewStru += acutBuildList(RTLB,
                                       RTSTR, _T("ORDER_POS"),
                                       RTSTR, IntType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, IntType_Len,                    // <len>
                                       RTSHORT, 0,                              // <dec>
                                    RTLE,
                                    RTLE, 0)) == NULL)
         return GS_BAD;

      // modifico struttura tabella
      if (pConn->UpdStruct(TableRef.get_name(), NewStru.get_head(), Values.get_head()) == GS_BAD)
         return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_ATTR_GRAPH                                       */
/*+
  Questa funzione converte la tabella GS_ATTR_GRAPH da GEOsim 2013 a GEOsim 2015.
  Cambia il formato da MDB a ACCDB.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_ATTR_GRAPH(C_PROJECT *pPrj)
{
   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_CONCT                                            */
/*+
  Questa funzione converte la tabella GS_CONCT da GEOsim 2013 a GEOsim 2015.
  Cambia il formato da MDB a ACCDB.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_CONCT(C_PROJECT *pPrj)
{
   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_LAYER_MODEL                                      */
/*+
  Questa funzione converte la tabella GS_LAYER_MODEL da GEOsim 2013 a GEOsim 2015.
  Cambia il formato da MDB a ACCDB. Aggiunta la colonna TRANSPARENCY_PERCENT.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_LAYER_MODEL(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;

   if (pPrj->getLayerModelTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   if (pConn->ConvFieldToChar(TableRef, _T("COLOR"), 11) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_CLASS_GRAPH                                      */
/*+
  Questa funzione converte la tabella GS_CLASS_GRAPH da GEOsim 2013 a GEOsim 2015.
  Il campo COLORE e HATCH_COLOR devono diventare un testo di 11 caratteri 
  (per memorizzare al max "255,255,255").
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_CLASS_GRAPH(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;

   if (pPrj->getGraphClassesTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   if (pConn->ConvFieldToChar(TableRef, _T("COLOR"), 11) == GS_BAD)
      return GS_BAD;
   if (pConn->ConvFieldToChar(TableRef, _T("HATCH_COLOR"), 11) == GS_BAD)
      return GS_BAD;

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_CLASS_GRAPH_INFO                                  */
/*+
  Questa funzione converte la tabella GS_CLASS_GRAPH_INFO da GEOsim 2013 a GEOsim 2015.
  Aggiunto il campo:
  COLOR_FORMAT di tipo numerico (int) (con valore di default = GSAutoCADColorIndexFormatColor)
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_CLASS_GRAPH_INFO(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;
   C_RB_LIST      Stru, NewStru, Values;

   if (pPrj->getClassGphDataSrcTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // Se la tabella non ha il campo "COLOR_FORMAT"
   if ((Stru << pConn->ReadStruct(TableRef.get_name())) == NULL) return GS_BAD;
   if (!Stru.Assoc(_T("COLOR_FORMAT")))
   {
      int      i = 0, IntType_Dec;
      presbuf  p;
      C_STRING IntType_ProviderDescr;
      long     IntType_Len;

      Values  << acutBuildList(RTLB, 0);
      while ((p = Stru.nth(i++)))
      {
         p = gsc_nth(0, p);
         Values += acutBuildList(RTLB, RTSTR, p->resval.rstring, RTSTR, p->resval.rstring, RTLE, 0);
      }
      Values += acutBuildList(RTLB, RTSTR, _T("COLOR_FORMAT"), RTSHORT, (int) GSAutoCADColorIndexFormatColor, RTLE,
                              0);
      Values += acutBuildList(RTLE, 0);
      //((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
      // converto campo da codice ADO in Provider dipendente (testo)
      if (pConn->Type2ProviderType(adSmallInt,               // DataType per campo integer
                                   FALSE,                    // IsLong
                                   FALSE,                    // IsFixedPrecScale
                                   RTNIL,                    // IsFixedLength
                                   TRUE,                     // IsWrite
                                   3,                        // Size
                                   0,                        // Prec
                                   IntType_ProviderDescr,   // ProviderDescr
                                   &IntType_Len, &IntType_Dec) == GS_BAD)
         return GS_BAD;

      // trasformo i tipi da codici ADO in descrizioni
      if ((NewStru << pConn->getStruForCreatingTable(Stru.get_head())) == NULL)
         return GS_BAD; 

      NewStru.remove_tail(); // elimino ultima tonda chiusa
      if ((NewStru += acutBuildList(RTLB,
                                       RTSTR, _T("COLOR_FORMAT"),
                                       RTSTR, IntType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, IntType_Len,                    // <len>
                                       RTSHORT, IntType_Dec,                    // <dec>
                                    RTLE, 
                                    RTLE, 0)) == NULL)
         return GS_BAD;

      // modifico struttura tabella                                  
      if (pConn->UpdStruct(TableRef.get_name(), NewStru.get_head(), Values.get_head()) == GS_BAD)
         return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_GROUP                                            */
/*+
  Questa funzione converte la tabella GS_GROUP da GEOsim 2013 a GEOsim 2015.
  Cambia il formato da MDB a ACCDB.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_GROUP(C_PROJECT *pPrj)
{
   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_LOCK                                             */
/*+
  Questa funzione converte la tabella GS_LOCK da GEOsim 2013 a GEOsim 2015.
  Cambia il formato da MDB a ACCDB.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_LOCK(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pDBConn;
   C_STRING       TableRef;

   // svuoto la tabella dei lock
   if (pPrj->getLocksTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   return pDBConn->DelRows(TableRef.get_name());
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_SEC                                              */
/*+
  Questa funzione converte la tabella GS_SEC da GEOsim 2013 a GEOsim 2015.
  Cambia il formato da MDB a ACCDB e aggiunto il campo DESCRIPTION,
  REAL_INIT_DISTANCE_ATTRIB, REAL_FINAL_DISTANCE_ATTRIB, 
  NOMINAL_INIT_DISTANCE_ATTRIB, NOMINAL_FINAL_DISTANCE_ATTRIB, REAL_OFFSET_ATTRIB.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_SEC(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;
   C_RB_LIST      Stru, NewStru, Values;
   int            i = 0, CharType_Dec;
   presbuf        p;
   C_STRING       CharType_ProviderDescr;
   long           CharType_Len;
   bool           ToUpd = false;

   if (pPrj->getSecsTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   C_PROVIDER_TYPE *providerType;
   if ((providerType = pConn->getCharProviderType()) == NULL) return GS_BAD;

   //((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
   // converto campo da codice ADO in Provider dipendente (testo)
   if (pConn->Type2ProviderType(providerType->get_Type(), // DataType per campo testo
                                FALSE,                    // IsLong
                                FALSE,                    // IsFixedPrecScale
                                RTNIL,                    // IsFixedLength
                                TRUE,                     // IsWrite
                                ACCESS_MAX_LEN_FIELDCHAR, // Size
                                0,                        // Prec
                                CharType_ProviderDescr,   // ProviderDescr
                                &CharType_Len, &CharType_Dec) == GS_BAD)
      return GS_BAD;

   // Se la tabella non ha il campo "DESCRIPTION"
   if ((Stru << pConn->ReadStruct(TableRef.get_name())) == NULL) return GS_BAD;
   i = 0;
   Values << acutBuildList(RTLB, 0);
   while ((p = Stru.nth(i++)))
   {
      p = gsc_nth(0, p);
      Values += acutBuildList(RTLB, RTSTR, p->resval.rstring, RTSTR, p->resval.rstring, RTLE, 0);
   }

   if ((NewStru << pConn->getStruForCreatingTable(Stru.get_head())) == NULL) return GS_BAD; 
   NewStru.remove_tail(); // elimino ultima tonda chiusa

   if (!Stru.Assoc(_T("DESCRIPTION")))
   {
      Values += acutBuildList(RTLB, RTSTR, _T("DESCRIPTION"), RTNIL, RTLE, 0);
      if ((NewStru += acutBuildList(RTLB,
                                       RTSTR, _T("DESCRIPTION"),
                                       RTSTR, CharType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, ACCESS_MAX_LEN_FIELDCHAR,        // <len>
                                       RTSHORT, 0,                               // <dec>
                                    RTLE, 0)) == NULL)
         return GS_BAD;

      ToUpd = true;
   }
   if (!Stru.Assoc(_T("REAL_INIT_DISTANCE_ATTRIB")))
   {
      Values += acutBuildList(RTLB, RTSTR, _T("REAL_INIT_DISTANCE_ATTRIB"), RTNIL, RTLE,
                              RTLB, RTSTR, _T("REAL_FINAL_DISTANCE_ATTRIB"), RTNIL, RTLE, 
                              RTLB, RTSTR, _T("NOMINAL_INIT_DISTANCE_ATTRIB"), RTNIL, RTLE, 
                              RTLB, RTSTR, _T("NOMINAL_FINAL_DISTANCE_ATTRIB"), RTNIL, RTLE,
                              RTLB, RTSTR, _T("REAL_OFFSET_ATTRIB"), RTNIL, RTLE, 0);
      if ((NewStru += acutBuildList(RTLB,
                                       RTSTR, _T("REAL_INIT_DISTANCE_ATTRIB"),
                                       RTSTR, CharType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, MAX_LEN_FIELDNAME - 1,           // <len>
                                       RTSHORT, 0,                               // <dec>
                                    RTLE,
                                    RTLB,
                                       RTSTR, _T("REAL_FINAL_DISTANCE_ATTRIB"),
                                       RTSTR, CharType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, MAX_LEN_FIELDNAME - 1,           // <len>
                                       RTSHORT, 0,                               // <dec>
                                    RTLE,
                                    RTLB,
                                       RTSTR, _T("NOMINAL_INIT_DISTANCE_ATTRIB"),
                                       RTSTR, CharType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, MAX_LEN_FIELDNAME - 1,           // <len>
                                       RTSHORT, 0,                               // <dec>
                                    RTLE,
                                    RTLB,
                                       RTSTR, _T("NOMINAL_FINAL_DISTANCE_ATTRIB"),
                                       RTSTR, CharType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, MAX_LEN_FIELDNAME - 1,           // <len>
                                       RTSHORT, 0,                               // <dec>
                                    RTLE,
                                    RTLB,
                                       RTSTR, _T("REAL_OFFSET_ATTRIB"),
                                       RTSTR, CharType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, MAX_LEN_FIELDNAME - 1,           // <len>
                                       RTSHORT, 0,                               // <dec>
                                    RTLE, 0)) == NULL)
         return GS_BAD;

      ToUpd = true;
   }

   if (ToUpd)
   {
      Values += acutBuildList(RTLE, 0);
      NewStru += acutBuildList(RTLE, 0);

      // modifico struttura tabella                                  
      if (pConn->UpdStruct(TableRef.get_name(), NewStru.get_head(), Values.get_head()) == GS_BAD)
         return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc gsc_Conv2ActualVer_GS_WRKSESSION                                       */
/*+
  Questa funzione converte la tabella GS_SEC da GEOsim 2013 a GEOsim 2015.
  Cambia il formato da MDB a ACCDB e aggiunto il campo CLASS_CODE_LIST.
  Parametri:
  C_PROJECT *pPrj;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
static int gsc_Conv2ActualVer_GS_WRKSESSION(C_PROJECT *pPrj)
{
   C_DBCONNECTION *pConn;
   C_STRING       TableRef;
   C_RB_LIST      Stru, NewStru, Values;
   int            i = 0, CharType_Dec, MemoLen = 2000; // <500 classi> * (<cod a 3 cifre> + <1 separatore>)
   presbuf        p;
   C_STRING       CharType_ProviderDescr;
   long           CharType_Len;
   bool           ToUpd = false;

   if (pPrj->getWrkSessionsTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   C_PROVIDER_TYPE *providerType;
   if ((providerType = pConn->getCharProviderType()) == NULL) return GS_BAD;

   //((<Name><Type><Dim><Dec><IsLong><IsFixedPrecScale><IsFixedLength><Write>)...)
   // converto campo da codice ADO in Provider dipendente (testo)
   if (pConn->Type2ProviderType(providerType->get_Type(), // DataType per campo testo
                                FALSE,                    // IsLong
                                FALSE,                    // IsFixedPrecScale
                                RTNIL,                    // IsFixedLength
                                TRUE,                     // IsWrite
                                MemoLen,                  // Size
                                0,                        // Prec
                                CharType_ProviderDescr,   // ProviderDescr
                                &CharType_Len, &CharType_Dec) == GS_BAD)
      return GS_BAD;

   // Se la tabella non ha il campo "CLASS_CODE_LIST"
   if ((Stru << pConn->ReadStruct(TableRef.get_name())) == NULL) return GS_BAD;
   i = 0;
   Values << acutBuildList(RTLB, 0);
   while ((p = Stru.nth(i++)))
   {
      p = gsc_nth(0, p);
      Values += acutBuildList(RTLB, RTSTR, p->resval.rstring, RTSTR, p->resval.rstring, RTLE, 0);
   }

   if ((NewStru << pConn->getStruForCreatingTable(Stru.get_head())) == NULL) return GS_BAD; 
   NewStru.remove_tail(); // elimino ultima tonda chiusa

   if (!Stru.Assoc(_T("CLASS_CODE_LIST")))
   {
      Values += acutBuildList(RTLB, RTSTR, _T("CLASS_CODE_LIST"), RTNIL, RTLE, 0);
      if ((NewStru += acutBuildList(RTLB,
                                       RTSTR, _T("CLASS_CODE_LIST"),
                                       RTSTR, CharType_ProviderDescr.get_name(), // <tipo>
                                       RTSHORT, MemoLen,                         // <len>
                                       RTSHORT, 0,                               // <dec>
                                    RTLE, 0)) == NULL)
         return GS_BAD;

      ToUpd = true;
   }

   if (ToUpd)
   {
      Values += acutBuildList(RTLE, 0);
      NewStru += acutBuildList(RTLE, 0);

      // modifico struttura tabella                                  
      if (pConn->UpdStruct(TableRef.get_name(), NewStru.get_head(), Values.get_head()) == GS_BAD)
         return GS_BAD;
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gsc_isCompatibGEOsimVersion                                           */
/*+
  Questa funzione verifica se la versione passata come parametro è compatibile 
  con la versione attuale di GEOsim.
  Parametri:
  const TCHAR *Version;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gs_isCompatibGEOsimVersion(void)
{
   resbuf *arg = acedGetArgs();

   acedRetNil();
   if (arg == NULL || arg->restype != RTSTR) // non è stato passato
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }  
   if (gsc_isCompatibGEOsimVersion(arg->resval.rstring) == GS_BAD) return RTNORM;
   acedRetT();

   return RTNORM;
}
int gsc_isCompatibGEOsimVersion(const TCHAR *Version)
{
   C_STRING Ver, ActVer;
   TCHAR    *pVer, *pActVer;

   if (!Version) return GS_BAD;

   Ver    = Version;
   ActVer = gsc_msg(130); // Versione attuale di GEOsim
   if (ActVer.comp(Ver, FALSE) == 0) return GS_GOOD;

   // cerco il primo punto per valutare il numero di versione principale 
   // (es "2015.0.d." = "2015")
   if ((pVer = Ver.at(_T('.')))) *pVer = _T('\0');
   if ((pActVer = ActVer.at(_T('.')))) *pActVer = _T('\0');
   if (ActVer.comp(Ver, FALSE) == 0) return GS_GOOD;
   // la versione 2015 non è compatibile con la 2013 e precedenti
   if (Ver.comp(_T("2015"), FALSE) == 0) return GS_GOOD;

   return GS_BAD;
}


/*****************************************************************************/
/*.doc gs_setActualGEOsimVersion                                             */
/*+
  Questa funzione LISP setta la versione  attuale di GEOsim di una classe 
  (da chiamare al termine della sua conversione).
  Parametri:
  (<prj><cls>)

  Restituisce T in caso di successo altrimenti restituisce NIL.
-*/  
/*****************************************************************************/
int gs_setActualGEOsimVersion(void)
{
   presbuf arg = acedGetArgs();
   int     prj, cls;

   acedRetNil();
   // Legge nella lista dei parametri: progetto e classe
   if (arg_to_prj_cls_sub(&arg, &prj, &cls) == GS_BAD) return RTERROR;
   // Setto attuale versione
   if (gsc_setActualGEOsimVersion(prj, cls) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}
int gsc_setActualGEOsimVersion(int prj, int cls)
{
   C_PROJECT      *pPrj;
   C_CLASS        *pCls;
   C_DBCONNECTION *pConn;
   C_STRING       statement, TableRef, SQLStr(gsc_msg(130)); // Versione attuale di GEOsim

   // Cerca progetto nella lista GEOsimAppl::PROJECTS
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL) return GS_BAD;
   // Ritorna il puntatore alla classe cercata
   if ((pCls = pPrj->find_class(cls)) == NULL) return GS_BAD;
   // setto il riferimento di GS_CLASS (<catalogo>.<schema>.<tabella>)
   if (pPrj->getClassesTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   pConn->Str2SqlSyntax(SQLStr);
   statement = _T("UPDATE ");
   statement += TableRef;
   statement += _T(" SET VERSION=");
   statement += SQLStr; // Versione attuale di GEOsim
   statement += _T(" WHERE GS_ID=");
   statement += cls;
   if (pConn->ExeCmd(statement) == GS_BAD) return GS_BAD;
   pCls->ptr_id()->Version = gsc_msg(130); // Versione attuale di GEOsim

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_change_group_link                   <internal> */
/*+
  Questa funzione LISP cambia i collegamenti alle classi membri di un gruppo.
  Usata ad esempio in questa situazione: una volta convertite le classi VIE, 
  CIVICI e INDIRIZZI, si effettua la copia classi perchè OLE-DB su DBASE 3 non 
  modifica i dati. Le copie VIE1, CIVICI1 e INDIRIZZI1 saranno in ACCESS.
  A questo punto per far si' che il gruppo INDIRIZZI1 punti alle nuove classi 
  VIE1 e CIVICI1 si devono cambiare i collegamenti alle classi membri di INDIRIZZI1.
  Parametri:
  Lista resbuf (<prj><cls> ((<old cls membro1><new cls membro1>)
                            (<old cls membro2><new cls membro2>)
                            ...))
  Restituisce TRUE in caso di successo altrimenti restituisce nil. 
-*/  
/*********************************************************/
int gs_change_group_link(void)
{
   resbuf         *arg = acedGetArgs();
   int            prj, cls;
   C_INT_INT_LIST List;
   C_CLASS        *pCls;

   acedRetNil();
   if (arg_to_prj_cls_sub(&arg, &prj, &cls) == GS_BAD) return RTERROR;
   if ((pCls = gsc_find_class(prj, cls)) == NULL) return RTERROR;
   if (List.from_rb(arg) == GS_BAD) return RTERROR;
   if (gsc_change_group_link(pCls, List) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}
int gsc_change_group_link(C_CLASS *pCls, C_INT_INT_LIST &NewLinkList)
{
   C_INT_INT_LIST List;
   C_INT_INT      *pItem;
   C_DBCONNECTION *pConn, *pClsConn;
   _RecordsetPtr  pRs;
   presbuf        pLinkClsId;
   C_RB_LIST      ColValues;
   int            LinkClsId, res;
   C_STRING       TableRef, LnkTableRef, statement;

   if (!pCls->ptr_group_list()) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (((C_PROJECT *) pCls->ptr_id()->pPrj)->getGrpsTabInfo(&pConn, &TableRef) == GS_BAD)
      return GS_BAD;

   // ricavo connessione OLE-DB
   if ((pClsConn = pCls->ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
   // ricavo tabella REL OLD
   if (pCls->getOldLnkTableRef(LnkTableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += pCls->ptr_id()->code;

   int IsTransactionSupported = pConn->BeginTrans();

   if (pConn->ExeCmd(statement, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); return GS_BAD; }
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
   {
      gsc_DBCloseRs(pRs); 
      if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans();
      return GS_BAD; 
   }
   pLinkClsId = ColValues.CdrAssoc(_T("LNK_CL_ID"));

   res = GS_GOOD;
   // scorro l'elenco dei collegamenti
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      // codice della classe collegata 
      if (gsc_rb2Int(pLinkClsId, &LinkClsId) == GS_BAD) { res = GS_BAD; break; }
      if ((pItem = (C_INT_INT *) NewLinkList.search_key(LinkClsId)))
      {  // aggiorno il record
         gsc_RbSubst(pLinkClsId, pItem->get_type());
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      }
      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs); 
   if (res == GS_BAD)
      { if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); return GS_BAD; }

   statement = _T("SELECT * FROM ");
   statement += LnkTableRef;

   int IsTransactionSupportedCls = pClsConn->BeginTrans();

   if (pClsConn->ExeCmd(statement, pRs, adOpenDynamic, adLockOptimistic) == GS_BAD)
   {
      if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans(); 
      if (IsTransactionSupportedCls == GS_GOOD) pClsConn->RollbackTrans();
      return GS_BAD; 
   }
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
   {
      gsc_DBCloseRs(pRs);
      if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans();
      if (IsTransactionSupportedCls == GS_GOOD) pClsConn->RollbackTrans();
      return GS_BAD; 
   }
   pLinkClsId = ColValues.CdrAssoc(_T("CLASS_ID"));

   res = GS_GOOD;
   // scorro l'elenco dei collegamenti
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      // codice della classe collegata 
      if (gsc_rb2Int(pLinkClsId, &LinkClsId) == GS_BAD) { res = GS_BAD; break; }
      if ((pItem = (C_INT_INT *) NewLinkList.search_key(LinkClsId)))
      {  // aggiorno il record
         gsc_RbSubst(pLinkClsId, pItem->get_type());
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { res = GS_BAD; break; }
      }
      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);
   if (res == GS_BAD)
   {
      if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans();
      if (IsTransactionSupportedCls == GS_GOOD) pClsConn->RollbackTrans();
      return GS_BAD;
   }

   if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();
   if (IsTransactionSupported == GS_GOOD) pClsConn->CommitTrans();

   return GS_GOOD;
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI PER AGGIORNAMENTO STRUTTURA SIMULAZIONE GAS
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gs_UpdateGasSimStru                    <external> */
/*+
  Questa funzione LISP aggiorna la struttura delle simulazioni gas.
  Per la sottoclasse "regolatore" deve essere inserito un nuovo attributo 
  nominato "MAX_EROGAZIONE"; inoltre deve essere inserita una nuova sottoclasse
  nominata PRV.
  La funzione è da da chiamare in fase di conversione della classe da GS8 a GS2011.
  Parametri:
  (<prj><cls>)
  
  Restituisce T in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_UpdateGasSimStru(void)
{
   presbuf        arg = acedGetArgs();
   C_EXTERN       *pCls;
   C_SUB          *pSub;
   int            prj, cls;
   C_DBCONNECTION *pConn;
   bool           toUpdate  = false;

   acedRetNil();
   // Legge nella lista dei parametri: progetto e classe
   if (arg_to_prj_cls_sub(&arg, &prj, &cls) == GS_BAD) return RTERROR;

   if ((pCls = (C_EXTERN *) gsc_find_class(prj, cls)) == NULL) return RTERROR;

   if (pCls->get_category() != CAT_EXTERN)
      { GS_ERR_COD = eGSInvClassType; return RTERROR; }

   // Se non si tratta di gas bassa (2) e neanche gas media (3)
   if (pCls->get_type() != 2 && pCls->get_type() != 3)
      { GS_ERR_COD = eGSInvClassType; return RTERROR; }
   // Cerca la sotto-classe regolatore
   if ((pSub = (C_SUB *) pCls->ptr_sub_list()->search_key(3)) == NULL)
      { GS_ERR_COD = eGSClassNotFound; return RTERROR; }
   // Connessione a DB
   pConn = pSub->ptr_info()->getDBConnection(OLD);

   // Verifico esistenza attributo "MAX_EROGAZIONE"
   if (pSub->ptr_attrib_list()->search_name(_T("MAX_EROGAZIONE"), FALSE) == NULL)
   {
      C_ATTRIB_LIST NewAttribList;
      C_ATTRIB      *pNewAttrib;
      C_STRING      DoubleProviderDescr;
      long          DoubleProviderLen;
      int           DoubleProviderDec;

      // converto campo da codice ADO in Provider dipendente
      if (pConn->Type2ProviderType(adDouble,      // DataType
                                   FALSE,         // IsLong
                                   FALSE,         // IsFixedPrecScale
                                   RTT,           // IsFixedLength
                                   TRUE,          // IsWrite
                                   20,            // Size
                                   8,             // Prec
                                   DoubleProviderDescr, // ProviderDescr
                                   &DoubleProviderLen, &DoubleProviderDec) == GS_BAD)
      return GS_BAD;

      if ((pNewAttrib = new C_ATTRIB()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return RTERROR; }
      pNewAttrib->name = _T("MAX_EROGAZIONE");
      gsc_strcpy(pNewAttrib->type, DoubleProviderDescr.get_name(), 25);
      pNewAttrib->len = DoubleProviderLen;
      pNewAttrib->dec = 2;
      pNewAttrib->valid_func = gsc_tostring(_T("(or (= max_erogazione -1) (> max_erogazione 0))"));
      pNewAttrib->def = acutBuildList(RTSHORT, -1, 0);
      pNewAttrib->mod_attr = 904;
      pNewAttrib->Descr = _T("mc/h (-1 = Nessun controllo)");
      NewAttribList.add_tail(pNewAttrib);

      if (gsc_alloc_mod_stru(prj, cls, 3, 0) == GS_BAD) return RTERROR;
      if (gsc_add_attr(&NewAttribList) == GS_BAD)
         { gsc_dealloc_GS_RESBUF(); return RTERROR; }
      toUpdate = true;
   }
   // Verifico esistenza attributo "PRESSIONEE"
   if (pSub->ptr_attrib_list()->search_name(_T("PRESSIONEE"), FALSE) == NULL)
   {
      C_ATTRIB_LIST NewAttribList;
      C_ATTRIB      *pNewAttrib;
      C_STRING      DoubleProviderDescr;
      long          DoubleProviderLen;
      int           DoubleProviderDec;

      // converto campo da codice ADO in Provider dipendente
      if (pConn->Type2ProviderType(adDouble,      // DataType
                                   FALSE,         // IsLong
                                   FALSE,         // IsFixedPrecScale
                                   RTT,           // IsFixedLength
                                   TRUE,          // IsWrite
                                   20,            // Size
                                   8,             // Prec
                                   DoubleProviderDescr, // ProviderDescr
                                   &DoubleProviderLen, &DoubleProviderDec) == GS_BAD)
      return GS_BAD;

      if ((pNewAttrib = new C_ATTRIB()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return RTERROR; }
      pNewAttrib->name = _T("pressioneE");
      gsc_strcpy(pNewAttrib->type, DoubleProviderDescr.get_name(), 25);
      pNewAttrib->len = DoubleProviderLen;
      pNewAttrib->dec = 2;
      pNewAttrib->def = acutBuildList(RTSHORT, 0, 0);
      pNewAttrib->mod_attr = 904; // non si può modificare l'obbligatorietà + i ltipo. il nome e non può essere canellato

      if (pCls->get_type() == 2) // Se si tratta di gas bassa (2)
         pNewAttrib->Descr = _T("mm h2o (pressione di erogazione)");
      else if (pCls->get_type() != 3)// Se si tratta di gas media (3)
         pNewAttrib->Descr = _T("kg/cmq (pressione di erogazione)");
      NewAttribList.add_tail(pNewAttrib);

      if (!toUpdate)
         if (gsc_alloc_mod_stru(prj, cls, 3, 0) == GS_BAD) return RTERROR;
      if (gsc_add_attr(&NewAttribList) == GS_BAD)
         { gsc_dealloc_GS_RESBUF(); return RTERROR; }
      toUpdate = true;
   }

   if (toUpdate)
   {
      // Aggiorno la struttura
      if (pSub->mod_stru() == GS_BAD)
         { gsc_dealloc_GS_RESBUF(); return RTERROR; }
      gsc_dealloc_GS_RESBUF();
   }

   // Cerca la sotto-classe PRV 
   if ((pSub = (C_SUB *) pCls->ptr_sub_list()->search_key(5)) == NULL)
   {  // Aggiungo nuova sottoclasse
      C_SUB     *pSubNode;
      C_PROFILE_SECTION_BTREE ProfileSections;
      C_RB_LIST SimDescrList, ColValues;
      presbuf   lista_sub, pPriSub, pType, pLinkSub;
      C_STRING  filename, SubTableRef, Prefix, Catalog, Schema, TableRef;
      TCHAR     *cod36 = NULL;
      C_GPH_INFO *pGraphInfo;

      // Leggo la sottoclasse nodo
      if ((pSubNode = (C_SUB *) pCls->ptr_sub_list()->search_key(1)) == NULL)
         return RTERROR;
      pSubNode->ptr_info()->copy(pCls->ptr_info());
      pGraphInfo = pSubNode->ptr_GphInfo();

      if (pCls->get_file_stru(filename) == GS_BAD) return RTERROR;
      if (gsc_read_profile(filename, ProfileSections) == GS_BAD) return RTERROR;

      // Ricopia i dati dalla struttura di ingresso
      if ((SimDescrList << pCls->read_stru(ProfileSections)) == NULL)
         return RTERROR;

      lista_sub = SimDescrList.nth(2);

      if ((pSub = new C_SUB) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return RTERROR; }
      if (pSub->from_rb(gsc_nth(4, lista_sub)) == GS_BAD)
         { delete pSub; return RTERROR; }
      pSub->ptr_id()->pPrj = pCls->ptr_id()->pPrj;
      pSub->ptr_id()->code = pCls->ptr_id()->code;

      pCls->ptr_info()->copy(pSub->ptr_info());
      pSub->allocGraphInfo(pGraphInfo->getDataSourceType());
      pGraphInfo->copy(pSub->ptr_GphInfo());
      pSub->ptr_info()->TempLastId = pSubNode->ptr_info()->TempLastId;

      // Leggo catalogo e schema
      if ((pConn = pSubNode->ptr_info()->getDBConnection(OLD)) == NULL) { delete pSub; return RTERROR; }
      if (pConn->split_FullRefTable(pSubNode->ptr_info()->OldTableRef,
                                    Catalog, Schema, Prefix) == GS_BAD)
         { delete pSub; return RTERROR; }
      if (gsc_long2base36(&cod36, pCls->ptr_id()->pPrj->get_key(), 2) == GS_BAD)
         { delete pSub; return RTERROR; }
      Prefix = _T("S");
      Prefix += cod36;
      free(cod36); cod36 = NULL;
      if (gsc_long2base36(&cod36, pCls->ptr_id()->code, 3) == GS_BAD)
         { delete pSub; return RTERROR; }
      Prefix += cod36; 
      free(cod36);
      // Il nome della tabella è come quello dei nodi con il numero cambiato
      SubTableRef = Prefix;
      SubTableRef += 5;
      if (pSub->ptr_info()->OldTableRef.paste(pConn->get_FullRefTable(Catalog, Schema, SubTableRef)) == NULL)
         { delete pSub; return RTERROR; }

      if (pSub->ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource)
      {
         C_DBGPH_INFO *pGphInfo = (C_DBGPH_INFO *) pSub->ptr_GphInfo();
         // Leggo catalogo e schema
         if ((pConn = pGphInfo->getDBConnection()) == NULL) { delete pSub; return RTERROR; }
         if (pConn->split_FullRefTable(pGphInfo->TableRef,
                                       Catalog, Schema, Prefix) == GS_BAD)
            { delete pSub; return RTERROR; }
         if (gsc_long2base36(&cod36, pCls->ptr_id()->pPrj->get_key(), 2) == GS_BAD)
            { delete pSub; return RTERROR; }
         Prefix = _T("S");
         Prefix += cod36;
         free(cod36); cod36 = NULL;
         if (gsc_long2base36(&cod36, pCls->ptr_id()->code, 3) == GS_BAD)
            { delete pSub; return RTERROR; }
         Prefix += cod36; 
         free(cod36);
         // Il nome della tabella è come quello dei nodi con il numero cambiato
         SubTableRef = Prefix;
         SubTableRef += 5;
         SubTableRef += _T("_geom");
         if (pGphInfo->TableRef.paste(pConn->get_FullRefTable(Catalog, Schema, SubTableRef)) == NULL)
            { delete pSub; return RTERROR; }
      }

      if (pSub->create() == GS_BAD)
         { delete pSub; return RTERROR; }

      pCls->ptr_sub_list()->add_tail(pSub);

      pCls->reindexTab(); // ricalcolo elast

      // setto il riferimento di GS_CONCT (<catalogo>.<schema>.<tabella>)
      ((C_PROJECT *) pCls->ptr_id()->pPrj)->getConnectionsTabInfo(&pConn, &TableRef);
      if ((ColValues << acutBuildList(RTLB,
                                      RTLB, RTSTR, _T("CLASS_ID"),  RTSHORT, cls, RTLE,
                                      RTLB, RTSTR, _T("SUB_CL_ID"), RTSHORT, 0, RTLE,
                                      RTLB, RTSTR, _T("TYPE"),      RTSHORT,  0, RTLE,
                                      RTLB, RTSTR, _T("LNK_CL_ID"), RTSHORT,  cls, RTLE,
                                      RTLB, RTSTR, _T("LNK_SUB_ID"), RTSHORT, 0, RTLE,
                                      RTLE, 0)) == NULL)
         return GS_BAD;
      pPriSub  = ColValues.CdrAssoc(_T("SUB_CL_ID"));
      pType    = ColValues.CdrAssoc(_T("TYPE"));
      pLinkSub = ColValues.CdrAssoc(_T("LNK_SUB_ID"));

      // Modifico regole di connessione per i restanti componenti
      if ((pSub = (C_SUB *) pCls->ptr_sub_list()->search_key(1)) != NULL)
      {
         gsc_RbSubst(pPriSub, 1);   // quale sotto-classe
         gsc_RbSubst(pType, 64);    // tipo di connessione
         gsc_RbSubst(pLinkSub, 5);  // con quale sotto-classe
         pConn->InsRow(TableRef.get_name(), ColValues);
      }
      if ((pSub = (C_SUB *) pCls->ptr_sub_list()->search_key(3)) != NULL)
      {
         gsc_RbSubst(pPriSub, 3);   // quale sotto-classe
         gsc_RbSubst(pType, 64);    // tipo di connessione
         gsc_RbSubst(pLinkSub, 5);  // con quale sotto-classe
         pConn->InsRow(TableRef.get_name(), ColValues);
      }
      if ((pSub = (C_SUB *) pCls->ptr_sub_list()->search_key(4)) != NULL)
      {
         gsc_RbSubst(pPriSub, 4);   // quale sotto-classe
         gsc_RbSubst(pType, 64);    // tipo di connessione
         gsc_RbSubst(pLinkSub, 5);  // con quale sotto-classe
         pConn->InsRow(TableRef.get_name(), ColValues);
      }
   }

   return RTNORM;
}


///////////////////////////////////////////////////////////////////////////////
// FUNZIONI PER AGGIORNAMENTO STRUTTURA SIMULAZIONE
///////////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc gs_UpdateIrenSimStru                        <external> */
/*+
  Questa funzione LISP aggiorna la struttura delle simulazioni di Iren dei progetti.
  Per la sottoclasse "valvola" deve essere rinominato l'attributo "ON_OFF" in "CHIUSA".
  La funzione è da da chiamare in fase di conversione della classe da GS2013 a GS2015.
  
  Restituisce T in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_UpdateIrenSimStru(void)
{
   presbuf arg = acedGetArgs();
   int     prj, cls;
   
   acedRetNil();
   // Legge nella lista dei parametri: progetto e classe
   if (arg_to_prj_cls_sub(&arg, &prj, &cls) == GS_BAD) return RTERROR;

   if (gsc_updateIrenSimStru(prj, cls) != GS_GOOD)
   {
      C_CLASS *pCls;
      if ((pCls = gsc_find_class(prj, cls)) == NULL) return RTERROR;
      acutPrintf(gsc_msg(1092), pCls->get_name(), pCls->get_pPrj()->get_name()); // "\nConversione simulazione <%s> del progetto <%s> fallita."
      return RTERROR;
   }

   return RTNORM;
}


/*****************************************************************************/
/*.doc gsc_updateIrenSimStru                                                 */
/*+
  Questa funzione LISP aggiorna la struttura della classe simulazione di Iren.
  Per la sottoclasse "valvola" deve essere rinominato l'attributo "ON_OFF" in "CHIUSA".
  La funzione è da da chiamare in fase di conversione della classe da GS2013 a GS2015.
  Parametri:
  int prj;     Codice progetto
  int cls;     codice classe

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_updateIrenSimStru(int prj, int cls)
{
   C_EXTERN *pCls;
   C_SUB    *pSub;
   int      valvSubId;

   if ((pCls = (C_EXTERN *) gsc_find_class(prj, cls)) == NULL) return GS_BAD;

   if (pCls->get_category() != CAT_EXTERN)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // Se non si tratta di h2o (1) e gas bassa (2) e neanche gas media (3)
   switch (pCls->get_type())
   {
      case 1: // Se si tratta di h2o (1)
         valvSubId = 5;
         break;
      case 2: case 3: // Se si tratta di gas bassa (2) o gas media (3)
         valvSubId = 4;
         break;
      default:
         GS_ERR_COD = eGSInvClassType;
         return GS_BAD;
   }

   // Cerca la sotto-classe valvola
   if ((pSub = (C_SUB *) pCls->ptr_sub_list()->search_key(valvSubId)) == NULL)
      { GS_ERR_COD = eGSClassNotFound; return GS_BAD; }

   // Verifico esistenza attributo "CHIUSA"
   if (pSub->ptr_attrib_list()->search_name(_T("CHIUSA"), FALSE) == NULL)
   {
      C_ATTRIB  *pAttrib;
      C_RB_LIST lista;
      int       mod_attr;

      if ((pAttrib = (C_ATTRIB *) pSub->ptr_attrib_list()->search_name(_T("ON_OFF"), FALSE)) == NULL)
         return GS_BAD;

      lista << pAttrib->to_rb();
      if (lista.CdrAssocSubst(_T("ATTRIB"), _T("CHIUSA")) != GS_GOOD)
         return GS_BAD;
      if (lista.CdrAssocSubst(_T("CAPTION"), _T("CHIUSA")) != GS_GOOD)
         return GS_BAD;
      lista.link_head(acutBuildList(RTLB, RTLB, RTSTR, _T("ON_OFF"), 0));
      lista += acutBuildList(RTLE, RTLE, 0);

      // modifico mod_attr per permettere di modificare il nome che altrimenti sarebbe vietato
      mod_attr = pAttrib->mod_attr;
      if (pAttrib->mod_attr & BIT_MOD_NAME) pAttrib->mod_attr -= BIT_MOD_NAME;

      if (gsc_alloc_mod_stru(prj, cls, valvSubId, 0) == GS_BAD)
         { pAttrib->mod_attr = mod_attr; gsc_dealloc_GS_RESBUF(); return GS_BAD; }

      pAttrib->mod_attr = mod_attr; // ripristino il valore originale

      if (gsc_mod_attr(lista.get_head()) == GS_BAD)
         { gsc_dealloc_GS_RESBUF(); return GS_BAD; }

      // Aggiorno la struttura
      if (pSub->mod_stru() == GS_BAD)
         { gsc_dealloc_GS_RESBUF(); return GS_BAD; }
      gsc_dealloc_GS_RESBUF();
   }

   return GS_GOOD;
}


/*****************************************************************************/
/*.doc gs_PrjMainTabsConv2DB                                                    */
/*+                                                           
  Questo funzione LISP converte le tabelle principali di GEOsim
  da ACCESS a un altro DB (es. PostGIS o ORACLE).
  Suggerimento: (gs_MainTabsConv2DB <codice progetto> (gsui_DBConn))
  Parametri:
  Lista RESBUF 
  <prj>(<ConnStrUDLFile>[<UDLProperties>])
  
  Ritorna TRUE in caso di successo altrimenti NIL.
-*/  
/*****************************************************************************/
int gs_MainTabsConv2DB(void)
{
   resbuf *arg = acedGetArgs();
   int    prj;
   C_INFO info;

   acedRetNil();

   if (gsc_rb2Int(arg, &prj) == GS_BAD)
      { GS_ERR_COD = eGSInvalidPrjCode; return RTERROR; }
   // DBConnection delle tabelle interne
   if (info.from_rb(arg->rbnext) == GS_BAD) return RTERROR;

   if (gsc_MainTabsConv2DB(prj, info.ConnStrUDLFile, info.UDLProperties) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}


/*****************************************************************************/
/*.doc gsc_MainTabsConv2DB                                                   */
/*+
  Questo funzione LISP converte le tabelle principali di GEOsim
  da ACCESS a un altro DB (es. PostGIS o ORACLE).
  Parametri:
  int         prj;               Codice progetto
  C_STRING    &ConnStrUDLFile;   Stringa di connessione o File di tipo .UDL
  C_2STR_LIST &UDLProperties;    Lista di proprietà UDL 

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*****************************************************************************/
int gsc_MainTabsConv2DB(int prj, C_STRING &ConnStrUDLFile, C_2STR_LIST &UDLProperties)
{
   C_PROJECT      *pPrj;
   int            result = GS_GOOD, i;
   C_STR_LIST     TableList;
   C_STR          *pTable;
   C_STRING       Catalog, Schema, Table, NewTableRef, statement;
   C_DBCONNECTION *pSourceConn, *pDestConn;
   C_RB_LIST      SrcColValues, DstColValues;
   presbuf        pRb;
   _RecordsetPtr  pInsRs, pRs;

   // solo per superutenti
   if (gsc_check_op(opConv2ActualVer) == GS_BAD) return GS_BAD;

   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

   // verifico esistenza sessioni di lavoro
   if (pPrj->is_free() != GS_GOOD)
   {
      acutPrintf(gsc_msg(445), pPrj->get_name()); // "\nConversione progetto %s fallita."
      acutPrintf(_T(" %s"), gsc_msg(607)); // "Trovate sessioni di lavoro..."
      GS_ERR_COD = eGSSessionsFound;
      return GS_BAD;
   }

   // Leggo la tabelle campione e la connessione OLEDB al DB campione
   if (gsc_getPRJSampleTables(TableList, &pSourceConn) == GS_BAD) return GS_BAD;

   pDestConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name(), &UDLProperties);

   int IsTransactionSupported = pDestConn->BeginTrans();

   do
   {
      // Copio tutte le tabelle leggendo le strutture dal database "campione" ACCESS
      result = GS_GOOD;
      pTable = (C_STR *) TableList.get_head();
      while (pTable)
      {
         if (pSourceConn->split_FullRefTable(pTable->get_name(),
                                             Catalog, Schema, Table) == GS_BAD)
            { result = GS_BAD; break; }
         if (NewTableRef.paste(pDestConn->get_FullRefTable(GEOPRJCATALOG, NULL, Table.get_name())) == NULL)
            { result = GS_BAD; break; }

         if (pSourceConn->CopyTable(pTable->get_name(), 
                                    *pDestConn, NewTableRef.get_name(),
                                    GSStructureOnlyCopyType) == GS_BAD)
            { result = GS_BAD; break; }

         pTable = (C_STR *) TableList.get_next();
      }

      if (result == GS_BAD) break;
      result = GS_BAD;

      // Mi connetto al db del progetto
      if ((pSourceConn = pPrj->getDBConnection()) == NULL) break;

      // Copio i dati di tutte le tabelle
      result = GS_GOOD;
      pTable = (C_STR *) TableList.get_head();
      while (pTable)
      {
         if (pSourceConn->split_FullRefTable(pTable->get_name(),
                                             Catalog, Schema, Table) == GS_BAD)
            { result = GS_BAD; break; }
         if (NewTableRef.paste(pDestConn->get_FullRefTable(GEOPRJCATALOG, NULL, Table.get_name())) == NULL)
            { result = GS_BAD; break; }

         acutPrintf(_T("%s %s..."), gsc_msg(594), NewTableRef.get_name());  // "\nConversione"

         // preparo RecordSet per l'inserimento di record nella tabella
         if (pDestConn->InitInsRow(NewTableRef.get_name(), pInsRs) == GS_BAD) { result = GS_BAD; break; }
         if (gsc_InitDBReadRow(pInsRs, DstColValues) == GS_BAD)
            { gsc_DBCloseRs(pInsRs); result = GS_BAD; break; }

         statement = _T("SELECT * FROM ");
         statement += pTable->get_name();
         if (pSourceConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockReadOnly, ONETEST) == GS_BAD)
            { gsc_DBCloseRs(pInsRs); result = GS_BAD; break; }
         if (gsc_InitDBReadRow(pRs, SrcColValues) == GS_BAD)
            { gsc_DBCloseRs(pInsRs); gsc_DBCloseRs(pRs); result = GS_BAD; break; }

         while (gsc_isEOF(pRs) == GS_BAD)
         {
            if (gsc_DBReadRow(pRs, SrcColValues) == GS_BAD) { result = GS_BAD; break; }

            i = 0;
            while ((pRb = SrcColValues.nth(i++)) != NULL)
               DstColValues.CdrAssocSubst(pRb->rbnext->resval.rstring,
                                          pRb->rbnext->rbnext);

            if (gsc_DBInsRow(pInsRs, DstColValues) == GS_BAD)
               { result = GS_BAD; break; }

            gsc_Skip(pRs);
         }

         gsc_DBCloseRs(pRs);
         gsc_DBCloseRs(pInsRs);

         if (result == GS_BAD) break;

         pTable = (C_STR *) TableList.get_next();
      }
      if (result == GS_BAD) break;
      result = GS_BAD;

      // aggiorno il progetto nel DB e in memoria
      if (gsc_getPrjTabInfo(&pSourceConn, &NewTableRef) == GS_BAD) break;

      Catalog = _T("SELECT * FROM ");
      Catalog += NewTableRef;
      Catalog += _T(" WHERE GS_ID=");
      Catalog += prj;

      if ((DstColValues << acutBuildList(RTLB, 
                                         RTLB, RTSTR, _T("UDL_FILE"), 0)) == NULL)
         break;
      if ((DstColValues += gsc_str2rb(ConnStrUDLFile)) == NULL) break;
      if ((DstColValues += acutBuildList(RTLE, RTLB, RTSTR, _T("UDL_PROP"), 0)) == NULL) break;
      Schema.paste(gsc_PropListToConnStr(UDLProperties));
      if (Schema.len() > 0)
         // Conversione path UDLProperties da dir relativo in assoluto
         if (pSourceConn->UDLProperties_drive2nethost(Schema) == GS_BAD) return GS_BAD;
      if ((DstColValues += gsc_str2rb(Schema)) == NULL) break;
      if ((DstColValues += acutBuildList(RTLE, RTLE, 0)) == NULL) break;

      // modifico riga in GS_PRJ
      // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
      // in una transazione fa casino (al secondo recordset che viene aperto)
      if (pSourceConn->OpenRecSet(Catalog, pRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD) break;
      if (gsc_DBUpdRow(pRs, DstColValues) == GS_BAD) { gsc_DBCloseRs(pRs); break; }
      if (gsc_DBCloseRs(pRs) == GS_BAD) break;

      pPrj->set_ConnStrUDLFile(ConnStrUDLFile.get_name());
      pPrj->set_UDLProperties(UDLProperties);
      result = GS_GOOD;
   }
   while (0);

   if (result == GS_BAD)
   {
      int error_code = GS_ERR_COD;

      if (IsTransactionSupported == GS_GOOD) pDestConn->RollbackTrans();
      GS_ERR_COD = error_code;
      return GS_BAD;
   }

   if (IsTransactionSupported == GS_GOOD) pDestConn->CommitTrans();

   return GS_GOOD;
}