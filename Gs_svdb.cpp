/**********************************************************
Name: GS_SVDB.CPP
                                   
Module description: File funzioni di base per il salvataggio. 
            
Author: Roberto Poltini

(c) Copyright 1995-2015 by IREN ACQUA GAS  S.p.A.

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

#include "..\gs_def.h" // definizioni globali
#include "gs_error.h"
#include "gs_opcod.h"

#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_list.h"
#include "gs_ase.h"
#include "gs_init.h"
#include "gs_user.h"
#include "gs_class.h"
#include "gs_sec.h"
#include "gs_prjct.h"
#include "gs_area.h"
#include "gs_graph.h"
#include "gs_attbl.h"
#include "gs_query.h"
#include "gs_lisp.h"
#include "gs_svdb.h"     // salvataggio database
#include "gs_dwg.h"      // gestione disegni
#include "gs_topo.h"
#include "gs_ade.h"

   #include <sys/timeb.h> // test
   #include <time.h>


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/

/*************************************************************************/
/* PRIVATE FUNCTIONS                                                     */
/*************************************************************************/

int gsc_is_completed_save(C_INT_INT_LIST *lista_cls);
int gsc_is_saveable(C_INT_INT_LIST *lista_cls, C_INT_INT *cls);
int gsc_mod_reldata(C_PREPARED_CMD_LIST &list_mother, int cls, long gs_id, long new_gs_id);

// per tabelle secondarie
int gsc_prepare_del_old_rel_sec_where_key_pri(C_SECONDARY *pSec, _CommandPtr &pCmd);
int gsc_prepare_del_old_rel_sec_where_key_attrib(C_SECONDARY *pSec, _CommandPtr &pCmd);
int gsc_do_save_sec(C_CLASS *pCls);
int gsc_mod_reldata_sec(C_PREPARED_CMD_LIST &list_mother, int Cls, int Sub,
                        long Key, long NewKey, int Mode = MODIFIED, long *qty = NULL);
int gsc_prepare_del_old_sec_where_key_pri(C_SECONDARY *pSec, _CommandPtr &pCmd);
int gsc_GetListDelStmOldSec(C_CLASS *pCls, C_PREPARED_CMD_LIST &CmdList);
int gsc_GetListDelStmOldRelSec(C_CLASS *pCls, C_PREPARED_CMD_LIST &CmdList);

void cls_print_save_report(long del_ent, long upd_ent, long new_ent);
void sec_print_save_report(long del_ent, long upd_ent, long new_ent);


/*********************************************************/
/*.doc (new 2) gsc_save <internal> */
/*+                                                                       
  Effettua il salvataggio delle classi indicate.
  Parametri:
  C_INT_LIST &ClsCodeList;	Lista dei codici delle classi da salvare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_save(C_INT_LIST &ClsCodeList)
{
   C_INT_INT_LIST class_list_to_save;
   C_INT_INT		*p_class_to_save;
   C_CLASS        *pCls;
   int			   times = 0, conf = GS_BAD, error_code;
   struct resbuf  undoctl;
   C_INT_LIST     NotifyClsList, SavedClsList;
   C_INT          *pNotifyCls;
   TCHAR          Msg[256];

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   if (GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) != GS_GOOD) return GS_BAD;

   // "Messaggio GEOsim: La sessione n°.%ld ha terminato il salvataggio."
   swprintf(Msg, 256, gsc_msg(276), GS_CURRENT_WRK_SESSION->get_id());

   acutPrintf(gsc_msg(123));    // "\nInizio salvataggio..."
   gsc_write_log(_T("Saving started."));

   // ordino le classi per livello di complessità
   GS_CURRENT_WRK_SESSION->get_pPrj()->ptr_classlist()->sort_for_extraction();

   pCls = (C_CLASS *) GS_CURRENT_WRK_SESSION->get_pPrj()->ptr_classlist()->get_head();
   while (pCls)
   {  // se è da salvare
      if (ClsCodeList.search_key(pCls->ptr_id()->code))
      {
         if ((p_class_to_save = new C_INT_INT) == NULL) 
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   		p_class_to_save->set_key(pCls->ptr_id()->code);
   		p_class_to_save->set_type(DESELECTED);

         class_list_to_save.add_tail(p_class_to_save);
      }

      pCls = (C_CLASS *) pCls->get_next();
   }

   do
   {
      // ciclo su classi da salvare
      while (gsc_is_completed_save(&class_list_to_save) != TRUE &&
             ++times <= GEOsimAppl::GLOBALVARS.get_NumTest())
      {
         p_class_to_save = (C_INT_INT *) class_list_to_save.get_head();
      
         while (p_class_to_save)
         {
            if (gsc_is_saveable(&class_list_to_save, p_class_to_save) == TRUE)
		      {
               // Ritorna il puntatore alla classe cercata
               if ((pCls = GS_CURRENT_WRK_SESSION->find_class(p_class_to_save->get_key())) == NULL)
                  return GS_BAD;

               // eseguo il salvataggio della classe
               if (pCls->save() == GS_GOOD)
               {
                  p_class_to_save->set_type(SELECTED); // la segno come salvata
                  // Aggiungo alla lista delle classi salvate
                  if ((pNotifyCls = new C_INT(pCls->ptr_id()->code)) != NULL)
                     SavedClsList.add_tail(pNotifyCls);
                  // La elimino dalla lista delle NON salvate
                  NotifyClsList.remove_key(pCls->ptr_id()->code);
               }
               else
               {
                  if (GS_ERR_COD == eGSLockBySession || 
                      GS_ERR_COD == eGSClassInUse || 
                      GS_ERR_COD == eGSClassLocked) // Se bloccata da un'altra sessione
                     if (NotifyClsList.search_key(pCls->ptr_id()->code) == NULL)
                        // Aggiungo alla lista delle NON salvate
                        if ((pNotifyCls = new C_INT(pCls->ptr_id()->code)) != NULL)
                           NotifyClsList.add_tail(pNotifyCls);

	               p_class_to_save->set_type(DESELECTED); // la segno come non salvata
                  gsc_write_log(gsc_err(GS_ERR_COD));
               }
		      }
            p_class_to_save = (C_INT_INT *) p_class_to_save->get_next();
         }
      }

      // se alcune classi non sono state salvate chiedere se si vuole riprovare
      if (gsc_is_completed_save(&class_list_to_save) == TRUE) break;

      acutPrintf(gsc_msg(327)); // \nLe seguenti classi non sono state salvate correttamente:
      p_class_to_save = (C_INT_INT *) class_list_to_save.get_head();     
      while (p_class_to_save)
      {
   	   if (p_class_to_save->get_type() == DESELECTED) //  non salvata
            acutPrintf(_T("\n%s"),
                       GS_CURRENT_WRK_SESSION->find_class(p_class_to_save->get_key())->ptr_id()->name);
         p_class_to_save = (C_INT_INT *) p_class_to_save->get_next();
      }

      // "Alcune classi non sono state salvate. Ripetere il salvataggio ?"
      gsc_ddgetconfirm(gsc_msg(172), &conf);
      if (conf == GS_BAD)
      {
         error_code = GS_ERR_COD;

         if (NotifyClsList.get_count() > 0)
            // Se le classi NON sono già in attesa di notifica
            if (gsc_isWaitingForSave(GS_CURRENT_WRK_SESSION->get_PrjId(), NotifyClsList) == GS_BAD)
            {
               // "Si vuole ricevere un avviso dalle altre sessioni di lavoro ?"
               gsc_ddgetconfirm(gsc_msg(269), &conf);     

               if (conf == GS_GOOD)
                  gsc_setWaitForSave(GS_CURRENT_WRK_SESSION->get_PrjId(), NotifyClsList);
            }


         // se UNDO era abilitato resetto la storia degli undo perchè
         // non si può gestire l'undo del salvataggio anche se parziale
         if (acedGetVar(_T("UNDOCTL"), &undoctl) == RTNORM)
         {
            if (undoctl.resval.rint & 1)
            {
               gsc_ClearUndoStack();
               // "\nLe operazioni effettuate fino ad ora non saranno più annullabili."
               acutPrintf(gsc_msg(597)); 
            }
         }

         // Notifico a chi doveva salvare ed estrarre la fine del salvataggio
         gsc_NotifyWaitForExtraction(GS_CURRENT_WRK_SESSION->get_PrjId(), SavedClsList, Msg);
         gsc_NotifyWaitForSave(GS_CURRENT_WRK_SESSION->get_PrjId(), SavedClsList, Msg);
         GS_ERR_COD = error_code;
         
         return GS_BAD;
      }

      times = 0;
   }
   while (1);

   // se UNDO era abilitato resetto la storia degli undo perchè
   // non si può gestire l'undo del salvataggio
   if (acedGetVar(_T("UNDOCTL"), &undoctl) == RTNORM)
   {
      if (undoctl.resval.rint & 1)
      {
         gsc_ClearUndoStack();
         // "\nLe operazioni effettuate fino ad ora non saranno più annullabili."
         acutPrintf(gsc_msg(597)); 
      }
   }

   // Notifico a chi doveva salvare ed estrarre la fine del salvataggio
   gsc_NotifyWaitForExtraction(GS_CURRENT_WRK_SESSION->get_PrjId(), SavedClsList, Msg);
   gsc_NotifyWaitForSave(GS_CURRENT_WRK_SESSION->get_PrjId(), SavedClsList, Msg);

   if (times > GEOsimAppl::GLOBALVARS.get_NumTest()) return GS_BAD;
   acutPrintf(gsc_msg(111));    // "\nTerminato.\n"
   gsc_write_log(_T("Saving terminated."));

   return GS_GOOD;
}


/////////////////////////////////////////////////////////////////////////
/////////////        FUNZIONI PER C_CLASS    INIZIO            //////////
/////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_CLASS::save                          <internal> */
/*+                                                                       
  Effettua il salvataggio della classe.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::save(void)
{
   int        Result, Locked;
   int        What = 99; // valore impossibile cioè non bisogna fare alcun backup
   resbuf     CreateBackupFileOfSourceDwg, *PrevCreateBackupFileOfSourceDwg;
   bool       SkipBackUp = FALSE;
   C_STR_LIST TempTransConnStrList, TransConnStrList;
   C_STRING   ConnStr, GeomConnStr;
   C_GPH_INFO *pGphInfo;
   long       LastId;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (ptr_info())
      // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
      if (ptr_info()->LinkedTable) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

   // segno la classe in fase di salvataggio in modo che nessuna altra sessione
   // possa effettuare il salvataggio ed estrazioni nello stesso momento sulla stessa classe
   // per non bloccare i dwg ad altri utenti in fase di salvataggio (che possono aver
   // già salvato le schede nei database ma non gli oggetti grafici).
   if (set_locked_on_save(GS_CURRENT_WRK_SESSION->get_id(), &Locked, &LastId) == GS_BAD)
      return GS_BAD;

   // non si può salvare la classe
   if (Locked == GS_BAD) { GS_ERR_COD = eGSLockBySession; return GS_BAD; }

   if (id.category == CAT_EXTERN)
   {
      C_SUB *pSub = (C_SUB *) ptr_sub_list()->get_head();
      ConnStr = pSub->ptr_info()->getDBConnection(OLD)->get_OrigConnectionStr();
      // Se la grafica era in formato DB
      pGphInfo = pSub->ptr_GphInfo();
   }
   else
   {
      if (ptr_info())
      {
         ConnStr = ptr_info()->getDBConnection(OLD)->get_OrigConnectionStr();
         ptr_info()->OldLastId = LastId;
      }
      // Se la grafica era in formato DB
      pGphInfo = ptr_GphInfo();
   }

   if (pGphInfo && pGphInfo->getDataSourceType() == GSDBGphDataSource)
      GeomConnStr = ((C_DBGPH_INFO *) pGphInfo)->getDBConnection()->get_OrigConnectionStr();

   BeginTransaction(TEMP, FALSE, TempTransConnStrList);

   if (id.category == CAT_EXTERN)
      BeginTransaction(OLD, TRUE, TransConnStrList);
   else
   {
      // Se si tratta di griglia con più di 100000 celle 
      // ACCESS da problemi con le transaction
      if (id.category == CAT_GRID && (ptr_grid()->nx * ptr_grid()->ny > 100000))
         { GeomConnStr.clear(); SkipBackUp = TRUE; }
      else
         BeginTransaction(OLD, TRUE, TransConnStrList);
   }

   if (pGphInfo)
      // Se la grafica era in formato DWG
      if (pGphInfo->getDataSourceType() == GSDwgGphDataSource)
      {
         // Delego il back-up dei disegni a MAP
         CreateBackupFileOfSourceDwg.rbnext = NULL;
         CreateBackupFileOfSourceDwg.restype = RTT;
         PrevCreateBackupFileOfSourceDwg = ade_prefgetval(_T("CreateBackupFileOfSourceDwg"));
         ade_prefsetval(_T("CreateBackupFileOfSourceDwg"), &CreateBackupFileOfSourceDwg);
      }
      else // Se la geometria era in formato DB e non è stata creata una transazione
         if (GeomConnStr.len() > 0)
            if (TransConnStrList.search_name(GeomConnStr.get_name()) == NULL)
               What = GRAPHICAL; // bisogna fare il backup della geometria

   // Se NON è stato possibile iniziare una transazione per la tabella degli attributi e 
   // non si deve saltare il backup (caso griglie)
   if (ConnStr.len() > 0)
      if (TransConnStrList.search_name(ConnStr.get_name()) == NULL && !SkipBackUp)
         if (What == 99) // non si doveva ancora fare alcun backup
            What = RECORD; // bisogna fare il backup degli attributi
         else // si doveva già fare il backup della geometria
            What = ALL; // bisogna fare il backup sia della geometriua che degli attributi

   Result = GS_BAD;
   do
   {
      if (What != 99) // se si deve fare qualche backup (geometria o attributi)
         // Creazione back-up della classe
         if (Backup(GSCreateBackUp, What) == GS_BAD) break;

      gsc_startTransaction();

      try
      {
         // Salvataggio del database
         if (save_DBdata() == GS_BAD) { gsc_abortTransaction(); break; }

         // Salvataggio della grafica
         if (save_GeomData() == GS_BAD) { gsc_abortTransaction(); break; }
      }

      catch (...)
      {
          gsc_abortTransaction();
          break;
      }

      gsc_endTransaction();

      if (ptr_info())
         // aggiorno gli elast in memoria
         if (id.category != CAT_EXTERN)
            ptr_info()->OldLastId = ptr_info()->TempLastId; // aggiorno il last originale
         else
         {
            C_SUB *pSub = (C_SUB *) ptr_sub_list()->get_head();

            while (pSub)
            {
               pSub->ptr_info()->OldLastId = pSub->ptr_info()->TempLastId; // aggiorno il last originale
               pSub = (C_SUB *) pSub->get_next();
            }
         }
   
      Result = GS_GOOD;

      // svuoto GS_DELETED per la classe indicata
      if (id.category != CAT_EXTERN)
         if (gsc_EmptyGsDeletedNoSub(&(id.code)) == GS_BAD) break;
   }
   while (0);

   if (Result == GS_BAD)
   {
      // Se ho dovuto fare un backup della classe
      if (What != 99)
         // Restora il backup
         if (Backup(GSRestoreBackUp, What) == GS_BAD)
            // "\nL'operazione è fallita. Ripristinare manualmente
            // \ni files di backup della classe (vedi manuale)."
            acutPrintf(gsc_msg(636));

      // se ci sono transazioni incominicate le annullo
      RollbackTrans(TransConnStrList);

      if (ptr_info())
		{
         // se ci sono transazioni incominicate nei db temporanei le annullo
         RollbackTrans(TempTransConnStrList);
			reindexTab(); // ripristino elast originali
		}

      set_unlocked(GS_CURRENT_WRK_SESSION->get_id(), GS_GOOD, false); // Non aggiorno ENT_LAST e LAST_SAVE_DATE
   }
   else // Il salvatagggio ha avuto successo
   {
      // Se ho dovuto fare un backup della classe
      if (What != 99)
         Backup(GSRemoveBackUp, What); // Cancella il back-up

      CommitTrans(TransConnStrList);
      CommitTrans(TempTransConnStrList);

      set_unlocked(GS_CURRENT_WRK_SESSION->get_id(), GS_GOOD, true, true); // Aggiorno ENT_LAST e LAST_SAVE_DATE

      if (ptr_info())
      {
         C_STRING pathfile;
         TCHAR    buf[ID_PROFILE_LEN];

         // Per la classe salvata scrivo su file "GS_CLASS_FILE" della classe
         // le C_INFO per memorizzare i nuovi last
         GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile, id.code);

         if (id.category != CAT_EXTERN)
         {
            swprintf(buf, ID_PROFILE_LEN, _T("%d.0"), id.code);
            if (ptr_info()->ToFile(pathfile, buf) == GS_BAD) return GS_BAD;
         }
         else
         {
            C_SUB *pSub = (C_SUB *) ptr_sub_list()->get_head();

            while (pSub)
            {
               swprintf(buf, ID_PROFILE_LEN, _T("%d.%d"), pSub->ptr_id()->code, pSub->ptr_id()->sub_code);
               if (pSub->ptr_info()->ToFile(pathfile, buf) == GS_BAD) return GS_BAD;
               pSub = (C_SUB *) pSub->get_next();
            }
         }
      }
   }

   if (pGphInfo)
      // Se la grafica era in formato DWG
      if (pGphInfo->getDataSourceType() == GSDwgGphDataSource)
      {
         ade_prefsetval(_T("CreateBackupFileOfSourceDwg"), PrevCreateBackupFileOfSourceDwg);
         acutRelRb(PrevCreateBackupFileOfSourceDwg);
      }

   // Se la classe è stata salvata correttamente
   if (Result == GS_GOOD) setModified(GS_BAD); // classe non modificata

   return Result;
}

int C_CLASS::save_DBdata(void) { return GS_GOOD; }


/*********************************************************/
/*.doc (new 2) C_CLASS::get_alldata_from_temp <internal> */
/*+                                                                       
  Preparo istruzione per la selezione di tutti i dati del temp esclusa
  la scheda di default.
  Parametri:
  _RecordsetPtr &pRs;
  long          *original_last;   se indicato seleziona solo le schede
                                  <= a last cioè le schede NON nuove
                                  (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. Da usarsi solo per simulazioni.
-*/  
/*********************************************************/
int C_CLASS::get_alldata_from_temp(_RecordsetPtr &pRs, long *original_last)
{
   C_INFO         *p_info;
   C_DBCONNECTION *pConn;
   C_STRING       statement, TableRef, FldName;

   if (is_subclass() == GS_BAD || (p_info = ptr_info()) == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // ricavo tabella dei link temporanei
   if (getTempTableRef(TableRef) == GS_BAD) return GS_BAD;

   if ((pConn = p_info->getDBConnection(TEMP)) == NULL) return GS_BAD;

   FldName = p_info->key_attrib;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += FldName;
   statement += _T("<>0");

   if (original_last)
   {
      statement += _T(" AND ");
      statement += p_info->key_attrib;
      statement += _T("<=");
      statement += *original_last;
   }

   // creo record-set
   // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
   // in una transazione fa casino (al secondo recordset che viene aperto)
   return pConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockOptimistic);
}


/*********************************************************/
/*.doc (new 2) C_CLASS::get_all_reldata_from_temp <internal> */
/*+                                                                       
  Legge tutte le relazioni temporanee di una classe gruppo.
  Parametri:
  _RecordsetPtr &pRs;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. da usarsi solo per gruppi
-*/  
/*********************************************************/
int C_CLASS::get_all_reldata_from_temp(_RecordsetPtr &pRs)
{                 
   C_INFO         *p_info;
   C_STRING       statement, LnkTableRef;
   C_DBCONNECTION *pConn;

   if ((p_info = ptr_info()) == NULL || !ptr_group_list())
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // ricavo tabella dei link temporanei
   if (getTempLnkTableRef(LnkTableRef) == GS_BAD) return GS_BAD;

   // leggo record in temp
   statement += _T("SELECT KEY_ATTRIB,CLASS_ID,ENT_ID,STATUS FROM ");
   statement += LnkTableRef;
   statement += _T(" ORDER BY KEY_ATTRIB");

   if ((pConn = p_info->getDBConnection(TEMP)) == NULL) return GS_BAD;

   // creo record-set in modifica con scroll avanti-indietro (per poter cancellare le relazioni)
   return pConn->OpenRecSet(statement, pRs, adOpenKeyset, adLockOptimistic);
}


/*********************************************************/
/*.doc (new 2) C_CLASS::prepare_del_reldata_from_old_where_key <internal> */
/*+                                                                       
  Preparo istruzione per la cancellazione delle relazioni di un
  gruppo noto dall'old delle relazioni.
  _CommandPtr &pCmd;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B. da usarsi solo per gruppi
-*/  
/*********************************************************/
int C_CLASS::prepare_del_reldata_from_old_where_key(C_PREPARED_CMD &pCmd)
{                 
   C_INFO         *p_info;
   C_STRING       statement, LnkTableRef;
   C_DBCONNECTION *pConn;
   _ParameterPtr  pParam;
   C_ATTRIB       *pAttrib;

   if ((p_info = ptr_info()) == NULL || !ptr_group_list())
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   
   // ricavo tabella dei link old
   if (getOldLnkTableRef(LnkTableRef) == GS_BAD) return GS_BAD;

   statement += _T("DELETE FROM ");
   statement += LnkTableRef;
   statement += _T(" WHERE KEY_ATTRIB=?");

   if ((pConn = p_info->getDBConnection(OLD)) == NULL) return GS_BAD;

   // Attributo chiave di ricerca
   pAttrib = (C_ATTRIB *) ptr_attrib_list()->search_name(p_info->key_attrib.get_name(), FALSE);
   if (pAttrib->init_ADOType(pConn) == NULL) return GS_BAD;

   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd.pCmd) == GS_BAD) return GS_BAD;

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), pAttrib->ADOType) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd.pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}
 

/*************************************************************/
/*.doc C_CLASS::delete_2_old                      <internal> */
/*+                                                                       
  Funzione che leggendo dalla tabella GS_DELETE (che serve a marcare le
  cancellate) verifica, per ogni oggetto cancellato della classe, che
  non sia stato inserito o modificato nella sessione corrente di lavoro, se 
  veramente non esiste più in grafica e, in caso positivo procede alla
  cancellazione del record e delle sue eventuali secondarie.
  Parametri:

  Restituisce quanti oggetti sono stati cancellati in caso di successo 
  altrimenti restituisce -1. 
-*/  
/*************************************************************/
long C_CLASS::delete_2_old(void)
{        
   C_STRING            statement, TableRef;
   C_RB_LIST	        ColValues, TempRelColValues;
   presbuf             pKeyVal, pStatus = NULL;
   int                 result, ToErase, Status;
   int                 IsOldRsCloseable;
   C_SELSET            ss;
   long                gs_id, Qty = 0;
   C_PREPARED_CMD_LIST ListSelCmdOldSec, ListSelCdmOldRelSec;
   C_PREPARED_CMD      *p_Cmd_del_sec, *p_Cmd_del_rel_sec, pOldCmd;
   C_DBCONNECTION      *pConn;
   _RecordsetPtr       pRs, pOldRs;
   C_PREPARED_CMD      pOldLnkCmd, pTempLnkCmd;
   C_TOPOLOGY          OldTopo;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return -1; }
   if (!ptr_attrib_list()) { GS_ERR_COD = eGSInvClassType; return -1; }

   // Preparo il comando di lettura dei dati della classe dall'old
   if (prepare_data(pOldCmd, OLD) == GS_BAD) return -1;

   // se gruppo
   if (ptr_group_list())
   {
      // preparo comando per la lettura delle relazioni TEMP
      if (prepare_reldata_where_key(pTempLnkCmd, TEMP) == GS_BAD) return -1;
      // preparo comando per la lettura delle relazioni OLD
      if (prepare_del_reldata_from_old_where_key(pOldLnkCmd) == GS_BAD) return -1;
   }

   if (id.sub_code > 0) // se sottoclasse
   {  // Setto la topologia di tipo rete per la simulazione
      OldTopo.set_type(TYPE_POLYLINE);
      OldTopo.set_cls(GS_CURRENT_WRK_SESSION->find_class(id.code));
      OldTopo.set_temp(GS_BAD);
   }
   
   // Creo una lista di istruzioni compilate per poter cancellare le schede e i link 
   // di eventuali secondarie collegate ad un'entità di questa classe
   if (gsc_GetListDelStmOldSec(this, ListSelCmdOldSec) == GS_BAD) return -1;
   if (gsc_GetListDelStmOldRelSec(this, ListSelCdmOldRelSec) == GS_BAD) return -1;
   
   if (GS_CURRENT_WRK_SESSION->getDeletedTabInfo(&pConn, &TableRef) == GS_BAD) return -1;

   // preparazione istruzione per selezione record in GS_DELETED
   statement = _T("SELECT KEY_ATTRIB FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += id.code;      // Codice classe
   statement += _T(" AND SUB_CL_ID=");
   statement += id.sub_code;  // Codice sotto-classe
   statement += _T(" AND KEY_ATTRIB>0 AND STATUS<>");
   // flag a bit per evidenziare che l'entità NON deve essere salvata      
   statement += (ERASED | NOSAVE);

   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return -1;
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return -1; }

   // codice dell'oggetto
   pKeyVal = ColValues.CdrAssoc(_T("KEY_ATTRIB"));

   // scorro l'elenco delle possibili cancellate
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { Qty = -1; break; }
      if (gsc_rb2Lng(pKeyVal, &gs_id) == GS_BAD) gs_id = 0;

      ToErase = TRUE;
      if (ptr_fas()) // se la classe ha rappresentaz. grafica
      {
         // Verifico se ci sono oggetti grafici (principali) collegati
         if (get_SelSet(gs_id, ss, GRAPHICAL) != GS_GOOD)
            { Qty = -1; break; }

         // numero di oggetti grafici associati al linkset
         if (ss.length() > 0) ToErase = FALSE;
      }
      else
      {  // verifico se nelle relazioni lo status e segnato come NOSAVE
         // restituisce la lista delle relazioni dei gruppi
         // ((<cod.gruppo><cod.classe figlia><cod entità figlia><status>) (...) (...) ...)
         if (gsc_get_reldata(pTempLnkCmd, gs_id, TempRelColValues) == GS_BAD)
            { Qty = -1; break; }

         // ciclo su tutte le relazioni
         int i = 0;
         while ((pStatus = TempRelColValues.nth(i++)) != NULL)
         {
            pStatus = gsc_nth(3, pStatus);
            gsc_rb2Int(pStatus, &Status);

            if (Status & NOSAVE)
               { ToErase = FALSE; break; } // non è da considerare
            if (!(Status & ERASED))
               { ToErase = FALSE; break; } // se almeno una relazione è valida non è da considerare
         }
      }

      if (ToErase) // procedo con la cancellazione
      {
         if (gsc_get_data(pOldCmd, gs_id, pOldRs, &IsOldRsCloseable) == GS_GOOD)
         {
            if (gsc_DBDelRow(pOldRs) == GS_BAD)
            {
               if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);
               Qty = -1;
               break;
            }
            if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);
            Qty++;

   			if (id.category == CAT_GROUP)
            {  // cancello anche le relazioni
               // Setto parametri della istruzione
               gsc_SetDBParam(pOldLnkCmd.pCmd, 0, gs_id); // Codice entità
               // Esegue il comando 
               if (gsc_ExeCmd(pOldLnkCmd.pCmd) == GS_BAD) { Qty = -1; break; }
            }

            if (id.sub_code > 0) // cancello la relaz. topologica per le simulazioni
               if (OldTopo.editdelelem(gs_id, id.type) == GS_BAD) { Qty = -1; break; }

            // cancello le schede e le relazioni delle secondarie dall'OLD
            p_Cmd_del_sec     = (C_PREPARED_CMD *) ListSelCmdOldSec.get_head();
            p_Cmd_del_rel_sec = (C_PREPARED_CMD *) ListSelCdmOldRelSec.get_head();
            result = GS_GOOD;

            while (p_Cmd_del_sec && p_Cmd_del_rel_sec)
            {
               // cancello le schede delle secondarie dall'OLD
               gsc_SetDBParam(p_Cmd_del_sec->pCmd, 0, gs_id); // Codice entità
               // Esegue il comando 
               if (gsc_ExeCmd(p_Cmd_del_sec->pCmd) == GS_BAD) { result = GS_BAD; break; }

               // cancello le relazioni delle secondarie dall'OLD
               gsc_SetDBParam(p_Cmd_del_rel_sec->pCmd, 0, gs_id); // Codice entità
               // Esegue il comando 
               if (gsc_ExeCmd(p_Cmd_del_rel_sec->pCmd) == GS_BAD) { result = GS_BAD; break; }

               p_Cmd_del_sec     = (C_PREPARED_CMD *) ListSelCmdOldSec.get_next();
               p_Cmd_del_rel_sec = (C_PREPARED_CMD *) ListSelCdmOldRelSec.get_next();
            }
            if (result == GS_BAD) { Qty = -1; break; }
         }
      }
      gsc_Skip(pRs);
   }
   gsc_DBCloseRs(pRs);

   return Qty;
}


/*********************************************************/
/*.doc C_CLASS::save_default                  <internal> */
/*+                                                                       
  Salvo eventuale scheda di default nella tabella OLD.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::save_default(void)
{                 
   _RecordsetPtr  pRs;
   C_PREPARED_CMD pTempCmd, pOldCmd;
   C_RB_LIST      ColValues;
   int            IsOldRsCloseable;
   
   if (!ptr_info()) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   
   // Se si tratta di una tabella collegata non posso cambiare i dati di default
   if (ptr_info()->LinkedTable) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

   // Preparo i comandi di lettura dei dati della classe dal temp/old
   if (prepare_data(pTempCmd, TEMP) == GS_BAD) return GS_BAD;
   if (prepare_data(pOldCmd, OLD) == GS_BAD) return GS_BAD;

   if (gsc_get_data(pTempCmd, 0, ColValues) == GS_BAD) return GS_GOOD;

   if (gsc_get_data(pOldCmd, 0, pRs, &IsOldRsCloseable) == GS_GOOD)
   {
      int Res = gsc_DBUpdRow(pRs, ColValues);

      if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

      return Res;
   }
   else
   {
      C_DBCONNECTION *pConn;

      if ((pConn = ptr_info()->getDBConnection(OLD)) == NULL) return GS_BAD;
      return pConn->InsRow(ptr_info()->OldTableRef.get_name(), ColValues);
   }
}


/*********************************************************/
/*.doc gsc_is_completed_save <internal> */
/*+                                                                       
  Verifica che tutte le classi siano state salvate.
  Parametri:
  C_INT_INT_LIST *lista_cls;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_is_completed_save(C_INT_INT_LIST *lista_cls)
{
   C_INT_INT *cls;

   cls = (C_INT_INT *) lista_cls->get_head();
   while (cls != NULL)
   {
      if (cls->get_type() == DESELECTED) return FALSE;
      cls = (C_INT_INT *) lista_cls->get_next();
   }

   return TRUE;
}


/*********************************************************/
/*.doc gsc_is_saveable <internal> */
/*+                                                                       
  Verifica che la classe sia nella condizione in cui si 
  possa salvare.
  Parametri:
  C_INT_INT_LIST *lista_cls;  lista classi da salvare
  C_INT_INT      *cls;        classe da salvare (codice e tipo)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_is_saveable(C_INT_INT_LIST *lista_cls, C_INT_INT *cls)
{
   C_INT_INT 	  *subcls, *subcls_lista_cls;
   C_CLASS   	  *pCls;
   C_GROUP_LIST *p_group_list;

   if (cls->get_type() == SELECTED) return GS_BAD;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls->get_key())) == NULL) return GS_BAD;

   if (pCls->ptr_id()->abilit != GSUpdateableData)
   {
      GS_ERR_COD = (pCls->ptr_id()->abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
   if (pCls->ptr_info())
      if (pCls->ptr_info()->LinkedTable) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }
   if (pCls->ptr_GphInfo() && pCls->ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource &&
       (((C_DBGPH_INFO *) pCls->ptr_GphInfo())->LinkedTable ||
        ((C_DBGPH_INFO *) pCls->ptr_GphInfo())->LinkedLblGrpTable ||
        ((C_DBGPH_INFO *) pCls->ptr_GphInfo())->LinkedLblTable))
      { GS_ERR_COD = eGSClassLocked; return GS_BAD; }



   if ((p_group_list = pCls->ptr_group_list()) != NULL)
   { // verifico che le classi che compongono il gruppo siano in lista
      subcls = (C_INT_INT *) p_group_list->get_head();
      while (subcls != NULL)
      {
         if ((subcls_lista_cls = (C_INT_INT *) lista_cls->search_key(subcls->get_key())) == NULL)
		      return GS_BAD;
         // se non è già stata già salvata
         if (subcls_lista_cls->get_type() != SELECTED) return GS_BAD;
         subcls = (C_INT_INT *) subcls->get_next();
	   }
   }
  
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLASS::get_list_mother_group <internal> */
/*+                                                                       
  Creazione di lista di comandi preparati relative alle classi aventi questa 
  classe come membro. Ogni elemento della lista contiene codice classe e 
  istruzione per modifica relazioni.
  Parametri:
  C_PREPARED_CMD_LIST &list_mother;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::get_list_mother_group(C_PREPARED_CMD_LIST &list_mother)
{
   C_CLASS 		    *pCls;
   C_CLS_PUNT_LIST extracted_list;
   C_CLS_PUNT	    *punt;
   C_PREPARED_CMD  *pCmd;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (GS_CURRENT_WRK_SESSION->get_pPrj()->extracted_class(extracted_list) == GS_BAD) return GS_BAD;
   
   list_mother.remove_all();

   punt = (C_CLS_PUNT *) extracted_list.get_head();
   while (punt)
   {
      pCls = (C_CLASS *) punt->get_class();
      // Se esiste la tabella dei link temporanei
	   if (pCls->ptr_id()->code != id.code && // se pCls non è la classe puntata da this
          pCls->is_member_class(id.code) == GS_GOOD) // se la classe è un membro di pCls
	   {
	      if ((pCmd = new C_PREPARED_CMD) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         pCmd->set_key(pCls->ptr_id()->code);
         if (pCls->prepare_reldata_where_member(*pCmd, TEMP) == GS_BAD)
		      return GS_BAD;
         list_mother.add_tail(pCmd);
	   }
	   punt = (C_CLS_PUNT *) extracted_list.get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_mod_reldata <internal> */
/*+                                                                       
  Modifico i codici dei membri delle relazioni usando i comandi preparati
  dalla funzione <get_list_mother_group>.
  Parametri:
  C_PREPARED_CMD_LIST &list_mother; Lista di istruzioni compilate
  int 		          Cls;			 Codice classe (membro del gruppo)
  long 		          Key;  		 Codice entità (membro del gruppo)
  long                NewKey;     Nuovo codice entità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_mod_reldata(C_PREPARED_CMD_LIST &list_mother, int Cls, long Key, long NewKey)
{          
   C_PREPARED_CMD *pCmd;
   _RecordsetPtr  pRs;
   presbuf        pEntID, pClassId;
   C_RB_LIST      ColValues;
   int            IsRsCloseable, ClassId;
   long           OldKey;

	// se membro di un gruppo ne aggiorno le relazioni
	pCmd = (C_PREPARED_CMD *) list_mother.get_head();
   while (pCmd)
   {
      if (gsc_get_reldata(*pCmd, Cls, Key, pRs, &IsRsCloseable) == GS_GOOD)
      {
         if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;

         pClassId = ColValues.CdrAssoc(_T("CLASS_ID"));
         // codice entità membro della classe
         pEntID = ColValues.CdrAssoc(_T("ENT_ID"));

         // per ogni relazione
         while (gsc_isEOF(pRs) == GS_BAD)
         {
            if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }

            // Esco quando cambia il codice della classe
            // (pRs potrebbe contenere tutte le relazioni temporanee dei gruppi se
            // la tabella supporta gli indici di ricerca)
            if (gsc_rb2Int(pClassId, &ClassId) == GS_BAD || 
                gsc_rb2Lng(pEntID, &OldKey) == GS_BAD)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }
            if (ClassId != Cls || OldKey != Key) break;

            // aggiorno codice entità membro
	         gsc_RbSubst(pEntID, NewKey);

            // aggiorno riga
            if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
               { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }

            gsc_Skip(pRs);
         }
      }
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);

      pCmd = (C_PREPARED_CMD *) list_mother.get_next();
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*  INIZIO FUNZIONI DELLA CATEGORIA C_SIMPLEX            */
/*********************************************************/


/*********************************************************/
/*.doc C_SIMPLEX::save_DBdata                 <internal> */
/*+                                                                       
  Salvataggio dati nell'OLD.
  Parametri:
  					
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SIMPLEX::save_DBdata(void)
{
   presbuf		        prb_key_insert = NULL;
   int			        result = GS_BAD, toRecalc, toUseUsrCmds;
   int                 Prcnt;
   long 		           n_entities, LastIdFromDB, key_val;
   long                del_ent = 0, new_ent = 0, upd_ent = 0, EntSSNdx;
   C_STRING            TempTableRef;
   C_STRING            SubstUpdUsrFun, BeforeUpdUsrFun, AfterUpdFun;
   C_ATTRIB_LIST       *p_attrib_list = ptr_attrib_list();
   C_ATTRIB            *p_attrib;
   C_RB_LIST	        TempColValues;
   C_PREPARED_CMD_LIST list_mother, list_mother_sec;
   TCHAR               Msg[MAX_LEN_MSG];
   ads_name            ent;
   C_SELSET            ClsSS, EntSS;
   C_LINK              Link;
   C_LINK_SET          LinkSet;
   _RecordsetPtr       pInsRs, pOldRs, dummyRs;
   C_DBCONNECTION      *pTempConn, *pOldConn;
   C_PREPARED_CMD      pTempCmd, pOldCmd;
   int                 IsOldRsCloseable;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1078), id.name); // "Salvataggio dati alfanumerici classe <%s>"

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico abilitazione
   if (gsc_check_op(opSaveArea) == GS_BAD) return GS_BAD;
   // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
   if (info.LinkedTable) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

   if (isDataModified() != GS_GOOD) return GS_GOOD;

   acutPrintf(gsc_msg(113), id.name); // "\n\nEntità classe %s:\n"
   swprintf(Msg, MAX_LEN_MSG, _T("Elaborating class <%s>."), id.name);  
   gsc_write_log(Msg);
   
   // ultimo codice valido della classe 
   LastIdFromDB = info.getOldLastIdFromDB();
   if (info.OldLastId > LastIdFromDB) LastIdFromDB = info.OldLastId;

   // connessioni ai database
   if ((pOldConn = info.getDBConnection(OLD)) == NULL ||
       (pTempConn = info.getDBConnection(TEMP)) == NULL)
      return GS_BAD;

   // ricavo riferimento a tabella temporanea
   if (getTempTableRef(TempTableRef) == GS_BAD)
      return GS_BAD;

   do
   {
      // cancello dall'OLD le cancellate
      if ((del_ent = delete_2_old()) == -1) break;

      // Creo una lista di istruzioni compilate per poter modificare i link di eventuali
      // gruppi collegati ad un'entità di questa classe
      if (get_list_mother_group(list_mother)  == GS_BAD) break;

      // Creo una lista di istruzioni compilate per poter modificare i link di eventuali
      // secondarie collegate ad un'entità di questa classe
      if (get_list_mother_sec(list_mother_sec) == GS_BAD) break;

      // Creo il selection set filtrando da GEOsimAppl::SAVE_SS tutti gli oggetti della classe
      if (GEOsimAppl::SAVE_SS.copyIntersectClsCode(ClsSS, id.code, id.sub_code) == GS_BAD)
         break;
      
      // numero di oggetti grafici
      if ((n_entities = ClsSS.length()) <= 0)
      {
         StatusBarProgressMeter.Set_Perc(100);
         gsc_write_log(_T("100 %% elaborated"));
         result = GS_GOOD;
         break;
      }

      // Compilo le istruzioni di lettura dei dati della classe dal temp/old
      if (prepare_data(pTempCmd, TEMP) == GS_BAD) break;
      if (prepare_data(pOldCmd, OLD) == GS_BAD) break;
      // preparo istruzione per l'inserimento di record nell'OLD
      if (pOldConn->InitInsRow(info.OldTableRef.get_name(), pInsRs) == GS_BAD) break;

      if (gsc_InitDBReadRow(pInsRs, TempColValues) == GS_BAD) break;
      prb_key_insert = TempColValues.CdrAssoc(info.key_attrib.get_name());

      // se l'attributo chiave è parametro di una funzione di calcolo o di validazione
      // oppure è un attributo visibile
      if (p_attrib_list->is_funcparam(info.key_attrib.get_name()) == GS_GOOD ||
          ((p_attrib = (C_ATTRIB *) p_attrib_list->search_name(info.key_attrib.get_name(), FALSE)) &&
           p_attrib->is_visible() == GS_GOOD)) 
         toRecalc = TRUE;
      else
         toRecalc = FALSE;

      // se ci sono dei comandi virtualizzati
      if (id.usr_cmds && wcslen(id.usr_cmds) > 0)
      {
         toUseUsrCmds = TRUE;
         // eventuale funzione esterna in completa sostituzione di quella di aggiornamento di GEOsim
         gsc_getAction(id.usr_cmds, STR_SUBST, STR_UPDATE, SubstUpdUsrFun);
         // eventuale funzione esterna prima di quella di aggiornamento di GEOsim
         gsc_getAction(id.usr_cmds, STR_BEFORE, STR_UPDATE, BeforeUpdUsrFun);
         // eventuale funzione esterna dopo quella di aggiornamento di GEOsim
         gsc_getAction(id.usr_cmds, STR_AFTER, STR_UPDATE, AfterUpdFun);
      }
      else
         toUseUsrCmds = FALSE;

      result = GS_GOOD;
      StatusBarProgressMeter.Init(n_entities);
      // scorro l'elenco delle entità della classe
      while (ClsSS.entname(0, ent) == GS_GOOD)
      {
         // legge il valore chiave e gruppo di selezione di tutti gli oggetti collegati alla scheda
         if (get_Key_SelSet(ent, &key_val, EntSS) != GS_GOOD)
            { result = GS_BAD; break; }

         // leggo da TEMP
         if (gsc_get_data(pTempCmd, key_val, TempColValues) != GS_GOOD)
            { result = GS_BAD; break; }
   
         if (is_NewEntity(key_val) == GS_GOOD) // entità nuova
         {
		      new_ent++;
            gsc_RbSubst(prb_key_insert, ++LastIdFromDB);

            // se esiste un campo calcolato che dipende dal campo chiave oppure
            // almeno un comando utente che ha virtualizzato il comando GEOsim
            if (toRecalc || toUseUsrCmds) 
            {
               C_STRING dummyStr; // Ho deciso che in fase di salvataggio NON devono
                                    // essere richiamate le funzioni di virtualizzazione                               

               // poichè è cambiato il campo chiave devo aggiornare la scheda
               // utilizzando una funzione speciale
               if (gsc_UpdDataForSave(this, TempColValues, EntSS,
                                      dummyStr, dummyStr, dummyStr) == GS_BAD)
                  { result = GS_BAD; break; }
            }
            
            // aggiungo la scheda nel'OLD con nuova codificazione
            if (gsc_DBInsRow(pInsRs, TempColValues) == GS_BAD)
            {
               swprintf(Msg, MAX_LEN_MSG, _T("*Errore* Entity %ld not inserted."), key_val);  
               gsc_write_log(Msg);

               result = GS_BAD; break;
            }

            // Collego tutti gli oggetti grafici dell'entità al nuovo valore chiave
            EntSSNdx = 0;
            while (EntSS.entname(EntSSNdx++, ent) == GS_GOOD)
               if (Link.Set(ent, id.code, id.sub_code, LastIdFromDB, MODIFY) == GS_BAD)
                  { result = GS_BAD; break; }

            if (result == GS_BAD) break;

            // sottraggo dal gruppo di selezione quello dell'entità corrente
            if (ClsSS.subtract(EntSS) != GS_GOOD) { result = GS_BAD; break; }

            // bisogna modificare eventuali relazioni di gruppi che sono legati 
            // a questa entità per aggiornare la nuova codificazione
            if (gsc_mod_reldata(list_mother, id.code, key_val, LastIdFromDB) == GS_BAD) 
               { result = GS_BAD; break; }

            // bisogna modificare eventuali relazioni di secondarie di GEOsim che sono 
            // legate a questa entità per aggiornare la nuova codificazione
            if (gsc_mod_reldata_sec(list_mother_sec, id.code, id.sub_code,
                                    key_val, LastIdFromDB) == GS_BAD)
               { result = GS_BAD; break; }
         }
         else 
         { // entità modificata già esistente sull'OLD
            upd_ent++;

            // leggo da OLD
            if (gsc_get_data(pOldCmd, key_val, pOldRs, &IsOldRsCloseable) != GS_GOOD)
            {
               if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }

               // questo record è stato cancellato quindi si presume che anche 
               // tutte le sue istanze grafiche lo siano
               // tolgo gli oggetti legati a questo record dal gruppo GEOsimAppl::SAVE_SS
               GEOsimAppl::SAVE_SS.subtract(EntSS);
            }
            else
            {
				   // aggiorno i dati sull'old
               if (gsc_DBUpdRow(pOldRs, TempColValues) == GS_BAD)
               {
                  if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);

                  swprintf(Msg, MAX_LEN_MSG, _T("*Errore* Entity %ld not updated."), key_val);  
                  gsc_write_log(Msg);

                  result = GS_BAD; break;
               }
               if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);
            }

            // sottraggo dal gruppo di selezione quello dell'entità corrente
            if (ClsSS.subtract(EntSS) != GS_GOOD) { result = GS_BAD; break; }
	      }

         Prcnt = (int) ((n_entities - ClsSS.length()) * 100 / n_entities);
         StatusBarProgressMeter.Set_Perc(Prcnt);
      }  // fine ciclo entità  

      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

      if (result == GS_BAD) break;

      result = GS_GOOD;
   }
   while (0);
   
   // stampa su video e su file (opzionale) il n. di oggetti cancellati, modificati e inseriti
   cls_print_save_report(del_ent, upd_ent, new_ent);

   list_mother_sec.remove_all(); // Rilascio tutte le istruzioni sulle secondarie
   gsc_DBCloseRs(pInsRs);
   
   if (result == GS_GOOD)
      // cancello tutte le schede della tabella TEMP
      if (pTempConn->DelRows(TempTableRef.get_name()) == GS_BAD)
         return GS_BAD;

   // salvataggio di eventuali secondarie
   if (gsc_do_save_sec(this) == GS_BAD) return GS_BAD;

   info.TempLastId = LastIdFromDB;

   return result;
}


/*********************************************************/
/*  FINE   FUNZIONI DELLA CATEGORIA C_SIMPLEX            */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_GROUP            */
/*********************************************************/


/*********************************************************/
/*.doc (new 2)C_GROUP::save_DBdata <internal> */
/*+                                                                       
  Salvataggio dati nell'OLD.
  Parametri:
  					
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_GROUP::save_DBdata(void)
{
   presbuf		        prb_key_insert = NULL;
   presbuf             pKeyAttrib, pClassId, pEntID, pStatus;
   int			        result = GS_BAD, Status, exit = FALSE;
   int                 toRecalc, toUseUsrCmds; 
   long 		           LastIdFromDB, key_val, previous_key_val, nn = 0;
   long                del_ent = 0, new_ent = 0, upd_ent = 0;
   C_STRING            TempTableRef, LnkTableRef, TmpLnkTableRef, statement;
   C_STRING            SubstUpdUsrFun, BeforeUpdUsrFun, AfterUpdFun;
   C_ATTRIB_LIST       *p_attrib_list = ptr_attrib_list();
   C_RB_LIST	        TempColValues, OldColValues, TempRelColValues;
   TCHAR               Msg[MAX_LEN_MSG];
   C_PREPARED_CMD_LIST list_mother_sec;
   C_DBCONNECTION      *pTempConn, *pOldConn;
   _RecordsetPtr       pInsLnkRs, pInsRs, pTempLnkRs, pOldRs;
   C_PREPARED_CMD      pTempCmd, pOldCmd;
   int                 IsOldRsCloseable;
   _CommandPtr         pTempLnkCmd;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1078), id.name); // "Salvataggio dati alfanumerici classe <%s>"

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico abilitazione
   if (gsc_check_op(opSaveArea) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
   if (info.LinkedTable) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

   acutPrintf(gsc_msg(113), id.name); // "\n\nEntità classe %s:\n"
   swprintf(Msg, MAX_LEN_MSG, _T("Elaborating class <%s>."), id.name);  
   gsc_write_log(Msg);

   // ultimo codice valido della classe 
   LastIdFromDB = info.getOldLastIdFromDB();
   if (info.OldLastId > LastIdFromDB) LastIdFromDB = info.OldLastId;

   // connessioni ai database
   if ((pOldConn = info.getDBConnection(OLD)) == NULL ||
       (pTempConn = info.getDBConnection(TEMP)) == NULL)
      return GS_BAD;

   // ricavo riferimento a tabella temporanea
   if (getTempTableRef(TempTableRef) == GS_BAD)
      return GS_BAD;

   do
   {
      StatusLineMsg.Init(gsc_msg(310), LITTLE_STEP); // ogni 10 "%ld entità GEOsim elaborate."

      // cancello dall'OLD le cancellate
      if ((del_ent = delete_2_old()) == -1) break;

      // Creo una lista di istruzioni compilate per poter modificare i link di eventuali
      // secondarie collegate ad un'entità di questa classe
      if (get_list_mother_sec(list_mother_sec)  == GS_BAD) break;

      // Compilo le istruzioni di lettura dei dati della classe dal temp/old
      if (prepare_data(pTempCmd, TEMP) == GS_BAD) break;
      if (prepare_data(pOldCmd, OLD) == GS_BAD) break;

      // preparo istruzione per l'inserimento di record nell'OLD
      if (pOldConn->InitInsRow(info.OldTableRef.get_name(), pInsRs) == GS_BAD) break;

      if (gsc_InitDBReadRow(pInsRs, TempColValues) == GS_BAD) break;
      prb_key_insert = TempColValues.CdrAssoc(info.key_attrib.get_name());

      // ricavo riferimento a tabella OLD dei link
      if (getOldLnkTableRef(LnkTableRef) == GS_BAD) break;

      // preparo RescodSet per l'inserimento di record nell'OLD dei link
      if (pOldConn->InitInsRow(LnkTableRef.get_name(), pInsLnkRs) == GS_BAD) break;

      // ricavo riferimento a tabella TEMP dei link
      if (getTempLnkTableRef(TmpLnkTableRef) == GS_BAD) break;
      
      // preparo l'istruzione per la ricerca delle relazioni sul temp in ordine di
      // codice compl. senza bloccare la tabella
      if (get_all_reldata_from_temp(pTempLnkRs) == GS_BAD) break;

      if (gsc_isEOF(pTempLnkRs) == GS_GOOD) 
      {
         StatusLineMsg.End(gsc_msg(114));	// "100%% elaborato."
         gsc_write_log(_T("100 %% elaborated"));
         result = GS_GOOD;
         break;
      }

      if (gsc_DBReadRow(pTempLnkRs, TempRelColValues) == GS_BAD) break;
      pKeyAttrib = TempRelColValues.CdrAssoc(_T("KEY_ATTRIB"));
      pClassId   = TempRelColValues.CdrAssoc(_T("CLASS_ID"));
      pEntID     = TempRelColValues.CdrAssoc(_T("ENT_ID"));
      pStatus    = TempRelColValues.CdrAssoc(_T("STATUS"));

      if (gsc_rb2Lng(pKeyAttrib, &key_val) == GS_BAD) key_val = 0; // Codice gruppo
      previous_key_val = key_val;

      // se ci sono funzioni di calcolo o di validazione
      toRecalc = (p_attrib_list->is_calculated() == GS_GOOD) ? TRUE : FALSE;
      // se ci sono dei comandi virtualizzati
      if (id.usr_cmds && wcslen(id.usr_cmds) > 0)
      {
         toUseUsrCmds = TRUE;
         // eventuale funzione esterna in completa sostituzione di quella di aggiornamento di GEOsim
         gsc_getAction(id.usr_cmds, STR_SUBST, STR_UPDATE, SubstUpdUsrFun);
         // eventuale funzione esterna prima di quella di aggiornamento di GEOsim
         gsc_getAction(id.usr_cmds, STR_BEFORE, STR_UPDATE, BeforeUpdUsrFun);
         // eventuale funzione esterna dopo quella di aggiornamento di GEOsim
         gsc_getAction(id.usr_cmds, STR_AFTER, STR_UPDATE, AfterUpdFun);
      }
      else
         toUseUsrCmds = FALSE;

      result = GS_GOOD;
      // scorro l'elenco delle relazioni temporanee della classe
      while (1)
      {
         StatusLineMsg.Set(++nn); // "%ld entità GEOsim elaborate."

         // leggo da TEMP
         if (gsc_get_data(pTempCmd, key_val, TempColValues) != GS_GOOD)
            { result = GS_BAD; break; }

         if (is_NewEntity(key_val) == GS_GOOD) // gruppo nuovo
         { 
   		   int      cod_cls_memb, compl_exist = FALSE, qty_skip = 0;
            long     gs_id_memb;
            C_SELSET entSS;
            C_CLASS  *p_class_memb;

   		   // devo verificare se esistono ancora gli  oggetti in grafica
            // vedi caso (ins. civico, ins. via, ins. compl, undo via...)
            // scorro l'elenco delle relazioni temporanee della classe
            do
            {
               gsc_rb2Int(pStatus, &Status); // Stato
               // relazione valida roby
               if (!(Status & ERASED) && !(Status & NOSAVE))
               {
                  compl_exist = TRUE;
                  gsc_rb2Int(pClassId, &cod_cls_memb); // codice classe membro
                  if ((p_class_memb = GS_CURRENT_WRK_SESSION->find_class(cod_cls_memb)) == NULL)
                     { exit = TRUE; break; }
                  if (gsc_rb2Lng(pEntID, &gs_id_memb) == GS_BAD) gs_id_memb = 0;   // codice oggetto membro
                  
                  // cerco se ci sono corrispondenze grafiche
                  if (p_class_memb->get_SelSet(gs_id_memb, entSS, GRAPHICAL) == GS_BAD)
                     { exit = TRUE; break; }
                  if (entSS.length() <= 0) // numero di oggetti grafici
                     { compl_exist = FALSE; break; }
               }

               // leggo la relazione successiva
               if (gsc_Skip(pTempLnkRs) == GS_BAD) break;
					qty_skip++;
               if (gsc_isEOF(pTempLnkRs) == GS_GOOD) break;

               // leggo relazioni temporanee
               if (gsc_DBReadRow(pTempLnkRs, TempRelColValues) == GS_BAD)
                  { result = GS_BAD; break; }
      
               // codice del gruppo
               if (gsc_rb2Lng(pKeyAttrib, &key_val) == GS_BAD) key_val = 0;
            }
            while (previous_key_val == key_val); // sullo stesso gruppo

            // torno al punto di prima
            if (gsc_Skip(pTempLnkRs, -qty_skip) == GS_BAD) { result = GS_BAD; break; }
   		      
            if (exit) break;

            // se esiste ancora il gruppo
            if (compl_exist)
            {
      		   new_ent++;

               gsc_RbSubst(prb_key_insert, ++LastIdFromDB);

               // se esiste un campo calcolato che dipende dal campo chiave oppure
               // almeno un comando utente che ha virtualizzato il comando GEOsim
               if (toRecalc || toUseUsrCmds) 
               {
                  C_SELSET dummySS;
                  C_STRING dummyStr; // Ho deciso che in fase di salvataggio NON devono
                                       // essere richiamate le funzioni di virtualizzazione                               

                  // poichè è cambiato il campo chiave devo aggiornare la scheda
                  // utilizzando una funzione speciale
                  if (gsc_UpdDataForSave(this, TempColValues, dummySS,
                                          dummyStr, dummyStr, dummyStr) == GS_BAD)
                     { result = GS_BAD; break; }
               }

               // aggiungo la scheda nell'OLD con nuova codificazione
               if (gsc_DBInsRow(pInsRs, TempColValues) == GS_BAD)
               {
                  swprintf(Msg, MAX_LEN_MSG, _T("*Errore* Entity %ld not inserted."), key_val);  
                  gsc_write_log(Msg);

                  result = GS_BAD; break;
               }

               // aggiungo i link OLD con la nuova codificazione
               // scorro l'elenco delle relazioni temporanee della classe
               // leggo relazioni temporanee
               if (gsc_DBReadRow(pTempLnkRs, TempRelColValues) == GS_BAD)
                  { result = GS_BAD; break; }
               do
               {
                  gsc_rb2Int(pStatus, &Status); // Stato
                  // relazione valida
                  if (!(Status & ERASED) && !(Status & NOSAVE))
                  {
                     gsc_RbSubst(pKeyAttrib, LastIdFromDB); // nuova codificazione codice gruppo
                     // serve per far saltare il valore STATUS
                     TempRelColValues.getptr_at((4*4)-2)->restype = RTLE;   // 14 elemento

                     if (gsc_DBInsRow(pInsLnkRs, TempRelColValues) == GS_BAD)
                        { TempRelColValues.getptr_at((4*4)-2)->restype = RTLB; exit = TRUE; break; }
                     
                     // serve ripristinare la lista
                     TempRelColValues.getptr_at((4*4)-2)->restype = RTLB;   // 14 elemento
                  }

                  if (gsc_Skip(pTempLnkRs) == GS_BAD) { exit = TRUE; break; }
						if (gsc_isEOF(pTempLnkRs) == GS_GOOD) { exit = TRUE; break; }

                  // leggo relazioni temporanee
                  if (gsc_DBReadRow(pTempLnkRs, TempRelColValues) == GS_BAD)
                     { result = GS_BAD; break; }
      
                  // codice del gruppo
                  if (gsc_rb2Lng(pKeyAttrib, &key_val) == GS_BAD) key_val = 0;
               }
               while (previous_key_val == key_val); // sullo stesso gruppo

               // bisogna modificare eventuali relazioni di secondarie di GEOsim che sono 
               // legate a questa entità per aggiornare la nuova codificazione
               if (gsc_mod_reldata_sec(list_mother_sec, id.code, id.sub_code,
                                       previous_key_val, LastIdFromDB) == GS_BAD)
                  { result = GS_BAD; break; }
            }
         }
         else
         { // entità modificata già esistente sull'OLD
            upd_ent++;

            // leggo da OLD
            if (gsc_get_data(pOldCmd, key_val, pOldRs, &IsOldRsCloseable) != GS_GOOD)
            {  // se l'errore non è perche questo record è stato cancellato
               if (GS_ERR_COD != eGSInvalidKey) { result = GS_BAD; break; }
            }
            else
            {
   				// aggiorno i dati sull'old
               if (gsc_DBUpdRow(pOldRs, TempColValues) == GS_BAD)
               {
                  if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);

                  swprintf(Msg, MAX_LEN_MSG, _T("*Errore* Entity %ld not updated."), key_val);  
                  gsc_write_log(Msg);

                  result = GS_BAD; break;
               }
               if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);

               /////////////////////
               // salvo le relazioni

               // cancello i link precedenti
               statement = _T("DELETE FROM ");
               statement += LnkTableRef;
               statement += _T(" WHERE KEY_ATTRIB=");
               statement += key_val;
               if (pOldConn->ExeCmd(statement) == GS_BAD) return GS_BAD;

               // aggiungo i nuovi link OLD
               // scorro l'elenco delle relazioni temporanee della classe
               // leggo relazioni temporanee
               if (gsc_DBReadRow(pTempLnkRs, TempRelColValues) == GS_BAD)
                  { result = GS_BAD; break; }
               do
               {
                  gsc_rb2Int(pStatus, &Status); // Stato
                  // relazione valida
                  if (!(Status & ERASED) && !(Status & NOSAVE))
                  {
                     // serve per far saltare il valore STATUS
                     TempRelColValues.getptr_at((4*4)-2)->restype = RTLE;   // 14 elemento

                     if (gsc_DBInsRow(pInsLnkRs, TempRelColValues) == GS_BAD)
                        { TempRelColValues.getptr_at((4*4)-2)->restype = RTLB; exit = TRUE; break; }

                     // serve ripristinare la lista
                     TempRelColValues.getptr_at((4*4)-2)->restype = RTLB;   // 14 elemento
                  }
   
                  if (gsc_Skip(pTempLnkRs) == GS_BAD) { exit = TRUE; break; }
						if (gsc_isEOF(pTempLnkRs) == GS_GOOD) { exit = TRUE; break; }

                  // leggo relazioni temporanee
                  if (gsc_DBReadRow(pTempLnkRs, TempRelColValues) == GS_BAD)
                     { result = GS_BAD; break; }
      
                  // codice del gruppo
                  if (gsc_rb2Lng(pKeyAttrib, &key_val) == GS_BAD) key_val = 0;
               }
               while (previous_key_val == key_val); // sullo stesso gruppo
            }
	      }
      
         // gruppo successivo
         while (previous_key_val == key_val && !exit) // sullo stesso gruppo
         {
            if (gsc_Skip(pTempLnkRs) == GS_BAD) { exit = TRUE; break; }
				if (gsc_isEOF(pTempLnkRs) == GS_GOOD) { exit = TRUE; break; }

            // leggo relazioni temporanee
            if (gsc_DBReadRow(pTempLnkRs, TempRelColValues) == GS_BAD)
               { result = GS_BAD; break; }
      
            // codice del gruppo
            if (gsc_rb2Lng(pKeyAttrib, &key_val) == GS_BAD) key_val = 0;
         }

         if (exit) break; // sono finite le relazioni dei gruppi

         previous_key_val = key_val;
         gsc_rb2Int(pStatus, &Status); // 4 elemento
      }  // fine ciclo letture relazioni da temporanee

      if (result == GS_BAD) break;

      StatusLineMsg.End(gsc_msg(310), nn); // "%ld entità GEOsim elaborate."
      result = GS_BAD;
      
      // cancello tutte le schede della tabella TEMP
      if (pTempConn->DelRows(TempTableRef.get_name()) == GS_BAD) break;

      // cancello tutti i link TEMP
      // Poichè i link possono essere stati modificati da un recordset
      // questo fa si che non sia possibile la cancellazione tramite un
      // comando ma solo tramite recordset (baco Microsoft)
      C_STRING statement(_T("SELECT * FROM "));
      _RecordsetPtr pDelRs; 
      statement += TmpLnkTableRef;
      // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
      // in una transazione fa casino (al secondo recordset che viene aperto)
      if (pTempConn->OpenRecSet(statement, pDelRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD) break;
      gsc_DBDelRows(pDelRs); 
      gsc_DBCloseRs(pDelRs);
      // if (pTempConn->DelRows(TmpLnkTableRef.get_name()) == GS_BAD) break;

      result = GS_GOOD;
   }
   while (0);

   // stampa su video e su file (opzionale) il n. di oggetti cancellati, modificati e inseriti
   cls_print_save_report(del_ent, upd_ent, new_ent);

   list_mother_sec.remove_all(); // Rilascio tutte le istruzioni sulle secondarie
   gsc_DBCloseRs(pInsRs);
   gsc_DBCloseRs(pInsLnkRs);

   // salvataggio di eventuali secondarie
   if (gsc_do_save_sec(this) == GS_BAD) return GS_BAD;

   info.TempLastId = LastIdFromDB;

   return result;
}


/*********************************************************/
/*  FINE   FUNZIONI DELLA CATEGORIA C_GROUP              */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_CGRID              */
/*********************************************************/


/**********************************************************/
/*.doc C_CGRID::save_DBdata                    <internal> */
/*+                                                                       
  Salvataggio dati nell'OLD.
  Parametri:
  					
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CGRID::save_DBdata(void)
{
   C_STRING       TempTableRef, statement, FldName;
   C_DBCONNECTION *pTempConn, *pOldConn;
   C_PREPARED_CMD pTempCmd, pOldCmd;
   _RecordsetPtr  pOldRs, pTempRs;
   C_RB_LIST	   ColValues;
   int            result = GS_GOOD;
   TCHAR          Msg[MAX_LEN_MSG];
   long           i = 0, Tot = grid.nx * grid.ny, upd_ent = 0;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1078), id.name); // "Salvataggio dati alfanumerici classe <%s>"

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico abilitazione
   if (gsc_check_op(opSaveArea) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
   if (info.LinkedTable) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

   if (isDataModified() != GS_GOOD) return GS_GOOD;

   acutPrintf(gsc_msg(113), id.name); // "\n\nEntità classe %s:\n"
   swprintf(Msg, MAX_LEN_MSG, _T("Elaborating class <%s>."), id.name);  
   gsc_write_log(Msg);

   // connessioni ai database
   if ((pOldConn = info.getDBConnection(OLD)) == NULL ||
       (pTempConn = info.getDBConnection(TEMP)) == NULL)
      return GS_BAD;

   do
   {
      FldName = info.key_attrib;
      if (gsc_AdjSyntax(FldName, pOldConn->get_InitQuotedIdentifier(),
                        pOldConn->get_FinalQuotedIdentifier(),
                        pOldConn->get_StrCaseFullTableRef()) == GS_BAD)
         { result = GS_BAD; break; }

      statement = _T("SELECT * FROM ");
      statement += info.OldTableRef;
      statement += _T(" WHERE ");
      statement += FldName;
      statement += _T("<>0 ORDER BY ");
      statement += FldName;

      // creo record-set
      // prima era adOpenKeyset ma postgresql in una transazione fa casino
      // poi ho provato con adOpenDynamic ma postgresql dopo 20480 aggiornamenti fa casino
      // e in una transazione fa casino (al secondo recordset che viene aperto)
      if (pOldConn->OpenRecSet(statement, pOldRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD)
         { result = GS_BAD; break; }

      // ricavo riferimento a tabella temporanea
      if (getTempTableRef(TempTableRef) == GS_BAD)
         { result = GS_BAD; break; }

      FldName = info.key_attrib;
      if (gsc_AdjSyntax(FldName, pTempConn->get_InitQuotedIdentifier(),
                        pTempConn->get_FinalQuotedIdentifier(),
                        pTempConn->get_StrCaseFullTableRef()) == GS_BAD)
         { result = GS_BAD; break; }

      statement = _T("SELECT * FROM ");
      statement += TempTableRef;
      statement += _T(" WHERE ");
      statement += FldName;
      statement += _T("<>0 ORDER BY ");
      statement += FldName;

      // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
      // in una transazione fa casino (al secondo recordset che viene aperto)
      if (pTempConn->OpenRecSet(statement, pTempRs, adOpenForwardOnly, adLockReadOnly) == GS_BAD)
         { result = GS_BAD; break; }

      StatusBarProgressMeter.Init(Tot);

      while (gsc_isEOF(pTempRs) == GS_BAD)
      {
         if (gsc_DBReadRow(pTempRs, ColValues) == GS_BAD ||
   	       gsc_DBUpdRow(pOldRs, ColValues) == GS_BAD)
         {
            result = GS_BAD; break;
         }
         
         upd_ent++;
         gsc_Skip(pTempRs);
         gsc_Skip(pOldRs);

         StatusBarProgressMeter.Set(++i);
      }
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   }
   while (0);

   gsc_DBCloseRs(pTempRs);
   gsc_DBCloseRs(pOldRs);

   // stampa su video e su file (opzionale) il n. di oggetti cancellati, modificati e inseriti
   cls_print_save_report(0, upd_ent, 0);

   return result;
}


/*********************************************************/
/*  FINE   FUNZIONI DELLA CATEGORIA C_CGRID              */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_EXTERN             */
/*********************************************************/


/*********************************************************/
/*.doc (new 2) C_EXTERN::save_DBdata <internal> */
/*+                                                                       
  Salvataggio dati nell'OLD.
  Parametri:
  					
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_EXTERN::save_DBdata()
{
   long           NodeMaxValue = 0, LinkMaxValue = 0, AreaMaxValue = 0, i = 0;
   C_SUB          *pSub;
   C_RB_LIST      ColValues;
   int            result = GS_GOOD;
   TCHAR          Msg[MAX_LEN_MSG];

   acutPrintf(gsc_msg(113), ptr_id()->name); // "\n\nEntità classe %s:\n"
   swprintf(Msg, MAX_LEN_MSG, _T("Elaborating class <%s>."), id.name);  
   gsc_write_log(Msg);

   pSub = (C_SUB *) ptr_sub_list()->get_head();
   while (pSub)
   {
      if (pSub->save_DBdata() == GS_BAD) { result = GS_BAD; break; }

      pSub = (C_SUB *) pSub->get_next();
   }

   return result;
}


/*********************************************************/
/*  FINE   FUNZIONI DELLA CATEGORIA C_EXTERN             */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_SUB                */
/*********************************************************/


/*********************************************************/
/*.doc (new 2) C_SUB::save_DBdata <internal> */
/*+                                                                       
  Salvataggio dati nell'OLD la funzione NON salva "ENT_LAST" in 
  GS_CLASS perchè per tutte le sottoclassi "puntuali" c'è un unico codice e
  lo stesso vale per tutte le sottoclassi "lineari".
  					
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SUB::save_DBdata(void)
{
   presbuf             prb_key_insert = NULL;
   int			        result = GS_BAD;
   int                 Prcnt;
   long 		           n_entities, key_val;
   long                del_ent = 0, new_ent = 0, upd_ent = 0;
   C_ATTRIB_LIST       *p_attrib_list = ptr_attrib_list();
   C_RB_LIST	        TempColValues, AdjNodes;
   C_STRING            TempTableRef, CompleteName;
   TCHAR               Msg[MAX_LEN_MSG];
   C_PREPARED_CMD_LIST list_mother_sec;
   ads_name            ent;
   C_SELSET            ClsSS, EntSS;
   C_LINK_SET          LinkSet;
   C_DBCONNECTION      *pTempConn, *pOldConn;
   C_PREPARED_CMD      pTempCmd, pOldCmd;
   int                 IsOldRsCloseable;
   _RecordsetPtr       pInsRs, pOldRs, pTempRs;
   C_TOPOLOGY          TempTopo, OldTopo;

   get_CompleteName(CompleteName);
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1078), CompleteName.get_name()); // "Salvataggio dati alfanumerici classe <%s>"

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico abilitazione
   if (gsc_check_op(opSaveArea) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
   if (info.LinkedTable) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

   if (isDataModified() != GS_GOOD) return GS_GOOD;
   
   acutPrintf(gsc_msg(113), id.name); // "\n\nEntità classe %s:\n"
   swprintf(Msg, MAX_LEN_MSG, _T("Elaborating class <%s>."), id.name);  
   gsc_write_log(Msg);

   // connessioni ai database
   if ((pOldConn = info.getDBConnection(OLD)) == NULL ||
       (pTempConn = info.getDBConnection(TEMP)) == NULL)
      return GS_BAD;

   // ricavo riferimento a tabella temporanea
   if (getTempTableRef(TempTableRef) == GS_BAD) return GS_BAD;

   do
   {
      // cancello dall'OLD le cancellate
      if ((del_ent = delete_2_old()) == -1) break;

      // Creo una lista di istruzioni compilate per poter modificare i link di eventuali
      // secondarie collegate ad un'entità di questa classe
      if (get_list_mother_sec(list_mother_sec) == GS_BAD) break;

      if (id.type == TYPE_POLYLINE)
      {  // Setto la topologia di tipo rete per la simulazione (TEMP e OLD)
         TempTopo.set_type(TYPE_POLYLINE);
         TempTopo.set_cls(GS_CURRENT_WRK_SESSION->find_class(id.code));
         TempTopo.set_temp(GS_GOOD);        
         OldTopo.set_type(TYPE_POLYLINE);
         OldTopo.set_cls(GS_CURRENT_WRK_SESSION->find_class(id.code));
         OldTopo.set_temp(GS_BAD);        
      }

      // preparo istruzione per l'inserimento di record nell'OLD
      if (pOldConn->InitInsRow(info.OldTableRef.get_name(), pInsRs) == GS_BAD) break;

      if (gsc_InitDBReadRow(pInsRs, TempColValues) == GS_BAD) break;
      prb_key_insert = TempColValues.CdrAssoc(info.key_attrib.get_name());

      // Creo il selection set filtrando da GEOsimAppl::SAVE_SS tutti gli oggetti della classe
      if (GEOsimAppl::SAVE_SS.copyIntersectClsCode(ClsSS, id.code, id.sub_code) == GS_BAD)
         break;

      // numero di oggetti grafici
      if ((n_entities = ClsSS.length()) <= 0) { result = GS_GOOD; break; }

      // Compilo le istruzioni di lettura dei dati della classe dal temp
      if (prepare_data(pTempCmd, TEMP) == GS_BAD) break;

      // scorro l'elenco delle entità della classe
      if (ClsSS.entname(0, ent) != GS_GOOD)
      {
         StatusBarProgressMeter.Set_Perc(100);
         gsc_write_log(_T("100 %% elaborated"));
      }

      result = GS_GOOD;
      StatusBarProgressMeter.Init(n_entities);

      // scorro l'elenco delle entità della classe
      while (ClsSS.entname(0, ent) == GS_GOOD)
      {
         // legge il valore chiave e gruppo di selezione di tutti gli oggetti collegati alla scheda
         if (get_Key_SelSet(ent, &key_val, EntSS) != GS_GOOD) { result = GS_BAD; break; }

         // leggo da TEMP
         if (gsc_get_data(pTempCmd, key_val, TempColValues) != GS_GOOD)
            { result = GS_BAD; break; }
        
         if (is_NewEntity(key_val) == GS_GOOD) // entità nuova
         {
		      new_ent++;
            // aggiungo la scheda nel'OLD
            if (gsc_DBInsRow(pInsRs, TempColValues) == GS_BAD)
            {
               swprintf(Msg, MAX_LEN_MSG, _T("*Errore* Entity %ld not inserted."), key_val);  
               gsc_write_log(Msg);

               result = GS_BAD; break;
            }
            
            if (id.type == TYPE_POLYLINE)
            {  // Leggo relazione topologica TEMP
               if ((AdjNodes << TempTopo.elemadj(key_val, TYPE_NODE)) == NULL)
                  { GS_ERR_COD = eGSNoNodeToConnect; result = GS_BAD; break; }
               // aggiungo relazione alla topologia OLD
               if (OldTopo.editlink(id.sub_code, key_val, NULL, AdjNodes) == GS_BAD)
                  { result = GS_BAD; break; }
            }

            // sottraggo dal gruppo di selezione quello dell'entità corrente
            if (ClsSS.subtract(EntSS) != GS_GOOD) { result = GS_BAD; break; }
         }
         else 
         { // entità modificata già esistente sull'OLD
            if (id.type == TYPE_POLYLINE)
            {  // Leggo relazione topologica TEMP
               if ((AdjNodes << TempTopo.elemadj(key_val, TYPE_NODE)) == NULL)
               {
                  swprintf(Msg, MAX_LEN_MSG, _T("Link <%ld> not connected on class <%s>."), key_val, id.name);  
                  gsc_write_log(Msg);
                  GS_ERR_COD = eGSNoNodeToConnect;
                  result = GS_BAD;
                  break;
               }
	            // Aggiorno relazione alla topologia OLD
               if (OldTopo.editlink(id.sub_code, key_val, NULL, AdjNodes) == GS_BAD)
                  { result = GS_BAD; break; }
            }

            // sottraggo dal gruppo di selezione quello dell'entità corrente
            if (ClsSS.subtract(EntSS) != GS_GOOD) { result = GS_BAD; break; }
	      }
      
         Prcnt = (int) ((n_entities - ClsSS.length()) * 100 / n_entities);
         StatusBarProgressMeter.Set_Perc(Prcnt);
      }  // fine ciclo entità  
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   }
   while (0);

   gsc_DBCloseRs(pInsRs);

   if (result == GS_BAD) return GS_BAD;

   // Selezione schede non nuove
   if (get_alldata_from_temp(pTempRs, &(info.OldLastId)) == GS_BAD)
      return GS_BAD;

   // Compilo le istruzioni di lettura dei dati della classe dall'old
   if (prepare_data(pOldCmd, OLD) == GS_BAD) return GS_BAD;

   // ciclo per portare gli aggiornamenti (anche delle schede che non hanno oggetti 
   // grafici visibili) nella tabella OLD
   result = GS_GOOD;
   while (gsc_isEOF(pTempRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pTempRs, TempColValues) == GS_BAD) { result = GS_BAD; break; }

      if (gsc_rb2Lng(prb_key_insert, &key_val) == GS_BAD) key_val = 0;

      // leggo da OLD (roby questa istruz genera "SELECT INVALID SELECT STATEMENT TO FORCE ODBC DRIVER TO UNPREPARED STATE")
      if (gsc_get_data(pOldCmd, key_val, pOldRs, &IsOldRsCloseable) == GS_GOOD)
      {	// aggiorno i dati sull'old
         upd_ent++;
   	   if (gsc_DBUpdRow(pOldRs, TempColValues) == GS_BAD)
         {
            if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);

            swprintf(Msg, MAX_LEN_MSG, _T("*Errore* Entity %ld not updated."), key_val);  
            gsc_write_log(Msg);

            result = GS_BAD; break;
         }
         if (IsOldRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);
      }
      gsc_Skip(pTempRs);
   }
   gsc_DBCloseRs(pTempRs);

   // stampa su video e su file (opzionale) il n. di oggetti cancellati, modificati e inseriti
   cls_print_save_report(del_ent, upd_ent, new_ent);

   if (result == GS_GOOD)
   {
      _CommandPtr pInsCmd;
      C_STRING    statement;

      // Svuoto la tabella temporanea
      if (pTempConn->DelRows(TempTableRef.get_name()) == GS_BAD) return GS_BAD;
      // copio intero contenuto tabella schede OLD in TEMP
      statement = _T("SELECT * FROM ");
      statement += info.OldTableRef.get_name();
      if (pOldConn->OpenRecSet(statement, pOldRs, adOpenForwardOnly, adLockOptimistic, ONETEST) == GS_BAD)
         return GS_BAD;
      if (pTempConn->InitInsRow(TempTableRef.get_name(), pInsCmd) == GS_BAD)
         { gsc_DBCloseRs(pOldRs); return GS_BAD; }
      if (gsc_DBInsRowSet(pInsCmd, pOldRs, ONETEST) == GS_BAD)
         { gsc_DBCloseRs(pOldRs); return GS_BAD; }
      gsc_DBCloseRs(pOldRs);
   }

   // salvataggio di eventuali secondarie
   if (gsc_do_save_sec(this) == GS_BAD) return GS_BAD;

   return result;
}


/*********************************************************/
/*  FINE   FUNZIONI DELLA CATEGORIA C_SUB                */
/*  INIZIO FUNZIONI DELLA CATEGORIA C_SECONDARY          */
/*********************************************************/


/*********************************************************/
/*.doc (new 2)C_SECONDARY::save_DBdata <internal> */
/*+                                                                       
  Salvataggio dati nell'OLD.
  					
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
  N.B.: questa funzione deve essere richiamata solo dopo aver salvato la classe madre !!!
-*/  
/*********************************************************/
int C_SECONDARY::save_DBdata(void)
{
   presbuf		   prb_key_insert = NULL;
   int			   result = GS_BAD, Status, pri_existing, toRecalc; 
   int            IsRsCloseable;
   long 		      LastIdFromDB, key_val, nn = 0, key_pri, prev_key_pri = 0;
   long           del_ent = 0, new_ent = 0, upd_ent = 0;
   C_STRING       TempTableRef, LnkTempTableRef, LnkTableRef;
   C_ATTRIB_LIST  *p_attrib_list = ptr_attrib_list();
   C_RB_LIST	   TempColValues, TempLinkValues;
   presbuf        pKeyPri, pKeyVal, pStatus;
   TCHAR          Msg[MAX_LEN_MSG];
   C_SELSET       entSS;
   C_DBCONNECTION *pTempConn, *pOldConn;
   C_PREPARED_CMD pTempCmd, pOldCmd;
   _CommandPtr    pDelOldLnkCmd;
   _RecordsetPtr  pInsLnkRs, pInsRs, pTempLnkRs, pOldRs;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(1079), name); // "Salvataggio dati alfanumerici tabella secondaria <%s>"

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   // verifico abilitazione
   if (gsc_check_op(opSaveArea) == GS_BAD) return GS_BAD;

   // si puo' modificare solo una tabella secondaria di GEOSIM
   if (type != GSInternalSecondaryTable) return GS_GOOD;

   acutPrintf(_T("\n\n%s%s\n"), gsc_msg(309), name); // "Tabella secondaria: "
   swprintf(Msg, MAX_LEN_MSG, _T("Elaborating secondary table <%s>."), name);
   gsc_write_log(Msg);

   StatusLineMsg.Init(gsc_msg(390), LITTLE_STEP); // ogni 100 "%ld schede secondarie elaborate."

   // connessioni ai database
   if ((pOldConn = info.getDBConnection(OLD)) == NULL ||
       (pTempConn = info.getDBConnection(TEMP)) == NULL)
      return GS_BAD;

   // ricavo riferimento a tabella temporanea (senza crearla)
   if (getTempTableRef(TempTableRef, GS_BAD) == GS_BAD) return GS_BAD;

   // ricavo riferimento a tabella OLD dei link
   if (getOldLnkTableRef(LnkTableRef) == GS_BAD) return GS_BAD;

   if (pTempConn->ExistTable(TempTableRef) == GS_BAD)
   {
      StatusLineMsg.End(gsc_msg(114));	// "100%% elaborato."
      gsc_write_log(_T("100 %% elaborated"));
      // stampa su video e su file (opzionale) il n. di oggetti cancellati, modificati e inseriti
      sec_print_save_report(del_ent, upd_ent, new_ent);

      return GS_GOOD;
   }

   // ricavo riferimento a tabella temporanea dei link
   if (getTempLnkTableRef(LnkTempTableRef) == GS_BAD) return GS_BAD;

   // ultimo codice valido della tabella secondaria interna
   LastIdFromDB = info.getOldLastIdFromDB();
   if (info.OldLastId > LastIdFromDB) LastIdFromDB = info.OldLastId;
   
   do
   {
      // Compilo le istruzioni di lettura dei dati della classe dal temp/old
      if (prepare_data(pTempCmd, TEMP) == GS_BAD) break;
      if (prepare_data(pOldCmd, OLD) == GS_BAD) break;

      // preparo istruzione per l'inserimento di record nell'OLD
      if (pOldConn->InitInsRow(info.OldTableRef.get_name(), pInsRs) == GS_BAD) break;

      if (gsc_InitDBReadRow(pInsRs, TempColValues) == GS_BAD) break;
      prb_key_insert = TempColValues.CdrAssoc(info.key_attrib.get_name());

      // preparo istruzione per l'inserimento link nell'OLD
      if (pOldConn->InitInsRow(LnkTableRef.get_name(), pInsLnkRs) == GS_BAD) break;

      // preparo istruzione per la cancellazione delle relazioni OLD
      if (gsc_prepare_del_old_rel_sec_where_key_attrib(this, pDelOldLnkCmd) == GS_BAD)
         break;

      // preparo l'istruzione per la ricerca delle relazioni sul temp (in ordine di codice principale)
      if (get_all_temp_rel_sec(pTempLnkRs) == GS_BAD) break;

      if (gsc_isEOF(pTempLnkRs) == GS_BAD) 
      {
         if (gsc_DBReadRow(pTempLnkRs, TempLinkValues) == GS_BAD) break;
         // codice della entità principale
         pKeyPri = TempLinkValues.CdrAssoc(_T("KEY_PRI"));
         // codice della secondaria
         pKeyVal = TempLinkValues.CdrAssoc(_T("KEY_ATTRIB"));
         // stato della secondaria
         pStatus = TempLinkValues.CdrAssoc(_T("STATUS"));

         // se ci sono funzioni di calcolo o di validazione
         toRecalc = (p_attrib_list->is_calculated() == GS_GOOD) ? TRUE : FALSE;

         result = GS_GOOD;
         // scorro l'elenco delle relazioni temporanee della classe
         while (1)
         {
            // codice della entità principale
            if (gsc_rb2Lng(pKeyPri, &key_pri) == GS_BAD) key_pri = 0;
            // codice della secondaria
            if (gsc_rb2Lng(pKeyVal, &key_val) == GS_BAD) key_val = 0;
            // stato della secondaria
            if (gsc_rb2Int(pStatus, &Status) == GS_BAD) Status = 0;

            StatusLineMsg.Set(++nn); // "%ld schede secondarie elaborate."

            // secondaria valida da salvare
            if (!(Status & NOSAVE) && Status != UNMODIFIED)
            {
               if (Status & ERASED) // secondaria cancellata
               {
                  if (key_val > 0) // secondaria già esistente nella tabella OLD
                  {
                     // leggo da OLD
                     if (gsc_get_data(pOldCmd, key_val, pOldRs, &IsRsCloseable) == GS_GOOD)
   				      { // la cancello dall'old
   				         del_ent++;
   				         if (gsc_DBDelRow(pOldRs) == GS_BAD)
                        {
                           if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);
                           result = GS_BAD;
                           break;
                        }
                        if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);
   				      }

   				      // cancello anche le relazioni
                     // Setto parametri della istruzione
                     gsc_SetDBParam(pDelOldLnkCmd, 0, key_val); // Codice entità
                     // Esegue il comando 
                     if (gsc_ExeCmd(pDelOldLnkCmd) == GS_BAD) { result = GS_BAD; break; }
                     
                  }
               }
               else // secondaria valida
               { 
                  // leggo da TEMP
                  if (gsc_get_data(pTempCmd, key_val, TempColValues) == GS_BAD)
                     { result = GS_BAD; break; }

                  if (is_NewEntity(key_val) == GS_GOOD) // secondaria nuova
                  { 
      		         // per essere sicuro che le secondarie siano ancora valide
      		         // verifico che la principale esista ancora (vedi caso di entità nuova sulla
      		         // quale viene applicato il comando "erase" oppure un "undo")
                     if (prev_key_pri != key_pri) // è una nuova principale
                     {
                        prev_key_pri = key_pri;

                        if (pCls->ptr_fas()) // se la classe ha rappresentaz. grafica
                        {
                           // cerco se ci sono corrispondenze grafiche
                           if (pCls->get_SelSet(key_pri, entSS, GRAPHICAL) == GS_BAD)
                              { result = GS_BAD; break; }
                           // numero di oggetti grafici associati al linkset
                           pri_existing = (entSS.length() > 0) ? TRUE : FALSE;
                        }
                     }

      		         if (pri_existing)
      		         {
         		         new_ent++;

                        gsc_RbSubst(prb_key_insert, ++LastIdFromDB);

                        if (toRecalc) // poichè è cambiato il campo chiave devo ricalcolare la scheda
                           if (p_attrib_list->calc(info.key_attrib.get_name(),
                                                   TempColValues, MODIFY) == GS_BAD)
                              { result = GS_BAD; break; }

                        // aggiungo la scheda nel'OLD con nuova codificazione
                        if (gsc_DBInsRow(pInsRs, TempColValues) == GS_BAD)
                        {
                           swprintf(Msg, MAX_LEN_MSG, _T("*Errore* Secondary data %ld not inserted."), key_val);  
                           gsc_write_log(Msg);

                           result = GS_BAD; break; }

                        // aggiungo il link OLD con la nuova codificazione
                        gsc_RbSubst(pKeyVal, LastIdFromDB); // codice della secondaria
                        // serve per far saltare il valore STATUS
                        TempLinkValues.getptr_at((5*4)-2)->restype = RTLE;   // 18 elemento
   
                        if (gsc_DBInsRow(pInsLnkRs, TempLinkValues) == GS_BAD)
                        {
                           TempLinkValues.getptr_at((5*4)-2)->restype = RTLB;
                           result = GS_BAD;
                           break;
                        }
                        
                        // serve ripristinare la lista
                        TempLinkValues.getptr_at((5*4)-2)->restype = RTLB;   // 18 elemento
                     }
                  }
                  else
                  { // entità modificata già esistente sull'OLD
                     upd_ent++;

                     // leggo da OLD
                     if (gsc_get_data(pOldCmd, key_val, pOldRs, &IsRsCloseable) == GS_GOOD)
                     {
         				   // aggiorno i dati sull'old
                        if (gsc_DBUpdRow(pOldRs, TempColValues) == GS_BAD)
                        {
                           if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);

                           swprintf(Msg, MAX_LEN_MSG, _T("*Errore* Seconday data %ld not updated."), key_val);  
                           gsc_write_log(Msg);

                           result = GS_BAD; break;
                        }
                        if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pOldRs);
                     }
   	            }
               }
            }      
            if (gsc_Skip(pTempLnkRs) == GS_BAD) { result = GS_BAD; break; }
            if (gsc_isEOF(pTempLnkRs) == GS_GOOD) break;

            // leggo relazioni temporanee
            if (gsc_DBReadRow(pTempLnkRs, TempLinkValues) == GS_BAD)
               { result = GS_BAD; break; }   
         }  // fine ciclo letture relazioni da temporanee

         StatusLineMsg.End(gsc_msg(390), nn); //  "%ld schede secondarie elaborate."
      }
      else result = GS_GOOD;

      if (result == GS_BAD) break;

      result = GS_BAD;

      // stampa su video e su file (opzionale) il n. di oggetti cancellati, modificati e inseriti
      sec_print_save_report(del_ent, upd_ent, new_ent);

      gsc_DBCloseRs(pInsRs);
      gsc_DBCloseRs(pInsLnkRs);
      gsc_DBCloseRs(pTempLnkRs);

      // termino le istruzioni sul TEMP per abilitare cancellazione totale
      // cancello tutte le schede della tabella TEMP
      if (pTempConn->DelRows(TempTableRef.get_name()) == GS_BAD) break;

      // cancello tutte le schede della tabella REL TEMP
      // Poichè i link possono essere stati modificati da un recordset
      // questo fa si che non sia possibile la cancellazione tramite un
      // comando ma solo tramite recordset (baco Microsoft)
      C_STRING statement(_T("SELECT * FROM "));
      _RecordsetPtr pDelRs; 
      
      statement += LnkTempTableRef;
      if (pTempConn->OpenRecSet(statement, pDelRs, adOpenDynamic, adLockOptimistic) == GS_BAD) break;
      gsc_DBDelRows(pDelRs); 
      gsc_DBCloseRs(pDelRs);
      //if (pTempConn->DelRows(LnkTempTableRef.get_name()) == GS_BAD) break;

      result = GS_GOOD;
   }
   while (0);
       
   if (result == GS_GOOD) // aggiorno GS_SEC (LAST_ENT)
   {
      C_RB_LIST pSec;

      if ((pSec << acutBuildList(RTLB, 
                                    RTLB, RTSTR, _T("LAST_ENT"), RTLONG, LastIdFromDB, RTLE,
                                 RTLE, 0)) == NULL)
         return GS_BAD;

      // Modifico riga in GS_SEC
      if (mod_sec_to_gs_sec(pSec) == GS_BAD) return GS_BAD;

      info.OldLastId = info.TempLastId = LastIdFromDB;
   }

   return result;
}


/*********************************************************/
/*.doc (new 2) C_SECONDARY::get_all_temp_rel_sec <internal> */
/*+                                                                       
  Preparo istruzione per la selezione di tutte le relazioni TEMP della
  tabella secondaria relative ad una stessa classe di GEOsim in ordine
  di codice della principale.
  es. ((<prj><cls><sub><key_pri><key_attr><status>)...)
  Parametri:
  _RecordsetPtr &pRs;      RecorsSet

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_SECONDARY::get_all_temp_rel_sec(_RecordsetPtr &pRs)
{                 
   C_STRING       statement, LnkTempTableRef;
   C_DBCONNECTION *pConn;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // ricavo riferimento a tabella temporanea dei link
   if (getTempLnkTableRef(LnkTempTableRef) == GS_BAD) return GS_BAD;

   // leggo record in temp
   statement = _T("SELECT CLASS_ID,SUB_CL_ID,KEY_PRI,KEY_ATTRIB,STATUS FROM ");
   statement += LnkTempTableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += pCls->ptr_id()->code;
   statement += _T(" AND SUB_CL_ID=");
   statement += pCls->ptr_id()->sub_code;
   statement += _T(" ORDER BY KEY_PRI");

   if ((pConn = ptr_info()->getDBConnection(TEMP)) == NULL) return GS_BAD;

   // creo record-set
   // prima era adOpenKeyset ma postgresql in una transazione fa casino
   return pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockOptimistic);
}


/*********************************************************/
/*.doc (new 2) gsc_prepare_del_old_sec_where_key_pri <internal> */
/*+                                                                       
  Preparo istruzione per la cancellazione delle schede secondarie OLD 
  legate ad una scheda di entità di GEOsim.
  Parametri:
  C_SECONDARY *pSec;     Puntatore a secondaria
  _CommandPtr &pCmd;    Comando preparato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_prepare_del_old_sec_where_key_pri(C_SECONDARY *pSec, _CommandPtr &pCmd)
{                 
   C_DBCONNECTION *pConn;
   C_INFO_SEC     *pInfo = pSec->ptr_info();
   C_STRING       statement, LnkTableRef, FldName;
   _ParameterPtr  pParam;
   C_ATTRIB       *pAttrib;

   if (pSec->getOldLnkTableRef(LnkTableRef) == GS_BAD) return GS_BAD;

   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

   FldName = pInfo->key_attrib;
   if (gsc_AdjSyntax(FldName, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   statement = _T("DELETE FROM ");
   statement += pInfo->OldTableRef;
   statement += _T(" WHERE ");
   statement += FldName;
   statement += _T(" IN (SELECT KEY_ATTRIB FROM ");
   statement += LnkTableRef;
   statement += _T(" WHERE KEY_PRI=?)");

   // Attributo chiave di ricerca
   pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(pInfo->key_attrib.get_name(), FALSE);
   if (pAttrib->init_ADOType(pConn) == NULL) return GS_BAD;
   
   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), pAttrib->ADOType) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) gsc_prepare_del_old_rel_sec_where_key_pri <internal> */
/*+                                                                       
  Preparo istruzione per la cancellazione delle relazioni OLD legate ad una
  scheda di GEOsim.
  Parametri:
  C_SECONDARY *pSec;     Puntatore a secondaria
  _CommandPtr &pCmd;     Comando preparato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_prepare_del_old_rel_sec_where_key_pri(C_SECONDARY *pSec, _CommandPtr &pCmd)
{                 
   C_DBCONNECTION *pConn;
   C_INFO_SEC     *pInfo = pSec->ptr_info();
   C_STRING       statement, LnkTableRef;
   _ParameterPtr  pParam;
   C_ATTRIB       *pAttrib;
   
   if (pSec->getOldLnkTableRef(LnkTableRef) == GS_BAD) return GS_BAD;

   statement = _T("DELETE FROM ");
   statement += LnkTableRef;
   statement += _T(" WHERE KEY_PRI=?");

   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

   // Attributo chiave di ricerca
   pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(pInfo->key_attrib.get_name(), FALSE);
   if (pAttrib->init_ADOType(pConn) == NULL) return GS_BAD;
   
   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), pAttrib->ADOType) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc (new 2) gsc_prepare_del_old_rel_sec_where_key_attrib <internal> */
/*+                                                                       
  Preparo istruzione per la cancellazione di una relazione OLD.
  Parametri:
  C_SECONDARY *pSec;             puntatore a secondaria
  _CommandPtr &pCmd;	            Comando preparato

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_prepare_del_old_rel_sec_where_key_attrib(C_SECONDARY *pSec, _CommandPtr &pCmd)
{
   C_INFO_SEC     *pInfo = pSec->ptr_info();
   C_STRING       statement, LnkTableRef;
   C_DBCONNECTION *pConn;
   _ParameterPtr  pParam;
   C_ATTRIB       *pAttrib;
   
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   
   // ricavo riferimento a tabella OLD dei link
   if (pSec->getOldLnkTableRef(LnkTableRef) == GS_BAD) return GS_BAD;

   statement = _T("DELETE FROM ");
   statement += LnkTableRef;
   statement += _T(" WHERE KEY_ATTRIB=?");

   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

   // Attributo chiave di ricerca
   pAttrib = (C_ATTRIB *) pSec->ptr_attrib_list()->search_name(pInfo->key_attrib.get_name(), FALSE);
   if (pAttrib->init_ADOType(pConn) == NULL) return GS_BAD;

   // preparo comando SQL
   if (pConn->PrepareCmd(statement, pCmd) == GS_BAD) return GS_BAD;

   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), pAttrib->ADOType) == GS_BAD) return GS_BAD;
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_do_save_sec <external> */
/*+                                                                       
  Effettua il salvataggio delle secondarie di una classe.
  Parametri:
  C_CLASS *pCls;		    Classe madre da salvare

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_do_save_sec(C_CLASS *pCls)
{
   C_ID        *p_id;
   C_SECONDARY *psecondary;

   if (pCls == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   p_id = pCls->ptr_id();

   if (p_id->category == CAT_SPAGHETTI) return GS_GOOD; // SPAGHETTI

   if (pCls->ptr_secondary_list == NULL) return GS_BAD;
   psecondary = (C_SECONDARY *) pCls->ptr_secondary_list->get_head();
   while (psecondary)
   {
      if (psecondary->get_type() == GSInternalSecondaryTable) // se secondaria di GEOsim
         if (psecondary->save_DBdata() == GS_BAD) return GS_BAD;
      
      psecondary = (C_SECONDARY *) pCls->ptr_secondary_list->get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_CLASS::get_list_mother_sec <internal> */
/*+                                                                       
  Creo lista di comandi preparati relativi alle tabelle temporanee
  secondarie aventi questa classe come classe madre.
  Parametri:
  C_PREPARED_CMD_LIST &list_mother;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_CLASS::get_list_mother_sec(C_PREPARED_CMD_LIST &list_mother)
{
   C_PREPARED_CMD *pCmd;
   C_STRING       TempTableRef;
   C_SECONDARY    *psecondary;
   C_DBCONNECTION *pConn;
   C_INFO         *pInfo;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if ((pInfo = ptr_info()) == NULL) { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
   if (ptr_secondary_list == NULL) return GS_BAD;

   if ((pConn = pInfo->getDBConnection(TEMP)) == NULL) return GS_BAD;

   psecondary = (C_SECONDARY *) ptr_secondary_list->get_head();
   while (psecondary)
   {
      if (psecondary->get_type() == GSInternalSecondaryTable) // se secondaria di GEOsim
      {
         // ricavo riferimento a tabella temporanea (senza crearla)
         if (psecondary->getTempTableRef(TempTableRef, GS_BAD) == GS_BAD) return GS_BAD;
         if (pConn->ExistTable(TempTableRef) == GS_GOOD)
         {
   	      if ((pCmd = new C_PREPARED_CMD) == NULL)
               { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

            pCmd->set_key(psecondary->code);
            if (psecondary->prepare_reldata_where_keyPri(*pCmd, TEMP) == GS_BAD)
		         return GS_BAD;
            list_mother.add_tail(pCmd);
         }
      }

      psecondary = (C_SECONDARY *) ptr_secondary_list->get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_GetListDelStmOldSec                <internal> */
/*+                                                                       
  Preparo lista di comandi compilate relative alla cancellazione delle
  secondarie OLD di una classe partendo dal codice della principale.
  Parametri:
  C_CLASS           *pCls;
  C_PREPARED_CMD_LIST &CmdList;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_GetListDelStmOldSec(C_CLASS *pCls, C_PREPARED_CMD_LIST &CmdList)
{
   C_PREPARED_CMD *pCmd;
   C_SECONDARY    *psecondary;

   if (!pCls) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   CmdList.remove_all();

   if (pCls->ptr_secondary_list == NULL) return GS_GOOD;

   psecondary = (C_SECONDARY *) pCls->ptr_secondary_list->get_head();
   while (psecondary)
   {
      if (psecondary->get_type() == GSInternalSecondaryTable) // se secondaria di GEOsim
      {
	      if ((pCmd = new C_PREPARED_CMD) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         // preparo l'istruzione per la ricerca dei dati sull'old       
         if (gsc_prepare_del_old_sec_where_key_pri(psecondary, pCmd->pCmd) == GS_BAD)
            { delete pCmd; return GS_BAD; }
         // aggiungo alla lista delle istruzioni compilate
         CmdList.add_tail(pCmd);
      }

      psecondary = (C_SECONDARY *) pCls->ptr_secondary_list->get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_GetListDelStmOldRelSec <internal> */
/*+                                                                       
  Preparo lista di istruzioni compilate relative alla cancellazione delle
  secondarie OLD di una classe.
  Parametri:
  C_CLASS             *pCls;
  C_PREPARED_CMD_LIST &CmdList;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_GetListDelStmOldRelSec(C_CLASS *pCls, C_PREPARED_CMD_LIST &CmdList)
{
   C_PREPARED_CMD *pCmd;
   C_SECONDARY    *psecondary;

   if (!pCls) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   CmdList.remove_all();

   if (pCls->ptr_secondary_list == NULL) return GS_GOOD;

   psecondary = (C_SECONDARY *) pCls->ptr_secondary_list->get_head();
   while (psecondary)
   {
      if (psecondary->get_type() == GSInternalSecondaryTable) // se secondaria di GEOsim
      {
	      if ((pCmd = new C_PREPARED_CMD) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         // preparo l'istruzione per la ricerca dei dati sull'old       
         if (gsc_prepare_del_old_rel_sec_where_key_pri(psecondary, pCmd->pCmd) == GS_BAD)
            { delete pCmd; return GS_BAD; }
         // aggiungo alla lista delle istruzioni compilate
         CmdList.add_tail(pCmd);
      }

      psecondary = (C_SECONDARY *) pCls->ptr_secondary_list->get_next();
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_mod_reldata_sec <internal> */
/*+                                                                       
  Modifico il codice dell'entità madre delle secondarie usando il cursore e la statement
  preparate nella funzione <get_list_mother_group>.
  Parametri:
  C_PREPARED_CMD_LIST &list_mother; Lista di istruzioni compilate
  int 		          Cls;          Codice classe
  int 		          Sub;          Codice sottoclasse
  long 		          Key;          Codice entità
  long                NewKey;       Nuovo codice entità
  int                 Mode;         Flag se modo = ERASED (segna le relazioni come cancellate)
                                    collegate alla entità con codice gs_id; se modo = MODIFIED
                                    aggiorna i codici dei campi key_pri (default = MODIFIED)
  long                *qty;         n. secondarie elaborate (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_mod_reldata_sec(C_PREPARED_CMD_LIST &list_mother, int Cls, int Sub,
                        long Key, long NewKey, int Mode, long *qty)
{          
   C_PREPARED_CMD *pCmd;
   _RecordsetPtr  pRs;
   presbuf        pCls, pSub, pStatus, pKeyPri;
   C_RB_LIST      ColValues;
   int            Status, IsRsCloseable, _Cls, _Sub;
   long           _Key;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   if (qty) *qty = 0;

	// per ogni tabella secondaria aggiorno le relazioni
	pCmd = (C_PREPARED_CMD *) list_mother.get_head();
   while (pCmd)
   {  // se fallisce per un motivo diverso da eGSInvalidKey
      if (gsc_get_reldata(*pCmd, Cls, Sub, Key, pRs, &IsRsCloseable) == GS_BAD)
      {
         pCmd = (C_PREPARED_CMD *) list_mother.get_next();
         continue;
      }

      if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
         { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }

      pCls    = ColValues.CdrAssoc(_T("CLASS_ID"));
      pSub    = ColValues.CdrAssoc(_T("SUB_CL_ID"));
      // status della entità secondaria 
      pStatus = ColValues.CdrAssoc(_T("STATUS"));
      pKeyPri = ColValues.CdrAssoc(_T("KEY_PRI"));

      // per ogni relazione
      while (gsc_isEOF(pRs) == GS_BAD)
      {
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
            { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }

         if (gsc_rb2Int(pCls, &_Cls) == GS_BAD || _Cls != Cls ||
             gsc_rb2Int(pSub, &_Sub) == GS_BAD || _Sub != Sub ||
             gsc_rb2Lng(pKeyPri, &_Key) == GS_BAD || _Key != Key)
            break;

         gsc_rb2Int(pStatus, &Status);
         // se è stata cancellata oppure non è da considerare 
         if (Status & ERASED || Status & NOSAVE)
         {
            gsc_Skip(pRs);
            continue;
         }

         if (Mode == ERASED)
            gsc_RbSubst(pStatus, ERASED); // la segno come cancellata
         else
            gsc_RbSubst(pKeyPri, NewKey); // modifico codice entità madre della secondaria

         // aggiorno riga
         if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD)
            { if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs); return GS_BAD; }

         if (qty) (*qty)++; 
         gsc_Skip(pRs);
      }
      if (IsRsCloseable == GS_GOOD) gsc_DBCloseRs(pRs);
      
      pCmd = (C_PREPARED_CMD *) list_mother.get_next();
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*  FINE   FUNZIONI DELLA CATEGORIA C_SECONDARY          */
/*********************************************************/


/*************************************************************/
/*.doc (new 2) cls_print_save_report              <internal> */
/*+                                                                       
  Funzione che stampa a video il risultato del salvataggio di classi
  Parametri:
  long del_ent;   n. entità cancellate
  long upd_ent;   n. entità aggiornate
  long new_ent;   n. entità inserite
-*/  
/*************************************************************/
void cls_print_save_report(long del_ent, long upd_ent, long new_ent)
{
   TCHAR Msg[MAX_LEN_MSG];

   acutPrintf(gsc_msg(124), del_ent); // "\n%ld entità GEOsim cancellate."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld erased entities."), del_ent);  
   gsc_write_log(Msg);
   acutPrintf(gsc_msg(125), upd_ent); // "\n%ld entità GEOsim aggiornate."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld updated entities."), upd_ent);  
   gsc_write_log(Msg);
   acutPrintf(gsc_msg(126), new_ent); // "\n%ld entità GEOsim inserite."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld inserted entities."), new_ent);  
   gsc_write_log(Msg);
}


/*************************************************************/
/*.doc (new 2) sec_print_save_report              <internal> */
/*+                                                                       
  Funzione che stampa a video il risultato del salvataggio di secondarie
  Parametri:
  long del_ent;   n. schede secondarie cancellate
  long upd_ent;   n. schede secondarie aggiornate
  long new_ent;   n. schede secondarie inserite
-*/  
/*************************************************************/
void sec_print_save_report(long del_ent, long upd_ent, long new_ent)
{
   TCHAR Msg[MAX_LEN_MSG];

   acutPrintf(gsc_msg(277), del_ent); // "\n%ld schede secondarie GEOsim cancellate."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld erased secondary record."), del_ent);  
   gsc_write_log(Msg);
   acutPrintf(gsc_msg(278), upd_ent); // "\n%ld schede secondarie GEOsim aggiornate."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld updated record."), upd_ent);  
   gsc_write_log(Msg);
   acutPrintf(gsc_msg(279), new_ent); // "\n%ld schede secondarie GEOsim inserite."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld inserted record."), new_ent);  
   gsc_write_log(Msg);
}