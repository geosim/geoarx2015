/**********************************************************
Name: GS_GPHDATA

Module description: File contenente le funzioni per 
                    la gestione dei dati grafici in formato DB
            
Author: Roberto Poltini

(c) Copyright 1996-2015 by IREN ACQUA GAS S.p.a. Genova


Modification history:
              
Notes and restrictions on use: 


**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include "MapODRecord.h"

#include "gs_opcod.h"
#include "gs_error.h"
#include "gs_utily.h"
#include "gs_resbf.h"
#include "gs_netw.h"      // funzioni di rete
#include "gs_init.h"                  
#include "gs_dbref.h"
#include "gs_ade.h"
#include "gs_thm.h"       // gestione tematismi e sistemi di coordinate
#include "gs_attbl.h"
#include "gs_query.h"
#include "gs_topo.h"      // per topologia simulazioni
#include "gs_gphdata.h"   // gestione dati grafici


#if defined(GSDEBUG) // se versione per debugging
   #include <sys/timeb.h>  // Solo per debug
   #include <time.h>       // Solo per debug
   double  gph_tempo=0, gph_tempo1=0, gph_tempo2=0, gph_tempo3=0, gph_tempo4=0, gph_tempo5=0, gph_tempo6=0, gph_tempo7=0;
   double  gph_tempo8=0, gph_tempo9=0, gph_tempo10=0, gph_tempo11=0, gph_tempo12=0, gph_tempo13=0, gph_tempo14=0;
#endif


///////////////////////////////////////////////////////////////////////////
// INIZIO FUNZIONI PRIVATE
///////////////////////////////////////////////////////////////////////////


 void gsc_geom_print_save_report(long deleted, long updated, long inserted);
 void gsc_label_print_save_report(long deleted, long updated, long inserted);


 /*************************************************************/
/*.doc cls_print_save_report                       <internal> */
/*+                                                                       
  Funzione che stampa a video il risultato del salvataggio di classi
  Parametri:
  long deleted;   n. entità cancellate
  long updated;   n. entità aggiornate
  long inserted;   n. entità inserite
-*/  
/*************************************************************/
void gsc_geom_print_save_report(long deleted, long updated, long inserted)
{
   TCHAR Msg[MAX_LEN_MSG];

   acutPrintf(gsc_msg(495), deleted); // "\n%ld oggetti grafici GEOsim cancellati."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld erased graphical objects."), deleted);  
   gsc_write_log(Msg);
   acutPrintf(gsc_msg(496), updated); // "\n%ld oggetti grafici GEOsim aggiornati."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld updated graphical objects."), updated);  
   gsc_write_log(Msg);
   acutPrintf(gsc_msg(497), inserted); // "\n%ld oggetti grafici GEOsim inseriti."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld inserted graphical objects."), inserted);  
   gsc_write_log(Msg);
}
void gsc_label_print_save_report(long deleted, long updated, long inserted)
{
   TCHAR Msg[MAX_LEN_MSG];

   acutPrintf(gsc_msg(498), deleted); // "\n%ld blocchi etichette GEOsim cancellati."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld erased label blocks."), deleted);  
   gsc_write_log(Msg);
   acutPrintf(gsc_msg(499), updated); // "\n%ld blocchi etichette GEOsim aggiornati."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld updated label blocks."), updated);  
   gsc_write_log(Msg);
   acutPrintf(gsc_msg(500), inserted); // "\n%ld blocchi etichette GEOsim inseriti."
   swprintf(Msg, MAX_LEN_MSG, _T("%ld inserted label blocks."), inserted);  
   gsc_write_log(Msg);
}


/*********************************************************/
/*.doc gsc_PrintBackUpMsg                     <internal> */
/*+
  Questa funzione stampa messaggi relativi al backup dei dati.
  Parametri:
  BackUpModeEnum Mode;
-*/  
/*********************************************************/
void gsc_PrintBackUpMsg(BackUpModeEnum Mode)
{
   switch (Mode)
   {
      case GSCreateBackUp: // crea backup
         acutPrintf(gsc_msg(478)); // "\nCreazione backup dati geometrici..."
         break;
      case GSRemoveBackUp: // cancella backup 
         acutPrintf(gsc_msg(479)); // "\nCancellazione backup dati geometrici..."
         break;
      case GSRestoreBackUp: // ripristina backup
         acutPrintf(gsc_msg(480)); // "\nRipristino backup dati geometrici..."
         break;
   }
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI PRIVATE
// INIZIO FUNZIONI C_GPH_OBJ_ARRAY
///////////////////////////////////////////////////////////////////////////


// costruttore
C_GPH_OBJ_ARRAY::C_GPH_OBJ_ARRAY() 
{
   mReleaseAllAtDistruction = true;
}

// distruttore
C_GPH_OBJ_ARRAY::~C_GPH_OBJ_ARRAY()
{
   if (mReleaseAllAtDistruction)
      removeAll();
}

int C_GPH_OBJ_ARRAY::length(void)
{
   if (ObjType == AcDbObjectIdType)
   {
      int Tot = 0;

      C_DWG *pDwg = (C_DWG *) DwgList.get_head();

      while (pDwg)
      {
         Tot += pDwg->ptr_ObjectIdArray()->length();
         
         pDwg = (C_DWG *) DwgList.get_next();
      }

      return Tot;
   }
   else
      return Ents.length();
}

void C_GPH_OBJ_ARRAY::removeAll(void)
{
   int        i;
   AcDbEntity *pEnt;
   C_INT_LONG *p;
   C_DWG      *pDwg = (C_DWG *) DwgList.get_head();

   while (pDwg)
   {
      pDwg->clear_ObjectIdArray();
      
      pDwg = (C_DWG *) DwgList.get_next();
   }

   p = (C_INT_LONG *) KeyEnts.get_head();
   for (i = 0; i < Ents.length(); i++)
   {
      if ((pEnt = Ents.at(i)))
         if (p)
         {
            // Se l'entità è da rilasciare (== 0)
            if (p->get_key() == 0) delete pEnt;
            p = (C_INT_LONG *) KeyEnts.get_next();
         }
         else
            delete pEnt;
   }
   Ents.removeAll();

   KeyEnts.remove_all();
}

///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_GPH_OBJ_ARRAY
// INIZIO FUNZIONI C_GPH_INFO
///////////////////////////////////////////////////////////////////////////


// costruttore
C_GPH_INFO::C_GPH_INFO() 
{
   prj = 0;
   cls = 0;
   sub = 0;
}

// distruttore
C_GPH_INFO::~C_GPH_INFO() {}

GraphDataSourceEnum C_GPH_INFO::getDataSourceType(void) { return GSNoneGphDataSource; }

int C_GPH_INFO::copy(C_GPH_INFO* out) 
{
   if (!out) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   out->prj = prj;
   out->cls = cls;
   out->sub = sub;
   out->coordinate_system = coordinate_system;

   return GS_GOOD;
}

int C_GPH_INFO::ToFile(C_STRING &filename, const TCHAR *sez)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   bool                    Unicode = false;

   if (gsc_path_exist(filename) == GS_GOOD)
      if (gsc_read_profile(filename, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   if (ToFile(ProfileSections, sez) == GS_BAD) return GS_BAD;

   return gsc_write_profile(filename, ProfileSections, Unicode);
}
int C_GPH_INFO::ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_STRING Buffer;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez)))
   {
      if (ProfileSections.add(sez) == GS_BAD) return GS_BAD;
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();
   }

   ProfileSection->set_entry(_T("GPH_INFO.PRJ"), prj);
   ProfileSection->set_entry(_T("GPH_INFO.CLS"), cls);
   ProfileSection->set_entry(_T("GPH_INFO.SUB"), sub);
  
   Buffer = (coordinate_system.get_name()) ? coordinate_system.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.COORDINATE_SYSTEM"), Buffer.get_name());

   return GS_GOOD;
}

int C_GPH_INFO::load(C_STRING &filename, const TCHAR *sez)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   
   if (gsc_read_profile(filename, ProfileSections) == GS_BAD) return GS_BAD;
   return load(ProfileSections, sez);
}
int C_GPH_INFO::load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez) 
{
   C_BPROFILE_SECTION *ProfileSection;
   C_2STR_BTREE       *pProfileEntries;
   C_B2STR            *pProfileEntry;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez))) return GS_CAN;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   // codice progetto (obbligatorio)
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.PRJ")))) return GS_CAN;
   prj = _wtoi(pProfileEntry->get_name2());

   // codice classe (obbligatorio)
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.CLS")))) return GS_CAN;
   cls = _wtoi(pProfileEntry->get_name2());

   // codice sotto-classe (obbligatorio)
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.SUB")))) return GS_CAN;
   sub = _wtoi(pProfileEntry->get_name2());

   // sistema di coordinate
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.COORDINATE_SYSTEM"))))
      coordinate_system = pProfileEntry->get_name2();

   return GS_GOOD; 
}


resbuf *C_GPH_INFO::to_rb(bool ConvertDrive2nethost, bool ToDB) 
{
   C_RB_LIST List;

   if (ToDB)
      if ((List << acutBuildList(RTLB, // non scrivo volutamente prj
                                    RTSTR, _T("CLASS_ID"),
                                    RTSHORT, cls,
                                 RTLE,
                                 RTLB,
                                    RTSTR, _T("SUB_CL_ID"),
                                    RTSHORT, sub,
                                 RTLE, 0)) == NULL) return NULL;

   if ((List += acutBuildList(RTLB,
                                 RTSTR, _T("COORDINATE"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(coordinate_system)) == NULL) return NULL;
   if ((List += acutBuildList(RTLE, 0)) == NULL) return NULL;  
   List.ReleaseAllAtDistruction(GS_BAD);

   return List.get_head(); 
}
int C_GPH_INFO::from_rb(C_RB_LIST &ColValues)
{
   return from_rb(ColValues.get_head());
}
int C_GPH_INFO::from_rb(resbuf *rb)
{
   presbuf p;

   if ((p = gsc_CdrAssoc(_T("PRJ"), rb, FALSE)))
      if (gsc_rb2Int(p, &prj) == GS_BAD) return GS_BAD;

   if ((p = gsc_CdrAssoc(_T("CLASS_ID"), rb, FALSE)))
      if (gsc_rb2Int(p, &cls) == GS_BAD) return GS_BAD;

   if ((p = gsc_CdrAssoc(_T("SUB_CL_ID"), rb, FALSE)))
      if (gsc_rb2Int(p, &sub) == GS_BAD) return GS_BAD;

   if ((p = gsc_CdrAssoc(_T("COORDINATE"), rb, FALSE)))
      if (p->restype == RTSTR) 
      {
         coordinate_system = p->resval.rstring;
         coordinate_system.alltrim();
      }
      else coordinate_system.clear();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_GPH_INFO::to_db                    <internal> */
/*+
  Questa funzione scrive i dati di una C_GPH_INFO nella
  tabella CLASS_GPH_DATA_SRC_TABLE_NAME.
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GPH_INFO::to_db(void)
{
   C_PROJECT      *pPrj;
   C_DBCONNECTION *pConn;
   C_STRING       MetaTableRef, statement;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;

   if ((ColValues << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;
   if ((ColValues += to_rb(true, true)) == NULL) return GS_BAD;
   if ((ColValues += acutBuildList(RTLE, 0)) == NULL) return GS_BAD;

   // cerco elemento in lista progetti
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL) return GS_BAD;

   // setto il riferimento di CLASS_GPH_DATA_SRC_TABLE_NAME (<catalogo>.<schema>.<tabella>)
   if (pPrj->getClassGphDataSrcTabInfo(&pConn, &MetaTableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += MetaTableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += cls;
   statement += _T(" AND SUB_CL_ID=");
   statement += sub;

   // leggo la riga della tabella bloccandola in modifica
   if (pConn->ExeCmd(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD) return GS_BAD;
   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      gsc_DBCloseRs(pRs);
      // inserisco nuova riga
      if (pConn->InsRow(MetaTableRef.get_name(), ColValues) == GS_BAD) return GS_BAD;
   }
   else
   {
      // modifico la riga del cursore
      if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) return GS_BAD;
      if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_GPH_INFO::del_db                     <internal> */
/*+
  Questa funzione cancella i dati di una C_GPH_INFO dalla
  tabella CLASS_GPH_DATA_SRC_TABLE_NAME.
  Parametri:
  bool DelResource;  Opzionale; flag di rimozione delle risorse (cartelle disegni...)
                     (default = false)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GPH_INFO::del_db(bool DelResource)
{
   C_PROJECT      *pPrj;
   C_DBCONNECTION *pConn;
   C_STRING       MetaTableRef;
   _RecordsetPtr  pRs;

   // cerco elemento in lista progetti
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL) return GS_BAD;

   // setto il riferimento di GS_CLASS_GRAPH_INFO (<catalogo>.<schema>.<tabella>)
   if (pPrj->getClassGphDataSrcTabInfo(&pConn, &MetaTableRef) == GS_BAD) return GS_BAD;

   // blocco riga in GS_CLASS_GRAPH_INFO
   if (gsc_lock_on_gs_class_graph_info(pConn, MetaTableRef, cls, sub, pRs) == GS_BAD)
      return GS_BAD;

   if (DelResource)
   {
      C_RB_LIST ColValues;

      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_unlock_on_gs_class_graph_info(pConn, pRs, READONLY); return GS_BAD; }

      if (from_rb(ColValues) == GS_BAD)
         { gsc_unlock_on_gs_class_graph_info(pConn, pRs, READONLY); return GS_BAD; }

      if (RemoveResource() == GS_BAD)
         { gsc_unlock_on_gs_class_graph_info(pConn, pRs, READONLY); return GS_BAD; }
   }

   // cancello la riga in CLASS_GPH_DATA_SRC_TABLE_NAME
   gsc_DBDelRow(pRs);

   // sblocco CLASS_GPH_DATA_SRC_TABLE_NAME
   return gsc_unlock_on_gs_class_graph_info(pConn, pRs);
}


/*********************************************************/
/*.doc C_GPH_INFO::isValid                    <external> */
/*+
  Questa funzione verifica la correttezza della C_GPH_INFO.
  Parametri:
  int GeomType;      Usato per compatibilità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GPH_INFO::isValid(int GeomType)
{
   // verifico validità coord
   return gsc_validcoord(coordinate_system.get_name());
}
int C_GPH_INFO::CreateResource(int GeomType)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_GPH_INFO::RemoveResource(void)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
bool C_GPH_INFO::IsTheSameResource(C_GPH_INFO *p)
   { GS_ERR_COD = eGSInvClassType; return false; }
bool C_GPH_INFO::ResourceExist(bool *pGeomExist, bool *pLblGroupingExist, bool *pLblExist)
{
   if (pGeomExist) *pGeomExist = false;
   if (pLblGroupingExist) *pLblGroupingExist = false;
   if (pLblExist) *pLblExist = false;
   GS_ERR_COD = eGSInvClassType;
   return false;
}


/*********************************************************/
/*.doc C_GPH_INFO::QueryClear                 <internal> */
/*+
  Questa funzione pulisce la condizione di query da applicare 
  alla fonte dati grafica (si usa la query ADE corrente).
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GPH_INFO::QueryClear(void)
{
   return (ade_qryclear() != RTNORM) ? GS_BAD : GS_GOOD;
}


/*********************************************************/
/*.doc C_GPH_INFO::LoadQueryFromCls           <internal> */
/*+
  Questa funzione carica la condizione di query impostata per la classe
  di GEOsim da applicare alla fonte dati grafica.
  Viene cancellata, se esistente, la query precedente dopodichè viene impostata
  la condizione spaziale e, se viene passata una lista di codici di entità
  da ricercare (lista non vuota), viene impostata una condizione partendo
  dalla posizione del cursore corrente nella lista considerando un certo numero di
  codici (vedi MAX_GRAPH_CONDITIONS). Se invece la lista dei codici di entità è 
  vuota allora viene impostata la query SQL e determinata la lista dei codici di
  entità che soddisfano la query impostando la condizione partendo
  dall'inizio della lista considerando un certo numero di codici (vedi MAX_GRAPH_CONDITIONS).
  Parametri:
  TCHAR *qryName;          Nome della query spaziale
  C_STR_LIST &IdList;      Lista dei codici di entità da estrarre                           

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GPH_INFO::LoadQueryFromCls(TCHAR *qryName, C_STR_LIST &IdList)
{
   C_STRING SQL;

   // Se la lista dei codici delle entità da cercare è vuota
   if (IdList.get_count() == 0)
   {
      // carico la query SQL
      if (gsc_ClsSQLQryLoad(cls, sub, SQL) == GS_GOOD)
      {       
         // leggo i codici delle entità che soddisfano la query SQL sulla tabella OLD
         if (gsc_getKeyListFromASISQLonOld(cls, sub, SQL.get_name(), IdList) == GS_BAD)
            return GS_BAD;
         if (IdList.get_count() == 0) return GS_GOOD;
      }
      IdList.get_head(); // mi posiziono all'inizio della lista
   }

   // carico e attivo la query spaziale cancellando la query precedente
   if (LoadSpatialQueryFromADEQry(qryName) == GS_BAD) return GS_BAD;

   // definisco la query per OD
   if (AddQueryFromEntityIds(IdList) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


int C_GPH_INFO::LoadSpatialQueryFromADEQry(TCHAR *qryName)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


/*********************************************************/
/*.doc C_GPH_INFO::AddQueryFromEntityIds      <internal> */
/*+
  Questa funzione aggiunge in "and" alla query corrente la condizione necessaria
  per cercare gli oggetti appartenenti ad una lista di ID sotto forma di una
  C_STR_LIST passata come parametro (utilizza il codice dell'entità).
  Parametri:
  C_STR_LIST &IdList;   Lista dei codici chiave degli oggetti da estrarre
  bool FromBeginnig;    Flag opzionale; se true parto dall'inizio della lista
                        KeyList altrimenti parto dalla posizione del cursore
                        (default = true)
  int MaxConditions;    Opzionale; numero massimo di condizioni impostabili.
                        Se = -1 non c'è limite (default = MAX_GRAPH_CONDITIONS)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GPH_INFO::AddQueryFromEntityId(long Id)
{
   C_STR_LIST IdList;
   C_STR      *pId;

   if ((pId = new C_STR()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
   (*pId) = Id;
   IdList.add_tail(pId);

   return AddQueryFromEntityIds(IdList);
}
int C_GPH_INFO::AddQueryFromEntityIds(C_STR_LIST &IdList, bool FromBeginnig,
                                      int MaxConditions)
{
   C_STR      *pKey;
	C_STRING   ODTblFldName, JOp, endGroups;
   bool       First = TRUE;
   C_RB_LIST  QryCond;
   int        i = 1;

   if (FromBeginnig)
      pKey = (C_STR *) IdList.get_head();
   else
      pKey = (C_STR *) IdList.get_cursor();

   if (!pKey) return GS_GOOD;
  
   gsc_getODTableName(prj, cls, sub, ODTblFldName);
   ODTblFldName += _T(".ID");

   // Costruisco la condition query da passare a QueryDefine
   QryCond << acutBuildList(RTLB,
                            RTSTR, _T("objdata"),
                            RTSTR, ODTblFldName.get_name(),
                            RTSTR, _T("="), RTSTR, pKey->get_name(),
                            RTLE, 0);
   pKey = (C_STR *) IdList.get_next();
   endGroups = (!pKey) ? _T(")"): GS_EMPTYSTR; // ultima condizione
   // Imposto la prima condizione di estrazione DATA
   if (QueryDefine(_T("and"),                         // joinop
                   _T("("),                           // bgGroups
                   GS_EMPTYSTR,                       // not_op
                   _T("Data"), QryCond.get_head(), 
                   endGroups.get_name()) == GS_BAD)   // endGroups
      return GS_BAD;

   // scorro la lista delle classi e degli oggetti che voglio estrarre 
   while (pKey && i++ < MaxConditions)
   {
      // Costruisco la condition query da passare ad ade_qrydefine()
      QryCond << acutBuildList(RTLB,
                               RTSTR, _T("objdata"),
                               RTSTR, ODTblFldName.get_name(),
                               RTSTR, _T("="), RTSTR, pKey->get_name(),
                               RTLE, 0);
      pKey = (C_STR *) IdList.get_next();

      endGroups = (!pKey || i >= MaxConditions) ? _T(")"): GS_EMPTYSTR; // ultima condizione

      // Imposto la condizione di estrazione DATA
      if (QueryDefine(_T("or"),                          // joinop
                      GS_EMPTYSTR,                       // bgGroups
                      GS_EMPTYSTR,                       // not_op
                      _T("Data"), QryCond.get_head(), 
                      endGroups.get_name()) == GS_BAD)   // endGroups
         return GS_BAD;
   }

	return GS_GOOD;
}


/*********************************************************/
/*.doc C_GPH_INFO::AddQueryOnlyLabels         <internal> */
/*+
  Questa funzione aggiunge in "and" alla query ADE corrente la condizione 
  necessaria per cercare solo le etichette.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GPH_INFO::AddQueryOnlyLabels(void)
{
   C_RB_LIST DollarTCond;

   if ((DollarTCond << acutBuildList(RTLB,
                                        RTSTR, _T("blockname"),
                                        RTSTR, _T("="),
                                        RTSTR, _T("$T"),
                                     RTLE, 0)) == NULL)
      return GS_BAD;

   return QueryDefine(_T("AND"), _T("(") , _T(""), _T("Property"),
                      DollarTCond.get_head(), _T(")"));
}


/*********************************************************/
/*.doc C_GPH_INFO::AddQueryOnlyGraphObjs      <internal> */
/*+
  Questa funzione aggiunge in "and" alla query ADE corrente la condizione 
  necessaria per cercare solo oggetti grafici senza etichette.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/
/*********************************************************/
int C_GPH_INFO::AddQueryOnlyGraphObjs(void)
{
   C_RB_LIST DollarTCond;

   if ((DollarTCond << acutBuildList(RTLB,
                                        RTSTR, _T("blockname"),
                                        RTSTR, _T("="),
                                        RTSTR, _T("$T"),
                                     RTLE, 0)) == NULL)
      return GS_BAD;
   
   return QueryDefine(_T("AND"), _T("(") , _T("NOT"), _T("Property"),
                      DollarTCond.get_head(), _T(")"));
}


/*********************************************************/
/*.doc C_GPH_INFO::QueryDefine                 <internal> */
/*+
  Questa funzione definisce la query ADE corrente da applicare 
  alla fonte dati grafica.
  Parametri:
  TCHAR* joinop;     A joining operator: "and" or "or" or "" (none). 
                     If "" (none) is specified, the default joining operator 
                     is used (see ade_prefgetval). 
  TCHAR* bggroups;   For grouping this condition with others in the query 
                     definition you are building. Use one or more open parentheses
                     as needed, or "" (none). For example, "((". 
  TCHAR* not_op;     The NOT operator, if needed: "not" or "" (none). 
  TCHAR* condtype;   A condition type: "Location", "Property", "Data", or "SQL". 
  presbuf qrycond;   A condition expression. Depends on the condition type.
  TCHAR* endgroups;  For grouping this condition with others in the query 
                     definition you are building. Use one or more close p
                     arentheses as needed, or "" (none). For example, "))". 

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_GPH_INFO::QueryDefine(TCHAR* joinOp, TCHAR* bgGroups, TCHAR* notOp,
                            TCHAR* condType, presbuf qryCond, TCHAR* endGroups)
{
   if (ade_qrydefine(joinOp, bgGroups, notOp, condType, qryCond, endGroups) == ADE_NULLID)
      { GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
   
   return GS_GOOD;
}


long C_GPH_INFO::Query(int WhatToDo,
                       C_SELSET *pSelSet, long BitForFAS, C_FAS *pFAS ,
                       C_STRING *pRptTemplate, C_STRING *pRptFile, const TCHAR *pRptMode,
                       int CounterToVideo)
   { GS_ERR_COD = eGSInvClassType; return -1; }
int C_GPH_INFO::ApplyQuery(C_GPH_OBJ_ARRAY &Objs, int CounterToVideo)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_GPH_INFO::QueryIn(C_GPH_OBJ_ARRAY &Objs, C_SELSET *pSelSet,
                       long BitForFAS, C_FAS *pFAS, int CounterToVideo)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_GPH_INFO::Report(C_GPH_OBJ_ARRAY &Objs, C_STRING &Template,
                       C_STRING &Path, const TCHAR *Mode)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_GPH_INFO::Preview(C_GPH_OBJ_ARRAY &Objs, long BitForFAS, C_FAS *pFAS)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_GPH_INFO::Save(void)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_GPH_INFO::editnew(ads_name sel_set)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
bool C_GPH_INFO::HasCompatibleGeom(AcDbObject *pObj, bool TryToExplode, AcDbVoidPtrArray *pExplodedSet)
   { GS_ERR_COD = eGSInvClassType; return false; }
bool C_GPH_INFO::HasValidGeom(AcDbEntity *pObj, C_STRING &WhyNotValid)
   { return false; }
bool C_GPH_INFO::HasValidGeom(AcDbEntityPtrArray &EntArray, C_STRING &WhyNotValid)
   { return false; }


/*********************************************************/
/*.doc bool C_GPH_INFO::HasCompatibleGeom     <external> /*
/*+
  Questa funzione verifica che la geometria di un oggetto grafico
  sia compatibile al tipo di risorsa grafica usata.
  Parametri:
  ads_name ent;            oggetto grafico
  bool TryToExplode;       Opzionale; flag di esplosione. Nel caso l'oggetto
                           grafico non sia compatibile vine esploso per ridurlo
                           ad oggetti semplici (default = false)
  C_SELSET *pExplodedSS;   Opzionale; Puntatore a risultato 
                           dell'esplosione (default = NULL)

  Restituisce true se l'oggetto ha una geometria compatibile altrimenti false.
-*/  
/*********************************************************/
bool C_GPH_INFO::HasCompatibleGeom(ads_name ent, bool TryToExplode, C_SELSET *pExplodedSS)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;
   bool         Result;
   AcDbVoidPtrArray ExplodedSet;
   ads_name         ExplodedEnt;

   if (pExplodedSS) pExplodedSS->clear();

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return false;
   if (acdbOpenObject(pObj, objId, AcDb::kForRead, true) != Acad::eOk) return false;
   Result = HasCompatibleGeom(pObj, TryToExplode, &ExplodedSet);
   pObj->close();

   if (Result && TryToExplode && pExplodedSS && ExplodedSet.length() > 0)
      for (int i = 0; i < ExplodedSet.length(); i++)
      {
         if (acdbGetAdsName(ExplodedEnt, ((AcDbObject *) ExplodedSet[i])->objectId()) == Acad::eOk)
            pExplodedSS->add(ExplodedEnt);
      }

   return Result;
}


/*********************************************************/
/*.doc bool C_GPH_INFO::HasValidGeom          <external> /*
/*+
  Questa funzione verifica che la validità della geometria di un oggetto grafico.
  Parametri:
  ads_name ent;  oggetto grafico

  Restituisce true se l'oggetto ha una geometria troppo complessa altrimenti false.
-*/  
/*********************************************************/
bool C_GPH_INFO::HasValidGeom(ads_name ent, C_STRING &WhyNotValid)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;
   bool         Result;

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return false;
   if (acdbOpenObject(pObj, objId, AcDb::kForRead, true) != Acad::eOk) return false;
   Result = HasValidGeom((AcDbEntity *) pObj, WhyNotValid);
   pObj->close();

   return Result;
}
bool C_GPH_INFO::HasValidGeom(C_SELSET &entSS, C_STRING &WhyNotValid)
{
   C_SELSET           GraphSS;
   AcDbEntityPtrArray EntArray;
   bool               Result;

   entSS.copyIntersectType(GraphSS, GRAPHICAL); // scarto le etichette
   if (GraphSS.get_AcDbEntityPtrArray(EntArray) == GS_BAD) return false;
   Result = HasValidGeom(EntArray, WhyNotValid);
   if (gsc_close_AcDbEntities(EntArray) == GS_BAD) return GS_BAD;

   return Result;
}


int C_GPH_INFO::Check(int WhatOperation)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }
int C_GPH_INFO::Detach(void)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


/****************************************************************************/
/*.doc C_GPH_INFO::reportHTML                                    <external> */
/*+
  Questa funzione stampa su un file html i dati della C_GPH_INFO.
  Parametri:
  FILE *file;        Puntatore a file
  bool SynthMode;    Opzionale. Flag di modalità di report.
                     Se = true attiva il report sintetico (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_GPH_INFO::reportHTML(FILE *file, bool SynthMode) 
{ 
   C_STRING TitleBorderColor("#808080"), TitleBgColor("#c0c0c0");
   C_STRING BorderColor("#00CCCC"), BgColor("#99FFFF");

   if (SynthMode) return GS_GOOD;

   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" bgcolor=\"%s\" width=\"100%%\" border=\"1\">"),
                TitleBorderColor.get_name(), TitleBgColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Caratteristiche Grafiche"
   if (fwprintf(file, _T("\n<tr><td align=\"center\"><b><font size=\"3\">%s</font></b></td></tr></table><br>"),
                gsc_msg(752)) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // intestazione tabella
   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" cellspacing=\"2\" cellpadding=\"2\" border=\"1\">"), 
                BorderColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Sistema coordinate"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(722),
                (coordinate_system.len() == 0) ? _T("&nbsp;") : coordinate_system.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // fine tabella
   if (fwprintf(file, _T("\n</table><br><br>")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


int C_GPH_INFO::Backup(BackUpModeEnum Mode, int MsgToVideo)
   { GS_ERR_COD = eGSInvClassType; return GS_BAD; }


/****************************************************************************/
/*.doc C_GPH_INFO::getLocatorReportTemplate                      <external> */
/*+
  Questa funzione resrituisce il template di default per individuare la
  posizione degli oggetti grafici.
  Parametri:
  C_STRING &Template;  Stringa contenente il template

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_GPH_INFO::getLocatorReportTemplate(C_STRING &Template)
{
   C_CLASS *pCls;
   C_ID    *pId;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;
   pId = pCls->ptr_id();

   switch (pId->category)
   {
      case CAT_SIMPLEX:
         switch (pId->type)
         {
            case TYPE_POLYLINE: 
               Template = _T(".X1,.Y1,.Z1,.X2,.Y2,.Z2");
               break;
            case TYPE_TEXT:
               Template = _T(".LABELPT");
               break;
            case TYPE_NODE:
               Template = _T(".CENTER");
               break;
            case TYPE_SURFACE:
               Template = _T(".CENTROID");
               break;
            default:
               GS_ERR_COD = eGSInvClassType; 
               return GS_BAD;
         }
         break;

      default:
         GS_ERR_COD = eGSInvClassType; 
         return GS_BAD;
   }

   return GS_GOOD;
}

int C_GPH_INFO::get_isCoordToConvert(void)
   { return GS_BAD; }


/*****************************************************************************/
/*.doc C_GPH_INFO::gsc_get_UnitCoordConvertionFactor              <internal> */
/*+
  Questa funzione restituisce il fattore moltiplicativo per convertire le unità delle coordinate
  degli oggetti dal sistema nel DB a quello della sessione di lavoro.

  Ritorna 1 = nessuna correzione, 0 = non inizializzato
-*/  
/*****************************************************************************/
double C_GPH_INFO::get_UnitCoordConvertionFactorFromClsToWrkSession(void)
{
   if (UnitCoordConvertionFactorFromClsToWrkSession != 0) // se il valore è già stato inizializzato
      return UnitCoordConvertionFactorFromClsToWrkSession;

   if (!GS_CURRENT_WRK_SESSION)
      UnitCoordConvertionFactorFromClsToWrkSession = 0;
   else
      UnitCoordConvertionFactorFromClsToWrkSession = gsc_get_UnitCoordConvertionFactor(get_ClsSRID_unit(), 
                                                                                       GS_CURRENT_WRK_SESSION->get_SRID_unit());

   return UnitCoordConvertionFactorFromClsToWrkSession;
}


/*****************************************************************************/
/*.doc C_GPH_INFO::get_ClsSRID_converted_to_AutocadSRID           <internal> */
/*+
  Questa funzione carica lo SRID in formato Autocad e le unità del 
  sistema di coordinate della classe. 
  Restituisce il puntatore allo SRID della classe convertito (se possibile) 
  nello SRID gestito da Autocad.
-*/  
/*****************************************************************************/
C_STRING *C_GPH_INFO::get_ClsSRID_converted_to_AutocadSRID(void)
   { GS_ERR_COD = eGSInvClassType; return NULL; }


/*****************************************************************************/
/*.doc C_GPH_INFO::get_ClsSRID_unit                               <internal> */
/*+
  Questa funzione restituisce l'unita del sistema di coordinate dello SRID della classe.
-*/  
/*****************************************************************************/
GSSRIDUnitEnum C_GPH_INFO::get_ClsSRID_unit(void)
{
   get_ClsSRID_converted_to_AutocadSRID();   
   return ClsSRID_unit;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_GPH_INFO
// INIZIO FUNZIONI C_BID_HANDLE
///////////////////////////////////////////////////////////////////////////


// costruttore
C_BID_HANDLE::C_BID_HANDLE() : C_BREAL() { pHandle = NULL; }

C_BID_HANDLE::C_BID_HANDLE(double in1) // costruttore che riceve solo la chiave
   { key = in1; pHandle = NULL; }

// distruttore
C_BID_HANDLE::~C_BID_HANDLE() {}

C_BSTR_REAL *C_BID_HANDLE::get_pHandle() 
   { return pHandle; }

int C_BID_HANDLE::set_pHandle(C_BSTR_REAL *in) 
   { pHandle = in; return GS_GOOD; }


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_BID_HANDLE
// INIZIO FUNZIONI C_BID_HANDLE_BTREE
///////////////////////////////////////////////////////////////////////////


C_BID_HANDLE_BTREE::C_BID_HANDLE_BTREE():C_LONG_BTREE() {}

C_BID_HANDLE_BTREE::~C_BID_HANDLE_BTREE() {}

C_BNODE* C_BID_HANDLE_BTREE::alloc_item(const void *in) // <in> deve essere un puntatore a double
{   
   const double *dummy = (double *) in;
   C_BID_HANDLE *p = new C_BID_HANDLE(*dummy);

   if (!p) GS_ERR_COD = eGSOutOfMem;
   return (C_BNODE *) p;
}


///////////////////////////////////////////////////////////////////////////
// FINE FUNZIONI C_BID_HANDLE_BTREE
// INIZIO FUNZIONI C_DBGPH_INFO
///////////////////////////////////////////////////////////////////////////


// costruttore
C_DBGPH_INFO::C_DBGPH_INFO() : C_GPH_INFO()
{
   LinkedTable           = false;
   LinkedLblGrpTable     = false;
   LinkedLblTable        = false;
   coordinate_system     = DEFAULT_DBGRAPHCOORD;
   rotation_unit         = GSCounterClockwiseDegreeUnit;
   geom_dim              = GS_2D; // standard OpenGIS
   key_attrib            = DEFAULT_KEY_ATTR;
   ent_key_attrib        = _T("GS_ID_PARENT");
   attrib_name_attrib    = _T("LBL_ATTR_NAME");
   attrib_invis_attrib   = _T("HIDDEN");
   geom_attrib           = _T("GEOM");
   x_attrib              = _T("X");
   y_attrib              = _T("Y");
   z_attrib              = _T("Z");
   rotation_attrib       = _T("ROT");
   vertex_parent_attrib  = _T("GS_ID_GEOM_PARENT");
   bulge_attrib          = _T("BULGE");
   aggr_factor_attrib    = _T("AGGR_FACTOR");
   layer_attrib          = _T("LAYER");
   color_attrib          = _T("COLOR");
   color_format          = GSAutoCADColorIndexFormatColor;
   
   line_type_attrib      = _T("LINE_TYPE");
   line_type_scale_attrib = _T("LINE_TYPE_SCALE");
   width_attrib          = _T("WIDTH");

   block_attrib          = _T("BLOCK");
   block_scale_attrib    = _T("BLOCK_SCALE");

   text_attrib           = _T("TEXT");
   text_style_attrib     = _T("TEXT_STYLE");
   h_text_attrib         = _T("H_TEXT");
   horiz_align_attrib    = _T("HORIZ_ALIGN");
   vert_align_attrib     = _T("VERT_ALIGN");
   oblique_angle_attrib  = _T("OBLIQUE_ANGLE");

   thickness_attrib      = _T("THICKNESS");

   hatch_attrib          = _T("HATCH");
   hatch_layer_attrib    = _T("HATCH_LAYER");
   hatch_color_attrib    = _T("HATCH_COLOR");
   hatch_scale_attrib    = _T("HATCH_SCALE");
   hatch_rotation_attrib = _T("HATCH_ROT");

   pGeomConn = NULL;
   isCoordToConvert = GS_CAN;

   mPlinegen = false;

   ClsSRID_unit = GSSRIDUnit_None;
   UnitCoordConvertionFactorFromClsToWrkSession = 0;
}

C_DBGPH_INFO::~C_DBGPH_INFO() {}

//-----------------------------------------------------------------------//

C_DBCONNECTION* C_DBGPH_INFO::getDBConnection(void)
{
   if (pGeomConn) return pGeomConn;
   pGeomConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name(), &UDLProperties);
   
   return pGeomConn;
}


//-----------------------------------------------------------------------//


GraphDataSourceEnum C_DBGPH_INFO::getDataSourceType(void) { return GSDBGphDataSource; }


/*********************************************************/
/*.doc C_DBGPH_INFO::get_SqlCondOnTable set_SqlCondOnTable <internal> */
/*+
  Questa funzione legge e setta la condizione SQL da applicare 
  per cercare i dati nella tabella TableRef nel caso questa
  non contenga dati esclusivamente della classe.
  N.B. La condizione deve essere del tipo "<campo1>=val1 AND <campo2>=val2"

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
TCHAR *C_DBGPH_INFO::get_SqlCondOnTable(void)
{
   return SqlCondOnTable.get_name();
}
int C_DBGPH_INFO::set_SqlCondOnTable(C_STRING &in)
{
   if (SqlCondToFieldValueList(in, FieldValueListOnTable) == GS_BAD) return GS_BAD;
   SqlCondOnTable = in;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::get_SqlCondOnLblGrpTable set_SqlCondOnLblGrpTable <internal> */
/*+
  Questa funzione legge e setta la condizione SQL da applicare 
  per cercare i dati nella tabella LblGroupingTableRef nel caso questa
  non contenga dati esclusivamente della classe.
  N.B. La condizione deve essere del tipo "<campo1>=val1 AND <campo2>=val2"

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
TCHAR *C_DBGPH_INFO::get_SqlCondOnLblGrpTable(void)
{
   return SqlCondOnLblGrpTable.get_name();
}
int C_DBGPH_INFO::set_SqlCondOnLblGrpTable(C_STRING &in)
{
   if (SqlCondToFieldValueList(in, FieldValueListOnLblGrpTable) == GS_BAD) return GS_BAD;
   SqlCondOnLblGrpTable = in;

   return GS_GOOD;
}

/*********************************************************/
/*.doc C_DBGPH_INFO::get_SqlCondOnLblTable set_SqlCondOnLblTable <internal> */
/*+
  Questa funzione legge e setta la condizione SQL da applicare 
  per cercare i dati nella tabella LblTableRef nel caso questa
  non contenga dati esclusivamente della classe.
  N.B. La condizione deve essere del tipo "<campo1>=val1 AND <campo2>=val2"

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
TCHAR *C_DBGPH_INFO::get_SqlCondOnLblTable(void)
{
   return SqlCondOnLblTable.get_name();
}
int C_DBGPH_INFO::set_SqlCondOnLblTable(C_STRING &in)
{
   if (SqlCondToFieldValueList(in, FieldValueListOnLblTable) == GS_BAD) return GS_BAD;
   SqlCondOnLblTable = in;

   return GS_GOOD;
}


//-----------------------------------------------------------------------//


bool C_DBGPH_INFO::LabelWith(void)
{
   C_CLASS *pCls = gsc_find_class(prj, cls, sub);

   if (pCls)
      if (pCls->get_type() == TYPE_TEXT ||
          pCls->get_type() == CAT_SPAGHETTI) return false;

   if (LblTableRef.len() == 0) return false;

   return true;
}

//-----------------------------------------------------------------------//

int C_DBGPH_INFO::copy(C_GPH_INFO *out)
{
   if (!out) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   // Copio i valori della classe base
   C_GPH_INFO::copy(out);

   // prima setto queste 2 informazioni che mi consentono di aprire la connessione
   // al database in modo corretto in set_SqlCondOnTable
   ((C_DBGPH_INFO *) out)->ConnStrUDLFile = ConnStrUDLFile;
   UDLProperties.copy(((C_DBGPH_INFO *) out)->UDLProperties);

   ((C_DBGPH_INFO *) out)->TableRef       = TableRef;
   ((C_DBGPH_INFO *) out)->LinkedTable    = LinkedTable;
   ((C_DBGPH_INFO *) out)->set_SqlCondOnTable(SqlCondOnTable);

   ((C_DBGPH_INFO *) out)->LblGroupingTableRef  = LblGroupingTableRef;
   ((C_DBGPH_INFO *) out)->LinkedLblGrpTable    = LinkedLblGrpTable;
   ((C_DBGPH_INFO *) out)->set_SqlCondOnLblGrpTable(SqlCondOnLblGrpTable);

   ((C_DBGPH_INFO *) out)->LblTableRef       = LblTableRef;
   ((C_DBGPH_INFO *) out)->LinkedLblTable    = LinkedLblTable;
   ((C_DBGPH_INFO *) out)->set_SqlCondOnLblTable(SqlCondOnLblTable);

   ((C_DBGPH_INFO *) out)->key_attrib = key_attrib;
   ((C_DBGPH_INFO *) out)->ent_key_attrib = ent_key_attrib;

   ((C_DBGPH_INFO *) out)->attrib_name_attrib = attrib_name_attrib;
   ((C_DBGPH_INFO *) out)->attrib_invis_attrib = attrib_invis_attrib;
   
   ((C_DBGPH_INFO *) out)->geom_dim = geom_dim;
   ((C_DBGPH_INFO *) out)->geom_attrib = geom_attrib;
   ((C_DBGPH_INFO *) out)->x_attrib = x_attrib;
   ((C_DBGPH_INFO *) out)->y_attrib = y_attrib;
   ((C_DBGPH_INFO *) out)->z_attrib = z_attrib;
   ((C_DBGPH_INFO *) out)->rotation_attrib = rotation_attrib;
   ((C_DBGPH_INFO *) out)->rotation_unit = rotation_unit;
   ((C_DBGPH_INFO *) out)->vertex_parent_attrib = vertex_parent_attrib;
   ((C_DBGPH_INFO *) out)->bulge_attrib = bulge_attrib;
   ((C_DBGPH_INFO *) out)->aggr_factor_attrib = aggr_factor_attrib;
   ((C_DBGPH_INFO *) out)->layer_attrib = layer_attrib;
   ((C_DBGPH_INFO *) out)->color_attrib = color_attrib;
   ((C_DBGPH_INFO *) out)->color_format = color_format;
   
   ((C_DBGPH_INFO *) out)->line_type_attrib = line_type_attrib;
   ((C_DBGPH_INFO *) out)->line_type_scale_attrib = line_type_scale_attrib;
   ((C_DBGPH_INFO *) out)->width_attrib = width_attrib;

   ((C_DBGPH_INFO *) out)->text_attrib = text_attrib;
   ((C_DBGPH_INFO *) out)->text_style_attrib = text_style_attrib;
   ((C_DBGPH_INFO *) out)->h_text_attrib = h_text_attrib;
   ((C_DBGPH_INFO *) out)->horiz_align_attrib = horiz_align_attrib;
   ((C_DBGPH_INFO *) out)->vert_align_attrib = vert_align_attrib;
   ((C_DBGPH_INFO *) out)->oblique_angle_attrib = oblique_angle_attrib;

   ((C_DBGPH_INFO *) out)->block_attrib = block_attrib;
   ((C_DBGPH_INFO *) out)->block_scale_attrib = block_scale_attrib;

   ((C_DBGPH_INFO *) out)->hatch_attrib = hatch_attrib;
   ((C_DBGPH_INFO *) out)->hatch_layer_attrib = hatch_layer_attrib;
   ((C_DBGPH_INFO *) out)->hatch_color_attrib = hatch_color_attrib;
   ((C_DBGPH_INFO *) out)->hatch_scale_attrib = hatch_scale_attrib;
   ((C_DBGPH_INFO *) out)->hatch_rotation_attrib = hatch_rotation_attrib;
   ((C_DBGPH_INFO *) out)->thickness_attrib = thickness_attrib;

   ((C_DBGPH_INFO *) out)->mPlinegen = mPlinegen;
   

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_DBGPH_INFO::rb2Color Color2rb Color2SQLValue <internal>              */
/*+
  Questa funzione converte colore e resbuf in base alle impostazioni 
  del formato del colore.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DBGPH_INFO::rb2Color(presbuf pRb, C_COLOR &color)
{
   switch (color_format)
   {
      case GSAutoCADColorIndexFormatColor:
      {
         int AutoCADColorIndex;

         if (gsc_rb2Int(pRb, &AutoCADColorIndex) == GS_BAD) break;
         return color.setAutoCADColorIndex(AutoCADColorIndex);
      }
      case GSHTMLFormatColor:
      {
         C_STRING HTMLColor;

         HTMLColor = pRb;
         if (color.setHTMLColor(HTMLColor) == GS_BAD) break;
         return GS_GOOD;
      }
      case GSHexFormatColor:
      {
         C_STRING HexColor;

         HexColor = pRb;
         if (color.setHexColor(HexColor) == GS_BAD) break;
         return GS_GOOD;
      }
      case GSRGBDecColonFormatColor:
      {
         C_STRING RGBDecColor;

         RGBDecColor = pRb;
         if (color.setRGBDecColor(RGBDecColor, _T(',')) == GS_BAD) break;
         return GS_GOOD;
      }
      case GSRGBDecBlankFormatColor:
      {
         C_STRING RGBDecColor;

         RGBDecColor = pRb;
         if (color.setRGBDecColor(RGBDecColor, _T(' ')) == GS_BAD) break;
         return GS_GOOD;
      }
      case GSRGBDXFFormatColor:
      {
         COLORREF RGBDXFColor;

         if (gsc_rb2Lng(pRb, (long *) &RGBDXFColor) == GS_BAD) return GS_BAD;
         return color.setRGBDXFColor(RGBDXFColor);
      }
      case GSRGBCOLORREFFormatColor:
      {
         COLORREF RGB;

         if (gsc_rb2Lng(pRb, (long *) &RGB) == GS_BAD) return GS_BAD;
         return color.setRGB(RGB);         
      }
      default:
         return GS_BAD;
   }

   if (pRb && pRb->restype == RTSTR)
      if (gsc_strcmp(pRb->resval.rstring, _T("BYLAYER"), FALSE) == 0)
         { color.setByLayer(); return GS_GOOD; }
      else if (gsc_strcmp(pRb->resval.rstring, _T("BYBLOCK"), FALSE) == 0)
         { color.setByBlock(); return GS_GOOD; }
      else if (gsc_strcmp(pRb->resval.rstring, _T("FOREGROUND"), FALSE) == 0)
         { color.setForeground(); return GS_GOOD; }

   return GS_BAD;
}
int C_DBGPH_INFO::Color2rb(C_COLOR &color, presbuf pRb)
{
   switch (color_format)
   {
      case GSAutoCADColorIndexFormatColor:
      {
         int AutoCADColorIndex;

         if (color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD) return GS_BAD;
         return gsc_RbSubst(pRb, AutoCADColorIndex);
      }
      case GSHTMLFormatColor:
      {
         C_STRING HTMLColor;

         if (color.getHTMLColor(HTMLColor) == GS_BAD) break;
         return gsc_RbSubst(pRb, HTMLColor.get_name());
      }
      case GSHexFormatColor:
      {
         C_STRING HexColor;

         if (color.getHexColor(HexColor) == GS_BAD) break;
         return gsc_RbSubst(pRb, HexColor.get_name());
      }
      case GSRGBDecColonFormatColor:
      {
         C_STRING RGBDecColor;

         if (color.getRGBDecColor(RGBDecColor, _T(',')) == GS_BAD) break;
         return gsc_RbSubst(pRb, RGBDecColor.get_name());
      }
      case GSRGBDecBlankFormatColor:
      {
         C_STRING RGBDecColor;

         if (color.getRGBDecColor(RGBDecColor, _T(' ')) == GS_BAD) break;
         return gsc_RbSubst(pRb, RGBDecColor.get_name());
      }
      case GSRGBDXFFormatColor:
      {
         COLORREF RGBDXFColor;

         if (color.getRGBDXFColor(&RGBDXFColor) == GS_BAD) break;
         return gsc_RbSubst(pRb, (long) RGBDXFColor);
      }
      case GSRGBCOLORREFFormatColor:
      {
         COLORREF RGB;

         if (color.getRGB(&RGB) == GS_BAD) break;
         return gsc_RbSubst(pRb, (long) RGB);
      }
      default:
         return GS_BAD;
   }

   switch (color.getColorMethod())
   {
      case C_COLOR::ByLayer:
         return gsc_RbSubst(pRb, _T("BYLAYER"));
      case C_COLOR::ByBlock:
         return gsc_RbSubst(pRb, _T("BYBLOCK"));
      case C_COLOR::Foreground:
         return gsc_RbSubst(pRb, _T("FOREGROUND"));
      default:
         return GS_BAD;
   }
}
int C_DBGPH_INFO::Color2SQLValue(C_COLOR &color, C_STRING &SQLValue)
{
   switch (color_format)
   {
      case GSAutoCADColorIndexFormatColor:
      {
         int AutoCADColorIndex;

         if (color.getAutoCADColorIndex(&AutoCADColorIndex) == GS_BAD)
            { SQLValue = _T("NULL"); return GS_GOOD; }
         SQLValue = AutoCADColorIndex;
         break;
      }
      case GSHTMLFormatColor:
      {
         if (color.getHTMLColor(SQLValue) == GS_BAD) SQLValue.clear();
         break;
      }
      case GSHexFormatColor:
      {
         if (color.getHexColor(SQLValue) == GS_BAD) SQLValue.clear();
         break;
      }
      case GSRGBDecColonFormatColor:
      {
         if (color.getRGBDecColor(SQLValue, _T(',')) == GS_BAD) SQLValue.clear();
         break;
      }
      case GSRGBDecBlankFormatColor:
      {
         if (color.getRGBDecColor(SQLValue, _T(' ')) == GS_BAD) SQLValue.clear();
         break;
      }
      case GSRGBDXFFormatColor:
      {
         COLORREF RGBDXFColor;

         if (color.getRGBDXFColor(&RGBDXFColor) == GS_BAD) SQLValue.clear();
         else SQLValue = (long) RGBDXFColor;
         break;
      }
      case GSRGBCOLORREFFormatColor:
      {
         COLORREF RGB;

         if (color.getRGB(&RGB) == GS_BAD) SQLValue.clear();
         else SQLValue = (long) RGB;
         break;
      }
      default:
         SQLValue = _T("NULL");
         return GS_GOOD;
   }

   if (SQLValue.len() == 0)
      switch (color.getColorMethod())
      {
         case C_COLOR::ByLayer:
            SQLValue = _T("BYLAYER");
            break;
         case C_COLOR::ByBlock:
            SQLValue = _T("BYBLOCK");
            break;
         case C_COLOR::Foreground:
            SQLValue = _T("FOREGROUND");
            break;
         default:
            SQLValue = _T("NULL");
            return GS_GOOD;
      }

   // Correggo la stringa secondo la sintassi SQL 
   return getDBConnection()->Str2SqlSyntax(SQLValue);
}


/*********************************************************/
/*.doc C_DBGPH_INFO::ToFile                   <internal> */
/*+
  Questa funzione salva su file i dati della C_DBGPH_INFO.
  Parametri:
  C_STRING &filename;   file in cui scrivere le informazioni
  const TCHAR *sez;     nome della sezione in cui scrivere

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::ToFile(C_STRING &filename, const TCHAR *sez)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   bool                    Unicode = false;

   if (gsc_path_exist(filename) == GS_GOOD)
      if (gsc_read_profile(filename, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   if (ToFile(ProfileSections, sez) == GS_BAD) return GS_BAD;

   return gsc_write_profile(filename, ProfileSections, Unicode);
}
int C_DBGPH_INFO::ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_STRING Buffer;

   if (getDBConnection() == NULL) return GS_BAD;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez)))
   {
      if (ProfileSections.add(sez) == GS_BAD) return GS_BAD;
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();
   }

   Buffer = GSDBGphDataSource;
   ProfileSection->set_entry(_T("GPH_INFO.SOURCE_TYPE"), Buffer.get_name());

   // Cancello l'eventuale informazione della connessione grafica a DWG
   // perchè questa sezione contiene solo un tipo di connessione a fonti grafiche
   ProfileSection->set_entry(_T("GPH_INFO.DIR_DWG"), NULL);

   // Scrivo i valori della classe base
   if (C_GPH_INFO::ToFile(ProfileSections, sez) == GS_BAD) return GS_BAD;

   // Conversione riferimento tabella da dir relativo in assoluto
   if (Buffer.paste(getDBConnection()->FullRefTable_drive2nethost(TableRef.get_name())) == NULL)
      return GS_BAD;
   ProfileSection->set_entry(_T("GPH_INFO.TABLE_REF"), Buffer.get_name());
   ProfileSection->set_entry(_T("GPH_INFO.LINKED_TABLE"), LinkedTable);
   ProfileSection->set_entry(_T("GPH_INFO.SQL_COND_ON_TABLE"), SqlCondOnTable.get_name());

   // Conversione riferimento tabella da dir relativo in assoluto (opzionale)
   if (LblGroupingTableRef.get_name())
   {
      if (Buffer.paste(getDBConnection()->FullRefTable_drive2nethost(LblGroupingTableRef.get_name())) == NULL)
         return GS_BAD;
   }
   else Buffer = GS_EMPTYSTR;

   ProfileSection->set_entry(_T("GPH_INFO.LBL_GRP_TABLE_REF"), Buffer.get_name());
   ProfileSection->set_entry(_T("GPH_INFO.LINKED_LBL_GRP_TABLE"), LinkedLblGrpTable);
   ProfileSection->set_entry(_T("GPH_INFO.SQL_COND_ON_LBL_GRP_TABLE"), SqlCondOnLblGrpTable.get_name());

   // Conversione riferimento tabella da dir relativo in assoluto (opzionale)
   if (LblTableRef.get_name())
   {
      if (Buffer.paste(getDBConnection()->FullRefTable_drive2nethost(LblTableRef.get_name())) == NULL)
      return GS_BAD;
   }
   else Buffer = GS_EMPTYSTR;

   ProfileSection->set_entry(_T("GPH_INFO.LBL_TABLE_REF"), Buffer.get_name());
   ProfileSection->set_entry(_T("GPH_INFO.LINKED_LBL_TABLE"), LinkedLblTable);
   ProfileSection->set_entry(_T("GPH_INFO.SQL_COND_ON_LBL_TABLE"), SqlCondOnLblTable.get_name());

   if (ConnStrUDLFile.get_name())
   {
      Buffer = ConnStrUDLFile;
      if (gsc_path_exist(Buffer) == GS_GOOD)
         if (gsc_drive2nethost(Buffer) == GS_BAD) return GS_BAD;
   }
   else Buffer = GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.UDL_FILE"), Buffer.get_name());

   Buffer.paste(gsc_PropListToConnStr(UDLProperties));
   if (Buffer.get_name())
   {
      // Conversione path UDLProperties da dir relativo in assoluto
      if (getDBConnection() == NULL) return GS_BAD;
      if (getDBConnection()->UDLProperties_drive2nethost(Buffer) == GS_BAD) return GS_BAD;
   }
   else Buffer = GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.UDLPROPERTIES"), Buffer.get_name());

   Buffer = (key_attrib.get_name()) ? key_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.KEY_ATTRIB"), Buffer.get_name());

   Buffer = (ent_key_attrib.get_name()) ? ent_key_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.ENT_KEY_ATTRIB"), Buffer.get_name());

   Buffer = (attrib_name_attrib.get_name()) ? attrib_name_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.ATTRIB_NAME_ATTRIB"), Buffer.get_name());

   Buffer = (attrib_invis_attrib.get_name()) ? attrib_invis_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.ATTRIB_INVIS_ATTRIB"), Buffer.get_name());

   ProfileSection->set_entry(_T("GPH_INFO.GEOM_DIM"), (int) geom_dim);

   Buffer = (geom_attrib.get_name()) ? geom_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.GEOM_ATTRIB"), Buffer.get_name());

   Buffer = (x_attrib.get_name()) ? x_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.X_ATTRIB"), Buffer.get_name());

   Buffer = (y_attrib.get_name()) ? y_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.Y_ATTRIB"), Buffer.get_name());

   Buffer = (z_attrib.get_name()) ? z_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.Z_ATTRIB"), Buffer.get_name());

   Buffer = (rotation_attrib.get_name()) ? rotation_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.ROTATION_ATTRIB"), Buffer.get_name());

   ProfileSection->set_entry(_T("GPH_INFO.ROTATION_UNIT"), (int) rotation_unit);

   Buffer = (vertex_parent_attrib.get_name()) ? vertex_parent_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.VERTEX_PARENT_ATTRIB"), Buffer.get_name());

   Buffer = (bulge_attrib.get_name()) ? bulge_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.BULGE_ATTRIB"), Buffer.get_name());

   Buffer = (aggr_factor_attrib.get_name()) ? aggr_factor_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.AGGR_FACTOR_ATTRIB"), Buffer.get_name());

   Buffer = (layer_attrib.get_name()) ? layer_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.LAYER_ATTRIB"), Buffer.get_name());

   Buffer = (color_attrib.get_name()) ? color_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.COLOR_ATTRIB"), Buffer.get_name());

   ProfileSection->set_entry(_T("GPH_INFO.COLOR_FORMAT"), (int) color_format);

   Buffer = (line_type_attrib.get_name()) ? line_type_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.LINE_TYPE_ATTRIB"), Buffer.get_name());

   Buffer = (line_type_scale_attrib.get_name()) ? line_type_scale_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.LINE_TYPE_SCALE_ATTRIB"), Buffer.get_name());

   Buffer = (width_attrib.get_name()) ? width_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.WIDTH_ATTRIB"), Buffer.get_name());

   Buffer = (text_attrib.get_name()) ? text_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.TEXT_ATTRIB"), Buffer.get_name());

   Buffer = (text_style_attrib.get_name()) ? text_style_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.TEXT_STYLE_ATTRIB"), Buffer.get_name());

   Buffer = (h_text_attrib.get_name()) ? h_text_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.H_TEXT_ATTRIB"), Buffer.get_name());

   Buffer = (horiz_align_attrib.get_name()) ? horiz_align_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.HORIZ_ALIGN_ATTRIB"), Buffer.get_name());

   Buffer = (vert_align_attrib.get_name()) ? vert_align_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.VERT_ALIGN_ATTRIB"), Buffer.get_name());

   Buffer = (oblique_angle_attrib.get_name()) ? oblique_angle_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.OBLIQUE_ANGLE_ATTRIB"), Buffer.get_name());

   Buffer = (block_attrib.get_name()) ? block_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.BLOCK_ATTRIB"), Buffer.get_name());

   Buffer = (block_scale_attrib.get_name()) ? block_scale_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.BLOCK_SCALE_ATTRIB"), Buffer.get_name());

   Buffer = (hatch_attrib.get_name()) ? hatch_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.HATCH_ATTRIB"), Buffer.get_name());

   Buffer = (hatch_layer_attrib.get_name()) ? hatch_layer_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.HATCH_LAYER_ATTRIB"), Buffer.get_name());

   Buffer = (hatch_color_attrib.get_name()) ? hatch_color_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.HATCH_COLOR_ATTRIB"), Buffer.get_name());

   Buffer = (hatch_scale_attrib.get_name()) ? hatch_scale_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.HATCH_SCALE_ATTRIB"), Buffer.get_name());

   Buffer = (hatch_rotation_attrib.get_name()) ? hatch_rotation_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.HATCH_ROTATION_ATTRIB"), Buffer.get_name());

   Buffer = (thickness_attrib.get_name()) ? thickness_attrib.get_name() : GS_EMPTYSTR;
   ProfileSection->set_entry(_T("GPH_INFO.THICKNESS_ATTRIB"), Buffer.get_name());

   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int C_DBGPH_INFO::load(C_STRING &filename, const TCHAR *sez)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   
   if (gsc_read_profile(filename, ProfileSections) == GS_BAD) return GS_BAD;
   return load(ProfileSections, sez);
}
int C_DBGPH_INFO::load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_2STR_BTREE       *pProfileEntries;
   C_B2STR            *pProfileEntry;
   C_STRING           Buffer;

   // Leggo i valori della classe base
   if (C_GPH_INFO::load(ProfileSections, sez) == GS_BAD) return GS_BAD;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez))) return GS_CAN;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   // UDL_FILE
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.UDL_FILE"))))
   {
      ConnStrUDLFile = pProfileEntry->get_name2();
      // converto path con uno senza alias host
      if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD) return GS_BAD;
   }

   // UDL_PROP
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.UDLPROPERTIES"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      if (Buffer.len() > 0)
      {
         // Conversione path UDLProperties da assoluto in dir relativo
         if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(), Buffer) == GS_BAD)
            return GS_BAD;
         if (gsc_PropListFromConnStr(Buffer.get_name(), UDLProperties) == GS_BAD)
            return GS_BAD;
      }
   }

   if (getDBConnection() == NULL) return GS_BAD;

   // Se non esiste questa informazione non si tratta di connessione grafica a DB
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.TABLE_REF")))) return GS_CAN;
   TableRef = pProfileEntry->get_name2();
   // Conversione riferimento tabella da dir relativo in assoluto
   if (TableRef.paste(getDBConnection()->FullRefTable_nethost2drive(TableRef.get_name())) == NULL)
      return NULL;

   // tabella collegata (obbligatorio)
   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.LINKED_TABLE")))) return GS_CAN;
   LinkedTable = (_wtoi(pProfileEntry->get_name2()) == 0) ? false : true;

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.SQL_COND_ON_TABLE"))))
      Buffer = pProfileEntry->get_name2();
   else
      Buffer.clear();
   if (set_SqlCondOnTable(Buffer) == GS_BAD) return GS_BAD;

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.LBL_GRP_TABLE_REF"))))
      LblGroupingTableRef = pProfileEntry->get_name2();
   // Conversione riferimento tabella da dir relativo in assoluto
   if (LblTableRef.paste(getDBConnection()->FullRefTable_nethost2drive(LblGroupingTableRef.get_name())) == NULL)
      return NULL;

   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.LINKED_LBL_GRP_TABLE")))) return GS_CAN;
   LinkedLblGrpTable = (_wtoi(pProfileEntry->get_name2()) == 0) ? false : true;

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.SQL_COND_ON_LBL_GRP_TABLE"))))
      Buffer = pProfileEntry->get_name2();
   else
      Buffer.clear();
   if (set_SqlCondOnLblGrpTable(Buffer) == GS_BAD) return GS_BAD;

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.LBL_TABLE_REF"))))
      LblTableRef = pProfileEntry->get_name2();
   // Conversione riferimento tabella da dir relativo in assoluto
   if (LblTableRef.paste(getDBConnection()->FullRefTable_nethost2drive(LblTableRef.get_name())) == NULL)
      return NULL;

   if (!(pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.LINKED_LBL_TABLE")))) return GS_CAN;
   LinkedLblTable = (_wtoi(pProfileEntry->get_name2()) == 0) ? false : true;

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.SQL_COND_ON_LBL_TABLE"))))
      Buffer = pProfileEntry->get_name2();
   else
      Buffer.clear();
   if (set_SqlCondOnLblTable(Buffer) == GS_BAD) return GS_BAD;
 
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.KEY_ATTRIB"))))
      key_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.ENT_KEY_ATTRIB"))))
      ent_key_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.ATTRIB_NAME_ATTRIB"))))
      attrib_name_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.ATTRIB_INVIS_ATTRIB"))))
      attrib_invis_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.GEOM_DIM"))))
      geom_dim = (GeomDimensionEnum) _wtoi(pProfileEntry->get_name2());
   else
      geom_dim = GS_2D;

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.GEOM_ATTRIB"))))
      geom_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.X_ATTRIB"))))
      x_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.Y_ATTRIB"))))
      y_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.Z_ATTRIB"))))
      z_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.ROTATION_ATTRIB"))))
      rotation_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.ROTATION_UNIT"))))
      rotation_unit = (RotationMeasureUnitsEnum) _wtoi(pProfileEntry->get_name2());
   else
      rotation_unit = GSCounterClockwiseDegreeUnit;

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.VERTEX_PARENT_ATTRIB"))))
      vertex_parent_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.BULGE_ATTRIB"))))
      bulge_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.AGGR_FACTOR_ATTRIB"))))
      aggr_factor_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.LAYER_ATTRIB"))))
      layer_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.COLOR_ATTRIB"))))
      color_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.COLOR_FORMAT"))))
      color_format = (GSFormatColorEnum) _wtoi(pProfileEntry->get_name2());
   else
      color_format = GSAutoCADColorIndexFormatColor;

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.LINE_TYPE_ATTRIB"))))
      line_type_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.LINE_TYPE_SCALE_ATTRIB"))))
      line_type_scale_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.WIDTH_ATTRIB"))))
      width_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.TEXT_ATTRIB"))))
      text_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.TEXT_STYLE_ATTRIB"))))
      text_style_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.H_TEXT_ATTRIB"))))
      h_text_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.HORIZ_ALIGN_ATTRIB"))))
      horiz_align_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.VERT_ALIGN_ATTRIB"))))
      vert_align_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.OBLIQUE_ANGLE_ATTRIB"))))
      oblique_angle_attrib = pProfileEntry->get_name2();
   
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.BLOCK_ATTRIB"))))
      block_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.BLOCK_SCALE_ATTRIB"))))
      block_scale_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.HATCH_ATTRIB"))))
      hatch_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.HATCH_LAYER_ATTRIB"))))
      hatch_layer_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.HATCH_COLOR_ATTRIB"))))
      hatch_color_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.HATCH_SCALE_ATTRIB"))))
      hatch_scale_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.HATCH_ROTATION_ATTRIB"))))
      hatch_rotation_attrib = pProfileEntry->get_name2();

   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("GPH_INFO.THICKNESS_ATTRIB"))))
      thickness_attrib = pProfileEntry->get_name2();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::to_rb                    <internal> */
/*+
  Questa funzione restituisce la C_DBGPH_INFO sottoforma di lista
  di coppie di resbuf ((<proprietà> <valore>) ...).
  Parametri:
  bool ConvertDrive2nethost; Se = TRUE le path vengono convertite
                             con alias di rete (default = false)
  bool ToDB;                 Se = true vengono considerate anche le
                             informazioni di identificazione della
                             classe per la scrittura in DB (default = false)

  Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
presbuf C_DBGPH_INFO::to_rb(bool ConvertDrive2nethost, bool ToDB)
{
   C_RB_LIST List;
   C_STRING  Buffer;
   short     restype;

   // Scrivo i valori della classe base
   if ((List << C_GPH_INFO::to_rb(ConvertDrive2nethost, ToDB)) == GS_BAD) return NULL;

   if (!ToDB)
      if ((List += acutBuildList(RTLB,
                                    RTSTR, _T("SOURCE_TYPE"),
                                    RTSHORT, (int) GSDBGphDataSource,
                                 RTLE,
                                 0)) == NULL) return NULL;

   if ((List += acutBuildList(RTLB,
                                 RTSTR, _T("TABLE_REF"),
                              0)) == NULL) return NULL;
   Buffer = TableRef;
   if (Buffer.len() > 0 && ConvertDrive2nethost)
   {
      // Conversione riferimento tabella da dir relativo in assoluto
      if (getDBConnection() == NULL) return NULL;
      if (Buffer.paste(getDBConnection()->FullRefTable_drive2nethost(Buffer.get_name())) == NULL)
         return NULL;
   }
   if ((List += gsc_str2rb(Buffer)) == NULL) return NULL;

   Buffer  = (SqlCondOnTable.len() > 0) ? SqlCondOnTable.get_name() : GS_EMPTYSTR;
   restype = (LinkedTable) ? RTT : RTNIL;
   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("LINKED_TABLE"),
                                 restype,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("SQL_COND_ON_TABLE"),
                                 RTSTR, Buffer.get_name(),
                              RTLE,
                              0)) == NULL) return NULL;

   Buffer = LblGroupingTableRef;
   if ((List += acutBuildList(RTLB,
                                 RTSTR, _T("LBL_GRP_TABLE_REF"),
                              0)) == NULL) return NULL;
   Buffer = LblGroupingTableRef;
   if (Buffer.len() > 0 && ConvertDrive2nethost)
   {
      // Conversione riferimento tabella da dir relativo in assoluto
      if (getDBConnection() == NULL) return NULL;
      if (Buffer.paste(getDBConnection()->FullRefTable_drive2nethost(Buffer.get_name())) == NULL)
         return NULL;
   }
   if ((List += gsc_str2rb(Buffer)) == NULL) return NULL;

   Buffer = (SqlCondOnLblGrpTable.len() > 0) ? SqlCondOnLblGrpTable.get_name() : GS_EMPTYSTR;
   restype = (LinkedLblGrpTable) ? RTT : RTNIL;
   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("LINKED_LBL_GRP_TABLE"),
                                 restype,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("SQL_COND_ON_LBL_GRP_TABLE"),
                                 RTSTR, Buffer.get_name(),
                              RTLE,
                              0)) == NULL) return NULL;

   if ((List += acutBuildList(RTLB,
                                 RTSTR, _T("LBL_TABLE_REF"),
                              0)) == NULL) return NULL;
   Buffer = LblTableRef;
   if (Buffer.len() > 0 && ConvertDrive2nethost)
   {
      // Conversione riferimento tabella da dir relativo in assoluto
      if (getDBConnection() == NULL) return NULL;
      if (Buffer.paste(getDBConnection()->FullRefTable_drive2nethost(Buffer.get_name())) == NULL)
         return NULL;
   }
   if ((List += gsc_str2rb(Buffer)) == NULL) return NULL;
   Buffer = (SqlCondOnLblTable.len() > 0) ? SqlCondOnLblTable.get_name() : GS_EMPTYSTR;
   restype = (LinkedLblTable) ? RTT : RTNIL;
   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("LINKED_LBL_TABLE"),
                                 restype,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("SQL_COND_ON_LBL_TABLE"),
                                 RTSTR, Buffer.get_name(),
                              RTLE,
                              0)) == NULL) return NULL;

   if ((List += acutBuildList(RTLB,
                                 RTSTR, _T("UDL_FILE"),
                              0)) == NULL) return NULL;
   Buffer = ConnStrUDLFile;
   if (Buffer.len() > 0 && ConvertDrive2nethost)
      if (gsc_drive2nethost(Buffer) == GS_BAD) return NULL;
   if ((List += gsc_str2rb(Buffer)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("UDL_PROP"),
                              0)) == NULL) return NULL;
   Buffer.paste(gsc_PropListToConnStr(UDLProperties));  
   if (Buffer.len() > 0 && ConvertDrive2nethost)
   {
      // Conversione path UDLProperties da dir relativo in assoluto
      if (getDBConnection() == NULL) return NULL;
      if (getDBConnection()->UDLProperties_drive2nethost(Buffer) == GS_BAD) return NULL;
   }
   if ((List += gsc_str2rb(Buffer)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("KEY_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(key_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("ENT_KEY_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(ent_key_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("ATTRIB_NAME_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(attrib_name_attrib)) == NULL) return NULL;


   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("ATTRIB_INVIS_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(attrib_invis_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("GEOM_DIM"),
                                 RTSHORT, (int) geom_dim,
                              0)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("GEOM_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(geom_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("X_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(x_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("Y_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(y_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("Z_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(z_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("ROTATION_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(rotation_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("ROTATION_UNIT"),
                                 RTSHORT, (int) rotation_unit,
                              0)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("VERTEX_PARENT_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(vertex_parent_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("BULGE_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(bulge_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("AGGR_FACTOR_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(aggr_factor_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("LAYER_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(layer_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("COLOR_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(color_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("COLOR_FORMAT"),
                                 RTSHORT, (int) color_format,
                              0)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("LINE_TYPE_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(line_type_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("LINE_TYPE_SCALE_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(line_type_scale_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("WIDTH_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(width_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("TEXT_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(text_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("TEXT_STYLE_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(text_style_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("H_TEXT_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(h_text_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("HORIZ_ALIGN_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(horiz_align_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("VERT_ALIGN_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(vert_align_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("OBLIQUE_ANGLE_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(oblique_angle_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("BLOCK_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(block_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("BLOCK_SCALE_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(block_scale_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("HATCH_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(hatch_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("HATCH_LAYER_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(hatch_layer_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("HATCH_COLOR_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(hatch_color_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("HATCH_SCALE_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(hatch_scale_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("HATCH_ROTATION_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(hatch_rotation_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("THICKNESS_ATTRIB"),
                              0)) == NULL) return NULL;
   if ((List += gsc_str2rb(thickness_attrib)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE, 0)) == NULL) return NULL;  

   List.ReleaseAllAtDistruction(GS_BAD);

   return List.get_head();
}


/*********************************************************/
/*.doc C_DBGPH_INFO::from_rb                  <internal> */
/*+
  Questa funzione legge le proprietà della C_DBGPH_INFO sottoforma di lista
  di coppie di resbuf ((<proprietà> <valore>) ...).
 (("PRJ"<val>)("CLS"<val>)("SUB"<val>)("COORDINATE"<val>)
  ("UDL_FILE"<val>)("UDL_PROP"<val>)("TABLE_REF"<val>)("LBL_GRP_TABLE_REF"<val>)
  ("LBL_TABLE_REF"<val>)("KEY_ATTRIB"<val>)("ENT_KEY_ATTRIB"<val>)("ATTRIB_NAME_ATTRIB"<val>)
  ("ATTRIB_INVIS_ATTRIB"<val>)("GEOM_DIM"<val>)("GEOM_ATTRIB"<val>)("X_ATTRIB"<val>)
  ("Y_ATTRIB"<val>)("Z_ATTRIB"<val>)("ROTATION_ATTRIB"<val>)("ROTATION_UNIT"<val>)
  ("VERTEX_PARENT_ATTRIB"<val>)("BULGE_ATTRIB"<val>)("AGGR_FACTOR_ATTRIB"<val>)
  ("LAYER_ATTRIB"<val>)("COLOR_ATTRIB"<val>)("COLOR_FORMAT"<val>)
  ("LINE_TYPE_ATTRIB"<val>)("LINE_TYPE_SCALE_ATTRIB"<val>)("WIDTH_ATTRIB"<val>)
  ("TEXT_ATTRIB"<val>)("TEXT_STYLE_ATTRIB"<val>)("H_TEXT_ATTRIB"<val>)("HORIZ_ALIGN_ATTRIB"<val>)
  ("VERT_ALIGN_ATTRIB"<val>)("OBLIQUE_ANGLE_ATTRIB"<val>)
  ("BLOCK_ATTRIB"<val>)("BLOCK_SCALE_ATTRIB"<val>)
  ("HATCH_ATTRIB"<val>)("HATCH_LAYER_ATTRIB"<val>)("HATCH_COLOR_ATTRIB"<val>)
  ("HATCH_SCALE_ATTRIB"<val>)("HATCH_ROTATION_ATTRIB"<val>)
  ("THICKNESS_ATTRIB"<val>)).

  Parametri:
  C_RB_LIST &ColValues; Lista di valori
  oppure
  resbuf *rb;           Lista di valori

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::from_rb(C_RB_LIST &ColValues)
{
   return from_rb(ColValues.get_head());
}
int C_DBGPH_INFO::from_rb(resbuf *rb)
{
   presbuf  p;
   C_STRING Buffer;

   // Scrivo i valori della classe base
   if (C_GPH_INFO::from_rb(rb) == GS_BAD) return GS_BAD;

   if ((p = gsc_CdrAssoc(_T("UDL_FILE"), rb, FALSE)))
      if (p->restype == RTSTR)
      {
         ConnStrUDLFile = p->resval.rstring;
         ConnStrUDLFile.alltrim();
         // traduco dir assoluto in dir relativo
         if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD) return GS_BAD;
      }
      else
         ConnStrUDLFile.clear();

   if ((p = gsc_CdrAssoc(_T("UDL_PROP"), rb, FALSE)))
      if (p->restype == RTSTR) // Le proprietà sono in forma di stringa
      {
         if (gsc_PropListFromConnStr(p->resval.rstring, UDLProperties) == GS_BAD)
            return GS_BAD;
      }
      else
      if (p->restype == RTLB) // Le proprietà sono in forma di lista
      {
         if (gsc_getUDLProperties(&p, UDLProperties) == GS_BAD) return GS_BAD;
      }
      else
         UDLProperties.remove_all();

   // Conversione path UDLProperties da assoluto in dir relativo
   if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(), UDLProperties) == GS_BAD)
      return GS_BAD;

   if (getDBConnection() == NULL) return GS_BAD;

   if ((p = gsc_CdrAssoc(_T("TABLE_REF"), rb, FALSE)))
      if (p->restype == RTSTR)
      {
         // Conversione riferimento tabella da dir relativo in assoluto
         if (TableRef.paste(getDBConnection()->FullRefTable_nethost2drive(p->resval.rstring)) == NULL)
            return NULL;
      }
      else
      if (p->restype == RTLB)
      {  // (<cat><sch><tab>)
         C_STRING Catalog, Schema, Table;

         if (!(p = p->rbnext)) { GS_ERR_COD = eGSInvalidArg; return NULL; }
         if (p->restype == RTSTR) Catalog = p->resval.rstring;
         if (!(p = p->rbnext)) { GS_ERR_COD = eGSInvalidArg; return NULL; }
         if (p->restype == RTSTR) Schema = p->resval.rstring;
         if (!(p = p->rbnext)) { GS_ERR_COD = eGSInvalidArg; return NULL; }
         if (p->restype == RTSTR) Table = p->resval.rstring;
         if (TableRef.paste(getDBConnection()->get_FullRefTable(Catalog, Schema, Table)) == NULL)
            return NULL;
      }
      else
         TableRef.clear();

   if ((p = gsc_CdrAssoc(_T("LINKED_TABLE"), rb, FALSE)))
      if (gsc_rb2Bool(p, &LinkedTable) == GS_BAD) LinkedTable = false;

   if ((p = gsc_CdrAssoc(_T("SQL_COND_ON_TABLE"), rb, FALSE)))
   {
      if (p->restype == RTSTR) Buffer = p->resval.rstring;
      else Buffer.clear();
      
      if (set_SqlCondOnTable(Buffer) == GS_BAD) return GS_BAD;
   }

   if ((p = gsc_CdrAssoc(_T("LBL_GRP_TABLE_REF"), rb, FALSE)))
      if (p->restype == RTSTR)
      {
         // Conversione riferimento tabella da dir relativo in assoluto
         if (LblGroupingTableRef.paste(getDBConnection()->FullRefTable_nethost2drive(p->resval.rstring)) == NULL)
            return NULL;
      }
      else
         LblGroupingTableRef.clear();

   if ((p = gsc_CdrAssoc(_T("LINKED_LBL_GRP_TABLE"), rb, FALSE)))
      if (gsc_rb2Bool(p, &LinkedLblGrpTable) == GS_BAD) LinkedLblGrpTable = false;

   if ((p = gsc_CdrAssoc(_T("SQL_COND_ON_LBL_GRP_TABLE"), rb, FALSE)))
   {
      if (p->restype == RTSTR) Buffer = p->resval.rstring;
      else Buffer.clear();
      
      set_SqlCondOnLblGrpTable(Buffer);
   }

   if ((p = gsc_CdrAssoc(_T("LBL_TABLE_REF"), rb, FALSE)))
      if (p->restype == RTSTR)
      {
         // Conversione riferimento tabella da dir relativo in assoluto
         if (LblTableRef.paste(getDBConnection()->FullRefTable_nethost2drive(p->resval.rstring)) == NULL)
            return NULL;
      }
      else
         LblTableRef.clear();

   if ((p = gsc_CdrAssoc(_T("LINKED_LBL_TABLE"), rb, FALSE)))
      if (gsc_rb2Bool(p, &LinkedLblTable) == GS_BAD) LinkedLblTable = false;

   if ((p = gsc_CdrAssoc(_T("SQL_COND_ON_LBL_TABLE"), rb, FALSE)))
   {
      if (p->restype == RTSTR) Buffer = p->resval.rstring;
      else Buffer.clear();
      
      set_SqlCondOnLblTable(Buffer);
   }

   if ((p = gsc_CdrAssoc(_T("KEY_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) key_attrib = p->resval.rstring;
      else key_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("ENT_KEY_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) ent_key_attrib = p->resval.rstring;
      else ent_key_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("ATTRIB_NAME_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) attrib_name_attrib = p->resval.rstring;
      else attrib_name_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("ATTRIB_INVIS_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) attrib_invis_attrib = p->resval.rstring;
      else attrib_invis_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("GEOM_DIM"), rb, FALSE)))
      if (gsc_rb2Int(p, (int *) &geom_dim) == GS_BAD) geom_dim = GS_2D;

   if ((p = gsc_CdrAssoc(_T("GEOM_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) geom_attrib = p->resval.rstring;
      else geom_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("X_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) x_attrib = p->resval.rstring;
      else x_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("Y_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) y_attrib = p->resval.rstring;
      else y_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("Z_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) z_attrib = p->resval.rstring;
      else z_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("ROTATION_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) rotation_attrib = p->resval.rstring;
      else rotation_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("ROTATION_UNIT"), rb, FALSE)))
      gsc_rb2Int(p, (int *) &rotation_unit);

   if ((p = gsc_CdrAssoc(_T("VERTEX_PARENT_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) vertex_parent_attrib = p->resval.rstring;
      else vertex_parent_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("BULGE_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) bulge_attrib = p->resval.rstring;
      else bulge_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("AGGR_FACTOR_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) aggr_factor_attrib = p->resval.rstring;
      else aggr_factor_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("LAYER_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) layer_attrib = p->resval.rstring;
      else layer_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("COLOR_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) color_attrib = p->resval.rstring;
      else color_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("COLOR_FORMAT"), rb, FALSE)))
      gsc_rb2Int(p, (int *) &color_format);

   if ((p = gsc_CdrAssoc(_T("LINE_TYPE_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) line_type_attrib = p->resval.rstring;
      else line_type_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("LINE_TYPE_SCALE_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) line_type_scale_attrib = p->resval.rstring;
      else line_type_scale_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("WIDTH_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) width_attrib = p->resval.rstring;
      else width_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("TEXT_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) text_attrib = p->resval.rstring;
      else text_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("TEXT_STYLE_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) text_style_attrib = p->resval.rstring;
      else text_style_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("H_TEXT_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) h_text_attrib = p->resval.rstring;
      else h_text_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("HORIZ_ALIGN_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) horiz_align_attrib = p->resval.rstring;
      else horiz_align_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("VERT_ALIGN_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) vert_align_attrib = p->resval.rstring;
      else vert_align_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("OBLIQUE_ANGLE_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) oblique_angle_attrib = p->resval.rstring;
      else oblique_angle_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("BLOCK_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) block_attrib = p->resval.rstring;
      else block_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("BLOCK_SCALE_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) block_scale_attrib = p->resval.rstring;
      else block_scale_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("HATCH_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) hatch_attrib = p->resval.rstring;
      else hatch_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("HATCH_LAYER_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) hatch_layer_attrib = p->resval.rstring;
      else hatch_layer_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("HATCH_COLOR_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) hatch_color_attrib = p->resval.rstring;
      else hatch_color_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("HATCH_SCALE_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) hatch_scale_attrib = p->resval.rstring;
      else hatch_scale_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("HATCH_ROTATION_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) hatch_rotation_attrib = p->resval.rstring;
      else hatch_rotation_attrib.clear();

   if ((p = gsc_CdrAssoc(_T("THICKNESS_ATTRIB"), rb, FALSE)))
      if (p->restype == RTSTR) thickness_attrib = p->resval.rstring;
      else thickness_attrib.clear();

   return GS_GOOD;
}


/***********************************************************/
/*.doc C_DBGPH_INFO::isValid                    <external> */
/*+
  Questa funzione verifica la correttezza della C_DBGPH_INFO.
  Parametri:
  int GeomType; Usato per stabilire il tipo di dato geometrico
                (TYPE_POLYLINE, TYPE_TEXT, TYPE_NODE, TYPE_SURFACE, TYPE_SPAGHETTI)
                Default = TYPE_SPAGHETTI ovvero tipo misto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/***********************************************************/
int C_DBGPH_INFO::isValid(int GeomType)
{
   C_GPH_INFO::isValid(GeomType);

   if (getDBConnection() == NULL) return GS_BAD;

   if (coordinate_system.len() == 0) coordinate_system = DEFAULT_DBGRAPHCOORD;

   if (pGeomConn->IsValidFieldName(key_attrib) == GS_BAD) // Obbligatorio
      { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if (GeomType == TYPE_SPAGHETTI)
   {
      // Se si tratta di spaghetti il campo chiave geometrico coincide con quello dell'entità
      if (key_attrib.comp(ent_key_attrib) != 0) ent_key_attrib = key_attrib;
   }
   else
   {
      if (pGeomConn->IsValidFieldName(ent_key_attrib) == GS_BAD) // Obbligatorio
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   }

   // Se si tratta di una tabella collegata esterna oppure 
   // spaghetti o testi non gestisco le tabelle delle etichette
   if (LinkedTable || GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT)
   {
      LblTableRef.clear();
      LblGroupingTableRef.clear();
   }
   else // Se non si tratta di spaghetti verifico che esista la tabella delle etichette
   if (LblTableRef.len() == 0)
   {
      C_STRING Catalog, Schema, GeomName, NewName;

      if (pGeomConn->split_FullRefTable(TableRef, Catalog, Schema, GeomName) == GS_BAD)
         return GS_BAD;
      NewName = GeomName;
      NewName += _T("_lbl");
      if (LblTableRef.paste(pGeomConn->get_FullRefTable(Catalog, Schema, NewName)) == NULL)
         return GS_BAD;

      if (LblGroupingTableRef.len() == 0)
      {
         NewName = GeomName;
         NewName += _T("_lbl_grp");
         if (LblGroupingTableRef.paste(pGeomConn->get_FullRefTable(Catalog, Schema,
                                                                   NewName)) == NULL)
            return GS_BAD;
      }
   }

   if (LblTableRef.len() > 0) // Se esiste la tabella delle etichette
   {
      if (pGeomConn->IsValidFieldName(attrib_name_attrib) == GS_BAD) // Obbligatorio
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
      if (attrib_invis_attrib.len() > 0) // Opzionale
         if (pGeomConn->IsValidFieldName(attrib_invis_attrib) == GS_BAD)
            { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   }

   if (geom_attrib.len() == 0)
   {
      if (pGeomConn->IsValidFieldName(x_attrib) == GS_BAD) // Obbligatorio
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
      if (pGeomConn->IsValidFieldName(y_attrib) == GS_BAD) // Obbligatorio
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
      if (geom_dim == GS_3D) 
         if (pGeomConn->IsValidFieldName(z_attrib) == GS_BAD)
            { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

      if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_POLYLINE || 
          GeomType == TYPE_SURFACE)
      {
         if (pGeomConn->IsValidFieldName(vertex_parent_attrib) == GS_BAD) // Obbligatorio
            { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
         if (bulge_attrib.len() > 0) // Opzionale
            if (pGeomConn->IsValidFieldName(bulge_attrib) == GS_BAD)
               { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
      }
   }
   else
      if (pGeomConn->IsValidFieldName(geom_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if (rotation_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(rotation_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   // Se si tratta di una tabella collegata esterna
   if (LinkedTable)
   {
      if (aggr_factor_attrib.len() > 0) // Opzionale
         if (pGeomConn->IsValidFieldName(aggr_factor_attrib) == GS_BAD)
            { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   }
   else
      // per le superfici è obbligatorio
      if (GeomType == TYPE_SURFACE)
      {
         if (pGeomConn->IsValidFieldName(aggr_factor_attrib) == GS_BAD) // Obbligatorio
            { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
      }
      else
         if (GeomType == TYPE_SPAGHETTI)
            aggr_factor_attrib.clear(); // non ha senso il fattore di aggregazione
         else // per le altre tipologie di classi
            if (aggr_factor_attrib.len() > 0) // Opzionale
               if (pGeomConn->IsValidFieldName(aggr_factor_attrib) == GS_BAD)
                  { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if (layer_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(layer_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
      
   if (color_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(color_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if (line_type_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(line_type_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (line_type_scale_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(line_type_scale_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (width_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(width_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT ||
       LblTableRef.len() > 0) // Se esiste la tabella delle etichette
   {
      if (pGeomConn->IsValidFieldName(text_attrib) == GS_BAD) // Obbligatorio
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   }

   if (text_style_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(text_style_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (h_text_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(h_text_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (horiz_align_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(horiz_align_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (vert_align_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(vert_align_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (oblique_angle_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(oblique_angle_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if (block_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(block_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (block_scale_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(block_scale_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if (thickness_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(thickness_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   if (hatch_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(hatch_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (hatch_layer_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(hatch_layer_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (hatch_color_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(hatch_color_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (hatch_scale_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(hatch_scale_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }
   if (hatch_rotation_attrib.len() > 0) // Opzionale
      if (pGeomConn->IsValidFieldName(hatch_rotation_attrib) == GS_BAD)
         { GS_ERR_COD = eGSInvAttribName; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreateResource           <external> */
/*+
  Questa funzione crea le risorse necessarie per memorizzare
  le informazioni geometriche.
  Parametri:
  int GeomType; Usato per stabilire il tipo di dato geometrico
                (TYPE_POLYLINE, TYPE_TEXT, TYPE_NODE, TYPE_SURFACE, TYPE_SPAGHETTI)
                Default = TYPE_SPAGHETTI ovvero tipo misto

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::CreateResource(int GeomType)
{
   C_RB_LIST ColValues;

   // ricavo connessione OLE-DB
   if (getDBConnection() == NULL) return GS_BAD;

   if (LinkedTable)
   {
      // Se non esiste la tabella della geometria
      if (getDBConnection()->ExistTable(TableRef) == GS_BAD)
         { GS_ERR_COD = eGSTableNotExisting; return GS_BAD; }
   }
   else
      // Creo la tabella della geometria
      if (CreateGphTable(TableRef, false, GeomType) == GS_BAD) return GS_BAD;

   // I tipi spaghetti e testo non prevedono label
   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT) return GS_GOOD;

   if (LblGroupingTableRef.len() > 0)
      if (LinkedLblGrpTable)
      {
         // Se non esiste la tabella di raggruppamento delle etichette
         if (getDBConnection()->ExistTable(LblGroupingTableRef) == GS_BAD)
            { GS_ERR_COD = eGSTableNotExisting; return GS_BAD; }
      }
      else
         // Creo la tabella di raggruppamento delle etichette
         if (CreateGphTable(LblGroupingTableRef, false, TYPE_NODE) == GS_BAD) return GS_BAD;

   if (LblTableRef.len() > 0)
      if (LinkedLblTable)
      {
         // Se non esiste la tabella delle etichette
         if (getDBConnection()->ExistTable(LblTableRef) == GS_BAD)
            { GS_ERR_COD = eGSTableNotExisting; return GS_BAD; }
      }
      else
         // Creo la tabella delle etichette
         if (CreateGphTable(LblTableRef, true) == GS_BAD) return GS_BAD;

   return GS_GOOD; 
}


/*********************************************************/
/*.doc C_DBGPH_INFO::RemoveResource           <internal> */
/*+
  Questa funzione rimuove la sorgente grafica.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::RemoveResource(void)
{
   // ricavo connessione OLE-DB
   if (getDBConnection() == NULL) return GS_BAD;

   if (LinkedTable == false) // tabella gestita da GEOsim
      // Se esiste la tabella della geometria
      if (getDBConnection()->ExistTable(TableRef.get_name()) == GS_GOOD)
         // cancello la tabella
         if (getDBConnection()->DelTable(TableRef.get_name()) != GS_GOOD) return GS_BAD;

   if (LblGroupingTableRef.len() > 0)
      if (LinkedLblGrpTable == false) // tabella gestita da GEOsim
         // Se esiste la tabella di raggruppamento delle etichette
         if (getDBConnection()->ExistTable(LblGroupingTableRef.get_name()) == GS_GOOD)
            // cancello la tabella
            if (getDBConnection()->DelTable(LblGroupingTableRef.get_name()) != GS_GOOD) return GS_BAD;

   if (LblTableRef.len() > 0)
      if (LinkedLblTable == false) // tabella gestita da GEOsim
         // Se esiste la tabella delle etichette
         if (getDBConnection()->ExistTable(LblTableRef.get_name()) == GS_GOOD)
            // cancello la tabella
            if (getDBConnection()->DelTable(LblTableRef.get_name()) != GS_GOOD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::RemoveResource           <internal> */
/*+
  Restituisce vero se entrambe le sorgenti grafiche puntano alle stesse tabelle.
-*/  
/*********************************************************/
bool C_DBGPH_INFO::IsTheSameResource(C_GPH_INFO *p)
{
   if (!p) return false;

   // Se il tipo di risorsa è diverso
   if (getDataSourceType() != p->getDataSourceType()) return false;

   // Se la connessione a DB è diversa
   if (getDBConnection() != ((C_DBGPH_INFO *)p)->getDBConnection()) return false;

   // Se la tabella della geometria è diversa
   if (TableRef.comp(((C_DBGPH_INFO *)p)->TableRef) != 0) return false;

   // Se la tabella di raggruppamento delle etichette è diversa
   if (LblTableRef.comp(((C_DBGPH_INFO *)p)->LblGroupingTableRef) != 0) return false;

   // Se la tabella delle etichette è diversa
   if (LblTableRef.comp(((C_DBGPH_INFO *)p)->LblTableRef) != 0) return false;

   return true;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::ResourceExist            <external> */
/*+
  Questa funzione restituisce true se la risorsa grafica è già esistente 
  (anche parzialmente) altrimenti restituisce false.
  Parametri:
  bool *pGeomExist;        (output) Vero se esiste la tabella della geometria
  bool *pLblGroupingExist; (output) Vero se esiste la tabella di raggruppamento delle etichette
  bool *pLblExist;         (output) Vero se esiste la tabella delle etichette
-*/  
/*********************************************************/
bool C_DBGPH_INFO::ResourceExist(bool *pGeomExist, bool *pLblGroupingExist, bool *pLblExist)
{
   bool result = false;

   if (pGeomExist) *pGeomExist = false;
   if (pLblGroupingExist) *pLblGroupingExist = false;
   if (pLblExist) *pLblExist = false;

   // ricavo connessione OLE-DB
   if (getDBConnection() == NULL) return false;

   // Se esiste la tabella della geometria
   if (getDBConnection()->ExistTable(TableRef) == GS_GOOD)
   {
      if (pGeomExist) *pGeomExist = true;
      result = true;
   }

   if (LblGroupingTableRef.len() > 0)
      // Se esiste la tabella di raggruppamento delle etichette
      if (getDBConnection()->ExistTable(LblGroupingTableRef) == GS_GOOD)
      {
         if (pLblGroupingExist) *pLblGroupingExist = true;
         result = true;
      }

   if (LblTableRef.len() > 0)
      // Se esiste la tabella delle etichette
      if (getDBConnection()->ExistTable(LblTableRef) == GS_GOOD)
      {
         if (pLblExist) *pLblExist = true;
         result = true;
      }

   return result; 
}


/*****************************************************************************/
/*.doc C_DBGPH_INFO::get_CurrWrkSessionSRID_converted_to_DBSRID <internal> */
/*+
  Questa funzione restituisce il puntatore allo SRID della sessione di lavoro corrente
  convertito (se possibile) nello SRID gestito dal DBMS.
-*/  
/*****************************************************************************/
C_STRING *C_DBGPH_INFO::get_CurrWrkSessionSRID_converted_to_DBSRID(void)
{
   if (CurrWrkSessionSRID_converted_to_DBSRID.len() == 0)
      if (GS_CURRENT_WRK_SESSION)
         // Se è stato dichiarato un sistema di coordinate x la sessione corrente
         if (gsc_strlen(GS_CURRENT_WRK_SESSION->get_coordinate()) > 0)
         {
            C_DBCONNECTION *pConn = getDBConnection();
            GSSRIDTypeEnum ClsSRIDType;
            GSSRIDUnitEnum dummy;

            if (pConn->get_SRIDType(ClsSRIDType) == GS_GOOD)
               if (gsc_get_srid(GS_CURRENT_WRK_SESSION->get_coordinate(), GSSRID_Autodesk,
                                ClsSRIDType, CurrWrkSessionSRID_converted_to_DBSRID, &dummy) != GS_GOOD)
                  CurrWrkSessionSRID_converted_to_DBSRID.clear();
         }

   return &CurrWrkSessionSRID_converted_to_DBSRID;
}


/*****************************************************************************/
/*.doc C_DBGPH_INFO::get_ClsSRID_converted_to_AutocadSRID         <internal> */
/*+
  Questa funzione carica lo SRID in formato Autocad e le unità del 
  sistema di coordinate della classe. 
  Restituisce il puntatore allo SRID della classe convertito (se possibile) 
  nello SRID gestito da Autocad.
-*/  
/*****************************************************************************/
C_STRING *C_DBGPH_INFO::get_ClsSRID_converted_to_AutocadSRID(void)
{
   if (ClsSRID_converted_to_AutocadSRID.len() == 0)
      // Se è stato dichiarato un sistema di coordinate per la classe
      if (coordinate_system.len() > 0)
      {
         C_DBCONNECTION *pConn = getDBConnection();
         GSSRIDTypeEnum ClsSRIDType;

         if (pConn->get_SRIDType(ClsSRIDType) == GS_GOOD)
            if (gsc_get_srid(coordinate_system, ClsSRIDType,
                             GSSRID_Autodesk, ClsSRID_converted_to_AutocadSRID, &ClsSRID_unit) != GS_GOOD)
            {
               ClsSRID_converted_to_AutocadSRID.clear();
               ClsSRID_unit = GSSRIDUnit_None;
            }
      }

   return &ClsSRID_converted_to_AutocadSRID;
}


/*****************************************************************************/
/*.doc C_DBGPH_INFO::get_isCoordToConvert                         <internal> */
/*+
  Questa funzione restituisce se è necessario effettuare la conversione delle
  coordinate degli oggetti dal sistema nella sessione di lavoro e quello nel DB.

  Ritorna GS_GOOD = da convertire, GS_BAD = da NON convertire, GS_CAN = non inizializzato
-*/  
/*****************************************************************************/
int C_DBGPH_INFO::get_isCoordToConvert(void)
{
   if (isCoordToConvert != GS_CAN) // se il valore è già stato inizializzato
      return isCoordToConvert;

   if (!GS_CURRENT_WRK_SESSION)
      isCoordToConvert = GS_CAN;
   else
   {
      // Se la sessione di lavoro corrente e la classe hanno specificato il 
      // sistema di coordinate valido (-1 = non specificato in PostGIS)
      if ((get_CurrWrkSessionSRID_converted_to_DBSRID()->len() > 0 &&
           get_CurrWrkSessionSRID_converted_to_DBSRID()->comp(_T("-1")) != 0) && 
          (coordinate_system.len() > 0 && 
           coordinate_system.comp(_T("-1")) != 0))
         // Se i sistemi di coordinate NON coincidono, la conversione è necessaria
         isCoordToConvert = (get_CurrWrkSessionSRID_converted_to_DBSRID()->comp(coordinate_system, FALSE) != 0) ? GS_GOOD : GS_BAD;
      else // la sessione di lavoro corrente o la classe non hanno specificato il 
           // sistema di coordinate valido
         isCoordToConvert = GS_BAD;
   }

   return isCoordToConvert;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::LoadSpatialQueryFromADEQry <internal> */
/*+
  Questa funzione carica la condizione di query spaziale dalla query ADE
  precedentemente salvata con nome "spaz_estr"
  Parametri:
  TCHAR *qryName; Opzionale, nome della query ADE. 
                  Se = NULL viene usata la query ADE di default = "spaz_estr"

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::LoadSpatialQueryFromADEQry(TCHAR *qryName)
{
   // carico e attivo la query spaziale
   if (gsc_ade_qlloadqry(qryName) != RTNORM) return GS_BAD;
   
   return GS_GOOD; 
}


/*********************************************************/
/*.doc C_DBGPH_INFO::Query                    <external> */
/*+
  Questa funzione legge dalle tabelle DB gli oggetti che soddisfano la
  query ADE corrente e poi li visualizza e/o ne fa il preview e/o ne fa un report.
  La funzione si è resa necessaria per compatibilità con quella della classe
  C_DWG_INFO.
  Parametri:
  int WhatToDo;      Flag a bit che determina cosa deve essere fatto degli oggetti 
                     letti dalla fonte grafica (PREVIEW, EXTRACTION, REPORT)
  C_SELSET *pSelSet; Opzionale; Usato nel modo EXTRACION è il gruppo di selezione 
                     degli oggetti disegnati (default = NULL)
  long BitForFAS;    Opzionale; Usato nel modo EXTRACION e PREVIEW è un flag 
                     a bit indicante quali caratteristiche grafiche devono essere 
                     impostate per gli oggetti (default = GSNoneSetting)
  C_FAS *pFAS;       Opzionale; Usato nel modo EXTRACION e PREVIEW è un
                     set di alterazione caratteristiche grafiche (default = NULL)
  C_STRING *pTemplate;     Opzionale; Usato nel modo REPORT è un modello 
                           contenente quali informazioni stampare nel file report
  C_STRING *pRptFile;    Opzionale; Usato nel modo REPORT è il percorso 
                         completo del file di report
  const TCHAR *pRptMode; Opzionale; Usato nel modo REPORT è un modo di apertura
                         del file di report:
                         "w" apre un file vuoto in scrittura, se il file 
                         esisteva il suo contenuto è distrutto,
                         "a" apre il file in scrittura aggiungendo alla fine 
                         del file senza rimuovere il contenuto del file; 
                         crea prima il file se non esiste.
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce il numero di oggetti trovati in caso di successo altrimenti 
  restituisce -1 in caso di errore e -2 in caso di impossibilità.
-*/  
/*********************************************************/
long C_DBGPH_INFO::Query(int WhatToDo,
                         C_SELSET *pSelSet, long BitForFAS, C_FAS *pFAS,
                         C_STRING *pRptTemplate, C_STRING *pRptFile, const TCHAR *pRptMode,
                         int CounterToVideo)
{
   C_GPH_OBJ_ARRAY Objs;
   int             Result;

   // Cerco gli oggetti
   //if ((Result = ApplyQuery(Objs)) != GS_GOOD)
   if ((Result = PROFILE_INT_FUNC(ApplyQuery, Objs, CounterToVideo)) != GS_GOOD)
      return (Result == GS_BAD) ? -1 : -2;

   // se si devono creare degli oggetti autocad nella sessione corrente
   if (WhatToDo & EXTRACTION)
      if ((Result = QueryIn(Objs, pSelSet, BitForFAS, pFAS, CounterToVideo)) != GS_GOOD)
         return (Result == GS_BAD) ? -1 : -2;

   // se si deve presentare un'anteprima degli oggetti a video
   // La funzione Preview ritorna GS_CAN se non è possibile fare il preview 
   // (la funzione continua senza segnalare l'errore per supporare l'estrazione per entità)
   // e GS_BAD in caso di errore
   if (WhatToDo & PREVIEW)
      if ((Result = Preview(Objs, BitForFAS, pFAS)) == GS_BAD) return -1;

   // se si deve scrivere un file di report degli oggetti
   if (WhatToDo & REPORT)
      if (pRptTemplate && pRptFile)
         if ((Result = Report(Objs, *pRptTemplate, *pRptFile, _T("a"))) != GS_GOOD)
            return (Result == GS_BAD) ? -1 : -2;

   return Objs.length();
}


/*********************************************************/
/*.doc C_DBGPH_INFO::ApplyQuery               <internal> */
/*+
  Questa funzione legge dalle tabelle DB gli oggetti che soddisfano la
  query ADE corrente.
  Parametri:
  C_GPH_OBJ_ARRAY &Objs; Oggetti risultanti dalla ricerca
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::ApplyQuery(C_GPH_OBJ_ARRAY &Objs, int CounterToVideo)
{
   C_STRING      GeomSQL, LabelSQL;
   _RecordsetPtr pRs;
   C_CLASS       *pCls = gsc_find_class(prj, cls, sub);

   if (!pCls) return GS_BAD;

   if (AdjTableParams() == GS_BAD) return GS_BAD;
   if (CurrentQryAdeToSQL(GeomSQL, LabelSQL) == GS_BAD) return GS_BAD;

   Objs.ObjType = AcDbEntityType;
   Objs.removeAll();

   if (GeomSQL.len() > 0)
   {
      #if defined(GSDEBUG) // se versione per debugging
         double t1 = gsc_get_profile_curr_time();
      #endif
      if (getDBConnection()->ExeCmd(GeomSQL, pRs) == GS_BAD) return GS_BAD;
      #if defined(GSDEBUG) // se versione per debugging
         gsc_profile_int_func(t1, 0, gsc_get_profile_curr_time(), "ExeCmd(GeomSQL");
      #endif
      if (AppendOnAcDbEntityPtrArray(pRs, Objs.Ents, Objs.KeyEnts, CounterToVideo) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      gsc_DBCloseRs(pRs);
   }

   // le label vengono lette dopo per essere disegnate dopo gli oggetti principali
   // (così vengono disegnate sopra)
   if (LabelSQL.len() > 0)
   {
      #if defined(GSDEBUG) // se versione per debugging
         double t1 = gsc_get_profile_curr_time();
      #endif
      if (getDBConnection()->ExeCmd(LabelSQL, pRs) == GS_BAD) return GS_BAD;
      #if defined(GSDEBUG) // se versione per debugging
         gsc_profile_int_func(t1, 0, gsc_get_profile_curr_time(), "ExeCmd(LabelSQL");
      #endif
      if (AppendDABlockOnAcDbEntityPtrArray(pRs, Objs.Ents, Objs.KeyEnts, CounterToVideo) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      gsc_DBCloseRs(pRs);
   }

   return GS_GOOD; 
}


/*********************************************************/
/*.doc C_DBGPH_INFO::PrepareCmdCountAggr      <internal> */
/*+
  Questa funzione genera il comando SQL compilato per il conteggio
  del fattore di aggregazione di un'entità GEOsim.
  Parametri:
  _CommandPtr &pCmd;

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::PrepareCmdCountAggr(_CommandPtr &pCmd)
{
   C_STRING      Stm;
   DataTypeEnum  DataType = adDouble;
   long          Size = 0;
   _ParameterPtr pParam;
   C_CLASS       *pCls;
   C_ID          *pId;

   // Ritorna il puntatore alla classe cercata
   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return GS_BAD;
   pId = pCls->ptr_id();

   if (pId->category == CAT_SPAGHETTI) return GS_GOOD;

   // Se esiste il fattore di aggregazione nei db allora non lo devo calcolare
   if (aggr_factor_attrib.len() > 0) return GS_GOOD;

   Size = getDBConnection()->ptr_DataTypeList()->search_Type(adDouble, TRUE)->get_Size();

   // Se esiste una tabella per le etichette
   if (LabelWith())
   {
      // SELECT SUM(AggrFact) FROM 
      // (SELECT COUNT(*) AS AggrFact FROM Tab1 WHERE gs_id=? UNION ALL
      // usare AS solo per l'alias dei campi e NON delle tabelle perchè ORACLE non lo supporta
      Stm = _T("SELECT SUM(AggrFact) FROM ");
      Stm += _T("(SELECT COUNT(*) AS AggrFact FROM ");
      Stm += TableRef;
      Stm += _T(" WHERE ");
      Stm += ent_key_attrib;
      Stm += _T("=?");

      Stm += _T(" UNION ALL ");

      // Possono esserci 2 casi:
      // 1) le etichette sono sotto forma di blocchi con attributi in 2 tabelle distinte
      // che sono unite tra loro con una join SQL
      // 2) le etichette sono sottoforma di testi
      if (LblGroupingTableRef.len() > 0) // caso 1
      {
         //  SELECT COUNT(*) AS AggrFact FROM Tab2 WHERE id_parent=?)
         // usare AS solo per l'alias dei campi e NON delle tabelle perchè ORACLE non lo supporta
         Stm += _T("SELECT COUNT(*) AS AggrFact FROM ");
         Stm += LblGroupingTableRef;
         Stm += _T(" WHERE ");
         Stm += ent_key_attrib;
         Stm += _T("=?");
      }
      else // caso 2
      {
         // SELECT COUNT(*) AS AggrFact FROM (SELECT DISTINCT id_parent FROM Tab2 WHERE id_parent=?)
         // usare AS solo per l'alias dei campi e NON delle tabelle perchè ORACLE non lo supporta
         Stm += _T("SELECT COUNT(*) AS AggrFact FROM ");
         Stm += LblTableRef;
         Stm += _T(" WHERE ");
         Stm += ent_key_attrib;
         Stm += _T("=?");
      }
      // usare AS solo per l'alias dei campi e NON delle tabelle perchè ORACLE non lo supporta
      Stm += _T(") MYALIAS");
   }
   else // non esistono etichette
   {
      // se la tabella della geometria e quella alfanumerica coincidono allora e il campo chiave è lo stesso
      // allora il fattore di aggregazione è 1 e non è necessario calcolarlo
      if (getDBConnection() == pCls->ptr_info()->getDBConnection(OLD) &&
          ent_key_attrib.comp(pCls->ptr_info()->key_attrib) == 0)
         return GS_GOOD;

      // SELECT COUNT(*) AS AggrFact FROM Tab1 WHERE gs_id=?
      // usare AS solo per l'alias dei campi e NON delle tabelle perchè ORACLE non lo supporta
      Stm = _T("SELECT COUNT(*) AS AggrFact FROM ");
      Stm += TableRef;
      Stm += _T(" WHERE ");
      Stm += ent_key_attrib;
      Stm += _T("=?");
   }

   // preparo comando SQL
   if (getDBConnection()->PrepareCmd(Stm, pCmd) == GS_BAD) return GS_BAD;
   // Creo una nuovo parametro
   if (gsc_CreateDBParameter(pParam, _T("?"), DataType, Size) == GS_BAD) return GS_BAD;
   pParam->PutType(adDouble);
   // Aggiungo parametro
   pCmd->Parameters->Append(pParam);

   // Se la classe non è testuale e ha una tabella per le etichette
   if (LabelWith())
   {
      // Creo una nuovo parametro
      if (gsc_CreateDBParameter(pParam, _T("?"), DataType, Size) == GS_BAD) return GS_BAD;
      pParam->PutType(adDouble);
      // Aggiungo parametro
      pCmd->Parameters->Append(pParam);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::AddToLinkTree            <internal> */
/*+
  Questa funzione inserisce nella lista in memoria delle coppie <handle>-<id> e 
  <id>-<puntatore a <handle-id>> (in forma di b-albero) un handle e il corrispondente
  codice univoco per la geometria in DB.
  Parametri:
  const TCHAR *Handle;  Handle
  double GeomKey;       Codice univoco per la geometria in DB (l'eventuale parte 
                        decimale serve per distinguere più geometrie lette da
                        uno stesso campo geometrico es. polygon)                        
  int What;             GRAPHICAL per gli oggetti grafici
                        DA_BLOCK  per i blocchi degli attributi visibili

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_DBGPH_INFO::AddToLinkTree(const TCHAR *Handle, double GeomKey, int What)
{
   // aggiorno la lista in memoria delle coppie <handle>-<id> e 
   // <id>-<puntatore a <handle-id>>
   HandleId_LinkTree.add(Handle);
   ((C_BSTR_REAL *) HandleId_LinkTree.get_cursor())->set_key(GeomKey);

   if (What == GRAPHICAL)
   {
      IndexByID_LinkTree.add(&GeomKey);
      ((C_BID_HANDLE *) IndexByID_LinkTree.get_cursor())->set_pHandle((C_BSTR_REAL *) HandleId_LinkTree.get_cursor());
   }
   else
   {
      DAIndexByID_LinkTree.add(&GeomKey);
      ((C_BID_HANDLE *) DAIndexByID_LinkTree.get_cursor())->set_pHandle((C_BSTR_REAL *) HandleId_LinkTree.get_cursor());
   }
}


/*********************************************************/
/*.doc C_DBGPH_INFO::DelToLinkTree            <internal> */
/*+
  Questa funzione cancella nella lista in memoria delle coppie <handle>-<id> e 
  <id>-<puntatore a <handle-id>> (in forma di b-albero) un handle e il corrispondente
  codice univoco per la geometria in DB.
  Parametri:
  const TCHAR *Handle;  Handle
  int What;             GRAPHICAL per gli oggetti grafici
                        DA_BLOCK  per i blocchi degli attributi visibili

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_DBGPH_INFO::DelToLinkTree(const TCHAR *Handle, int What)
{
   C_BSTR_REAL *pHandleId = (C_BSTR_REAL *) HandleId_LinkTree.search(Handle);
   double      GeomKey;

   if (!pHandleId) return;
   GeomKey = pHandleId->get_key();

   // aggiorno la lista in memoria delle coppie <handle>-<id> e 
   // <id>-<puntatore a <handle-id>>

   if (What == GRAPHICAL)
      IndexByID_LinkTree.remove(&GeomKey);
   else
      DAIndexByID_LinkTree.remove(&GeomKey);

   HandleId_LinkTree.remove_at();
}


/*********************************************************/
/*.doc C_DBGPH_INFO::QueryIn                  <internal> */
/*+
  Questa funzione inserisce gli oggetti grafici letti dalla query
  nel disegno corrente senza duplicare oggetti già estratti.
  Parametri:
  C_GPH_OBJ_ARRAY &Objs;  Oggetti risultanti dalla ricerca
  C_SELSET      *pSelSet; Opzionale; Gruppo di selezione degli oggetti disegnati 
                          (default = NULL)
  long         BitForFAS; Opzionale; Flag a bit indicante quali caratteristiche grafiche
                          devono essere impostate per gli oggetti 
                          (default = GSNoneSetting)
  C_FAS         *pFAS;    Opzionale; Set di alterazione caratteristiche grafiche (default = NULL)
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::QueryIn(C_GPH_OBJ_ARRAY &Objs, C_SELSET *pSelSet, 
                          long BitForFAS, C_FAS *pFAS, int CounterToVideo)
{
   C_INT_LONG            *pKeyEnt = (C_INT_LONG *) Objs.KeyEnts.get_head();
   double                Id;
   AcDbBlockTable        *pBlockTable;
   AcDbBlockTableRecord  *pInternalBlockTableRecord;
   AcDbObjectId          OutputId;
   TCHAR                 Handle[MAX_LEN_HANDLE];
   _CommandPtr           pCmd;
   C_EED                 eed;
   _RecordsetPtr         pInternalRs;
   C_RB_LIST             ColValues;
   presbuf               pRb = NULL;
   int                   Result = GS_GOOD, AggrFactor, i, Added = 0;
   ads_name              ent;
   C_BLONG_INT           *pIDAggrFactor;
   C_LONG_INT_BTREE      IDAggrFactor_Tree;
   C_BID_HANDLE_BTREE    *pNdxTree;
   AcMapODTable          *pODTable;
   AcMapODTableRecord    ODRecord;
   AcMapValue            ODValue;
   C_STRING              IDStr;
   C_SELSET              AlteredSS;
   C_CLASS               *pCls = gsc_find_class(prj, cls, sub);
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(481)); // "Visualizzazione oggetti"

//#if defined(GSDEBUG) // se versione per debugging
//   struct _timeb t1, t2;
//   double tempo=0;
//   _ftime(&t1);
//#endif

   if (Objs.Ents.length() == 0) return GS_GOOD;
   if (!pCls) return GS_BAD;

   // preparo i comandi per leggere i fattori di aggregazione
   if (PrepareCmdCountAggr(pCmd) == GS_BAD) return GS_BAD;

   // Ottengo la tabella OD della classe
   if ((pODTable = gsc_getODTable(prj, cls, sub)) == NULL) return GS_BAD;
   pODTable->InitRecord(ODRecord);

   if (acdbHostApplicationServices()->workingDatabase()->getSymbolTable(pBlockTable, AcDb::kForRead) != Acad::eOk)
      { GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   if (pBlockTable->getAt(ACDB_MODEL_SPACE, pInternalBlockTableRecord, AcDb::kForWrite) != Acad::eOk)
      { pBlockTable->close(); GS_ERR_COD = eGSAdsCommandErr; return GS_BAD; }
   pBlockTable->close();

   gsc_startTransaction(); // con le transazioni grafiche sembra più lento...
   acTransactionManagerPtr()->queueForGraphicsFlush();

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(Objs.Ents.length());
   
   // Ciclo gli oggetti in Objs
   for (i = 0; i < Objs.Ents.length(); i++)
   {
      Id = pKeyEnt->get_id(); // leggo il codice dell'oggetto grafico
      // se l'oggetto geometrico è composto da + sotto-geometrie (es polygon)
      if (pKeyEnt->get_key() > 1)
         Id += (pKeyEnt->get_key() / 100.0);

      // Se get_key > 0 significa che è un oggetto grafico principale
      // altrimenti è un blocco DA
      pNdxTree = (pKeyEnt->get_key() > 0) ? &IndexByID_LinkTree : &DAIndexByID_LinkTree;

      // se è già stato estratto lo salto cercandolo nell'indice dell'albero su ID
      if (pNdxTree->search(&Id) == NULL)
      {
         // ricavo il codice dell'entità GEOsim
         if (pCls->get_type() != TYPE_SPAGHETTI) eed.load(Objs.Ents.at(i));

         if (pCmd.GetInterfacePtr() != NULL) // Fattore di aggregazione - inizio
         {
            // se il fattore di aggregazione NON è già stato calcolato
            if ((pIDAggrFactor = (C_BLONG_INT *) IDAggrFactor_Tree.search(&(eed.gs_id))) == NULL)
            {
               // determino il suo fattore di aggregazione
               if (gsc_SetDBParam(pCmd, 0, eed.gs_id) == GS_BAD) { Result = GS_BAD; break; }
               // Se esiste una tabella per le etichette
               if (LabelWith()) // c'è un secondo parametro SQL
                  if (gsc_SetDBParam(pCmd, 1, eed.gs_id) == GS_BAD) { Result = GS_BAD; break; }

               if (gsc_ExeCmd(pCmd, pInternalRs) == GS_BAD) { Result = GS_BAD; break; }
               if (gsc_DBReadRow(pInternalRs, ColValues) == GS_BAD)
                  { gsc_DBCloseRs(pInternalRs); Result = GS_BAD; break; }
               if (gsc_DBCloseRs(pInternalRs) == GS_BAD) { Result = GS_BAD; break; }
               if (!pRb) pRb = ColValues.getptr_at(4); // se non inizializzato punta al quarto elemento

               gsc_rb2Int(pRb, &AggrFactor); // leggo il fattore di aggregazione

               if (AggrFactor > 1)
               {
                  // aggiorno la lista in memoria delle coppie <id>-<fattore di aggregazione>
                  IDAggrFactor_Tree.add(&(eed.gs_id));
                  ((C_BLONG_INT *) IDAggrFactor_Tree.get_cursor())->set_int(AggrFactor);

                  // modifico il fattore di aggregazione all'oggetto grafico 
                  // (che era stato messo a 1)
                  eed.num_el = AggrFactor;
                  eed.save(Objs.Ents.at(i));
               }
            }
            else
            {
               AggrFactor = pIDAggrFactor->get_int();

               // modifico il fattore di aggregazione all'oggetto grafico 
               // (che era stato messo a 1)
               eed.num_el = AggrFactor;
               eed.save(Objs.Ents.at(i));
            }
         } // Fattore di aggregazione - fine

         // lo aggiungo al db corrente di acad
         if (pInternalBlockTableRecord->appendAcDbEntity(OutputId, Objs.Ents.at(i)) != Acad::eOk)
            { Result = GS_BAD; break; }
         Objs.Ents.at(i)->close();
         acdbTransactionManagerPtr()->addNewlyCreatedDBRObject(Objs.Ents.at(i));

         Added++;
         if (CounterToVideo == GS_GOOD)
            StatusBarProgressMeter.Set((long) Added);

         // Aggiungo tabella OD con il codice dell'entità o il codice geometrico (se SPAGHETTI)
         IDStr = (pCls->get_type() == TYPE_SPAGHETTI) ? Id : eed.gs_id;
         ODRecord.Value(0) = IDStr.get_name(); 
         if (pODTable->AddRecord(ODRecord, OutputId) != AcMap::kOk)
            { gsc_printAdeErr(); GS_ERR_COD = eGSAdsCommandErr; Objs.Ents.at(i)->close(); Result = GS_BAD; break; }

         // ricavo l'handle
         gsc_enthand(OutputId, Handle);

         // ricavo ent e la aggiungo nel gruppo di selezione
         if (pSelSet || 
             (BitForFAS != GSNoneSetting && pFAS))
         {
            if (acdbGetAdsName(ent, OutputId) == Acad::eOk)
            {
               if (pSelSet) pSelSet->add(ent);
               if (BitForFAS != GSNoneSetting && pFAS) AlteredSS.add(ent);
            }
         }

         // aggiorno la lista in memoria delle coppie <handle>-<id> e 
         // <id>-<puntatore a <handle-id>>
         // Se get_key > 0 significa che è un oggetto grafico principale
         // altrimenti è un blocco DA
         AddToLinkTree(Handle, Id, (pKeyEnt->get_key() > 0) ? GRAPHICAL : DA_BLOCK);
      }
      else
         pKeyEnt->set_key(0); // lo segno da distruggere perchè inserito in db di autocad (= 0)

      pKeyEnt = (C_INT_LONG *) Objs.KeyEnts.get_next();
   }

   pInternalBlockTableRecord->close();

   // applico l'alterazione grafica
   gsc_modifyEntToFas(AlteredSS, pFAS, BitForFAS, FALSE);

   acTransactionManagerPtr()->flushGraphics();
   acTransactionManagerPtr()->endTransaction();

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(70), (long) Added); // "%ld oggetti grafici elaborati."
   else
   {
      C_STRING CompleteName;
      pCls->get_CompleteName(CompleteName);
      acutPrintf(gsc_msg(695), CompleteName.get_name()); // "\nElaborazione classe: <%s>\n"
      acutPrintf(gsc_msg(70), (long) Added); // "%ld oggetti grafici elaborati."
   }

//#if defined(GSDEBUG) // se versione per debugging
//   _ftime(&t2);
//   tempo = (t2.time + (double)(t2.millitm)/1000) - (t1.time + (double)(t1.millitm)/1000);
//   acutPrintf(_T("\nLa generazione grafica impiega %6.2f secondi.\n"), tempo);
//#endif

   return Result;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::Report                   <internal> */
/*+
  Questa funzione scrive su file delle informazioni relative 
  agli oggetti ricercati.
  Parametri:
  C_GPH_OBJ_ARRAY &Objs;  Oggetti risultanti dalla ricerca

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::Report(C_GPH_OBJ_ARRAY &Objs, C_STRING &Template,
                         C_STRING &Path, const TCHAR *Mode)
{
   ads_point p1, p2;
   C_STRING  Buffer, StrPt, ListSep;
   FILE      *file;

   if (gsc_getLocaleListSep(ListSep) == GS_BAD) return GS_BAD;
   if ((file = gsc_fopen(Path, Mode)) == NULL) return GS_BAD;

   // Ciclo gli oggetti in Objs
   for (int i = 0; i < Objs.Ents.length(); i++)
   {
      if (Template.comp(_T(".X1,.Y1,.Z1,.X2,.Y2,.Z2"), FALSE) == 0)
      {
         if (gsc_get_firstPoint(Objs.Ents.at(i), p1) != GS_GOOD ||
             gsc_get_lastPoint(Objs.Ents.at(i), p2) != GS_GOOD)
            continue;

         if (gsc_conv_Point(p1, StrPt) == GS_BAD) continue;
         Buffer = StrPt;
         Buffer += ListSep;
         if (gsc_conv_Point(p2, StrPt) == GS_BAD) continue;
         Buffer += StrPt;

         if (fwprintf(file, _T("%s\n"), Buffer.get_name()) < 0)
            { gsc_fclose(file); return GS_BAD; }
      }
      else
      if (Template.comp(_T(".LABELPT"), FALSE) == 0 ||
          Template.comp(_T(".CENTER"), FALSE) == 0)
      {
         if (gsc_get_firstPoint(Objs.Ents.at(i), p1) != GS_GOOD) continue;
         if (gsc_conv_Point(p1, StrPt) == GS_BAD) continue;

         if (fwprintf(file, _T("%s\n"), StrPt.get_name()) < 0)
            { gsc_fclose(file); return GS_BAD; }
      }
      else
      if (Template.comp(_T(".CENTROID"), FALSE) == 0)
      {
         if (gsc_get_centroidpoint(Objs.Ents.at(i), p1) != GS_GOOD) continue;
         if (gsc_conv_Point(p1, StrPt) == GS_BAD) continue;

         if (fwprintf(file, _T("%s\n"), StrPt.get_name()) < 0)
             { gsc_fclose(file); return GS_BAD; }
      }
   }
   
   return gsc_fclose(file);
}
int C_DBGPH_INFO::Preview(C_GPH_OBJ_ARRAY &Objs, long BitForFAS, C_FAS *pFAS)
   { return GS_CAN; } // non possibile


/****************************************************************************/
/*.doc C_DBGPH_INFO::GetNextKeyValue                             <internal> */
/*+
  Funzione di ausilio per il salvataggio. Verifica se il campo chiave 
  è aggiornabile e, in caso affermativo, calcola il prossimo valore da 
  usare per eventuali inserimenti.
  Parametri:
  C_STRING &_TableRef;  Riferimento completo della tabella da controllare
  long *NextKeyValue;   Valore chiave successivo all'ultimo. 
                        Assume valore 0 se il campo chiave è autoincrementato.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_DBGPH_INFO::GetNextKeyValue(C_STRING &_TableRef, long *NextKeyValue)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_STRING       AdjSyntaxFldName, Stm;
   _RecordsetPtr  pRs;
   bool           IsGeomKeyAutoIncrement;

   *NextKeyValue = 0;

   // Verifico se il campo chiave della geometria è aggiornabile (potrebbe essere 
   // auto-incrementato)
   AdjSyntaxFldName = key_attrib;
   gsc_AdjSyntax(AdjSyntaxFldName, pConn->get_InitQuotedIdentifier(),
                 pConn->get_FinalQuotedIdentifier(),
                 pConn->get_StrCaseFullTableRef());

   Stm = _T("SELECT ");
   Stm += AdjSyntaxFldName;
   Stm += _T(" FROM ");
   Stm += _TableRef;

   if (pConn->OpenRecSet(Stm, pRs) == GS_BAD) return GS_BAD;
   IsGeomKeyAutoIncrement = (gsc_isUdpateableField(pRs, key_attrib.get_name()) == GS_GOOD) ? false : true;
   gsc_DBCloseRs(pRs);

   if (IsGeomKeyAutoIncrement == false) // Se il campo non è autoincrementato
   {  // Calcolo il numero max del campo chiave
      int      result;
      ads_real dummy;

      if ((result = pConn->GetNumAggrValue(_TableRef.get_name(), key_attrib.get_name(),
                                           _T("MAX"), &dummy)) == GS_BAD)
         return GS_BAD;
      if (result == GS_CAN) *NextKeyValue = 1; // non ci sono righe in tabella
      else *NextKeyValue = (long) dummy + 1;
   }

   return GS_GOOD;
}


/****************************************************************************/
/*.doc C_DBGPH_INFO::Save                                        <external> */
/*+
  Salvataggio dati grafici nelle tabelle geometriche.
  Le entità salvate vengono rimosse da GEOsimAppl::SAVE_SS.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_DBGPH_INFO::Save(void)
{
   C_DBCONNECTION   *pConn = getDBConnection();
   ads_name         ent;
   C_EED            eed;
   C_SELSET         ClsSS, NewSS;
   int              OpMode, Result;
   TCHAR            Handle[MAX_LEN_HANDLE];
   TCHAR            Msg[MAX_LEN_MSG];
   long             i = 0, NewGeomKey;
   C_CLASS          *pCls = gsc_find_class(prj, cls, sub);
   C_STR_LIST       statement_list;
   C_STR            *pStm;
   C_BSTR_REAL      *pHandleId;
   C_STR_REAL_BTREE NewHandleIDList;
   C_STRING         DummyStr, ODTableName, CompleteName, Title;
   bool             IsGeomKeyAutoIncrement, AlreadyAsked = false;
   double           GeomKeyIntPart;
   AcMapODRecordIterator *pIter;
   AcMapODTable          *pTable; 
   bool             VerifyBlockDef = false, VerifyLineTypeDef = false, VerifyHatchDef = false;
   C_STR_LIST       GEOsimBlockNameList, NewBlockNameList, NewLineTypeNameList;
   C_2STR_LIST      GEOsimLineTypeNameList, GEOsimHatchNameList;
   C_2STR_LIST      NewHatchDescrList; // Nome riempimento e sua descrizione ASCII
   C_STRING         BlockName, LineTypeName;
   long             deleted = 0, updated = 0, inserted = 0;
   TCHAR            HatchName[MAX_LEN_HATCHNAME];

   if (!pCls) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
   if (LinkedTable) { GS_ERR_COD = eGSClassLocked; return GS_BAD; }
      
   // Calcolo il valore successivo del campo chiave
   if (GetNextKeyValue(TableRef, &NewGeomKey) == GS_BAD) return GS_BAD;
   IsGeomKeyAutoIncrement = (NewGeomKey == 0) ? true : false;

   // Creo il selection set filtrando da GEOsimAppl::SAVE_SS tutti gli oggetti della classe
   // considerando anche quelli cancellati
   if (GEOsimAppl::SAVE_SS.copyIntersectClsCode(ClsSS, cls, sub, true) == GS_BAD)
      return GS_BAD;

   // se la sessione ha ancora subito un congelamento 
   if (GS_CURRENT_WRK_SESSION->HasBeenFrozen() == GS_GOOD)
   {
      C_SELSET ss_from;

      // estraggo le entità dalle tabelle di provenienza (se esistono)
      if (gsc_ExtrOldGraphObjsFrom_SS(cls, sub, ClsSS, ss_from) == GS_BAD)
         return GS_BAD;

      ClsSS.add_selset(ss_from);
      if (ss_from.Erase() != GS_GOOD) return GS_BAD;
   }

   // Salvataggio delle etichette
   if (SaveDA(ClsSS) == GS_BAD) return GS_BAD;

   pCls->get_CompleteName(CompleteName);
   DummyStr = gsc_msg(658);  // "geometria"
   Title.set_name_formatted(gsc_msg(601), CompleteName.get_name(), DummyStr.get_name()); // "Salvataggio dati grafici classe %s (%s)"
   swprintf(Msg, MAX_LEN_MSG, _T("Saving geometries for class <%s>."), CompleteName.get_name());  
   gsc_write_log(Msg);

   // Inizializzazioni per OD table
   if ((pTable = gsc_getODTable(prj, cls, sub)) == NULL) return GS_BAD;
   if (gsc_GetObjectODRecordIterator(pIter) == GS_BAD) { delete pTable; return GS_BAD; }

   // Se la classe è spaghetti o nodo
   if (pCls->get_category() == CAT_SPAGHETTI || pCls->get_type() == TYPE_NODE)
   {
      VerifyBlockDef = true;
      // Leggo la lista dei blocchi di GEOsim
      gsc_getGEOsimBlockList(GEOsimBlockNameList);
   }

   // Se la classe è spaghetti o polilinea o superficie
   if (pCls->get_category() == CAT_SPAGHETTI || pCls->get_type() == TYPE_POLYLINE || pCls->get_type() == TYPE_SURFACE)
   {
      VerifyLineTypeDef = true;
      // Leggo la lista dei tipi linea di GEOsim
      gsc_getGEOsimLType(GEOsimLineTypeNameList);
   }
   
   // Se la classe è spaghetti o superficie
   if (pCls->get_category() == CAT_SPAGHETTI || pCls->get_type() == TYPE_SURFACE)
   {
      VerifyHatchDef = true;
      // Leggo la lista dei riempimenti di GEOsim
      gsc_getGEOsimHatch(GEOsimHatchNameList);
   }

   // Salvataggio degli oggetti grafici principali - inizio
   i         = 0;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(Title);
   StatusBarProgressMeter.Init(ClsSS.length());

   // Se non si tratta di superficie
   if (pCls->get_type() != TYPE_SURFACE)
   {
      while (ClsSS.entname(i++, ent) == GS_GOOD)
      {
         StatusBarProgressMeter.Set(i);

         statement_list.remove_all();

		   // se l'oggetto è stato cancellato
         if (gsc_IsErasedEnt(ent) == GS_GOOD)
         {
            if (gsc_UnEraseEnt(ent) != GS_GOOD) // lo ripristino
            {
               delete pTable; delete pIter;
               return GS_BAD;
            }

            if (gsc_is_DABlock(ent) == GS_GOOD) // se è blocco DA salto l'oggetto
               { gsc_EraseEnt(ent); continue; }

            // ricavo l'handle
            if (gsc_enthand(ent, Handle) == GS_BAD)
            {
               delete pTable; delete pIter; 
               gsc_EraseEnt(ent);
               return GS_BAD;
            }

            // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
            // è un oggetto estratto
            if ((pHandleId = (C_BSTR_REAL *) HandleId_LinkTree.search(Handle)))
            {
               // Lo cancello
               OpMode = ERASE;
               // Leggo la parte intera
               modf(pHandleId->get_key(), &GeomKeyIntPart);

               if (SQLFromEnt(ent, (long) GeomKeyIntPart, OpMode, statement_list) == GS_BAD)
               {
                  if (gsc_get_NotValidGraphObjMsg(ent, DummyStr) == GS_GOOD)
                  {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                     acutPrintf(GS_LFSTR);
                     acutPrintf(DummyStr.get_name());                   
                     gsc_write_log(DummyStr);
                  }

                  delete pTable; delete pIter; gsc_EraseEnt(ent);
                  return GS_BAD; 
               }
            }

            if (gsc_EraseEnt(ent) != GS_GOOD) // lo ricancello
            {
               delete pTable; delete pIter;
               return GS_BAD;
            }
         }
         else // oggetto esistente
         {
            if (gsc_is_DABlock(ent) == GS_GOOD) continue; // se è blocco DA salto l'oggetto

            // ricavo l'handle
            if (gsc_enthand(ent, Handle) == GS_BAD)
            {
               delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add(ent);
               return GS_BAD;
            }
            // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
            // è un oggetto estratto
            if ((pHandleId = (C_BSTR_REAL *) HandleId_LinkTree.search(Handle)))
            {  // Lo modifico
               OpMode = MODIFY;
               // Leggo la parte intera
               modf(pHandleId->get_key(), &GeomKeyIntPart);

               Result = SQLFromEnt(ent, (long) GeomKeyIntPart, OpMode, statement_list);
            }
            else // è un oggetto non ancora estratto
            {  // Lo inserisco
               OpMode = INSERT;

               Result = SQLFromEnt(ent, (IsGeomKeyAutoIncrement) ? 0 : NewGeomKey,
                                   OpMode, statement_list);
            }

            if (Result != GS_GOOD)
            {
               if (gsc_get_NotValidGraphObjMsg(ent, DummyStr) == GS_GOOD)
               {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                  acutPrintf(GS_LFSTR);
                  acutPrintf(DummyStr.get_name());                   
                  gsc_write_log(DummyStr);
               }

               GEOsimAppl::REFUSED_SS.add(ent);

               if (Result == GS_BAD)
               {
                  delete pTable; delete pIter; 
                  return GS_BAD;
               }

               if (Result == GS_CAN && !AlreadyAsked) // se non l'ho ancora chiesto
               {
                  AlreadyAsked = true;
                  // "Alcuni oggetti non saranno salvati, si vuole annullare il salvataggio ?"
                  if (gsc_ddgetconfirm(gsc_msg(768), &Result) == GS_GOOD && Result == GS_GOOD)
                  {
                     delete pTable; delete pIter;
                     return GS_BAD; // annullo il salvataggio
                  }
                  else 
                     continue; // continuo a salvare
               }
            }

            if (VerifyBlockDef)
               // Verifico se il blocco non è definito in GEOsim (scartando i blocchi $T e GEOSIM)
               if (gsc_get_blockName(ent, BlockName) == GS_GOOD && 
                   BlockName.comp(_T("$T")) != 0 && BlockName.comp(_T("GEOSIM")) != 0)
                  if (GEOsimBlockNameList.search_name(BlockName.get_name(), FALSE) == NULL)
                     NewBlockNameList.add_tail_unique(BlockName.get_name());

            if (VerifyLineTypeDef)
               // Verifico se il tipolinea non è definito in GEOsim (scartando i tipilinea "CONTINUOUS" e "DALAYER")
               if (gsc_get_lineType(ent, LineTypeName) == GS_GOOD && 
                   LineTypeName.comp(_T("CONTINUOUS"), FALSE) != 0 && LineTypeName.comp(_T("BYLAYER"), FALSE) != 0)
                  if (GEOsimLineTypeNameList.search_name(LineTypeName.get_name(), FALSE) == NULL)
                     NewLineTypeNameList.add_tail_unique(LineTypeName.get_name());

            if (VerifyHatchDef)
               // Verifico se il riempimento non è definito in GEOsim (scartando il riempineto "SOLID")
               if (gsc_getInfoHatch(ent, HatchName) == GS_GOOD && 
                   gsc_strcmp(HatchName, _T("SOLID"), FALSE) != 0)
                  if (GEOsimHatchNameList.search_name(HatchName, FALSE) == NULL)
                     if (NewHatchDescrList.search_name(HatchName) == NULL)
                     {
                        C_STRING ASCII;

                        if (gsc_get_ASCIIFromHatch(ent, ASCII) == GS_GOOD)
                           NewHatchDescrList.add_tail_2str(HatchName, ASCII.get_name());
                     }
         }

         // Eseguo le istruzioni SQL
         pStm = (C_STR *) statement_list.get_head();
         while (pStm)
         {
            if (gsc_ExeCmd(pConn->get_Connection(), pStm->get_name()) == GS_BAD)
            {
               delete pTable;
               delete pIter;
               GEOsimAppl::REFUSED_SS.add(ent);

               // Notifico in file log
               TCHAR Msg[MAX_LEN_MSG];
               swprintf(Msg, MAX_LEN_MSG, _T("SQL Failed: %s"), pStm->get_name());
               acutPrintf(GS_LFSTR);
               acutPrintf(Msg);                   
               gsc_write_log(Msg);

               return GS_BAD; 
            }
            pStm = (C_STR *) statement_list.get_next();
         }

         if (OpMode == INSERT && statement_list.get_head()) // se è stato inserito qualcosa
         {
            AcDbObjectId objId;
            AcDbObject   *pObj;

            // Memorizzo questa la coppia <handle>-<id> in una lista temporanea dei nuovi oggetti
            NewHandleIDList.add(Handle);
            ((C_BSTR_REAL *) NewHandleIDList.get_cursor())->set_key(NewGeomKey);

            // se si tratta di spaghetti
            if (pCls->get_type() == TYPE_SPAGHETTI)
            {
               // memorizzo identificatore OD nell'entità
               if (acdbGetObjectId(objId, ent) != Acad::eOk)
               { 
                  delete pIter; delete pTable; GS_ERR_COD = eGSInvGraphObjct; 
                  return GS_BAD;
               }
               if (acdbOpenObject(pObj, objId, AcDb::kForWrite, true) != Acad::eOk) 
               {
                  delete pIter; delete pTable; GS_ERR_COD = eGSInvGraphObjct;
                  return GS_BAD;
               }
               DummyStr = NewGeomKey;
               gsc_setID2ODTable(pObj, pTable, DummyStr, pIter);
               pObj->close();
            }

            if (!IsGeomKeyAutoIncrement) NewGeomKey++; // lo incremento

            inserted++;
         }
         else if (OpMode == MODIFY)
            updated++;
         else if (OpMode == ERASE)
            deleted++;
      }
   }
   else // Se si tratta di superficie
   {
      C_SELSET      entSS, OriginalEntSS;
      C_LONG_BTREE  KeyList;
      long          EntKey, nGphObject, OriginalEntKey;
      C_STRING      EntKeyFromGeomKeyStatement;
      _RecordsetPtr pRs;
      C_RB_LIST     ColValues;
      C_STR_LIST    partial_statement_list, HandleList;
      C_STR         *pHandle;
      C_LINK_SET    LinkSet;

      while (ClsSS.entname(i++, ent) == GS_GOOD)
      {
         StatusBarProgressMeter.Set(i);

		   // se l'oggetto è stato cancellato
         if (gsc_IsErasedEnt(ent) == GS_GOOD)
         {
            if (gsc_UnEraseEnt(ent) != GS_GOOD) // lo ripristino
            {
               delete pTable; delete pIter;
               return GS_BAD;
            }

            if (gsc_is_DABlock(ent) == GS_GOOD) // se è blocco DA salto l'oggetto
               { gsc_EraseEnt(ent); continue; }

            // ricavo l'handle
            if (gsc_enthand(ent, Handle) == GS_BAD)
            { 
               delete pTable; delete pIter; gsc_EraseEnt(ent);
               return GS_BAD;
            }

            // Cerco tutti gli oggetti principali esistenti dell'entità
            if (pCls->get_Key_SelSet(ent, &EntKey, entSS, GRAPHICAL) != GS_GOOD)
            { 
               delete pTable; delete pIter; gsc_EraseEnt(ent);
               return GS_BAD;
            }

            gsc_EraseEnt(ent); // la ricancello
         }
         else
         {
            if (gsc_is_DABlock(ent) == GS_GOOD) // se è blocco DA salto l'oggetto
               continue;

            // Cerco tutti gli oggetti principali esistenti dell'entità
            if (pCls->get_Key_SelSet(ent, &EntKey, entSS, GRAPHICAL) != GS_GOOD)
            {
               delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add(ent);
               return GS_BAD;
            }
         }

         // Se entità già trattata la salto
         if (KeyList.search(&EntKey)) continue;
         KeyList.add(&EntKey);

         // leggo la lista degli handle degli oggetti grafici (senza DA) che formano l'entità
         // in questo modo ho gli handle anche degli oggetti cancellati.
         if (LinkSet.GetHandleList(cls, sub, EntKey, HandleList, GRAPHICAL) != GS_GOOD)
         { 
            delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add_selset(entSS);
            return GS_BAD;
         }

         statement_list.remove_all();

         pHandleId = NULL;
         pHandle   = (C_STR *) HandleList.get_head();
         // Se in HandleList c'è almeno un oggetto che era stato estratto
         while (pHandle)
         {
            // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
            // è un oggetto estratto
            if ((pHandleId = (C_BSTR_REAL *) HandleId_LinkTree.search(pHandle->get_name()))) 
            {
               // Leggo la parte intera
               modf(pHandleId->get_key(), &GeomKeyIntPart);
               // Mi ricavo il codice dell'entità originale
               SQLGetEntKeyFromGeomKey((long) GeomKeyIntPart, EntKeyFromGeomKeyStatement);
               if (pConn->OpenRecSet(EntKeyFromGeomKeyStatement, pRs) == GS_BAD)
               {
                  delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add_selset(entSS);
                  return GS_BAD;
               }
               if (gsc_DBReadRow(pRs, ColValues) == GS_GOOD)
               {
                  gsc_DBCloseRs(pRs);
                  if (gsc_rb2Lng(ColValues.CdrAssoc(ent_key_attrib.get_name()), &OriginalEntKey) == GS_BAD)
                  { 
                     delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add_selset(entSS);
                     return GS_BAD;
                  }
                  // Se il codice entità originale non coincide con quello attuale (è stata fatta un'aggregazione)
                  if (EntKey != OriginalEntKey)
                     // Se entità non ancora trattata
                     if (KeyList.search(&OriginalEntKey) == NULL)
                     {
                        KeyList.add(&OriginalEntKey);

                        // aggiorno l'entità con codice originale
                        // Cerco tutti gli oggetti principali esistenti dell'entità
                        if (pCls->get_SelSet(OriginalEntKey, OriginalEntSS, GRAPHICAL) != GS_GOOD)
                        { 
                           delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add_selset(entSS);
                           return GS_BAD;
                        }
                        OriginalEntSS.is_presentGraphicalObject(&nGphObject);
                        // Se non ci sono dei contorni cancello l'entità altrimenti la modifico
                        OpMode = (nGphObject > 0) ? MODIFY : ERASE;
                        if (SQLFromPolygon(OriginalEntSS, (long) GeomKeyIntPart, OpMode,
                                           partial_statement_list) != GS_GOOD)
                        {
                           ads_name FirstEnt;
                           if (OriginalEntSS.entname(0, FirstEnt) == GS_GOOD)
                              if (gsc_get_NotValidGraphObjMsg(FirstEnt, DummyStr) == GS_GOOD)
                              {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                                 acutPrintf(GS_LFSTR);
                                 acutPrintf(DummyStr.get_name());                   
                                 gsc_write_log(DummyStr);
                              }

                           delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add_selset(entSS);
                           return GS_BAD; 
                        }

                        if (OpMode != ERASE)
                        {
                           if (VerifyLineTypeDef)
                              // Verifico se il tipolinea non è definito in GEOsim (scartando i tipilinea "CONTINUOUS" e "DALAYER")
                              if (gsc_get_lineType(OriginalEntSS, LineTypeName) == GS_GOOD && 
                                  LineTypeName.comp(_T("CONTINUOUS"), FALSE) != 0 && LineTypeName.comp(_T("BYLAYER"), FALSE) != 0)
                                 if (GEOsimLineTypeNameList.search_name(LineTypeName.get_name(), FALSE) == NULL)
                                    NewLineTypeNameList.add_tail_unique(LineTypeName.get_name());

                           if (VerifyHatchDef)
                              // Verifico se il riempimento non è definito in GEOsim (scartando il riempineto "SOLID")
                              if (gsc_getInfoHatchSS(OriginalEntSS, HatchName) == GS_GOOD && 
                                  gsc_strcmp(HatchName, _T("SOLID"), FALSE) != 0)
                                 if (GEOsimHatchNameList.search_name(HatchName, FALSE) == NULL)
                                    if (NewHatchDescrList.search_name(HatchName) == NULL)
                                    {
                                       C_STRING ASCII;

                                       if (gsc_get_ASCIIFromHatch(OriginalEntSS, ASCII) == GS_GOOD)
                                          NewHatchDescrList.add_tail_2str(HatchName, ASCII.get_name());
                                    }
                        }

                        statement_list.paste_tail(partial_statement_list);
                     }
               }
               else
                  gsc_DBCloseRs(pRs);
            }

            pHandle = (C_STR *) HandleList.get_next();
         }

         pHandle   = (C_STR *) HandleList.get_head();
         // Se in HandleList c'è almeno un oggetto che era stato estratto
         while (pHandle)
         {
            // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
            // è un oggetto estratto
            if ((pHandleId = (C_BSTR_REAL *) HandleId_LinkTree.search(pHandle->get_name()))) 
               break;
            pHandle = (C_STR *) HandleList.get_next();
         }

         entSS.is_presentGraphicalObject(&nGphObject);

         // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
         // è un oggetto estratto
         if (pHandleId)
         {
            // Se non ci sono dei contorni cancello l'entità altrimenti la modifico
            OpMode = (nGphObject > 0) ? MODIFY : ERASE;

            if (SQLFromPolygon(entSS, (long) GeomKeyIntPart, OpMode, partial_statement_list) != GS_GOOD)
            {
               ads_name FirstEnt;
               if (entSS.entname(0, FirstEnt) == GS_GOOD)
                  if (gsc_get_NotValidGraphObjMsg(FirstEnt, DummyStr) == GS_GOOD)
                  {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                     acutPrintf(GS_LFSTR);
                     acutPrintf(DummyStr.get_name());                   
                     gsc_write_log(DummyStr);
                  }

               delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add_selset(entSS);
               return GS_BAD;
            }
         }
         else // è un oggetto non ancora estratto
         if (nGphObject > 0)
         {  // Lo inserisco
            OpMode = INSERT;
            if (SQLFromPolygon(entSS, (IsGeomKeyAutoIncrement) ? 0 : NewGeomKey,
                               OpMode, partial_statement_list) != GS_GOOD) 
            {
               ads_name FirstEnt;
               if (entSS.entname(0, FirstEnt) == GS_GOOD)
                  if (gsc_get_NotValidGraphObjMsg(FirstEnt, DummyStr) == GS_GOOD)
                  {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                     acutPrintf(GS_LFSTR);
                     acutPrintf(DummyStr.get_name());                   
                     gsc_write_log(DummyStr);
                  }

               delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add_selset(entSS);
               return GS_BAD;
            }
         }
         else
            continue;

         if (OpMode != ERASE)
         {
            if (VerifyLineTypeDef)
               // Verifico se il tipolinea non è definito in GEOsim (scartando i tipilinea "CONTINUOUS" e "DALAYER")
               if (gsc_get_lineType(entSS, LineTypeName) == GS_GOOD && 
                   LineTypeName.comp(_T("CONTINUOUS"), FALSE) != 0 && LineTypeName.comp(_T("BYLAYER"), FALSE) != 0)
                  if (GEOsimLineTypeNameList.search_name(LineTypeName.get_name(), FALSE) == NULL)
                     NewLineTypeNameList.add_tail_unique(LineTypeName.get_name());

            if (VerifyHatchDef)
               // Verifico se il riempimento non è definito in GEOsim (scartando il riempineto "SOLID")
               if (gsc_getInfoHatchSS(entSS, HatchName) == GS_GOOD && 
                     gsc_strcmp(HatchName, _T("SOLID"), FALSE) != 0)
                  if (GEOsimHatchNameList.search_name(HatchName, FALSE) == NULL)
                     if (NewHatchDescrList.search_name(HatchName) == NULL)
                     {
                        C_STRING ASCII;

                        if (gsc_get_ASCIIFromHatch(OriginalEntSS, ASCII) == GS_GOOD)
                           NewHatchDescrList.add_tail_2str(HatchName, ASCII.get_name());
                     }
         }

         statement_list.paste_tail(partial_statement_list);

         // Eseguo le istruzioni SQL
         pStm = (C_STR *) statement_list.get_head();
         while (pStm)
         {
            if (gsc_ExeCmd(pConn->get_Connection(), pStm->get_name()) == GS_BAD)
            {
               delete pTable;
               delete pIter;
               GEOsimAppl::REFUSED_SS.add_selset(entSS);

               // Notifico in file log
               TCHAR Msg[MAX_LEN_MSG];
               swprintf(Msg, MAX_LEN_MSG, _T("SQL Failed: %s"), pStm->get_name());
               acutPrintf(GS_LFSTR);
               acutPrintf(Msg);                   
               gsc_write_log(Msg);

               return GS_BAD; 
            }

            pStm = (C_STR *) statement_list.get_next();
         }

         if (OpMode == INSERT)
         {
            for (int sub_geom = 1; sub_geom <= entSS.length(); sub_geom++)
            {
               entSS.entname(sub_geom - 1, ent);

               // ricavo l'handle
               if (gsc_enthand(ent, Handle) == GS_BAD)
               {
                  delete pIter; delete pTable; GS_ERR_COD = eGSInvGraphObjct;
                  return GS_BAD;
               }

               // Memorizzo questa la coppia <handle>-<id> in una lista temporanea dei nuovi oggetti
               NewHandleIDList.add(Handle);
               if (sub_geom > 1)
                  ((C_BSTR_REAL *) NewHandleIDList.get_cursor())->set_key(NewGeomKey + (sub_geom / 100.0));
               else
                  ((C_BSTR_REAL *) NewHandleIDList.get_cursor())->set_key(NewGeomKey);
            }

            if (!IsGeomKeyAutoIncrement) NewGeomKey++; // lo incremento

            inserted++;
         }
         else if (OpMode == MODIFY)
            updated++;
         else if (OpMode == ERASE)
            deleted++;
      }
   }
   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   gsc_geom_print_save_report(deleted, updated, inserted);

   acutPrintf(GS_LFSTR);

   delete pTable; 
   delete pIter;

   pHandleId = (C_BSTR_REAL *) NewHandleIDList.go_top();
   while (pHandleId)
   {
      // aggiorno la lista in memoria delle coppie <handle>-<id> e 
      // <id>-<puntatore a <handle-id>>
      AddToLinkTree(pHandleId->get_name(), pHandleId->get_key(), GRAPHICAL);
      pHandleId =  (C_BSTR_REAL *) NewHandleIDList.go_next();
   }

   // elimino gli oggetti salvati da GEOsimAppl::SAVE_SS
   GEOsimAppl::SAVE_SS.subtract(ClsSS);

   // Salvo le definizioni dei blocchi che non sono definiti in GEOsim
   // in USRTHM.DWG
   if (NewBlockNameList.get_count() > 0)
   {
      C_STRING DwgPath;

      DwgPath = GEOsimAppl::GEODIR;
      DwgPath += _T('\\');
      DwgPath += GEOTHMDIR;
      DwgPath += _T("\\USRTHM.DWG");

      gsc_save_blocks(NewBlockNameList, DwgPath, GS_GOOD);
   }

   // Salvo le definizioni dei tipilinea che non sono definiti in GEOsim
   if (NewLineTypeNameList.get_count() > 0)
   {
      C_STRING LineTypesPath;

      if (gsc_getCurrentGEOsimLTypeFilePath(LineTypesPath) == GS_GOOD)
         gsc_save_lineTypes(NewLineTypeNameList, LineTypesPath);
   }

   // Salvo le definizioni dei riempimenti che non sono definiti in GEOsim
   if (NewHatchDescrList.get_count() > 0)
   {
      C_STRING HatchesPath;

      // Salvo i riempienti sul file dei riempimenti sul server (poi lo copia sul client)
      if (gsc_getCurrentGEOsimHatchFilePath(HatchesPath, false) == GS_GOOD)
      {
         gsc_save_hatches(NewHatchDescrList, HatchesPath);
         // copio il file dei riempimenti anche sul client perchè serve ad AutoCAD
         gsc_refresh_thm();
      }
   }

   return GS_GOOD; 
}


/****************************************************************************/
/*.doc C_DBGPH_INFO::SaveDA                                      <internal> */
/*+
  Salvataggio dati grafici delle etichette nelle tabelle geometriche.
  Parametri:
  C_SELSET &ClsSS;   Gruppo degli oggetti della classe da salvare (principali e blocchi DA)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_DBGPH_INFO::SaveDA(C_SELSET &ClsSS)
{
   C_DBCONNECTION   *pConn = getDBConnection();
   ads_name         ent;
   int              result = GS_GOOD;
   TCHAR            Handle[MAX_LEN_HANDLE];
   TCHAR            Msg[MAX_LEN_MSG];
   long             i = 0, NewLblKey, NewDABlkKey;
   C_CLASS          *pCls = gsc_find_class(prj, cls, sub);
   C_STR_LIST       statement_list;
   C_STR            *pStm;
   C_BSTR_REAL      *pHandleId;
   C_STR_REAL_BTREE NewHandleIDList;
   C_STRING         DummyStr, AdjSyntaxFldName, CompleteName, Title;
   _RecordsetPtr    pRs;
   bool             IsDABlkKeyAutoIncrement, IsLblKeyAutoIncrement = false;
   C_EED            eed;
   AcMapODRecordIterator *pIter;
   AcMapODTable          *pTable; 
   long             deleted = 0, updated = 0, inserted = 0;

   if (!pCls) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
   if (LinkedLblGrpTable || LinkedLblTable) { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }

   // Per questo tipi non sono previste etichette
   if (pCls->get_category() == CAT_SPAGHETTI ||
       pCls->get_type() == TYPE_TEXT) return GS_GOOD;

   pCls->get_CompleteName(CompleteName);
   DummyStr = gsc_msg(608); // "etichette"
   Title.set_name_formatted(gsc_msg(601), CompleteName.get_name(), DummyStr.get_name()); // "Salvataggio dati grafici classe %s (%s)"
   swprintf(Msg, MAX_LEN_MSG, _T("Saving label for class <%s>."), CompleteName.get_name());  
   gsc_write_log(Msg);

   if (LblTableRef.len() == 0) return GS_GOOD;
   // Calcolo il valore successivo del campo chiave per le label
   if (GetNextKeyValue(LblTableRef, &NewLblKey) == GS_BAD) return GS_BAD;
   IsLblKeyAutoIncrement = (NewLblKey == 0) ? true : false;

   if (LblGroupingTableRef.len() > 0)
   {
      // Calcolo il valore successivo del campo chiave per i blocchi DA
      if (GetNextKeyValue(LblGroupingTableRef, &NewDABlkKey) == GS_BAD) return GS_BAD;
      IsDABlkKeyAutoIncrement = (NewDABlkKey == 0) ? true : false;
   }

   // Inizializzazioni per OD table
   if ((pTable = gsc_getODTable(prj, cls, sub)) == NULL) return GS_BAD;
   if (gsc_GetObjectODRecordIterator(pIter) == GS_BAD) { delete pTable; return GS_BAD; }

   i         = 0;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(Title);
   StatusBarProgressMeter.Init(ClsSS.length());

   while (ClsSS.entname(i++, ent) == GS_GOOD)
   {
      StatusBarProgressMeter.Set(i);

      statement_list.remove_all();

	   // se l'oggetto è stato cancellato
      if (gsc_IsErasedEnt(ent) == GS_GOOD)
      {
         if (gsc_UnEraseEnt(ent) != GS_GOOD) // lo ripristino
            { delete pTable; delete pIter; return GS_BAD; }

         if (gsc_is_DABlock(ent) != GS_GOOD) // se non è un blocco DA salto l'oggetto
            { gsc_EraseEnt(ent); continue; }

         // ricavo l'handle
         if (gsc_enthand(ent, Handle) == GS_BAD)
            { delete pTable; delete pIter; gsc_EraseEnt(ent); return GS_BAD; }

         // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
         // è un blocco DA estratto
         if ((pHandleId = (C_BSTR_REAL *) HandleId_LinkTree.search(Handle)))
            // Lo cancello
            if (SQLFromEnt(ent, (long) pHandleId->get_key(), ERASE, statement_list) == GS_BAD)
            {
               if (gsc_get_NotValidGraphObjMsg(ent, DummyStr) == GS_GOOD)
               {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                  acutPrintf(GS_LFSTR);
                  acutPrintf(DummyStr.get_name());                   
                  gsc_write_log(DummyStr);
               }
               delete pTable; delete pIter; gsc_EraseEnt(ent); return GS_BAD;
            }
            else
               deleted++;

         if (gsc_EraseEnt(ent) != GS_GOOD) 
            { delete pTable; delete pIter; return GS_BAD; } // lo ricancello
      }
      else // blocco DA esistente
      {
         if (gsc_is_DABlock(ent) != GS_GOOD) continue; // se non è un blocco DA salto l'oggetto

         // ricavo l'handle
         if (gsc_enthand(ent, Handle) == GS_BAD) 
            { delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add(ent); return GS_BAD; }
         // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
         // è un blocco DA estratto
         if ((pHandleId = (C_BSTR_REAL *) HandleId_LinkTree.search(Handle)))
         {  // Lo modifico
            if (SQLFromEnt(ent, (long) pHandleId->get_key(), MODIFY, statement_list) == GS_BAD)
            {
               if (gsc_get_NotValidGraphObjMsg(ent, DummyStr) == GS_GOOD)
               {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                  acutPrintf(GS_LFSTR);
                  acutPrintf(DummyStr.get_name());                   
                  gsc_write_log(DummyStr);
               }
               delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add(ent); return GS_BAD;
            }
            else
               updated++;
         }
         else // è un blocco DA non ancora estratto
         {  // Lo inserisco
            AcDbBlockReference *pObj;
            AcDbObjectId       objId;
            AcDbAttribute      *pAttrib;
            AcDbObjectIterator *pAttrIter;
            long               ParentId;

            if (acdbGetObjectId(objId, ent) != Acad::eOk)
               { delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add(ent); return GS_BAD; }
            if (acdbOpenObject(pObj, objId, AcDb::kForWrite, true) != Acad::eOk)
               { delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add(ent); return GS_BAD; }
            // Se le etichette sono sotto forma di blocchi con attributi in 2 tabelle distinte
            if (LblGroupingTableRef.len() > 0)
            {
               if (SQLFromBlock(pObj, (IsDABlkKeyAutoIncrement) ? 0 : NewDABlkKey, 
                                INSERT, DummyStr, true) == GS_BAD)
               {
                  if (gsc_get_NotValidGraphObjMsg(ent, DummyStr) == GS_GOOD)
                  {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                     acutPrintf(GS_LFSTR);
                     acutPrintf(DummyStr.get_name());                   
                     gsc_write_log(DummyStr);
                  }
                  delete pTable; delete pIter; pObj->close(); GEOsimAppl::REFUSED_SS.add(ent); return GS_BAD;
               }
               if (IsDABlkKeyAutoIncrement) // se campo chiave autoincrementato
               { // aggiungo all'istruzione SQL "RETURNING ..."
               }
               if (gsc_ExeCmd(pConn->get_Connection(), DummyStr.get_name()) == GS_BAD)
                  { delete pTable; delete pIter; pObj->close(); GEOsimAppl::REFUSED_SS.add(ent); return GS_BAD; }
               if (IsDABlkKeyAutoIncrement) // se campo chiave autoincrementato
               {
                  //NewDABlkKey = ...
               }

               // Memorizzo questa la coppia <handle>-<id> in una lista temporanea dei nuovi oggetti
               NewHandleIDList.add(Handle);
               ((C_BSTR_REAL *) NewHandleIDList.get_cursor())->set_key(NewDABlkKey);

               ParentId = NewDABlkKey;
            }
            else
            {
               eed.load(pObj); // leggo codice entità e fattore di aggregazione
               ParentId = eed.gs_id;
            }

            // Inserisco gli attributi del blocco DA
            pAttrIter = pObj->attributeIterator();
            for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
            {
               objId = pAttrIter->objectId();

               if (pObj->openAttribute(pAttrib, objId, AcDb::kForRead) != Acad::eOk)
               { 
                  delete pTable; delete pIter;
                  pObj->close(); 
                  GS_ERR_COD = eGSInvEntityOp; 
                  GEOsimAppl::REFUSED_SS.add(ent); 
                  return GS_BAD; 
               }

               if (SQLFromAttrib(pAttrib, (IsLblKeyAutoIncrement) ? 0 : NewLblKey, 
                                 INSERT, ParentId, eed.num_el, DummyStr) == GS_BAD)
               {
                  if (gsc_get_NotValidGraphObjMsg(ent, DummyStr) == GS_GOOD)
                  {  // scrivo coordinate oggetto grafico a video e nel log (se abilitato)
                     acutPrintf(GS_LFSTR);
                     acutPrintf(DummyStr.get_name());                   
                     gsc_write_log(DummyStr);
                  }

                  delete pTable; delete pIter; 
                  pAttrib->close(); 
                  pObj->close(); 
                  GEOsimAppl::REFUSED_SS.add(ent); 
                  return GS_BAD; 
               }
               pAttrib->close();

               if (LblGroupingTableRef.len() == 0) // Se gli attributi non sono raggruppati
                  if (IsLblKeyAutoIncrement) // se campo chiave autoincrementato
                  { // aggiungo all'istruzione SQL "RETURNING ..."
                  }
               if (gsc_ExeCmd(pConn->get_Connection(), DummyStr.get_name()) == GS_BAD)
                  { delete pTable; delete pIter; pObj->close(); GEOsimAppl::REFUSED_SS.add(ent); return GS_BAD; }
               if (LblGroupingTableRef.len() == 0) // Se gli attributi non sono raggruppati
               {
                  // Memorizzo questa la coppia <handle>-<id> in una lista temporanea dei nuovi oggetti
                  NewHandleIDList.add(Handle);
                  ((C_BSTR_REAL *) NewHandleIDList.get_cursor())->set_key(NewLblKey);
               }

               if (!IsLblKeyAutoIncrement) // se campo chiave non è autoincrementato
                  NewLblKey++; // lo incremento
            }
            pObj->close();

            if (LblGroupingTableRef.len() > 0)
               if (!IsDABlkKeyAutoIncrement) // se campo chiave non è autoincrementato
                  NewDABlkKey++; // lo incremento

            inserted++;
         }
      }

      // Eseguo le istruzioni SQL (usato solo in caso di cancellazione o modifica)
      pStm = (C_STR *) statement_list.get_head();
      while (pStm)
      {
         if (gsc_ExeCmd(pConn->get_Connection(), pStm->get_name()) == GS_BAD)
            { delete pTable; delete pIter; GEOsimAppl::REFUSED_SS.add(ent); return GS_BAD; }
         pStm = (C_STR *) statement_list.get_next();
      }
   }

   delete pTable; 
   delete pIter;
   StatusBarProgressMeter.End();

   gsc_label_print_save_report(deleted, updated, inserted);

   pHandleId =  (C_BSTR_REAL *) NewHandleIDList.go_top();
   while (pHandleId)
   {
      // aggiorno la lista in memoria delle coppie <handle>-<id> e 
      // <id>-<puntatore a <handle-id>>
      AddToLinkTree(pHandleId->get_name(), pHandleId->get_key(), DA_BLOCK);
      pHandleId =  (C_BSTR_REAL *) NewHandleIDList.go_next();
   }
   
   return GS_GOOD; 
}


/*********************************************************/
/*.doc long C_DBGPH_INFO::editnew             <external> /*
/*+
  Ottiene un gruppo di selezione degli oggetti nuovi della classe
  leggendolo dal gruppo di selezione degli oggetti da salvare.
  Parametri:
  ads_name sel_set;  output, Gruppo di oggetti nuovi

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_DBGPH_INFO::editnew(ads_name sel_set)
{
   TCHAR    Handle[MAX_LEN_HANDLE];
   ads_name ent;
   long     i = 0;
   C_SELSET InternalSS;
   C_EED    eed;

   while (GEOsimAppl::SAVE_SS.entname(i++, ent) == GS_GOOD)
      // Scarto oggetti non appartenenti alla classe
      if (eed.load(ent) == GS_GOOD && eed.cls == cls && eed.sub == sub)
      {
         // ricavo l'handle
         if (gsc_enthand(ent, Handle) == GS_BAD) return GS_BAD;
         // se NON esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
         if (HandleId_LinkTree.search(Handle) == NULL)
            // significa che è nuovo
            if (InternalSS.add(ent) == GS_BAD) return GS_BAD;
      }

   InternalSS.get_selection(sel_set);
   InternalSS.ReleaseAllAtDistruction(GS_BAD);

   return GS_GOOD; 
}


/*********************************************************/
/*.doc bool C_DBGPH_INFO::HasCompatibleGeom   <external> /*
/*+
  Questa funzione verifica che la geometria di un oggetto grafico
  sia compatibile al tipo di risorsa grafica usata.
  Parametri:
  AcDbObject *pObj;        oggetto grafico
  bool TryToExplode;       Opzionale; flag di esplosione. Nel caso l'oggetto
                           grafico non sia compatibile vine esploso per ridurlo
                           ad oggetti semplici (default = false)
  AcDbVoidPtrArray *pExplodedSet;  Opzionale; Puntatore a risultato 
                                   dell'esplosione (default = NULL)

  Restituisce true se l'oggetto ha una geometria compatibile altrimenti false.
-*/  
/*********************************************************/
bool C_DBGPH_INFO::HasCompatibleGeom(AcDbObject *pObj, bool TryToExplode, 
                                     AcDbVoidPtrArray *pExplodedSet)
{
   if (pExplodedSet) pExplodedSet->removeAll();

   if (pObj->isKindOf(AcDbBlockReference::desc()))  // è un blocco
   {
      C_STRING BlockName;
      // non sono ammessi i blocchi con attributi
      BlockName.set_name(gsc_getSymbolName(((AcDbBlockReference *) pObj)->blockTableRecord()));
      return (gsc_isBlockWithAttributes(BlockName.get_name()) == GS_GOOD) ? false : true;
   }
   else
   if ((gsc_isLinearEntity((AcDbEntity *) pObj) && !pObj->isKindOf(AcDbLeader::desc())) || // è un oggetto lineare (ma non leader) oppure
       pObj->isKindOf(AcDbCircle::desc())         ||  // è un cerchio oppure
       gsc_isText((AcDbEntity *) pObj)            ||  // è un testo oppure
       pObj->isKindOf(AcDbPoint::desc())          ||  // è un punto oppure
       pObj->isKindOf(AcDbSolid::desc())          ||  // è un solido oppure
       pObj->isKindOf(AcDbFace::desc())           ||  // è un 3dface oppure
       pObj->isKindOf(AcDbEllipse::desc()))           // è un ellisse
      return true; 
   else
   if (pObj->isKindOf(AcDbHatch::desc()) || // è un riempimento
         pObj->isKindOf(AcDbMPolygon::desc())) // è un poligono
   {
      // Se geom_attrib è vuoto (la geometria è sotto forma di 2/3 campi numerici)
      if (geom_attrib.len() == 0)
         return false; // le superfici sono gestibli solo da un campo di tipo geometria
      else
         return true;
   }
   else // tipologia non gestita
   if (TryToExplode && pExplodedSet)
   {
      AcDbVoidPtrArray InternalSet, InternalSet1;

      // esplodo per semplificare l'oggetto grafico
      if (gsc_explode((AcDbEntity *) pObj, false, NULL, &InternalSet) == GS_BAD) return false;
      // nel caso siano stati inseriti dei nuovi nuovi oggetti grafici
      for (int i = 0; i < InternalSet.length(); i++)
      {
         if (HasCompatibleGeom((AcDbObject *) InternalSet[i], TryToExplode, &InternalSet1) == false)
            return false;
         // Se non si è dovuto esplodere
         if (InternalSet1.length() == 0) pExplodedSet->append(InternalSet[i]);
         else
            for (int j = 0; j < InternalSet1.length(); j++)
               pExplodedSet->append(InternalSet1[j]);
      }
      return true;
   }
   else
      return false;
}


/*********************************************************/
/*.doc bool C_DBGPH_INFO::HasValidGeom        <external> /*
/*+
  Questa funzione verifica la validità della geometria di un oggetto grafico.
  Parametri:
  AcDbObject *pObj;        Oggetto grafico
  C_STRING &WhyNotValid;   Motivo per cui l'oggetto non è ritenuto valido

  Restituisce true se l'oggetto ha una geometria troppo complessa altrimenti false.
-*/  
/*********************************************************/
bool C_DBGPH_INFO::HasValidGeom(AcDbEntity *pObj, C_STRING &WhyNotValid)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   C_STRING       WKT, statement, SrcSRID;

   WhyNotValid.clear();
   if (pCls->get_category() == CAT_SPAGHETTI)
   {
      if (gsc_isLinearEntity(pObj)) // è un oggetto lineare
      {
         if (WKTFromPolyline(pObj, WKT) == GS_BAD)
         {
            gsc_get_NotValidGraphObjMsg(pObj, WhyNotValid);
            return false;
         }
      }
      else
      if (gsc_isPunctualEntity(pObj)) // è un oggetto puntuale
         return true;
      else // non è un oggetto puntuale quindi è un poligono
         if (WKTFromPolygon(pObj, WKT) == GS_BAD)
         {
            gsc_get_NotValidGraphObjMsg(pObj, WhyNotValid);
            return false;
         }
   }
   else
      switch (pCls->get_type())
      {
         case TYPE_POLYLINE:
            if (!gsc_isLinearEntity(pObj)) // non è un oggetto lineare
            {
               gsc_get_NotValidGraphObjMsg(pObj, WhyNotValid);
               return false;
            }
            if (WKTFromPolyline(pObj, WKT) == GS_BAD)
            {
               gsc_get_NotValidGraphObjMsg(pObj, WhyNotValid);
               return false;
            }
            break;
         case TYPE_SURFACE:
            if (WKTFromPolygon(pObj, WKT) == GS_BAD) // non è un poligono
            {
               gsc_get_NotValidGraphObjMsg(pObj, WhyNotValid);
               return false;
            }
            break;
         case TYPE_TEXT:
         case TYPE_NODE:
            return true;
      }

   if (get_isCoordToConvert() == GS_GOOD)
      SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

   // Se posso verificare la validità tramite SQL
   if (pConn->get_IsValidWKTGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, statement) == GS_GOOD)
   {
      C_RB_LIST      ColValues;
      presbuf        p;
      bool           Result;

      if (pConn->ReadRows(statement, ColValues) == GS_BAD) return false;
      if (!(p = gsc_nth(1, gsc_nth(0, ColValues.nth(0))))) return false;
      if (gsc_rb2Bool(p, &Result) == GS_BAD) return false;
      
      if (!Result)
      {
         // Se posso verificare la ragione della NON validità tramite SQL
         if (pConn->get_IsValidReasonWKTGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, statement) == GS_GOOD)
         {
            if (pConn->ReadRows(statement, ColValues) == GS_BAD) return false;
            if (!(p = gsc_nth(1, gsc_nth(0, ColValues.nth(0))))) return false;
            WhyNotValid = p;
         }
         else
            gsc_get_NotValidGraphObjMsg(pObj, WhyNotValid);

         return false;
      }
   }

   return true;
}
bool C_DBGPH_INFO::HasValidGeom(AcDbEntityPtrArray &EntArray, C_STRING &WhyNotValid)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);

   WhyNotValid.clear();
   if (pCls->get_type() != TYPE_SURFACE)
   {
      for (int i = 0; i < EntArray.length(); i++)
         if (!HasValidGeom(EntArray.at(i), WhyNotValid)) return false;
      return true;
   }

   C_STRING       WKT, statement, SrcSRID;
   GSSRIDUnitEnum SrcUnit;
   AcDbMPolygon   *pMPolygon;
   int            Result;
      
   if ((pMPolygon = gsc_create_MPolygon(EntArray)) == NULL) // non è un oggetto poligono
   {
      gsc_get_NotValidGraphObjMsg(EntArray.at(0), WhyNotValid);
      return false;
   }

   Result = WKTFromMPolygon(pMPolygon, WKT);
   delete pMPolygon;
   if (Result == GS_BAD)
   {
      gsc_get_NotValidGraphObjMsg(EntArray.at(0), WhyNotValid);
      return false;
   }

   if (GS_CURRENT_WRK_SESSION)
      // Se è stato dichiarato un sistema di coordinate x la sessione corrente
      // e per la classe
      if (gsc_strlen(GS_CURRENT_WRK_SESSION->get_coordinate()) > 0 &&
          coordinate_system.len() > 0)
      {
         GSSRIDTypeEnum ClsSRIDType;

         if (pConn->get_SRIDType(ClsSRIDType) == GS_GOOD)
            if (gsc_get_srid(GS_CURRENT_WRK_SESSION->get_coordinate(), GSSRID_Autodesk,
                             ClsSRIDType, SrcSRID, &SrcUnit) == GS_GOOD)
            // Se i sistemi di coordinate coincidono, la conversione non è necessaria
            if (SrcSRID.comp(coordinate_system, FALSE) == 0) SrcSRID.clear();
      }

   // Se posso verificare la validità tramite SQL
   if (pConn->get_IsValidWKTGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, statement) == GS_GOOD)
   {
      C_RB_LIST      ColValues;
      presbuf        p;
      bool           Result;

      if (pConn->ReadRows(statement, ColValues) == GS_BAD) return false;
      if (!(p = gsc_nth(1, gsc_nth(0, ColValues.nth(0))))) return false;
      if (gsc_rb2Bool(p, &Result) == GS_BAD) return false;
      
      if (!Result)
      {
         // Se posso verificare la ragione della NON validità tramite SQL
         if (pConn->get_IsValidReasonWKTGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, statement) == GS_GOOD)
         {
            if (pConn->ReadRows(statement, ColValues) == GS_BAD) return false;
            if (!(p = gsc_nth(1, gsc_nth(0, ColValues.nth(0))))) return false;
            WhyNotValid = p;
         }
         else
            gsc_get_NotValidGraphObjMsg(EntArray.at(0), WhyNotValid);

         return false;
      }
   }

   return true;
}


/******************************************************************************/
/*.doc C_DBGPH_INFO::Check                                        <external>  */
/*+
  Funzione per creazione indici dei dati geometrici.
  Parametri:
  int WhatOperation;    Tipo di controllo da eseguire:
                        reindicizzazione (REINDEX)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DBGPH_INFO::Check(int WhatOperation)
{
   if (WhatOperation & REINDEX)
   {
      if (!LinkedTable) // Se si tratta di una tabella collegata non posso reindicizzare
         if (ReindexTable(TableRef) == GS_BAD) return GS_BAD;
      
      if (LblGroupingTableRef.len() > 0)
         if (!LinkedLblGrpTable) // Se si tratta di una tabella collegata non posso reindicizzare
            if (ReindexTable(LblGroupingTableRef) == GS_BAD) return GS_BAD;
      
      if (LblTableRef.len() > 0)
         if (!LinkedLblTable) // Se si tratta di una tabella collegata non posso reindicizzare
            if (ReindexTable(LblTableRef) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_DBGPH_INFO::Detach                                        <external>  */
/*+
  Funzione per il rilascio della sorgente dati geometrica.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DBGPH_INFO::Detach(void)
{
   HandleId_LinkTree.remove_all();
   IndexByID_LinkTree.remove_all();
   DAIndexByID_LinkTree.remove_all();
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::TerminateSQL             <external> */
/*+
  Questa funzione corregge la tabella nel caso sia una funzione
  con parametri come nel caso della tabella storica in cui il parametro è
  una data ("select * from myfunction(NULL::timestamp)"). Il parametro è
  espresso fra parentesi tonde ed è seguito da :: e dal tipo SQL
  (es. timestamp, numeric, varchar...)
  Parametri:
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::AdjTableParams(void)
{
   // se non si tratta di una tabella esterna
   if (!LinkedTable) return GS_GOOD;
   // Se la tabella geometrica è una funzione con parametri
   if (getDBConnection()->AdjTableParams(TableRef) != GS_GOOD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::TerminateSQL             <external> */
/*+
  Questa funzione annulla la connessione OLE-DB da terminare.
  Parametri:
  C_DBCONNECTION *pConnToTerminate; Opzionale; connessione da terminare,
                                    se non passata termina tutte le connessioni
                                    (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_DBGPH_INFO::TerminateSQL(C_DBCONNECTION *pConnToTerminate)
{
   bool Terminate = true;

   if (pConnToTerminate && pConnToTerminate != pGeomConn) Terminate = false;
   if (Terminate) pGeomConn = NULL;
}


/******************************************************************************/
/*.doc C_DBGPH_INFO::ReindexTable                                 <internal>  */
/*+
  Funzione di ausilio per creazione indici dei dati geometrici.
  Parametri:
  C_STRING &MyTableRef; Riferimento completo tabella

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DBGPH_INFO::ReindexTable(C_STRING &MyTableRef)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_RB_LIST      Stru;
   C_STRING       Catalog, Schema, Name, IndexRef;

   // Leggo struttura tabella per verificare l'esistenza dei campi
   if ((Stru = pConn->ReadStruct(TableRef.get_name())) == NULL)
      return GS_BAD;

   if (pConn->split_FullRefTable(MyTableRef, Catalog, Schema, Name) == GS_BAD)
      return GS_BAD;

   // key_attrib (nome del campo chiave dell'oggetto grafico)
   if (Stru.Assoc(key_attrib.get_name(), FALSE))
   {
      C_STRING PrimaryKey;

      // Nome della chiave primaria
      get_PrimaryKeyName(MyTableRef, PrimaryKey);

      // Cancello chiave primaria
      if (pConn->DelPrimaryKey(MyTableRef.get_name(), PrimaryKey.get_name(), ONETEST, GS_BAD) == GS_BAD)
      { // La chiave primaria non c'era. Al suo posto c'era un indice normale.
         C_STRING Catalog, Schema, Name, IndexRef;

         // cancello l'indice per codice chiave
         if (pConn->split_FullRefTable(MyTableRef.get_name(), Catalog, Schema, Name) == GS_BAD)
            return GS_BAD;

         // Ottengo il nome completo dell'indice per la cancellazione
         if (IndexRef.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                      Schema.get_name(),
                                                      Name.get_name(),
                                                      key_attrib.get_name(),
                                                      ERASE)) == NULL)
            return GS_BAD;
         // cancello indice
         pConn->DelIndex(MyTableRef, IndexRef);
      }

      // Creo chiave primaria
      if (pConn->CreatePrimaryKey(MyTableRef.get_name(), PrimaryKey.get_name(), 
                                  key_attrib.get_name()) == GS_BAD)
         // Se il provider non supporta la creazione di indice (vedi Excel) vado avanti comunque
         if (GS_ERR_COD != eGSExeNotSupported)
            return GS_BAD;
   }

   // Gli spaghetti hanno key_attrib = ent_key_attrib
   // ent_key_attrib (nome del campo chiave dell'entità di appartenenza dell'oggetto grafico)
   if (key_attrib.comp(ent_key_attrib) != 0 && Stru.Assoc(ent_key_attrib.get_name(), FALSE))
   {
      // Ottengo il nome completo dell'indice per la cancellazione
      if (IndexRef.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                   Schema.get_name(),
                                                   Name.get_name(),
                                                   ent_key_attrib.get_name(),
                                                   ERASE)) == NULL)
         return GS_BAD;
      // cancello indice
      pConn->DelIndex(MyTableRef, IndexRef);

      // Ottengo il nome completo dell'indice per la creazione
      if (IndexRef.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                   Schema.get_name(),
                                                   Name.get_name(),
                                                   ent_key_attrib.get_name())) == NULL)
         return GS_BAD;
      // creo indice per campo chiave dell'entità di appartenenza dell'oggetto grafico
      if (pConn->CreateIndex(IndexRef.get_name(), MyTableRef.get_name(), ent_key_attrib.get_name(), INDEX_NOUNIQUE) == GS_BAD)
         // Se il provider non supporta la creazione di indice (vedi Excel) vado avanti comunque
         if (GS_ERR_COD != eGSExeNotSupported)
            return GS_BAD;
   }

   // geom_attrib (nome del campo geometrico dell'oggetto grafico)
   if (Stru.Assoc(geom_attrib.get_name(), FALSE))
   {
      // Ottengo il nome completo dell'indice per la cancellazione
      if (IndexRef.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                   Schema.get_name(),
                                                   Name.get_name(),
                                                   geom_attrib.get_name(),
                                                   ERASE)) == NULL)
         return GS_BAD;
      // cancello indice
      pConn->DelIndex(MyTableRef, IndexRef);

      // Ottengo il nome completo dell'indice per la creazione
      if (IndexRef.paste(pConn->getNewFullRefIndex(Catalog.get_name(),
                                                   Schema.get_name(),
                                                   Name.get_name(),
                                                   geom_attrib.get_name())) == NULL)
         return GS_BAD;
      // creo indice per campo chiave dell'entità di appartenenza dell'oggetto grafico
      if (pConn->CreateIndex(IndexRef.get_name(), MyTableRef.get_name(), geom_attrib.get_name(), SPATIAL_INDEX_NOUNIQUE) == GS_BAD)
         // Se il provider non supporta la creazione di indice (vedi Excel) vado avanti comunque
         if (GS_ERR_COD != eGSExeNotSupported)
            return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::get_PrimaryKeyName       <external> */
/*+
  Questa funzione restituisce il nome della chiave primaria 
  della tabella.
  Parametri:
  C_STRING &_TableRef;  Riferimento completo della tabella
  C_STRING &PriKeyName; Nome chiave primaria
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::get_PrimaryKeyName(C_STRING &_TableRef, C_STRING &PriKeyName)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_STRING       Catalog, Schema, Name;

   // Ricavo il nome della tabella
   if (pConn->split_FullRefTable(_TableRef, Catalog, Schema, Name) == GS_BAD)
      return GS_BAD;
   // Nome della chiave primaria
   PriKeyName = Name;
   PriKeyName += key_attrib;

   return GS_GOOD;
}


int C_DBGPH_INFO::reportHTML(FILE *file, bool SynthMode)
{
   if (SynthMode) return GS_GOOD;

   if (C_GPH_INFO::reportHTML(file, SynthMode) == GS_BAD) return GS_BAD;

   C_STRING Buffer;
   C_STRING TitleBorderColor("#808080"), TitleBgColor("#c0c0c0");
   C_STRING BorderColor("#00CCCC"), BgColor("#99FFFF");

   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" bgcolor=\"%s\" width=\"100%%\" border=\"1\">"),
                TitleBorderColor.get_name(), TitleBgColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Collegamento a Grafica"
   if (fwprintf(file, _T("\n<tr><td align=\"center\"><b><font size=\"3\">%s</font></b></td></tr></table><br>"),
                gsc_msg(766)) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // intestazione tabella
   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" cellspacing=\"2\" cellpadding=\"2\" border=\"1\">"), 
                BorderColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Riferimento tabella di geometria"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\" width=\"30%%\"><b>%s:</b></td><td width=\"70%%\">%s</td></tr>"),
                BgColor.get_name(), gsc_msg(106),
                (TableRef.len() == 0) ? _T("&nbsp;") : TableRef.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Riferimento tabella di raggruppamento etichette"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\" width=\"30%%\"><b>%s:</b></td><td width=\"70%%\">%s</td></tr>"),
                BgColor.get_name(), gsc_msg(216),
                (LblGroupingTableRef.len() == 0) ? _T("&nbsp;") : LblGroupingTableRef.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Riferimento tabella di geometria etichette"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\" width=\"30%%\"><b>%s:</b></td><td width=\"70%%\">%s</td></tr>"),
                BgColor.get_name(), gsc_msg(209),
                (LblTableRef.len() == 0) ? _T("&nbsp;") : LblTableRef.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Connessione UDL"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(728),
                (ConnStrUDLFile.len() == 0) ? _T("&nbsp;") : ConnStrUDLFile.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Proprietà UDL"
   Buffer.paste(gsc_PropListToConnStr(UDLProperties));
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(790),
                (Buffer.len() == 0) ? _T("&nbsp;") : Buffer.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Unità di misura delle rotazioni"
   Buffer.paste(gsc_getRotMeasureUnitsDescription(rotation_unit));
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(258),
                (Buffer.len() == 0) ? _T("&nbsp;") : Buffer.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Formato del colore"
   Buffer.paste(gsc_getColorFormatDescription(color_format));
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(258),
                (Buffer.len() == 0) ? _T("&nbsp;") : Buffer.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // fine tabella
   if (fwprintf(file, _T("\n</table><br><br>")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_DBGPH_INFO::BackupTable                                  <internal>  */
/*+
  Questa funzione di ausilio per creazione backup dei dati geometrici.
  Parametri:
  C_STRING &MyTableRef; Riferimento completo tabella
  BackUpModeEnum Mode;  Flag di modalità: Creazione , restore o cancellazione
                        del backup esistente (default = GSCreateBackUp).
  int MsgToVideo; flag, se = GS_GOOD stampa a video i messaggi (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DBGPH_INFO::BackupTable(C_STRING &MyTableRef, BackUpModeEnum Mode, int MsgToVideo)
{
   C_STRING       Catalog, Schema, Table, BackUpTable;
   C_DBCONNECTION *pConn = getDBConnection();
   int            conf = GS_BAD, result = GS_GOOD;

   int IsTransactionSupported = pConn->BeginTrans();

   do
   {
      if (pConn->split_FullRefTable(MyTableRef, Catalog, Schema, Table) == GS_BAD)
         { result = GS_BAD; break; }

      // se il riferimento completo è una path di un file (il catalogo è il direttorio)
      if (pConn->get_CatalogResourceType() == DirectoryRes)
         Catalog += _T("\\BACKUP"); // eseguo/ ripristino/cancello il backup nel sottodirettorio "BACKUP"
      BackUpTable = Table;
      BackUpTable += _T("BACKUP");
  
      if (BackUpTable.paste(pConn->get_FullRefTable(Catalog, Schema, BackUpTable)) == NULL)
         { result = GS_BAD; break; }

      switch (Mode)
      {
         case GSCreateBackUp: // crea backup
            if (pConn->get_CatalogResourceType() == DirectoryRes && Catalog.get_name())
               if (gsc_mkdir(Catalog.get_name()) == GS_BAD) 
               { result = GS_BAD; break; }
            if (pConn->ExistTable(BackUpTable) == GS_GOOD)
            {
               // "Tabella di backup già esistente. Sovrascrivere la tabella ?"
               if (gsc_ddgetconfirm(gsc_msg(793), &conf) == GS_BAD) 
                  { result = GS_BAD; break; }
               if (conf != GS_GOOD) { result = GS_CAN; break; }

               if (pConn->DelTable(BackUpTable.get_name()) == GS_BAD)
                  { result = GS_BAD; break; }
            }

            // Non copio gli indici (Oracle non ammette nomi di indici uguali)
            if (pConn->CopyTable(MyTableRef.get_name(), BackUpTable.get_name(),
                                 GSStructureAndDataCopyType, GS_BAD) == GS_BAD)
               { result = GS_BAD; break; }

            break;

         case GSRemoveBackUp: // cancella backup
            if (pConn->ExistTable(BackUpTable) == GS_GOOD)
               if (pConn->DelTable(BackUpTable.get_name()) == GS_BAD)
                  { result = GS_BAD; break; }
            if (pConn->get_CatalogResourceType() == DirectoryRes && Catalog.get_name())
               gsc_rmdir(Catalog, ONETEST);
            break;

         case GSRestoreBackUp: // ripristina backup
         {
            if (pConn->ExistTable(MyTableRef) == GS_GOOD)
               if (pConn->DelTable(MyTableRef.get_name()) == GS_BAD)
                  { result = GS_BAD; break; }
            if (pConn->CopyTable(BackUpTable.get_name(), MyTableRef.get_name()) == GS_BAD)
               { result = GS_BAD; break; }

            // reindicizzo per ripristinare le chiavi primarie
            if (ReindexTable(MyTableRef) == GS_BAD)
               { result = GS_BAD; break; }

            pConn->DelTable(BackUpTable.get_name());
            break;
         }

         default:
            break;  
      }
   }
   while (0);

   if (result != GS_GOOD)
   {
      if (IsTransactionSupported == GS_GOOD) pConn->RollbackTrans();
      return GS_BAD;
   }
   else
   {
      if (IsTransactionSupported == GS_GOOD) pConn->CommitTrans();
      if (Mode == GSRestoreBackUp) // ripristina backup
         if (pConn->get_CatalogResourceType() == DirectoryRes && Catalog.get_name())
            gsc_rmdir(Catalog, ONETEST);
   }

   return GS_GOOD; 
}


/******************************************************************************/
/*.doc C_DBGPH_INFO::Backup                                       <external>  */
/*+
  Questa funzione esegue il backup dei dati geometrici.
  Parametri:
  BackUpModeEnum Mode;  Flag di modalità: Creazione , restore o cancellazione
                        del backup esistente (default = GSCreateBackUp).
  int MsgToVideo; flag, se = GS_GOOD stampa a video i messaggi (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DBGPH_INFO::Backup(BackUpModeEnum Mode, int MsgToVideo)
{
   // Se si tratta di una tabella collegata non posso gestire il backup
   if (!LinkedTable)
      if (BackupTable(TableRef, Mode, MsgToVideo) == GS_BAD) return GS_BAD;

   if (LblGroupingTableRef.len() > 0)
      // Se si tratta di una tabella collegata non posso gestire il backup
      if (!LinkedLblGrpTable)
         if (BackupTable(LblGroupingTableRef, Mode, MsgToVideo) == GS_BAD) return GS_BAD;
   
   if (LblTableRef.len() > 0)
      // Se si tratta di una tabella collegata non posso gestire il backup
      if (!LinkedLblTable)
         if (BackupTable(LblTableRef, Mode, MsgToVideo) == GS_BAD) return GS_BAD;

   if (Mode == GSRestoreBackUp) // ripristina backup
      // creo indici
      if (Check(REINDEX) == GS_BAD) return GS_BAD;

   return GS_GOOD; 
}


void C_DBGPH_INFO::get_AliasFieldName(C_STRING &AliasTableName, C_STRING &FieldName,
                                      C_STRING &AliasFieldName, C_STRING *pOperationStr)
{
   C_STRING dummy(FieldName);

   gsc_AdjSyntax(dummy, getDBConnection()->get_InitQuotedIdentifier(),
                 getDBConnection()->get_FinalQuotedIdentifier(),
                 getDBConnection()->get_StrCaseFullTableRef());

   AliasFieldName = AliasTableName;
   AliasFieldName += _T(".");
   AliasFieldName += dummy;
   if (pOperationStr) AliasFieldName += pOperationStr->get_name();
   AliasFieldName += _T(" AS ");
   AliasFieldName += getDBConnection()->get_InitQuotedIdentifier();
   AliasFieldName += AliasTableName;
   AliasFieldName += _T("_");
   AliasFieldName += FieldName;
   AliasFieldName += getDBConnection()->get_FinalQuotedIdentifier();
}


/*********************************************************/
/*.doc C_DBGPH_INFO::internal_get_SQLFieldNameList <internal> */
/*+
  Ottiene una stringa contenente la lista dei campi da leggere nella
  tabella geometrica in formato SQL (viene usato l'alias "GEO.nome_campo"
  oppure "LBL.nome_campo" per le etichette)
  Parametri:
  C_STRING &TableRef;         Nome della tabella per verifica esistenza campi
  TCHAR *AliasTabName;        Alias SQL da usare
  C_STRING &SQLFieldNameList; Lista dei campi
  int objectType;             Tipo di oggetti da generare

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::internal_get_SQLFieldNameList(C_STRING &TableRef, C_STRING &AliasTabName,
                                                C_STRING &SQLFieldNameList, int objectType)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_RB_LIST      Stru;
   C_STRING       AttrName, AliasFieldName, UnitConvFactorStr;
   bool           isCoordToConvert;
   double         UnitConvFactor;

   SQLFieldNameList.clear();
   // Leggo struttura tabella per verificare l'esistenza dei campi
   if ((Stru = pConn->ReadStruct(TableRef.get_name())) == NULL)
      return GS_BAD;

   isCoordToConvert = (get_isCoordToConvert() == GS_GOOD) ? true : false;
   UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();
   if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
   {
      UnitConvFactorStr = _T(" * ");
      UnitConvFactorStr += UnitConvFactor;
   }

   // key_attrib (nome del campo chiave dell'oggetto grafico)
   if (Stru.Assoc(key_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, key_attrib, AliasFieldName);
      SQLFieldNameList += AliasFieldName;
   }

   // ent_key_attrib (nome del campo contenente il codice dell'entita' di appartenenza
   // oppure in caso di label codice del blocco a cui appartengono gli attributi)
   // se esiste nella struttura ed è diverso da key_attrib
   if (Stru.Assoc(ent_key_attrib.get_name(), FALSE) && ent_key_attrib.comp(key_attrib) != 0)
   {
      get_AliasFieldName(AliasTabName, ent_key_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
   if (geom_attrib.len() == 0)
   {
      if (Stru.Assoc(x_attrib.get_name(), FALSE))
      {
         get_AliasFieldName(AliasTabName, x_attrib, AliasFieldName);
         SQLFieldNameList += _T(',');
         SQLFieldNameList += AliasFieldName;
      }

      if (Stru.Assoc(y_attrib.get_name(), FALSE))
      {
         get_AliasFieldName(AliasTabName, y_attrib, AliasFieldName);
         SQLFieldNameList += _T(',');
         SQLFieldNameList += AliasFieldName;
      }

      if (Stru.Assoc(z_attrib.get_name(), FALSE))
      {
         get_AliasFieldName(AliasTabName, z_attrib, AliasFieldName);
         SQLFieldNameList += _T(',');
         SQLFieldNameList += AliasFieldName;
      }

      if (Stru.Assoc(vertex_parent_attrib.get_name(), FALSE))
      {
         get_AliasFieldName(AliasTabName, vertex_parent_attrib, AliasFieldName);
         SQLFieldNameList += _T(',');
         SQLFieldNameList += AliasFieldName;
      }

      if (Stru.Assoc(bulge_attrib.get_name(), FALSE))
      {
         get_AliasFieldName(AliasTabName, bulge_attrib, AliasFieldName);
         SQLFieldNameList += _T(',');
         SQLFieldNameList += AliasFieldName;
      }
   }
   else
   {
      if (Stru.Assoc(geom_attrib.get_name(), FALSE))
      {
         C_STRING DestSRID;
         C_STRING GeomToWKT_SQLSyntax;

         if (isCoordToConvert)
            DestSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

         // es. st_asewkt(alias.campo) in POSTGIS
         //     alias.campo.get_wkt() in ORACLE
         SQLFieldNameList += _T(',');

         // Ricavo la sintassi SQL per leggere la geometria in formato testo
         if (pConn->get_GeomToWKT_SQLSyntax(AliasTabName, geom_attrib, DestSRID, GeomToWKT_SQLSyntax) != GS_GOOD)
            return GS_BAD;
         SQLFieldNameList += GeomToWKT_SQLSyntax;

         SQLFieldNameList += _T(" AS ");
         SQLFieldNameList += pConn->get_InitQuotedIdentifier();
         SQLFieldNameList += AliasTabName;
         SQLFieldNameList += _T("_");
         SQLFieldNameList += geom_attrib;
         SQLFieldNameList += pConn->get_FinalQuotedIdentifier();
      }
   }

   // rotation_attrib (nome del campo contenente la rotazione dell'oggetto)
   if (Stru.Assoc(rotation_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, rotation_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // aggr_factor_attrib (nome del campo contenente il fattore di aggregazione dell'oggetto)
   if (Stru.Assoc(aggr_factor_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, aggr_factor_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // layer_attrib (nome del campo contenente il layer dell'oggetto)
   if (Stru.Assoc(layer_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, layer_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // color_attrib (nome del campo contenente il codice colore dell'oggetto)
   if (Stru.Assoc(color_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, color_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // line_type_attrib (nome del campo contenente il nome del tipo linea dell'oggetto)
   if (Stru.Assoc(line_type_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, line_type_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // line_type_scale_attrib (nome del campo contenente la scala del tipo linea dell'oggetto)
   if (Stru.Assoc(line_type_scale_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, line_type_scale_attrib, AliasFieldName, &UnitConvFactorStr);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // width_attrib (nome del campo contenente la larghezza dell'oggetto)
   if (Stru.Assoc(width_attrib.get_name(), FALSE))
   {
      if (objectType == TYPE_TEXT) 
         // il fattore di larghezza se si tratta di testo non va cambiato
         get_AliasFieldName(AliasTabName, width_attrib, AliasFieldName);
      else
         get_AliasFieldName(AliasTabName, width_attrib, AliasFieldName, &UnitConvFactorStr);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // text_attrib (nome del campo contenente il testo dell'oggetto)
   if (Stru.Assoc(text_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, text_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // text_style_attrib (nome del campo contenente lo stile testo dell'oggetto)
   if (Stru.Assoc(text_style_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, text_style_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // h_text_attrib (nome del campo contenente l'altezza testo dell'oggetto)
   if (Stru.Assoc(h_text_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, h_text_attrib, AliasFieldName, &UnitConvFactorStr);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // horiz_align_attrib (nome del campo contenente l'allineamento orizzontale dell'oggetto)
   if (Stru.Assoc(horiz_align_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, horiz_align_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // vert_align_attrib (nome del campo contenente l'allineamento verticale dell'oggetto)
   if (Stru.Assoc(vert_align_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, vert_align_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // oblique_angle_attrib (nome del campo contenente l'angolo obliquo del testo dell'oggetto)
   if (Stru.Assoc(oblique_angle_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, oblique_angle_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // block_attrib (nome del campo contenente il nome del blocco dell'oggetto)
   if (Stru.Assoc(block_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, block_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // scale_attrib (nome del campo contenente il fattore di scala dell'oggetto)
   if (Stru.Assoc(block_scale_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, block_scale_attrib, AliasFieldName, &UnitConvFactorStr);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // thickness_attrib (nome del campo contenente lo spessore verticale dell'oggetto)
   if (Stru.Assoc(thickness_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, thickness_attrib, AliasFieldName, &UnitConvFactorStr);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // hatch_attrib (nome del campo contenente il nome del riempimento dell'oggetto)
   if (Stru.Assoc(hatch_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, hatch_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // hatch_layer_attrib (nome del campo contenente il layer del riempimento dell'oggetto)
   if (Stru.Assoc(hatch_layer_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, hatch_layer_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // hatch_color_attrib (nome del campo contenente il layer del riempimento dell'oggetto)
   if (Stru.Assoc(hatch_color_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, hatch_color_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // hatch_scale_attrib (nome del campo contenente il fattore di scala del riempimento dell'oggetto)
   if (Stru.Assoc(hatch_scale_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, hatch_scale_attrib, AliasFieldName, &UnitConvFactorStr);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // hatch_rotation_attrib (nome del campo contenente il fattore di scala del riempimento dell'oggetto)
   if (Stru.Assoc(hatch_rotation_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, hatch_rotation_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // attrib_name_attrib (nome del campo contenente il nome
   // dell'attributo a cui l'etichetta fa riferimento)
   if (Stru.Assoc(attrib_name_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, attrib_name_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   // attrib_invis_attrib (nome del campo contenente il flag
   // di invisibilità dell'etichetta)
   if (Stru.Assoc(attrib_invis_attrib.get_name(), FALSE))
   {
      get_AliasFieldName(AliasTabName, attrib_invis_attrib, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   return GS_GOOD;
}
int C_DBGPH_INFO::get_SQLFieldNameList(C_STRING &SQLFieldNameList)
{
   C_STRING Alias(_T("GEO"));
   C_CLASS  *pCls = gsc_find_class(prj, cls, sub);

   if (!pCls) return GS_BAD;

   if (internal_get_SQLFieldNameList(TableRef, Alias, SQLFieldNameList, pCls->get_type()) == GS_BAD) return GS_BAD;

   // se si tratta di linee di simulazione
   if (pCls->is_subclass() == GS_GOOD && pCls->get_type() == TYPE_POLYLINE)
   {
      C_STRING FieldName, AliasFieldName;

      // includo anche le informazioni topologiche
      Alias     = _T("TOPO");
      FieldName = _T("INIT_ID");
      get_AliasFieldName(Alias, FieldName, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;

      FieldName = _T("FINAL_ID");
      get_AliasFieldName(Alias, FieldName, AliasFieldName);
      SQLFieldNameList += _T(',');
      SQLFieldNameList += AliasFieldName;
   }

   return GS_GOOD;
}
int C_DBGPH_INFO::get_SQLLblFieldNameList(C_STRING &SQLFieldNameList)
{
   C_STRING Alias;

   if (LblGroupingTableRef.len() > 0)
   {
      C_STRING dummy;

      Alias = _T("LBL");
      if (internal_get_SQLFieldNameList(LblTableRef, Alias, SQLFieldNameList, TYPE_TEXT) == GS_BAD)
          return GS_BAD;

      Alias = _T("GEO");
      if (internal_get_SQLFieldNameList(LblGroupingTableRef, Alias, dummy, TYPE_NODE) == GS_BAD)
         return GS_BAD;
      if (dummy.len() > 0) SQLFieldNameList += _T(',');
      SQLFieldNameList += dummy;
   }
   else
   {
      Alias = _T("GEO");
      if (internal_get_SQLFieldNameList(LblTableRef, Alias, SQLFieldNameList, TYPE_TEXT) == GS_BAD)
          return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CurrentQryAdeToSQL       <external> */
/*+
  Legge le impostazione della query ADE corrente e ne ricava 2 istruzioni
  SQL per leggere i dati dalle tabelle delle geometrie e delle etichette.
  Parametri:
  C_STRING &GeomSQL;  Istruzione SQL per lettura geometria
  C_STRING &LabelSQL; Istruzione SQL per lettura etichette
                      (se esiste la tabella di raggruppamento delle etichette
                       l'istruzione si riferirà a quest'ultima)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::CurrentQryAdeToSQL(C_STRING &GeomSQL, C_STRING &LabelSQL)
{
   int       i = 1;
   C_RB_LIST QryCondIDList, QryCond, SpatialCond;
   presbuf   p, pRbCondType;
   C_STRING  SQLFieldNameList;
   C_STRING  JoinOpStr, NotStr, BgGroupsStr, EndGroupsStr, CondStr, PartialSQL;
   bool      FirstTime = true, noLbl = false;
   C_CLASS   *pCls = gsc_find_class(prj, cls, sub);

   if (!pCls) return GS_BAD;

   /////////////////////////////////////////////////
   // inizio SELECT per tabella geometria principale
   // (i campi usano l'alias "GEO.<nome attributo>")
   // usare AS solo per l'alias dei campi e NON delle tabelle perchè ORACLE non lo supporta
   GeomSQL = _T("SELECT ");
   if (get_SQLFieldNameList(SQLFieldNameList) == GS_BAD) return GS_BAD;
   GeomSQL += SQLFieldNameList;
   GeomSQL += _T(" FROM ");
   GeomSQL += TableRef;
   GeomSQL += _T(" GEO");

   // se si tratta di linee di simulazione
   if (pCls->is_subclass() == GS_GOOD && pCls->get_type() == TYPE_POLYLINE)
   {  // includo anche le informazioni topologiche
      C_STRING   FieldName, AliasFieldName, Catalog, Schema, Table, TopoTableRef;
      C_TOPOLOGY Topo;

      FieldName = ent_key_attrib;
      gsc_AdjSyntax(FieldName, getDBConnection()->get_InitQuotedIdentifier(),
                    getDBConnection()->get_FinalQuotedIdentifier(),
                    getDBConnection()->get_StrCaseFullTableRef());
      AliasFieldName = _T("GEO.");
      AliasFieldName += FieldName;

      // nome tabella topologica OLD   
      Topo.set_type(TYPE_POLYLINE);  // tipologia di tipo rete
      pCls->ptr_info()->getDBConnection(OLD)->split_FullRefTable(pCls->ptr_info()->OldTableRef, 
                                                                 Catalog, Schema, Table);
      Topo.getTopoTabName(pCls->ptr_info()->getDBConnection(OLD), prj, cls, Catalog.get_name(), 
                          Schema.get_name(), TopoTableRef);

      GeomSQL += _T(" LEFT OUTER JOIN ");
      GeomSQL += TopoTableRef;
      GeomSQL += _T(" TOPO ON (");
      GeomSQL += AliasFieldName;
      GeomSQL += _T('=');

      FieldName = _T("LINK_ID");
      gsc_AdjSyntax(FieldName, getDBConnection()->get_InitQuotedIdentifier(),
                    getDBConnection()->get_FinalQuotedIdentifier(),
                    getDBConnection()->get_StrCaseFullTableRef());
      AliasFieldName = _T("TOPO.");
      AliasFieldName += FieldName;

      GeomSQL += AliasFieldName;
      GeomSQL += _T(" AND ");

      FieldName = _T("LINK_SUB");
      gsc_AdjSyntax(FieldName, getDBConnection()->get_InitQuotedIdentifier(),
                    getDBConnection()->get_FinalQuotedIdentifier(),
                    getDBConnection()->get_StrCaseFullTableRef());
      AliasFieldName = _T("TOPO.");
      AliasFieldName += FieldName;

      GeomSQL += AliasFieldName;
      GeomSQL += _T('=');
      GeomSQL += sub;
      GeomSQL += _T(')');
   }
   // fine SELECT per tabella geometria principale
   /////////////////////////////////////////////////

   /////////////////////////////////////////////////
   // inizio SELECT per tabella delle label
   if (LabelWith())
   {
      // usare AS solo per l'alias dei campi e NON delle tabelle perchè ORACLE non lo supporta
      LabelSQL = _T("SELECT ");
      if (get_SQLLblFieldNameList(SQLFieldNameList) == GS_BAD)
         return GS_BAD;
      LabelSQL += SQLFieldNameList;
      LabelSQL += _T(" FROM ");

      // Possono esserci 2 casi:
      // 1) le etichette sono sotto forma di blocchi con attributi in 2 tabelle distinte
      // che sono unite tra loro con una join SQL
      // (la tabella con etichette usa l'alias "LBL.<nome attributo>")
      // 2) le etichette sono sottoforma di testi
      if (LblGroupingTableRef.len() > 0) // caso 1
      {
         LabelSQL += LblGroupingTableRef;
         LabelSQL += _T(" GEO LEFT OUTER JOIN ");
         LabelSQL += LblTableRef;
         LabelSQL += _T(" LBL ON(GEO.");  // alias tab. geometria principale
         LabelSQL += key_attrib;
         LabelSQL += _T("=");
         LabelSQL += _T("LBL.");
         LabelSQL += ent_key_attrib;      // alias tab. etichette
         LabelSQL += _T(")");
      }
      else // caso 2
      {
         LabelSQL += LblTableRef;
         LabelSQL += _T(" GEO ");
      }
   }
   else LabelSQL.clear();
   // fine SELECT per tabella delle label
   /////////////////////////////////////////////////

   // se esiste una sola query spaziale definita
   if ((QryCondIDList << gsc_ade_qrylist()) == NULL) return GS_BAD;

   while ((p = QryCondIDList.getptr_at(i++)) != NULL)
   { 
      QryCond << ade_qrygetcond((ade_id) p->resval.rreal);
      // Aggiungo una tonda all'inizio e una alla fine
      QryCond.link_head(acutBuildList(RTLB, 0));
      QryCond += acutBuildList(RTLE, 0);

      // A joining operator: "and" or "or" or "" (none)
      if ((p = QryCond.nth(0)) && p->restype == RTSTR) JoinOpStr = p->resval.rstring;
      else JoinOpStr.clear();

      // For grouping this condition with others in the query definition 
      // you are building. Use one or more open parentheses as needed, or "" (none).
      // For example, "((
      if ((p = QryCond.nth(1)) && p->restype == RTSTR) BgGroupsStr = p->resval.rstring;
      else BgGroupsStr.clear();

      // The NOT operator, if needed: "not" or "" (none)
      if ((p = QryCond.nth(2)) && p->restype == RTSTR) NotStr = p->resval.rstring;
      else NotStr.clear();

      // A condition type: "Location", "Property", "Data", or "SQL"
      pRbCondType = QryCond.nth(3); // CondType

      // A condition expression. Depends on the condition type
      p = QryCond.nth(4);

      if (gsc_strcmp(pRbCondType->resval.rstring, _T("location"), FALSE) == 0)
      {
         if (LocationQryAdeToSQL(p, CondStr) == GS_BAD) return GS_BAD;
      }
      else
      if (gsc_strcmp(pRbCondType->resval.rstring, _T("Data"), FALSE) == 0)
      {
         if (DataQryAdeToSQL(p, CondStr) == GS_BAD) return GS_BAD;
      }
      else
      if (gsc_strcmp(pRbCondType->resval.rstring, _T("Property"), FALSE) == 0)
      {
         if (PropertyQryAdeToSQL(p, CondStr) == GS_BAD) return GS_BAD;
      }

      if ((p = QryCond.nth(5)) && p->restype == RTSTR) EndGroupsStr = p->resval.rstring;
      else EndGroupsStr.clear();

      if (CondStr.len() > 0) // se esiste una condizione
      {
         // se la condizione significa che non si vuole estrarre le etichette
         if (CondStr.comp(_T("GEO.\"block\"='$T'"), FALSE) == 0)
            noLbl = true;
         else
         {
            if (PartialSQL.len() > 0) // se era già stata impostata una condizione
            {
               PartialSQL = _T(" ");
               PartialSQL += JoinOpStr;
               PartialSQL += _T(" ");
               PartialSQL += BgGroupsStr;
            }
            else // in questo modo evito di mettere JoinOpStr se la condizione spaziale = all 
            {
               PartialSQL = _T(" ");
               PartialSQL += BgGroupsStr;
            }

            if (NotStr.len() > 0)
            {
               PartialSQL += NotStr;
               PartialSQL += _T("(");
               PartialSQL += CondStr;
               PartialSQL += _T(")");
            }
            else
               PartialSQL += CondStr;

            PartialSQL += EndGroupsStr;
   
            if (FirstTime)
            {
               FirstTime = false;
               GeomSQL +=  _T(" WHERE");
               // Se esiste un SQL per le label (la classe è provvista di tabella/e per le label)
               if (LabelSQL.len() > 0)
                  LabelSQL +=  _T(" WHERE");
            }
            GeomSQL += PartialSQL;

            // Se esiste un SQL per le label (la classe è provvista di tabella/e per le label)
            if (LabelSQL.len() > 0)
               LabelSQL += PartialSQL;
         }
      }
   }

   if (FieldValueListOnTable.get_count() > 0)
   {
      C_2STR *pFieldValue = (C_2STR *) FieldValueListOnTable.get_head();

      if (FirstTime) GeomSQL += _T(" WHERE (");
      else GeomSQL += _T(" AND (");

      while (pFieldValue)
      {
         GeomSQL += _T("GEO."); // usare alias "GEO."        
         GeomSQL += pFieldValue->get_name(); // Nome campo
         GeomSQL += _T("=");
         GeomSQL += pFieldValue->get_name2(); // Valore campo

         pFieldValue = (C_2STR *) pFieldValue->get_next();
         if (pFieldValue) GeomSQL += _T(" AND ");
      }
      GeomSQL += _T(')');
   }

   if (noLbl) LabelSQL.clear();

   if (LabelSQL.len() > 0)
   {
      if (LblGroupingTableRef.len() > 0 && FieldValueListOnLblGrpTable.get_count() > 0)
      {
         C_2STR *pFieldValue = (C_2STR *) FieldValueListOnLblGrpTable.get_head();

         if (FirstTime) LabelSQL += _T(" WHERE (");
         else LabelSQL += _T(" AND (");

         while (pFieldValue)
         {
            LabelSQL += _T("GEO."); // usare alias "GEO."        
            LabelSQL += pFieldValue->get_name(); // Nome campo
            LabelSQL += _T("=");
            LabelSQL += pFieldValue->get_name2(); // Valore campo

            pFieldValue = (C_2STR *) pFieldValue->get_next();
            if (pFieldValue) LabelSQL += _T(" AND ");
         }
         LabelSQL += _T(')');
      }

      if (LblTableRef.len() > 0 && FieldValueListOnLblTable.get_count() > 0)
      {
         C_2STR *pFieldValue = (C_2STR *) FieldValueListOnLblTable.get_head();

         if (FirstTime) LabelSQL += _T(" WHERE (");
         else LabelSQL += _T(" AND (");

         while (pFieldValue)
         {
            // se esiste la tabella di raggruppamento usare alias "LBL."
            // altrimenti usare alias "GEO."
            if (LblGroupingTableRef.len() > 0)
               LabelSQL += _T("LBL.");
            else
               LabelSQL += _T("GEO.");

            LabelSQL += pFieldValue->get_name(); // Nome campo
            LabelSQL += _T("=");
            LabelSQL += pFieldValue->get_name2(); // Valore campo

            pFieldValue = (C_2STR *) pFieldValue->get_next();
            if (pFieldValue) LabelSQL += _T(" AND ");
         }
         LabelSQL += _T(')');
      }

      // se le etichette sono sotto forma di blocchi con attributi in 2 tabelle distinte
      // che sono unite tra loro con una join SQL
      if (LblGroupingTableRef.len() > 0)
      {
         LabelSQL += _T(" ORDER BY GEO."); // ROBY 2017
         LabelSQL += key_attrib;
      }
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::LocationQryAdeToSQL      <external> */
/*+
  Legge le impostazione della query spaziale ADE corrente e ne ricava 
  un'istruzione SQL. I campi usano un alias di tabella geometrica = GEO.
  Parametri:
  presbuf pRbCond;         (input)Condizione ADE corrente
  C_STRING &CondStr;       (output) Istruzione SQL
  int *SpatialMode;        (output) Opzionale; ritorna la modalità di 
                           selezione spaziale (INSIDE o CROSSING); default = NULL
  AcDbPolyline **pPolygon; (output) Se la selezione spaziale è con un poligono ritorna
                           il poligono come oggetto autocad; default = NULL

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::LocationQryAdeToSQL(presbuf pRbCond, C_STRING &CondStr,
                                      int *SpatialMode, AcDbPolyline **pPolygon)
{
   presbuf p;

   if ((p = gsc_nth(0, pRbCond)) == NULL || p->restype != RTSTR)
      return GS_BAD;

   // Location expressions
   if (gsc_strcmp(p->resval.rstring, ALL_SPATIAL_COND, FALSE) == 0) // tutto
      return GS_GOOD;
   else
   if (gsc_strcmp(p->resval.rstring, WINDOW_SPATIAL_COND, FALSE) == 0) // window
   {
      double Xmin, Ymin, Xmax, Ymax, dummy;

      // primo punto
      if ((p = gsc_nth(2, pRbCond)) && 
          (p->restype == RTPOINT || p->restype == RT3DPOINT))
      {
         Xmin = p->resval.rpoint[X];
         Ymin = p->resval.rpoint[Y];
      }
      else
         return GS_BAD;

      // secondo punto
      if ((p = gsc_nth(3, pRbCond)) && 
          (p->restype == RTPOINT || p->restype == RT3DPOINT))
      {
         Xmax = p->resval.rpoint[X];
         Ymax = p->resval.rpoint[Y];
      }
      else
         return GS_BAD;

      if (Xmin > Xmax) { dummy = Xmax; Xmax = Xmin; Xmin = dummy; }
      if (Ymin > Ymax) { dummy = Ymax; Ymax = Ymin; Ymin = dummy; }

      // Search type keyword (string): "inside" or "crossing".
      if ((p = gsc_nth(1, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;

      // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
      if (geom_attrib.len() == 0) 
      {
         C_CLASS  *pCls = gsc_find_class(prj, cls, sub);
         
         switch (pCls->get_type())
         {
            case TYPE_SURFACE:
            case TYPE_SPAGHETTI:
               { GS_ERR_COD = eGSOpNotAble; return GS_BAD; }
            case TYPE_POLYLINE:
               CondStr = _T("GEO."); // Alias tab. geometria principale
               CondStr += vertex_parent_attrib;

               if (gsc_strcmp(p->resval.rstring, _T("inside"), FALSE) == 0)
               {  // tutti i vertici devono essere interni
                  // id_linee IN (SELECT ID_LINEE FROM (SELECT SUM(X >=15 AND X <= 25 AND Y >=5 AND Y <= 20) AS conteggio_vertici_interni, SUM(TRUE) as conteggio_vertici, ID_LINEE
                  // FROM LINEE_G GROUP BY ID_LINEE) WHERE conteggio_vertici_interni = conteggio_vertici)
                  CondStr += _T(" IN (SELECT ");
                  CondStr += vertex_parent_attrib;
                  CondStr += _T(" FROM (SELECT SUM(");
                  CondStr += x_attrib;
                  CondStr += _T(" >= ");
                  CondStr += Xmin;
                  CondStr += _T(" AND ");
                  CondStr += x_attrib;
                  CondStr += _T(" <= ");
                  CondStr += Xmax;
                  CondStr += _T(" AND ");
                  CondStr += y_attrib;
                  CondStr += _T(" >= ");
                  CondStr += Ymin;
                  CondStr += _T(" AND ");
                  CondStr += y_attrib;
                  CondStr += _T(" <= ");
                  CondStr += Ymax;
                  CondStr += _T(") AS conteggio_vertici_interni, SUM(TRUE) as conteggio_vertici, ");
                  CondStr += vertex_parent_attrib;
                  CondStr += _T(" FROM ");
                  CondStr += TableRef;
                  CondStr += _T(" GROUP BY ");
                  CondStr += vertex_parent_attrib;
                  CondStr += _T(") WHERE conteggio_vertici_interni = conteggio_vertici)");

                  if (SpatialMode) *SpatialMode = INSIDE;
               }
               else // crossing
               {  // basta che un vertice della linea sia interno o sul bordo alla finestra
                  // id_linee IN (SELECT DISTINCT ID_LINEE FROM LINEE_G 
                  // WHERE X >=15 AND X <= 25 AND Y >=5 AND Y <= 20)
                  CondStr += _T(" IN (SELECT DISTINCT ");
                  CondStr += vertex_parent_attrib;
                  CondStr += _T(" FROM ");
                  CondStr += TableRef;
                  CondStr += _T(" WHERE ");
                  CondStr += x_attrib;
                  CondStr += _T(" >= ");
                  CondStr += Xmin;
                  CondStr += _T(" AND ");
                  CondStr += x_attrib;
                  CondStr += _T(" <= ");
                  CondStr += Xmax;
                  CondStr += _T(" AND ");
                  CondStr += y_attrib;
                  CondStr += _T(" >= ");
                  CondStr += Ymin;
                  CondStr += _T(" AND ");
                  CondStr += y_attrib;
                  CondStr += _T(" <= ");
                  CondStr += Ymax;
                  CondStr += _T(")");

                  if (SpatialMode) *SpatialMode = CROSSING;
               }
               break;
            default:
               CondStr = _T("GEO."); // Alias tab. geometria principale
               CondStr += x_attrib;
               CondStr += _T(" >= ");
               CondStr += Xmin;
               CondStr += _T(" AND ");
               CondStr += _T("GEO."); // Alias tab. geometria principale
               CondStr += x_attrib;
               CondStr += _T(" <= ");
               CondStr += Xmax;
               CondStr += _T(" AND ");
               CondStr += _T("GEO."); // Alias tab. geometria principale
               CondStr += y_attrib;
               CondStr += _T(" >= ");
               CondStr += Ymin;
               CondStr += _T(" AND ");
               CondStr += _T("GEO."); // Alias tab. geometria principale
               CondStr += y_attrib;
               CondStr += _T(" <= ");
               CondStr += Ymax;

               if (SpatialMode) *SpatialMode = CROSSING;

               break;
         }
      }
      else // campo geometrico
      {
         C_DBCONNECTION *pConn = getDBConnection();
         C_STRING       GeomField, WKT, WKTToGeom_SQLSyntax, SrcSRID;

         GeomField = _T("GEO."); // Alias tab. geometria principale
         GeomField += geom_attrib;

         if (get_isCoordToConvert() == GS_GOOD)
            SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

         WKT = _T("POLYGON((");
         // punto in basso a sx
         WKT += Xmin; WKT += _T(" "); WKT += Ymin;
         WKT += _T(", ");
         // punto in basso a dx
         WKT += Xmin; WKT += _T(" "); WKT += Ymax;
         WKT += _T(", ");
         // punto in alto a dx
         WKT += Xmax; WKT += _T(" "); WKT += Ymax;
         WKT += _T(", ");
         // punto in alto a sx
         WKT += Xmax; WKT += _T(" "); WKT += Ymin;
         WKT += _T(", ");
         // punto in basso a sx
         WKT += Xmin; WKT += _T(" "); WKT += Ymin;
         WKT += _T("))");
         // Converto in geometria
         pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

         if (gsc_strcmp(p->resval.rstring, _T("inside"), FALSE) == 0)
         {
            // strettamente interni
            pConn->get_InsideSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                       CondStr);
            if (SpatialMode) *SpatialMode = INSIDE;
         }
         else
         {
            // interni e che attraversano il perimetro della finestra
            pConn->get_CrossingSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                         CondStr);
            if (SpatialMode) *SpatialMode = CROSSING;
         }
      }
   }
   else
   if (gsc_strcmp(p->resval.rstring, POLYLINE_SPATIAL_COND, FALSE) == 0) // polylinea
   {  // ("polyline" "bufferfence" searchtype offset ename)
      // ("polyline" "fence" ename)
      // ("polyline" "polygon" searchtype ename)
      C_DBCONNECTION *pConn = getDBConnection();
      C_STRING       GeomField, WKT, WKTToGeom_SQLSyntax;

      // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
      if (geom_attrib.len() == 0) return GS_BAD;

      GeomField = _T("GEO."); // Alias tab. geometria principale
      GeomField += geom_attrib;

      if ((p = gsc_nth(1, pRbCond)) == NULL || p->restype != RTSTR)
         return GS_BAD;
      // Location sub-expressions
      if (gsc_strcmp(p->resval.rstring, POLYGON_SPATIAL_COND, FALSE) == 0) // Polyline-Polygon
      {  // ("polyline" "polygon" searchtype (flag_closed (normal_vector) bulge1 (x1 y1 z1) ...))
         C_POINT_LIST PtList;
         C_STRING     CoordStr, SrcSRID;

         // inizio descrizione polilinea
         if ((p = gsc_nth(3, pRbCond)) == NULL) return GS_BAD;

         if (PtList.add_vertex_point(&p, GS_GOOD,
                                     pConn->get_MaxTolerance2ApproxCurve(),
                                     pConn->get_MaxSegmentNum2ApproxCurve()) == GS_BAD) return GS_BAD;
         // Se il vertice iniziale non è uguale a quello finale
         if (*((C_POINT *) PtList.get_head()) != *((C_POINT *) PtList.get_tail()))
            if (PtList.add_point(((C_POINT *) PtList.get_head())->x(), 
                                 ((C_POINT *) PtList.get_head())->y(), 
                                 ((C_POINT *) PtList.get_head())->z()) == GS_BAD) return GS_BAD;

         if (pPolygon)
            if ((*pPolygon = (AcDbPolyline *) PtList.toPolyline()) == NULL) return GS_BAD;

         WKT = _T("POLYGON((");
         if (PtList.toStr(CoordStr, geom_dim, false, _T(','), _T(' '),
                          pConn->get_MaxTolerance2ApproxCurve(),
                          pConn->get_MaxSegmentNum2ApproxCurve()) < 0)
            return GS_BAD;
         WKT += CoordStr;
         WKT += _T("))");

         if (get_isCoordToConvert() == GS_GOOD)
            SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

         // Converto in geometria
         pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

         // Search type keyword (string): "inside" or "crossing".
         if ((p = gsc_nth(2, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;

         if (gsc_strcmp(p->resval.rstring, _T("inside"), FALSE) == 0)
         {
            // strettamente interni
            pConn->get_InsideSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                       CondStr);
            if (SpatialMode) *SpatialMode = INSIDE;
         }
         else
         {
            // interni e che attraversano il perimetro della finestra
            pConn->get_CrossingSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                         CondStr);
            if (SpatialMode) *SpatialMode = CROSSING;
         }
      }
      else
      if (gsc_strcmp(p->resval.rstring, BUFFERFENCE_SPATIAL_COND, FALSE) == 0) // Polyline-Bufferfence
      {  // ("polyline" "bufferfence" searchtype buffer (flag_closed (normal_vector) bulge1 (x1 y1 z1) ...))
         C_POINT_LIST PtList;
         C_STRING     CoordStr, SrcSRID, BufferLengthStr;
         double       BufferLength;

         // inizio descrizione polilinea
         if ((p = gsc_nth(4, pRbCond)) == NULL) return GS_BAD;

         if (PtList.add_vertex_point(&p, GS_GOOD,
                                     pConn->get_MaxTolerance2ApproxCurve(),
                                     pConn->get_MaxSegmentNum2ApproxCurve()) == GS_BAD) return GS_BAD;

         if (pPolygon)
            if ((*pPolygon = (AcDbPolyline *) PtList.toPolyline()) == NULL) return GS_BAD;

         // Search buffer length (double).
         if ((p = gsc_nth(3, pRbCond)) == NULL || gsc_rb2Dbl(p, &BufferLength) != GS_GOOD) return GS_BAD;

         WKT = _T("LINESTRING(");
         if (PtList.toStr(CoordStr, geom_dim, false, _T(','), _T(' '),
                          pConn->get_MaxTolerance2ApproxCurve(),
                          pConn->get_MaxSegmentNum2ApproxCurve()) < 0)
            return GS_BAD;
         WKT += CoordStr;
         WKT += _T(")");

         if (get_isCoordToConvert() == GS_GOOD)
            SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

         // Converto in geometria
         pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

         // Creo il buffer
         BufferLengthStr = _T(",");
         BufferLengthStr += BufferLength;
         BufferLengthStr += _T(")");
         WKTToGeom_SQLSyntax.addPrefixSuffix(_T("Buffer("), BufferLengthStr.get_name());

         // Search type keyword (string): "inside" or "crossing".
         if ((p = gsc_nth(2, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;

         if (gsc_strcmp(p->resval.rstring, _T("inside"), FALSE) == 0)
         {
            // strettamente interni
            pConn->get_InsideSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                       CondStr);
            if (SpatialMode) *SpatialMode = INSIDE;
         }
         else
         {
            // interni e che attraversano il perimetro della finestra
            pConn->get_CrossingSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                         CondStr);
            if (SpatialMode) *SpatialMode = CROSSING;
         }
      }
      else
      if (gsc_strcmp(p->resval.rstring, FENCE_SPATIAL_COND, FALSE) == 0) // Polyline-Fence
      {  // ("polyline" "fence" (flag_closed (normal_vector) bulge1 (x1 y1 z1) ...))
         C_POINT_LIST PtList;
         C_STRING     CoordStr, SrcSRID;

         // inizio descrizione polilinea
         if ((p = gsc_nth(2, pRbCond)) == NULL) return GS_BAD;

         if (PtList.add_vertex_point(&p, GS_GOOD,
                                     pConn->get_MaxTolerance2ApproxCurve(),
                                     pConn->get_MaxSegmentNum2ApproxCurve()) == GS_BAD) return GS_BAD;

         if (pPolygon)
            if ((*pPolygon = (AcDbPolyline *) PtList.toPolyline()) == NULL) return GS_BAD;

         WKT = _T("LINESTRING(");
         if (PtList.toStr(CoordStr, geom_dim, false, _T(','), _T(' '),
                          pConn->get_MaxTolerance2ApproxCurve(),
                          pConn->get_MaxSegmentNum2ApproxCurve()) < 0)
            return GS_BAD;
         WKT += CoordStr;
         WKT += _T(")");

         if (get_isCoordToConvert() == GS_GOOD)
            SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

         // Converto in geometria
         pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

         // interni e che attraversano il perimetro della finestra
         pConn->get_CrossingSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                      CondStr);
         if (SpatialMode) *SpatialMode = CROSSING;
      }
      else
         return GS_BAD; // espressione non supportata
   }
   else
   if (gsc_strcmp(p->resval.rstring, POLYGON_SPATIAL_COND, FALSE) == 0) // polygon
   { // ("polygon" searchtype pt1 pt2 ... ptN)
      C_DBCONNECTION *pConn = getDBConnection();
      C_STRING       GeomField, WKT, WKTToGeom_SQLSyntax;
      ads_point      FirstPt;
      AcGePoint2d    PolygonPt;
      double         Bulge;
      AcDbPolyline   Polygon;
      C_POINT_LIST   PtList;
      C_STRING       CoordStr, SrcSRID;

      // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
      if (geom_attrib.len() == 0) return GS_BAD;

      GeomField = _T("GEO."); // Alias tab. geometria principale
      GeomField += geom_attrib;

      if ((p = gsc_nth(1, pRbCond)) == NULL || p->restype != RTSTR)
         return GS_BAD;

      if (!(p = p->rbnext)) return GS_BAD;

      // Primo bulge
      if (p->restype == RTREAL)
      {
         gsc_rb2Dbl(p, &Bulge);
         p = p->rbnext;
      }
      else
         Bulge = 0;

      // Primo punto 
      if (p->restype != RTPOINT && p->restype != RT3DPOINT)
         return GS_BAD;

      PolygonPt.set(p->resval.rpoint[X], p->resval.rpoint[Y]);
      Polygon.addVertexAt(0, PolygonPt, Bulge); 

      Polygon.setClosed(kTrue);

      if (pPolygon)
      {
         *pPolygon = new AcDbPolyline();
         PolygonPt.set(p->resval.rpoint[X], p->resval.rpoint[Y]);
         (*pPolygon)->addVertexAt(0, PolygonPt, Bulge);               
      }

      ads_point_set(p->resval.rpoint, FirstPt);

      while ((p = p->rbnext) != NULL)
      {
         // bulge
         if (p->restype == RTREAL)
         {
            gsc_rb2Dbl(p, &Bulge);
            p = p->rbnext;
         }
         else
            Bulge = 0;

         // punto 
         if (p->restype != RTPOINT && p->restype != RT3DPOINT)
            break;

         PolygonPt.set(p->resval.rpoint[X], p->resval.rpoint[Y]);
         Polygon.addVertexAt(0, PolygonPt, Bulge); 
      }

      if (pPolygon)
         if ((*pPolygon = (AcDbPolyline *) Polygon.clone()) == NULL) return GS_BAD;

      WKT = _T("POLYGON((");
      if (PtList.add_vertex_point(&Polygon, GS_GOOD,
                                  pConn->get_MaxTolerance2ApproxCurve(),
                                  pConn->get_MaxSegmentNum2ApproxCurve()) == GS_BAD) return GS_BAD;
      if (PtList.toStr(CoordStr, geom_dim, false, _T(','), _T(' '),
                       pConn->get_MaxTolerance2ApproxCurve(),
                       pConn->get_MaxSegmentNum2ApproxCurve()) < 0)
         return GS_BAD;
      WKT += CoordStr;
      WKT += _T("))");

      if (get_isCoordToConvert() == GS_GOOD)
         SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

      // Converto in geometria
      pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

      // Search type keyword (string): "inside" or "crossing".
      if ((p = gsc_nth(1, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;

      if (gsc_strcmp(p->resval.rstring, _T("inside"), FALSE) == 0)
      {
         // strettamente interni
         pConn->get_InsideSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                    CondStr);
            if (SpatialMode) *SpatialMode = INSIDE;
      }
      else
      {
         // interni e che attraversano il perimetro della finestra
         pConn->get_CrossingSpatialOperator_SQLSyntax(GeomField, WKTToGeom_SQLSyntax,
                                                      CondStr);
         if (SpatialMode) *SpatialMode = CROSSING;
      }
   }
   else // espressione non supportata
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::DataQryAdeToSQL          <external> */
/*+
  Legge le impostazione della query sui dati ADE corrente e ne ricava 
  un'istruzione SQL.
  Parametri:
  presbuf pRbCond;    Condizione ADE corrente
  C_STRING &CondStr;  Istruzione SQL

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::DataQryAdeToSQL(presbuf pRbCond, C_STRING &CondStr)
{
   presbuf  p;
   C_CLASS  *pCls = gsc_find_class(prj, cls, sub);
   C_STRING AliasFieldName;

   if (!pCls) return GS_BAD;

   // Data expressions
   if ((p = gsc_nth(0, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;

   if (gsc_strcmp(p->resval.rstring, _T("objdata"), FALSE) == 0)
   {
   	C_STRING ODTblFldName;
      LONG     number;

      gsc_getODTableName(prj, cls, sub, ODTblFldName);
      ODTblFldName += _T(".ID");

      // tablename.fieldname
      if ((p = gsc_nth(1, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;
      // verifico che si tratti della tabella OD della classe + ".ID"
      if (ODTblFldName.comp(p->resval.rstring, FALSE) != 0) return GS_GOOD;
      
      AliasFieldName = ent_key_attrib;
      gsc_AdjSyntax(AliasFieldName, getDBConnection()->get_InitQuotedIdentifier(),
                    getDBConnection()->get_FinalQuotedIdentifier(),
                    getDBConnection()->get_StrCaseFullTableRef());
       
      CondStr = _T("GEO.");
      CondStr += AliasFieldName;

      // Comparison operator (string): "=", ">", "<", "<=", ">=", or "<>". 
      // Note that the only valid operator in a string context is "=".
      if ((p = gsc_nth(2, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;
      CondStr += p->resval.rstring;
      // Value to match
      if ((p = gsc_nth(3, pRbCond)) == NULL) return GS_BAD;

      if (gsc_rb2Lng(p, &number) == GS_BAD) return GS_BAD;

      CondStr += number;
   }
   else // espressione non supportata
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::PropertyQryAdeToSQL      <external> */
/*+
  Legge le impostazione della query sui dati ADE corrente e ne ricava 
  un'istruzione SQL.
  Parametri:
  presbuf pRbCond;    Condizione ADE corrente
  C_STRING &CondStr;  Istruzione SQL

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::PropertyQryAdeToSQL(presbuf pRbCond, C_STRING &CondStr)
{
   presbuf  p;
   C_STRING AliasFieldName;

   // Property expressions
   if ((p = gsc_nth(0, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;

   if (gsc_strcmp(p->resval.rstring, _T("blockname"), FALSE) == 0)
   {
   	C_STRING BlockName;
      
      AliasFieldName = block_attrib;
      gsc_AdjSyntax(AliasFieldName, getDBConnection()->get_InitQuotedIdentifier(),
                    getDBConnection()->get_FinalQuotedIdentifier(),
                    getDBConnection()->get_StrCaseFullTableRef());
       
      CondStr = _T("GEO.");
      CondStr += AliasFieldName;

      // Comparison operator (string): "=", ">", "<", "<=", ">=", or "<>". 
      // Note that the only valid operator in a string context is "=".
      if ((p = gsc_nth(1, pRbCond)) == NULL || p->restype != RTSTR) return GS_BAD;
      CondStr += p->resval.rstring;
      // Value to match
      if ((p = gsc_nth(2, pRbCond)) == NULL) return GS_BAD;
      BlockName = p;
      // Correggo la stringa secondo la sintassi SQL
      if (getDBConnection()->Str2SqlSyntax(BlockName) == GS_BAD) return GS_BAD;
      CondStr += BlockName;
   }
   else // espressione non supportata
      return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::InitAppendOnAcDbEntityPtrArray <internal> */
/*+
  Funzione di inizializzazione per la funzione AppendOnAcDbEntityPtrArray
  nel caso di lettura degli oggetti grafici principali.
  Parametri:
  int GeomType;            Usato per stabilire il tipo di dato geometrico
                           (TYPE_POLYLINE, TYPE_TEXT, TYPE_NODE, TYPE_SURFACE,
                           TYPE_SPAGHETTI)
  C_RB_LIST &ColValues;
  presbuf *pKey;
  presbuf *pEntKey;
  presbuf *pAggrFactor;
  presbuf *pGeom;
  presbuf *pX;
  presbuf *pY;
  presbuf *pZ;
  presbuf *pVertParent;
  presbuf *pBulge;
  presbuf *pLayer;
  presbuf *pColor;
  presbuf *pLineType;
  presbuf *pLineTypeScale;
  presbuf *pWidth;
  presbuf *pText;
  presbuf *pTextStyle;
  presbuf *pTextHeight;
  presbuf *pVertAlign;
  presbuf *pHorizAlign;
  presbuf *pBlockName;
  presbuf *pBlockScale;
  presbuf *pRot;
  presbuf *pThickness;
  presbuf *pHatchName;
  presbuf *pHatchLayer;
  presbuf *pHatchColor;
  presbuf *pHatchScale;
  presbuf *pHatchRot;
  presbuf *pLblAttrName;   Opzionale, Usato solo se vogliono leggere le 
                           label (in questo caso non sarà considerato GeomType,
                           default = NULL) 
  presbuf *pLblAttrInvis;  Opzionale, Usato solo se vogliono leggere le 
                           label (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::InitAppendOnAcDbEntityPtrArray(int GeomType,
                                                 C_RB_LIST &ColValues,
                                                 presbuf *pKey, 
                                                 presbuf *pEntKey,
                                                 presbuf *pAggrFactor,
                                                 presbuf *pGeom, // geometria
                                                 presbuf *pX, presbuf *pY, presbuf *pZ, // geometria
                                                 presbuf *pVertParent, presbuf *pBulge, // geometria
                                                 presbuf *pLayer,
                                                 presbuf *pColor,
                                                 presbuf *pLineType, presbuf *pLineTypeScale, presbuf *pWidth,
                                                 presbuf *pText, presbuf *pTextStyle, presbuf *pTextHeight,
                                                 presbuf *pVertAlign, presbuf *pHorizAlign, presbuf *pObliqueAng,
                                                 presbuf *pBlockName, presbuf *pBlockScale,
                                                 presbuf *pRot, 
                                                 presbuf *pThickness,
                                                 presbuf *pHatchName, presbuf *pHatchLayer,
                                                 presbuf *pHatchColor, presbuf *pHatchScale, presbuf *pHatchRot,
                                                 presbuf *pLblAttrName, presbuf *pLblAttrInvis)
{
   // se il flag Label = true possono esserci 2 casi:
   // 1) le etichette sono sotto forma di blocchi con attributi in 2 tabelle distinte
   // che sono unite tra loro con una join SQL
   // (la tabella con etichette usa l'alias "LBL.<nome attributo>")
   // 2) le etichette sono sottoforma di testi
   C_STRING TabAlias(_T("GEO_")), AttrName;

   if (pLblAttrName) // se si vogliono leggere le label
      if (LblGroupingTableRef.len() > 0) // caso 1
         TabAlias = _T("LBL_"); // uso un alias

   // key_attrib (nome del campo chiave dell'oggetto grafico)
   AttrName = TabAlias;
   AttrName += key_attrib;
   if (!(*pKey = ColValues.CdrAssoc(AttrName.get_name(), FALSE))) // Obbligatorio
      return GS_BAD;

   if (GeomType != TYPE_SPAGHETTI)
   {
      // ent_key_attrib (nome del campo contenente il codice dell'entita' di appartenenza
      // oppure in caso di label codice del blocco a cui appartengono gli attributi)
      AttrName = TabAlias;
      AttrName += ent_key_attrib;
      if (!(*pEntKey = ColValues.CdrAssoc(AttrName.get_name(), FALSE))) // Obbligatorio
         return GS_BAD;
   }
   else
      *pEntKey = NULL;

   AttrName = TabAlias;
   AttrName += geom_attrib;
   *pGeom = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
   // Se geom_attrib non esiste, la geometria è sotto forma di 2/3 campi numerici
   if (*pGeom == NULL)
   {
      AttrName = TabAlias;
      AttrName += x_attrib;
      if ((*pX = ColValues.CdrAssoc(AttrName.get_name(), FALSE)) == NULL) // Obbligatorio
         return GS_BAD;
      AttrName = TabAlias;
      AttrName += y_attrib;
      if ((*pY = ColValues.CdrAssoc(AttrName.get_name(), FALSE)) == NULL) // Obbligatorio
         return GS_BAD;
      if (geom_dim == GS_3D && z_attrib.len() > 0) // 3D
      {
         AttrName = TabAlias;
         AttrName += z_attrib;
         if ((*pZ = ColValues.CdrAssoc(AttrName.get_name(), FALSE)) == NULL) return GS_BAD;
      }
      else *pZ = NULL;

      if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_POLYLINE || 
          GeomType == TYPE_SURFACE)
      {
         // vertex_parent_attrib (nome del campo contenente un codice per 
         //                       raggruppare i vertici per comporre una linea)
         AttrName = TabAlias;
         AttrName += vertex_parent_attrib;
         if ((*pVertParent = ColValues.CdrAssoc(AttrName.get_name(), FALSE)) == NULL)
            return GS_BAD;

         // bulge_attrib (Nome del campo contenente il bulge tra un
         // vertice e l'altro dell'oggetto grafico lineare)
         AttrName = TabAlias;
         AttrName += bulge_attrib;
         *pBulge = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      }
      else
         *pVertParent = *pBulge = NULL;
   }

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT || GeomType == TYPE_NODE)
   {
      // rotation_attrib (nome del campo contenente la rotazione dell'oggetto)
      AttrName = TabAlias;
      AttrName += rotation_attrib;
      *pRot = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
   }
   else
      *pRot = NULL;

   // aggr_factor_attrib (nome del campo contenente il fattore di aggregazione dell'oggetto)
   AttrName = TabAlias;
   AttrName += aggr_factor_attrib;
   *pAggrFactor = ColValues.CdrAssoc(AttrName.get_name(), FALSE);

   // layer_attrib (nome del campo contenente il layer dell'oggetto)
   AttrName = TabAlias;
   AttrName += layer_attrib;
   *pLayer = ColValues.CdrAssoc(AttrName.get_name(), FALSE);

   // color_attrib (nome del campo contenente il codice colore dell'oggetto)
   AttrName = TabAlias;
   AttrName += color_attrib;
   *pColor = ColValues.CdrAssoc(AttrName.get_name(), FALSE);

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_POLYLINE || GeomType == TYPE_SURFACE)
   {
      // line_type_attrib (nome del campo contenente il nome del tipo linea dell'oggetto)
      AttrName = TabAlias;
      AttrName += line_type_attrib;
      *pLineType = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // line_type_scale_attrib (nome del campo contenente la scala del tipo linea dell'oggetto)
      AttrName = TabAlias;
      AttrName += line_type_scale_attrib;
      *pLineTypeScale = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
   }
   else
      *pLineType = NULL;

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_POLYLINE || GeomType == TYPE_SURFACE ||
       GeomType == TYPE_TEXT)
   {
      // width_attrib (nome del campo contenente la larghezza o il fattore di larghezza dell'oggetto)
      AttrName = TabAlias;
      AttrName += width_attrib;
      *pWidth  = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
   }
   else
      *pWidth = NULL;

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT)
   {
      // text_attrib (nome del campo contenente il testo dell'oggetto)
      AttrName = TabAlias;
      AttrName += text_attrib;
      *pText = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      if (GeomType == TYPE_TEXT)
         if (*pText == NULL) return GS_BAD; // Obbligatorio

      // text_style_attrib (nome del campo contenente lo stile testo dell'oggetto)
      AttrName = TabAlias;
      AttrName += text_style_attrib;
      *pTextStyle = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // h_text_attrib (nome del campo contenente l'altezza testo dell'oggetto)
      AttrName = TabAlias;
      AttrName += h_text_attrib;
      *pTextHeight = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // horiz_align_attrib (nome del campo contenente l'allineamento orizzontale dell'oggetto)
      AttrName = TabAlias;
      AttrName += horiz_align_attrib;
      *pHorizAlign = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // vert_align_attrib (nome del campo contenente l'allineamento verticale dell'oggetto)
      AttrName = TabAlias;
      AttrName += vert_align_attrib;
      *pVertAlign = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // oblique_angle_attrib (nome del campo contenente l'angolo obliquo del testo dell'oggetto)
      AttrName = TabAlias;
      AttrName += oblique_angle_attrib;
      *pObliqueAng = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
   }
   else
      *pTextStyle = *pTextHeight = *pHorizAlign = *pVertAlign = *pObliqueAng = NULL;

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_NODE)
   {
      // block_attrib (nome del campo contenente il nome del blocco dell'oggetto)
      AttrName = TabAlias;
      AttrName += block_attrib;
      *pBlockName = ColValues.CdrAssoc(AttrName.get_name(), FALSE);

      // block_scale_attrib (nome del campo contenente il fattore di scala dell'oggetto))
      AttrName = TabAlias;
      AttrName += block_scale_attrib;
      *pBlockScale = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
   }
   else
      *pBlockName = *pBlockScale = NULL;

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT || GeomType == TYPE_POLYLINE ||
       GeomType == TYPE_SURFACE)
   {
      // thickness_attrib (nome del campo contenente lo spessore verticale dell'oggetto)
      AttrName = TabAlias;
      AttrName += thickness_attrib;
      *pThickness = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
   }
   else
      *pThickness = NULL;

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_SURFACE)
   {
      // hatch_attrib (nome del campo contenente il nome del riempimento dell'oggetto)
      AttrName = TabAlias;
      AttrName += hatch_attrib;
      *pHatchName = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // hatch_layer_attrib (nome del campo contenente il layer del riempimento dell'oggetto)
      AttrName = TabAlias;
      AttrName += hatch_layer_attrib;
      *pHatchLayer = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // hatch_color_attrib (nome del campo contenente il layer del riempimento dell'oggetto)
      AttrName = TabAlias;
      AttrName += hatch_color_attrib;
      *pHatchColor = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // hatch_scale_attrib (nome del campo contenente il fattore di scala del riempimento dell'oggetto)
      AttrName = TabAlias;
      AttrName += hatch_scale_attrib;
      *pHatchScale = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      // hatch_rotation_attrib (nome del campo contenente il fattore di scala del riempimento dell'oggetto)
      AttrName = TabAlias;
      AttrName += hatch_rotation_attrib;
      *pHatchRot = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
   }
   else
      *pHatchName = *pHatchLayer = *pHatchColor = *pHatchScale = *pHatchRot = NULL;

   if (pLblAttrName)
   {
      // attrib_name_attrib (nome del campo contenente il nome
      // dell'attributo a cui l'etichetta fa riferimento)
      AttrName = TabAlias;
      AttrName += attrib_name_attrib;
      if (!(*pLblAttrName = ColValues.CdrAssoc(AttrName.get_name(), FALSE))) // Obbligatorio
         return GS_BAD;

      if (pLblAttrInvis)
      {
         // attrib_invis_attrib (nome del campo contenente il flag
         // di invisibilità dell'etichetta)
         AttrName = TabAlias;
         AttrName += attrib_invis_attrib;
         *pLblAttrInvis = ColValues.CdrAssoc(AttrName.get_name(), FALSE);
      }
   }

   return GS_GOOD;
}


/*******************************************************************/
/*.doc C_DBGPH_INFO::InitAppendOnAcDbEntityPtrArrayTopo <internal> */
/*+
  Funzione di inizializzazione per la funzione AppendOnAcDbEntityPtrArray
  nel caso di lettura della topologia degli oggetti grafici principali.
  Parametri:
  C_RB_LIST &ColValues;
  presbuf *pInitID;
  presbuf *pFinalID;

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*******************************************************************/
int C_DBGPH_INFO::InitAppendOnAcDbEntityPtrArrayTopo(C_RB_LIST &ColValues,
                                                     presbuf *pInitID, 
                                                     presbuf *pFinalID)
{
   // solo per topologia simulazioni

   // codice entità iniziale
   *pInitID = ColValues.CdrAssoc(_T("TOPO_INIT_ID"), FALSE);
   // codice entità finale
   *pFinalID = ColValues.CdrAssoc(_T("TOPO_FINAL_ID"), FALSE);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::AppendOnAcDbEntityPtrArray  <internal> */
/*+
  Crea degli oggetti acad leggendo dalla tabella della geometria.
  Parametri:
  _RecordsetPtr &pRs;       recordset di dati
  AcDbEntityPtrArray &Ents; Vettore di oggetti acad
  C_INT_LONG_LIST &KeyEnts; Lista di codici identificativi degli oggetti in Ents
                            Ogni elemento della lista contiene un intero e un numero long.
                            Il numero intero identifica il tipo di geometria:
                            > 0 se si tratta di geometria principale. Il numero
                                individua la posizione della geometria quando il
                                campo geometrico è composto da più geometrie (1-based)
                            < 0 se si tratta di blocco DA.
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)
                            
  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::AppendOnAcDbEntityPtrArray(_RecordsetPtr &pRs,
                                             AcDbEntityPtrArray &Ents, 
                                             C_INT_LONG_LIST &KeyEnts, int CounterToVideo)
{
   int          GeomType;
   C_RB_LIST    ColValues;
   presbuf      pKey, pEntKey, pAggrFactor;
   presbuf      pGeom;                // geometria
   presbuf      pX, pY, pZ;           // geometria
   presbuf      pVertParent, pBulge;  // geometria
   presbuf      pRot;
   presbuf      pLayer; 
   presbuf      pColor;
   presbuf      pLineType, pLineTypeScale, pWidth;
   presbuf      pText, pTextStyle, pTextHeight;
   presbuf      pHorizAlign, pVertAlign, pObliqueAng;
   presbuf      pBlockName, pBlockScale;
   presbuf      pThickness;
   presbuf      pHatchName, pHatchLayer;
   presbuf      pHatchColor, pHatchScale, pHatchRot;
   presbuf      pInitID = NULL, pFinalID = NULL;
   AcDbEntity   *pEnt;
   long         ActualKey, i = 0;
   C_CLASS      *pCls = gsc_find_class(prj, cls, sub);
   C_STRING     PrevBlockName, BlockName, PrevTextStyle, TextStyle;
   TCHAR        *pWKTGeom;
   AcDbObjectId BlockId, StyleId;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(482)); // "Ricerca oggetti grafici"
   AcDbEntityPtrArray *pAcDbEntityPtrArray;
   int                j;

   if (gsc_isEOF(pRs) == GS_GOOD) return GS_GOOD;
   if (!pCls) return GS_BAD;

   GeomType = pCls->get_type();

   // Inizializzo la lista di resbuf per lettura
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;

   // Inizializzo i puntatori alla lista di resbuf per lettura
   if (InitAppendOnAcDbEntityPtrArray(GeomType,
                                      ColValues,
                                      &pKey, 
                                      &pEntKey,
                                      &pAggrFactor,
                                      &pGeom,
                                      &pX, &pY, &pZ,
                                      &pVertParent, &pBulge,
                                      &pLayer, &pColor,
                                      &pLineType, &pLineTypeScale, &pWidth,
                                      &pText, &pTextStyle, &pTextHeight,
                                      &pVertAlign, &pHorizAlign, &pObliqueAng,
                                      &pBlockName, &pBlockScale,
                                      &pRot, 
                                      &pThickness,
                                      &pHatchName, &pHatchLayer,
                                      &pHatchColor, &pHatchScale, &pHatchRot) == GS_BAD)
      return GS_BAD;              

   // se si tratta di linee di simulazione
   if (pCls->is_subclass() == GS_GOOD && pCls->get_type() == TYPE_POLYLINE)
      if (InitAppendOnAcDbEntityPtrArrayTopo(ColValues, &pInitID, &pFinalID) == GS_BAD)
         return GS_BAD;

   // inizializzo il flag mPlinegen leggendolo dalla variabile d'ambiente autocad PLINEGEN
   resbuf RbPlinegen;
   if (acedGetVar(_T("PLINEGEN"), &RbPlinegen) == RTNORM) // Read current setting
      gsc_rb2Bool(&RbPlinegen, &mPlinegen);

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.Init(gsc_msg(70), LARGE_STEP); // ogni 1000 "%ld oggetti grafici elaborati"

   if (pGeom) // geometria in un campo geometrico
      // Lettura record
      while (gsc_isEOF(pRs) == GS_BAD)
      {
         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD; 

         // leggo codice dell'oggetto grafico
         if (gsc_rb2Lng(pKey, &ActualKey) == GS_BAD) return GS_BAD;

         // La geometria potrebbe essere nella forma "SRID=4326;POLYGON((0 0,0 1,1 1,1 0,0 0))"
         if ((pWKTGeom = gsc_strstr(pGeom->resval.rstring, _T(";")))) pWKTGeom++;
         else pWKTGeom = pGeom->resval.rstring;

         if (gsc_strstr(pWKTGeom, _T("POINT"), FALSE) == pWKTGeom ||
             gsc_strstr(pWKTGeom, _T("MULTIPOINT"), FALSE) == pWKTGeom)
         {
            if (GeomType == TYPE_NODE ||
                (pBlockName && pBlockName->restype == RTSTR))
            {  // creo un blocco o un multi blocco
               // leggo il nome del blocco
               if (pBlockName && pBlockName->restype == RTSTR)
                  BlockName = pBlockName->resval.rstring;
               else
                  BlockName = pCls->ptr_fas()->block; // valore di default della classe

               if (PrevBlockName.comp(BlockName) != 0)
               {
                  // ricavo l'identificativo del blocco
                  if (gsc_getBlockId(BlockName.get_name(), BlockId) != GS_GOOD)
                     // se non c'è uso il blocco di default della classe
                     if (gsc_getBlockId(pCls->ptr_fas()->block, BlockId) != GS_GOOD)
                        return GS_BAD;
                  PrevBlockName = BlockName;
               }

               if ((pAcDbEntityPtrArray = CreateBlocksFromOpenGis(pEntKey, pAggrFactor, pWKTGeom, 
                                                                  BlockId, pRot, pBlockScale,
                                                                  pLayer, pColor)) == NULL)
                  return GS_BAD;
            }
            else // se si tratta di testi
               if (GeomType == TYPE_TEXT ||
                   (pText && pText->restype == RTSTR))
               {
                  // creo un testo o un multi testo
                  if (pTextStyle && pTextStyle->restype == RTSTR)
                     TextStyle = pTextStyle->resval.rstring;
                  else
                     TextStyle = pCls->ptr_fas()->style; // valore di default della classe

                  if (PrevTextStyle.comp(TextStyle) != 0)
                  {
                     // ricavo l'identificativo dello stile
                     if (gsc_getTextStyleId(TextStyle.get_name(), StyleId) != GS_GOOD)
                        // se non c'è uso lo stile di default della classe
                        if (gsc_getTextStyleId(pCls->ptr_fas()->style, StyleId) != GS_GOOD)
                           return GS_BAD;
                     PrevTextStyle = TextStyle;
                  }

                  if ((pAcDbEntityPtrArray = CreateTextsFromOpenGis(pEntKey, pAggrFactor, 
                                                                    pWKTGeom, StyleId, pText,
                                                                    pRot, pWidth, pLayer, pColor,
                                                                    pTextHeight, pHorizAlign, pVertAlign,
                                                                    pThickness, pObliqueAng)) == NULL)
                     return GS_BAD;
               }
               else
                  // creo un punto o un multipunto (quindi spaghetti)
                  if ((pAcDbEntityPtrArray = CreatePointsFromOpenGis(pEntKey, pAggrFactor, pWKTGeom, 
                                                                     pLayer, pColor,
                                                                     pThickness)) == NULL)
                     return GS_BAD;                 
         }
         if (gsc_strstr(pWKTGeom, _T("LINESTRING"), FALSE) == pWKTGeom ||
             gsc_strstr(pWKTGeom, _T("MULTILINESTRING"), FALSE) == pWKTGeom)
         {
            // creo una polilinea o una multi polilinea
            if ((pAcDbEntityPtrArray = CreatePolylinesFromOpenGis(pEntKey, pAggrFactor, pWKTGeom,
                                                                  pLayer, pColor, 
                                                                  pLineType, pLineTypeScale,
                                                                  pWidth, pThickness,
                                                                  pInitID, pFinalID)) == NULL)
               return GS_BAD;
         }
         else
         if (gsc_strstr(pWKTGeom, _T("POLYGON"), FALSE) == pWKTGeom ||
             gsc_strstr(pWKTGeom, _T("MULTIPOLYGON"), FALSE) == pWKTGeom)
            // creo un poligono o un multipoligono
            if ((pAcDbEntityPtrArray = CreatePolygonFromOpenGis(pEntKey, pAggrFactor,
                                                                pWKTGeom,
                                                                pLayer, pColor, 
                                                                pLineType, pLineTypeScale,
                                                                pWidth, pThickness,
                                                                pHatchName, pHatchLayer,
                                                                pHatchColor, pHatchScale, pHatchRot)) == NULL)
               return GS_BAD;

         if (pAcDbEntityPtrArray)
         {
            for (j = 0; j < pAcDbEntityPtrArray->length(); j++)
            {
               Ents.append(pAcDbEntityPtrArray->at(j));
               KeyEnts.add_tail_int_long(j + 1, ActualKey);
            }
            i += j;
            if (CounterToVideo == GS_GOOD)
               StatusLineMsg.Set(i); // "%ld oggetti grafici elaborati."

            delete pAcDbEntityPtrArray; pAcDbEntityPtrArray = NULL;
         }

         gsc_Skip(pRs);
      }
   else // geometrica in campo numerici con coordinate
   {
      bool Remain;

      // Lettura record
      while (gsc_isEOF(pRs) == GS_BAD)
      {
         i++;
         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(i); // "%ld oggetti grafici elaborati."

         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD; 

         Remain = false;

         // leggo codice entità
         if (gsc_rb2Lng(pKey, &ActualKey) == GS_BAD) return GS_BAD;

         switch (GeomType)
         {
            case TYPE_NODE:
               // leggo il nome del blocco
               if (pBlockName && pBlockName->restype == RTSTR)
                  BlockName = pBlockName->resval.rstring;
               else
                  BlockName = pCls->ptr_fas()->block; // valore di default della classe

               if (PrevBlockName.comp(BlockName) != 0)
               {
                  // ricavo l'identificativo del blocco
                  if (gsc_getBlockId(BlockName.get_name(), BlockId) != GS_GOOD)
                     // se non c'è uso il blocco di default della classe
                     if (gsc_getBlockId(pCls->ptr_fas()->block, BlockId) != GS_GOOD)
                        return GS_BAD;
                  PrevBlockName = BlockName;
               }

               if ((pAcDbEntityPtrArray = CreateBlockFromNumericFields(pEntKey, pAggrFactor, pX, pY,
                                                                       BlockId, pZ, pRot, pBlockScale,
                                                                       pLayer, pColor)) == NULL)
                  return GS_BAD;

               break;
            case TYPE_TEXT:
               // creo un testo
               if (pTextStyle && pTextStyle->restype == RTSTR)
                  TextStyle = pTextStyle->resval.rstring;
               else
                  TextStyle = pCls->ptr_fas()->style; // valore di default della classe

               if (PrevTextStyle.comp(TextStyle) != 0)
               {
                  // ricavo l'identificativo dello stile
                  if (gsc_getTextStyleId(TextStyle.get_name(), StyleId) != GS_GOOD)
                     // se non c'è uso lo stile di default della classe
                     if (gsc_getTextStyleId(pCls->ptr_fas()->style, StyleId) != GS_GOOD)
                        return GS_BAD;
                  PrevTextStyle = TextStyle;
               }

               if ((pAcDbEntityPtrArray = CreateTextFromNumericFields(pEntKey, pAggrFactor, 
                                                                      pX, pY, StyleId, pText, pZ,
                                                                      pRot, pWidth, pLayer, pColor,
                                                                      pTextHeight, pHorizAlign, pVertAlign,
                                                                      pThickness, pObliqueAng)) == NULL)
                  return GS_BAD;

               break;
            case TYPE_POLYLINE:
            {
               long         NextKey, ActualKey;
               C_POINT_LIST PtList;
               C_POINT      *pPt;
               double       Thickness = -1.0, LineTypeScale = -1.0, Width = -1.0; // -1 = da non usare
               C_COLOR      Color;
               C_EED        eed;
               C_STRING     Layer, LineType;

               // Leggo il codice della polilinea
               if (gsc_rb2Lng(pVertParent, &ActualKey) == GS_BAD) return GS_BAD;

               // Leggo le caratteristiche grafiche della polilinea
               if (pEntKey)
                  if (gsc_rb2Lng(pEntKey, &(eed.gs_id)) == GS_BAD) return GS_BAD;

               if (pAggrFactor)
                  if (gsc_rb2Int(pAggrFactor, &(eed.num_el)) == GS_BAD) return GS_BAD;

               // Leggo il colore in base al formato impostato
               if (rb2Color(pColor, Color) == GS_BAD)
                  Color = pCls->ptr_fas()->color; // valore di default della classe

               if (gsc_rb2Dbl(pLineTypeScale, &LineTypeScale) == GS_BAD || LineTypeScale <= 0)
                  LineTypeScale = pCls->ptr_fas()->line_scale; // valore di default della classe

               if (gsc_rb2Dbl(pWidth, &Width) == GS_BAD || Width < 0)
                  Width = pCls->ptr_fas()->width; // valore di default della classe

               if (gsc_rb2Dbl(pThickness, &Thickness) == GS_BAD || Thickness <= 0)
                  Thickness = pCls->ptr_fas()->thickness; // valore di default della classe

               if (pLayer && pLayer->restype == RTSTR)
                  Layer = pLayer->resval.rstring;
               else
                  Layer = pCls->ptr_fas()->layer; // valore di default della classe

               if (pLineType && pLineType->restype == RTSTR)
                  LineType = pLineType->resval.rstring;
               else
                  LineType = pCls->ptr_fas()->line; // valore di default della classe

               pAcDbEntityPtrArray = NULL;
               do
               {
                  // Leggo la geometria da campi numerici
                  if ((pPt = new C_POINT()) == NULL)
                     { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
                  if (gsc_rb2Dbl(pX, &(pPt->point[X])) == GS_BAD) { delete pPt; return GS_BAD; }
                  if (gsc_rb2Dbl(pY, &(pPt->point[Y])) == GS_BAD) { delete pPt; return GS_BAD; }
                  if (gsc_rb2Dbl(pZ, &(pPt->point[Z])) == GS_BAD) pPt->point[Z] = 0.0;
                  if (gsc_rb2Dbl(pBulge, &(pPt->Bulge)) == GS_BAD) pPt->Bulge = 0.0;
                  PtList.add_tail(pPt);
                  
                  gsc_Skip(pRs);
                  if (gsc_isEOF(pRs) == GS_GOOD) { Remain = true; break; }
                  if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD; 
                  // leggo codice della polilinea
                  if (gsc_rb2Lng(pVertParent, &NextKey) == GS_BAD) return GS_BAD;
                  // se si tratta di una polilinea diversa esco dal ciclo
                  if (NextKey != ActualKey) { Remain = true; break; }
               }
               while (1);

               if ((pEnt = PtList.toPolyline(Layer.get_name(),
                                             &Color,
                                             LineType.get_name(),
                                             LineTypeScale, Width, Thickness)) == NULL)
                  return GS_BAD;

               eed.cls = cls;
               eed.sub = sub;

               if (eed.save(pEnt) == GS_BAD)
                  { delete pEnt; return GS_BAD; }

               if ((pAcDbEntityPtrArray = new AcDbEntityPtrArray()) == NULL)
                  { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
               pAcDbEntityPtrArray->append(pEnt);
                 
               break;
            }
         }

         if (pAcDbEntityPtrArray)
         {
            for (j = 0; j < pAcDbEntityPtrArray->length(); j++)
            {
               Ents.append(pAcDbEntityPtrArray->at(j));
               KeyEnts.add_tail_int_long(j + 1, ActualKey);
            }
            i += (j -1);

            delete pAcDbEntityPtrArray; pAcDbEntityPtrArray = NULL;
         }

         if (Remain == false)
            gsc_Skip(pRs);
      }
   }

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.End(gsc_msg(70), i); // "%ld oggetti grafici elaborati."

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::AppendDABlockOnAcDbEntityPtrArray  <internal> */
/*+
  Crea dei blocchi DA per acad leggendo dalla tabella delle label.
  Parametri:
  _RecordsetPtr &pRs;       recordset di dati
  AcDbEntityPtrArray &Ents; Vettore di oggetti acad
  C_INT_LONG_LIST &KeyEnts; Lista di codici identificativi degli oggetti in Ents
                            Ogni elemento della lista contiene un intero e un numero long.
                            Il numero intero identifica il tipo di geometria:
                            1 se si tratta di geometria principale
                            -1 se si tratta di blocco DA.
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::AppendDABlockOnAcDbEntityPtrArray(_RecordsetPtr &pRs,
                                                    AcDbEntityPtrArray &Ents, 
                                                    C_INT_LONG_LIST &KeyEnts, int CounterToVideo)
{
   int        GeomType;
   C_RB_LIST  ColValues;
   presbuf    pRbDummy, pKey, pLblKey;
   presbuf    pEntKey, pParentLblKey, pAggrFactor;
   presbuf    pGeom, pLblGeom;                  // geometria
   presbuf    pX, pY, pZ, pLblX, pLblY, pLblZ;  // geometria
   presbuf    pRot, pLblRot;
   presbuf    pLayer, pLblLayer; 
   presbuf    pColor, pLblColor;
   presbuf    pText, pTextStyle, pTextHeight;
   presbuf    pHorizAlign, pVertAlign, pObliqueAng;
   presbuf    pBlockScale, pLblWidth;
   presbuf    pThickness;
   presbuf    pLblAttrName, pLblAttrInvis;
   AcDbEntity *pEnt;
   long       ActualKey, NextKey, i = 0;
   C_RB_LIST  DefRot, DefScale;
   AcDbObjectId BlockId, StyleId;
   C_CLASS    *pCls = gsc_find_class(prj, cls, sub);
   C_STRING   PrevTextStyle, TextStyle;
   TCHAR      *pWKTGeom, *pWKTLblGeom;
   bool       Remain;
   C_STATUSLINE_MESSAGE StatusLineMsg(gsc_msg(532)); // "Ricerca etichette"
   AcDbEntityPtrArray *pAcDbEntityPtrArray;

   if (gsc_isEOF(pRs) == GS_GOOD) return GS_GOOD;
   if (!pCls) return GS_BAD;

   // ricavo l'identificativo del blocco
   if (gsc_getBlockId(_T("$T"), BlockId) != GS_GOOD) return GS_BAD;

   // se il flag Label = true possono esserci 2 casi:
   // 1) le etichette sono sotto forma di blocchi con attributi in 2 tabelle distinte
   // che sono unite tra loro con una join SQL
   // (la tabella con etichette usa l'alias "LBL.<nome attributo>")
   // 2) le etichette sono sottoforma di testi
   if (LblGroupingTableRef.len() > 0) // caso 1
      GeomType = TYPE_NODE; // label in forma di blocchi DA
   else // caso 2
   {
      GeomType = TYPE_TEXT; // label in forma di testi
     
      DefRot << acutBuildList(RTREAL, CounterClockwiseRadiantToRotationUnit(0.0), 0);
      DefScale << acutBuildList(RTREAL, 1.0, 0);   
   }

   // Inizializzo la lista di resbuf per lettura
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;

   // Inizializzo i puntatori alla lista di resbuf per lettura
   if (InitAppendOnAcDbEntityPtrArray(GeomType,
                                      ColValues,
                                      &pKey, 
                                      &pEntKey,
                                      &pAggrFactor,
                                      &pGeom,
                                      &pX, &pY, &pZ,
                                      &pRbDummy, &pRbDummy,
                                      &pLayer, &pColor,
                                      &pRbDummy, &pRbDummy, &pRbDummy,
                                      &pText, &pTextStyle, &pTextHeight,
                                      &pVertAlign, &pHorizAlign, &pObliqueAng,
                                      &pRbDummy, &pBlockScale,
                                      &pRot, 
                                      &pThickness,
                                      &pRbDummy, &pRbDummy,
                                      &pRbDummy, &pRbDummy, &pRbDummy,
                                      (LblGroupingTableRef.len() == 0) ? &pLblAttrName : NULL) == GS_BAD)
      return GS_BAD;              

   if (LblGroupingTableRef.len() > 0) // lettura label nel caso 1
   {
      // Inizializzo i puntatori alla lista di resbuf per lettura label
      // prendo in considerazione gli attributi dei blocchi DA
      if (InitAppendOnAcDbEntityPtrArray(TYPE_TEXT,
                                         ColValues,
                                         &pLblKey, 
                                         &pParentLblKey,
                                         &pRbDummy,
                                         &pLblGeom,
                                         &pLblX, &pLblY, &pLblZ,
                                         &pRbDummy, &pRbDummy,
                                         &pLblLayer, &pLblColor,
                                         &pRbDummy, &pRbDummy,
                                         &pLblWidth,
                                         &pText, &pTextStyle, &pTextHeight,
                                         &pVertAlign, &pHorizAlign, &pObliqueAng,
                                         &pRbDummy, &pRbDummy,
                                         &pLblRot, 
                                         &pThickness,
                                         &pRbDummy, &pRbDummy,
                                         &pRbDummy, &pRbDummy, &pRbDummy,
                                         &pLblAttrName, &pLblAttrInvis) == GS_BAD)
         return GS_BAD;
   }

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.Init(gsc_msg(70), LARGE_STEP); // ogni 1000 "%ld oggetti grafici elaborati"

   if (pGeom) // geometria in un campo geometrico
      // Lettura record
      while (gsc_isEOF(pRs) == GS_BAD)
      {
         i++;
         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(i); // "%ld oggetti grafici elaborati."

         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD; 

         Remain = false;

         // La geometria potrebbe essere nella forma "SRID=4326;POLYGON((0 0,0 1,1 1,1 0,0 0))"
         if ((pWKTGeom = gsc_strstr(pGeom->resval.rstring, _T(";")))) pWKTGeom++;
         else pWKTGeom = pGeom->resval.rstring;

         if (gsc_strstr(pWKTGeom, _T("POINT")) == pWKTGeom)
         {
            // leggo codice blocco
            if (gsc_rb2Lng(pKey, &ActualKey) == GS_BAD) return GS_BAD;

            // se si tratta di etichette in forma di testo (caso 2)
            if (LblGroupingTableRef.len() == 0)
            {
               if ((pAcDbEntityPtrArray = CreateBlocksFromOpenGis(pEntKey, pAggrFactor,
                                                                  pWKTGeom, BlockId,
                                                                  DefRot.get_head(),
                                                                  DefScale.get_head(),
                                                                  pLayer, pColor)) == NULL)
                  return GS_BAD;
               pEnt = pAcDbEntityPtrArray->first();
               delete pAcDbEntityPtrArray;

               // leggo codice blocco
               if (gsc_rb2Lng(pKey, &ActualKey) == GS_BAD) return GS_BAD;

               if (pTextStyle && pTextStyle->restype == RTSTR)
                  TextStyle = pTextStyle->resval.rstring;
               else
                  TextStyle = pCls->ptr_fas()->style; // valore di default della classe

               if (PrevTextStyle.comp(TextStyle) != 0)
               {
                  // ricavo l'identificativo dello stile
                  if (gsc_getTextStyleId(TextStyle.get_name(), StyleId) != GS_GOOD) return GS_BAD;
                  PrevTextStyle = TextStyle;
               }

               if (AppendAttribFromOpenGis((AcDbBlockReference *) pEnt, pWKTGeom,
                                           StyleId,
                                           pText, pLblAttrName,
                                           pRot, pBlockScale, pLayer, pColor,
                                           pTextHeight, pHorizAlign, pVertAlign,
                                           pThickness, pObliqueAng) != GS_GOOD)
                  return GS_BAD;
            }
            else
            {
               if ((pAcDbEntityPtrArray = CreateBlocksFromOpenGis(pEntKey, pAggrFactor,
                                                                  pWKTGeom, BlockId,
                                                                  pRot, pBlockScale,
                                                                  pLayer, pColor)) == NULL)
                  return GS_BAD;
               pEnt = pAcDbEntityPtrArray->first();
               delete pAcDbEntityPtrArray;

               // Lettura record attributi di blocco
               do
               {
                  if (pTextStyle && pTextStyle->restype == RTSTR)
                     TextStyle = pTextStyle->resval.rstring;
                  else
                     TextStyle = pCls->ptr_fas()->style; // valore di default della classe

                  if (PrevTextStyle.comp(TextStyle) != 0)
                  {
                     // ricavo l'identificativo dello stile
                     if (gsc_getTextStyleId(TextStyle.get_name(), StyleId) != GS_GOOD) return GS_BAD;
                     PrevTextStyle = TextStyle;
                  }

                  if (pLblGeom->restype != RTSTR) pWKTLblGeom = NULL;
                  else
                     // La geometria potrebbe essere nella forma "SRID=4326;POLYGON((0 0,0 1,1 1,1 0,0 0))"
                     if ((pWKTLblGeom = gsc_strstr(pLblGeom->resval.rstring, _T(";")))) pWKTLblGeom++;
                     else pWKTLblGeom = pLblGeom->resval.rstring;

                  if (AppendAttribFromOpenGis((AcDbBlockReference *) pEnt, pWKTLblGeom,
                                              StyleId,
                                              pText, pLblAttrName,  
                                              pLblRot, pLblWidth,
                                              pLblLayer, pLblColor,
                                              pTextHeight, pHorizAlign, pVertAlign,
                                              pThickness, pObliqueAng,
                                              pLblAttrInvis) != GS_GOOD)
                     return GS_BAD;

                  gsc_Skip(pRs);
                  if (gsc_isEOF(pRs) == GS_GOOD) { Remain = true; break; }
                  if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD; 
                  // leggo codice blocco
                  if (gsc_rb2Lng(pKey, &NextKey) == GS_BAD) return GS_BAD;
                  // se si tratta di un blocco diverso esco dal ciclo
                  if (NextKey != ActualKey) { Remain = true; break; }
               }
               while (1);
            }

            Ents.append(pEnt);
            KeyEnts.add_tail_int_long(-1, ActualKey);
         }

         if (Remain == false)
            gsc_Skip(pRs);
      }
   else // geometrica in campo numerici con coordinate
      // Lettura record
      while (gsc_isEOF(pRs) == GS_BAD)
      {
         i++;
         if (CounterToVideo == GS_GOOD)
            StatusLineMsg.Set(i); // "%ld oggetti grafici elaborati."

         if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD; 

         gsc_Skip(pRs);
      }

   if (CounterToVideo == GS_GOOD)
      StatusLineMsg.End(gsc_msg(70), i); // "%ld oggetti grafici elaborati."

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreateGphTable           <external> */
/*+
  Crea la tabella della geometria.
  Parametri:
  C_STRING &MyTableRef; Riferimento completo alla tabella
  bool Label;           Flag, se = vero viene creata la tabella della geometria
                        etichette.
  int GeomType;         Opzionale, Usato per stabilire il tipo di dato geometrico
                        se non si sta creando una tabella geometrica per le etichette
                        (TYPE_POLYLINE, TYPE_TEXT, TYPE_NODE, TYPE_SURFACE, TYPE_SPAGHETTI)
                        Default = TYPE_TEXT

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::CreateGphTable(C_STRING &MyTableRef, bool Label, int GeomType)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_RB_LIST List;
   long      LongType_Len, DoubleType_Len, CharType_Len;
   long      BoolType_Len, SmallIntType_Len;
   int       LongType_Dec, DoubleType_Dec, CharType_Dec;
   int       BoolType_Dec, SmallIntType_Dec;
   C_STRING  LongType_ProviderDescr, DoubleType_ProviderDescr, CharType_ProviderDescr;
   C_STRING  BoolType_ProviderDescr, SmallIntType_ProviderDescr;
   C_STRING  Catalog, Schema, Name, IndexRef;

   if (Label) GeomType = TYPE_TEXT;

   // converto campo da codice ADO in Provider dipendente (numero intero lungo)
   if (pConn->Type2ProviderType(CLASS_KEY_TYPE_ENUM,    // DataType per campo chiave
                                FALSE,                  // IsLong
                                FALSE,                  // IsFixedPrecScale
                                RTT,                    // IsFixedLength
                                TRUE,                   // IsWrite
                                CLASS_LEN_KEY_ATTR,     // Size
                                0,                      // Prec
                                LongType_ProviderDescr, // ProviderDescr
                                &LongType_Len, &LongType_Dec) == GS_BAD)
      return GS_BAD;
   // converto campo da codice ADO in Provider dipendente (numero reale)
   if (pConn->Type2ProviderType(adDouble,                        // DataType
                                FALSE,                           // IsLong
                                FALSE,                           // IsFixedPrecScale
                                RTT,                             // IsFixedLength
                                TRUE,                            // IsWrite
                                ACCESS_MAX_LEN_FIELDNUMERIC,     // Size
                                ACCESS_MAX_LEN_FIELDNUMERIC_DEC, // Prec
                                DoubleType_ProviderDescr, // ProviderDescr
                                &DoubleType_Len, &DoubleType_Dec) == GS_BAD)
      return GS_BAD;

   C_PROVIDER_TYPE *providerType;
   if ((providerType = pConn->getCharProviderType()) == NULL) return GS_BAD;

   // converto campo da codice ADO in Provider dipendente (carattere)
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
   // converto campo da codice ADO in Provider dipendente (boolean)
   if (pConn->Type2ProviderType(adBoolean,              // DataType
                                FALSE,                  // IsLong
                                FALSE,                  // IsFixedPrecScale
                                RTT,                    // IsFixedLength
                                TRUE,                   // IsWrite
                                1,                      // Size
                                0,                      // Prec
                                BoolType_ProviderDescr, // ProviderDescr
                                &BoolType_Len, &BoolType_Dec) == GS_BAD)
      return GS_BAD;
   // converto campo da codice ADO in Provider dipendente (small int)
   if (pConn->Type2ProviderType(adSmallInt,             // DataType
                                FALSE,                  // IsLong
                                FALSE,                  // IsFixedPrecScale
                                RTT,                    // IsFixedLength
                                TRUE,                   // IsWrite
                                1,                      // Size
                                0,                      // Prec
                                SmallIntType_ProviderDescr, // ProviderDescr
                                &SmallIntType_Len, &SmallIntType_Dec) == GS_BAD)
      return GS_BAD;


   if ((List << acutBuildList(RTLB, 0)) == NULL) return GS_BAD;

   // key_attrib (nome del campo chiave dell'oggetto grafico)
   if ((List += acutBuildList(RTLB,
                              RTSTR, key_attrib.get_name(), 
                              RTSTR, LongType_ProviderDescr.get_name(),
                              RTLONG, LongType_Len, RTSHORT, LongType_Dec,
                              RTLE, 0)) == NULL) return GS_BAD;

   if (GeomType != TYPE_SPAGHETTI)
      // ent_key_attrib (nome del campo contenente il codice dell'entita' di appartenenza)
      if ((List += acutBuildList(RTLB,
                                 RTSTR, ent_key_attrib.get_name(), 
                                 RTSTR, LongType_ProviderDescr.get_name(),
                                 RTLONG, LongType_Len, RTSHORT, LongType_Dec,
                                 RTLE, 0)) == NULL) return GS_BAD;

   if (Label) // si sta creando una tabella delle etichette
   {
      // attrib_name_attrib (nome del campo contenente il nome
      // dell'attributo a cui l'etichetta fa riferimento)
      if ((List += acutBuildList(RTLB,
                                 RTSTR, attrib_name_attrib.get_name(), 
                                 RTSTR, CharType_ProviderDescr.get_name(),
                                 RTLONG, MAX_LEN_FIELDNAME, RTSHORT, CharType_Dec,
                                 RTLE, 0)) == NULL) return GS_BAD;
      // attrib_invis_attrib (nome del campo contenente il flag di invisibilità
      // dell'etichetta)
      if ((List += acutBuildList(RTLB,
                                 RTSTR, attrib_invis_attrib.get_name(), 
                                 RTSTR, BoolType_ProviderDescr.get_name(),
                                 RTLONG, BoolType_Len, RTSHORT, BoolType_Dec,
                                 RTLE, 0)) == NULL) return GS_BAD;
   }

   if (geom_attrib.len() == 0)
   {
      // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
      if ((List += acutBuildList(RTLB,
                                 RTSTR, x_attrib.get_name(), 
                                 RTSTR, DoubleType_ProviderDescr.get_name(),
                                 RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                 RTLE, 0)) == NULL) return GS_BAD;
      if ((List += acutBuildList(RTLB,
                                 RTSTR, y_attrib.get_name(),
                                 RTSTR, DoubleType_ProviderDescr.get_name(),
                                 RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                 RTLE, 0)) == NULL) return GS_BAD;
      if (geom_dim == GS_3D && z_attrib.len() > 0) // 3D
         if ((List += acutBuildList(RTLB,
                                    RTSTR, z_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_POLYLINE || 
          GeomType == TYPE_SURFACE)
      {
         // vertex_parent_attrib (nome del campo contenente un codice per 
         //                       raggruppare i vertici per comporre una linea)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, vertex_parent_attrib.get_name(), 
                                    RTSTR, LongType_ProviderDescr.get_name(),
                                    RTLONG, LongType_Len, RTSHORT, LongType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

         if (bulge_attrib.len() > 0) 
            // bulge_attrib (Nome del campo contenente il bulge tra un
            // vertice e l'altro dell'oggetto grafico lineare)
            if ((List += acutBuildList(RTLB,
                                       RTSTR, bulge_attrib.get_name(), 
                                       RTSTR, DoubleType_ProviderDescr.get_name(),
                                       RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                       RTLE, 0)) == NULL) return GS_BAD;
      }
   }

   if (rotation_attrib.len() > 0 &&
      (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT || GeomType == TYPE_NODE))
      // rotation_attrib (nome del campo contenente la rotazione dell'oggetto)
      if ((List += acutBuildList(RTLB,
                                 RTSTR, rotation_attrib.get_name(), 
                                 RTSTR, DoubleType_ProviderDescr.get_name(),
                                 RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                 RTLE, 0)) == NULL) return GS_BAD;

   // se non si tratta di tabella per le etichette e non si tratta di spaghetti
   if (!Label && GeomType != TYPE_SPAGHETTI)
      // aggr_factor_attrib (nome del campo contenente il fattore di aggregazione dell'oggetto)
      if (aggr_factor_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, aggr_factor_attrib.get_name(), 
                                    RTSTR, SmallIntType_ProviderDescr.get_name(),
                                    RTLONG, SmallIntType_Len, RTSHORT, SmallIntType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

   // layer_attrib (nome del campo contenente il layer dell'oggetto)
   if (layer_attrib.len() > 0)
      if ((List += acutBuildList(RTLB,
                                 RTSTR, layer_attrib.get_name(), 
                                 RTSTR, CharType_ProviderDescr.get_name(),
                                 RTLONG, CharType_Len, RTSHORT, CharType_Dec,
                                 RTLE, 0)) == NULL) return GS_BAD;

   // color_attrib (nome del campo contenente il codice colore dell'oggetto)
   if (color_attrib.len() > 0)
      // Il colore va espresso in formato carattere di 11
      if ((List += acutBuildList(RTLB,
                                 RTSTR, color_attrib.get_name(), 
                                 RTSTR, CharType_ProviderDescr.get_name(),
                                 RTLONG, 11, RTSHORT, 0,
                                 RTLE, 0)) == NULL) return GS_BAD;

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_POLYLINE || GeomType == TYPE_SURFACE)
   {
      // line_type_attrib (nome del campo contenente il nome del tipo linea dell'oggetto)
      if (line_type_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, line_type_attrib.get_name(), 
                                    RTSTR, CharType_ProviderDescr.get_name(),
                                    RTLONG, CharType_Len, RTSHORT, CharType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // line_type_scaleattrib (nome del campo contenente la scala del tipo linea dell'oggetto)
      if (line_type_scale_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, line_type_scale_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;
   }

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_POLYLINE || GeomType == TYPE_SURFACE ||
       GeomType == TYPE_TEXT)
   {
      // width_attrib (nome del campo contenente la larghezza o il fattore di larghezza dell'oggetto)
      if (width_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, width_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;
   }

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT)
   {
      // text_attrib (nome del campo contenente il testo dell'oggetto)
      if ((List += acutBuildList(RTLB,
                                 RTSTR, text_attrib.get_name(), 
                                 RTSTR, CharType_ProviderDescr.get_name(),
                                 RTLONG, CharType_Len, RTSHORT, CharType_Dec,
                                 RTLE, 0)) == NULL) return GS_BAD;

      // text_style_attrib (nome del campo contenente lo stile testo dell'oggetto)
      if (text_style_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, text_style_attrib.get_name(), 
                                    RTSTR, CharType_ProviderDescr.get_name(),
                                    RTLONG, CharType_Len, RTSHORT, CharType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // h_text_attrib (nome del campo contenente l'altezza testo dell'oggetto)
      if (h_text_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, h_text_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // horiz_align_attrib (nome del campo contenente l'allineamento orizzontale dell'oggetto)
      if (horiz_align_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, horiz_align_attrib.get_name(), 
                                    RTSTR, SmallIntType_ProviderDescr.get_name(),
                                    RTLONG, SmallIntType_Len, RTSHORT, SmallIntType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // vert_align_attrib (nome del campo contenente l'allineamento verticale dell'oggetto)
      if (vert_align_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, vert_align_attrib.get_name(), 
                                    RTSTR, SmallIntType_ProviderDescr.get_name(),
                                    RTLONG, SmallIntType_Len, RTSHORT, SmallIntType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // oblique_angle_attrib (nome del campo contenente l'angolo obliquo del testo dell'oggetto)
      if (oblique_angle_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, oblique_angle_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;
   }

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_NODE)
   {
      // block_attrib (nome del campo contenente il nome del blocco dell'oggetto)
      if (block_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, block_attrib.get_name(), 
                                    RTSTR, CharType_ProviderDescr.get_name(),
                                    RTLONG, CharType_Len, RTSHORT, CharType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // block_scale_attrib (nome del campo contenente il fattore di scala dell'oggetto)
      if (block_scale_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, block_scale_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;
   }

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_TEXT || GeomType == TYPE_POLYLINE ||
       GeomType == TYPE_SURFACE)
   {
      // thickness_attrib (nome del campo contenente lo spessore verticale dell'oggetto)
      if (thickness_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, thickness_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;
   }

   if (GeomType == TYPE_SPAGHETTI || GeomType == TYPE_SURFACE)
   {
      // hatch_attrib (nome del campo contenente il nome del riempimento dell'oggetto)
      if (hatch_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, hatch_attrib.get_name(), 
                                    RTSTR, CharType_ProviderDescr.get_name(),
                                    RTLONG, CharType_Len, RTSHORT, CharType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // hatch_layer_attrib (nome del campo contenente il layer del riempimento dell'oggetto)
      if (hatch_layer_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, hatch_layer_attrib.get_name(), 
                                    RTSTR, CharType_ProviderDescr.get_name(),
                                    RTLONG, CharType_Len, RTSHORT, CharType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;
   
      // hatch_color_attrib (nome del campo contenente il codice colore del riempimento dell'oggetto)
      if (hatch_color_attrib.len() > 0)
         // Il colore va espresso in formato carattere di 11
         if ((List += acutBuildList(RTLB,
                                    RTSTR, hatch_color_attrib.get_name(), 
                                    RTSTR, CharType_ProviderDescr.get_name(),
                                    RTLONG, 11, RTSHORT, 0,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // hatch_scale_attrib (nome del campo contenente il fattore di scala del riempimento dell'oggetto)
      if (hatch_scale_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, hatch_scale_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;

      // hatch_rotation_attrib (nome del campo contenente la rotazione del riempimento dell'oggetto)
      if (hatch_rotation_attrib.len() > 0)
         if ((List += acutBuildList(RTLB,
                                    RTSTR, hatch_rotation_attrib.get_name(), 
                                    RTSTR, DoubleType_ProviderDescr.get_name(),
                                    RTLONG, DoubleType_Len, RTSHORT, DoubleType_Dec,
                                    RTLE, 0)) == NULL) return GS_BAD;
   }

   if (pConn->CreateTable(MyTableRef.get_name(), List.get_head()) == GS_BAD)
      return GS_BAD;

   // se la geometria è memorizzata in un campo geometrico
   // lo aggiungo a parte con una istruzione SQL dedicata
   // infatti alcuni provider non consentono di creare 
   // campi geometrici tramite l'istruzione "CREATE TABLE"
   if (geom_attrib.len() > 0)
      if (getDBConnection()->AddGeomField(MyTableRef.get_name(),
                                          geom_attrib.get_name(),
                                          coordinate_system.get_name(),
                                          GeomType, geom_dim) == GS_BAD)
         { pConn->DelTable(MyTableRef.get_name()); return GS_BAD; }

   // Creo indici
   if (ReindexTable(MyTableRef) == GS_BAD)
      { pConn->DelTable(MyTableRef.get_name()); return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreateBlocksFromOpenGis   <internal> */
/*+
  Crea uno o più BLOCCHI a partire da una geometria espressa in formato OpenGis.
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  presbuf pGeom;     Puntatore a resbuf contenente la geometria in formato
                     OpenGis WKT (Well Know Text)
  AcDbObjectId &BlockId; Id del blocco (viene letto prima per ottimizzare le prestazioni)
  presbuf pRot;      Puntatore a resbuf contenente la rotazione (default = NULL)
  presbuf pBlockScale; Puntatore a resbuf contenente la fattore scala (default = NULL)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreateBlocksFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                                          TCHAR *pWKTGeom,
                                                          AcDbObjectId &BlockId,
                                                          presbuf pRot, presbuf pBlockScale,
                                                          presbuf pLayer, presbuf pColor)
{
   // esempio: POINT(0 0)
   // esempio: MULTIPOINT(40 40,20 45,45 30)
   C_POINT_LIST PtList;

   if ((pWKTGeom = wcschr(pWKTGeom, _T('('))) == NULL) // cerco la parentesi aperta
      return NULL;
   // Il separatore tra punti = ',' quello tra coordinate = ' '
   if (PtList.fromStr(pWKTGeom + 1, geom_dim, false, _T(','), _T(' ')) == GS_BAD)
      return NULL;

   return CreateBlocksFromDB(pEntKey, pAggrFactor, PtList, BlockId, pRot, pBlockScale,
                            pLayer, pColor);
}


/*****************************************************************************/
/*.doc C_DBGPH_INFO::CreateBlockFromNumericFields <internal>                 */
/*+
  Crea un BLOCCO a partire da una geometria espressa in formato di campi numerici,
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  presbuf pX;        Puntatore a resbuf contenente la coordinata X
  presbuf pY;        Puntatore a resbuf contenente la coordinata Y
  AcDbObjectId &BlockId; Id del blocco (viene letto prima per ottimizzare le prestazioni)
  presbuf pZ;        Puntatore a resbuf contenente la coordinata Z (default = NULL)
  presbuf pRot;      Puntatore a resbuf contenente la rotazione (default = NULL)
  presbuf pBlockScale; Puntatore a resbuf contenente la fattore scala (default = NULL)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*****************************************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreateBlockFromNumericFields(presbuf pEntKey, presbuf pAggrFactor,
                                                               presbuf pX, presbuf pY,
                                                               AcDbObjectId &BlockId,
                                                               presbuf pZ, presbuf pRot, presbuf pBlockScale,
                                                               presbuf pLayer, presbuf pColor)
{
   ads_point    Pt;
   C_POINT_LIST PtList;

   // Leggo la geometria da campi numerici
   if (gsc_rb2Dbl(pX, &(Pt[X])) == GS_BAD) return GS_BAD;
   if (gsc_rb2Dbl(pY, &(Pt[Y])) == GS_BAD) return GS_BAD;
   if (gsc_rb2Dbl(pZ, &(Pt[Z])) == GS_BAD) Pt[Z] = 0.0;
   PtList.add_point(Pt);

   return CreateBlocksFromDB(pEntKey, pAggrFactor, PtList, BlockId, pRot, pBlockScale,
                             pLayer, pColor);
}


/*****************************************************************************/
/*.doc C_DBGPH_INFO::CreateBlockFromDB <internal>                            */
/*+
  Crea uno o più BLOCCHI a partire da una lettura da DB.
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  C_POINT_LIST &PtList; Lista dei punti
  AcDbObjectId &BlockId; Id del blocco (viene letto prima per ottimizzare le prestazioni)
  presbuf pRot;      Puntatore a resbuf contenente la rotazione (default = NULL)
  presbuf pBlockScale; Puntatore a resbuf contenente la fattore scala (default = NULL)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)


  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*****************************************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreateBlocksFromDB(presbuf pEntKey, presbuf pAggrFactor,
                                                     C_POINT_LIST &PtList, AcDbObjectId &BlockId,
                                                     presbuf pRot, presbuf pBlockScale,
                                                     presbuf pLayer, presbuf pColor)
{
   AcDbBlockReference *pEnt;
   double             Rot = 0.0, BlockScale = 1.0;
   C_COLOR            Color;
   C_EED              eed;
   C_CLASS            *pCls = gsc_find_class(prj, cls, sub);
   C_POINT            *pPt;
   AcDbEntityPtrArray *pEntArray;
   bool                err = false;

   if (!pCls) return GS_BAD;

   if (pEntKey) // gli spaghetti non hanno il gs_id
      if (gsc_rb2Lng(pEntKey, &(eed.gs_id)) == GS_BAD) return NULL;

   if (pAggrFactor)
      if (gsc_rb2Int(pAggrFactor, &(eed.num_el)) == GS_BAD) return NULL;

   if (gsc_rb2Dbl(pRot, &Rot) == GS_BAD)
      Rot = pCls->ptr_fas()->rotation; // valore di default della classe
   else
      // Converto la rotazione in gradi in senso antiorario (per Autocad)
      Rot = ToCounterClockwiseDegreeUnit(Rot);

   if (gsc_rb2Dbl(pBlockScale, &BlockScale) == GS_BAD || BlockScale <= 0)
      BlockScale = pCls->ptr_fas()->block_scale; // valore di default della classe

   if ((pEntArray = new AcDbEntityPtrArray()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   eed.cls = cls;
   eed.sub = sub;
   pPt = (C_POINT *) PtList.get_head();
   while (pPt)
   {
      if ((pEnt = gsc_create_block(BlockId, pPt->point, 
                                   BlockScale, BlockScale, Rot)) == NULL)
         { err = true; break; }

      if (pLayer && pLayer->restype == RTSTR)
      {
         if (gsc_setLayer(pEnt, pLayer->resval.rstring) != GS_GOOD)
            { delete pEnt; err = true; break; }
      }
      else
         // valore di default della classe
         if (gsc_setLayer(pEnt, pCls->ptr_fas()->layer) != GS_GOOD)
            { delete pEnt; err = true; break; }

      if (rb2Color(pColor, Color) == GS_BAD)
         Color = pCls->ptr_fas()->color; // valore di default della classe

      if (gsc_set_color(pEnt, Color) == GS_BAD)
         { delete pEnt;err = true; break; }

      if (eed.save(pEnt) == GS_BAD)
         { err = true; break; }
      pEntArray->append(pEnt);

      pPt = (C_POINT *) PtList.get_next();
   }

   if (err)
   {
      for (int i = 0; i < pEntArray->length(); i++)
         delete ((AcDbEntity *) pEntArray->at(i));
      pEntArray->removeAll();
      return NULL;
   }

   return pEntArray;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::AppendAttribFromOpenGis   <internal> */
/*+
  Aggiunge un ATTRIBUTO ad un blocco a partire da una geometria espressa in formato
  OpenGis.
  Parametri:
  AcDbBlockReference *pEnt;  Puntatore a entità blocco precedentemente creata
  TCHAR *pWKTGeom;     Puntatore alla geometria in formato OpenGis WKT (Well Know Text)
  AcDbObjectId &StyleId; Id dello stile (viene letto prima per ottimizzare le prestazioni)
  presbuf pText;     Testo dell'attributo
  presbuf pAttrName; Nome dell'attributo
  presbuf pRot;      Puntatore a resbuf contenente la rotazione (default = NULL)
  presbuf pWidth;    Puntatore a resbuf contenente il fattore di larghezza (default = NULL)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pTextHeight; Puntatore a resbuf contenente l'altezza testo (default = NULL)
  presbuf pHorizAlign; Puntatore a resbuf contenente il flag di generazione 
                       orizzontale (default = NULL)
  presbuf pVertAlign;  Puntatore a resbuf contenente il flag di generazione 
                       verticale (default = NULL)
  presbuf pThickness;  Puntatore a resbuf contenente lo spessore verticale (default = NULL)
  presbuf pObliqueAng; Puntatore a resbuf contenente l'angolo obliquo (default = NULL)
  presbuf pAttrInvis;  Puntatore a resbuf contenente il flag di invisibilità
                       (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::AppendAttribFromOpenGis(AcDbBlockReference *pEnt, TCHAR *pWKTGeom,
                                          AcDbObjectId &StyleId,
                                          presbuf pText, presbuf pAttrName,
                                          presbuf pRot, presbuf pWidth,
                                          presbuf pLayer, presbuf pColor,
                                          presbuf pTextHeight,
                                          presbuf pHorizAlign, presbuf pVertAlign,
                                          presbuf pThickness, presbuf pObliqueAng,
                                          presbuf pAttrInvis)
{
   AcDbAttribute  *pAtt;
   C_POINT        Pt;
   double         Height = 1.0, Rot = 0.0, Thickness = 0.0, Width = 1.0, ObliqueAng = 0.0;
   int            HorizAlign, VertAlign, dummy;
   C_COLOR        Color;
   Adesk::Boolean Invisible = kFalse;
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   C_STRING       TextValue;
   TCHAR          *pStart;

   if (!pCls) return GS_BAD;

   // Caso in cui esiste la coordinata del blocco $T che però è senza attributi e quindi pWKTGeom è vuoto
   if (pWKTGeom == NULL) return GS_GOOD;

   if ((pStart = gsc_strstr(pWKTGeom, _T("("))) == NULL) return GS_BAD;
   // Il separatore tra coordinate è ' '
   if (Pt.fromStr(pStart + 1, _T(' ')) == GS_BAD) return GS_BAD;

   TextValue = (pText->restype != RTSTR) ? _T("") : pText->resval.rstring;

   if (pAttrName->restype != RTSTR) return GS_BAD;

   if (gsc_rb2Dbl(pRot, &Rot) == GS_BAD)
      Rot = pCls->ptr_fas()->rotation; // valore di default della classe
   else
      // Converto la rotazione in gradi in senso antiorario (per Autocad)
      Rot = ToCounterClockwiseDegreeUnit(Rot);

   if (gsc_rb2Dbl(pWidth, &Width) == GS_BAD || Width <= 0) Width = 1.0;

   if (gsc_rb2Dbl(pTextHeight, &Height) == GS_BAD || Height <= 0)
      Height = pCls->ptr_fas()->h_text; // valore di default della classe

   if (gsc_rb2Dbl(pThickness, &Thickness) == GS_BAD || Thickness < 0)
      Thickness = pCls->ptr_fas()->thickness; // valore di default della classe

   if (gsc_rb2Dbl(pObliqueAng, &ObliqueAng) == GS_BAD) ObliqueAng = 0.0;

   if (gsc_rb2Int(pHorizAlign, &HorizAlign) == GS_BAD) HorizAlign = kTextLeft;

   if (gsc_rb2Int(pVertAlign, &VertAlign) == GS_BAD) VertAlign = kTextBase;

   if ((pAtt = gsc_create_attribute(TextValue.get_name(), pAttrName->resval.rstring,
                                    Pt.point, 
                                    Height, Rot, StyleId, 
                                    Thickness, Width, ObliqueAng,
                                    (TextHorzMode) HorizAlign, (TextVertMode) VertAlign)) == NULL)
      return GS_BAD;

   if (pLayer && pLayer->restype == RTSTR)
   {
      if (gsc_setLayer(pAtt, pLayer->resval.rstring) != GS_GOOD)
         { delete pAtt; return GS_BAD; }
   }
   else
      if (gsc_setLayer(pAtt, pCls->ptr_fas()->layer) != GS_GOOD) // valore di default della classe
         { delete pAtt; return GS_BAD; }

   if (rb2Color(pColor, Color) == GS_BAD)
      Color = pCls->ptr_fas()->color; // valore di default della classe
   gsc_set_color(pAtt, Color);

   if (pAttrInvis)
   {
      if (pAttrInvis->restype == RTT) Invisible = kTrue;
      else 
         if (gsc_rb2Int(pAttrInvis, &dummy) == GS_GOOD && dummy != 0)
            Invisible = kTrue;

      if (Invisible == kTrue)
      {
         Acad::ErrorStatus Res = pAtt->upgradeOpen();

         if (Res != Acad::eOk && Res != Acad::eWasOpenForWrite)
            { GS_ERR_COD = eGSInvGraphObjct; delete pAtt; return GS_BAD; }
         pAtt->setInvisible(Invisible);
         pAtt->close();
      }
   }

   pEnt->appendAttribute(pAtt);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreateTextFromOpenGis    <internal> */
/*+
  Crea un o più TESTO a partire da una geometria espressa in formato OpenGis.
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  TCHAR *pWKTGeom;     Puntatore alla geometria in formato OpenGis WKT (Well Know Text)
  AcDbObjectId &StyleId; Id dello stile (viene letto prima per ottimizzare le prestazioni)
  presbuf pText;     Puntatore a resbuf contenente il testo
  presbuf pRot;      Puntatore a resbuf contenente la rotazione (default = NULL)
  presbuf pWidth;    Puntatore a resbuf contenente il fattore di larghezza (default = NULL)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pTextHeight; Puntatore a resbuf contenente l'altezza testo (default = NULL)
  presbuf pHorizAlign; Puntatore a resbuf contenente il flag di generazione 
                       orizzontale (default = NULL)
  presbuf pVertAlign;  Puntatore a resbuf contenente il flag di generazione 
                       verticale (default = NULL)
  presbuf pThickness;  Puntatore a resbuf contenente lo spessore verticale (default = NULL)
  presbuf pObliqueAng; Puntatore a resbuf contenente l'angolo obliquo (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreateTextsFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                                         TCHAR *pWKTGeom,
                                                         AcDbObjectId &StyleId, presbuf pText,
                                                         presbuf pRot, presbuf pWidth,
                                                         presbuf pLayer, presbuf pColor,
                                                         presbuf pTextHeight,
                                                         presbuf pHorizAlign, presbuf pVertAlign,
                                                         presbuf pThickness, presbuf pObliqueAng)
{
   // esempio: POINT(0 0)
   // esempio: MULTIPOINT(40 40,20 45,45 30)
   C_POINT_LIST PtList;

   if ((pWKTGeom = wcschr(pWKTGeom, _T('('))) == NULL) // cerco la parentesi aperta
      return NULL;
   // Il separatore tra punti = ',' quello tra coordinate = ' '
   if (PtList.fromStr(pWKTGeom + 1, geom_dim, false, _T(','), _T(' ')) == GS_BAD)
      return NULL;

   return CreateTextsFromDB(pEntKey, pAggrFactor, PtList, StyleId, pText,
                            pRot, pWidth, pLayer, pColor,
                            pTextHeight, pHorizAlign, pVertAlign,
                            pThickness, pObliqueAng);
}

/*****************************************************************************/
/*.doc C_DBGPH_INFO::CreateTextFromNumericFields    <internal> */
/*+
  Crea un TESTO a partire da una geometria espressa da campi numerici.
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  presbuf pX;        Puntatore a resbuf contenente la coordinata X
  presbuf pY;        Puntatore a resbuf contenente la coordinata Y
  AcDbObjectId &StyleId; Id dello stile (viene letto prima per ottimizzare le prestazioni)
  presbuf pText;     Puntatore a resbuf contenente il testo
  presbuf pZ;        Puntatore a resbuf contenente la coordinata Z (default = NULL)
  presbuf pRot;      Puntatore a resbuf contenente la rotazione (default = NULL)
  presbuf pWidth;    Puntatore a resbuf contenente il fattore di larghezza (default = NULL)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pTextHeight; Puntatore a resbuf contenente l'altezza testo (default = NULL)
  presbuf pHorizAlign; Puntatore a resbuf contenente il flag di generazione 
                       orizzontale (default = NULL)
  presbuf pVertAlign;  Puntatore a resbuf contenente il flag di generazione 
                       verticale (default = NULL)
  presbuf pThickness;  Puntatore a resbuf contenente lo spessore verticale (default = NULL)
  presbuf pObliqueAng; Puntatore a resbuf contenente l'angolo obliquo (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*****************************************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreateTextFromNumericFields(presbuf pEntKey, presbuf pAggrFactor,
                                                              presbuf pX, presbuf pY, 
                                                              AcDbObjectId &StyleId, presbuf pText,
                                                              presbuf pZ, presbuf pRot, presbuf pWidth,
                                                              presbuf pLayer, presbuf pColor,
                                                              presbuf pTextHeight,
                                                              presbuf pHorizAlign, presbuf pVertAlign,
                                                              presbuf pThickness, presbuf pObliqueAng)
{
   ads_point    Pt;
   C_POINT_LIST PtList;

   // Leggo la geometria da campi numerici
   if (gsc_rb2Dbl(pX, &(Pt[X])) == GS_BAD) return NULL;
   if (gsc_rb2Dbl(pY, &(Pt[Y])) == GS_BAD) return NULL;
   if (gsc_rb2Dbl(pZ, &(Pt[Z])) == GS_BAD) Pt[Z] = 0.0;
   PtList.add_point(Pt);

   return CreateTextsFromDB(pEntKey, pAggrFactor, PtList, StyleId, pText,
                            pRot, pWidth, pLayer, pColor,
                            pTextHeight, pHorizAlign, pVertAlign,
                            pThickness, pObliqueAng);
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreateTextsFromDB    <internal> */
/*+
  Crea uno o più TESTI a partire da una lettura da DB.
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  C_POINT_LIST &PtList; Lista dei punti
  AcDbObjectId &StyleId; Id dello stile (viene letto prima per ottimizzare le prestazioni)
  presbuf pText;     Puntatore a resbuf contenente il testo (default = NULL)
  presbuf pRot;      Puntatore a resbuf contenente la rotazione (default = NULL)
  presbuf pWidth;    Puntatore a resbuf contenente il fattore di larghezza (default = NULL)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pTextHeight; Puntatore a resbuf contenente l'altezza testo (default = NULL)
  presbuf pHorizAlign; Puntatore a resbuf contenente il flag di generazione 
                       orizzontale (default = NULL)
  presbuf pVertAlign;  Puntatore a resbuf contenente il flag di generazione 
                       verticale (default = NULL)
  presbuf pThickness;  Puntatore a resbuf contenente lo spessore verticale (default = NULL)
  presbuf pObliqueAng; Puntatore a resbuf contenente l'angolo obliquo (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreateTextsFromDB(presbuf pEntKey, presbuf pAggrFactor,
                                                    C_POINT_LIST &PtList, 
                                                    AcDbObjectId &StyleId, presbuf pText,
                                                    presbuf pRot, presbuf pWidth,
                                                    presbuf pLayer, presbuf pColor,
                                                    presbuf pTextHeight,
                                                    presbuf pHorizAlign, presbuf pVertAlign,
                                                    presbuf pThickness, presbuf pObliqueAng)
{
   AcDbText           *pEnt;
   double             Height = 1.0, Rot = 0.0, Thickness = 0.0, Width = 1.0, ObliqueAng = 0.0;
   int                HorizAlign, VertAlign;
   C_COLOR            Color;
   C_EED              eed;
   C_CLASS            *pCls = gsc_find_class(prj, cls, sub);
   C_STRING           TextValue;
   C_POINT            *pPt;
   AcDbEntityPtrArray *pEntArray;
   bool                err = false;

   if (!pCls) return GS_BAD;

   if (pEntKey) // gli spaghetti non hanno il gs_id
      if (gsc_rb2Lng(pEntKey, &(eed.gs_id)) == GS_BAD) return NULL;

   if (pAggrFactor)
      if (gsc_rb2Int(pAggrFactor, &(eed.num_el)) == GS_BAD) return NULL;

   TextValue = (pText->restype != RTSTR) ? _T("") : pText->resval.rstring;

   if (gsc_rb2Dbl(pRot, &Rot) == GS_BAD)
      Rot = pCls->ptr_fas()->rotation; // valore di default della classe
   else
      // Converto la rotazione in gradi in senso antiorario (per Autocad)
      Rot = ToCounterClockwiseDegreeUnit(Rot);

   if (gsc_rb2Dbl(pWidth, &Width) == GS_BAD || Width < 0) Width = 0.0;

   if (gsc_rb2Dbl(pTextHeight, &Height) == GS_BAD || Height <= 0)
      Height = pCls->ptr_fas()->h_text; // valore di default della classe

   if (gsc_rb2Dbl(pThickness, &Thickness) == GS_BAD || Thickness < 0)
      Thickness = pCls->ptr_fas()->thickness; // valore di default della classe

   if (gsc_rb2Dbl(pObliqueAng, &ObliqueAng) == GS_BAD) ObliqueAng = 0.0;
   
   if (gsc_rb2Int(pHorizAlign, &HorizAlign) == GS_BAD) HorizAlign = kTextLeft;

   if (gsc_rb2Int(pVertAlign, &VertAlign) == GS_BAD) VertAlign = kTextBase;

   if ((pEntArray = new AcDbEntityPtrArray()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   eed.cls = cls;
   eed.sub = sub;
   pPt = (C_POINT *) PtList.get_head();
   while (pPt)
   {
      if ((pEnt = gsc_create_text(TextValue.get_name(), pPt->point, 
                                  Height, Rot, StyleId, 
                                  Thickness, Width, ObliqueAng,
                                  (TextHorzMode) HorizAlign, (TextVertMode) VertAlign)) == NULL)
         { err = true; break; }

      if (pLayer && pLayer->restype == RTSTR)
      {
         if (gsc_setLayer(pEnt, pLayer->resval.rstring) != GS_GOOD)
            { delete pEnt; err = true; break; }
      }
      else
         if (gsc_setLayer(pEnt, pCls->ptr_fas()->layer) != GS_GOOD) // valore di default della classe
            { delete pEnt; err = true; break; }

      // Leggo il colore in base al formato impostato
      if (rb2Color(pColor, Color) == GS_BAD)
         Color = pCls->ptr_fas()->color; // valore di default della classe

      if (gsc_set_color(pEnt, Color) == GS_BAD)
         { delete pEnt; err = true; break; }

      if (eed.save(pEnt) == GS_BAD)
         { err = true; break; }
      pEntArray->append(pEnt);

      pPt = (C_POINT *) PtList.get_next();
   }

   if (err)
   {
      for (int i = 0; i < pEntArray->length(); i++)
         delete ((AcDbEntity *) pEntArray->at(i));
      pEntArray->removeAll();
      return NULL;
   }

   return pEntArray;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreatePointsFromOpenGis   <internal> */
/*+
  Crea uno più PUNTI a partire da una geometria espressa in formato OpenGis.
  Parametri:
  presbuf pEntKey;     Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  TCHAR *pWKTGeom;     Puntatore alla geometria in formato OpenGis WKT (Well Know Text)
  presbuf pLayer;      Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;      Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pThickness;  Puntatore a resbuf contenente lo spessore verticale (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreatePointsFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                                          TCHAR *pWKTGeom,
                                                          presbuf pLayer, presbuf pColor,
                                                          presbuf pThickness)
{
   // esempio: POINT(0 0)
   // esempio: MULTIPOINT(40 40,20 45,45 30)
   C_POINT_LIST PtList;

   if ((pWKTGeom = wcschr(pWKTGeom, _T('('))) == NULL) // cerco la parentesi aperta
      return NULL;
   // Il separatore tra punti = ',' quello tra coordinate = ' '
   if (PtList.fromStr(pWKTGeom + 1, geom_dim, false, _T(','), _T(' ')) == GS_BAD)
      return NULL;

   return CreatePointsFromDB(pEntKey, pAggrFactor, PtList, pLayer, pColor, pThickness);
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreatePointsFromDB       <internal> */
/*+
  Crea un TESTO a partire da una lettura da DB.
  Parametri:
  presbuf pEntKey;      Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor;  Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  C_POINT_LIST &PtList; Lista dei punti
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pThickness;  Puntatore a resbuf contenente lo spessore verticale (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreatePointsFromDB(presbuf pEntKey, presbuf pAggrFactor,
                                                     C_POINT_LIST &PtList, 
                                                     presbuf pLayer, presbuf pColor,
                                                     presbuf pThickness)
{
   AcDbPoint          *pEnt;
   double             Thickness = 0.0;
   C_COLOR            Color;
   C_STRING           Layer;
   C_EED              eed;
   C_CLASS            *pCls = gsc_find_class(prj, cls, sub);
   C_POINT            *pPt;
   AcDbEntityPtrArray *pEntArray;
   bool                err = false;

   if (!pCls) return GS_BAD;

   if (pEntKey) // gli spaghetti non hanno il gs_id
      if (gsc_rb2Lng(pEntKey, &(eed.gs_id)) == GS_BAD) return NULL;

   if (pAggrFactor)
      if (gsc_rb2Int(pAggrFactor, &(eed.num_el)) == GS_BAD) return NULL;

   if (pLayer && pLayer->restype == RTSTR)
      Layer = pLayer->resval.rstring;
   else
      Layer = pCls->ptr_fas()->layer; // valore di default della classe

   // Leggo il colore in base al formato impostato
   if (rb2Color(pColor, Color) == GS_BAD)
      Color = pCls->ptr_fas()->color; // valore di default della classe

   if (gsc_rb2Dbl(pThickness, &Thickness) == GS_BAD || Thickness < 0)
      Thickness = pCls->ptr_fas()->thickness; // valore di default della classe

   if ((pEntArray = new AcDbEntityPtrArray()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   eed.cls = cls;
   eed.sub = sub;
   pPt = (C_POINT *) PtList.get_head();
   while (pPt)
   {
      if ((pEnt = gsc_create_point(pPt->point, Layer.get_name(), &Color, Thickness)) == NULL)
         { err = true; break; }
      if (eed.save(pEnt) == GS_BAD)
         { err = true; break; }
      pEntArray->append(pEnt);
      pPt = (C_POINT *) PtList.get_next();
   }

   if (err)
   {
      for (int i = 0; i < pEntArray->length(); i++)
         delete ((AcDbEntity *) pEntArray->at(i));
      pEntArray->removeAll();
      return NULL;
   }

   return pEntArray;
}


/**********************************************************/
/*.doc C_DBGPH_INFO::CreatePolylineFromOpenGis <internal> */
/*+
  Crea una o più POLILINEE a partire da una geometria espressa in formato OpenGis.
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  TCHAR *pWKTGeom;   Puntatore alla geometria in formato OpenGis WKT (Well Know Text)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pLineType; Puntatore a resbuf contenente il tipo linea (default = NULL)
  presbuf pLineTypeScale; Puntatore a resbuf contenente il fattore di scala
                          del tipolinea (default = NULL)
  presbuf pWidth;    Puntatore a resbuf contenente la largezza della linea (default = NULL)
  presbuf pThickness; Puntatore a resbuf contenente lo spessore verticale (default = NULL)
  presbuf pInitID;    Puntatore a resbuf contenente il codice dell'entità iniziale (default = NULL)
  presbuf pFinalID;   Puntatore a resbuf contenente il codice dell'entità finale (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreatePolylinesFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                                             TCHAR *pWKTGeom,
                                                             presbuf pLayer, presbuf pColor, 
                                                             presbuf pLineType, presbuf pLineTypeScale,
                                                             presbuf pWidth, presbuf pThickness,
                                                             presbuf pInitID, presbuf pFinalID)
{
   // esempio: LINESTRING(40 40, 20 45, 45 30)
   // esempio: MULTILINESTRING((40 40, 20 45, 45 30),(140 140, 120 145, 145 130),(240 240, 220 245, 245 230))
   C_STRING           Polyline;
   TCHAR              *p;
   C_POINT_LIST       PtList;
   AcDbEntityPtrArray *pEntArray;
   bool               err = false;
   AcDbEntity         *pEnt;
   int                nRead;

   if ((pEntArray = new AcDbEntityPtrArray()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   while (*pWKTGeom != _T('\0'))
   {
      if (*pWKTGeom == _T('(') && *(pWKTGeom + 1) != _T('('))
      {
         pWKTGeom++;
         if ((p = wcschr(pWKTGeom, _T(')'))) == NULL) return NULL;
         Polyline.set_name(pWKTGeom, 0, (int) (p - pWKTGeom - 1));

         // Il separatore tra punti = ',' quello tra coordinate = ' '
         if (PtList.fromStr(Polyline.get_name(), geom_dim, false, _T(','), _T(' '), &nRead) == GS_BAD)
            { err = true; break; }

         if ((pEnt = CreatePolylineFromDB(pEntKey, pAggrFactor, PtList, pLayer, pColor, 
                                          pLineType, pLineTypeScale, pWidth, pThickness,
                                          pInitID, pFinalID)) == NULL)
            { err = true; break; }
         pEntArray->append(pEnt);
         pWKTGeom += (nRead + 1);
      }
      else
         pWKTGeom++;
   }

   if (err)
   {
      for (int i = 0; i < pEntArray->length(); i++)
         delete ((AcDbEntity *) pEntArray->at(i));
      pEntArray->removeAll();
      return NULL;
   }

   return pEntArray;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreatePolylineFromDB     <internal> */
/*+
  Crea una POLILINEA a partire da una lettura da DB.
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  C_POINT_LIST &PtList; Lista dei vertici
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pLineType; Puntatore a resbuf contenente il tipo linea (default = NULL)
  presbuf pLineTypeScale; Puntatore a resbuf contenente il fattore di scala
                          del tipolinea (default = NULL)
  presbuf pWidth;    Puntatore a resbuf contenente la largezza della linea (default = NULL)
  presbuf pThickness; Puntatore a resbuf contenente lo spessore verticale (default = NULL)
  presbuf pInitID;    Puntatore a resbuf contenente il codice dell'entità iniziale (default = NULL)
  presbuf pFinalID;   Puntatore a resbuf contenente il codice dell'entità finale (default = NULL)

  Restituisce il puntatore all'entità ACAD in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
AcDbEntity *C_DBGPH_INFO::CreatePolylineFromDB(presbuf pEntKey, presbuf pAggrFactor,
                                               C_POINT_LIST &PtList,
                                               presbuf pLayer, presbuf pColor, 
                                               presbuf pLineType, presbuf pLineTypeScale,
                                               presbuf pWidth, presbuf pThickness,
                                               presbuf pInitID, presbuf pFinalID)
{
   double    Thickness = -1.0, LineTypeScale = -1.0, Width = -1.0; // -1 = da non usare
   C_COLOR   Color;
   AcDbCurve *pEnt;
   C_EED     eed;
   C_CLASS   *pCls = gsc_find_class(prj, cls, sub);
   C_STRING  Layer, LineType;

   if (!pCls) return GS_BAD;

   if (pEntKey) // gli spaghetti non hanno il gs_id
      if (gsc_rb2Lng(pEntKey, &(eed.gs_id)) == GS_BAD) return NULL;

   if (pAggrFactor)
      if (gsc_rb2Int(pAggrFactor, &(eed.num_el)) == GS_BAD) return NULL;

   // Leggo il colore in base al formato impostato
   if (rb2Color(pColor, Color) == GS_BAD)
      Color = pCls->ptr_fas()->color; // valore di default della classe

   if (gsc_rb2Dbl(pLineTypeScale, &LineTypeScale) == GS_BAD)
      LineTypeScale = pCls->ptr_fas()->line_scale; // valore di default della classe

   if (gsc_rb2Dbl(pWidth, &Width) == GS_BAD || Width < 0)
      Width = pCls->ptr_fas()->width; // valore di default della classe

   if (gsc_rb2Dbl(pThickness, &Thickness) == GS_BAD || Thickness < Thickness)
      Thickness = pCls->ptr_fas()->thickness; // valore di default della classe

   if (pLayer && pLayer->restype == RTSTR)
      Layer = pLayer->resval.rstring;
   else
      Layer = pCls->ptr_fas()->layer; // valore di default della classe

   if (pLineType && pLineType->restype == RTSTR)
      LineType = pLineType->resval.rstring;
   else
      LineType = pCls->ptr_fas()->line; // valore di default della classe

   if ((pEnt = PtList.toPolyline(Layer.get_name(),
                                 &Color,
                                 LineType.get_name(),
                                 LineTypeScale, Width, Thickness, &mPlinegen)) == NULL)
      return NULL;

   eed.cls = cls;
   eed.sub = sub;

   if (pInitID)
      gsc_rb2Lng(pInitID, &(eed.initial_node));

   if (pFinalID)
      gsc_rb2Lng(pFinalID, &(eed.final_node));

   if (eed.save(pEnt) == GS_BAD)
      { delete pEnt; return NULL; }

   return pEnt;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CreatePolygonFromOpenGis <internal> */
/*+
  Crea una POLILINEA a partire da una geometria espressa in formato
  OpenGis.
  Parametri:
  presbuf pEntKey;   Puntatore a resbuf contenente il codice dell'entità
  presbuf pAggrFactor; Puntatore a resbuf contenente il fattore di aggregazione dell'entità
  TCHAR *pWKTGeom;   Puntatore alla geometria in formato OpenGis WKT (Well Know Text)
  presbuf pLayer;    Puntatore a resbuf contenente il nome layer (default = NULL)
  presbuf pColor;    Puntatore a resbuf contenente il codice colore (default = NULL)
  presbuf pLineType; Puntatore a resbuf contenente il tipo linea (default = NULL)
  presbuf pLineTypeScale; Puntatore a resbuf contenente il fattore di scala
                          del tipolinea (default = NULL)
  presbuf pWidth;    Puntatore a resbuf contenente la largezza della linea (default = NULL)
  presbuf pThickness; Puntatore a resbuf contenente lo spessore verticale (default = NULL)
  presbuf pHatchName; Puntatore a resbuf contenente il nome del riempimento (default = NULL)
  presbuf pHatchLayer; Puntatore a resbuf contenente i layer del riempiento (default = NULL)
  presbuf pHatchColor; Puntatore a resbuf contenente il colore del riempiento (default = NULL)
  presbuf pHatchScale; Puntatore a resbuf contenente la scal del riempiento (default = NULL)
  presbuf pHatchRot; Puntatore a resbuf contenente la rotazione del riempiento (default = NULL)

  Restituisce un puntatore ad un vettore di puntatori ad entità ACAD 
  in caso di successo altrimenti NULL.
  N.B. Alloca memoria
-*/  
/*********************************************************/
AcDbEntityPtrArray *C_DBGPH_INFO::CreatePolygonFromOpenGis(presbuf pEntKey, presbuf pAggrFactor,
                                                           TCHAR *pWKTGeom,
                                                           presbuf pLayer, presbuf pColor,
                                                           presbuf pLineType, presbuf pLineTypeScale,
                                                           presbuf pWidth, presbuf pThickness,
                                                           presbuf pHatchName, presbuf pHatchLayer,
                                                           presbuf pHatchColor, presbuf pHatchScale,
                                                           presbuf pHatchRot)
{
   C_POINT_LIST       PtList;
   double             Thickness = -1.0, LineTypeScale = -1.0, Width = -1.0; // -1 = da non usare
   C_COLOR            Color;
   AcDbEntityPtrArray *pResult;
   C_EED              eed;
   C_STRING           Layer, LineType, HatchName, Polyline;
   C_CLASS            *pCls = gsc_find_class(prj, cls, sub);
   bool               Exit = false;

   if (!pCls) return GS_BAD;

   if (pEntKey) // gli spaghetti non hanno il gs_id
      if (gsc_rb2Lng(pEntKey, &(eed.gs_id)) == GS_BAD) return NULL;

   if (pAggrFactor)
      if (gsc_rb2Int(pAggrFactor, &(eed.num_el)) == GS_BAD) return NULL;

   eed.cls = cls;
   eed.sub = sub;

   // Leggo il colore in base al formato impostato
   if (rb2Color(pColor, Color) == GS_BAD)
      Color = pCls->ptr_fas()->color; // valore di default della classe

   if (gsc_rb2Dbl(pLineTypeScale, &LineTypeScale) == GS_BAD)
      LineTypeScale = pCls->ptr_fas()->line_scale; // valore di default della classe

   if (gsc_rb2Dbl(pWidth, &Width) == GS_BAD || Width < 0)
      Width = pCls->ptr_fas()->width; // valore di default della classe

   if (gsc_rb2Dbl(pThickness, &Thickness) == GS_BAD || Thickness < 0)
      Thickness = pCls->ptr_fas()->thickness; // valore di default della classe

   if (pLayer && pLayer->restype == RTSTR)
      Layer = pLayer->resval.rstring;
   else
      Layer = pCls->ptr_fas()->layer; // valore di default della classe

   if (pLineType && pLineType->restype == RTSTR)
      LineType = pLineType->resval.rstring;
   else
      LineType = pCls->ptr_fas()->line; // valore di default della classe

   if ((pResult = new AcDbEntityPtrArray()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return NULL; }

   if (WKTToPolygon(pWKTGeom, *pResult, eed, Layer.get_name(), &Color,
                    LineType.get_name(), LineTypeScale, Width, Thickness, &mPlinegen) == GS_BAD)
   {
      for (int i = 0; i < pResult->length(); i++) delete pResult->at(i);
      delete pResult;
      return NULL;
   }

   if (pHatchName)
   {
      if (pHatchName->restype == RTSTR) HatchName = pHatchName->resval.rstring;
   }
   else
      HatchName = pCls->ptr_fas()->hatch; // valore di default della classe

   if (HatchName.len() > 0)
   {
      AcDbHatch *pHatch;
      double    HatchRot = 0.0, HatchScale = -1.0; // -1 = da non usare

      // Leggo il colore in base al formato impostato
      if (rb2Color(pHatchColor, Color) == GS_BAD)
         Color = pCls->ptr_fas()->hatch_color; // valore di default della classe

      if (gsc_rb2Dbl(pHatchScale, &HatchScale) == GS_BAD || HatchScale <= 0)
         HatchScale = pCls->ptr_fas()->hatch_scale; // valore di default della classe

      if (gsc_rb2Dbl(pHatchRot, &HatchRot) == GS_BAD)
         HatchRot = pCls->ptr_fas()->hatch_rotation; // valore di default della classe

      if (pHatchLayer && pHatchLayer->restype == RTSTR)
         Layer = pHatchLayer->resval.rstring;
      else
         Layer = pCls->ptr_fas()->hatch_layer; // valore di default della classe

      if ((pHatch = gsc_create_hatch(*pResult, 
                                     HatchName.get_name(), HatchScale,
                                     HatchRot, &Color, 
                                     Layer.get_name())) != NULL)
      {
         // Se si tratta di spaghetti genero solo il riempimento e non il contorno perchè
         // non essendo supportato il concetto di aggregazione ogni riga della tabella
         // deve generare solo un oggetto grafico
         if (pCls->get_category() == CAT_SPAGHETTI)
         {
            for (int i = 0; i < pResult->length(); i++) delete pResult->at(i);
            pResult->removeAll();
         }
         // Inserisco il riempimento come primo elemento affinchè possa
         // essere disegnato per primo (e sovrascritto dal bordo)
         pResult->insertAt(0, pHatch);
         eed.save(pHatch);
      }
   }

   return pResult;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromBlock             <internal> */
/*+
  Crea una stringa di istruzione SQL a partire da un BLOCCO AutoCAD.
  A seconda della modalità scelta sarà un'istruzione SQL di 
  inserimento, modifica o di cancellazione.
  Parametri:
  AcDbBlockReference *pEnt;   puntatore all'oggetto autocad
  long               GeomKey; codice dell'oggetto geometrico
                              (se si sta inserendo e questo codice = 0 significa
                              che il campo è un autoincremento e quindi non 
                              impostabile da una istruz. SQL)
  int                Mode;    tipo di istruzione SQL da generare:
                              INSERT = inserimento, MODIFY = aggiornamento,
                              ERASE = cancellazione
  C_STRING           &statement; Istruzione SQL (output)
  bool               Label;      Flag opzionale, se = vero viene creato un blocchi DA
                                 altrimenti degli oggetti grafici principali

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromBlock(AcDbBlockReference *pEnt, long GeomKey,
                               int Mode, C_STRING &statement, bool Label)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_EED          eed;
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   //             Valori delle caratteristiche grafiche trasformate in stringhe SQL
   C_STRING       BlockNameValue, RotValue, ScaleValue, LayerValue, ColorValue, internalTableRef;
   C_2STR_LIST    *pSqlCondFieldValueList;
   C_2STR         *pSqlCondFieldValue;

   if (Label)
   {
      if (LblGroupingTableRef.len() > 0)
      {
         internalTableRef = LblGroupingTableRef; // tabella del raggruppamento delle etichette
         pSqlCondFieldValueList = &FieldValueListOnLblGrpTable;
      }
      else
         return GS_CAN; // tabella delle etichette non viene supportata da questa funzione
   }
   else
   {
      internalTableRef = TableRef; // tabella della geometria principale
      pSqlCondFieldValueList = &FieldValueListOnTable;
   }

   if (!pCls) return GS_BAD;

   if (Mode != ERASE) // se non devo cancellare
   {  // leggo le caratteristiche dell'oggetto grafico
      double dummyDbl, UnitConvFactor;

      UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();

      // Fattore di aggregazione e codice entità
      if (eed.load(pEnt) == GS_BAD) return GS_BAD;
      // Nome del blocco
      if (block_attrib.len() > 0)
      {
         BlockNameValue.set_name(gsc_getSymbolName(pEnt->blockTableRecord()));
         // Correggo la stringa secondo la sintassi SQL
         if (pConn->Str2SqlSyntax(BlockNameValue) ==  GS_BAD) return GS_BAD;
      }
      // Rotazione
      if (rotation_attrib.len() > 0) 
      {
         if (gsc_get_rotation(pEnt, &dummyDbl) == GS_BAD) return GS_BAD;
         RotValue = CounterClockwiseRadiantToRotationUnit(dummyDbl);
      }
      // Scala
      if (block_scale_attrib.len() > 0) 
      {
         if (gsc_get_scale(pEnt, &dummyDbl) == GS_BAD) return GS_BAD;
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            ScaleValue = dummyDbl / UnitConvFactor;
         else
            ScaleValue = dummyDbl;
      }
      // Layer
      if (layer_attrib.len() > 0) 
      {
         gsc_getLayer(pEnt, LayerValue);
         // Correggo la stringa secondo la sintassi SQL
         if (pConn->Str2SqlSyntax(LayerValue) ==  GS_BAD) return GS_BAD;
      }
      // Color
      if (color_attrib.len() > 0)
      {
         C_COLOR color;
         if (gsc_get_color(pEnt, color) == GS_BAD) return GS_BAD;
         Color2SQLValue(color, ColorValue);
      }
   }

   // Tipi di operazioni
   switch (Mode)
   {
      case INSERT: // inserimento nuovo oggetto grafico - inizio
      {
         // INSERT INTO Tabella (colonna1, colonna 2, ...) VALUES (valore1, valore2, ...)
         statement = _T("INSERT INTO ");
         statement += internalTableRef;
         statement += _T(" (");
         
         // nell'ordine geometria,ent_key,[key],[aggr_factor],[BlockName],[rotation],
         // [scale],[layer],[color],
         
         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
            statement += geom_attrib;
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            statement += x_attrib;
            statement += _T(',');
            statement += y_attrib;
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               statement += _T(',');
               statement += z_attrib;
            }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += key_attrib;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
         }

         if (block_attrib.len() > 0)
         {
            statement += _T(',');
            statement += block_attrib;
         }

         if (rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += rotation_attrib;
         }

         if (block_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += block_scale_attrib;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
         }

         // Lista nomi dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValueList->get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }        

         statement += _T(") VALUES (");

         // nell'ordine geometria,ent_key,[key],[aggr_factor],[BlockName],[rotation],
         // [scale],[layer],[color],

         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

            if (WKTFromPoint(pEnt, WKT) == GS_BAD) return GS_BAD;

            if (get_isCoordToConvert() == GS_GOOD)
               SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

            // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
            // SDO_GEOMETRY(campo, srid) in ORACLE
            pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);
            statement += WKTToGeom_SQLSyntax;
         }
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            ads_point point;

            // Punto di inserimento
            if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;
            statement += point[X];
            statement += _T(',');
            statement += point[Y];
            if (geom_dim == GS_3D) { statement += _T(','); statement += point[Z]; }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += eed.gs_id;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += GeomKey;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += eed.num_el;
         }

         if (block_attrib.len() > 0)
         {
            statement += _T(',');
            statement += BlockNameValue;
         }

         if (rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += RotValue;
         }

         if (block_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ScaleValue;
         }

         if (layer_attrib.len() > 0)
         {
            statement += _T(',');
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ColorValue;
         }

         // Lista valori dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValueList->get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name2();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(")");

         if (GeomKey == 0) // se campo chiave della geometria è autoincrementato
         { // aggiungo all'istruzione SQL "RETURNING ..."
         }

         break;
      } // inserimento nuovo oggetto grafico - fine

      case MODIFY:  // modifica oggetto grafico esistente - inizio
      {
         // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
         statement = _T("UPDATE ");
         statement += internalTableRef;
         statement += _T(" SET ");

         // nell'ordine geometria,ent_key,[aggr_factor],[BlockName],[rotation],
         // [scale],[layer], [color]

         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

            if (WKTFromPoint(pEnt, WKT) == GS_BAD) return GS_BAD;

            if (get_isCoordToConvert() == GS_GOOD)
               SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

            // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
            // SDO_GEOMETRY(campo, srid) in ORACLE
            if (pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax) == GS_BAD)
               return GS_BAD;

            statement += geom_attrib;
            statement += _T("=");
            statement += WKTToGeom_SQLSyntax;
         }
         else // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            ads_point point;

            // Punto di inserimento
            if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;
            statement += x_attrib;
            statement += _T("=");
            statement += point[X];
            statement += _T(',');
            statement += y_attrib;
            statement += _T("=");
            statement += point[Y];
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               statement += _T(',');
               statement += z_attrib;
               statement += _T("=");
               statement += point[Z];
            }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
            statement += _T("=");
            statement += eed.gs_id;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
            statement += _T("=");
            statement += eed.num_el;
         }

         if (block_attrib.len() > 0)
         {
            statement += _T(',');
            statement += block_attrib;
            statement += _T("=");
            statement += BlockNameValue;
         }

         if (rotation_attrib.len() > 0)
         {
            statement += _T(',');
            statement += rotation_attrib;
            statement += _T("=");
            statement += RotValue;
         }

         if (block_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += block_scale_attrib;
            statement += _T("=");
            statement += ScaleValue;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
            statement += _T("=");
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
            statement += _T("=");
            statement += ColorValue;
         }

         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // modifica oggetto grafico esistente - fine

      case ERASE: // cancellazione oggetto grafico esistente - inizio
      {
         // DELETE FROM Tabella WHERE key_attrib=valore
         statement = _T("DELETE FROM ");
         statement += internalTableRef;
         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;
      } // cancellazione oggetto grafico esistente - fine
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromAttrib            <internal> */
/*+
  Crea una stringa di istruzione SQL a partire da un attributo di blocco AutoCAD.
  A seconda della modalità scelta sarà un'istruzione SQL di 
  inserimento, modifica o di cancellazione.
  Parametri:
  AcDbAttribute *pEnt;     Puntatore all'oggetto autocad attribute
  long        GeomKey;     Codice dell'oggetto geometrico
                           (se si sta inserendo e questo codice = 0 significa
                           che il campo è un autoincremento e quindi non 
                           impostabile da una istruz. SQL)
                           2) Codice del blocco parente nel caso le etichette
                           siano memorizzate come testi raggruppati da un blocco.
  int           Mode;      Tipo di istruzione SQL da generare:
                           INSERT = inserimento, MODIFY = aggiornamento,
                           ERASE = cancellazione
  long         ent_key;    1) Codice dell'entità nel caso le etichette
                           siano memorizzate come testi indipendenti
                           2) Codice del blocco parente nel caso le etichette
                           siano memorizzate come testi raggruppati da un blocco.
  int       AggrFactor;    Nel caso 1 precedente è necessario anche il fattore di aggregazione
  C_STR_LIST &statement;   Lista di istruzioni SQL per gli relativi attributi.

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromAttrib(AcDbAttribute *pEnt, long GeomKey, int Mode, long ent_key, int AggrFactor,
                                C_STRING &statement)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   double         HText, ObliqueAng, WidthFactor;
   int            HorizAlign, VertAlign;
   //             Valori delle caratteristiche grafiche trasformate in stringhe SQL  
   C_STRING       StyleNameValue, TextValue, AttrNameValue, RotValue, LayerValue;
   C_STRING       ThicknessValue, ColorValue;
   C_2STR         *pSqlCondFieldValue;

   if (!pCls) return GS_BAD;

   // Nome dell'attributo
   AttrNameValue = pEnt->tag();

   if (gsc_AdjSyntax(AttrNameValue, _T(''), _T(''), pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   // Correggo la stringa secondo la sintassi SQL
   if (pConn->Str2SqlSyntax(AttrNameValue) == GS_BAD) return GS_BAD;

   if (Mode != ERASE) // se non devo cancellare
   {  // leggo le caratteristiche dell'oggetto grafico
      double dummyDbl, UnitConvFactor;

      UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();

      // Caratteristiche varie
      if (gsc_getInfoText(pEnt, &TextValue, &StyleNameValue, &HText,
                          &HorizAlign, &VertAlign, &ObliqueAng, &WidthFactor) == GS_BAD) return GS_BAD;
      // Altezza testo
      if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
         HText = HText / UnitConvFactor;
      // Stile testo
      if (text_style_attrib.len() > 0)
         // Correggo la stringa secondo la sintassi SQL
         if (pConn->Str2SqlSyntax(StyleNameValue) == GS_BAD) return GS_BAD;
      // Correggo la stringa secondo la sintassi SQL 
      if (pConn->Str2SqlSyntax(TextValue) == GS_BAD) return GS_BAD;
      // Rotazione
      if (rotation_attrib.len() > 0) 
      {
         if (gsc_get_rotation(pEnt, &dummyDbl) == GS_BAD) return GS_BAD;
         RotValue = CounterClockwiseRadiantToRotationUnit(dummyDbl);
      }
      // Layer
      if (layer_attrib.len() > 0) 
      {
         gsc_getLayer(pEnt, LayerValue);
         // Correggo la stringa secondo la sintassi SQL
         if (pConn->Str2SqlSyntax(LayerValue) == GS_BAD) return GS_BAD;
      }
      // Thickness
      if (thickness_attrib.len() > 0) 
      {
         if (gsc_get_thickness(pEnt, &dummyDbl) == GS_BAD) return GS_BAD;
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            ThicknessValue = dummyDbl / UnitConvFactor;
         else
            ThicknessValue = dummyDbl;
      }
      // Color
      if (color_attrib.len() > 0)
      {
         C_COLOR color;
         if (gsc_get_color(pEnt, color) == GS_BAD) return GS_BAD;
         Color2SQLValue(color, ColorValue);
      }
   }

   // Tipi di operazioni
   switch (Mode)
   {
      case INSERT: // inserimento nuovo oggetto grafico - inizio
      {
         // INSERT INTO Tabella (colonna1, colonna 2, ...) VALUES (valore1, valore2, ...)
         statement = _T("INSERT INTO ");
         statement += LblTableRef;
         statement += _T(" (");
         
         // nell'ordine geometria,ent_key,[key],[aggr_factor]usato quando le etichette
         // non sono raggruppate in blocchi,[Style],Text,AttrName
         // [rotation],[scale],[layer],[color],[text_height],
         // [horiz_align],[vert_align],[thickness],[oblique_ang],[AttrInvis]
         
         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
            statement += geom_attrib;
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            statement += x_attrib;
            statement += _T(',');
            statement += y_attrib;
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               statement += _T(',');
               statement += z_attrib;
            }
         }

         statement += _T(',');
         statement += ent_key_attrib;
         
         if (GeomKey != 0) // Se il codice è aggiornabile
         {
            statement += _T(',');
            statement += key_attrib;
         }

         // se si vuole inserire una etichetta come testo indipendente
         if (LblGroupingTableRef.len() == 0)
            if (aggr_factor_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += aggr_factor_attrib;
            }

         if (text_style_attrib.len() > 0)
         {
            statement += _T(',');
            statement += text_style_attrib;
         }

         statement += _T(',');
         statement += text_attrib;

         statement += _T(',');
         statement += attrib_name_attrib;

         if (rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += rotation_attrib;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
         }

         if (h_text_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += h_text_attrib;
         }

         if (horiz_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += horiz_align_attrib;
         }

         if (vert_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += vert_align_attrib;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
         }

         if (oblique_angle_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += oblique_angle_attrib;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
         }

         if (attrib_invis_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += attrib_invis_attrib;
         }

         // Lista nomi dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnLblTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(") VALUES (");

         // nell'ordine geometria,ent_key,[key],[aggr_factor]usato quando le etichette
         // non sono raggruppate in blocchi,[Style],Text,AttrName
         // [rotation],[scale],[layer],[color],[text_height],
         // [horiz_align],[vert_align],[thickness],[oblique_ang],[AttrInvis]
         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

            if (WKTFromPoint(pEnt, WKT) == GS_BAD) return GS_BAD;

            if (get_isCoordToConvert() == GS_GOOD)
               SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

            // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
            // SDO_GEOMETRY(campo, srid) in ORACLE
            pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);
            statement += WKTToGeom_SQLSyntax;
         }
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            ads_point point;

            // Punto di inserimento
            if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;

            statement += point[X];
            statement += _T(',');
            statement += point[Y];
            if (geom_dim == GS_3D) { statement += _T(','); statement += point[Z]; }
         }

         statement += _T(',');

         // se si vuole inserire una etichetta come testo indipendente
         // ent_key = codice entità GEOsim
         // se si vuole inserire una etichetta legata ad un blocco
         // ent_key = codice blocco parente 
         statement += ent_key;

         if (GeomKey != 0) // Se il codice è aggiornabile
         {
            statement += _T(',');
            statement += GeomKey;
         }

         // se si vuole inserire una etichetta come testo indipendente
         if (LblGroupingTableRef.len() == 0)
            if (aggr_factor_attrib.len() > 0)
            {
               statement += _T(',');
               statement += AggrFactor;
            }

         if (text_style_attrib.len() > 0)
         {
            statement += _T(',');
            statement += StyleNameValue;
         }

         statement += _T(',');
         statement += TextValue;

         statement += _T(',');
         statement += AttrNameValue;

         if (rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += RotValue;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ColorValue;
         }

         if (h_text_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HText;
         }

         if (horiz_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HorizAlign;
         }

         if (vert_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += VertAlign;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ThicknessValue;
         }

         if (oblique_angle_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ObliqueAng;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += WidthFactor;
         }

         if (attrib_invis_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += pConn->boolToSQL((pEnt->isInvisible() == 0) ? false : true);
         }


         // Lista valori dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnLblTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name2();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(")");

         if (GeomKey == 0) // se campo chiave della geometria è autoincrementato
         { // aggiungo all'istruzione SQL "RETURNING ..."
         }

         break;
      } // inserimento nuovo oggetto grafico - fine

      case MODIFY:  // modifica oggetto grafico esistente - inizio
      {
         // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
         statement = _T("UPDATE ");
         statement += LblTableRef;
         statement += _T(" SET ");

         // nell'ordine geometria,ent_key,[key],[aggr_factor]usato quando le etichette
         // non sono raggruppate in blocchi,[Style],Text,AttrName
         // [rotation],[scale],[layer],[color],[text_height],
         // [horiz_align],[vert_align],[thickness],[oblique_ang],[AttrInvis]
         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

            if (WKTFromPoint(pEnt, WKT) == GS_BAD) return GS_BAD;

            if (get_isCoordToConvert() == GS_GOOD)
               SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

            // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
            // SDO_GEOMETRY(campo, srid) in ORACLE
            pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

            statement += geom_attrib;
            statement += _T("=");
            statement += WKTToGeom_SQLSyntax;
         }
         else // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            ads_point point;

            // Punto di inserimento
            if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;
            statement += x_attrib;
            statement += _T("=");
            statement += point[X];
            statement += _T(',');
            statement += y_attrib;
            statement += _T("=");
            statement += point[Y];
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               statement += _T(',');
               statement += z_attrib;
               statement += _T("=");
               statement += point[Z];
            }
         }

         statement += _T(',');
         statement += ent_key_attrib;
         statement += _T("=");

         // se si vuole inserire una etichetta come testo indipendente
         // ent_key = codice entità GEOsim
         // se si vuole inserire una etichetta legata ad un blocco
         // ent_key = codice blocco parente 
         statement += ent_key;

         // se si vuole aggiornare una etichetta come testo indipendente
         if (LblGroupingTableRef.len() == 0)
            if (aggr_factor_attrib.len() > 0)
            {
               statement += _T(',');
               statement += aggr_factor_attrib;
               statement += _T("=");
               statement += AggrFactor;
            }

         if (text_style_attrib.len() > 0)
         {
            statement += _T(',');
            statement += text_style_attrib;
            statement += _T("=");
            statement += StyleNameValue;
         }

         statement += _T(',');
         statement += text_attrib;
         statement += _T("=");
         statement += TextValue;

         statement += _T(',');
         statement += attrib_name_attrib;
         statement += _T("=");
         statement += AttrNameValue;

         if (rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += rotation_attrib;
            statement += _T("=");
            statement += RotValue;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
            statement += _T("=");
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
            statement += _T("=");
            statement += ColorValue;
         }

         if (h_text_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += h_text_attrib;
            statement += _T("=");
            statement += HText;
         }

         if (horiz_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += horiz_align_attrib;
            statement += _T("=");
            statement += HorizAlign;
         }

         if (vert_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += vert_align_attrib;
            statement += _T("=");
            statement += VertAlign;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
            statement += _T("=");
            statement += ThicknessValue;
         }

         if (oblique_angle_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += oblique_angle_attrib;
            statement += _T("=");
            statement += ObliqueAng;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
            statement += _T("=");
            statement += WidthFactor;
         }

         if (attrib_invis_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += attrib_invis_attrib;
            statement += _T("=");
            statement += pConn->boolToSQL((pEnt->isInvisible() == 0) ? false : true);
         }

         statement += _T(" WHERE ");

         // se si vuole aggiornare una etichetta come testo indipendente
         if (LblGroupingTableRef.len() == 0)
         {
            statement += key_attrib;
            statement += _T("=");
            statement += GeomKey;
         }
         else
         { // se si vuole aggiornare una etichetta legata ad un blocco
            statement += ent_key_attrib;
            statement += _T("=");
            statement += ent_key;
            statement += _T(" AND ");
            statement += attrib_name_attrib;
            statement += _T("=");
            statement += AttrNameValue;
         }

         break;
      } // modifica oggetto grafico esistente - fine

      case ERASE: // cancellazione oggetto grafico esistente - inizio
      {
         // DELETE FROM Tabella WHERE key_attrib=valore
         statement = _T("DELETE FROM ");
         statement += LblTableRef;
         statement += _T(" WHERE ");

         // se si vuole cancellare una etichetta come testo indipendente
         if (LblGroupingTableRef.len() == 0)
         {
            statement += key_attrib;
            statement += _T("=");
            statement += GeomKey;
         }
         else
         { // se si vuole cancellare una etichetta legata ad un blocco
            statement += ent_key_attrib;
            statement += _T("=");
            statement += ent_key;
            statement += _T(" AND ");
            statement += attrib_name_attrib;
            statement += _T("=");
            statement += AttrNameValue;
         }

         break;
      } // cancellazione oggetto grafico esistente - fine
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromText             <internal> */
/*+
  Crea una stringa di istruzione SQL a partire da un testo AutoCAD.
  A seconda della modalità scelta sarà un'istruzione SQL di 
  inserimento, modifica o di cancellazione.
  Parametri:
  AcDbEntity *pEnt;  puntatore all'oggetto autocad (text, mtext, attribute...)
  long               GeomKey; codice dell'oggetto geometrico
                              (se si sta inserendo e questo codice = 0 significa
                              che il campo è un autoincremento e quindi non 
                              impostabile da una istruz. SQL)
  int                Mode;    tipo di istruzione SQL da generare:
                              INSERT = inserimento, MODIFY = aggiornamento,
                              ERASE = cancellazione
  C_STRING           &statement; Istruzione SQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromText(AcDbEntity *pEnt, long GeomKey, int Mode, C_STRING &statement)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_EED          eed;
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   double         HText, ObliqueAng, WidthFactor;
   int            HorizAlign, VertAlign;
   //             Valori delle caratteristiche grafiche trasformate in stringhe SQL  
   C_STRING       StyleNameValue, TextValue, RotValue, LayerValue, ThicknessValue;
   C_STRING       ColorValue;
   C_2STR         *pSqlCondFieldValue;

   if (!pCls) return GS_BAD;

   if (Mode != ERASE) // se non devo cancellare
   {  // leggo le caratteristiche dell'oggetto grafico
      double dummyDbl, UnitConvFactor;

      UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();

      // Fattore di aggregazione e codice entità
      if (eed.load(pEnt) == GS_BAD) return GS_BAD;
      // Caratteristiche varie
      if (gsc_getInfoText(pEnt, &TextValue, &StyleNameValue, &HText,
                          &HorizAlign, &VertAlign, &ObliqueAng, &WidthFactor) == GS_BAD) return GS_BAD;
      // Altezza testo
      if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
         HText = HText / UnitConvFactor;
      // Stile testo
      if (text_style_attrib.len() > 0)
         // Correggo la stringa secondo la sintassi SQL
         if (pConn->Str2SqlSyntax(StyleNameValue) == GS_BAD) return GS_BAD;
      // Correggo la stringa secondo la sintassi SQL
      if (pConn->Str2SqlSyntax(TextValue) == GS_BAD) return GS_BAD;
      // Rotazione
      if (rotation_attrib.len() > 0) 
      {
         if (gsc_get_rotation(pEnt, &dummyDbl) == GS_BAD) return GS_BAD;
         RotValue = CounterClockwiseRadiantToRotationUnit(dummyDbl);
      }
      // Layer
      if (layer_attrib.len() > 0) 
      {
         gsc_getLayer(pEnt, LayerValue);
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(LayerValue) == GS_BAD) return GS_BAD;
      }
      // Thickness
      if (thickness_attrib.len() > 0) 
      {
         if (gsc_get_thickness(pEnt, &dummyDbl) == GS_BAD) return GS_BAD;
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            ThicknessValue = dummyDbl / UnitConvFactor;
         else
            ThicknessValue = dummyDbl;
      }
      // Color
      if (color_attrib.len() > 0)
      {
         C_COLOR color;
         if (gsc_get_color(pEnt, color) == GS_BAD) return GS_BAD;
         Color2SQLValue(color, ColorValue);
      }
   }

   // Tipi di operazioni
   switch (Mode)
   {
      case INSERT: // inserimento nuovo oggetto grafico - inizio
      {
         // INSERT INTO Tabella (colonna1, colonna 2, ...) VALUES (valore1, valore2, ...)
         statement = _T("INSERT INTO ");
         statement += TableRef;
         statement += _T(" (");
         
         // nell'ordine geometria,ent_key,[key],[aggr_factor],[Style],Text,
         // [rotation],[scale],[layer],[color],[text_height],
         // [horiz_align],[vert_align],[thickness],[oblique_ang]
         
         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
            statement += geom_attrib;
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            statement += x_attrib;
            statement += _T(',');
            statement += y_attrib;
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               statement += _T(',');
               statement += z_attrib;
            }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += key_attrib;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
         }

         if (text_style_attrib.len() > 0)
         {
            statement += _T(',');
            statement += text_style_attrib;
         }

         statement += _T(',');
         statement += text_attrib;

         if (rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += rotation_attrib;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
         }

         if (h_text_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += h_text_attrib;
         }

         if (horiz_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += horiz_align_attrib;
         }

         if (vert_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += vert_align_attrib;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
         }

         if (oblique_angle_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += oblique_angle_attrib;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
         }

         // Lista nomi dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(") VALUES (");

         // nell'ordine geometria,ent_key,[key],[aggr_factor],[Style],Text,
         // [rotation],[scale],[layer],[color],[text_height],
         // [horiz_align],[vert_align],[thickness],[oblique_ang]

         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

            if (WKTFromPoint(pEnt, WKT) == GS_BAD) return GS_BAD;

            if (get_isCoordToConvert() == GS_GOOD)
               SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

            // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
            // SDO_GEOMETRY(campo, srid) in ORACLE
            pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);
            statement += WKTToGeom_SQLSyntax;
         }
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            ads_point point;

            // Punto di inserimento
            if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;
            statement += point[X];
            statement += _T(',');
            statement += point[Y];
            if (geom_dim == GS_3D) { statement += _T(','); statement += point[Z]; }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += eed.gs_id;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += GeomKey;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += eed.num_el;
         }

         if (text_style_attrib.len() > 0)
         {
            statement += _T(',');
            statement += StyleNameValue;
         }

         statement += _T(',');
         statement += TextValue;

         if (rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += RotValue;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ColorValue;
         }

         if (h_text_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HText;
         }

         if (horiz_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HorizAlign;
         }

         if (vert_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += VertAlign;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ThicknessValue;
         }

         if (oblique_angle_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ObliqueAng;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += WidthFactor;
         }


         // Lista valori dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name2();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(")");

         if (GeomKey == 0) // se campo chiave della geometria è autoincrementato
         { // aggiungo all'istruzione SQL "RETURNING ..."
         }

         break;
      } // inserimento nuovo oggetto grafico - fine

      case MODIFY:  // modifica oggetto grafico esistente - inizio
      {
         // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
         statement = _T("UPDATE ");
         statement += TableRef;
         statement += _T(" SET ");

         // nell'ordine geometria,ent_key,[aggr_factor],[Style],Text,
         // [rotation],[scale],[layer],[color],[text_height],
         // [horiz_align],[vert_align],[thickness],[oblique_ang]

         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

            if (WKTFromPoint(pEnt, WKT) == GS_BAD) return GS_BAD;

            if (get_isCoordToConvert() == GS_GOOD)
               SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

            // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
            // SDO_GEOMETRY(campo, srid) in ORACLE
            pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

            statement += geom_attrib;
            statement += _T("=");
            statement += WKTToGeom_SQLSyntax;
         }
         else // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            ads_point point;

            // Punto di inserimento
            if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;
            statement += x_attrib;
            statement += _T("=");
            statement += point[X];
            statement += _T(',');
            statement += y_attrib;
            statement += _T("=");
            statement += point[Y];
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               statement += _T(',');
               statement += z_attrib;
               statement += _T("=");
               statement += point[Z];
            }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
            statement += _T("=");
            statement += eed.gs_id;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
            statement += _T("=");
            statement += eed.num_el;
         }

         if (text_style_attrib.len() > 0)
         {
            statement += _T(',');
            statement += text_style_attrib;
            statement += _T("=");
            statement += StyleNameValue;
         }

         if (text_attrib.len() > 0)
         {
            statement += _T(',');
            statement += text_attrib;
            statement += _T("=");
            statement += TextValue;
         }

         if (rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += rotation_attrib;
            statement += _T("=");
            statement += RotValue;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
            statement += _T("=");
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
            statement += _T("=");
            statement += ColorValue;
         }

         if (h_text_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += h_text_attrib;
            statement += _T("=");
            statement += HText;
         }

         if (horiz_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += horiz_align_attrib;
            statement += _T("=");
            statement += HorizAlign;
         }

         if (vert_align_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += vert_align_attrib;
            statement += _T("=");
            statement += VertAlign;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
            statement += _T("=");
            statement += ThicknessValue;
         }

         if (oblique_angle_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += oblique_angle_attrib;
            statement += _T("=");
            statement += ObliqueAng;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
            statement += _T("=");
            statement += WidthFactor;
         }

         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // modifica oggetto grafico esistente - fine

      case ERASE: // cancellazione oggetto grafico esistente - inizio
      {
         // DELETE FROM Tabella WHERE key_attrib=valore
         statement = _T("DELETE FROM ");
         statement += TableRef;
         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // cancellazione oggetto grafico esistente - fine
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromPoint             <internal> */
/*+
  Crea una stringa di istruzione SQL a partire da un punto AutoCAD.
  A seconda della modalità scelta sarà un'istruzione SQL di 
  inserimento, modifica o di cancellazione.
  Parametri:
  AcDbPoint *pEnt;      puntatore all'oggetto autocad
  long      GeomKey;    codice dell'oggetto geometrico
                        (se si sta inserendo e questo codice = 0 significa
                        che il campo è un autoincremento e quindi non 
                        impostabile da una istruz. SQL)
  int       Mode;       tipo di istruzione SQL da generare:
                        INSERT = inserimento, MODIFY = aggiornamento,
                        ERASE = cancellazione
  C_STRING  &statement; Istruzione SQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromPoint(AcDbPoint *pEnt, long GeomKey, int Mode, C_STRING &statement)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_EED          eed;
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   //             Valori delle caratteristiche grafiche trasformate in stringhe SQL  
   C_STRING       LayerValue, ColorValue, ThicknessValue;
   C_2STR         *pSqlCondFieldValue;

   if (!pCls) return GS_BAD;

   if (Mode != ERASE) // se non devo cancellare
   {  // leggo le caratteristiche dell'oggetto grafico
      double dummyDbl, UnitConvFactor;

      UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();

      // Fattore di aggregazione e codice entità
      if (eed.load(pEnt) == GS_BAD) return GS_BAD;
      // Layer
      if (layer_attrib.len() > 0) 
      {
         gsc_getLayer(pEnt, LayerValue);
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(LayerValue) == GS_BAD) return GS_BAD;
      }
      // Color
      if (color_attrib.len() > 0)
      {
         C_COLOR color;
         if (gsc_get_color(pEnt, color) == GS_BAD) return GS_BAD;
         Color2SQLValue(color, ColorValue);
      }
      // Thickness
      if (thickness_attrib.len() > 0) 
      {
         if (gsc_get_thickness(pEnt, &dummyDbl) == GS_BAD) return GS_BAD;
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            ThicknessValue = dummyDbl / UnitConvFactor;
         else
            ThicknessValue = dummyDbl;
      }
   }

   // Tipi di operazioni
   switch (Mode)
   {
      case INSERT: // inserimento nuovo oggetto grafico - inizio
      {
         // INSERT INTO Tabella (colonna1, colonna 2, ...) VALUES (valore1, valore2, ...)
         statement = _T("INSERT INTO ");
         statement += TableRef;
         statement += _T(" (");
         
         // nell'ordine geometria,ent_key,[key],[aggr_factor],[layer],[color],[thickness]
         
         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
            statement += geom_attrib;
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            statement += x_attrib;
            statement += _T(',');
            statement += y_attrib;
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               statement += _T(',');
               statement += z_attrib;
            }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += key_attrib;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
         }

         // Lista nomi dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(") VALUES (");

         // nell'ordine geometria,ent_key,[key],[aggr_factor],[layer],[color],[thickness]

         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

            if (WKTFromPoint(pEnt, WKT) == GS_BAD) return GS_BAD;

            if (get_isCoordToConvert() == GS_GOOD)
               SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

            // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
            // SDO_GEOMETRY(campo, srid) in ORACLE
            pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);
            statement += WKTToGeom_SQLSyntax;
         }
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            ads_point point;

            // Punto di inserimento
            if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;
            statement += point[X];
            statement += _T(',');
            statement += point[Y];
            if (geom_dim == GS_3D) { statement += _T(','); statement += point[Z]; }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += eed.gs_id;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += GeomKey;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += eed.num_el;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ColorValue;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ThicknessValue;
         }

         // Lista valori dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name2();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(")");

         if (GeomKey == 0) // se campo chiave della geometria è autoincrementato
         { // aggiungo all'istruzione SQL "RETURNING ..."
         }

         break;
      } // inserimento nuovo oggetto grafico - fine

      case MODIFY:  // modifica oggetto grafico esistente - inizio
      {
         // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
         statement = _T("UPDATE ");
         statement += TableRef;
         statement += _T(" SET ");

         // nell'ordine geometria,ent_key,[aggr_factor],[layer],[color],[thickness]

         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

            if (WKTFromPoint(pEnt, WKT) == GS_BAD) return GS_BAD;

            if (get_isCoordToConvert() == GS_GOOD)
               SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

            // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
            // SDO_GEOMETRY(campo, srid) in ORACLE
            if (pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax) == GS_BAD)
               return GS_BAD;

            statement += geom_attrib;
            statement += _T("=");
            statement += WKTToGeom_SQLSyntax;
         }
         else // Se geom_attrib è vuoto, la geometria è sotto forma di 2/3 campi numerici
         {
            ads_point point;

            // Punto di inserimento
            if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;
            statement += x_attrib;
            statement += _T("=");
            statement += point[X];
            statement += _T(',');
            statement += y_attrib;
            statement += _T("=");
            statement += point[Y];
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               statement += _T(',');
               statement += z_attrib;
               statement += _T("=");
               statement += point[Z];
            }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
            statement += _T("=");
            statement += eed.gs_id;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
            statement += _T("=");
            statement += eed.num_el;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
            statement += _T("=");
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
            statement += _T("=");
            statement += ColorValue;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
            statement += _T("=");
            statement += ThicknessValue;
         }

         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // modifica oggetto grafico esistente - fine

      case ERASE: // cancellazione oggetto grafico esistente - inizio
      {
         // DELETE FROM Tabella WHERE key_attrib=valore
         statement = _T("DELETE FROM ");
         statement += TableRef;
         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // cancellazione oggetto grafico esistente - fine
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromPolyline          <internal> */
/*+
  Crea una stringa di istruzione SQL a partire da una polilinea AutoCAD.
  A seconda della modalità scelta sarà una o più istruzioni SQL di 
  inserimento, modifica o di cancellazione.
  Parametri:
  AcDbEntity  *pEnt;           puntatore all'oggetto autocad (lwpolyline, polyline...)
  long        GeomKey;         codice dell'oggetto geometrico
                               (se si sta inserendo e questo codice = 0 significa
                               che il campo è un autoincremento e quindi non 
                               impostabile da una istruz. SQL)
  int         Mode;            tipo di istruzione SQL da generare:
                               INSERT = inserimento, MODIFY = aggiornamento,
                               ERASE = cancellazione
   C_STR_LIST &statement_list; Lista di istruzioni SQL che nel caso di
                               campo geometria è composta di 1 sola
                               istruzione. Nel caso di campi numerici
                               ci sono diverse istruzioni SQL tante 
                               quante sono i vertici della polilinea (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromPolyline(AcDbEntity *pEnt, long GeomKey, 
                                  int Mode, C_STR_LIST &statement_list)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_POINT_LIST   PtList;
   C_EED          eed;
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   C_STRING       CoordStr, PrefixStm, statement, ColorValue;
   //             Valori delle caratteristiche grafiche trasformate in stringhe SQL  
   C_STRING       LayerValue, LineTypeValue, LineTypeScaleValue, WidthValue, ThicknessValue;
   C_2STR         *pSqlCondFieldValue;

   if (!pCls) return GS_BAD;

   if (Mode != ERASE) // se non devo cancellare
   {  // leggo le caratteristiche dell'oggetto grafico
      double dummyDbl, UnitConvFactor;

      UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();

      // Fattore di aggregazione e codice entità
      if (eed.load(pEnt) == GS_BAD) return GS_BAD;
      // Layer
      if (layer_attrib.len() > 0) 
      {
         gsc_getLayer(pEnt, LayerValue);
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(LayerValue) == GS_BAD) return GS_BAD;
      }
      // Tipolinea
      if (line_type_attrib.len() > 0)
      {
         gsc_get_lineType(pEnt, LineTypeValue);
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(LineTypeValue) == GS_BAD) return GS_BAD;
      }
      // Scala tipo linea
      if (line_type_scale_attrib.len() > 0) 
      {
         dummyDbl = pEnt->linetypeScale();
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            LineTypeScaleValue = dummyDbl / UnitConvFactor;
         else
            LineTypeScaleValue = dummyDbl;
      }
      // Larghezza
      if (width_attrib.len() > 0) 
      {
         if (gsc_get_width(pEnt, &dummyDbl) == GS_BAD) return GS_BAD;
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            WidthValue = dummyDbl / UnitConvFactor;
         else
            WidthValue = dummyDbl;
      }
      // Spessore verticale
      if (thickness_attrib.len() > 0) 
      {
         gsc_get_thickness(pEnt, &dummyDbl);
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            ThicknessValue = dummyDbl / UnitConvFactor;
         else
            ThicknessValue = dummyDbl;
      }
      // Color
      if (color_attrib.len() > 0)
      {
         C_COLOR color;
         if (gsc_get_color(pEnt, color) == GS_BAD) return GS_BAD;
         Color2SQLValue(color, ColorValue);
      }
   }

   statement_list.remove_all();

   // Tipi di operazioni
   switch (Mode)
   {
      case INSERT: // inserimento nuovo oggetto grafico - inizio
      {
         // INSERT INTO Tabella (colonna1, colonna 2, ...) VALUES (valore1, valore2, ...)
         PrefixStm = _T("INSERT INTO ");
         PrefixStm += TableRef;
         PrefixStm += _T(" (");
         
         // nell'ordine geometria,ent_key,[key],[aggr_factor],
         // [layer],[color],[line_type],[scale],[width],[thickness]
         
         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
            PrefixStm += geom_attrib;
         else
         // Se geom_attrib è vuoto, la geometria è sotto forma di campi numerici
         {  
            // Lista dei vertici e bulge
            if (PtList.add_vertex_point(pEnt, GS_GOOD,
                                        pConn->get_MaxTolerance2ApproxCurve(),
                                        pConn->get_MaxSegmentNum2ApproxCurve()) == GS_BAD) return GS_BAD;

            // la geometria è espressa da i-esimo vertice,x,y,[z],[bulge]
            PrefixStm += key_attrib;
            PrefixStm += _T(',');
            
            PrefixStm += x_attrib;
            PrefixStm += _T(',');
            PrefixStm += y_attrib;
            if (geom_dim == GS_3D && z_attrib.len() > 0)
            {
               PrefixStm += _T(',');
               PrefixStm += z_attrib;
            }
            if (bulge_attrib.len() > 0)
            {
               PrefixStm += _T(',');
               PrefixStm += bulge_attrib;
            }
         }

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            PrefixStm += _T(',');
            PrefixStm += ent_key_attrib;
         }

         if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
         {
            if (GeomKey != 0)
            {
               PrefixStm += _T(',');
               PrefixStm += key_attrib;
            }
         }
         else
         {  // il codice geometrico in queto caso rappresenta il codice dell'intera polilinea e 
            // non del songolo vertice che invece viene rappresentato da key_attrib;
            PrefixStm += _T(',');
            PrefixStm += vertex_parent_attrib;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            PrefixStm += _T(',');
            PrefixStm += aggr_factor_attrib;
         }

         if (layer_attrib.len() > 0) 
         {
            PrefixStm += _T(',');
            PrefixStm += layer_attrib;
         }

         if (color_attrib.len() > 0) 
         {
            PrefixStm += _T(',');
            PrefixStm += color_attrib;
         }

         if (line_type_attrib.len() > 0) 
         {
            PrefixStm += _T(',');
            PrefixStm += line_type_attrib;
         }

         if (line_type_scale_attrib.len() > 0) 
         {
            PrefixStm += _T(',');
            PrefixStm += line_type_scale_attrib;
         }

         if (width_attrib.len() > 0) 
         {
            PrefixStm += _T(',');
            PrefixStm += width_attrib;
         }

         if (thickness_attrib.len() > 0) 
         {
            PrefixStm += _T(',');
            PrefixStm += thickness_attrib;
         }

         // Lista nomi dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            PrefixStm += _T(',');
            PrefixStm += pSqlCondFieldValue->get_name();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         PrefixStm += _T(") VALUES (");

         C_POINT *pPt = (C_POINT *) PtList.get_head();
         int     i_vertex = 1;

         do
         {
            statement = PrefixStm;

            // nell'ordine geometria,ent_key,[key],[aggr_factor],
            // [layer],[color],[line_type],[scale],[width],[thickness]

            if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
            {
               C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

               if (WKTFromPolyline(pEnt, WKT) == GS_BAD) return GS_BAD;

               if (get_isCoordToConvert() == GS_GOOD)
                  SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

               // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
               // SDO_GEOMETRY(campo, srid) in ORACLE
               pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);
               statement += WKTToGeom_SQLSyntax;
            }
            else
            // Se geom_attrib è vuoto, la geometria è sotto forma di campi numerici
            {  // la geometria è espressa da i-esimo vertice,x,y,[z],[bulge]
               statement += i_vertex;
               statement += _T(',');
               statement += pPt->x();
               statement += _T(',');
               statement += pPt->y();
               if (geom_dim == GS_3D && z_attrib.len() > 0)
               { 
                  statement += _T(',');
                  statement += pPt->z();
               }
               if (bulge_attrib.len() > 0)
               { 
                  statement += _T(',');
                  statement += pPt->Bulge;
               }
            }

            if (pCls->get_category() != CAT_SPAGHETTI)
            {
               statement += _T(',');
               statement += eed.gs_id;
            }

            if (GeomKey != 0)
            {
               statement += _T(',');
               statement += GeomKey;
            }

            if (aggr_factor_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += eed.num_el;
            }

            if (layer_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += LayerValue;
            }

            if (color_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += ColorValue;
            }

            if (line_type_attrib.len() > 0)
            {
               statement += _T(',');
               statement += LineTypeValue;
            }

            if (line_type_scale_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += LineTypeScaleValue;
            }

            if (width_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += WidthValue;
            }

            if (thickness_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += ThicknessValue;
            }

            // Lista valori dei campi di condizione opzionale
            pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
            while (pSqlCondFieldValue)
            {
               statement += _T(',');
               statement += pSqlCondFieldValue->get_name2();
               pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
            }

            statement += _T(")");

            statement_list.add_tail_str(statement.get_name());

            if (geom_attrib.len() > 0) // formato OpenGis WKT (Well Know Text)
            {
               if (GeomKey == 0) // se campo chiave della geometria è autoincrementato
               { // aggiungo all'istruzione SQL "RETURNING ..."
               }

               break;
            }

            // se non si tratta di geometria in WKT continuo finchè ci sono vertici
            pPt = (C_POINT *) PtList.get_next(); // prossimo vertice
            i_vertex++;
         }
         while (pPt);

         break;
      } // inserimento nuovo oggetto grafico - fine

      case MODIFY:  // modifica oggetto grafico esistente - inizio
      {
         // Se geom_attrib è vuoto, la geometria è sotto forma di campi numerici
         if (geom_attrib.len() == 0)
         {
            C_STR_LIST NewPolyStm_list;

            // Cancello la polilinea e la reinserisco
            // con lo stesso codice geometrico
            if (SQLFromPolyline(pEnt, GeomKey, ERASE, statement_list) == GS_BAD)
               return GS_BAD;
            if (SQLFromPolyline(pEnt, GeomKey, INSERT, NewPolyStm_list) == GS_BAD)
               return GS_BAD;
            statement_list.paste_tail(NewPolyStm_list);

            return GS_GOOD;
         }

         // formato OpenGis WKT (Well Know Text)
         // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
         statement = _T("UPDATE ");
         statement += TableRef;
         statement += _T(" SET ");

         // nell'ordine geometria,ent_key,[aggr_factor],
         // [layer],[color],[line_type],[scale],[width],[thickness]
         C_STRING WKT, WKTToGeom_SQLSyntax, SrcSRID;

         if (WKTFromPolyline(pEnt, WKT) == GS_BAD) return GS_BAD;

         if (get_isCoordToConvert() == GS_GOOD)
            SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

         // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
         // SDO_GEOMETRY(campo, srid) in ORACLE
         pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

         statement += geom_attrib;
         statement += _T("=");
         statement += WKTToGeom_SQLSyntax;

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
            statement += _T("=");
            statement += eed.gs_id;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
            statement += _T("=");
            statement += eed.num_el;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
            statement += _T("=");
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
            statement += _T("=");
            statement += ColorValue;
         }

         if (line_type_attrib.len() > 0)
         {
            statement += _T(',');
            statement += line_type_attrib;
            statement += _T("=");
            statement += LineTypeValue;
         }

         if (line_type_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += line_type_scale_attrib;
            statement += _T("=");
            statement += LineTypeScaleValue;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
            statement += _T("=");
            statement += WidthValue;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
            statement += _T("=");
            statement += ThicknessValue;
         }

         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;
         statement_list.add_tail_str(statement.get_name());

         break;
      } // modifica oggetto grafico esistente - fine

      case ERASE: // cancellazione oggetto grafico esistente - inizio
      {
         // DELETE FROM Tabella WHERE key_attrib=valore
         statement = _T("DELETE FROM ");
         statement += TableRef;
         statement += _T(" WHERE ");

         // Se geom_attrib è vuoto, la geometria è sotto forma di campi numerici
         if (geom_attrib.len() == 0)
            // Cancello i vertici che hanno lo stesso parente geometrico
            statement += vertex_parent_attrib;
         else
            statement += key_attrib;

         statement += _T("=");
         statement += GeomKey;
         statement_list.add_tail_str(statement.get_name());

         break;
      } // cancellazione oggetto grafico esistente - fine
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::WKTFromPoint             <external> */
/*+
  Crea una stringa WKT relativa ad un oggetto grafico puntule.
  Parametri:
  AcDbEntity *pEnt;    Oggetto grafico di autocad puntuale 
                       (punto, testo, blocco, attributo di blocco)
  C_STRING &WKT;       Stringa risultato in WKT

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::WKTFromPoint(AcDbEntity *pEnt, C_STRING &WKT)
{
   ads_point point;

   if (!gsc_isPunctualEntity(pEnt))
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (gsc_get_firstPoint(pEnt, point) == GS_BAD) return GS_BAD;

   WKT = _T("POINT(");
   WKT += point[X];
   WKT += _T(" ");
   WKT += point[Y];
   if (geom_dim == GS_3D) { WKT += _T(" "); WKT += point[Z]; }
   WKT += _T(")");

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::WKTFromPolyline             <external> */
/*+
  Crea una stringa WKT relativa ad un oggetto grafico lineare.
  Parametri:
  AcDbEntity *pEnt;    Oggetto grafico di autocad lineare 
                       (line, polyline, 2Dpolyline)
  C_STRING &WKT;       Stringa risultato in WKT

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::WKTFromPolyline(AcDbEntity *pEnt, C_STRING &WKT)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_POINT_LIST   PtList;
   C_STRING       CoordStr;

   // Lista dei vertici e bulge
   if (PtList.add_vertex_point(pEnt, GS_GOOD,
                               pConn->get_MaxTolerance2ApproxCurve(),
                               pConn->get_MaxSegmentNum2ApproxCurve()) == GS_BAD) return GS_BAD;

   // se ha 2 vertici che sono coincidenti (lunghezza = 0)
   if (PtList.get_count() == 2 &&
      *((C_POINT *) PtList.get_head()) == *((C_POINT *) PtList.get_tail()))
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   // se il numero vertici è troppo grande per il provider OLE-DB
   if (PtList.get_count() > pConn->get_SQLMaxVertexNum())
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   WKT = _T("LINESTRING(");
   if (PtList.toStr(CoordStr, geom_dim, false, _T(','), _T(' '),
                    pConn->get_MaxTolerance2ApproxCurve(),
                    pConn->get_MaxSegmentNum2ApproxCurve()) < 0)
      return GS_BAD;
   WKT += CoordStr;
   WKT += _T(")");

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::WKTFromMPolygonLoop      <internal> */
/*+
  Crea una stringa WKT relativa ad un loo di un MPolygon.
  Parametri:
  AcDbMPolygon *pMPolygon;    Oggetto autocad MPolygon
  int LoopIndex;              Numero del loop
  C_STRING &WKT;              Stringa risultato in WKT

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::WKTFromMPolygonLoop(AcDbMPolygon *pMPolygon, int LoopIndex, C_STRING &WKT)
{
   AcGePoint2dArray vertices;
   AcGeDoubleArray  bulges;
   C_POINT_LIST     PtList;
   C_STRING         CoordStr;
   C_DBCONNECTION   *pConn = getDBConnection();

   if (pMPolygon->getMPolygonLoopAt(LoopIndex, vertices, bulges) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   if (PtList.add_vertex_point(vertices, bulges) == GS_BAD) return GS_BAD;  
   // se il numero vertici è troppo grande per il provider OLE-DB
   if (PtList.get_count() > pConn->get_SQLMaxVertexNum())
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   // verifico che abbia almeno 4 vertici (il primo e l'ultimo si ripetono)
   if (PtList.toStr(CoordStr, geom_dim, false, _T(','), _T(' '),
                    pConn->get_MaxTolerance2ApproxCurve(),
                    pConn->get_MaxSegmentNum2ApproxCurve()) < 4)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }
   WKT = _T("(");
   WKT += CoordStr;
   WKT += _T(")");

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::WKTFromMPolygon           <external> */
/*+
  Crea una stringa WKT relativa ad un poligono.
  Parametri:
  AcDbMPolygon *pMPolygon;    oggetto MPolygon di autocad
  C_STRING &WKT;              Stringa risultato in WKT
  oppure
  AcDbEntityPtrArray &EntArray;  vettore alle polilinee del poligono (lwpolyline, polyline...)
  C_STRING &WKT;                 Stringa risultato in WKT
  oppure
  C_SELSET &ss;                  gruppo di selezione delle polilinee del poligono (lwpolyline, polyline...)
  C_STRING &WKT;                 Stringa risultato in WKT

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::WKTFromMPolygon(AcDbMPolygon *pMPolygon, C_STRING &WKT)
{
   AcDbMPolygonNode *pRootNode, *pNode, *pHole;
   C_STRING         Loop_WKT;

   if (pMPolygon->getMPolygonTree(pRootNode) != Acad::eOk)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   if (pRootNode->mChildren.length() == 0)
      { GS_ERR_COD = eGSInvGraphObjct; return GS_BAD; }

   WKT = _T("MULTIPOLYGON(");
   for (int i = 0; i < pRootNode->mChildren.length(); i++)
   {
      pNode = pRootNode->mChildren[i];
      if (WKTFromMPolygonLoop(pMPolygon, pNode->mLoopIndex, Loop_WKT) == GS_BAD)
         return GS_BAD;
      WKT += (i == 0) ? _T("(") : _T(",(");
      WKT += Loop_WKT;

      // Considero i buchi
      for (int j = 0; j < pNode->mChildren.length(); j++)
      {
         pHole = pNode->mChildren[j];
         if (WKTFromMPolygonLoop(pMPolygon, pHole->mLoopIndex, Loop_WKT) == GS_BAD)
            return GS_BAD;
         WKT += _T(",");
         WKT += Loop_WKT;
      }
      WKT += _T(")");
   }
   WKT += _T(")");

   pMPolygon->deleteMPolygonTree(pRootNode);

   return GS_GOOD;
}
int C_DBGPH_INFO::WKTFromPolygon(AcDbEntityPtrArray &EntArray, C_STRING &WKT)
{  // scarto i riempimenti
   C_DBCONNECTION   *pConn = getDBConnection();
   AcDbMPolygon     *pMPolygon;
   int              Result;
   AcDbEntity       *pEnt;
   AcDbEntityPtrArray MyEntArray;
   AcDbEntityPtrArray MyEntArrayToDelete;

   // Se si tratta di un MPolygon
   if (EntArray.length() == 1 && EntArray.at(0)->isKindOf(AcDbMPolygon::desc()))
      return WKTFromMPolygon(((AcDbMPolygon *) EntArray.at(0)), WKT);

   for (int i = 0; i < EntArray.length(); i++)
   {
      pEnt = EntArray.at(i);
      if (pEnt->isKindOf(AcDbPolyline::desc()) ||
          pEnt->isKindOf(AcDb2dPolyline::desc()) ||
          pEnt->isKindOf(AcDbCircle::desc()))
         MyEntArray.append(pEnt);
      else if (pEnt->isKindOf(AcDbSolid::desc()) ||
               pEnt->isKindOf(AcDbFace::desc()) ||
               pEnt->isKindOf(AcDbEllipse::desc()))
      {  // Provo a trasformarlo in polilinea
         C_POINT_LIST PtList;
         AcDbCurve    *pConvertedEnt;

         if (PtList.add_vertex_point(pEnt, GS_GOOD,
                                     pConn->get_MaxTolerance2ApproxCurve(),
                                     pConn->get_MaxSegmentNum2ApproxCurve()) == GS_BAD)
            return GS_BAD;
         if (!(pConvertedEnt = PtList.toPolyline())) return GS_BAD;
         MyEntArray.append(pConvertedEnt);
         MyEntArrayToDelete.append(pConvertedEnt);
      }
   }

   pMPolygon = gsc_create_MPolygon(MyEntArray);
   for (int i = 0; i < MyEntArrayToDelete.length(); i++) delete (AcDbCurve *) MyEntArrayToDelete[i];
   if (!pMPolygon) return GS_BAD;
   Result = WKTFromMPolygon(pMPolygon, WKT);
   delete pMPolygon;

   return Result;
}
int C_DBGPH_INFO::WKTFromPolygon(C_SELSET &ss, C_STRING &WKT)
{
   AcDbMPolygon *pMPolygon;
   int          Result;
   
   if ((pMPolygon = gsc_create_MPolygon(ss)) == NULL) return GS_BAD;
   Result = WKTFromMPolygon(pMPolygon, WKT);
   delete pMPolygon;

   return Result;
}
int C_DBGPH_INFO::WKTFromPolygon(AcDbEntity *pEnt, C_STRING &WKT)
{
   C_DBCONNECTION     *pConn = getDBConnection();
   AcDbMPolygon       *pMPolygon;
   int                Result;
   AcDbEntityPtrArray MyEntArray;
   AcDbEntityPtrArray MyEntArrayToDelete;

   if (pEnt->isKindOf(AcDbPolyline::desc()) ||
       pEnt->isKindOf(AcDb2dPolyline::desc()) ||
       pEnt->isKindOf(AcDbCircle::desc()))
      MyEntArray.append(pEnt);
   else if (pEnt->isKindOf(AcDbSolid::desc()) ||
            pEnt->isKindOf(AcDbFace::desc()) ||
            pEnt->isKindOf(AcDbEllipse::desc()))
   {  // Provo a trasformarlo in polilinea
      C_POINT_LIST PtList;
      AcDbCurve    *pConvertedEnt;

      if (PtList.add_vertex_point(pEnt, GS_GOOD,
                                  pConn->get_MaxTolerance2ApproxCurve(),
                                  pConn->get_MaxSegmentNum2ApproxCurve()) == GS_BAD)
         return GS_BAD;
      if (!(pConvertedEnt = PtList.toPolyline())) return GS_BAD;
      MyEntArray.append(pConvertedEnt);
      MyEntArrayToDelete.append(pConvertedEnt);
   }
   else if (pEnt->isKindOf(AcDbHatch::desc()))
   {
      return WKTFromPolygon((AcDbHatch *) pEnt, WKT);
   }
   else
   {
      GS_ERR_COD = eGSInvGraphObjct;
      return GS_BAD;
   }

   pMPolygon = gsc_create_MPolygon(MyEntArray);
   for (int i = 0; i < MyEntArrayToDelete.length(); i++) delete (AcDbCurve *) MyEntArrayToDelete[i];
   if (!pMPolygon) return GS_BAD;
   Result = WKTFromMPolygon(pMPolygon, WKT);
   delete pMPolygon;

   return Result;
}
int C_DBGPH_INFO::WKTFromPolygon(AcDbHatch *pEnt, C_STRING &WKT)
{
   C_DBCONNECTION     *pConn = getDBConnection();
   AcDbMPolygon       *pMPolygon;
   int                Result;
   AcDbEntityPtrArray MyEntArrayToDelete;

   // Ciclo i loop di un hatch
   for (int i = 0; i < pEnt->numLoops(); i++)
   {  // Provo a trasformarlo in polilinea
      C_POINT_LIST PtList;
      AcDbCurve    *pConvertedEnt;

      if (PtList.add_vertex_point_from_hatch(pEnt, i, 
                                             pConn->get_MaxTolerance2ApproxCurve(),
                                             pConn->get_MaxSegmentNum2ApproxCurve()) != GS_GOOD)
         return GS_BAD;
      if (!(pConvertedEnt = PtList.toPolyline())) return GS_BAD;
      MyEntArrayToDelete.append(pConvertedEnt);
   }

   if ((pMPolygon = gsc_create_MPolygon(MyEntArrayToDelete)) == NULL)
   {
      for (int i = 0; i < MyEntArrayToDelete.length(); i++) delete (AcDbCurve *) MyEntArrayToDelete[i];
      return GS_BAD;
   }
   Result = WKTFromMPolygon(pMPolygon, WKT);
   delete pMPolygon;

   return Result;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::WKTToPolygonLoop         <internal> */
/*+
  Crea una polilinea relativo ad un anello del poligono relativo alla stringa WKT.
  Parametri:
  TCHAR **WKT;             Stringa in WKT da aperta tonda che viene spostata a chiusa tonda
  AcDbCurve *pEnt;         Oggetto autocad polilinea (lwpolyline, polyline...)
  C_EED &eed;              Entità estesa x i marker di GEOsim + gs_id + fattore di aggregazione
  const TCHAR *Layer;      (default = NULL)
  C_COLOR *Color;          (default = NULL)
  const TCHAR *LineType;   (default = NULL)
  double LineTypeScale;    (default -1)
  double Width;            (default -1)
  double Thickness         (default -1)
  bool *Plinegen;          (default = NULL), usato solo per linee 2D
  int *nRead;              Opzionale, ritorna il numero di caratteri letti (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria
-*/  
/*********************************************************/
int C_DBGPH_INFO::WKTToPolygonLoop(TCHAR **WKT, AcDbCurve **pEnt,
                                   C_EED &eed, const TCHAR *Layer, 
                                   C_COLOR *Color, const TCHAR *LineType, 
                                   double LineTypeScale, double Width, double Thickness,
                                   bool *Plinegen, int *nRead)
{
   // esempio: (0 0,10 0,10 10,0 10,0 0)
   TCHAR        *p = *WKT; // inizio anello
   C_STRING     Polyline;
   C_POINT_LIST PtList;

   p++;
   if ((*WKT = wcschr(p, _T(')'))) == NULL) return GS_BAD; // fine anello
      
   Polyline.set_name(p, 0, (int) (*WKT - p - 1));

   // Il separatore tra punti = ',' quello tra coordinate = ' '
   if (PtList.fromStr(Polyline.get_name(), geom_dim, false, _T(','), _T(' '), nRead) == GS_BAD)
      return GS_BAD;

   nRead++; // prima tonda
   // tolgo l'ultimo punto che è uguale al primo e setto il flag di polilinea chiusa
   PtList.remove_tail();
   // Creo la polilinea
   if ((*pEnt = PtList.toPolyline(Layer, Color, LineType, LineTypeScale, Width, Thickness, Plinegen)) == NULL)
      return GS_BAD;

   // Chiudo la polilinea (altrimenti non vanno funzioni tipo il calcolo della sessione ".area")
   if (gsc_close_pline(*pEnt) == GS_BAD)
      { delete *pEnt; return GS_BAD; }

   // Aggiungo i marker di GEOsim + gs_id + fattore di aggregazione
   if (eed.save(*pEnt) == GS_BAD)
      { delete *pEnt; return GS_BAD; }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::WKTToPolygon           <external> */
/*+
  Crea un vettore alle polilinee del poligono relativo alla stringa WKT.
  Parametri:
  const TCHAR *WKT;             Stringa in WKT
  AcDbEntityPtrArray &EntArray;  Vettore alle polilinee del poligono (lwpolyline, polyline...)
  C_EED &eed;                   Entità estesa x i marker di GEOsim + gs_id + fattore di aggregazione
  const TCHAR *Layer;           (default = NULL)
  C_COLOR *Color;               (default = NULL)
  const TCHAR *LineType;        (default = NULL)
  double LineTypeScale;         (default -1)
  double Width;                 (default -1)
  double Thickness              (default -1)
  bool *Plinegen;          (default = NULL), usato solo per linee 2D

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::WKTToPolygon(TCHAR *pWKTGeom, AcDbEntityPtrArray &EntArray,
                               C_EED &eed, const TCHAR *Layer, 
                               C_COLOR *Color, const TCHAR *LineType, 
                               double LineTypeScale, double Width, double Thickness,
                               bool *Plinegen)
{
   // esempio: POLYGON((0 0,10 0,10 10,0 10,0 0),(5 5,7 5,7 7,5 7, 5 5))
   // esempio: MULTIPOLYGON(((40 40, 20 45, 45 30, 40 40)),
   //                       ((20 35, 45 20, 30 5, 10 10, 10 30, 20 35),(30 20, 20 25, 20 15, 30 20)))
   AcDbCurve *pEnt;

   EntArray.removeAll();
   while (*pWKTGeom != _T('\0'))
   {
      if (*pWKTGeom == _T('(') && *(pWKTGeom + 1) != _T('('))
      {
         // leggo anello
         if (WKTToPolygonLoop(&pWKTGeom, &pEnt, eed, Layer, Color,
                              LineType, LineTypeScale,
                              Width, Thickness, Plinegen) == GS_BAD)
         {
            for (int i = 0; i < EntArray.length(); i++) delete ((AcDbEntity *) EntArray[i]);
            EntArray.removeAll();
            return GS_BAD;
         }
         EntArray.append(pEnt);
      }
      else
         pWKTGeom++;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromPolygon           <internal> */
/*+
  Crea una stringa di istruzione SQL per geometria SOLO in formato
  WKT a partire da un poligono GEOsim che è composto da una o più polilinee.
  A seconda della modalità scelta sarà un'istruzione SQL di 
  inserimento, modifica o di cancellazione.
  Nel caso in cui la geometria fosse espressa tramite campi numerici 
  bisogna usare la funzione C_DBGPH_INFO::SQLFromPolyline.
  Parametri:
  AcDbEntityPtrArray &EntArray;  vettore alle polilinee del poligono (lwpolyline, polyline...)
  long               GeomKey;    codice dell'oggetto geometrico
                                 (se si sta inserendo e questo codice = 0 significa
                                 che il campo è un autoincremento e quindi non 
                                 impostabile da una istruz. SQL)
  int                Mode;       tipo di istruzione SQL da generare:
                                 INSERT = inserimento, MODIFY = aggiornamento,
                                 ERASE = cancellazione
  C_STRING           &statement; Istruzione SQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromPolygon(AcDbEntityPtrArray &EntArray, long GeomKey,
                                 int Mode, C_STRING &statement)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_POINT_LIST   PtList;
   C_EED          eed;
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   C_STRING       CoordStr;
   double         HatchScale = 1;
   int            IndexFirstPolyline = -1;
   C_COLOR        HatchColor;
   C_STRING       WKT, WKTToGeom_SQLSyntax;
   //             Valori delle caratteristiche grafiche trasformate in stringhe SQL  
   C_STRING       LayerValue, LineTypeValue, LineTypeScaleValue, WidthValue, ThicknessValue;
   C_STRING       HatchValue, HatchLayerValue, HatchRotValue, ColorValue, HatchColorValue;
   C_2STR         *pSqlCondFieldValue;

   // Questa funzione supporta solo il formato OpenGis WKT (Well Know Text)
   if (geom_attrib.len() == 0) return GS_BAD;

   if (!pCls) return GS_BAD;

   if (Mode != ERASE) // se non devo cancellare
   {  // leggo le caratteristiche dell'oggetto grafico
      double   dummyDbl, HatchRotation = 0, UnitConvFactor;
      TCHAR    HatchName[MAX_LEN_HATCHNAME] = _T(""), HatchLayerName[MAX_LEN_LAYERNAME] = _T("");
      C_STRING SrcSRID;

      UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();

      // Fattore di aggregazione e codice entità
      if (eed.load((EntArray.at(0))) == GS_BAD) return GS_BAD;

      // Cerco il primo riempimento tra le entità in EntArray
      for (int i = 0; i < EntArray.length(); i++)
         // scarto i riempimenti dei poligoni perchè non sono oggetti reali di autocad (es. non hanno handle)
         if (EntArray.at(i)->isKindOf(AcDbMPolygon::desc()) == false &&
             gsc_getInfoHatch(EntArray.at(i),
                              HatchName, &HatchScale, &HatchRotation,
                              HatchLayerName, &HatchColor) == GS_GOOD)
         {
            if (IndexFirstPolyline != -1) break; // se è gia stato trovato l'indice del primo oggetto non riempimento
         }
         else
            // memorizzo l'indice del primo oggetto non riempimento
            if (IndexFirstPolyline == -1) IndexFirstPolyline = i; 

      // se IndexFirstPolyline = -1 significa che si stava salvando solo un riempimento o un mpolygon
      if (IndexFirstPolyline == -1) IndexFirstPolyline = 0;

      if (WKTFromPolygon(EntArray, WKT) == GS_BAD) return GS_BAD;

      if (get_isCoordToConvert() == GS_GOOD)
         SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

      // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
      // SDO_GEOMETRY(campo, srid) in ORACLE
      pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

      // Layer
      if (layer_attrib.len() > 0) 
      {
         gsc_getLayer(EntArray.at(IndexFirstPolyline), LayerValue);
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(LayerValue) == GS_BAD) return GS_BAD;
      }
      // Tipolinea
      if (line_type_attrib.len() > 0)
      {
         gsc_get_lineType(EntArray.at(IndexFirstPolyline), LineTypeValue);
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(LineTypeValue) == GS_BAD) return GS_BAD;
      }
      // Scala tipo linea
      if (line_type_scale_attrib.len() > 0) 
      {
         dummyDbl = EntArray.at(IndexFirstPolyline)->linetypeScale();
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            LineTypeScaleValue = dummyDbl / UnitConvFactor;
         else
            LineTypeScaleValue = dummyDbl;
      }

      // Larghezza
      if (width_attrib.len() > 0) 
      {
         if (gsc_get_width(EntArray.at(IndexFirstPolyline), &dummyDbl) == GS_BAD) return GS_BAD;
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            WidthValue = dummyDbl / UnitConvFactor;
         else
            WidthValue = dummyDbl;
      }
      // Spessore verticale
      if (thickness_attrib.len() > 0) 
      {
         gsc_get_thickness(EntArray.at(IndexFirstPolyline), &dummyDbl);
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            ThicknessValue = dummyDbl / UnitConvFactor;
         else
            ThicknessValue = dummyDbl;
      }
      // Riempimento
      if (hatch_attrib.len() > 0)
      {
         HatchValue = HatchName;
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(HatchValue) == GS_BAD) return GS_BAD;
      }
      // Layer del riempimento
      if (hatch_layer_attrib.len() > 0) 
      {
         HatchLayerValue = HatchLayerName;
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(HatchLayerValue) == GS_BAD) return GS_BAD;
      }
      // Rotazione del riempimento
      if (hatch_rotation_attrib.len() > 0) 
         HatchRotValue = CounterClockwiseRadiantToRotationUnit(HatchRotation);
      // Scala del riempimento
      if (hatch_scale_attrib.len() > 0) 
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            HatchScale = HatchScale / UnitConvFactor;

      // Color
      if (color_attrib.len() > 0)
      {
         C_COLOR color;
         if (gsc_get_color(EntArray.at(IndexFirstPolyline), color) == GS_BAD) return GS_BAD;
         Color2SQLValue(color, ColorValue);
      }
      // Layer del riempimento
      if (hatch_color_attrib.len() > 0) 
         Color2SQLValue(HatchColor, HatchColorValue);      
   }

   // Tipi di operazioni
   switch (Mode)
   {
      case INSERT: // inserimento nuovo oggetto grafico - inizio
      {
         // INSERT INTO Tabella (colonna1, colonna 2, ...) VALUES (valore1, valore2, ...)
         statement = _T("INSERT INTO ");
         statement += TableRef;
         statement += _T(" (");
         
         // nell'ordine geometria,ent_key,[key],[aggr_factor],
         // [layer],[color],[line_type],[scale],[width],[thickness],
         // [HatchName],[HatchLayer],[HatchColor],[HatchScale],[HatchRot]
         
         statement += geom_attrib;

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += key_attrib;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
         }

         if (line_type_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += line_type_attrib;
         }

         if (line_type_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += line_type_scale_attrib;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
         }

         if (hatch_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_attrib;
         }

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_layer_attrib;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_color_attrib;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_scale_attrib;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_rotation_attrib;
         }

         // Lista nomi dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(") VALUES (");

         // nell'ordine geometria,ent_key,[key],[aggr_factor],
         // [layer],[color],[line_type],[scale],[width],[thickness],
         // [HatchName],[HatchLayer],[HatchColor],[HatchScale],[HatchRot]

         statement += WKTToGeom_SQLSyntax;

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += eed.gs_id;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += GeomKey;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += eed.num_el;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ColorValue;
         }

         if (line_type_attrib.len() > 0)
         {
            statement += _T(',');
            statement += LineTypeValue;
         }

         if (line_type_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += LineTypeScaleValue;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += WidthValue;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ThicknessValue;
         }

         if (hatch_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchValue;
         }

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchLayerValue;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchColorValue;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchScale;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchRotValue;
         }

         // Lista valori dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name2();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(")");

         if (GeomKey == 0) // se campo chiave della geometria è autoincrementato
         { // aggiungo all'istruzione SQL "RETURNING ..."
         }

         break;
      } // inserimento nuovo oggetto grafico - fine

      case MODIFY:  // modifica oggetto grafico esistente - inizio
      {
         // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
         statement = _T("UPDATE ");
         statement += TableRef;
         statement += _T(" SET ");

         // nell'ordine geometria,ent_key,[aggr_factor],
         // [layer],[color],[line_type],[scale],[width],[thickness],
         // [HatchName],[HatchLayer],[HatchColor],[HatchScale],[HatchRot]
         statement += geom_attrib;
         statement += _T("=");

         statement += WKTToGeom_SQLSyntax;

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
            statement += _T("=");
            statement += eed.gs_id;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
            statement += _T("=");
            statement += eed.num_el;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
            statement += _T("=");
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
            statement += _T("=");
            statement += ColorValue;
         }

         if (line_type_attrib.len() > 0)
         {
            statement += _T(',');
            statement += line_type_attrib;
            statement += _T("=");
            statement += LineTypeValue;
         }

         if (line_type_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += line_type_scale_attrib;
            statement += _T("=");
            statement += LineTypeScaleValue;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
            statement += _T("=");
            statement += WidthValue;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
            statement += _T("=");
            statement += ThicknessValue;
         }

         if (hatch_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_attrib;
            statement += _T("=");
            statement += HatchValue;
         }

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_layer_attrib;
            statement += _T("=");
            statement += HatchLayerValue;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_color_attrib;
            statement += _T("=");
            statement += HatchColorValue;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_scale_attrib;
            statement += _T("=");
            statement += HatchScale;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_rotation_attrib;
            statement += _T("=");
            statement += HatchRotValue;
         }

         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // modifica oggetto grafico esistente - fine
   
      case ERASE: // cancellazione oggetto grafico esistente - inizio
      {
         // DELETE FROM Tabella WHERE key_attrib=valore
         statement = _T("DELETE FROM ");
         statement += TableRef;
         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // cancellazione oggetto grafico esistente - fine
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromMPolygon             <internal> */
/*+
  Crea una stringa di istruzione SQL per geometria SOLO in formato
  WKT a partire da un mpolygon.
  A seconda della modalità scelta sarà un'istruzione SQL di 
  inserimento, modifica o di cancellazione.
  Parametri:
  AcDbMPolygon *pEnt;   puntatore all'oggetto autocad mpolygon
  long      GeomKey;    codice dell'oggetto geometrico a cui va associato il riempimento
  int       Mode;       tipo di istruzione SQL da generare:
                        INSERT = inserimento, MODIFY = aggiornamento,
                        ERASE = cancellazione
  C_STRING  &statement; Istruzione SQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromMPolygon(AcDbMPolygon *pEnt, long GeomKey, int Mode, C_STRING &statement)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_POINT_LIST   PtList;
   C_EED          eed;
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   C_STRING       CoordStr;
   double         HatchScale = 1;
   int            IndexFirstPolyline = -1;
   C_STRING       WKT, WKTToGeom_SQLSyntax;
   //             Valori delle caratteristiche grafiche trasformate in stringhe SQL  
   C_STRING       LayerValue, LineTypeValue, LineTypeScaleValue, WidthValue, ThicknessValue;
   C_STRING       HatchValue, HatchLayerValue, HatchScaleValue, HatchRotValue, ColorValue, HatchColorValue; 
   C_2STR         *pSqlCondFieldValue;

   // Questa funzione supporta solo il formato OpenGis WKT (Well Know Text)
   if (geom_attrib.len() == 0) return GS_BAD;

   if (!pCls) return GS_BAD;

   if (Mode != ERASE) // se non devo cancellare
   {  // leggo le caratteristiche dell'oggetto grafico
      double   dummyDbl, HatchRotation = 0, UnitConvFactor;
      TCHAR    HatchName[MAX_LEN_HATCHNAME] = _T(""), HatchLayerName[MAX_LEN_LAYERNAME] = _T("");
      C_COLOR  HatchColor;
      C_STRING SrcSRID;
      
      UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();

      // scarto i riempimenti dei poligoni perchè non sono oggetti reali di autocad (es. non hanno handle)
      // gsc_getInfoHatch(pEnt, HatchName, &HatchScale, &HatchRotation, HatchLayerName, &HatchColor);

      if (WKTFromMPolygon(pEnt, WKT) == GS_BAD) return GS_BAD;

      if (get_isCoordToConvert() == GS_GOOD)
         SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

      // Converto in geometria es. st_geomfromtext(campo, srid) in POSTGIS
      // SDO_GEOMETRY(campo, srid) in ORACLE
      pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);

      // Layer
      if (layer_attrib.len() > 0) 
      {
         gsc_getLayer(pEnt, LayerValue);
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(LayerValue) == GS_BAD) return GS_BAD;
      }
      // Tipolinea
      if (line_type_attrib.len() > 0)
      {
         gsc_get_lineType(pEnt, LineTypeValue);
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(LineTypeValue) == GS_BAD) return GS_BAD;
      }
      // Scala tipo linea
      if (line_type_scale_attrib.len() > 0) 
      {
         dummyDbl = pEnt->linetypeScale();
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            LineTypeScaleValue = dummyDbl / UnitConvFactor;
         else
            LineTypeScaleValue = dummyDbl;
      }
      // Larghezza
      if (width_attrib.len() > 0) 
      {
         if (gsc_get_width(((AcDbEntity *) pEnt), &dummyDbl) == GS_BAD) return GS_BAD;
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            WidthValue = dummyDbl / UnitConvFactor;
         else
            WidthValue = dummyDbl;
      }
      // Spessore verticale
      if (thickness_attrib.len() > 0) 
      {
         gsc_get_thickness(((AcDbEntity *) pEnt), &dummyDbl);
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            ThicknessValue = dummyDbl / UnitConvFactor;
         else
            ThicknessValue = dummyDbl;
      }
      // Riempimento
      if (hatch_attrib.len() > 0)
      {
         HatchValue = HatchName;
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(HatchValue) == GS_BAD) return GS_BAD;
      }
      // Layer del riempimento
      if (hatch_layer_attrib.len() > 0) 
      {
         HatchLayerValue = HatchLayerName;
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(HatchLayerValue) == GS_BAD) return GS_BAD;
      }
      // Rotazione del riempimento
      if (hatch_rotation_attrib.len() > 0) 
         HatchRotValue = CounterClockwiseRadiantToRotationUnit(HatchRotation);
      // Color
      if (color_attrib.len() > 0)
      {
         C_COLOR color;
         if (gsc_get_color(pEnt, color) == GS_BAD) return GS_BAD;
         Color2SQLValue(color, ColorValue);
      }
      // Colore del riempimento
      if (hatch_color_attrib.len() > 0) 
         Color2SQLValue(HatchColor, HatchColorValue);
   }

   // Tipi di operazioni
   switch (Mode)
   {
      case INSERT: // inserimento nuovo oggetto grafico - inizio
      {
         // INSERT INTO Tabella (colonna1, colonna 2, ...) VALUES (valore1, valore2, ...)
         statement = _T("INSERT INTO ");
         statement += TableRef;
         statement += _T(" (");
         
         // nell'ordine geometria,ent_key,[key],[aggr_factor],
         // [layer],[color],[line_type],[scale],[width],[thickness],
         // [HatchName],[HatchLayer],[HatchColor],[HatchScale],[HatchRot]
         
         statement += geom_attrib;

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += key_attrib;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
         }

         if (line_type_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += line_type_attrib;
         }

         if (line_type_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += line_type_scale_attrib;
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
         }

         if (hatch_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_attrib;
         }

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_layer_attrib;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_color_attrib;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_scale_attrib;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_rotation_attrib;
         }

         // Lista nomi dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(") VALUES (");

         // nell'ordine geometria,ent_key,[key],[aggr_factor],
         // [layer],[color],[line_type],[scale],[width],[thickness],
         // [HatchName],[HatchLayer],[HatchColor],[HatchScale],[HatchRot]

         statement += WKTToGeom_SQLSyntax;

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += eed.gs_id;
         }

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += GeomKey;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += eed.num_el;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ColorValue;
         }

         if (line_type_attrib.len() > 0)
         {
            statement += _T(',');
            statement += LineTypeValue;
         }

         if (line_type_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ((AcDbEntity *) pEnt)->linetypeScale();
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += WidthValue;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += ThicknessValue;
         }

         if (hatch_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchValue;
         }

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchLayerValue;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchColorValue;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchScale;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchRotValue;
         }

         // Lista valori dei campi di condizione opzionale
         pSqlCondFieldValue = (C_2STR *) FieldValueListOnTable.get_head();
         while (pSqlCondFieldValue)
         {
            statement += _T(',');
            statement += pSqlCondFieldValue->get_name2();
            pSqlCondFieldValue = (C_2STR *) pSqlCondFieldValue->get_next();
         }

         statement += _T(")");

         if (GeomKey == 0) // se campo chiave della geometria è autoincrementato
         { // aggiungo all'istruzione SQL "RETURNING ..."
         }

         break;
      } // inserimento nuovo oggetto grafico - fine

      case MODIFY:  // modifica oggetto grafico esistente - inizio
      {
         // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
         statement = _T("UPDATE ");
         statement += TableRef;
         statement += _T(" SET ");

         // nell'ordine geometria,ent_key,[aggr_factor],
         // [layer],[color],[line_type],[scale],[width],[thickness],
         // [HatchName],[HatchLayer],[HatchColor],[HatchScale],[HatchRot]
         statement += geom_attrib;
         statement += _T("=");

         statement += WKTToGeom_SQLSyntax;

         if (pCls->get_category() != CAT_SPAGHETTI)
         {
            statement += _T(',');
            statement += ent_key_attrib;
            statement += _T("=");
            statement += eed.gs_id;
         }

         if (aggr_factor_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += aggr_factor_attrib;
            statement += _T("=");
            statement += eed.num_el;
         }

         if (layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += layer_attrib;
            statement += _T("=");
            statement += LayerValue;
         }

         if (color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += color_attrib;
            statement += _T("=");
            statement += ColorValue;
         }

         if (line_type_attrib.len() > 0)
         {
            statement += _T(',');
            statement += line_type_attrib;
            statement += _T("=");
            statement += LineTypeValue;
         }

         if (line_type_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += line_type_scale_attrib;
            statement += _T("=");
            statement += ((AcDbEntity *) pEnt)->linetypeScale();
         }

         if (width_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += width_attrib;
            statement += _T("=");
            statement += WidthValue;
         }

         if (thickness_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += thickness_attrib;
            statement += _T("=");
            statement += ThicknessValue;
         }

         if (hatch_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_attrib;
            statement += _T("=");
            statement += HatchValue;
         }

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_layer_attrib;
            statement += _T("=");
            statement += HatchLayerValue;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_color_attrib;
            statement += _T("=");
            statement += HatchColorValue;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_scale_attrib;
            statement += _T("=");
            statement += HatchScale;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_rotation_attrib;
            statement += _T("=");
            statement += HatchRotValue;
         }

         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // modifica oggetto grafico esistente - fine
   
      case ERASE: // cancellazione oggetto grafico esistente - inizio
      {
         // DELETE FROM Tabella WHERE key_attrib=valore
         statement = _T("DELETE FROM ");
         statement += TableRef;
         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // cancellazione oggetto grafico esistente - fine
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromHatch             <internal> */
/*+
  Crea una stringa di istruzione SQL per geometria SOLO in formato
  WKT a partire da un riempimento.
  A seconda della modalità scelta sarà un'istruzione SQL di 
  inserimento, modifica o di cancellazione.
  Parametri:
  AcDbHatch *pEnt;      puntatore all'oggetto autocad riempimento
  long      GeomKey;    codice dell'oggetto geometrico a cui va associato il riempimento
  int       Mode;       tipo di istruzione SQL da generare:
                        INSERT = inserimento, MODIFY = aggiornamento,
                        ERASE = cancellazione
  C_STRING  &statement; Istruzione SQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromHatch(AcDbHatch *pEnt, long GeomKey, int Mode, C_STRING &statement)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_CLASS        *pCls = gsc_find_class(prj, cls, sub);
   double         HatchScale = 1;
   C_COLOR        HatchColor;
   C_STRING       WKT, WKTToGeom_SQLSyntax;
   //             Valori delle caratteristiche grafiche trasformate in stringhe SQL  
   C_STRING       HatchValue, HatchLayerValue, HatchRotValue, HatchColorValue;

   if (hatch_attrib.len() == 0) return GS_GOOD; // Non supporta i riempimenti vuoti

   if (!pCls) return GS_BAD;

   if (Mode != ERASE) // se non devo cancellare
   {  // leggo le caratteristiche dell'oggetto grafico
      double HatchRotation = 0, UnitConvFactor;
      TCHAR  HatchName[MAX_LEN_HATCHNAME] = _T(""), HatchLayerName[MAX_LEN_LAYERNAME] = _T("");

      UnitConvFactor = get_UnitCoordConvertionFactorFromClsToWrkSession();

      if (gsc_getInfoHatch(pEnt, HatchName, &HatchScale, &HatchRotation,
                           HatchLayerName, &HatchColor) != GS_GOOD) return GS_BAD;

      // per la classe spaghetti gli hatch sono oggetti indipendenti
      if (pCls->get_category() == CAT_SPAGHETTI)
      {
         C_STRING SrcSRID;

         if (WKTFromPolygon(pEnt, WKT) == GS_BAD) return GS_BAD;

         if (get_isCoordToConvert() == GS_GOOD)
            SrcSRID = get_CurrWrkSessionSRID_converted_to_DBSRID()->get_name();

         // Converto in geometria
         // es. st_geomfromtext(campo, srid) in POSTGIS
         //     SDO_GEOMETRY(campo, srid) in ORACLE
         pConn->get_WKTToGeom_SQLSyntax(WKT, SrcSRID, coordinate_system, WKTToGeom_SQLSyntax);
      }
      else // Solo per gli spaghetti è permesso Mode = INSERT
         Mode = MODIFY;

      // Riempimento
      if (hatch_attrib.len() > 0)
      {
         HatchValue = HatchName;
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(HatchValue) == GS_BAD) return GS_BAD;
      }
      // Layer del riempimento
      if (hatch_layer_attrib.len() > 0) 
      {
         HatchLayerValue = HatchLayerName;
         // Correggo la stringa secondo la sintassi SQL 
         if (pConn->Str2SqlSyntax(HatchLayerValue) == GS_BAD) return GS_BAD;
      }
      // Rotazione del riempimento
      if (hatch_rotation_attrib.len() > 0) 
         HatchRotValue = CounterClockwiseRadiantToRotationUnit(HatchRotation);
      // Scala del riempimento
      if (hatch_scale_attrib.len() > 0) 
         if (UnitConvFactor != 0 && UnitConvFactor != 1) // se valido e diverso da 1
            HatchScale = HatchScale / UnitConvFactor;
      // Colore del riempimento
      if (hatch_color_attrib.len() > 0) 
         Color2SQLValue(HatchColor, HatchColorValue);
   }

   // Tipi di operazioni
   switch (Mode)
   {
      case INSERT: // inserimento nuovo oggetto grafico - inizio (solo per spaghetti)
      {
         // INSERT INTO Tabella (colonna1, colonna 2, ...) VALUES (valore1, valore2, ...)
         statement = _T("INSERT INTO ");
         statement += TableRef;
         statement += _T(" (");
         
         // nell'ordine geometria,[key],
         // HatchName,[HatchLayer],[HatchColor],[HatchScale],[HatchRot]
         
         statement += geom_attrib;

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += key_attrib;
         }

         statement += _T(',');
         statement += hatch_attrib;

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_layer_attrib;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_color_attrib;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_scale_attrib;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_rotation_attrib;
         }

         statement += _T(") VALUES (");

         // nell'ordine geometria,[key],
         // HatchName,[HatchLayer],[HatchColor],[HatchScale],[HatchRot]

         statement += WKTToGeom_SQLSyntax;

         if (GeomKey != 0)
         {
            statement += _T(',');
            statement += GeomKey;
         }

         statement += _T(',');
         statement += HatchValue;

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchLayerValue;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchColorValue;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchScale;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += HatchRotValue;
         }

         statement += _T(")");

         if (GeomKey == 0) // se campo chiave della geometria è autoincrementato
         { // aggiungo all'istruzione SQL "RETURNING ..."
         }

         break;
      } // inserimento nuovo oggetto grafico - fine

      case MODIFY:  // modifica oggetto grafico esistente - inizio
      {
         // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
         statement = _T("UPDATE ");
         statement += TableRef;
         statement += _T(" SET ");

         // nell'ordine
         // geometria solo per spaghetti,
         // HatchName,[HatchLayer],[HatchColor],[HatchScale],[HatchRot]
         if (pCls->get_category() == CAT_SPAGHETTI)
         {
            statement += geom_attrib;
            statement += _T("=");
            statement += WKTToGeom_SQLSyntax;
            statement += _T(',');
         }

         statement += hatch_attrib;
         statement += _T("=");
         statement += HatchValue;

         if (hatch_layer_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_layer_attrib;
            statement += _T("=");
            statement += HatchLayerValue;
         }

         if (hatch_color_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_color_attrib;
            statement += _T("=");
            statement += HatchColorValue;
         }

         if (hatch_scale_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_scale_attrib;
            statement += _T("=");
            statement += HatchScale;
         }

         if (hatch_rotation_attrib.len() > 0) 
         {
            statement += _T(',');
            statement += hatch_rotation_attrib;
            statement += _T("=");
            statement += HatchRotValue;
         }

         statement += _T(" WHERE ");
         statement += key_attrib;
         statement += _T("=");
         statement += GeomKey;

         break;
      } // modifica oggetto grafico esistente - fine
  
      case ERASE: // cancellazione oggetto grafico esistente - inizio
      {
         if (pCls->get_category() == CAT_SPAGHETTI)
         {
            // DELETE FROM Tabella WHERE key_attrib=valore
            statement = _T("DELETE FROM ");
            statement += TableRef;
            statement += _T(" WHERE ");
            statement += key_attrib;
            statement += _T("=");
            statement += GeomKey;
         }
         else
         {
            // UPDATE Tabella SET colonna1=valore1, colonna2=valore2, ... WHERE key_attrib=valore
            statement = _T("UPDATE ");
            statement += TableRef;
            statement += _T(" SET ");

            // nell'ordine HatchName,[HatchLayer],[HatchColor],[HatchScale],[HatchRot]

            statement += hatch_attrib;
            statement += _T("=NULL");

            if (hatch_layer_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += hatch_layer_attrib;
               statement += _T("=NULL");
            }

            if (hatch_color_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += hatch_color_attrib;
               statement += _T("=NULL");
            }

            if (hatch_scale_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += hatch_scale_attrib;
               statement += _T("=NULL");
            }

            if (hatch_rotation_attrib.len() > 0) 
            {
               statement += _T(',');
               statement += hatch_rotation_attrib;
               statement += _T("=NULL");
            }

            statement += _T(" WHERE ");
            statement += key_attrib;
            statement += _T("=");
            statement += GeomKey;
         }

         break;
      } // cancellazione oggetto grafico esistente - fine
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLGetEntKeyFromGeomKey  <internal> */
/*+
  Crea una stringa di istruzione SQL per leggere il codice
  dell'entità partendo dal codice geometrico.
  Parametri:
  long      GeomKey;    codice dell'oggetto geometrico a cui va associato il riempimento
  C_STRING  &statement; Istruzione SQL (output)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLGetEntKeyFromGeomKey(long GeomKey, C_STRING &statement)
{
   statement = _T("SELECT ");
   statement += ent_key_attrib;
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += key_attrib;
   statement += _T("=");
   statement += GeomKey;      

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SQLFromEnt               <internal> */
/*+
  Crea una stringa di istruzione SQL per geometria da un oggetto
  grafico (escluso le superfici che sono composte da 1 o più polilinee).
  A seconda della modalità scelta sarà un'istruzione SQL di 
  inserimento, modifica o di cancellazione.
  Parametri:
  ads_name   ent;             oggetto grafico
  long       GeomKey;         codice dell'oggetto geometrico
                              (se si sta inserendo e questo codice = 0 significa
                              che il campo è un autoincremento e quindi non 
                              impostabile da una istruz. SQL)
  int        Mode;            tipo di istruzione SQL da generare:
                              INSERT = inserimento, MODIFY = aggiornamento,
                              ERASE = cancellazione
  C_STR_LIST &statement_list; Istruzione SQL (output);
                              A volte è una sola stringa e a volte più stringhe 
                              quindi per compatibilità si usa una lista di stringhe.

  Restituisce GS_GOOD in caso di successo, GS_CAN se l'oggetto grafico non è supportato
  altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SQLFromEnt(ads_name ent, long GeomKey, int Mode, C_STR_LIST &statement_list)
{
   AcDbObjectId objId;
   AcDbObject   *pObj;
   int          Result = GS_GOOD;
   long         ParentId;

   statement_list.remove_all();

   if (acdbGetObjectId(objId, ent) != Acad::eOk) return GS_BAD;
   if (acdbOpenObject(pObj, objId, AcDb::kForRead, true) != Acad::eOk) return GS_BAD;

   if (HasCompatibleGeom(pObj) == false) // Se oggetto grafico non supportato
      { pObj->close(); return GS_CAN; }

   if (gsc_is_DABlock(pObj) == GS_GOOD) // Blocco di attributi DA
   {
      C_STRING statement;
      C_EED    eed;

      eed.load(pObj); // leggo codice entità e fattore di aggregazione

      // le etichette sono sotto forma di blocchi con attributi in 2 tabelle distinte
      if (LblGroupingTableRef.len() > 0)
      {
         if (SQLFromBlock((AcDbBlockReference *) pObj, GeomKey, Mode, statement, true) == GS_BAD) return GS_BAD;
         statement_list.add_tail_str(statement.get_name());
         ParentId = GeomKey;
      }
      else
         ParentId = eed.gs_id;


      // Se non si sta inserendo (l'inserimento lo posso fare solo dopo aver inserito
      // il blocco, nel caso di raggruppamento, per avere il suo codice chiave)
      if (Mode != INSERT)
      {
         AcDbObjectId       AttribId;
         AcDbAttribute      *pAttrib;
         AcDbObjectIterator *pAttrIter;

         // Ciclo gli attributi
         pAttrIter = ((AcDbBlockReference *) pObj)->attributeIterator();
	      for (pAttrIter->start(); !pAttrIter->done(); pAttrIter->step())
         {
	         objId = pAttrIter->objectId();

            if (((AcDbBlockReference *) pObj)->openAttribute(pAttrib, objId, AcDb::kForRead) != Acad::eOk)
               { GS_ERR_COD = eGSInvEntityOp; return GS_BAD; }

            if (SQLFromAttrib(pAttrib, GeomKey, Mode, ParentId, eed.num_el, statement) == GS_BAD)
               { pAttrib->close(); return GS_BAD; }

            statement_list.add_tail_str(statement.get_name());

            pAttrib->close();
         }
      }
   }
   else
   if (gsc_isLinearEntity((AcDbEntity *) pObj) || // è un oggetto lineare o un cerchio
       pObj->isKindOf(AcDbCircle::desc()))
   {
      C_CLASS *pCls = gsc_find_class(prj, cls, sub);

      // se si tratta di spaghetti e l'oggetto è chiuso
      if (pCls->get_category() == CAT_SPAGHETTI && gsc_isClosedEntity((AcDbEntity *) pObj))
      {
         AcDbEntityPtrArray EntArray;
         C_STRING           statement;

         // se è possibile trasformarlo in "POLYGON" SQL
         EntArray.append((AcDbEntity *) pObj);
         if ((Result = SQLFromPolygon(EntArray, GeomKey, Mode, statement)) == GS_GOOD)
            statement_list.add_tail_str(statement.get_name());
         else // altrimenti diventa "LINESTRING" SQL
            Result = SQLFromPolyline((AcDbEntity *) pObj, GeomKey, Mode, statement_list);
      }
      else
         Result = SQLFromPolyline((AcDbEntity *) pObj, GeomKey, Mode, statement_list);
   }
   else
   if (pObj->isKindOf(AcDbBlockReference::desc())) // è un blocco
   {
      C_STRING statement;
      Result = SQLFromBlock((AcDbBlockReference *) pObj, GeomKey, Mode, statement, false);
      statement_list.add_tail_str(statement.get_name());
   }
   else
   if (gsc_isText((AcDbEntity *) pObj)) // è un testo
   {
      C_STRING statement;
      Result = SQLFromText((AcDbEntity *) pObj, GeomKey, Mode, statement);
      statement_list.add_tail_str(statement.get_name());
   }
   else
   if (pObj->isKindOf(AcDbPoint::desc())) // è un punto
   {
      C_STRING statement;
      Result = SQLFromPoint((AcDbPoint *) pObj, GeomKey, Mode, statement);
      statement_list.add_tail_str(statement.get_name());
   }
   else
   if (pObj->isKindOf(AcDbSolid::desc()) || 
       pObj->isKindOf(AcDbFace::desc()) ||
       pObj->isKindOf(AcDbEllipse::desc())) // è un solido o una 3dface o una ellisse
   {
      AcDbEntityPtrArray EntArray;
      C_STRING           statement;

      if (pObj->isKindOf(AcDbSolid::desc()))
         EntArray.append((AcDbSolid *) pObj);
      else if (pObj->isKindOf(AcDbFace::desc()))
         EntArray.append((AcDbFace *) pObj);
      else if (pObj->isKindOf(AcDbEllipse::desc()))
         EntArray.append((AcDbEllipse *) pObj);

      Result = SQLFromPolygon(EntArray, GeomKey, Mode, statement);
      statement_list.add_tail_str(statement.get_name());
   }
   else
   if (pObj->isKindOf(AcDbHatch::desc())) // è un riempimento
   {
      C_STRING statement;
      Result = SQLFromHatch((AcDbHatch *) pObj, GeomKey, Mode, statement);
      statement_list.add_tail_str(statement.get_name());
   }
   else
   if (pObj->isKindOf(AcDbMPolygon::desc())) // è un poligono
   {
      C_STRING statement;
      Result = SQLFromMPolygon((AcDbMPolygon *) pObj, GeomKey, Mode, statement);
      statement_list.add_tail_str(statement.get_name());
   }
   else
      Result = GS_CAN; // Oggetto grafico non supportato

   if (pObj->close() != Acad::eOk) return GS_BAD;
   
   return Result;
}
int C_DBGPH_INFO::SQLFromPolygon(C_SELSET &ss, long GeomKey, int Mode, C_STR_LIST &statement_list)
{
   AcDbEntityPtrArray EntArray;
   int                Result;
   C_STRING           statement;  

   if (ss.get_AcDbEntityPtrArray(EntArray) == GS_BAD) return GS_BAD;

   if ((Result = SQLFromPolygon(EntArray, GeomKey, Mode, statement)) == GS_GOOD)
      statement_list.add_tail_str(statement.get_name());

   if (gsc_close_AcDbEntities(EntArray) == GS_BAD) return GS_BAD;
   
   return Result;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::ToCounterClockwiseDegreeUnit <internal> */
/*+
  Trasforma una rotazione nell'unità espressa dal membro rotation_unit 
  in unità gradi in senso antiorario.
  Parametri:
  double rot;   Valore di rotazione

  Restituisce una rotazione in gradi in senso orario.
-*/  
/*********************************************************/
double C_DBGPH_INFO::ToCounterClockwiseDegreeUnit(double rot)
{
   switch (rotation_unit)
   {
      case GSClockwiseDegreeUnit: // cambio verso
         return 360 - rot;
      case GSNoneRotationUnit:
      case GSCounterClockwiseDegreeUnit: // senza conversione
         return rot;
      case GSClockwiseRadiantUnit: // cambio unità (rad->gradi) e verso
         return 360 - gsc_rad2grd(rot);
      case GSCounterClockwiseRadiantUnit: // cambio unità (rad->gradi)
         return gsc_rad2grd(rot);
      case GSClockwiseGonsUnit: // cambio unità e verso
         return 360 - gsc_rad2grd(gsc_gons2rad(rot));
      case GSCounterClockwiseGonsUnit: // cambio unità (gons->rad->gradi)
         return gsc_rad2grd(gsc_gons2rad(rot));
      case GSTopobaseGonsUnit: // cambio unità e verso
      {
         double n = 360 - gsc_rad2grd(gsc_gons2rad(rot)) + 90;
         return (n > 360) ? fmod(n, 360) : n;
      }
   }
   
   return rot;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::CounterClockwiseRadiantToRotationUnit <internal> */
/*+
  Trasforma una rotazione in radianti in senso antiorario nell'unità espressa 
  dal membro rotation_unit.
  Parametri:
  double rot;   Valore di rotazione in radianti senso antiorario

  Restituisce una rotazione nell'unità espressa dal membro rotation_unit.
-*/  
/*********************************************************/
double C_DBGPH_INFO::CounterClockwiseRadiantToRotationUnit(double rot)
{
   switch (rotation_unit)
   {
      case GSClockwiseDegreeUnit: // cambio verso e unità (rad->gradi)
         return gsc_rad2grd((2 * PI) - rot); 
      case GSCounterClockwiseDegreeUnit: // cambio unità (rad->gradi)
         return gsc_rad2grd(rot);
      case GSClockwiseRadiantUnit: // cambio il verso
         return (2 * PI) - rot;
      case GSNoneRotationUnit:
      case GSCounterClockwiseRadiantUnit:
         return rot; // senza conversione
      case GSClockwiseGonsUnit: // cambio verso e unità (rad->gons)
         return gsc_rad2gons((2 * PI) - rot); 
      case GSCounterClockwiseGonsUnit:
         return gsc_rad2gons(rot); // cambio unità (rad->gons)
      case GSTopobaseGonsUnit: // cambio unità e verso
      {
         double n = gsc_rad2gons((2 * PI) - rot) + 100;
         return (n > 400) ? fmod(n, 400) : n;
      }
   }

   return rot;
}


/*********************************************************/
/*.doc C_DBGPH_INFO::SqlCondToFieldValueList  <internal> /*
/*+
  Questa funzione restituisce la lista dei nomi dei campi con i valori di confronto
  letti da una stringa contenente una condizione SQL.
  La condizione prevede che l'unico operatore di confronto sia "=" 
  (es. "CAMPO1 = 2" va bene, "CAMPO1 > 2" NON va bene).
  L'unico operatore logico ammesso è "AND" (es. "CAMPO1 = 2 AND CAMPO2 = 7" va bene,
  "CAMPO1 = 2 OR CAMPO2 = 7" NON va bene).
  Parametri:
  C_STRING    &Cond;             Condizione SQL (es. "CAMPO1 = 2")
  C_2STR_LIST &FieldValueList;   Lista dei nomi dei campi e loro valori

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DBGPH_INFO::SqlCondToFieldValueList(C_STRING &Cond, C_2STR_LIST &FieldValueList)
{
   C_DBCONNECTION *pConn = getDBConnection();
   C_STRING        Buffer, FieldName, Value, TokenSeparators = _T(" =<>()+-*/");
   int             i = 0;
   TCHAR           ch;
   TCHAR           *LiteralPrefix, *LiteralSuffix;
   C_PROVIDER_TYPE *pProviderType;

   FieldValueList.remove_all();
   if (Cond.len() == 0) return GS_GOOD;

   // Cerco il tipo carattere nella connessione OLE-DB
   if (!(pProviderType = pConn->getCharProviderType())) return GS_BAD;

   LiteralPrefix = pProviderType->get_LiteralPrefix();
   LiteralSuffix = pProviderType->get_LiteralSuffix();

   // cerco i parametri (: seguito da una serie di caratteri alfanumerici o '_')
   while ((ch = Cond.get_chr(i)) != _T('\0'))
   {
      if (ch == _T(' ')) { i++; continue; } // salto gli spazi

      if (ch == LiteralPrefix[0]) // inizio stringa
      {
         Value = ch;
         // vado fino in fondo alla stringa
         while ((ch = Cond.get_chr(++i)) != _T('\0'))
         {
            Value += ch;

            if (ch == LiteralSuffix[0]) // fine stringa
            {
               // se il carattere che segue è un'altro fine stringa 
               // NON viene considerato come carattere di fine stringa vero
               if ((ch = Cond.get_chr(++i)) == _T('\0'))
                  break;
               if (ch != LiteralSuffix[0]) // fine stringa vero
                  break;
            }
         }
      }
      else 
      if (ch == pConn->get_InitQuotedIdentifier()) // inizio nome di campo
      {
         FieldName = ch;
         // vado fino in fondo al nome di campo
         while ((ch = Cond.get_chr(++i)) != _T('\0'))
         {
            FieldName += ch;

            if (ch == pConn->get_FinalQuotedIdentifier()) // fine nome di campo
            {
               // se il carattere che segue è un'altro fine stringa 
               // NON viene considerato come carattere di fine nome di campo vero
               if ((ch = Cond.get_chr(++i)) == _T('\0'))
                  { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }
               if (ch != pConn->get_FinalQuotedIdentifier()) // fine nome di campo vero
                  break;
            }
         }
      }
      else
      {
         Buffer = ch;

         if (TokenSeparators.at(ch) == NULL) // non era un separatore di token
            // vado fino in fondo al token
            while ((ch = Cond.get_chr(++i)) != _T('\0'))
               if (TokenSeparators.at(ch) != NULL) // fine token
                  break;
               else            
                  Buffer += ch;

         if (gsc_is_numeric(Buffer.get_name()) == GS_GOOD) // è un numero
            Value = Buffer;
         else // è un nome di campo
         {
            // se FieldName = "LIKE" "IN" "NOT" "OR" "WHERE"
            if (Buffer.comp(_T("LIKE"), FALSE) == 0 ||
                Buffer.comp(_T("IN"), FALSE) == 0 ||
                Buffer.comp(_T("NOT"), FALSE) == 0 ||
                Buffer.comp(_T("OR"), FALSE) == 0 ||
                Buffer.comp(_T("WHERE"), FALSE) == 0 ||
                Buffer.comp(_T("<")) == 0 ||
                Buffer.comp(_T(">")) == 0 ||
                Buffer.comp(_T(">=")) == 0 ||
                Buffer.comp(_T("<=")) == 0)
               { GS_ERR_COD = eGSInvalidSqlStatement; return GS_BAD; }

            if (Buffer.comp(_T("=")) != 0) // se non è l'unico operatore ammesso
               FieldName = Buffer;
         }
      }

      if (Value.len() > 0 && FieldName.len() > 0)
      {
         FieldValueList.add_tail_2str(FieldName.get_name(), Value.get_name());
         FieldName.clear();
         Value.clear();
      }

      i++;
   }

   return GS_GOOD;
}


//-----------------------------------------------------------------------//
////////////////////  C_DBGPH_INFO  FINE  /////////////////////////////////
////////////////////  C_DWG_INFO  INIZIO  /////////////////////////////////
//-----------------------------------------------------------------------//


// costruttore
C_DWG_INFO::C_DWG_INFO() : C_GPH_INFO()
{
   coordinate_system = DEFAULT_DWGCOORD;
}

// distruttore
C_DWG_INFO::~C_DWG_INFO() {}

GraphDataSourceEnum C_DWG_INFO::getDataSourceType(void) { return GSDwgGphDataSource; }

int C_DWG_INFO::copy(C_GPH_INFO *out)
{
   if (!out) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }
   
   // Copio i valori della classe base
   C_GPH_INFO::copy(out);

   ((C_DWG_INFO *) out)->dir_dwg = dir_dwg;
   dwg_list.copy(((C_DWG_INFO *) out)->dwg_list);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG_INFO::ToFile                     <internal> */
/*+
  Questa funzione salva su file i dati della C_DWG_INFO.
  Parametri:
  C_STRING &filename;   file in cui scrivere le informazioni
  const TCHAR *sez;     nome della sezione in cui scrivere

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::ToFile(C_STRING &filename, const TCHAR *sez)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   bool                    Unicode = false;

   if (gsc_path_exist(filename) == GS_GOOD)
      if (gsc_read_profile(filename, ProfileSections, &Unicode) == GS_BAD) return GS_BAD;

   if (ToFile(ProfileSections, sez) == GS_BAD) return GS_BAD;

   return gsc_write_profile(filename, ProfileSections, Unicode);
}
int C_DWG_INFO::ToFile(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_BPROFILE_SECTION *ProfileSection;
   C_STRING Buffer;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(sez)))
   {
      if (ProfileSections.add(sez) == GS_BAD) return GS_BAD;
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();
   }

   ProfileSection->set_entry(_T("GPH_INFO.SOURCE_TYPE"), (int) GSDwgGphDataSource);

   // Cancello l'eventuale informazione della connessione grafica a DB
   // perchè questa sezione contiene solo un tipo di connessione a fonti grafiche
   ProfileSection->set_entry(_T("GPH_INFO.TABLE_REF"), NULL);

   // Scrivo i valori della classe base
   if (C_GPH_INFO::ToFile(ProfileSections, sez) == GS_BAD) return GS_BAD;

   // traduco dir relativo in dir assoluto
   Buffer = dir_dwg;
   if (gsc_drive2nethost(Buffer) == GS_BAD) return GS_BAD;
   ProfileSection->set_entry(_T("GPH_INFO.DIR_DWG"), Buffer.get_name());

   return GS_GOOD;
}
//-----------------------------------------------------------------------//
int C_DWG_INFO::load(C_STRING &filename, const TCHAR *sez)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   
   if (gsc_read_profile(filename, ProfileSections) == GS_BAD) return GS_BAD;
   return load(ProfileSections, sez);
}
int C_DWG_INFO::load(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_B2STR *pProfileEntry;

   // Leggo i valori della classe base
   if (C_GPH_INFO::load(ProfileSections, sez) == GS_BAD) return GS_BAD;

   // Se non esiste questa informazione non si tratta di connessione grafica a DWG
   if (!(pProfileEntry = ProfileSections.get_entry(sez, _T("GPH_INFO.DIR_DWG"))))
      return GS_CAN;
   dir_dwg = pProfileEntry->get_name2();
   // converto path con uno senza alias host
   if (gsc_nethost2drive(dir_dwg) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG_INFO::to_rb                    <internal> */
/*+
  Questa funzione restituisce la C_DWG_INFO sottoforma di lista
  di coppie di resbuf ((<proprietà> <valore>) ...).
  Parametri:
  bool ConvertDrive2nethost; Se = TRUE le path vengono convertite
                             con alias di rete (default = FALSE)
  bool ToDB;                 Se = true vengono considerate anche le
                             informazioni di identificazione della
                             classe (default = false)

  Restituisce la lista in caso di successo altrimenti restituisce NULL.
-*/  
/*********************************************************/
presbuf C_DWG_INFO::to_rb(bool ConvertDrive2nethost, bool ToDB)
{
   C_RB_LIST List;
   C_STRING  Buffer;

   // Scrivo i valori della classe base
   if ((List << C_GPH_INFO::to_rb(ConvertDrive2nethost, ToDB)) == NULL) return NULL;

   if (!ToDB)
      if ((List += acutBuildList(RTLB,
                                    RTSTR, _T("SOURCE_TYPE"),
                                    RTSHORT, (int) GSDwgGphDataSource,
                                 RTLE,
                                 0)) == NULL) return NULL;

   if ((List += acutBuildList(RTLB,
                                 RTSTR, _T("DIR_DWG"),
                              0)) == NULL) return NULL;

   Buffer = dir_dwg;
   if (ConvertDrive2nethost)
      if (gsc_drive2nethost(Buffer) == GS_BAD) return NULL;
   if ((List += gsc_str2rb(Buffer)) == NULL) return NULL;

   if ((List += acutBuildList(RTLE, 0)) == NULL) return NULL;  

   List.ReleaseAllAtDistruction(GS_BAD);

   return List.get_head();
}


/*********************************************************/
/*.doc C_DWG_INFO::from_rb                    <internal> */
/*+
  Questa funzione legge le proprietà della C_DWG_INFO sottoforma di lista
  di coppie di resbuf 
  (("PRJ"<val>)("CLS"<val>)("SUB"<val>)("COORDINATE"<val>)("DIR_DWG"<val>)).
  Parametri:
  C_RB_LIST &ColValues; Lista di valori
  oppure
  resbuf *rb;           Lista di valori

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::from_rb(C_RB_LIST &ColValues)
{
   return from_rb(ColValues.get_head());
}
int C_DWG_INFO::from_rb(resbuf *rb)
{
   presbuf p;

   // Leggo i valori della classe base
   if (C_GPH_INFO::from_rb(rb) == GS_BAD) return GS_BAD;

   if ((p = gsc_CdrAssoc(_T("DIR_DWG"), rb, FALSE)))
      if (p->restype == RTSTR)
      {
         dir_dwg = p->resval.rstring;
         dir_dwg.alltrim();
         // traduco dir assoluto in dir relativo
         if (gsc_nethost2drive(dir_dwg) == GS_BAD) return GS_BAD;
      }
      else
         dir_dwg.clear();

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG_INFO::isValid                    <external> */
/*+
  Questa funzione verifica la correttezza della C_DWG_INFO.
  Parametri:
  int GeomType;      Usato per compatibilità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::isValid(int GeomType)
{
   C_GPH_INFO::isValid(GeomType);

   // verifico validità dir_dwg
   if (gsc_validdirdwg(dir_dwg) == NULL) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG_INFO::CreateResource             <external> */
/*+
  Questa funzione crea le risorse necessarie per memorizzare
  le informazioni geometriche.
  Parametri:
  int GeomType;      Usato per compatibilità
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::CreateResource(int GeomType)
{
   // creo directory DWG
   if (gsc_mkdir(dir_dwg) == GS_BAD) return GS_BAD;

   // creo primo file DWG vuoto
   return gsc_CreaFirstDWG(prj, cls, dir_dwg.get_name(),
                           coordinate_system.get_name());
}


/*********************************************************/
/*.doc C_DWG_INFO::RemoveResource             <external> */
/*+
  Questa funzione cancella le risorse necessarie per memorizzare
  le informazioni geometriche.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::RemoveResource(void)
{
   // cancello i files DWG e (se possibile) il direttorio che li contiene
   if (gsc_del_dwg(prj, cls, dir_dwg.get_name()) == GS_BAD) return GS_BAD;
   // cancello (se è possibile) directory DWG
   // questo perchè gsc_del_dwg funziona solo se la classe è già stata inserita
   // nella lista delle classi nel progetto e questo non è vero se la funzione
   // viene chiamata in fase di creazione della classe
   gsc_rmdir(dir_dwg, ONETEST);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG_INFO::IsTheSameResource             <external> */
/*+
  Questa funzione verifica che le risorse usate per memorizzare
  i dati geometrici siano le stesse di quelle indicate dal parametro.
  Parametri:
  C_GPH_INFO *p;  puntatore a risorse grafiche da confrontare

  Restituisce vewro in caso di successo altrimenti restituisce falso.
-*/  
/*********************************************************/
bool C_DWG_INFO::IsTheSameResource(C_GPH_INFO *p)
{
   if (!p) return false;

   // Se il tipo di risorsa è diverso
   if (getDataSourceType() != p->getDataSourceType()) return false;
   // Se il riferimento a progetto e a classe è diverso
   // (il nome del disegno è codificato con il codice del progetto e della classe)
   if (prj != p->prj || cls != p->cls) return false;
   
   return true;
}

/*********************************************************/
/*.doc C_DWG_INFO::ResourceExist            <external> */
/*+
  Questa funzione restituisce true se la risorsa grafica è già esistente 
  (anche parzialmente) altrimenti restituisce false.
  Parametri:
  bool *pGeomExist;        usato per compatibilità con C_DBGPH_INFO
  bool *pLblGroupingExist; usato per compatibilità con C_DBGPH_INFO
  bool *pLblExist;         usato per compatibilità con C_DBGPH_INFO
-*/  
/*********************************************************/
bool C_DWG_INFO::ResourceExist(bool *pGeomExist, bool *pLblGroupingExist, bool *pLblExist)
{
   TCHAR      *cod36prj = NULL, *cod36cls = NULL;
   C_STRING   Mask;
   C_STR_LIST FileNameList;

   if (gsc_long2base36(&cod36prj, prj, 2) == GS_BAD || !cod36prj) return false;
   if (gsc_long2base36(&cod36cls, cls, 3) == GS_BAD || !cod36cls)
      { free(cod36prj); return false; }

   Mask = dir_dwg;
   Mask += _T('\\');
   Mask += cod36prj;
   Mask += cod36cls;
   Mask += _T("*.DWG");

   free(cod36prj);
   free(cod36cls);

   gsc_adir(Mask.get_name(), &FileNameList);

   if (FileNameList.get_count() == 0)
   {
      if (pGeomExist) *pGeomExist = false;
      if (pLblGroupingExist) *pLblGroupingExist = false;
      if (pLblExist) *pLblExist = false;
      return false;
   }
   else
   {
      if (pGeomExist) *pGeomExist = true;
      if (pLblGroupingExist) *pLblGroupingExist = true;
      if (pLblExist) *pLblExist = true;
      return true;
   }
}


/*********************************************************/
/*.doc C_DWG_INFO::LoadSpatialQueryFromADEQry <internal> */
/*+
  Questa funzione carica la condizione di query spaziale dalla query ADE
  precedentemente salvata con nome "spaz_estr". La funzione inoltre
  "seleziona" solo i disegni che intersecano la condizione spaziale. 
  Parametri:
  TCHAR *qryName; Opzionale, nome della query ADE. 
                  Se = NULL viene usata la query ADE di default = "spaz_estr"
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::LoadSpatialQueryFromADEQry(TCHAR *qryName)
{
   double Xmin, Ymin, Xmax, Ymax;

   // carico e attivo la query spaziale
   if (gsc_ade_qlloadqry(qryName) != RTNORM) return GS_BAD;

   // inizializzo la lista dei dwg della classe
   if (dwg_list.load(prj, cls) == GS_BAD) return GS_BAD;
   // se è possibile ottenere una finestra delle estensioni spaziali
   if (gsc_getCurrQryExtents(&Xmin, &Ymin, &Xmax, &Ymax) == GS_GOOD)
      // Filtro i DWG della classe per intersezione con la zona da estrarre
      if (dwg_list.intersect(Xmin, Ymin, Xmax, Ymax) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_DWG_INFO::Query                      <external> */
/*+
  Questa funzione legge dai disegni Autocad gli oggetti che soddisfano la
  query ADE corrente e poi li visualizza e/o ne fa il preview e/o ne fa un report.
  La funzione si è resa necessaria perchè, per un baco di map, se si esegue 
  la lettura degli oggetti da ciascun disegno della classe e poi si visualizzano
  (o preview o report) gli oggetti letti da ciascun disegno si ottiene un errore.
  Con questa funzione, per ciascun disegno vengono e letti gli oggetti e subito 
  dopo visualizzati (o preview o report).
  Parametri:
  int WhatToDo;      Flag a bit che determina cosa deve essere fatto degli oggetti 
                     letti dalla fonte grafica (PREVIEW, EXTRACTION, REPORT)
  C_SELSET *pSelSet; Opzionale; Usato nel modo EXTRACION è il gruppo di selezione 
                     degli oggetti disegnati (default = NULL)
  long BitForFAS;    Opzionale; Usato nel modo EXTRACION e PREVIEW è un flag 
                     a bit indicante quali caratteristiche grafiche devono essere 
                     impostate per gli oggetti (default = GSNoneSetting)
  C_FAS *pFAS;       Opzionale; Usato nel modo EXTRACION e PREVIEW è un
                     set di alterazione caratteristiche grafiche (default = NULL)
  C_STRING *pTemplate;     Opzionale; Usato nel modo REPORT è un modello 
                           contenente quali informazioni stampare nel file report
  C_STRING *pRptFile;    Opzionale; Usato nel modo REPORT è il percorso 
                         completo del file di report
  const TCHAR *pRptMode; Opzionale; Usato nel modo REPORT è un modo di apertura
                         del file di report:
                         "w" apre un file vuoto in scrittura, se il file 
                         esisteva il suo contenuto è distrutto,
                         "a" apre il file in scrittura aggiungendo alla fine 
                         del file senza rimuovere il contenuto del file; 
                         crea prima il file se non esiste.
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce il numero di oggetti trovati in caso di successo altrimenti 
  restituisce -1 in caso di errore e -2 in caso di impossibilità.
-*/  
/*********************************************************/
long C_DWG_INFO::Query(int WhatToDo,
                       C_SELSET *pSelSet, long BitForFAS, C_FAS *pFAS,
                       C_STRING *pRptTemplate, C_STRING *pRptFile, const TCHAR *pRptMode,
                       int CounterToVideo)
{
   C_DWG *pDwg = (C_DWG *) dwg_list.get_head();
   long  Tot = 0, i = 0;
   int   Result;
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(481)); // "Visualizzazione oggetti"

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.Init(dwg_list.get_count());

   while (pDwg)
   {
      if (CounterToVideo == GS_GOOD)
         StatusBarProgressMeter.Set(++i);

      // Cerco gli oggetti nel disegno
      pDwg->clear_ObjectIdArray();
      if ((Result = pDwg->ApplyQuery()) != GS_GOOD)
         return (Result == GS_BAD) ? -1 : -2;

      // se si devono creare degli oggetti autocad nella sessione corrente
      if (WhatToDo & EXTRACTION)
         if ((Result = pDwg->QueryIn(pSelSet, BitForFAS, pFAS)) != GS_GOOD)
            { pDwg->clear_ObjectIdArray(); return (Result == GS_BAD) ? -1 : -2; }

      // se si deve presentare un'anteprima degli oggetti a video
      if (WhatToDo & PREVIEW)
         if ((Result = pDwg->Preview(BitForFAS, pFAS)) != GS_GOOD)
            { pDwg->clear_ObjectIdArray(); return (Result == GS_BAD) ? -1 : -2; }

      // se si deve scrivere un file di report degli oggetti
      if (WhatToDo & REPORT)
         if (pRptTemplate && pRptFile)
         {
            if (ade_qrysettype(_T("report"), FALSE, pRptTemplate->get_name(),
                               pRptFile->get_name()) != RTNORM)
               { pDwg->clear_ObjectIdArray(); return -1; }
            if ((Result = pDwg->Report(*pRptFile, _T("a"))) != GS_GOOD)
               { pDwg->clear_ObjectIdArray(); return (Result == GS_BAD) ? -1 : -2; }
         }

      Tot += pDwg->ptr_ObjectIdArray()->length();
      pDwg->clear_ObjectIdArray();

      pDwg = (C_DWG *) pDwg->get_next();
   }

   if (CounterToVideo == GS_GOOD)
      StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."

   return Tot;
}


/*********************************************************/
/*.doc C_DWG_INFO::ApplyQuery                 <internal> */
/*+
  Questa funzione legge da disegni DWG gli oggetti che soddisfano la
  query ADE corrente.
  Parametri:
  C_GPH_OBJ_ARRAY &Objs; Oggetti risultanti dalla ricerca
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::ApplyQuery(C_GPH_OBJ_ARRAY &Objs, int CounterToVideo)
{
   C_DWG *pDwg = (C_DWG *) dwg_list.get_head(); 

   Objs.ObjType = AcDbObjectIdType;

   // Svuoto gli array degli oggetti evntualmente estratti precedentemente
   while (pDwg)
   {
      pDwg->clear_ObjectIdArray();
      
      pDwg = (C_DWG *) pDwg->get_next();
   }
   // Copio la lista dei disegni
   dwg_list.copy(Objs.DwgList);

   return Objs.DwgList.ApplyQuery(MORETESTS, CounterToVideo);
}


/*********************************************************/
/*.doc C_DWG_INFO::QueryIn                    <internal> */
/*+
  Questa funzione inserisce gli oggetti grafici letti dalla query
  nel disegno corrente senza duplicare oggetti già estratti.
  Parametri:
  C_GPH_OBJ_ARRAY &Objs;  Oggetti risultanti dalla ricerca
  C_SELSET      *pSelSet; Opzionale; Gruppo di selezione degli oggetti disegnati 
                          (default = NULL)
  long         BitForFAS; Opzionale; Flag a bit indicante quali caratteristiche grafiche
                          devono essere impostate per gli oggetti 
                          (default = GSNoneSetting)
  C_FAS         *pFAS;    Opzionale; Caratteristiche grafiche (default = NULL)
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::QueryIn(C_GPH_OBJ_ARRAY &Objs, C_SELSET *pSelSet, 
                        long BitForFAS, C_FAS *pFAS, int CounterToVideo)
{   
   return Objs.DwgList.QueryIn(pSelSet, BitForFAS, pFAS, CounterToVideo);
}


/*********************************************************/
/*.doc C_DWG_INFO::Report                     <internal> */
/*+
  Questa funzione scrive informazioni degli oggetti grafici letti 
  dalla query su file testo.
  Parametri:
  C_GPH_OBJ_ARRAY &Objs; Oggetti risultanti dalla ricerca
  C_STRING &Template;    Quali informazioni stampare
  C_STRING &Path;        Percorso file di scrittura
  const TCHAR *Mode;     "w"  Opens an empty file for writing.
                         If the given file exists, its contents are destroyed.
                         "a"  Opens for writing at the end of the file (appending) without removing 
                         the EOF marker before writing new data to the file; 
                         creates the file first if it doesn’t exist.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::Report(C_GPH_OBJ_ARRAY &Objs, C_STRING &Template,
                       C_STRING &Path, const TCHAR *Mode)
{
   if (ade_qrysettype(_T("report"), FALSE, Template.get_name(),
                      Path.get_name()) != RTNORM)
      { gsc_printAdeErr(); return GS_BAD; }
   return Objs.DwgList.Report(Path, Mode);
}


/*********************************************************/
/*.doc C_DWG_INFO::Preview                    <internal> */
/*+
  Questa funzione crea un blocco di anteprima degli 
  oggetti nel disegno corrente.
  Parametri:
  C_GPH_OBJ_ARRAY &Objs;  Oggetti risultanti dalla ricerca
  long         BitForFAS; Opzionale; Flag a bit indicante quali caratteristiche grafiche
                          devono essere impostate per gli oggetti 
                          (default = GSNoneSetting)
  C_FAS         *pFAS;    Opzionale; Caratteristiche grafiche (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_DWG_INFO::Preview(C_GPH_OBJ_ARRAY &Objs, long BitForFAS, C_FAS *pFAS)
{
   return Objs.DwgList.Preview(BitForFAS, pFAS);
}


/*********************************************************/
/*.doc C_DWG_INFO::Save                       <internal> */
/*+                                                                       
  Salvataggio dati grafici nei files DWG di autocad.
  Le entità salvate vengono rimosse da GEOsimAppl::SAVE_SS.
  Parametri:
  					
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_DWG_INFO::Save(void)
{
   C_SELSET   ClsSS, NewSS;
   ads_name   dummySS;
   presbuf    OldValues4Speed;
   int        result = GS_BAD, OldCmdDia;
   long       i = 0;
   C_DWG      *pDwg;
   C_CLASS    *pCls = gsc_find_class(prj, cls, sub);

   if (!pCls) return GS_BAD;
   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // BLIPMODE, HIGHLIGHT, CMDECHO non attivi
   if ((OldValues4Speed = gsc_setVarForCommandSpeed()) == NULL) return GS_BAD;
   
   gsc_set_cmddia(0, &OldCmdDia);

   do
   {
      result = GS_GOOD;
      // Creo il selection set filtrando da GEOsimAppl::SAVE_SS tutti gli oggetti della classe
      // considerando anche quelli cancellati
      if (GEOsimAppl::SAVE_SS.copyIntersectClsCode(ClsSS, cls, sub, true) == GS_BAD)
         { result = GS_BAD; break; }
      result = GS_BAD;

      // se la classe ha collegamento a DB e
      // sono stati aggiunti i link ASE con LPT in fase di estrazione
      if (pCls->ptr_info() && GEOsimAppl::GLOBALVARS.get_AddLPTOnExtract() == GS_GOOD)
      {
         C_STRING LPTName;

         // rimuovo i link ASE
         if (pCls->GetLPN4OLD(LPTName) == GS_BAD) break;
         if (gsc_EraseASELink(LPTName, ClsSS, GS_GOOD) == GS_BAD) break;
      }

      // inizializzo la lista dei dwg della classe corrente caricando comunque 
      // i file di estensione (indipendentemente dalle date dei file)
      if (dwg_list.load(prj, cls, false) == GS_BAD) break;

      // Se nessun DWG della classe è attaccato e attivato
      pDwg = (C_DWG *) dwg_list.get_head();
      while (pDwg)
      {
         if (pDwg->is_activated() == GS_GOOD) break; // attivato
         pDwg = (C_DWG *) dwg_list.get_next();
      }
      // attacco e attivo il più piccolo DWG della classe 
      // (altrimenti le funzioni ADE non vanno)
      if (!pDwg)
      {
         if ((pDwg = (C_DWG *) dwg_list.search_min()) == NULL) break;
         if (pDwg->activate() == GS_BAD) break;
      }

      // preparo il gruppo di selezione ADE per il salvataggio.
      if (gsc_PrepareObjects4AdeSave(cls, sub, ClsSS) == GS_BAD) break;

      // Se si tratta di spaghetti
      if (pCls->get_category() == CAT_SPAGHETTI)
      {
         // ci sono nuovi oggetti
         if (ade_editnew(dummySS) == RTNORM && ads_sslength(dummySS, &i) == RTNORM && i > 0)
         {
            C_STRING  ODTableName;
            C_SELSET  SScopy;
            ads_name  ent;

            // faccio una copia perchè gsc_SaveNewObjs svuota il gruppo di selezione
            SScopy.add_selset(dummySS);

            // Salvo il gruppo di selezione dei nuovi oggetti
            if (gsc_SaveNewObjs(cls, SScopy, dwg_list) == GS_BAD) break;

            // elimino gli oggetti salvati da GEOsimAppl::SAVE_SS
            GEOsimAppl::SAVE_SS.subtract(dummySS);

            // aggiungo ad ognuno un identificatore
            // (<handle>-<nome dwg>) che verrà successivamente salvato

            // setto la tabella interna
            gsc_getODTableName(prj, cls, sub, ODTableName);

            i = 0;
            result = GS_GOOD;
            while (acedSSName(dummySS, i++, ent) == RTNORM)
               // memorizzo identificatore OD nell'entità
               if (gsc_setID2ODTable(ent, ODTableName) == GS_BAD) { result = GS_BAD; break; }

            if (result == GS_BAD) break;
            result = GS_BAD;
            // inserisco gli oggetti nel gruppo di selezione ADE
            SScopy.add_selset(dummySS);
            if (gsc_adeSelObjs(SScopy) == GS_BAD) break;

            // salvo le entità modificate (esistenti già dall'inizio) e
            // risalvo gli oggetti grafici a cui era stato aggiunto un identificatore
            if (dwg_list.SaveObjs() == GS_BAD) break;

            acutPrintf(gsc_msg(622)); // "\nSalvataggio nuovi oggetti terminato."
         }
         else
         {
            acutPrintf(gsc_msg(621));  // "\nNessun nuovo oggetto."
            // salvo le entità modificate (esistenti già dall'inizio)
            if (dwg_list.SaveObjs() == GS_BAD) break;
         }
      }
      else // se NON non si tratta di spaghetti
      {
         // salvo le entità modificate (esistenti già dall'inizio)
         if (dwg_list.SaveObjs() == GS_BAD) break;

         // ci sono nuovi oggetti
         if (ade_editnew(dummySS) == RTNORM && ads_sslength(dummySS, &i) == RTNORM && i > 0)
         {
            NewSS << dummySS;
            // Filtro da NewSS tutti gli oggetti della classe
            if (NewSS.intersectClsCode(cls, sub) == GS_BAD) break;
            // Salvo il gruppo di selezione dei nuovi oggetti
            if (gsc_SaveNewObjs(cls, NewSS, dwg_list) == GS_BAD) break;
         }
         else
            acutPrintf(gsc_msg(621));  // "\nNessun nuovo oggetto."
      }

      // elimino gli oggetti salvati da GEOsimAppl::SAVE_SS
      GEOsimAppl::SAVE_SS.subtract(ClsSS);

      // Aggiorno le nuove estensioni dei disegni della classe
      // modificando i file INF già esistenti senza crearli se questi non esistono
      dwg_list.update_extents(ClsSS);

      // se la classe ha collegamento a DB e
      // sono stati aggiunti i link ASE con LPT in fase di estrazione
      // riaggiungo i link ASE che avevo rimosso
      if (pCls->ptr_info() && GEOsimAppl::GLOBALVARS.get_AddLPTOnExtract() == GS_GOOD)
      {
         gsc_SetASEKey(ClsSS, GS_GOOD);
         gsc_SetASEKey(NewSS, GS_GOOD);
      }

      result = GS_GOOD;
   }
   while (0);

   if (gsc_setVarForCommandNormal(OldValues4Speed) == GS_BAD) result = GS_BAD;
   gsc_set_cmddia(OldCmdDia); // Reset CMDDIA

   return result;
}


/*********************************************************/
/*.doc long C_DWG_INFO::editnew               <external> /*
/*+
  Ottiene un gruppo di selezione degli oggetti nuovi leggendolo
  dal gruppo di selezione degli oggetti da salvare.
  Parametri:
  ads_name sel_set;  output. Gruppo di oggetti nuovi

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_DWG_INFO::editnew(ads_name sel_set)
{
   return (ade_editnew(sel_set) == RTNORM) ? GS_GOOD : GS_BAD;
}


/*********************************************************/
/*.doc bool C_DWG_INFO::HasCompatibleGeom     <external> /*
/*+
  Questa funzione verifica che la geometria di un oggetto grafico
  sia compatibile al tipo di risorsa grafica usata.
  Parametri:
  AcDbObject *pObj;        oggetto grafico
  bool TryToExplode;       Opzionale; flag di esplosione. Nel caso l'oggetto
                           grafico non sia compatibile vine esploso per ridurlo
                           ad oggetti semplici (default = false)
  AcDbVoidPtrArray *pExplodedSet;   Opzionale; Puntatore a risultato 
                                    dell'esplosione (default = NULL)

  Restituisce true se l'oggetto ha una geometria compatibile altrimenti false.
-*/  
/*********************************************************/
bool C_DWG_INFO::HasCompatibleGeom(AcDbObject *pObj, bool TryToExplode, AcDbVoidPtrArray *pExplodedSet)
{ 
   if (pExplodedSet) pExplodedSet->removeAll();
   return true;
}


/*********************************************************/
/*.doc bool C_DWG_INFO::HasValidGeom          <external> /*
/*+
  Questa funzione verifica che la validità della geometria di un oggetto grafico.
  Parametri:
  AcDbObject *pObj;  oggetto grafico

  Restituisce true se l'oggetto ha una geometria troppo complessa altrimenti false.
-*/  
/*********************************************************/
bool C_DWG_INFO::HasValidGeom(AcDbEntity *pObj, C_STRING &WhyNotValid)
   { return true; }
bool C_DWG_INFO::HasValidGeom(AcDbEntityPtrArray &EntArray, C_STRING &WhyNotValid)
   { return true; }


/******************************************************************************/
/*.doc C_DWG_INFO::Check                                                      */
/*+
  Questa funzione esegue il controllo dei dwg
  Parametri:
  int WhatOperation;    Tipo di controllo da eseguire:
                        reindicizzazione (REINDEX)
                        sblocco degli oggetti bloccati (UNLOCKOBJS)
                        creazione file INF di estensione dei disegni (SAVEEXTENTS)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DWG_INFO::Check(int WhatOperation)
{
   C_CLASS    *pCls = gsc_find_class(prj, cls, sub);
   C_DWG_LIST DwgList;
   int        unlocked;
   long       SessionCode;
   TCHAR      Msg[MAX_LEN_MSG];

   if (!pCls) return GS_BAD;

   if (pCls->get_category() != CAT_EXTERN)
      // se non ha rappresentazioni grafiche: grazie è stato bello...
      if (pCls->ptr_fas() == NULL) return GS_GOOD;

   // se la classe è in una sessione di lavoro non si può modificare il\i DWG
   if (pCls->is_inarea(&SessionCode) == GS_BAD) return GS_BAD;
   if (SessionCode != 0)
   {
      acutPrintf(gsc_msg(612), pCls->ptr_id()->name); // "\nControllo dati classe %s fallita."
      acutPrintf(_T(" %s"), gsc_msg(607)); // "Trovate sessioni di lavoro..."
      return GS_GOOD;
   }

   // inizializzo la lista dei dwg della classe corrente caricando comunque 
   // i file di estensione (indipendentemente dalle date dei file)
   if (DwgList.load(prj, cls, (bool) FALSE) == GS_BAD) return GS_BAD;

   // Sblocca tutti i DWG della classe che non sono
   // usati da nessuna sessione di lavoro (cancella i *.DWK, *.DWL?, *.MAP).
   if (pCls->DWGsUnlock(&unlocked) == GS_GOOD)
      if (unlocked)
      {
         acutPrintf(_T("\n%s%s"), gsc_msg(230), pCls->ptr_id()->name);  // "Classe: "
         acutPrintf(_T("; %s"), gsc_msg(661)); // "Disegni sbloccati."
      }

   if (WhatOperation & REINDEX || WhatOperation & UNLOCKOBJS)
   {
      if (WhatOperation & UNLOCKOBJS)
      {
         // Sblocca tutti gli oggetti rimasti bloccati per errore nei DWG attaccati.
         if (DwgList.unlockObjs() == GS_BAD)
         {
            swprintf(Msg, MAX_LEN_MSG, _T("Unlock Objs DWGs class %d failed."), cls);
            gsc_write_log(Msg);
            return GS_BAD;
         }
         swprintf(Msg, MAX_LEN_MSG, _T("Unlock Objs DWGs class %d terminated."), cls);
         gsc_write_log(Msg);
      }

      if (WhatOperation & REINDEX)
      {
         if (DwgList.index(prj, cls) == GS_BAD)
         {
            swprintf(Msg, MAX_LEN_MSG, _T("Reindex DWGs class %d failed."), cls);
            gsc_write_log(Msg);
            return GS_BAD;
         }
         swprintf(Msg, MAX_LEN_MSG, _T("Reindex DWGs class %d terminated."), cls);
         gsc_write_log(Msg);
      }

      // stacco tutti i DWG della classe
      if (DwgList.detach() == GS_BAD) return GS_BAD;
   }

   // gli INF devono essere sempre fatti altrimenti diventano più vecchi dei DWG
   if (WhatOperation & REINDEX || WhatOperation & UNLOCKOBJS || WhatOperation & SAVEEXTENTS)
   {         
      if (WhatOperation & SAVEEXTENTS)
      {
         if (DwgList.regen_extents_from_dwg(GS_BAD) == GS_BAD) // ricostruisce tutti i file di estensione
         {
            swprintf(Msg, MAX_LEN_MSG, _T("Extents files creation class %d failed."), cls);
            gsc_write_log(Msg);
            return GS_BAD;
         }
         swprintf(Msg, MAX_LEN_MSG, _T("Extents files creation class %d terminated."), cls);
         gsc_write_log(Msg);
      }
      else
      {
         C_SELSET dummy;

         // aggiorna data e ora dei file INF già esistenti senza crearli 
         // se questi non esistono.
         if (DwgList.update_extents(dummy) == GS_BAD)
         {
            swprintf(Msg, MAX_LEN_MSG, _T("Extents files update class %d failed."), cls);
            gsc_write_log(Msg);
            return GS_BAD;
         }
         swprintf(Msg, MAX_LEN_MSG, _T("Extents files update class %d terminated."), cls);
         gsc_write_log(Msg);
      }
   }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_DWG_INFO::Detach                                         <external>  */
/*+
  Funzione per il rilascio della sorgente dati geometrica.
  Parametri:

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DWG_INFO::Detach(void)
{ 
   return dwg_list.detach();
}


/****************************************************************************/
/*.doc C_DWG_INFO::reportHTML                                    <external> */
/*+
  Questa funzione stampa su un file html i dati della C_DWG_INFO.
  Parametri:
  FILE *file;        Puntatore a file
  bool SynthMode;    Opzionale. Flag di modalità di report.
                     Se = true attiva il report sintetico (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/
/****************************************************************************/
int C_DWG_INFO::reportHTML(FILE *file, bool SynthMode) 
{
   if (SynthMode) return GS_GOOD;

   if (C_GPH_INFO::reportHTML(file, SynthMode) == GS_BAD) return GS_BAD;

   C_STRING TitleBorderColor("#808080"), TitleBgColor("#c0c0c0");
   C_STRING BorderColor("#00CCCC"), BgColor("#99FFFF");

   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" bgcolor=\"%s\" width=\"100%%\" border=\"1\">"),
                TitleBorderColor.get_name(), TitleBgColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Collegamento a Grafica"
   if (fwprintf(file, _T("\n<tr><td align=\"center\"><b><font size=\"3\">%s</font></b></td></tr></table><br>"),
                gsc_msg(766)) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // intestazione tabella
   if (fwprintf(file, _T("\n<table bordercolor=\"%s\" cellspacing=\"2\" cellpadding=\"2\" border=\"1\">"), 
                BorderColor.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // "Cartella disegni"
   if (fwprintf(file, _T("\n<tr><td align=\"right\" bgcolor=\"%s\"><b>%s:</b></td><td>%s</td></tr>"),
                BgColor.get_name(), gsc_msg(720),
                (dir_dwg.len() == 0) ? _T("&nbsp;") : dir_dwg.get_name()) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   // fine tabella
   if (fwprintf(file, _T("\n</table><br><br>")) < 0)
      { GS_ERR_COD = eGSWriteFile; return GS_BAD; }

   return GS_GOOD;
}


/******************************************************************************/
/*.doc C_DWG_INFO::Backup                                         <internal>  */
/*+
  Questa funzione esegue il backup dei dati geometrici.
  Parametri:
  BackUpModeEnum Mode;  Flag di modalità: Creazione , restore o cancellazione
                        del backup esistente (default = GSCreateBackUp).
  int MsgToVideo; flag, se = GS_GOOD stampa a video i messaggi (default = GS_GOOD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/******************************************************************************/
int C_DWG_INFO::Backup(BackUpModeEnum Mode, int MsgToVideo)
{
   int        result = GS_GOOD;
   TCHAR      *cod36prj = NULL, *cod36cls = NULL;
   C_STRING   drive, dir, name, CopiedFile;
   C_DWG      *pDwg;
   C_STR_LIST FileNameList;
   C_STR      *pFileName;

   if (gsc_long2base36(&cod36prj, prj, 2) == GS_BAD || !cod36prj) return GS_BAD;
   if (gsc_long2base36(&cod36cls, cls, 3) == GS_BAD || !cod36cls)
      { free(cod36prj); return GS_BAD; }

   if (MsgToVideo == GS_GOOD) gsc_PrintBackUpMsg(Mode);

   CopiedFile = dir_dwg;
   CopiedFile += _T('\\');
   CopiedFile += cod36prj;
   CopiedFile += cod36cls;
   CopiedFile += _T("*.BAK");

   free(cod36prj);
   free(cod36cls);

   gsc_adir(CopiedFile.get_name(), &FileNameList);

   switch (Mode)
   {
      case GSCreateBackUp: // crea backup (creo files .BAK proprio come fa MAP)
      case GSRemoveBackUp: // cancella backup
      {
         // Cancello i files *.BAK
         pFileName = (C_STR *) FileNameList.get_head();
         
         while (pFileName)
         {
            CopiedFile = dir_dwg;
            CopiedFile += _T('\\');
            CopiedFile += pFileName->get_name();
            if (gsc_delfile(CopiedFile) == GS_BAD)
               { result = GS_BAD; break; }

            pFileName = (C_STR *) FileNameList.get_next();
         }

         if (result == GS_BAD) break;

         // provo la cancellazione del direttorio di backup
         gsc_rmdir(dir_dwg, ONETEST);

         // crea backup (creo files .BAK proprio come fa MAP)
         if (Mode == GSCreateBackUp)
         {
            // inizializzo la lista dei dwg della classe corrente
            // nel caso fosse cambiata
            if (dwg_list.load(prj, cls) == GS_BAD) return GS_BAD;
            // copio tutti i files DWG
            pDwg = (C_DWG *) dwg_list.get_head();
            while (pDwg)
            {
               // ricavo il nome e l'estensione del file
               gsc_splitpath(pDwg->get_name(), &drive, &dir, &name);

               CopiedFile = drive;
               CopiedFile += dir;
               CopiedFile += name;
               CopiedFile += _T(".BAK");

               if (gsc_copyfile(pDwg->get_name(), CopiedFile.get_name()) == GS_BAD)
                  { result = GS_BAD; break; }

               pDwg = (C_DWG *) dwg_list.get_next();
            }
         }

         break;
      }

      case GSRestoreBackUp: // ripristina backup
      {
         C_STRING Dest;

         pFileName = (C_STR *) FileNameList.get_head();
         
         while (pFileName)
         {
            CopiedFile = dir_dwg;
            CopiedFile += _T('\\');
            CopiedFile += pFileName->get_name();
            // ricavo il nome e l'estensione del file dwg
            gsc_splitpath(CopiedFile, NULL, NULL, &name);

            Dest = dir_dwg;
            Dest += _T('\\');
            Dest += name;
            Dest += _T(".DWG");

            if (gsc_copyfile(CopiedFile, Dest) == GS_BAD)
               { result = GS_BAD; break; }

            pFileName = (C_STR *) FileNameList.get_next();
         }

         break;
      }
   }

   return result;
}


/*****************************************************************************/
/*.doc C_DWG_INFO::get_isCoordToConvert                           <internal> */
/*+
  Questa funzione restituisce se è necessario effettuare la conversione delle
  coordinate degli oggetti dal sistema nella sessione di lavoro e quello nel DB.

  Ritorna GS_GOOD = da convertire, GS_BAD = da NON convertire, GS_CAN = non inizializzato
-*/  
/*****************************************************************************/
int C_DWG_INFO::get_isCoordToConvert(void)
{
   if (isCoordToConvert != GS_CAN) return isCoordToConvert;  
   if (!GS_CURRENT_WRK_SESSION)
      isCoordToConvert = GS_CAN;
   else
      // Se i sistemi di coordinate NON coincidono, la conversione è necessaria
      isCoordToConvert = (coordinate_system.comp(GS_CURRENT_WRK_SESSION->get_coordinate(), FALSE) != 0) ? GS_GOOD : GS_BAD;

   return isCoordToConvert;
}


/*****************************************************************************/
/*.doc C_DWG_INFO::get_ClsSRID_converted_to_AutocadSRID           <internal> */
/*+
  Questa funzione carica lo SRID in formato Autocad e le unità del 
  sistema di coordinate della classe. 
  Restituisce il puntatore allo SRID della classe convertito (se possibile) 
  nello SRID gestito da Autocad.
-*/  
/*****************************************************************************/
C_STRING *C_DWG_INFO::get_ClsSRID_converted_to_AutocadSRID(void)
{
   if (ClsSRID_unit == GSSRIDUnit_None)
      // Se è stato dichiarato un sistema di coordinate per la classe
      if (coordinate_system.len() > 0)
      {
         C_STRING dummy;

         if (gsc_get_srid(coordinate_system, GSSRID_Autodesk,
                           GSSRID_Autodesk, dummy, &ClsSRID_unit) != GS_GOOD)
            ClsSRID_unit = GSSRIDUnit_None;
      }

   return &coordinate_system;
}


//---------------------------------------------------------------------//
////////////////////   C_DWG_INFO   FINE  ///////////////////////////////
////////////////   C_GPH_INFO_LIST  INIZIO  /////////////////////////////
//---------------------------------------------------------------------//

// costruttore
C_GPH_INFO_LIST::C_GPH_INFO_LIST():C_LIST() {}

// distruttore
C_GPH_INFO_LIST::~C_GPH_INFO_LIST() {}


int C_GPH_INFO_LIST::copy(C_GPH_INFO_LIST &out)
{
	C_GPH_INFO *new_GphInfo, *punt;

	out.remove_all();
	punt = (C_GPH_INFO *) get_head();  
	while (punt)
	{
      if ((new_GphInfo = gsc_alloc_GraphInfo(punt->getDataSourceType())) == NULL)
         return GS_BAD;
		out.add_tail(new_GphInfo);
      punt->copy(new_GphInfo);
	  
		punt = (C_GPH_INFO *) get_next();
	}

	return GS_GOOD;
}

int C_GPH_INFO_LIST::from_rb(resbuf *rb_GphInfolist)
{
   C_GPH_INFO          *currentpGphInfo;
   GraphDataSourceEnum SrcType;

   remove_all();

   if(!rb_GphInfolist) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
   rb_GphInfolist = rb_GphInfolist->rbnext;

   while (rb_GphInfolist->restype != RTLE)
   {
      if (rb_GphInfolist->restype != RTLB)  { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

      if ((SrcType = gsc_getGraphDataSourceType(rb_GphInfolist)) == GSNoneGphDataSource)
         { GS_ERR_COD = eGSInvRBType; return GS_BAD; }
      if ((currentpGphInfo = gsc_alloc_GraphInfo(SrcType)) == NULL)
         { remove_all(); return GS_BAD; }
   
      if (currentpGphInfo->from_rb(rb_GphInfolist) == GS_BAD)
         { delete currentpGphInfo; remove_all(); return GS_BAD; }
  
      add_tail(currentpGphInfo);

      // avanzo all'elemento successivo
		if ((rb_GphInfolist = gsc_scorri(rb_GphInfolist)) == NULL) 
			{ GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      rb_GphInfolist = rb_GphInfolist->rbnext;
   }                      
      
   return GS_GOOD;
} 

presbuf C_GPH_INFO_LIST::to_rb(void)
{
   C_RB_LIST  List;
   C_GPH_INFO *pGphInfo;  

	pGphInfo = (C_GPH_INFO *) get_head();

	List << acutBuildList(RTLB, 0);
   if (!pGphInfo) List << acutBuildList(RTNIL, 0);

   while (pGphInfo)
   {
      List += acutBuildList(RTLB, 0);
      if ((List += pGphInfo->to_rb()) == NULL) return NULL;
      List += acutBuildList(RTLE, 0);
      pGphInfo = (C_GPH_INFO *) get_next();
   }

   List += acutBuildList(RTLE, 0);
   List.ReleaseAllAtDistruction(GS_BAD);

   return List.get_head();
}


//---------------------------------------------------------------------//
////////////////   C_GPH_INFO_LIST  FINE    /////////////////////////////
//---------------------------------------------------------------------//


/*********************************************************/
/*.doc gsc_GphDataSrcFromDB                   <internal> */
/*+
  Questa funzione carica i dati di una C_GPH_INFO dalla
  lettura della tabella GS_CLASS_GRAPH_INFO.
  Parametri:
  C_NODE *pPrj;   Puntatore a progetto
  int cls;  Codice classe
  int sub;  Opzionale; codice sottoclasse (default = 0)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
C_GPH_INFO *gsc_GphDataSrcFromDB(C_NODE *pPrj, int cls, int sub)
{
   C_DBCONNECTION *pConn;
   C_STRING       MetaTableRef, statement;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        pRb;
   C_GPH_INFO     *pResult;

   // setto il riferimento di CLASS_GPH_DATA_SRC_TABLE_NAME (<catalogo>.<schema>.<tabella>)
   if (((C_PROJECT *) pPrj)->getClassGphDataSrcTabInfo(&pConn, &MetaTableRef) == GS_BAD)
      return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += MetaTableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += cls;
   statement += _T(" AND SUB_CL_ID=");
   statement += sub;

   // leggo la riga della tabella
   if (pConn->ExeCmd(statement, pRs) == GS_BAD) return GS_BAD;
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   gsc_DBCloseRs(pRs);
   
   MetaTableRef.clear();
   if ((pRb = ColValues.CdrAssoc(_T("DIR_DWG"))) && pRb->restype == RTSTR)
   {
      MetaTableRef = pRb->resval.rstring;
      MetaTableRef.alltrim();
   }

   if (MetaTableRef.len() > 0)
       pResult = new C_DWG_INFO(); // Collegamento a DWG
   else
       pResult = new C_DBGPH_INFO(); // Collegamento a DB

   if (pResult->from_rb(ColValues) == GS_BAD)
      { delete pResult; return NULL; }
   pResult->prj = pPrj->get_key();

   return pResult;
}


/*********************************************************/
/*.doc gsc_GphDataSrcFromRb                   <internal> */
/*+
  Questa funzione carica i dati di una C_GPH_INFO dalla
  lettura di una lista di resbuf.
  Parametri:
  resbuf *rb;  Lista di resbuf
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
C_GPH_INFO *gsc_GphDataSrcFromRb(C_RB_LIST &ColValues)
{
   return gsc_GphDataSrcFromRb(ColValues.get_head());
}
C_GPH_INFO *gsc_GphDataSrcFromRb(resbuf *rb)
{
   C_STRING   MetaTableRef;
   presbuf    p;
   C_GPH_INFO *pResult;
   
   if ((p = gsc_CdrAssoc(_T("DIR_DWG"), rb, FALSE)))
   {
      MetaTableRef = p->resval.rstring;
      MetaTableRef.alltrim();
   }

   if (MetaTableRef.len() > 0)
       pResult = new C_DWG_INFO(); // Collegamento a DWG
   else
       pResult = new C_DBGPH_INFO(); // Collegamento a DB

   if (pResult->from_rb(rb) == GS_BAD)
      { delete pResult; return NULL; }

   return pResult;
}


/*********************************************************/
/*.doc gsc_lock_on_gs_class_graph_info        <internal> */
/*+
  Questa funzione blocca le righe della classe in GS_CLASS_GRAPH_INFO.
  Parametri:
  C_DBCONNECTION *pConn;            Puntatore connessione OLE-DB
  C_STRING       &ClassesTableRef;  Riferimento alla tabella GS_CLASS_GRAPH_INFO
  int cls;                          Codice classe
  int sub;                          Codice sotto-classe
  _RecordsetPtr &pRs;               Recordset
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_lock_on_gs_class_graph_info(C_DBCONNECTION *pConn, C_STRING &ClassesTableRef, int cls,
                                    int sub, _RecordsetPtr &pRs)
{   
   C_STRING statement;

   // seleziono riga del progetto in GS_PRJ
   statement = _T("SELECT * FROM ");
   statement += ClassesTableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += cls;
   if (sub > 0 )
   {
      statement += _T(" AND SUB_CL_ID=");
      statement += sub;
   }

   if (pConn->get_RowLockSuffix_SQLKeyWord()) // se esiste un suffisso da usare per bloccare la riga
      { statement += _T(" "); statement += pConn->get_RowLockSuffix_SQLKeyWord(); }
   pConn->BeginTrans();
   if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
   {
      if (GS_ERR_COD == eGSLocked) GS_ERR_COD = eGSClassInUse;
      pConn->RollbackTrans();
      return GS_BAD;
   }
   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      gsc_DBCloseRs(pRs);
      GS_ERR_COD = eGSInvClassCode;
      pConn->RollbackTrans();
      return GS_BAD;
   }

   return gsc_DBLockRs(pRs);
}


/*********************************************************/
/*.doc gsc_unlock_on_gs_class_graph_info      <internal> */
/*+
  Questa funzione sblocca la riga della sessione in GS_CLASS_GRAPH_INFO.
  Chiude una transazione aperta con "gsc_lock_on_gs_class_graph_info"
  e chiude anche il recordset.
  Parametri:
  _RecordsetPtr &pRs;   Recordset di lettura
  int           Mode;   Flag se = UPDATEABLE viene aggiornato il 
                        recordset altrimenti vengono abbandonate
                        le modifiche (default = UPDATEABLE)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int gsc_unlock_on_gs_class_graph_info(C_DBCONNECTION *pConn, _RecordsetPtr &pRs, int Mode)
{
   if (gsc_DBUnLockRs(pRs, Mode, true) == GS_BAD)
   {
      pConn->RollbackTrans();
      return GS_BAD;
   }

   if (Mode != UPDATEABLE)
      pConn->RollbackTrans();
   else
      pConn->CommitTrans();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_getDefaultGraphInfo                 <external> */
/*+
  Questa funzione LISP restituisce una lista dei valori di default 
  delle caratteristiche di memorizzazione della geometria.
  Parametri da lisp: [prj[External Data]]

  External Data = se T significa che si vuole un collegamento a dati esterni già esistenti. 
                  In tutti gli altri casi si vuole che i dati geometrici siano 
                  creati e gestiti da GEOsim (default nil)
-*/  
/*********************************************************/
int gs_getDefaultGraphInfo(void)
{
   C_GPH_INFO *p;
   presbuf    arg = acedGetArgs();
   C_RB_LIST  ret;
   bool       ExternalData = false;

   acedRetNil();

   // Codice progetto opzionale
   if (arg)
   {
      int prj;

      if (gsc_rb2Int(arg, &prj) == GS_BAD) prj = 0;
      if ((arg = arg->rbnext) && arg->restype == RTT) ExternalData = true;

      p = gsc_getDefaultGraphInfo(prj, ExternalData);
   }
   else p = gsc_getDefaultGraphInfo();

   if (p == NULL) return RTERROR;
   
   ret << p->to_rb();
   ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_getDefaultGraphInfo                <external> */
/*+
  Questa funzione restituisce i valori di default della GraphInfo.
  Parametri:
  int prj;           Codice del progetto, opzionale (default = 0)
  bool ExternalData; Se true significa che si vuole un collegamento a dati esterni già esistenti. 
                     In tutti gli altri casi si vuole che i dati geometrici siano 
                     creati e gestiti da GEOsim (default = false)

  Ritorna u puntatore ad una C_GPH_INFO in caso di successo altrimenti NULL;
  N.B. Alloca memoria.
-*/  
/*********************************************************/
C_GPH_INFO *gsc_getDefaultGraphInfo(int prj, bool ExternalData)
{
   C_GPH_INFO *p;

   if (ExternalData)
   {
      if ((p = new C_DBGPH_INFO()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

      ((C_DBGPH_INFO *) p)->LinkedTable = true;
      ((C_DBGPH_INFO *) p)->LinkedLblGrpTable = ((C_DBGPH_INFO *) p)->LinkedLblTable = true;

      // se è stato specificato un progetto eredito il suo collegatmento a DB
      if (prj > 0)
      {
         C_PROJECT *pPrj;

         if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
            { delete p; return NULL; }

         ((C_DBGPH_INFO *) p)->ConnStrUDLFile = pPrj->get_ConnStrUDLFile();
         pPrj->ptr_UDLProperties()->copy(((C_DBGPH_INFO *) p)->UDLProperties);
      }
   }
   else
   {
      if ((p = new C_DWG_INFO()) == NULL)
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

      // se è stato specificato un progetto eredito il direttorio dei disegni e 
      // il sistema di coordinate
      if (prj > 0)
      {
         C_PROJECT *pPrj;

         if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
            { delete p; return NULL; }
         ((C_DWG_INFO *) p)->dir_dwg = pPrj->get_dir();
         ((C_DWG_INFO *) p)->coordinate_system = pPrj->get_coordinate();
      }
   }

   return p;
}

inline C_GPH_INFO *gsc_alloc_GraphInfo(GraphDataSourceEnum Type)
{
   C_GPH_INFO *p = NULL;

   switch (Type)
   {
      case GSDwgGphDataSource: 
         if ((p =  new C_DWG_INFO()) == NULL) GS_ERR_COD = eGSOutOfMem;
         break;
      case GSDBGphDataSource:
         if ((p = new C_DBGPH_INFO()) == NULL) GS_ERR_COD = eGSOutOfMem;
         break;
   }

   return p;
}
GraphDataSourceEnum gsc_getGraphDataSourceType(resbuf *arg)
{
   resbuf              *p;
   GraphDataSourceEnum Type;

   if (arg == NULL || arg->restype != RTLB)              // Parentesi '(' 
      { GS_ERR_COD = eGSInvRBType; return GSNoneGphDataSource; }
   if ((p = gsc_CdrAssoc(_T("SOURCE_TYPE"), arg, FALSE)) == NULL)
      { GS_ERR_COD = eGSInvRBType; return GSNoneGphDataSource; }
   if (gsc_rb2Int(p, (int *) &Type) == GS_BAD)
      { GS_ERR_COD = eGSInvRBType; return GSNoneGphDataSource; }

   return Type;
}
GraphDataSourceEnum gsc_getGraphDataSourceType(C_PROFILE_SECTION_BTREE &ProfileSections, const TCHAR *sez)
{
   C_B2STR             *pProfileEntry;
   GraphDataSourceEnum Type;

   // Se non esiste questa informazione non si tratta di connessione grafica
   if (!(pProfileEntry = ProfileSections.get_entry(sez, _T("GPH_INFO.SOURCE_TYPE"))))
      return GSNoneGphDataSource;
   Type = (GraphDataSourceEnum) _wtoi(pProfileEntry->get_name2());

   return Type;
}


/*********************************************************/
/*.doc gsc_GetSpatialFromSQL                  <external> */
/*+
  Questa funzione restituisce le coordinate di un rettangolo
  contenente gli oggetti che soddisfano una condizione SQL.
  Parametri:
  Lista resbuf
  (<cls><cond>[sub])
  oppure
  int         prj;       codice progetto
  C_CLASS     *pCls;     puntatore a classe GEOsim
  const TCHAR *Cond;     condizione SQL
  ads_point   corner1;   angolo basso-sinistra
  ads_point   corner2;   angolo alto-destra

  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
  N.B. La funzione setta la sessione in estrazione e poi la ripristina allo
  stato corrente.
-*/  
/*********************************************************/
int gs_GetSpatialFromSQL(void)
{
   presbuf   arg = acedGetArgs();
   int       cls, sub = 0;
   TCHAR     *cond;
   ads_point corner1, corner2;
   C_RB_LIST result;

   acedRetNil();
   if (!arg || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   cls = arg->resval.rint;

   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   cond = arg->resval.rstring;

   if ((arg = arg->rbnext) && arg->restype != RTSHORT)
      sub = arg->resval.rint;

   if (gsc_GetSpatialFromSQL(cls, sub, cond, corner1, corner2) == GS_BAD)	
      return RTERROR;

   if ((result << acutBuildList(RTPOINT, corner1, RTPOINT, corner2, 0)) == NULL)
      return RTERROR;

   result.LspRetList();

   return RTNORM;
}
int gsc_GetSpatialFromSQL(int cls, int sub, const TCHAR *Cond,
                          ads_point corner1, ads_point corner2)	
{
   C_RB_LIST QryCond;
   C_STRING  ReportFile;

   ReportFile = GEOsimAppl::CURRUSRDIR;
   ReportFile += _T('\\');
   ReportFile += GEOTEMPDIR;
   ReportFile += _T("\\COORD.TXT");

   if (gsc_nethost2drive(ReportFile) == GS_BAD) return GS_BAD;
   if (gsc_path_exist(ReportFile) == GS_GOOD)
      if (gsc_delfile(ReportFile) == GS_BAD) return GS_BAD;
   
   ade_qryclear();
   // Imposto la condizione di location
   QryCond << acutBuildList(RTLB, RTSTR, ALL_SPATIAL_COND, RTLE, 0); // tutto
   if (ade_qrydefine(GS_EMPTYSTR, _T("(") , GS_EMPTYSTR, _T("Location"), 
                     QryCond.get_head(), _T(")")) == ADE_NULLID)
      { GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }

   if (gsc_save_qry() == GS_BAD) return GS_BAD;

   if (gsc_SQLExtract(cls, sub, Cond, REPORT, &ReportFile) == GS_BAD) return GS_BAD;

   if (gsc_GetAreaFromReport(ReportFile.get_name(), corner1, corner2) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/***************************************************************************/
/*.doc (mod 2) gsc_test_cond_fas                                           */
/*+	   
  Questa funzione analizza la condizione di FAS modificata e carica 
  eventuali tipilinea, riempimenti, layer non presenti in memoria.
  Parametri:
  C_FAS &FAS;        Caratteristiche grafiche
  long BitForFAS;    Flag a bit per sapere quali proprietà gestire nella FAS

   Ritorna GS_GOOD in caso di successo altrimenti GS_BAD
-*/  
/**************************************************************************/
int gsc_test_cond_fas(C_FAS &FAS, long BitForFAS)
{  
   // controllo il tipolinea
   if (BitForFAS & GSLineTypeSetting)
      if (gsc_load_linetype(FAS.line) != GS_GOOD) return GS_BAD;

   // controllo il piano
   if (BitForFAS & GSLayerSetting)
      if (gsc_crea_layer(FAS.layer) != GS_GOOD) return GS_BAD;

   // controllo il piano del riempimento
   if (BitForFAS & GSHatchLayerSetting)
      if (gsc_crea_layer(FAS.hatch_layer) != GS_GOOD) return GS_BAD;

   // controllo lo stile
   if (BitForFAS & GSTextStyleSetting)
      if (gsc_load_textstyle(FAS.style) != GS_GOOD) return GS_BAD;

   // controllo il riempimento
   if (BitForFAS & GSHatchNameSetting)  // da fare
   {}

   return GS_GOOD;
}


/*********************************************************************/
/*.doc gsc_ExtrOldGraphObjsFrom_SS <external> */
/*+
  Per ciascun oggetto di un gruppo di selezione, la funzione 
  estrae lo stesso oggetto grafico della stessa classe e con lo stesso 
  codice dal disegno originale.
  Da usare per le sessioni scongelate in cui bisogna caricare e cancellare gli
  oggetti originali.
  Parametri:
  int Cls;                 Codice classe
  int Sub;                 Codice sottoclasse
  C_SELSET &ss;            Gruppo di selezione degli oggetti da estrarre
  C_SELSET &ss_from_dwg;   Gruppo si selezione degli oggetti estratti
  
  La funzione restituisce GS_BAD in caso di errore altrimenti GS_GOOD.
  N.B. Lo stato della sessione è gestito da C_WRK_SESSION::save().
-*/  
/*********************************************************************/
int gsc_ExtrOldGraphObjsFrom_SS(int Cls, int Sub, C_SELSET &ss, C_SELSET &ss_from_dwg)
{
   long       i;
   int        result = GS_GOOD;
   ads_name   last_ent, ent;
   C_EED      eed;
   C_CLASS    *pCls;
   C_RB_LIST  DwgAndHandle;
   TCHAR      Handle[MAX_LEN_HANDLE];
   C_STRING   ODTableName, ODValue;
   short      stop = FALSE;
   C_STR_LIST KeyList;
   C_STR      *pKey;
   bool       EntToErase;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }

   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(Cls, Sub)) == NULL) return GS_BAD;

   // ricavo la tabella interna 
   gsc_getODTableName(GS_CURRENT_WRK_SESSION->get_PrjId(), Cls, Sub, ODTableName);

   // memorizzo l'ultimo oggetto grafico estratto
   if (acdbEntLast(last_ent) != RTNORM) ads_name_clear(last_ent);

   i = 0;
   while (ss.entname(i++, ent) == GS_GOOD)
   {
		if (gsc_IsErasedEnt(ent) == GS_GOOD) // se l'oggetto non esiste
      {
         EntToErase = true;
         if (gsc_UnEraseEnt(ent) != GS_GOOD) return GS_BAD; // lo ripristino
      }
      else
         EntToErase = false;

      // Se la classe usa i DWG
      if (pCls->ptr_GphInfo()->getDataSourceType() == GSDwgGphDataSource)
      {
         // se è un oggetto grafico legato ad un disegno
         // è un oggetto estratto e lo scarto
         if ((DwgAndHandle << gsc_ade_qrygetdwgandhandle(ent)) != NULL)
            { if (EntToErase) gsc_EraseEnt(ent); continue; }
      }
      else
         // ricavo l'handle
         if (gsc_enthand(ent, Handle) == GS_GOOD)
            // se esiste nell'albero delle corrispondenze <Handle>-<codice oggetto grafico>
            // è un oggetto estratto e lo scarto
            if (((C_DBGPH_INFO *) pCls->ptr_GphInfo())->HandleId_LinkTree.search(Handle))
               { if (EntToErase) gsc_EraseEnt(ent); continue; }

      // se non è una entità di GEOsim la scarto
      if (eed.load(ent) != GS_GOOD)
         { if (EntToErase) gsc_EraseEnt(ent); continue; }
         
      // se non è una entità della classe da estrarre la salto
      if (eed.cls != Cls || eed.sub != Sub)
         { if (EntToErase) gsc_EraseEnt(ent); continue; }

      // ricavo ID dell'entità dall'eed
      if (pCls->get_category() != CAT_SPAGHETTI)
         ODValue = eed.gs_id;
      else
         // ricavo ID dell'entità dalla tabella precedente
         if (gsc_getIDfromODTable(ent, ODTableName, ODValue) != GS_GOOD)
            { if (EntToErase) gsc_EraseEnt(ent); continue; }

      if (EntToErase) // se era stato ripristinato
         if (gsc_EraseEnt(ent) != GS_GOOD) return GS_BAD; // lo ricancello

      if ((pKey = new C_STR(ODValue.get_name())) == NULL)
         { result = GS_BAD; GS_ERR_COD = eGSOutOfMem; break; }
      KeyList.add_tail(pKey);
   }

   if (gsc_extractFromKeyList(Cls, Sub, KeyList, EXTRACTION) == GS_BAD)
      return GS_BAD;

   if (ads_name_nil(last_ent))
   {  // prima entità del DB grafico.
      if (gsc_mainentnext(NULL, ent) != GS_GOOD) stop = TRUE;
   }
   else
      if (gsc_mainentnext(last_ent, ent) != GS_GOOD) stop = TRUE;
   
   while (!stop)
   {
      if (ss_from_dwg.add(ent) == GS_BAD) return GS_BAD;
      if (gsc_mainentnext(ent, ent) != GS_GOOD) stop = TRUE;
   }
 
   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_SQLExtract <external> */
/*+
  Questa funzione estrae gli oggetti di una classe con condizione SQL
  sulla tabella OLD.
  L'operazione di attach avviene in "gruppo" per tutti i DWG della
  classe specificata. Infine viene eseguita un'unica estrazione.
  Parametri:  
  int cls;                 classe di interesse
  int sub;                 sottoclasse relativa
  const TCHAR *Condition;  condizione sql impostata (formato MAP)
  int mode;                modo estrazione      (EXTRACTION, PREVIEW, REPORT)
  C_STRING *pReportFile;   File di report usato solo se "mode"=REPORT
                           (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_SQLExtract(int cls, int sub, const TCHAR *Cond, int mode, C_STRING *pReportFile)
{
   C_STR_LIST KeyList;

   // leggo i codici delle entità che soddisfano la query SQL sulla tabella OLD
   if (gsc_getKeyListFromASISQLonOld(cls, sub, Cond, KeyList) == GS_BAD) return GS_BAD;

   return gsc_extractFromKeyList(cls, sub, KeyList, mode, pReportFile);
}


/*********************************************************/
/*.doc gsc_SQLExtract <external> */
/*+
  Questa funzione estrae gli oggetti di una classe leggendo i
  codici delle entità da una sorgente dati.
  L'operazione di attach avviene in "gruppo" per tutti i DWG della
  classe specificata. Infine viene eseguita un'unica estrazione.
  Parametri:  
  int cls;                 classe di interesse
  int sub;                 sottoclasse relativa
  C_DBCONNECTION           *pConn;
  const TCHAR *TableRef;    condizione sql impostata
  const TCHAR *Field;       campo della tabella
  int mode;                modo estrazione      (EXTRACTION, PREVIEW, REPORT)
  C_STRING *pReportFile;   File di report usato solo se "mode"=REPORT
                           (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_SQLExtract(int cls, int sub, 
                   C_DBCONNECTION *pConn, const TCHAR *TableRef, const TCHAR *Field,
                   int mode, C_STRING *pReportFile)
{
   _RecordsetPtr pRs;
   C_STRING      statement, dummy(Field);
   C_RB_LIST     ColValues;
   presbuf       pRb;
   int           result = GS_GOOD;
   C_STR_LIST    KeyList;
   C_STR         *pKey;
   long          Key;

   statement = _T("SELECT ");

   if (gsc_AdjSyntax(dummy, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;

   statement += dummy;
   statement += _T(" FROM ");

   dummy = TableRef;
   if (gsc_AdjSyntax(dummy, pConn->get_InitQuotedIdentifier(),
                     pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return GS_BAD;
   
   statement += dummy;

   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;

   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   if ((pRb = ColValues.CdrAssoc(Field, FALSE)) == NULL)
      { GS_ERR_COD = eGSInvalidKey; return GS_BAD; }

   // leggo i codici delle entità da estrarre
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { result = GS_BAD; break; }
      if (gsc_rb2Lng(pRb, &Key) == GS_BAD) Key = 0; // Codice gruppo
      if ((pKey = new C_STR()) == NULL)
         { result = GS_BAD; GS_ERR_COD = eGSOutOfMem; break; }
      (*pKey) = Key;
      KeyList.add_tail(pKey);

      gsc_Skip(pRs);
   }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
   if (result == GS_BAD) return GS_BAD;

   return gsc_extractFromKeyList(cls, sub, KeyList, mode, pReportFile);
}


/*********************************************************/
/*.doc gsc_extractFromKeyList                 <external> */
/*+
  Questa funzione estrae gli oggetti identificati dalla lista
  passata come parametro (utilizza il codice dell'entità)
  Parametri:
  int cls;                 codice classe
  int sub;                 codice sottoclasse
  C_STR_LIST &KeyList;     Lista dei codici chiave degli oggetti da estrarre
  int mode;                Flag a bit, Modo estrazione (EXTRACTION, PREVIEW, REPORT)
  C_STRING *pReportFile;   File di report usato solo se "mode"=REPORT
                           (default = NULL)
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. La funzione setta la sessione in estrazione e poi la ripristina allo
  stato corrente.
-*/  
/*********************************************************/
int gsc_extractFromKeyList(int cls, int sub, C_STR_LIST &KeyList, int mode, 
                           C_STRING *pReportFile, int CounterToVideo)
{
   C_CLASS   *pCls;
	C_STRING  ReportTempl;
   C_RB_LIST QryCond;
   long      nExtracted;

   if (KeyList.get_count() == 0) return GS_GOOD;
 
   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;
   if (pCls->ptr_GphInfo() == NULL)
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   if (mode & REPORT) // Leggo il template standard
      if (pCls->ptr_GphInfo()->getLocatorReportTemplate(ReportTempl) == GS_BAD) 
         return GS_BAD;

   ade_qryclear();
   // Imposto la condizione di location
   QryCond << acutBuildList(RTLB, RTSTR, ALL_SPATIAL_COND, RTLE, 0); // tutto
   if (ade_qrydefine(GS_EMPTYSTR, _T("(") , GS_EMPTYSTR, _T("Location"), 
                     QryCond.get_head(), _T(")")) == ADE_NULLID)
      { GS_ERR_COD = eGSQryCondNotDef; return GS_BAD; }
   if (gsc_save_qry() == GS_BAD) return GS_BAD;

   if ((nExtracted = pCls->extract_GraphData(mode, NULL, ADE_SPATIAL_QRY_NAME, &KeyList,
                                             &ReportTempl, pReportFile, CounterToVideo)) == -1)
      return GS_BAD;
   else if (nExtracted == -2) // Interruzione voluta dall'utente
      return GS_CAN;

	return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_getKeyListFromASISQLonOld             <external> */
/*+
  Questa funzione ottiene i codici chiave delle entità che soddisfano
  una condizione SQL nella tabella OLD.
  Parametri:  
  int cls;                 classe di interesse
  int sub;                 sottoclasse relativa
  const TCHAR *Condition;  condizione sql impostata (formato MAP)
  C_STR_LIST &KeyList;     Lista dei codici chiave
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getKeyListFromASISQLonOld(int cls, int sub, const TCHAR *Cond, C_STR_LIST &KeyList)
{
   C_DBCONNECTION *pConn;
   C_STRING       UDL, statement, MapTableRef, KeyAttribCorrected;
   C_INFO         *pInfo;
   C_CLASS        *pCls;
   CAsiSession    *pSession;
   CAsiExecStm    *pExecStm;
   CAsiCsr        *pCsr;
   CAsiData       *pData;
   CAsiRow        *pRow;
   resbuf         rbKey;
   C_STR          *pKey;
   long           Key;
   int            Result = GS_GOOD;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return GS_BAD; }
   
   // Ritorna il puntatore alla classe cercata
   if ((pCls = GS_CURRENT_WRK_SESSION->find_class(cls, sub)) == NULL) return GS_BAD;
      
   // Ricavo le informazioni sulla classe
   if ((pInfo = pCls->ptr_info()) == NULL || !pCls->ptr_attrib_list())
      { GS_ERR_COD = eGSInvClassType; return GS_BAD; }

   // preparo la ricerca dei codici dei record che soddisfano la condizione SQL
   // per la tabella OLD
   if ((pConn = pInfo->getDBConnection(OLD)) == NULL) return GS_BAD;

   if (gsc_Table2MAPFormat(pConn, pInfo->OldTableRef, MapTableRef) == GS_BAD) return GS_BAD;

   // Correggo la sintassi del nome del campo per SQL MAP
   KeyAttribCorrected = pInfo->key_attrib;
   gsc_AdjSyntaxMAPFormat(KeyAttribCorrected);

   statement = _T("SELECT ");
   statement += KeyAttribCorrected;
   statement += _T(" FROM ");
   statement += MapTableRef;
   statement += _T(" WHERE ");
   statement += Cond;

   // Preparo se necessario il file UDL
   if (((C_CLASS *) pCls)->getLPNameOld(UDL) == GS_BAD) return GS_BAD;
   if (gsc_SetACADUDLFile(UDL.get_name(), pConn, pInfo->OldTableRef.get_name()) == GS_BAD)
      return GS_BAD;
   // Creo una sessione ASI utilizzando il file UDL
   if ((pSession = gsc_ASICreateSession(UDL.get_name())) == NULL) return GS_BAD;
   if (gsc_ASIPrepareSql(pSession, statement.get_name(), UDL.get_name(),
                         &pExecStm, &pCsr) == GS_BAD)
      { gsc_ASITermSession(&pSession); return GS_BAD; }
   // Apro cursore
   if (pCsr->Open() != kAsiGood)
   {
      delete pCsr;
      gsc_ASITermStm(&pExecStm);
      gsc_ASITermSession(&pSession);
      return GS_BAD;
   }
   
   KeyList.remove_all();
   // Ciclo di lettura dei codici
   while (pCsr->Fetch() == kAsiGood)
   {
      if ((pRow = pCsr->getCurrentRow()) == NULL ||
          (pData = (*pRow)[0].getData()) == NULL ||
          gsc_ASIData2Rb(pData, &rbKey) == GS_BAD ||
          gsc_rb2Lng(&rbKey, &Key) == GS_BAD)
         { Result = GS_BAD; GS_ERR_COD = eGSReadRow; break; }
      
      if ((pKey = new C_STR()) == NULL)
         { Result = GS_BAD; GS_ERR_COD = eGSOutOfMem; break; }
      (*pKey) = Key;
      KeyList.add_tail(pKey);
   }
   pCsr->Close();
   delete pCsr;
   gsc_ASITermStm(&pExecStm);
   gsc_ASITermSession(&pSession);

   return Result;
}


/////////////////////////////////////////////////////////////////////////
/////////////        FUNZIONI PER C_CLASS    INIZIO            //////////
/////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc long C_CLASS::save_GeomData            <internal> /*
/*+
  Effettua il salvataggio degli oggetti grafici della classe.

  Restituisce GS_GOOD se tutto OK altrimenti GS_BAD
-*/  
/*********************************************************/
int C_CLASS::save_GeomData(void)
{
   TCHAR Msg[MAX_LEN_MSG];

   // verifico abilitazione
   if (gsc_check_op(opSaveArea) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   if (!ptr_GphInfo()) return GS_GOOD; // classe senza dati grafici

   // se la tabella è collegata non posso salvare perchè gestita da altre applicazioni
   if (ptr_GphInfo() && ptr_GphInfo()->getDataSourceType() == GSDBGphDataSource &&
       (((C_DBGPH_INFO *) ptr_GphInfo())->LinkedTable ||
        ((C_DBGPH_INFO *) ptr_GphInfo())->LinkedLblGrpTable ||
        ((C_DBGPH_INFO *) ptr_GphInfo())->LinkedLblTable))
      { GS_ERR_COD = eGSClassLocked; return GS_BAD; }

   acutPrintf(gsc_msg(121), id.name); // "\nInizio salvataggio grafica classe %s..."
   swprintf(Msg, MAX_LEN_MSG, _T("Graphical entities saving, class <%s>."), id.name);  
   gsc_write_log(Msg);

   return ptr_GphInfo()->Save();
}


/*********************************************************/
/*.doc long C_CLASS::extract_GraphData()      <internal> /*
/*+
  Effettua l'estrazione degli oggetti grafici della classe
  bloccando la classe in estrazione e rilasciandola.
  La funzione riallinea i link e nel caso di alterazioni grafiche
  inserisce gli oggetti nel gruppo di selezione GS_ALTER_FAS_OBJS.
  Le query spaziale e SQL devono essere già impostate.
  La funzione provvede ad impostare le eventuali condizioni di alterazione
  grafiche.
  Parametri:
  int mode;               Modo di estrazione PREVIEW, EXTRACTION, REPORT
  C_SELSET *pSS;          Puntatore a sel set (già allocato), se <> NULL verranno aggiunti
                          gli oggetti estratti (default = NULL)
  C_STR_LIST *pIdList;    Lista di codici di entità da elaborare se = NULL verrà usata
                          la condizione SQL memorizzata per l'estrazione (default = NULL)
  C_STRING *pRptTemplate; Opzionale. Usato solo se mode = REPORT, indica le informazioni 
                          da scrivere nel file testo di report. Se = NULL verrà letto
                          dall'impostazione ADE corrente (default = NULL)
  C_STRING *pRptFile;     Opzionale. Usato solo se mode = REPORT, path completa del file
                          testo di report. Se = NULL verrà letto
                          dall'impostazione ADE corrente (default = NULL)                        
  int CounterToVideo;     Flag, se = GS_GOOD stampa a video eventuali messaggi
                          (default = GS_GOOD)
  C_FAMILY_LIST *pFamilyList; Opzionale, serve per verificare se la classe appartiene a gruppi.
                              Se NULL lo ricava la funzione (default = NULL)

  Restituisce il numero di oggetti estratti se tutto OK altrimenti 
  -1 in caso di errore, -2 in caso di interruzione voluta dall'utente con CTRL-C.
-*/  
/*********************************************************/
long C_CLASS::extract_GraphData(int mode, C_SELSET *pSS, TCHAR *SpatialQryName,
                                C_STR_LIST *pIdList,
                                C_STRING *pRptTemplate, C_STRING *pRptFile, 
                                int CounterToVideo,
                                C_FAMILY_LIST *pFamilyList)
{
   long       tot = 0;
   int        result;
   C_STRING   RptFile, RptTemplate;

   if (!GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSNotCurrentSession; return -1; }
   if (id.abilit == EXCLUSIVEBYANOTHER) { GS_ERR_COD = eGSClassLocked; return -1; }

   if (is_extracted() == GS_BAD) // se la classe non era ancora stata estratta
      if (ptr_fas() && ptr_GphInfo())
         ptr_fas()->convertUnit(ptr_GphInfo()->get_UnitCoordConvertionFactorFromClsToWrkSession());

   // se si deve generare un report e non è specificato Template o File
   if (mode & REPORT)
   {
      if (pRptTemplate) RptTemplate = pRptTemplate->get_name();
      if (pRptFile) RptFile = pRptFile->get_name();

      if (!pRptTemplate || !pRptFile)
      {
         C_STRING    dummy, *p1 = NULL, *p2 = NULL;
         ade_boolean multiLine;

         if (!pRptTemplate) p1 = &RptTemplate;
         if (!pRptFile) p2 = &RptFile;

         // leggo il nome del file e del template dalle impostazioni correnti ADE
         if (gsc_ade_qrygettype(&dummy, &multiLine, p1, p2) == GS_BAD) return -1;
      }
   }

   // setta la classe in stato di estrazione/salvataggio in modo che nessuna altra 
   // sessione possa effettuare il salvataggio nello stesso momento
   if (set_locked_on_extract(GS_CURRENT_WRK_SESSION->get_id(), &result) == GS_BAD ||
       result == GS_BAD)
      return -1;

   // se simulazione leggo dalla prima sottoclasse
   if (id.category == CAT_EXTERN)
   {
      C_NODE *ptr  = ptr_sub_list()->get_cursor(); // posiz. precedente
      C_SUB  *pSub = (C_SUB *) ptr_sub_list()->get_head(), *pPrevSub;
      long   PartialTot;

      do
      {
         PartialTot = pSub->extract_GraphData_internal(mode, pSS, SpatialQryName, pIdList,
                                                       &RptTemplate, &RptFile, CounterToVideo);

         if (PartialTot < 0)
         {
            tot = PartialTot; // errore
            break;
         }

         pPrevSub = pSub;
         // vado alla sottoclasse successiva
         pSub = (C_SUB *) pSub->get_next();

         // Solo se la sottoclasse usa DWG
         if (pSub && 
             pSub->ptr_GphInfo()->getDataSourceType() == GSDwgGphDataSource)
            while (pSub)
            {
               // verifico se questa sottoclasse usa risorse grafiche
               // già usate dalle sottoclassi precedenti
               while (pSub->ptr_GphInfo()->IsTheSameResource(pPrevSub->ptr_GphInfo()) == false)
                   if ((pPrevSub = (C_SUB *) pPrevSub->get_prev()) == NULL) break;

               // Se NON esiste una sottoclasse che usa le stesse risorse grafiche
               if (!pPrevSub) break; // esco dal ciclo

               pSub = (C_SUB *) pSub->get_next();
            }
      }
      while (pSub);
      ptr_sub_list()->getpos(ptr);
   }
   else // classi NON simulazioni
   {
      if (!pIdList) // Se non è passata una lista di codici di entità
      {
         C_STR_LIST IdList, IdListFromGrp;
         C_STRING   SQL;
         bool       ClsHasSQL = false, UseIdList = false;

         // carico eventuale query SQL
         if (gsc_ClsSQLQryLoad(id.code, id.sub_code, SQL) == GS_GOOD)
         {
            ClsHasSQL = true;
            UseIdList = true;
            // leggo i codici delle entità che soddisfano la query SQL sulla tabella OLD
            if (gsc_getKeyListFromASISQLonOld(id.code, id.sub_code, SQL.get_name(), IdList) == GS_BAD)
               { set_unlocked(GS_CURRENT_WRK_SESSION->get_id(), GS_GOOD); return -1; }
         }

         C_FAMILY_LIST family_list, *_pFamilyList;
         C_FAMILY      *pFamily;

         // se la famiglia non è arrivata come parametro me la ricavo
         if (!pFamilyList)
         {
            if (gsc_getfamily(GS_CURRENT_WRK_SESSION->get_pPrj(), &family_list) == GS_BAD)
               { set_unlocked(GS_CURRENT_WRK_SESSION->get_id(), GS_GOOD); return -1; }
            _pFamilyList = &family_list;
         }
         else
            _pFamilyList = pFamilyList;

         pFamily = (C_FAMILY *) _pFamilyList->get_head();
         while (pFamily)
         {  // se la classe è membro di questa famiglia-gruppo
            if (pFamily->relation.search_key(id.code) != NULL)
            {
               C_CLASS *pGroupCls = GS_CURRENT_WRK_SESSION->get_pPrj()->find_class(pFamily->get_key());

               // se anche la classe gruppo è da estrarre
               if (pGroupCls->ptr_id()->sel == SELECTED || pGroupCls->ptr_id()->sel == EXTR_SEL)
                  // se esiste una condizione SQL sul gruppo
                  if (gsc_ClsSQLQryLoad(pFamily->get_key(), 0, SQL) == GS_GOOD)
                  {
                     C_STR_LIST       GroupIdList;
                     C_STR            *pGroupId;
                     C_PREPARED_CMD   PreparedOldCmd, NoUsedCmd;
                     C_2INT_LONG_LIST ent_list;
                     C_2INT_LONG      *ent_punt;
                     C_STRING         IdString;

                     UseIdList = true;

                     // leggo i codici delle entità gruppo che soddisfano la query SQL sulla tabella OLD
                     if (gsc_getKeyListFromASISQLonOld(pFamily->get_key(), 0, SQL.get_name(), GroupIdList) == GS_BAD)
                        { set_unlocked(GS_CURRENT_WRK_SESSION->get_id(), GS_GOOD); return -1; }

                     // compilazione per tabella OLD link
                     if (pGroupCls->prepare_reldata_where_key(PreparedOldCmd, OLD) == GS_BAD)
                        { set_unlocked(GS_CURRENT_WRK_SESSION->get_id(), GS_GOOD); return -1; }

                     // scorro la lista dei gruppi collegati all'entità
                     pGroupId = (C_STR *) GroupIdList.get_head();
                     while (pGroupId)
                     {
                        if (pGroupCls->get_member(NoUsedCmd, PreparedOldCmd,
   			 	                                   pGroupId->tol(), &ent_list) == GS_BAD)
                           { set_unlocked(GS_CURRENT_WRK_SESSION->get_id(), GS_GOOD); return -1; }

                        // per ogni gruppo leggo il codice entità membro della classe semplice
                        ent_punt = (C_2INT_LONG *) ent_list.get_head();
                        while (ent_punt)
                        {
                           if (ent_punt->get_key() == id.code && ent_punt->get_type() == id.sub_code)
                           {
                              IdString = ent_punt->get_id();
                              IdListFromGrp.add_tail_str(IdString.get_name());
                           }

                           ent_punt = (C_2INT_LONG *) ent_list.get_next();
                        }

                        pGroupId = (C_STR *) GroupIdList.get_next();
                     }

                     // se era stata impostata una condizione SQL sulla classe
                     if (ClsHasSQL)
                        // faccio l'intersezione tra i codici che soddisfano l'SQL della classe con quelli del gruppo
                        IdList.listIntersect(IdListFromGrp);
                     else // altrimenti uso la lista del gruppo
                        IdList.listUnion(IdListFromGrp);
                  }
            }

            pFamily = (C_FAMILY *) pFamily->get_next();
         }

         if (tot != 1)
            if (UseIdList == true)
               tot = extract_GraphData_internal(mode, pSS, SpatialQryName, &IdList,
                                                &RptTemplate, &RptFile, CounterToVideo);
            else
               tot = extract_GraphData_internal(mode, pSS, SpatialQryName, NULL,
                                                &RptTemplate, &RptFile, CounterToVideo);
      }
      else // Se è passata una lista di codici di entità 
         tot = extract_GraphData_internal(mode, pSS, SpatialQryName, pIdList,
                                          &RptTemplate, &RptFile, CounterToVideo);
   }

   // slocco la classe
   set_unlocked(GS_CURRENT_WRK_SESSION->get_id(), GS_GOOD);

   return tot;
}


/*********************************************************/
/*.doc long C_CLASS::extract_GraphData_internal() <internal> /*
/*+
  Funzione interna usata extract_GraphData che effettivamente estrae
  gli oggetti grafici della classe.
  La funzione riallinea i link e nel caso di alterazioni grafiche
  inserisce gli oggetti nel gruppo di selezione GS_ALTER_FAS_OBJS.
  Eventualmente aggiunge i link ASE e riallinea la grafica.
  La funzione provvede ad impostare le eventuali condizioni di alterazione
  grafiche.
  Parametri:
  int mode;               Flag a bit. Modi di estrazione PREVIEW, EXTRACTION, REPORT
                          anche combinati tra loro.
  C_SELSET *pSS;          Opzionale. Puntatore a sel set (già allocato), 
                          se <> NULL verranno aggiunti gli oggetti estratti 
                          (default = NULL)
  TCHAR *SpatialQryName;  Opzionale, nome della query spaziale ADE. 
                          Se = NULL viene usata la query ADE di default = "spaz_estr"
  C_STR_LIST *pIdList;    Opzionale. Lista dei codici delle entità da estrarre
                          (default = NULL)
  C_STRING *pRptTemplate; Opzionale. Usato solo se mode = REPORT, indica le informazioni 
                          da scrivere nel file testo di report (default = NULL)
  C_STRING *pRptFile;     Opzionale. Usato solo se mode = REPORT, path completa del file
                          testo di report (default = NULL)
  int CounterToVideo; flag, se = GS_GOOD stampa a video il numero di entità che si 
                      stanno elaborando (default = GS_BAD)

  Restituisce il numero di oggetti estratti se tutto OK altrimenti 
  -1 in caso di errore, -2 in caso di interruzione voluta dall'utente con CTRL-C.

  N.B. La funzione non setta lo stato della sessione di lavoro corrente in estrazione.
-*/  
/*********************************************************/
long C_CLASS::extract_GraphData_internal(int mode, C_SELSET *pSS, 
                                         TCHAR *SpatialQryName, C_STR_LIST *pIdList,
                                         C_STRING *pRptTemplate, C_STRING *pRptFile,
                                         int CounterToVideo)
{
   long       tot = 0, nPartial, flag_fas = GSNoneSetting;
   C_SELSET   SelSet;
   int        result;
   bool       AlteratedFAS = FALSE, AddToSelSet = FALSE;
   C_FAS      AlteredFAS;
   C_STR_LIST *pInternalIdList;
   C_STR      *pId;

   if (!ptr_GphInfo()) { GS_ERR_COD = eGSInvClassType; return -1; }

   if (pIdList) // se si deve estrarre solo le entità con codici noti
   {
      pInternalIdList = pIdList;
      pInternalIdList->get_head(); // mi posiziono all'inizio della lista
   }
   else pInternalIdList = new C_STR_LIST();

   // se estrazione in modo REPORT cancello il file di report eventualmente esistente
   if ((mode & REPORT) && pRptFile)
      if (gsc_path_exist(*pRptFile) == GS_GOOD)
         if (gsc_delfile(*pRptFile) == GS_BAD) return -1;

   result = GS_GOOD;
   do
   {
      pId = (C_STR *) pInternalIdList->get_cursor();

      // se si deve estrarre solo le entità con codici noti e la lista è vuota
      if (pIdList && pIdList->get_count() == 0)
         return 0;

      // carico e attivo la query spaziale e sql
      if (ptr_GphInfo()->LoadQueryFromCls(SpatialQryName, *pInternalIdList) == GS_BAD)
         { result = GS_BAD; break; }

      // se si devono estrarre oggetti a video e sono impostate delle alterazioni grafiche
      if (mode && EXTRACTION && 
          gsc_ClsAlterPropQryLoad(id.code, id.sub_code, AlteredFAS, &flag_fas) == GS_GOOD)
      {
         AlteratedFAS = TRUE;

         // analizzo la condizione e carico eventuali tipi linea, stili testo,
         // riempimenti non presenti in memoria
         if ((result = gsc_test_cond_fas(AlteredFAS, flag_fas)) == GS_BAD) break;

         // se classe nodo con almeno un attributo visibile e
         // se l'alterazione della FAS riguarda il blocco 
         // allora estraggo i blocchi $T senza modificarne la definizione
         if (id.type == TYPE_NODE && ptr_attrib_list()->is_visible() == GS_GOOD &&
             (flag_fas & GSBlockNameSetting))
         {
            // cerco solo le etichette
            if ((result = ptr_GphInfo()->AddQueryOnlyLabels()) == GS_BAD) break;

            // Imposto le alterazioni nella query corrente senza alterazione blocco
            if ((nPartial = ptr_GphInfo()->Query(mode, 
                                                 &SelSet, flag_fas - GSBlockNameSetting, &AlteredFAS,
                                                 NULL, NULL, NULL, CounterToVideo)) < 0)
               { result = (nPartial == -1 ) ? GS_BAD : GS_CAN; break; }

            tot += nPartial;

            // ricarica la condizione di query impostata per la classe 
            // (sia spaziale che SQL)
            pInternalIdList->set_cursor(pId); // riposiziono il cursore per estrarre
                                              // le stesse entità estratte prima
            if ((result = ptr_GphInfo()->LoadQueryFromCls(SpatialQryName, 
                                                          *pInternalIdList)) == GS_BAD) break;

            // cerco solo scartando le etichette e li visualizza e/o ne fa il preview e/o ne fa un report
            if ((result = ptr_GphInfo()->AddQueryOnlyGraphObjs()) == GS_BAD) break;
         }
      }

      // se si devono estrarre oggetti a video
      if (mode & EXTRACTION)
         // Se si altera la grafica
         if (AlteratedFAS) AddToSelSet = TRUE;
         else
            // Se si tratta di spaghetti se bisogna restituire un gruppo di selezione
            if (id.category == CAT_SPAGHETTI)
               { if (pSS) AddToSelSet = TRUE; }
            else // altrimenti sempre
               AddToSelSet = TRUE;

      // Cerca gli oggetti e li visualizza e/o ne fa il preview e/o ne fa un report
      #if defined(GSDEBUG) // se versione per debugging
         double t1 = gsc_get_profile_curr_time();
      #endif
      if ((nPartial = ptr_GphInfo()->Query(mode, 
                                           (AddToSelSet) ? &SelSet : NULL, flag_fas, &AlteredFAS,
                                           pRptTemplate, pRptFile, _T("a"), CounterToVideo)) < 0)
         { result = (nPartial == -1 ) ? GS_BAD : GS_CAN; break; }
      #if defined(GSDEBUG) // se versione per debugging
         gsc_profile_int_func(t1, 0, gsc_get_profile_curr_time(), "ptr_GphInfo()->Query");
      #endif

      tot += nPartial;
   }
   while (pInternalIdList->get_cursor());

   if (pIdList == NULL) delete pInternalIdList;

   switch (result)
   {
      case GS_CAN:
         tot = -2;
         break;
      case GS_BAD:
         tot = -1;
   }

   // Se si sta estraendo oggetti a video di una classe con collegamento a DB
   if (mode & EXTRACTION && id.category != CAT_SPAGHETTI)
   {
      C_LINK_SET LnkDB;
      // aggiorno i link a DB
      if (LnkDB.RefreshSS(SelSet, CounterToVideo) == GS_BAD) return -1;
 
      // se si devono aggiugere i link ASE con LPT
      if (GEOsimAppl::GLOBALVARS.get_AddLPTOnExtract() == GS_GOOD)
         gsc_SetASEKey(SelSet, CounterToVideo);

      // se si deve aggiornare la grafica di una classe aggiornabile in 
      // una sessione aggiornabile, non congelata e nel caso:
      // 1) simulazione con la variabile di aggiornamento grafica 
      //    per simulazioni impostata
      // 2) classe con la variabile di aggiornamento grafica per qualsiasi classe impostata
      if (GS_CURRENT_WRK_SESSION->HasBeenFrozen() == GS_BAD &&
          GS_CURRENT_WRK_SESSION->get_level() == GSUpdateableData && 
          id.abilit == GSUpdateableData &&
          (GEOsimAppl::GLOBALVARS.get_UpdGraphOnExtractSim() == GS_GOOD && (id.category == CAT_EXTERN || is_subclass() == GS_GOOD)) ||
          GEOsimAppl::GLOBALVARS.get_UpdGraphOnExtract() == GS_GOOD)
         //                  selset,change_fas,AttribValuesFromVideo,SS,CounterToVideo,tipo modifica
         if (gsc_class_align(SelSet, GS_BAD, GS_BAD, &(GEOsimAppl::REFUSED_SS), 
                             CounterToVideo, RECORD_MOD) == -1)
            return -1;
   }

   // Se gli oggetti sono stati caricati con alterazioni grafiche
   // in una sessione di lavoro aggiornabile in una classe aggiornabile
   if (GS_CURRENT_WRK_SESSION->get_level() == GSUpdateableData &&
       id.abilit == GSUpdateableData &&
       AlteratedFAS && (mode & EXTRACTION))
   {
      C_STRING pathfile;
      
      // memorizzo in un gruppo di selezione gli oggetti con FAS modificata
      pathfile = GS_CURRENT_WRK_SESSION->get_dir();
      pathfile += _T('\\');
      pathfile += GEOTEMPSESSIONDIR;
      pathfile += _T('\\');
      pathfile += GS_ALTER_FAS_OBJS_FILE;
      if (GS_ALTER_FAS_OBJS.add_selset(SelSet, GS_GOOD, pathfile.get_name()) == GS_BAD) return -1;
   }

   // Se è stata fatta un'estrazione senza errori
   if ((mode & EXTRACTION) && tot >= 0)
   { // scrivo nel file della sessione di lavoro le zone estratte
      C_RECT_LIST RectList;

      if (gsc_getCurrQryExtents(RectList) == GS_GOOD)
      {
         C_STRING pathfile;
         FILE     *f;
         bool     Unicode = false;
   
         GS_CURRENT_WRK_SESSION->get_TempInfoFilePath(pathfile);

         if ((f = gsc_open_profile(pathfile, UPDATEABLE, MORETESTS, &Unicode)) != NULL)
         {
            C_RECT      *pRect = (C_RECT *) RectList.get_head();
            C_STRING    buffer, entry;
            C_2STR_LIST EntryList;

            if (gsc_get_profile(f, WRK_SESSIONS_AREAS_SEZ, EntryList, Unicode) == GS_BAD &&
                GS_ERR_COD != eGSSezNotFound)
               { gsc_close_profile(f); return -1; }
            entry = EntryList.get_count();

            while (pRect)
            {
               pRect->toStr(buffer);
               if (gsc_set_profile(f, WRK_SESSIONS_AREAS_SEZ, entry.get_name(), buffer.get_name(), Unicode) == GS_BAD)
                  { gsc_close_profile(f); return -1; }

               pRect = (C_RECT *) RectList.get_next();
            }
            gsc_close_profile(f);
         }
      }
   }

   if (pSS) pSS->add_selset(SelSet);   

   return tot;
}


/////////////////////////////////////////////////////////////////////////
//         FINE FUNZIONI DELLA  C_CLASS                                //
//         INIZIO FUNZIONI DELLA CATEGORIA C_EXTERN                    //
/////////////////////////////////////////////////////////////////////////


/*********************************************************/
/*.doc C_EXTERN::save_GeomData                 <internal> */
/*+                                                                       
  Effettua il salvataggio degli oggetti grafici della classe.
  					
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_EXTERN::save_GeomData(void)
{
   C_SUB *pSub = (C_SUB *) ptr_sub_list()->get_head();
   TCHAR Msg[MAX_LEN_MSG];
   int   result = GS_GOOD;

   // verifico abilitazione
   if (gsc_check_op(opSaveArea) == GS_BAD) return GS_BAD;

   if (id.abilit != GSUpdateableData)
   {
      GS_ERR_COD = (id.abilit == GSReadOnlyData) ? eGSClassIsReadOnly : eGSClassLocked;
      return GS_BAD;
   }

   acutPrintf(gsc_msg(121), id.name); // "\nInizio salvataggio grafica classe %s..."
   swprintf(Msg, MAX_LEN_MSG, _T("Graphical entities saving, class <%s>."), id.name);  
   gsc_write_log(Msg);

   pSub = (C_SUB *) ptr_sub_list()->get_head();

   while (pSub)
   {
      // Salvo dati sottoclasse
      if (pSub->ptr_GphInfo())
         if (pSub->ptr_GphInfo()->Save() == GS_BAD)
            { result = GS_BAD; break; }

      pSub = (C_SUB *) pSub->get_next();
   }

   return result;
}


/////////////////////////////////////////////////////////////////////////
//         FINE   FUNZIONI DELLA CATEGORIA C_EXTERN                    //
/////////////////////////////////////////////////////////////////////////


/*********************************************************************/
/*.doc gs_check_geoms_for_save                            <external> */
/*+
  Questa funzione lisp verifica che la geometria degli oggetti grafici
  sia idonea al tipo di risorsa grafica usata da una data classe.
  Parametri:
  (<ss><prj><cls>[<sub>])

  <sub>         = codice sottoclasse (default = 0)
  
  Ritorna gli oggetti non idonei.
-*/  
/*********************************************************************/
int gs_check_geoms_for_save(void)
{
   presbuf  arg = acedGetArgs();
   C_SELSET ss, res, entSS;
   ads_name ent;
   long     i = 0, Tot;
   C_CLASS  *pDestCls, *pSrcCls;
   int      prj, cls, sub = 0;
   C_EED    eed;
   C_STRING WhyNotValid;

   acedRetNil();

   // Gruppo di selezione da esaminare 
   if (arg->restype != RTPICKS && arg->restype != RTENAME)
      { GS_ERR_COD = eGSInvRBType; return RTERROR; }
   ss << arg->resval.rlname;
   ss.ReleaseAllAtDistruction(GS_BAD);
   arg = arg->rbnext;
   // Classe in cui gli oggetti grafici dovranno essere salvati
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   if ((pDestCls = gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;
   if (pDestCls->ptr_GphInfo() == NULL)
      { GS_ERR_COD = eGSInvClassType; return RTERROR; }

   Tot = ss.length();
   C_STATUSBAR_PROGRESSMETER StatusBarProgressMeter(gsc_msg(1063)); // "Elaborazione oggetti grafici"
   StatusBarProgressMeter.Init(Tot);

   while (ss.entname(0, ent) == GS_GOOD)
   {
      // Se è un oggetto grafico di GEOsim con database associato
      if (eed.load(ent) == GS_GOOD && GS_CURRENT_WRK_SESSION &&
          (pSrcCls = GS_CURRENT_WRK_SESSION->find_class(eed.cls, eed.sub)) &&
          pSrcCls->ptr_info() &&
          pSrcCls->get_SelSet(ent, entSS) == GS_GOOD && entSS.length() > 0)
      {
         if (!pDestCls->ptr_GphInfo()->HasValidGeom(entSS, WhyNotValid))
         {
            if (WhyNotValid.len() > 0)
            {
               acutPrintf(GS_LFSTR);
               acutPrintf(WhyNotValid.get_name());
               gsc_write_log(WhyNotValid);
            }
            res.add_selset(entSS);
         }
         long PrevLen = ss.length();
         ss.subtract(entSS);
         i = i + (PrevLen - ss.length());
      }
      else
      {
         if (!pDestCls->ptr_GphInfo()->HasValidGeom(ent, WhyNotValid))
         {
            if (WhyNotValid.len() > 0)
            {
               acutPrintf(GS_LFSTR);
               acutPrintf(WhyNotValid.get_name());
               gsc_write_log(WhyNotValid);
            }
            res.add(ent);
         }
         i++;
         ss.subtract_ent(ent);
      }
      StatusBarProgressMeter.Set(i);
   }

   StatusBarProgressMeter.End(gsc_msg(1090)); // "Terminato."
   acutPrintf(gsc_msg(227), Tot,  res.length()); // "\nOggetti grafici elaborati %ld, scartati %ld."
   
   if (res.length() == 0) return RTNORM;
   res.get_selection(ent);
   acedRetName(ent, RTPICKS);
   res.ReleaseAllAtDistruction(GS_BAD);

   return RTNORM;
}


/*********************************************************/
/*.doc gs_mod_dbgph_info                      <external> */
/*+
  Questa funzione LISP modifica la C_DBGPH_INFO di una classe di entità di GEOsim.
  Parametri:
  Lista RESBUF, descrizione classe :
  <prj><cls><sub> C_DBGPH_INFO
    
  Restituisce GS_GOOD in caso di successo, GS_CAN se l'operazione è stata abortita,
  GS_BAD in caso di errore.
-*/  
/*********************************************************/
int gs_mod_dbgph_info(void)
{
   C_DBGPH_INFO DbGphInfo;
   resbuf       *arg;
   C_CLASS      *pCls;
   int          prj, cls, sub;

   acedRetNil();
   arg = acedGetArgs();
   // Legge nella lista dei parametri il progetto, classe e sottoclasse
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;
   if ((pCls = gsc_find_class(prj, cls, sub)) == NULL) return RTERROR;
   
   if (pCls->ptr_GphInfo()->getDataSourceType() != GSDBGphDataSource ||
       !((C_DBGPH_INFO *) pCls->ptr_GphInfo())->LinkedTable)
      { GS_ERR_COD = eGSInvClassType; return RTERROR; }

   if (!arg || !(arg = arg->rbnext)) return RTERROR;
   // Ricopia i dati dalla struttura di ingresso
   if (DbGphInfo.from_rb(arg) == GS_BAD) return RTERROR;
   DbGphInfo.prj = prj;
   DbGphInfo.cls = cls;
   DbGphInfo.sub = sub;

   if (DbGphInfo.isValid(pCls->get_type()) == GS_BAD) return RTERROR;
   if (DbGphInfo.to_db() == GS_BAD) return RTERROR;
   if (DbGphInfo.copy(pCls->ptr_GphInfo()) == GS_BAD)
      { GS_ERR_COD = eGSInvClassType; return RTERROR; }

   acedRetT();

   return RTNORM;
}