/**********************************************************
Name: GS_PRJCT

Module description: File contenente le funzioni per la gestione dei progetti
                    di GEOsim 
            
Author: Poltini Roberto
        Paolo De Sole

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.A.
              
Modification history:
              
Notes and restrictions on use: 

**********************************************************/


/*********************************************************/
/* INCLUDES */
/*********************************************************/


#include "stdafx.h"         // MFC core and standard components

#define INITGUID
#import "msado15.dll" no_namespace rename ("EOF", "EndOfFile") rename ("EOS", "ADOEOS")


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "adslib.h"
#include "AcExtensionModule.h" // per CAcModuleResourceOverride

#include "resource.h"

#include "gs_opcod.h"     // codici delle operazioni
#include "gs_utily.h"     // (gsc_tostring)
#include "gs_error.h"     // codici errori
#include "gs_resbf.h"     // (gsc_nth)
#include "gs_list.h"
#include "gs_sql.h"       // gestione database
#include "gs_class.h"
#include "gs_conv.h"      // conversione dati da versione di GEOsim precedente
#include "gs_sec.h"
#include "gs_user.h"      // check_op     
#include "gs_init.h"      // direttori globali
#include "gs_netw.h"      // funzioni di rete
#include "gs_prjct.h"     // prototipi funzioni extern
#include "gs_area.h"
#include "gs_lock.h"

   
/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


/*********************************************************/
/* PRIVATE FUNCTIONS */
/*********************************************************/


int gsc_getNewCode(_RecordsetPtr &pRs, int *new_code);


/*********************************************************/
/*.doc gs_current_prj <external> */
/*+
  Questa funzione LISP restituisce i dati del progetto corrente 
  in caso di successo altrimenti restituisce NIL.
  (<gs_id><name><descr><dir><key_dwg><coord><level><dwgdim>)
-*/  
/*********************************************************/
int gs_current_prj(void)
{
   C_RB_LIST      ret;
   C_INT_INT_LIST lista;
   
   acedRetNil();
   
   if (!GS_CURRENT_WRK_SESSION) return RTNORM;
   if ((ret << GS_CURRENT_WRK_SESSION->get_pPrj()->to_rb()) == NULL) return RTERROR;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gscreateprj <external> */
/*+
  Questo comando senza dcl crea un progetto di GEOsim.
  -*/  
/*********************************************************/
void gscreateprj(void)
{
   C_PROJECT *pProject;
   int       new_code, esco=0;
   C_STRING  direct, keydwg;
   TCHAR     name[MAX_LEN_PRJNAME] = GS_EMPTYSTR;
   TCHAR     syscoor[MAX_LEN_COORDNAME];
   TCHAR     buff[133];

   GEOsimAppl::CMDLIST.StartCmd();

   if (ads_getstring(TRUE, gsc_msg(138), buff) != RTNORM)
      return GEOsimAppl::CMDLIST.ErrorCmd(); // "\nNome progetto: "
   if (wcslen(buff) >= MAX_LEN_PRJNAME) { GS_ERR_COD = eGSStringTooLong; return GEOsimAppl::CMDLIST.ErrorCmd(); } 
   if (wcslen(buff) == 0) { GS_ERR_COD = eGSInvalidArg; return GEOsimAppl::CMDLIST.ErrorCmd(); } 
   gsc_strcpy(name, buff, MAX_LEN_PRJNAME);
  
   if (ads_getstring(TRUE, gsc_msg(139), buff) != RTNORM) return GEOsimAppl::CMDLIST.ErrorCmd(); // "\nDirettorio: "
   if (wcslen(buff) == 0) { GS_ERR_COD = eGSInvalidArg; return GEOsimAppl::CMDLIST.ErrorCmd(); } 
   direct = buff;

   if (ads_getstring(TRUE, gsc_msg(140), buff) != RTNORM) return GEOsimAppl::CMDLIST.ErrorCmd(); // "\nDisegno chiave: "
   keydwg = buff;
  
   while (esco!=1)
   { 
      if (ads_getstring(TRUE, gsc_msg(141), buff) != RTNORM) return GEOsimAppl::CMDLIST.ErrorCmd(); // "\nSistema coordinate: "
      if ((gsc_strcmp(buff, _T("?")) == 0) || (gsc_strcmp(buff, GS_EMPTYSTR) == 0)) // l'utente ha dato return stampo lista
      {
         if (printlistcoord()!=GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd();
      } 
      else
      {
         if (wcslen(buff) >= MAX_LEN_COORDNAME)
            { GS_ERR_COD = eGSStringTooLong; return GEOsimAppl::CMDLIST.ErrorCmd(); }
         if (gsc_toupper(buff)==GS_GOOD)         // "Sistema coordinate"
            { gsc_strcpy(syscoor, buff, MAX_LEN_COORDNAME); esco=1; }

      }
   }
   // Alloca una nuovo progetto
   if ((pProject = new C_PROJECT()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   // inizializzo i valori del progetto letti in input
   if ((pProject->set_name(name)==GS_BAD)||
      (pProject->set_dir(direct.get_name())==GS_BAD)||
      (pProject->set_keydwg(keydwg.get_name())==GS_BAD)||
      (pProject->set_coordinate(syscoor)==GS_BAD)) 
      { delete pProject; return GEOsimAppl::CMDLIST.ErrorCmd(); }

   // chiamata funzione C++ 
   if ((new_code=gsc_createprj(pProject)) == GS_BAD)
      { delete pProject; return GEOsimAppl::CMDLIST.ErrorCmd(); }
      
   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gs_createprj <external> */
/*+
  Questa funzione LISP crea un progetto di GEOsim.
  Parametri:
  Lista RESBUF 
  (<prjname><prjDescr><prjdir>[<key_dwg>[<coordsys>[<dwg_dim>[<ConnStrUDLFile>[<UDLProperties>]]]]])
  
  prjname;       nome del progetto da creare
  prjDescr;      descrizione del progetto da creare
  prjdir;        direttorio del progetto
  key_dwg;       nome del file .dwg da utilizzare come sinottico
  coordsys;      codice sistema di coordinate
    
  Restituisce il codice del nuovo progetto in caso di successo altrimenti 
  restituisce NIL.
-*/  
/*********************************************************/
int gs_createprj(void)
{
   resbuf    *arg;
   C_PROJECT *pProject;
   int        new_code;
   
   acedRetNil();
   arg = acedGetArgs();

   // Alloca una nuovo progetto
   if ((pProject = new C_PROJECT()) == NULL)
      { GS_ERR_COD = eGSOutOfMem; return RTERROR; }
   // Ricopia i dati dalla struttura di ingresso
   if (pProject->from_rb(arg) == GS_BAD) { delete pProject; return RTERROR; }

   if ((new_code = gsc_createprj(pProject)) == GS_BAD) { delete pProject; return RTERROR; }
   
   acedRetInt(new_code);
   
   return RTNORM;
}


/*********************************************************/
/*.doc gs_ClassesStructReport                 <external> */
/*+
  Questa funzione LISP esporta su file testo la struttura di una lista
  di classi di un progetto di GEOsim.
  Parametri:
  Lista RESBUF (<Path><prj><open_file>(<cls1><cls2>...))
      
  Restituisce il TRUE in caso di successo altrimenti 
  restituisce NIL.
-*/  
/*********************************************************/
int gs_ClassesStructReport(void)
{
   presbuf    arg = acedGetArgs();
   int        prjcode;
   C_PROJECT  *pProject;
   C_INT_LIST cls_list;
   TCHAR      *Path, *open_file;
   bool       SynthMode = false;
   
   acedRetNil();

   // Path
   if (!arg || arg->restype != RTSTR) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   Path = arg->resval.rstring;

   // codice del progetto
   if (!(arg = arg->rbnext) || arg->restype != RTSHORT)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   prjcode = arg->resval.rint;

   // modalità di apertura file
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((open_file = arg->resval.rstring) == GS_BAD) return RTERROR;

   // Lista classi
   if (!(arg = arg->rbnext))
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (cls_list.from_rb(arg) == GS_BAD) return RTERROR;
   
   // opzionale, flag di report sintetico
   if ((arg = gsc_scorri(arg)) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if ((arg = arg->rbnext) && arg->restype == RTT) SynthMode = true;

   // cerco elemento in lista progetti
   if ((pProject =(C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prjcode)) == NULL) return RTERROR;
   
   // reindicizzo le classi del progetto
   if (pProject->ClassesStructReport(Path, open_file, cls_list, SynthMode) == GS_BAD)
      return RTERROR;
   
   acedRetT();
   
   return RTNORM;
}


/*********************************************************/
/*.doc gs_checkclass <external> */
/*+
  Questa funzione LISP controlla le classi di un progetto di GEOsim.
  Parametri:
  Lista RESBUF (<prj>, (<cls1><cls2>...) [DwgOpType [Path di file script]])
  
  RTSHORT prj;                codice del progetto
  RTSHORT cls1, cls2, ... ;   classi da reindicizzare
  DwgOpType;                  1=reindex, 2=sblocco, 4=estensioni, 8=purge
    
  Restituisce il TRUE in caso di successo altrimenti 
  restituisce NIL.
-*/  
/*********************************************************/
int gs_checkclass(void)
{
   presbuf    arg;
   int        prj;
   C_PROJECT  *pProject;
   C_INT_LIST cls_list;
   
   acedRetNil();
   arg = acedGetArgs();

   // codice del progetto
   if (!arg || gsc_rb2Int(arg, &prj) != GS_GOOD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if ((arg = arg->rbnext) == NULL) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (cls_list.from_rb(arg) == GS_BAD) return RTERROR;
   
   // cerco elemento in lista progetti
   if ((pProject =(C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL) return RTERROR;

   if (arg->restype == RTLB) arg = gsc_scorri(arg);

   if (arg && (arg = arg->rbnext) && arg->restype == RTSHORT)
   {
      int   DwgOpType = arg->resval.rint;
      TCHAR *ScriptFile = NULL;

      if ((arg = arg->rbnext) && arg->restype == RTSTR) ScriptFile = arg->resval.rstring;
      // reindicizzo le classi del progetto con tipo operazione per DWG
      if (pProject->ClassCheck(&cls_list, DwgOpType, ScriptFile) == GS_BAD) return RTERROR;
   }
   else   
      // reindicizzo le classi del progetto
      if (pProject->ClassCheck(&cls_list) == GS_BAD) return RTERROR;
   
   acedRetT();
   
   return RTNORM;
}


////////////////////////////////////////////////////////////////////////////////
//    class C_PROJECT                                                         //      
////////////////////////////////////////////////////////////////////////////////


// costruttore
C_PROJECT::C_PROJECT() : C_NODE() 
{ 
   gs_id   = 0;
   name[0] = _T('\0');
   wcscpy(coordinate, DEFAULT_DWGCOORD);
   level   = GSInvisibleData;
   dwg_dim = DEFAULT_DWG_DIM;
   classlist.set_father_prj(this);
   pConn = NULL;
   isRsSelectLockSupported = -1; // non ancora inizializzato
}

// costruttore
C_PROJECT::C_PROJECT(const TCHAR *nameprj, const TCHAR *descrprj, const TCHAR *dirprj, const TCHAR *keydwgprj,
                     const TCHAR *coordinateprj, int dwgdim) : C_NODE()
{
   set_name(nameprj);
   set_descr(descrprj);
   set_dir(dirprj);
   set_keydwg(keydwgprj);
   set_coordinate(coordinateprj);
   set_dwg_dim(dwgdim);
   return; 
}

// distruttore
C_PROJECT::~C_PROJECT() 
{ 
   classlist.remove_all(); 
   TerminateSQL();
}

/*********************************************************/
/*.doc C_PROJECT::get_key e set_key           <external> */
/*+
  Questa funzioni leggono e inizializzano il codice del progetto (non agisce sui database)
    
  C_PROJECT::get_key restituisce il codice del progetto.
  C_PROJECT::set_key restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::get_key(void)        
   { return gs_id; }
int C_PROJECT::set_key(int in) 
{ 
   if (in <= 0) return GS_BAD;
   gs_id = in;
   return GS_GOOD; 
}


/*********************************************************/
/*.doc C_PROJECT::get_name e set_name         <external> */
/*+
  Questa funzioni leggono e inizializzano il nome del progetto (non agisce sui database)
    
  C_PROJECT::get_name restituisce il nome del progetto.
  C_PROJECT::set_name restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
TCHAR *C_PROJECT::get_name(void)       
   { return name; }
int C_PROJECT::set_name(const TCHAR *nameprj)
{
   if (!nameprj || wcslen(nameprj) == 0) 
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (wcslen(nameprj) >= MAX_LEN_PRJNAME)
      { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }
   wcscpy(name, nameprj);

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::get_descr e set_descr       <external> */
/*+
  Questa funzioni leggono e inizializzano la descrizione del progetto (non agisce sui database)
    
  C_PROJECT::get_descr restituisce la descrizione del progetto.
  C_PROJECT::set_descr restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
TCHAR *C_PROJECT::get_descr(void)       
   { return Descr.get_name(); }
int C_PROJECT::set_descr(const TCHAR *descrprj)
{
   Descr = descrprj;
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::get_dir e set_dir           <external> */
/*+
  Questa funzioni leggono e inizializzano il direttorio del progetto (non agisce sui database)
    
  C_PROJECT::get_dir restituisce il nome del progetto.
  C_PROJECT::set_dir restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
TCHAR *C_PROJECT::get_dir(void)        
   { return dir.get_name(); }
int C_PROJECT::set_dir(const TCHAR *dirprj)
{
   if (!dirprj) { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   dir = dirprj;
   dir.alltrim();
   if (gsc_validdir(dir) == GS_BAD) 
      { dir.clear(); return GS_BAD; }
   else return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::get_keydwg e set_keydwg     <external> */
/*+
  Questa funzioni leggono e inizializzano il disegno chiave 
  del progetto (non agisce sui database)
    
  C_PROJECT::get_keydwg restituisce il disegno chiave del progetto.
  C_PROJECT::set_keydwg restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
TCHAR *C_PROJECT::get_keydwg(void)     
   { return keydwg.get_name(); }
int C_PROJECT::set_keydwg(const TCHAR *keydwgprj)
{
   if (!keydwgprj) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   keydwg = keydwgprj;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::get_coordinate e set_coordinate <external> */
/*+
  Questa funzioni leggono e inizializzano il sistema di coordinate
  del progetto (non agisce sui database)
    
  C_PROJECT::get_coordinate restituisce il sistema di coordinate del progetto.
  C_PROJECT::set_coordinate restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
TCHAR *C_PROJECT::get_coordinate(void) 
   { return coordinate; }
int C_PROJECT::set_coordinate(const TCHAR *coordinateprj)
{
   if (gsc_validcoord(coordinateprj) == GS_BAD) return GS_BAD;
   
   gsc_strcpy(coordinate, coordinateprj, MAX_LEN_COORDNAME);
   gsc_alltrim(coordinate);
   gsc_toupper(coordinate);

   return GS_GOOD;
}


GSDataPermissionTypeEnum C_PROJECT::get_level(void)      
   { return level; }


/*********************************************************/
/*.doc C_PROJECT::get_dwg_dim e set_dwg_dim <external> */
/*+
  Questa funzioni leggono e inizializzano la dimensione massima dei dwg
  (MByte) delle classi del progetto (non agisce sui database)
    
  C_PROJECT::get_dwg_dim restituisce la dimensione massima dei dwg.
  C_PROJECT::set_dwg_dim restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::get_dwg_dim(void)    
   { return dwg_dim; }
int C_PROJECT::set_dwg_dim(int in) 
{ 
   if (in <= 0) return GS_BAD;
   dwg_dim = in;
   return GS_GOOD; 
}


/*********************************************************/
/*.doc C_PROJECT::get_ConnStrUDLFile e set_ConnStrUDLFile <external> */
/*+
  Questa funzioni leggono e inizializzano il tipo di DB usato per le tabelle 
  interne del progetto e proposto come default quando si creano nuove classi
  (non agisce sui database)
    
  C_PROJECT::get_ConnStrUDLFile restituisce il sistema di coordinate del progetto.
  C_PROJECT::set_ConnStrUDLFile restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
const TCHAR *C_PROJECT::get_ConnStrUDLFile(void) 
   { return ConnStrUDLFile.get_name(); }
int C_PROJECT::set_ConnStrUDLFile(const TCHAR *in)
{
   ConnStrUDLFile = in;
   ConnStrUDLFile.alltrim();
   if (gsc_path_exist(ConnStrUDLFile) == GS_GOOD)
      // se si tratta di un file e NON di una stringa di connessione
      gsc_nethost2drive(ConnStrUDLFile); // lo converto

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::get_UDLProperties e set_UDLProperties <external> */
/*+
  Questa funzioni leggono e inizializzano il tipo di DB usato per le tabelle 
  interne del progetto e proposto come default quando si creano nuove classi
  (non agisce sui database)
    
  C_PROJECT::ptr_UDLProperties restituisce le proprietà UDL di connessione al DB.
  C_PROJECT::set_UDLProperties restituisce GS_GOOD in caso di successo 
  (il puntatore <arg> viene avanzato nella lista) altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
C_2STR_LIST *C_PROJECT::ptr_UDLProperties(void)
   { return &UDLProperties; }
int C_PROJECT::set_UDLProperties(presbuf *arg)
   { return gsc_getUDLProperties(arg, UDLProperties); }
int C_PROJECT::set_UDLProperties(C_2STR_LIST &_UDLProperties)
   { return _UDLProperties.copy(UDLProperties); }


C_CLASS_LIST *C_PROJECT::ptr_classlist(void) 
   { return &classlist; }



/*********************************************************/
/*.doc C_PROJECT::get_Bitmap                 <external> */
/*+
  Funzione che restituisce la bitmap del progetto.
  Parametri:
  bool LargeSize;    Se vero la dimensione della bitmap sarà di 32x16
                     altrimenti sarà di 16x16
  CBitmap &CBMP;     Oggetto bitmap (output)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::get_Bitmap(bool LargeSize, CBitmap &CBMP)
{
   UINT nIDResource = (LargeSize) ? IDB_PROJECT_32X16 : IDB_PROJECT_16X16;
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
/*.doc C_PROJECT::getDBConnection             <external> */
/*+
  Questa funzione restituisce il puntatore alla connessione OLE-DB
  per il progetto.
    
  Restituisce GS-GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
C_DBCONNECTION* C_PROJECT::getDBConnection()
{
   if (!pConn)
   {
      // se non è impostato alcun collegamento
      if (ConnStrUDLFile.len() == 0 && UDLProperties.get_count() == 0)
      {  // Imposto il DB ACCESS
         C_STRING DataSource;

         DataSource = dir;
         DataSource += _T('\\');
         DataSource += ACCESSGEOPRJDB;

         UDLProperties.add_tail_2str(_T("Data Source"), DataSource.get_name());
         ConnStrUDLFile = GEOSIM_DATASOURCE;
      }

      pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ConnStrUDLFile.get_name(), &UDLProperties);
   }

   return pConn;
}


/*********************************************************/
/*.doc C_PROJECT::get_CmdSelectLockWhereKey              */
/*+
  Questa funzione restituisce il comando SQL per la ricerca di lock
  di una entità di una classe.
-*/  
/*********************************************************/
_CommandPtr C_PROJECT::get_CmdSelectLockWhereKey(void)
{
   if (CmdSelectLockWhereKey.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pDBConn;
      C_STRING       TableRef;

      if (getLocksTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;
      if (gsc_PrepareSelLockWhereKey(pDBConn, TableRef,
                                     CmdSelectLockWhereKey) == GS_BAD) return NULL;
   }

   return CmdSelectLockWhereKey;
}


/*********************************************************/
/*.doc C_PROJECT::get_RsSelectLock                       */
/*+
  Questa funzione restituisce un recordset set per la ricerca dei lock.
-*/  
/*********************************************************/
_RecordsetPtr C_PROJECT::get_RsSelectLock(void)
{
   if (isRsSelectLockSupported == -1) // non ancora inizializzato
   {
      if (RsSelectLock.GetInterfacePtr() == NULL)
      {
         C_DBCONNECTION *pDBConn;
         C_STRING       TableRef;
         bool           RsOpened = false;

         if (getLocksTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;

         // 1) il metodo seek è attualmente supportato solo da JET
         // 2) se si prova ad aprire un recordset con la modalità adCmdTableDirect
         //    mentre si è in una transazione questa viene automaticamente abortita
         //    con PostgreSQL
         if (gsc_strcmp(pDBConn->get_DBMSName(), _T("MS JET"), FALSE) != 0)
         {
            // Mi segno che non supporta il metodo seek per non riprovare più
            isRsSelectLockSupported = 0;
            return NULL;
         }

         // rimuove se esiste il prefisso e il suffisso (es. per ACCESS = [ ])
         TableRef.removePrefixSuffix(pDBConn->get_InitQuotedIdentifier(), pDBConn->get_FinalQuotedIdentifier());
      
         // istanzio un recordset
         if (FAILED(RsSelectLock.CreateInstance(__uuidof(Recordset))))
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

	      try
         {  
            RsSelectLock->CursorLocation = adUseServer;
            // apro il recordset 
            RsSelectLock->Open(TableRef.get_name(), pDBConn->get_Connection().GetInterfacePtr(),
                               adOpenDynamic, adLockPessimistic, adCmdTableDirect);
            RsOpened = true;

            if (RsSelectLock->Supports(adIndex) && RsSelectLock->Supports(adSeek))
               isRsSelectLockSupported = 1; // supportato
            else
            {
               isRsSelectLockSupported = 0; // non supportato
               gsc_DBCloseRs(RsSelectLock);
            }
         }

	      catch (_com_error)
	      {
            isRsSelectLockSupported = 0; // non supportato
            if (RsOpened) gsc_DBCloseRs(RsSelectLock);
            else RsSelectLock.Release();
         }
      }
   }
   else // già inizializzato
      if (isRsSelectLockSupported == 1) // se era supportato il recordset
         // e per qualche motivo il recordset risulta chiuso
         // (per esempio chiambiando indice su una tabella... baco microsoft)
         if (gsc_DBIsClosedRs(RsSelectLock) == GS_GOOD)
         {
            isRsSelectLockSupported = 0;
            try
            {
               RsSelectLock.Release();
            }
	         catch (_com_error)
	         {}
         }

   return (isRsSelectLockSupported == 1) ? RsSelectLock : NULL;
}


/*********************************************************/
/*.doc C_PROJECT::get_RsInsertLock                       */
/*+
  Questa funzione restituisce un recordset per l'inserimento dei locks.
-*/  
/*********************************************************/
_RecordsetPtr C_PROJECT::get_RsInsertLock(void)
{
   if (RsInsertLock.GetInterfacePtr() == NULL)
   {
      C_DBCONNECTION *pDBConn;
      C_STRING       TableRef;
   
      // istanzio un recordset
      if (FAILED(RsInsertLock.CreateInstance(__uuidof(Recordset))))
         { GS_ERR_COD = eGSOutOfMem; return NULL; }

      if (getLocksTabInfo(&pDBConn, &TableRef) == GS_BAD) return NULL;

      if (pDBConn->InitInsRow(TableRef.get_name(), RsInsertLock) == GS_BAD) return NULL;
   }

   return RsInsertLock;
}


/*********************************************************/
/*.doc C_PROJECT::TerminateSQL                <internal> */
/*+
  Questa funzione termina i recordset e i comandi eventualmente 
  inizializzati e pone la connessione OLE-DB nulla.
  Parametri:
  C_DBCONNECTION *pConnToTerminate; Opzionale; connessione da terminare,
                                    se non passata termina tutte le connessioni
                                    (default = NULL)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void C_PROJECT::TerminateSQL(C_DBCONNECTION *pConnToTerminate)
{
   bool Terminate = true;

   if (pConnToTerminate && pConnToTerminate != pConn) Terminate = false;
   if (Terminate)
   {
      if (RsSelectLock.GetInterfacePtr())
         { RsSelectLock->Close(); RsSelectLock.Release(); }
      if (RsInsertLock.GetInterfacePtr())
         { RsInsertLock->Close(); RsInsertLock.Release(); }

      if (CmdSelectLockWhereKey.GetInterfacePtr()) CmdSelectLockWhereKey.Release();
      if (CmdSelectLockWhereStatusFree.GetInterfacePtr()) CmdSelectLockWhereStatusFree.Release();

      pConn = NULL;
   }
}


/*********************************************************/
/*.doc C_PROJECT::from_rb <external> */
/*+
  Questa funzione riempie i valori di un progetto da un resbuf
  Lista RESBUF
  (<name><dir>[<key_dwg>[<coordsys>[<dwg_dim>[(<DBConnection>)]]]])
  dove:
  <DBConnection>  = (<UDL reference><UDL Props>)
  <UDL reference> = UDL file | UDL connection string
  <UDL Props>     = ((<Prop Name1><Value1>)(<Prop Name2><Value2>)...) | nil

  Restituisce GS-GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::from_rb(resbuf *rb)
{
   if (rb == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   // name
   if (rb->restype == RTSTR)
   {
      if (wcslen(rb->resval.rstring) >= MAX_LEN_PRJNAME)
         { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }
      else wcscpy(name, rb->resval.rstring);
   }
   else { GS_ERR_COD=eGSInvRBType; return GS_BAD; }

   // descr
   if ((rb = rb->rbnext) == NULL) return GS_BAD;
   if (rb->restype == RTSTR) Descr = rb->resval.rstring;
   else Descr.clear();

   // dir
   if ((rb = rb->rbnext) == NULL) return GS_BAD;
   if (rb->restype == RTSTR)
   {
      if (set_dir(rb->resval.rstring) == GS_BAD) return GS_BAD;
   }
   else { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   // keydwg (opzionale)
   if ((rb = rb->rbnext) == NULL) return GS_GOOD;
   if (rb->restype == RTSTR)
   {
      if (set_keydwg(rb->resval.rstring) == GS_BAD) return GS_BAD;
   }
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   // coordinate (opzionale)
   if ((rb = rb->rbnext) == NULL) return GS_GOOD;
   if (rb->restype == RTSTR)
   {
      if (wcslen(rb->resval.rstring) >= MAX_LEN_COORDNAME)
         { GS_ERR_COD = eGSStringTooLong; return GS_BAD; }
      else wcscpy(coordinate, rb->resval.rstring);
   }
   else if (rb->restype != RTNIL) { GS_ERR_COD = eGSInvRBType; return GS_BAD; }

   // dwg_dim divisione territorio (opzionale)
   if ((rb = rb->rbnext) == NULL) return GS_GOOD;
   gsc_rb2Int(rb, &dwg_dim);

   // DBConnection delle tabelle interne (opzionale)
   if ((rb = rb->rbnext) == NULL) return GS_GOOD;
   if (rb->restype != RTLB)
   {
      ConnStrUDLFile.clear();
      UDLProperties.remove_all();
   }
   else
   {
      C_INFO info;

      if (info.from_rb(rb) == NULL) return GS_BAD;
      ConnStrUDLFile = info.ConnStrUDLFile;
      info.UDLProperties.copy(UDLProperties);
   }

   return GS_GOOD;
} 


/*********************************************************/
/*.doc C_PROJECT::is_valid                    <external> */
/*+
  Questa funzione controlla i valori di un progetto.
    
  Restituisce GS-GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::is_valid()
{
   // verifico validità name
   if (wcslen(name) == 0) { GS_ERR_COD = eGSInvalidPrjName; return GS_BAD; }
   // verifico validità dir
   if (gsc_validdir(dir) == NULL) return GS_BAD;
   if (keydwg.len() > 0)
      // verifico validità keydwg (funziona anche se keydwg è dir + file)
      if (gsc_validdirdwg(keydwg) == NULL) return GS_BAD;
   // verifico validità coord
   if (gsc_validcoord(coordinate) == GS_BAD) return GS_BAD;
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_ReindexMainTab                      <external> */
/*+
  Questa funzione LISP reindicizza le tabelle principali di GEOsim e
  dei progetti.
  Parametri:
  [prj]  opzionale; se viene passato il codice di un progetto 
         verrà reindicizzato solo quel progetto
    
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_reindexmaintab(void)
{  
   presbuf arg = acedGetArgs();
   int     prj = 0;

   acedRetNil();

   // codice del progetto
   if (arg)
      if (gsc_rb2Int(arg, &prj) != GS_GOOD)
         { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   if (gsc_ReindexMainTab(prj) == GS_BAD) return RTERROR;

   acedRetT();

   return RTNORM;
}
                                       
                         
/*********************************************************/
/*.doc gsc_ReindexMainTab <external> */
/*+
  Questa funzione reindicizza le tabelle principali di GEOsim e
  dei progetti.
  Parametri:
  int prj;     codice progetto da reindicizzare (opzionale; se = 0 
               reindicizza tutti i progetti)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_ReindexMainTab(int prj)
{  
   C_STRING       TableRef, Index;
   C_DBCONNECTION *pConn;
   C_PROJECT      *punt;
   int            result = GS_GOOD;

   if (gsc_getPrjTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // (se fallisce devo comunque continuare)

   // aggiorno gli indici di GS_PRJ
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   // memorizzo gli indici utilizzati dalla tabella originale
   pConn->Reindex(TableRef.get_name());

   punt = (C_PROJECT *) GEOsimAppl::PROJECTS.get_head();
   while (punt)
   { 
      if (prj == 0 || punt->get_key() == prj)
         if (punt->reindex() == GS_BAD) result = GS_BAD;

      punt = (C_PROJECT *) GEOsimAppl::PROJECTS.get_next();
   }                             
   
   return result;
}   
                                       
                         
/*********************************************************/
/*.doc gsc_getPRJSampleTables                 <internal> */
/*+
  Questa funzione legge la lista delle tabelle campione usate
  come tabelle interne (meta-tabelle) di un progetto GEOsim.
  Parametri:
  C_STR_LIST &TableList;  Lista delle tabelle
  C_DBCONNECTION **pConn; Opzionale, connessione al DB campione
                          (default = NULL)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getPRJSampleTables(C_STR_LIST &TableList, C_DBCONNECTION **pConn)
{
   C_STRING       DataSource;
   C_2STR_LIST    UDLProps;
   C_DBCONNECTION *pMyConn;

   DataSource = GEOsimAppl::GEODIR + _T('\\') + GEOSAMPLEDIR + _T('\\') + ACCESSGEOPRJSAMPLEDB;
   UDLProps.add_tail_2str(_T("Data Source"), DataSource.get_name());

   // Leggo la lista delle tabelle dal database "campione" ACCESS
   if ((pMyConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(ACCESS_DATASOURCE, &UDLProps)) == NULL)
      return GS_BAD;
   if (pMyConn->getTableAndViewList(TableList, NULL, NULL, NULL,
                                    true, _T("TABLE"), false) == GS_BAD)
      return GS_BAD;

   if (pConn) *pConn = pMyConn;

   return GS_GOOD;
}                                       
 

/*********************************************************/
/*.doc gsc_createprj <external> */
/*+
  Questa funzione crea un progetto di GEOsim.
  Parametri:
  C_PROJECT *pProject;  Progetto campione
    
  Restituisce codice progetto in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_createprj(C_PROJECT *pProject)
{  
   int new_code;

   if ((new_code = pProject->create()) == GS_BAD) return GS_BAD; // creaz.
      
   // inserimento in lista
   if ((GEOsimAppl::PROJECTS.add_tail(pProject)) == GS_BAD) return GS_BAD;

   // ordino per nome progetto con sensitive = FALSE
   GEOsimAppl::PROJECTS.sort_name();
   
   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(new_code);

   return new_code;
}   

  
/*********************************************************
/*.doc C_PROJECT::create <external> */
/*+
  Questa funzione crea un progetto di GEOsim.
    
  La protezione di questa operazione è garantita perchè la procedura
  controlla che non ci sia un altro progetto nello stesso direttorio e, 
  per quanto riguarda la multiutenza, è stato usato un indice con modalità
  UNIQUE sul campo GS_ID e sul campo NAME della tabella GS_PRJ.
  Questo garantisce che, se due utenti creano un progetto nello stesso momento,
  potrebbe essere che entrambi abbiano lo stesso nuovo codice (GS_ID)
  ma solo uno dei due potrà inserirlo, l'altro ritenterà con un nuovo codice.
  Se invece, mentre viene inserito un progetto, un altro utente aggiorna 
  un altro progetto aggiornandone il nome,
  questo potrebbe entrare in conflitto con quello da inserire. Il programma
  dopo aver ritentato l'inserimento per GS_NUM_TEST volte, termina con un
  errore.

  Restituisce codice progetto in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::create()
{                               
   C_STRING       Path, DataSource, Catalog, Schema, Table, NewTableRef;
   int            new_code, result = GS_BAD;
   C_DBCONNECTION *pSourceConn;
   C_2STR_LIST    UDLProps;
   C_STR_LIST     TableList;
   C_STR          *pTable;

   // verifico abilitazione
   if (gsc_check_op(opCreatePrj) == GS_BAD) return GS_BAD;

   // verifico correttezza valori
   if (is_valid() == GS_BAD) return GS_BAD;

   // verifico se esisteva già il direttorio progetto 
   if (gsc_path_exist(dir) == GS_GOOD)
      { GS_ERR_COD = eGSPathAlreadyExisting; return GS_BAD; }

   // Leggo la tabelle campione e la connessione OLEDB al DB campione
   if (gsc_getPRJSampleTables(TableList, &pSourceConn) == GS_BAD) return GS_BAD;

   int IsTransactionSupported = getDBConnection()->BeginTrans();

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
         if (NewTableRef.paste(getDBConnection()->get_FullRefTable(GEOPRJCATALOG, NULL, Table.get_name())) == NULL)
            { result = GS_BAD; break; }

         if (pSourceConn->CopyTable(pTable->get_name(), 
                                    *getDBConnection(), NewTableRef.get_name(),
                                    GSStructureOnlyCopyType) == GS_BAD)
            { result = GS_BAD; break; }

         pTable = (C_STR *) TableList.get_next();
      }
      if (result == GS_BAD) break;
      result = GS_BAD;

      if (gsc_mkdir(dir) == GS_BAD) break;

      // creo sottodirettorio GEOUSRDIR al direttorio del progetto
      Path = dir;
      Path += _T('\\');
      Path += GEOUSRDIR;
      if (gsc_mkdir(Path) == GS_BAD) break;
      // creo sottodirettorio GEOQRYDIR al direttorio del progetto
      Path = dir;
      Path += _T('\\');
      Path += GEOQRYDIR;
      if (gsc_mkdir(Path) == GS_BAD) break;   
      // creo sottodirettorio GEOVISDIR al direttorio del progetto
      Path = dir;
      Path += _T('\\');
      Path += GEOVISDIR;
      if (gsc_mkdir(Path) == GS_BAD) break;
      // creo sottodirettorio GEOSELCLSDIR al direttorio del progetto
      Path = dir;
      Path += _T('\\');
      Path += GEOSELCLSDIR;
      if (gsc_mkdir(Path) == GS_BAD) break;
      // creo sottodirettorio GEOSUPPORTDIR al direttorio del progetto
      Path = dir;
      Path += _T('\\');
      Path += GEOSUPPORTDIR;
      if (gsc_mkdir(Path) == GS_BAD) break;

      DataSource = GEOsimAppl::GEODIR;
      DataSource += _T('\\');
      DataSource += ACCESSGEOMAINDB;
      UDLProps.add_tail_2str(_T("Data Source"), DataSource.get_name());
      if ((pSourceConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &UDLProps)) == NULL)
         break;

      // inserisco nuovo progetto in GS_PRJ
      if ((new_code = ins_prj_to_gs_prj(*pSourceConn)) == GS_BAD) break;
      gs_id = new_code;
      level = GSUpdateableData;

      result = GS_GOOD;
   }
   while (0);

   if (result == GS_BAD)
   {
      int error_code = GS_ERR_COD;

      if (IsTransactionSupported == GS_GOOD) getDBConnection()->RollbackTrans();
      gsc_delall(dir.get_name(), RECURSIVE);
      GS_ERR_COD = error_code;
      return GS_BAD;
   }

   if (IsTransactionSupported == GS_GOOD) getDBConnection()->CommitTrans();

   return new_code;
}


/*********************************************************/
/*.doc gs_rprj                                <external> */
/*+
  Questa funzione LISP restituisce l'elenco dei progetti visibili
  all'utente corrente.
-*/  
/*********************************************************/
int gs_rprj(void)
{
   C_RB_LIST ret;

   acedRetNil();

   if ((ret << GEOsimAppl::PROJECTS.to_rb()) == NULL) return RTNORM;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}


/*********************************************************
/*.doc C_PROJECT::to_rb <external> */
/*+
  Questa funzione trasforma in resbuf un oggetto progetto secondo la forma:
  (<gs_id><name><descr><dir><key_dwg><coord><level><dwgdim>)

  Restituisce la lista resbuf in caso di successo altrimenti restituisce
  NULL.
  N.B.: Alloca memoria !!!
-*/  
/*********************************************************/
resbuf *C_PROJECT::to_rb(void)
{
   C_RB_LIST List;
   TCHAR     str[1] = GS_EMPTYSTR;
   TCHAR     *prjname, *prjDescr, *prjdir, *key_dwg, *coordsys;

   if ((prjname  = get_name()) == NULL) prjname = str;
   if ((prjDescr  = get_descr()) == NULL) prjDescr = str;
   if ((prjdir   = get_dir()) == NULL) prjdir = str;
   if ((key_dwg  = get_keydwg()) == NULL) key_dwg = str;
   if ((coordsys = get_coordinate()) == NULL) coordsys = str;

   if ((List << acutBuildList(RTSHORT, gs_id,
                              RTSTR,   prjname,
                              RTSTR,   prjDescr,
                              RTSTR,   prjdir,
                              RTSTR,   key_dwg,
                              RTSTR,   coordsys,
                              RTSHORT, level,
                              RTSHORT, dwg_dim, 0)) == NULL)
      return GS_BAD;

   if ((List += gsc_str2rb(ConnStrUDLFile)) == NULL) return NULL;
   
   if (UDLProperties.get_count() == 0)
      { if ((List += acutBuildList(RTNIL, 0)) == NULL) return NULL; }
   else
   {
      presbuf p = UDLProperties.to_rb();
      if ((List += gsc_lspforrb(&p)) == NULL) return NULL;
   }

   List.ReleaseAllAtDistruction(GS_BAD);
   return List.get_head();
}


/*********************************************************/
/*.doc ins_prj_to_gs_prj <internal> */
/*+
  Questa funzione inserisce un nuovo progetto in GS_PRJ.
  Parametri:
  C_DBCONNECTION &pConn;   Connessione OLE-DB (la funzione potrebbe
                           ricavarselo da sola ma serve alle funzioni chiamanti
                           per gestire il RollBack)

  Restituisce codice progetto in caso di successo altrimenti restituisce 0. 
-*/  
/*********************************************************/
int C_PROJECT::ins_prj_to_gs_prj(C_DBCONNECTION &_pConn)
{
   C_STRING      abs_dir, abs_keydwg, TableRef, Catalog, Buffer;
   int           new_code, n_test = 0;
   C_RB_LIST     ColValues;
   presbuf       pRb;
   _RecordsetPtr pInsRs;

   Catalog = GEOsimAppl::GEODIR;
   Catalog += _T('\\');
   Catalog += ACCESSGEOMAINDB;
   TableRef.paste(_pConn.get_FullRefTable(Catalog.get_name(), NULL, PRJS_TABLE_NAME));

   // traduco dir relativo in dir assoluto
   abs_dir = dir;
   if (gsc_drive2nethost(abs_dir) == GS_BAD) return 0;
   if (abs_dir.len() > ACCESS_MAX_LEN_FIELDCHAR)
      { GS_ERR_COD = eGSStringTooLong; return 0; }

   abs_keydwg = keydwg;
   if (gsc_drive2nethost(abs_keydwg) == GS_BAD) return 0;
   if (abs_keydwg.len() > ACCESS_MAX_LEN_FIELDCHAR)
      { GS_ERR_COD = eGSStringTooLong; return 0; }

   // inserisco nuova riga in GS_PRJ
   if ((ColValues << acutBuildList(RTLB, 
                                   RTLB, RTSTR, _T("NAME"), RTSTR, name, RTLE,
                                   RTLB, RTSTR, _T("DESCRIPTION"), RTSTR, (Descr.get_name()) ? Descr.get_name() : GS_EMPTYSTR, RTLE,
                                   RTLB, RTSTR, _T("DIR"), 0)) == NULL)
      return 0;
   if ((ColValues += gsc_str2rb(abs_dir)) == NULL) return 0;
   if ((ColValues += acutBuildList(RTLE, RTLB, RTSTR, _T("KEYDWG"), 0)) == NULL) return 0;
   if ((ColValues += gsc_str2rb(abs_keydwg)) == NULL) return 0;
   if ((ColValues += acutBuildList(RTLE, RTLB, RTSTR, _T("COORDINATE"), 0)) == NULL) return 0;
   if ((ColValues += gsc_str2rb(coordinate)) == NULL) return 0;
   if ((ColValues += acutBuildList(RTLE, RTLB, RTSTR, _T("DWG_DIM"), RTSHORT, dwg_dim, RTLE,
                                   RTLB, RTSTR, _T("VERSION"), RTSTR, gsc_msg(130), RTLE, 0)) == NULL) // Versione attuale di GEOsim
      return 0;
   if ((ColValues += acutBuildList(RTLB, RTSTR, _T("UDL_FILE"), 0)) == NULL) return GS_BAD;
   if ((ColValues += gsc_str2rb(ConnStrUDLFile)) == NULL) return GS_BAD;
   if ((ColValues += acutBuildList(RTLE, RTLB, RTSTR, _T("UDL_PROP"), 0)) == NULL) return GS_BAD;
   Buffer.paste(gsc_PropListToConnStr(UDLProperties));
   if (Buffer.len() > 0)
      // Conversione path UDLProperties da dir relativo in assoluto
      if (_pConn.UDLProperties_drive2nethost(Buffer) == GS_BAD) return GS_BAD;

   if ((ColValues += gsc_str2rb(Buffer)) == NULL) return GS_BAD;
   if ((ColValues += acutBuildList(RTLE, RTLE, 0)) == NULL) return GS_BAD;

   if (_pConn.InitInsRow(TableRef.get_name(), pInsRs) == GS_BAD) return 0;
   if (gsc_DBInsRow(pInsRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pInsRs); return 0; }
   // Rileggo il codice del progetto che è stato assegnato da un campo contatore
   if (gsc_DBReadRow(pInsRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pInsRs); return 0; }
   if (gsc_DBCloseRs(pInsRs) == GS_BAD) return 0;
   if ((pRb = ColValues.CdrAssoc(_T("GS_ID"))) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return 0; }
   if (gsc_rb2Int(pRb, &new_code) == GS_BAD) new_code = 0;
   
   return new_code;
}   


/*********************************************************
/*.doc (new 2) printlistproject <internal> */
/*+
  Questa funzione stampa la lista dei progetti di GEOsim

  Restituisce GS_GOOD se tutto va bene.
-*/  
/*********************************************************/
int printlistproject(void)
{
   C_PROJECT *punt;

   acutPrintf(_T("%s"), gsc_msg(148));        // \nLista progetti:
   punt = (C_PROJECT *) GEOsimAppl::PROJECTS.get_head();
   while (punt != NULL)
   { 
      acutPrintf(_T("\n  %s"), punt->get_name());
      punt = (C_PROJECT *) GEOsimAppl::PROJECTS.get_next();
   }                             

   return GS_GOOD;      
}


/*********************************************************
/*.doc C_LIST_PRJ::to_rb <external> */
/*+
  Questa funzione trasforma in resbuf la lista GEOsimAppl::PROJECTS dei progetti
  visibili all'utente.

  Restituisce la lista resbuf in caso di successo altrimenti restituisce
  NULL.
-*/  
/*********************************************************/
presbuf C_LIST_PRJ::to_rb(void)
{
   C_RB_LIST list;
   C_PROJECT *punt;

   punt = (C_PROJECT*) get_head();
   while (punt != NULL)
   {  
      if ((list += acutBuildList(RTLB, 0)) == NULL) return NULL;
      if ((list += punt->to_rb()) == NULL) return NULL;
      if ((list += acutBuildList(RTLE, 0)) == NULL) return NULL;

      punt = (C_PROJECT*) get_next();
   }
   
   list.ReleaseAllAtDistruction(GS_BAD);

   return list.get_head();      
}


/*********************************************************/
/*.doc gsdelprj                               <external> */
/*+
  Questa comando senza dcl cancella un progetto di GEOsim.
    -*/  
/*********************************************************/
void gsdelprj(void)
{ 
   C_PROJECT *pProject;
   TCHAR name[MAX_LEN_PRJNAME] = GS_EMPTYSTR, Password[128];
   TCHAR buff[133];
   int   ris;

   GEOsimAppl::CMDLIST.StartCmd();

   if (ads_getstring(TRUE, gsc_msg(147), buff) != RTNORM)
      return GEOsimAppl::CMDLIST.ErrorCmd(); // "\nProgetto <?>: "
   if ((gsc_strcmp(buff, _T("?")) == 0) || (gsc_strcmp(buff, GS_EMPTYSTR) == 0))
   {
      if (printlistproject()!=GS_GOOD) return GEOsimAppl::CMDLIST.ErrorCmd(); 
   }
   else 
   {
      if((pProject=(C_PROJECT *) GEOsimAppl::PROJECTS.search_name(buff))==NULL)
      {
         GS_ERR_COD = eGSInvalidPrjName;      
         return GEOsimAppl::CMDLIST.ErrorCmd();
      }
      acutPrintf(GS_LFSTR);
      if (ads_getstring(TRUE, gsc_msg(129), Password) != RTNORM) // "Password: "
         return GEOsimAppl::CMDLIST.ErrorCmd();
      if (gsc_getconfirm(gsc_msg(163),&ris,GS_BAD)==GS_GOOD)   // "Confermare.
      {
         if (ris==GS_GOOD)
            if (gsc_delprj(pProject->get_key(), Password) == GS_BAD) return GEOsimAppl::CMDLIST.ErrorCmd();
      }
   }

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************/
/*.doc gs_delprj <external> */
/*+
  Questa funzione LISP cancella un progetto di GEOsim.
  Parametri:
  Lista RESBUF: <prjcode><password>
  
  <prjcode>  = codice progetto da cancellare
  <password> = password utente corrente
    
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_delprj(void)
{
   int     prj;
   presbuf arg = acedGetArgs();

   acedRetNil();

   // codice del progetto
   if (!arg || gsc_rb2Int(arg, &prj) != GS_GOOD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   // password utente corrente
   if (!(arg = arg->rbnext) || arg->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // chiamata funzione C++ 
   if (gsc_delprj(prj, arg->resval.rstring) == GS_BAD) return RTERROR;
   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_delprj <external> */
/*+
  Questa funzione cancella un progetto di GEOsim dal codice del progetto.
  Parametri:
  int prjcode;             codice progetto
  const TCHAR *Password;   password uetnete corrente
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_delprj(int prjcode, const TCHAR *Password)
{  
   C_PROJECT *pProject;
   
   // cerco elemento in lista progetti
   if ((pProject =(C_PROJECT*)GEOsimAppl::PROJECTS.search_key(prjcode)) == NULL) return GS_BAD;
   
   // cancello progetto
   if (pProject->del(Password) == GS_BAD) return GS_BAD;

   // cancello il progetto da lista GEOsimAppl::PROJECTS
   if ((GEOsimAppl::PROJECTS.remove(pProject)) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc del                                    <external> */
/*+
  Questa funzione cancella un progetto di GEOsim.
  Il progetto viene cancellato solo se questo è privo di classi di entità.
  Parametri:
  const TCHAR *Password;    Password dell'utente corrente (per controllo)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::del(const TCHAR *Password)                             
{
   C_STRING           pathfile, Catalog, Schema, Table, NewTableRef;
   _RecordsetPtr      pRs;
   long               ClassesQty;
   C_INT_INT_STR_LIST pusr_list;       // lista utenti
   C_INT_INT_STR      *pusr;
   C_INT_INT_LIST     UsrPrjList;    // lista progetti utente
   C_INT_INT          *pUsrPrj;
   C_DBCONNECTION     *pSourceConn;
   C_STR_LIST         TableList;
   C_STR              *pTable;
   int                result;

   // verifico abilitazione
   if (gsc_check_op(opDelPrj) == GS_BAD) return GS_BAD;
   // verifico che la password sia corretta
   if (gsc_strcmp(Password, (TCHAR *) GEOsimAppl::GS_USER.pwd) != 0)
      { GS_ERR_COD = eGSInvalidPwd; return GS_BAD; }

   if ((ClassesQty = global_count_class()) < 0) return GS_BAD;
   if (ClassesQty > 0)
      { GS_ERR_COD = eGSPrjNotEmpty; return GS_BAD; }

   // verifico esistenza aree di lavoro (solo per sicurezza)
   if (is_free() == GS_BAD)
   {
      GS_ERR_COD = eGSSessionsFound;
      return GS_BAD;
   }

   // blocco riga in GS_PRJ
   if (Lock(pRs) == GS_BAD) return GS_BAD;

   // leggo lista degli utenti
   if (gsc_getusrlist(&pusr_list) == GS_GOOD) 
   {
      pusr = (C_INT_INT_STR *) pusr_list.get_head();  // inizio lista
      // per ciascun utente non SUPER-USER cancello il progetto dal
      // suo file delle abilitazioni
      while (pusr != NULL)
      {
         if (pusr->get_type() != SUPERUSR) 
            if (gsc_getPrjPermissions(pusr->get_key(), UsrPrjList) == GS_GOOD)
               if ((pUsrPrj = (C_INT_INT *) UsrPrjList.search_key(gs_id)) != NULL)
                  if (UsrPrjList.remove(pUsrPrj->get_key(), pUsrPrj->get_type()) == GS_GOOD)
                     gsc_setPersonalPrjPermissions(pusr->get_key(), UsrPrjList);
         pusr = (C_INT_INT_STR *) pusr_list.get_next();  // elemento successivo  
      }
   }

   // Leggo la tabelle campione e la connessione OLEDB al DB campione
   if (gsc_getPRJSampleTables(TableList, &pSourceConn) == GS_BAD)
      { Unlock(pRs, READONLY); return GS_BAD; }

   int IsTransactionSupported = getDBConnection()->BeginTrans();

   result = GS_GOOD;
   pTable = (C_STR *) TableList.get_head();
   while (pTable)
   {
      if (pSourceConn->split_FullRefTable(pTable->get_name(),
                                          Catalog, Schema, Table) == GS_BAD)
         { result = GS_BAD; break; }
      if (NewTableRef.paste(getDBConnection()->get_FullRefTable(GEOPRJCATALOG, NULL, Table.get_name())) == NULL)
         { result = GS_BAD; break; }

      if (getDBConnection()->ExistTable(NewTableRef) == GS_GOOD)
         if (getDBConnection()->DelTable(NewTableRef.get_name()) == GS_BAD)
            { result = GS_BAD; break; }

      pTable = (C_STR *) TableList.get_next();
   }
   if (result == GS_BAD)
   {
      if (IsTransactionSupported == GS_GOOD) getDBConnection()->RollbackTrans();
      Unlock(pRs, READONLY);
      return GS_BAD;
   }
   if (IsTransactionSupported == GS_GOOD) getDBConnection()->CommitTrans();

   // cancello il direttorio del progetto e tutti i suoi sottodirettori
   // sottodirettorio GEOUSRDIR
   pathfile = dir;
   pathfile += _T('\\');
   pathfile += GEOUSRDIR;
   gsc_delall(pathfile.get_name(), NORECURSIVE);
   // sottodirettorio GEOQRYDIR
   pathfile = dir;
   pathfile += _T('\\');
   pathfile += GEOQRYDIR;
   gsc_delall(pathfile.get_name(), NORECURSIVE);
   // sottodirettorio GEOVISDIR
   pathfile = dir;
   pathfile += _T('\\');
   pathfile += GEOVISDIR;
   gsc_delall(pathfile.get_name(), NORECURSIVE);
   // sottodirettorio GEOSELCLSDIR
   pathfile = dir;
   pathfile += _T('\\');
   pathfile += GEOSELCLSDIR;
   gsc_delall(pathfile.get_name(), NORECURSIVE);
   // sottodirettorio GEOSUPPORTDIR
   pathfile = dir;
   pathfile += _T('\\');
   pathfile += GEOSUPPORTDIR;
   gsc_delall(pathfile.get_name(), NORECURSIVE);

   // cancello la riga del recordset vedi "Lock"
   if (gsc_DBDelRow(pRs) == GS_BAD)
   {  
      Unlock(pRs, READONLY);
      return GS_BAD;
   }
                                     
   if (Unlock(pRs) == GS_BAD) return GS_BAD;

   // Chiudo la connessione con il database del progetto perchè 
   // questa blocca il database (ACCESS)
   GEOsimAppl::TerminateSQL(getDBConnection());
   gsc_delall(dir.get_name(), NORECURSIVE);
  
   return GS_GOOD;
}


/*********************************************************/
/*.doc gs_modprj <external> */
/*+
  Questa funzione LISP modifica un progetto di GEOsim.
  Parametri:
  Lista RESBUF (prjcode, prjname, prjdescr, key_dwg, coordsys)
    
  Restituisce TRUE in caso di successo altrimenti restituisce NIL.
-*/  
/*********************************************************/
int gs_modprj(void)
{
   TCHAR    *prjname, *key_dwg, *coordsys;
   int      prj, dwg_dim = 0;
   resbuf   *arg = acedGetArgs();
   C_STRING prjDescr;

   acedRetNil();

   // codice del progetto
   if (!arg || gsc_rb2Int(arg, &prj) != GS_GOOD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // nome del progetto da modificare
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSTR) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   prjname = arg->resval.rstring;

   // descrizione del progetto da modificare
   if ((arg = arg->rbnext) == NULL) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (arg->restype == RTSTR) prjDescr = arg->resval.rstring;

   // nome del file .dwg da utilizzare come key
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSTR) // Errore argomenti
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   key_dwg = arg->resval.rstring;

   // codice sistema di coordinate
   if ((arg = arg->rbnext) == NULL || arg->restype != RTSTR) // Errore argomenti
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   coordsys = arg->resval.rstring;

   // dwg_dim divisione territorio (opzionale)
   if ((arg = arg->rbnext) != NULL)
      if (gsc_rb2Int(arg, &dwg_dim) == GS_BAD) return GS_BAD;

   // chiamata funzione C++ 
   if (gsc_modprj(prj, prjname, prjDescr.get_name(), key_dwg, coordsys, dwg_dim) == GS_BAD)
      return RTERROR;

   acedRetT();

   return RTNORM;
}


/*********************************************************/
/*.doc gsc_modprj <external> */
/*+
  Questa funzione modifica un progetto di GEOsim.
  Parametri:
  int prjcode;         codice progetto
  TCHAR *prjname;      nuovo nome del progetto
  TCHAR *prjDescr;     nuova descrizione del progetto
  TCHAR *key_dwg;      nuovo nome del file .dwg da utilizzare come key
  TCHAR *coordsys;     nuovo codice sistema di coordinate
  int  dwg_dim;        Nuove dimensioni DWG classi (Mb) (se = 0 rimangono invariate;
                       default = 0)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_modprj(int prjcode, TCHAR *prjname, TCHAR *prjDescr,
               TCHAR *key_dwg, TCHAR *coordsys, int dwg_dim)
{  
   C_PROJECT *pProject;
   
   // cerco elemento in lista progetti
   if ((pProject = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prjcode)) == NULL) return GS_BAD;
   
   // modifico riga di GS_PRJ con i nuovi valori.
   if (pProject->modify(prjname, prjDescr, key_dwg, coordsys, dwg_dim) == GS_BAD)
      return GS_BAD;
  
   return GS_GOOD;
}


/*********************************************************
/*.doc modify <external> */
/*+
  Questa funzione crea un progetto di GEOsim.
    
  La protezione di questa operazione è garantita perchè la procedura
  controlla che non ci siano altri progetti con lo stesso nome. Per quanto
  riguarda la multiutenza, è stato usato un indice con modalità UNIQUE sul
  campo GS_ID e sul campo NAME della tabella GS_PRJ. Questo garantisce che,
  se due utenti modificano un progetto nello stesso momento, potrebbe essere
  che entrambi abbiano lo stesso NAME. Allo stesso modo, se un utente sta
  aggiornando un progetto aggiornandone il nome mentre viene inserito un nuovo
  progetto con lo stesso nome si avrebbe una situazione di conflitto.
  In questi casi la funzione termina con un errore.
  Parametri:
  TCHAR *prjname;       nuovo nome del progetto
  TCHAR *prjDescr;      nuova descrizione del progetto
  TCHAR *key_dwg;       nuovo nome del file .dwg da utilizzare come key
  TCHAR *coordsys;      nuovo codice sistema di coordinate
  int   _dwg_dim;       Nuove dimensioni DWG classi (Mb) (se = 0 rimangono invariate;
                        default = 0)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::modify(TCHAR *prjname, TCHAR *prjDescr, TCHAR *key_dwg, 
                      TCHAR *coordsys, int _dwg_dim)
{
   C_RB_LIST      ColValues;
   C_PROJECT      temp;
   C_STRING       _key_dwg, TableRef, statement;
   C_DBCONNECTION *_pConn;
   _RecordsetPtr  pRs;

   // verifico abilitazione (vedi anche aree aperte)
   if (gsc_check_op(opModPrj) == GS_BAD) return GS_BAD;

   if (temp.set_name(prjname) == GS_BAD || temp.set_descr(prjDescr) == GS_BAD || temp.set_keydwg(key_dwg) == GS_BAD ||
       temp.set_coordinate(coordsys) == GS_BAD || temp.set_dir(get_dir()) == GS_BAD)
      return GS_BAD; 
   if (_dwg_dim > 0) set_dwg_dim(_dwg_dim);

   // traduco il path relativo ("C:/DIR1") in path assoluto (<MACCHINA1>/DIR1)
   _key_dwg = key_dwg;
   if (gsc_drive2nethost(_key_dwg) == GS_BAD) return GS_BAD;

   if ((ColValues << acutBuildList(RTLB,
                                   RTLB, RTSTR, _T("NAME"), RTSTR, prjname, RTLE,
                                   RTLB, RTSTR, _T("DESCRIPTION"), RTSTR, (prjDescr) ? prjDescr : GS_EMPTYSTR, RTLE,
                                   RTLB, RTSTR, _T("KEYDWG"), 0)) == NULL)
      return GS_BAD;
   if ((ColValues += gsc_str2rb(_key_dwg)) == NULL) return GS_BAD;
   if ((ColValues += acutBuildList(RTLE, RTLB, RTSTR, _T("COORDINATE"), 0)) == NULL) return GS_BAD;
   if ((ColValues += gsc_str2rb(coordsys)) == NULL) return GS_BAD;
   if ((ColValues += acutBuildList(RTLE, RTLB, RTSTR, _T("DWG_DIM"), RTSHORT, dwg_dim, RTLE,
                                   RTLE, 0)) == NULL) return GS_BAD;

   if (gsc_getPrjTabInfo(&_pConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE GS_ID=");
   statement += gs_id;

   // modifico riga in GS_PRJ
   // prima era adOpenKeyset poi adOpenDynamic ma PostgreSQL
   // in una transazione fa casino (al secondo recordset che viene aperto)
   if (_pConn->OpenRecSet(statement, pRs, adOpenForwardOnly, adLockOptimistic) == GS_BAD) return GS_BAD;
   if (gsc_DBUpdRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
  
   // modifico progetto in memoria
   set_name(prjname);
   set_descr(prjDescr);
   set_keydwg(key_dwg);
   set_coordinate(coordsys);

   // memorizzo il codice del progetto in GEOSIM.INI
   gsc_setLastUsedPrj(gs_id);

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getClassesTabInfo          <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente la lista delle classi del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              la lista dei progetti di GEOsim (default = NULL)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getClassesTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, CLASSES_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getGraphClassesTabInfo       <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni sulle 
  caratteristiche grafiche di default delle classi del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getGraphClassesTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, 
                                               GRAPH_CLASS_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getClassGphDataSrcTabInfo  <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni sulla sorgente dati 
  grafica delle classi del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getClassGphDataSrcTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, 
                                               CLASS_GPH_DATA_SRC_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getAttribsTabInfo          <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le caratteristiche 
  NON grafiche degli attributi delle classi del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              la lista dei progetti di GEOsim (default = NULL)
  int            BackUp;      Flag, se = GS_BAD ci si riferisce alla tabella
                              originale degli attributi, altrimenti ci si 
                              riferisce alla tabella di back-up degli attributi
                              (default = GS_BAD)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getAttribsTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef, int BackUp)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      if (BackUp == GS_BAD) // si vuole la tabella originale
         pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, ATTRIBUTES_TABLE_NAME));
      else // si vuole la tabella di back-up
         pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, BACKUP_ATTRIBUTES_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getGraphAttribsTabInfo     <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le caratteristiche 
  grafiche degli attributi delle classi del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              la lista dei progetti di GEOsim (default = NULL)
  int            BackUp;      Flag, se = GS_BAD ci si riferisce alla tabella
                              originale degli attributi, altrimenti ci si 
                              riferisce alla tabella di back-up degli attributi
                              (default = GS_BAD)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getGraphAttribsTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef, int BackUp)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      if (BackUp == GS_BAD) // si vuole la tabella originale
         pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, 
                                                  GRAPH_ATTRIBUTES_TABLE_NAME));
      else // si vuole la tabella di back-up
         pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL,
                                                  BACKUP_GRAPH_ATTRIBUTES_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getSecsTabInfo             <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni
  delle tabelle secondarie del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getSecsTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, SECONDARIES_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getLocksTabInfo            <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni
  dei lock alle entità del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getLocksTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, LOCKS_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getWrkSessionsTabInfo      <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni
  delle sessioni di lavoro del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getWrkSessionsTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, WRKSESSIONS_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getConnectionsTabInfo      <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni
  delle regole di connessioni delle classi del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getConnectionsTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, CONNECTIONS_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getGrpsTabInfo        <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni
  della composizione delle classi gruppo del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getGrpsTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, GROUPS_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getLayerModelTabInfo   <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni dei prototipi
  dei layer del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getLayerModelTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, LAYER_MODEL_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getClassSetTabInfo   <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni dei set di classi del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getClassSetTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, SET_CLASS_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc C_PROJECT::getClassSetTreeTabInfo   <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente le informazioni di ciò che è contenuto 
  nei set di classi del progetto.
  Parametri:
  C_DBCONNECTION **pDBConn;   Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              le informazioni sulla grafica
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getClassSetTreeTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef)
{
   if ((*pDBConn = getDBConnection()) == NULL) return GS_BAD;

   if (pTableRef)
      pTableRef->paste(pConn->get_FullRefTable(GEOPRJCATALOG, NULL, SET_CLASS_TREE_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_newcode <external> */
/*+
  Questa funzione cerca nella tabella indicata da tablerif nella colonna
  nominata GS_ID qual'è il codice successivo, nuovo, per poter inserire 
  una nuova riga nella tabella stessa.
  Parametri:
  C_DBCONNECTION *pConn;   Connessione OLE-DB
  C_STRING &tablerif;      Riferimento completo alla tabella
  int *new_code;           Nuovo codice progressivo
  const TCHAR *Attrib;
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_newcode(C_DBCONNECTION *pConn, C_STRING &TableRef, int *new_code, const TCHAR *Attrib)
{
   C_STRING      statement, Field(Attrib);
   _RecordsetPtr pRs;
   
   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return NULL;

   *new_code = 1;
          
   statement = _T("SELECT ");
   statement += Field; // Attrib
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" ORDER BY ");
   statement += Field; // Attrib

   // leggo le righe della tabella senza bloccarla
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;

   if (gsc_getNewCode(pRs, new_code) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_newsubcode                         <external> */
/*+
  Questa funzione cerca nella tabella indicata da tablerif nella colonna
  nominata SUB_GS_ID qual'è il codice successivo, nuovo, per poter inserire 
  una nuova riga nella tabella stessa.
  Parametri:
  C_DBCONNECTION *pConn;   Connessione OLE-DB
  C_STRING &tablerif;      Riferimento completo alla tabella
  int  new_code            codice classe 
  int  *new_subcode        nuovo codice progressivo della sottoclasse

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_newsubcode(C_DBCONNECTION *pConn, C_STRING &TableRef, int code,
                   int *new_subcode)
{
   C_STRING      statement, csrname;
   _RecordsetPtr pRs;
   presbuf       colvalues = NULL;
   C_STR_LIST    FieldList;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("GS_ID"));
   FieldList.add_tail_str(_T("SUB_GS_ID"));
   if (gsc_AdjSyntax(FieldList, pConn) == GS_BAD) return GS_BAD;

   *new_subcode = 1;
          
   statement = _T("SELECT ");
   statement += FieldList.goto_pos(2)->get_name(); // SUB_GS_ID
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" WHERE "); 
   statement += FieldList.goto_pos(1)->get_name(); // GS_ID
   statement += _T("=");
   statement += code;
   statement += _T(" ORDER BY ");
   statement += FieldList.goto_pos(2)->get_name(); // SUB_GS_ID

   // leggo le righe della tabella senza bloccarla
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;

   if (gsc_getNewCode(pRs, new_subcode) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


int gsc_getNewCode(_RecordsetPtr &pRs, int *new_code)
{
   presbuf   gs_id;
   int       gs_id_prev = 0, gs_id_actual;
   C_RB_LIST ColValues;

   // scorro l'elenco dei GS_ID
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) return GS_BAD;
      if ((gs_id = gsc_nth(1, gsc_nth(0, ColValues.get_head()))) == NULL)
         return GS_BAD;
      // per le simulazioni con sottoclassi
      if (gsc_rb2Int(gs_id, &gs_id_actual) == GS_BAD) return GS_BAD;
      if (gs_id_prev == gs_id_actual) (*new_code)--;
      if (*new_code != gs_id_actual) return GS_GOOD;
         
      (*new_code)++;
      gs_id_prev = gs_id_actual;

      gsc_Skip(pRs);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc restore_projects <external> */
/*+
  Questa funzione carica GS_PRJ in memoria in ordine di nome progetto.
  Parametri:
  int cod_usr;    codice utente abilitato ai progetti, se = -1 utente
                  corrente (default = -1)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_LIST_PRJ::restore_projects(int cod_usr)
{            
   C_STRING       statement, TableRef;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   C_RB_LIST	   ColValues;
   C_INT_INT_LIST pusr_listprj;           // lista progetti utente
   C_INT_INT      *pusr_prj;
   C_PROJECT      *pProject;
   int            Codice_utente;

   // cancello la lista attuale dei progetti
   if (remove_all() == GS_BAD) return GS_BAD;

   Codice_utente = (cod_usr == -1) ? GEOsimAppl::GS_USER.code : cod_usr;

   if (gsc_getPrjTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
  
   if (gsc_superuser(Codice_utente) != GS_GOOD)  // non è un SUPER USER
   {  // leggo lista dei progetti visibili per l'utente corrente
      // lista progetti utente
      bool first = true;

      if (gsc_getPrjPermissions(Codice_utente, pusr_listprj) == GS_BAD)
         return GS_GOOD; // nessuna abilitazione   
      
      if (pusr_listprj.is_empty() == TRUE)
         return GS_GOOD; // nessuna abilitazione   
      
      statement += _T(" WHERE ");

      pusr_prj = (C_INT_INT *) pusr_listprj.get_head();  // inizio lista
      while (pusr_prj)
      {
         if (pusr_prj->get_type() == (int) GSInvisibleData) // salto i progetti invisibili
            { pusr_prj = (C_INT_INT *) pusr_listprj.get_next(); continue; }

         if (first)
            first = false;
         else
            statement += _T(" OR ");

         statement += _T("GS_ID=");
         statement += pusr_prj->get_key();
         pusr_prj = (C_INT_INT *) pusr_listprj.get_next();  // elemento successivo  
      }
   }   

   statement += _T(" ORDER BY GS_ID");     
      
   // leggo le righe della tabella in ordine di GS_ID senza bloccarla
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;

   // scorro l'elenco dei progetti
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      pProject = new C_PROJECT();
      if (pProject->from_rb_db(ColValues.get_head(), &pusr_listprj) == GS_BAD)
      {
         gsc_DBCloseRs(pRs);
         delete pProject;
         return GS_BAD;
      }

      add_tail((C_NODE *) pProject);  // inserimento in lista
 
      gsc_Skip(pRs);
   }

   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

   // ordino per nome progetto con sensitive = FALSE
   sort_name();

   return GS_GOOD;
}


/*********************************************************/
/*.doc gsc_restoreproject <internal> */
/*+
  Questa funzione carica in memoria un progetto di GEOsim
  inizializzando GEOsimAppl::PROJECTS.
  Parametri:
  int cod_prog;      Codice del progetto
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_restoreproject(int cod_prog)
{  
   if (GEOsimAppl::PROJECTS.restore_project(cod_prog) == NULL) return GS_BAD;

   return GS_GOOD;
}   


/*********************************************************/
/*.doc restore_project <external> */
/*+
  Questa funzione carica un progetto da GS_PRJ in memoria.
  Parametri:
  int cod_prog;      Codice del progetto
    
  Restituisce il puntatore al progetto  altrimenti restituisce NULL.
-*/  
/*********************************************************/
C_PROJECT *C_LIST_PRJ::restore_project(int cod_prog)
{            
   C_STRING       statement, TableRef;
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   C_RB_LIST	   ColValues;
   C_INT_INT_LIST pusr_listprj;           // lista progetti utente
   C_INT_INT      *pusr_prj;
   C_PROJECT      *pProject;

   remove_key(cod_prog);  // cancello elemento già presente

   if (gsc_getPrjTabInfo(&pConn, &TableRef) == GS_BAD) return NULL;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   
   if (gsc_superuser() != GS_GOOD)  // non è un SUPER USER
   {
      // lista progetti utente
      if (gsc_getPrjPermissions(GEOsimAppl::GS_USER.code, pusr_listprj) == GS_BAD)
         return NULL; // nessuna abilitazione   
      if (!(pusr_prj = (C_INT_INT *) pusr_listprj.search_key(cod_prog)) || 
          pusr_prj->get_type() == (int) GSInvisibleData) // salto il progetto se invisibile
         return NULL; // classe non abilitata
   }   

   statement += _T(" WHERE GS_ID=");
   statement += cod_prog;
      
   // leggo le righe della tabella in ordine di GS_ID senza bloccarla
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return NULL;
                
   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return NULL; }

      if (gsc_DBCloseRs(pRs) == GS_BAD) return NULL;

      pProject = new C_PROJECT();
      if (pProject->from_rb_db(ColValues.get_head(), &pusr_listprj) == GS_BAD)
         { delete pProject; return NULL; }

      if (add_tail((C_NODE*)pProject) == GS_BAD) return NULL; // inserimento in lista

      // ordino per nome progetto con sensitive = FALSE
      sort_name();
   }
   else
   {
      gsc_DBCloseRs(pRs);
      GS_ERR_COD = eGSInvalidPrjCode;
      return NULL;
   }
         
   return pProject;
}
 

/*********************************************************
/*.doc from_rb_db <external> */
/*+
  Questa funzione carica i dati del progetto da un resbuf generato dalla
  lettura su tabella GS_PRJ.
  Parametri:             
  presbuf ColValues;           lista colonna-valore della riga di GS_PROG
  C_INT_INT_LIST pusr_listprj; lista progetti utente
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/
/*********************************************************/
int C_PROJECT::from_rb_db(presbuf ColValues, C_INT_INT_LIST *pusr_listprj)
{
   presbuf p_prj;
   C_INT_INT *pusr_prj;

   if ((p_prj = gsc_CdrAssoc(DEFAULT_KEY_ATTR, ColValues)) == NULL)     // GS_ID
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }  
   if (gsc_rb2Int(p_prj, &gs_id) == GS_BAD)
      { GS_ERR_COD = eGSInvalidPrjCode; return GS_BAD; }

   if ((p_prj = gsc_CdrAssoc(_T("NAME"), ColValues)) == NULL)      // NAME
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (p_prj->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidPrjName; return GS_BAD; }
   gsc_strcpy(name, gsc_rtrim(p_prj->resval.rstring), MAX_LEN_PRJNAME);

   if ((p_prj = gsc_CdrAssoc(_T("DESCRIPTION"), ColValues)) && p_prj->restype == RTSTR) // DESCRIPTION
      Descr = p_prj->resval.rstring;
   else
      Descr.clear();
      
   if ((p_prj = gsc_CdrAssoc(_T("DIR"), ColValues)) == NULL)       // DIR
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (p_prj->restype != RTSTR)
      { GS_ERR_COD = eGSInvalidPath; return GS_BAD; }
   dir = gsc_rtrim(p_prj->resval.rstring);
   // traduco dir assoluto in dir relativo
   if (gsc_nethost2drive(dir) == GS_BAD && GS_ERR_COD != eGSInvHostName)
      return GS_BAD;  // se l'errore è per la mancanza di un Host, vado avanti

   if ((p_prj = gsc_CdrAssoc(_T("KEYDWG"), ColValues)) == NULL)    // KEYDWG
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (p_prj->restype != RTSTR) keydwg.clear();
   else
      keydwg = gsc_rtrim(p_prj->resval.rstring);

   // traduco dir assoluto in dir relativo
   if (gsc_nethost2drive(keydwg) == GS_BAD && GS_ERR_COD != eGSInvHostName)
      return GS_BAD;

   if ((p_prj = gsc_CdrAssoc(_T("COORDINATE"), ColValues)) == NULL) // COORDINATE
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   if (p_prj->restype != RTSTR) 
      swprintf(coordinate, MAX_LEN_COORDNAME, _T("%s"), DEFAULT_DWGCOORD);
   else
      gsc_strcpy(coordinate, gsc_rtrim(p_prj->resval.rstring), MAX_LEN_COORDNAME);

   if (gsc_superuser() == GS_GOOD)
      level = GSUpdateableData;   // progetti modificabili 
   else                                     
   {
      pusr_prj = (C_INT_INT *) pusr_listprj->search_key(gs_id);
      level = (GSDataPermissionTypeEnum) pusr_prj->get_type();   // abilitazione progetto
   }  

   // dwg_dim divisione territorio (opzionale)
   if ((p_prj = gsc_CdrAssoc(_T("DWG_DIM"), ColValues)))
      if (gsc_rb2Int(p_prj, &dwg_dim) == GS_BAD)
         dwg_dim = DEFAULT_DWG_DIM;

   // UDL_FILE (opzionale)
   if ((p_prj = gsc_CdrAssoc(_T("UDL_FILE"), ColValues)))
      if (p_prj->restype != RTSTR) ConnStrUDLFile.clear();
      else
      {
         ConnStrUDLFile = gsc_alltrim(p_prj->resval.rstring);
         if (gsc_path_exist(ConnStrUDLFile) == GS_GOOD)
            // traduco dir da assoluto in dir relativo
            if (gsc_nethost2drive(ConnStrUDLFile) == GS_BAD) return GS_BAD;
      }

   // UDL_PROP (opzionale)
   if ((p_prj = gsc_CdrAssoc(_T("UDL_PROP"), ColValues)))
      if (p_prj->restype != RTSTR) UDLProperties.remove_all();
      else
      {
         C_STRING Buffer(gsc_alltrim(p_prj->resval.rstring));
         
         // Conversione path UDLProperties da assoluto in dir relativo
         if (gsc_UDLProperties_nethost2drive(ConnStrUDLFile.get_name(), Buffer) == GS_BAD)
            return GS_BAD;
         if (gsc_PropListFromConnStr(Buffer.get_name(), UDLProperties) == GS_BAD)
            return GS_BAD;
      }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::find_class                  <external> */
/*+
  Ritorna il putatore alla classe del progetto specificata.
  Parametri:
  int cls;        Codice classe
  int sub;        Codice sottoclasse
  oppure 
  ads_name ent;   Oggetto grafico
  
  Restituisce NULL in caso di errore.
-*/  
/*********************************************************/
C_CLASS *C_PROJECT::find_class(ads_name ent)
{
   C_EED eed;

   if (eed.load(ent) == GS_BAD) return NULL;

   return find_class(eed.cls, eed.sub);
}
C_CLASS *C_PROJECT::find_class(int cls, int sub)
{
   #if defined(GSDEBUG) // se versione per debugging
      double t1 = gsc_get_profile_curr_time();
   #endif
   C_CLASS *pCls = ptr_classlist()->search_key(cls, sub); // cerco in memoria

   if (!pCls) // carico la classe da database
      if (!(pCls = load_class_from_db(cls))) return NULL;

   #if defined(GSDEBUG) // se versione per debugging
      gsc_profile_int_func(t1, 0, gsc_get_profile_curr_time(), "C_PROJECT::find_class");
   #endif

   return (sub == 0) ? pCls : ptr_classlist()->search_key(cls, sub);
}  


/*********************************************************/
/*.doc C_PROJECT::load_class_from_db          <internal> */
/*+
  Questa funzione carica una classe in memoria da database.
  Parametri:
  int cod_class;      Codice della classe
    
  Restituisce il puntatore alla classe in caso di successo altrimenti NULL.
-*/  
/*********************************************************/
C_CLASS *C_PROJECT::load_class_from_db(int cod_class)
{
   GSDataPermissionTypeEnum cod_level;
   int                      category;
   C_STRING                 statement, TableRef;
   C_DBCONNECTION           *pConn;
   _RecordsetPtr            pRs;
   C_RB_LIST	             ColValues;
   C_INT_INT_LIST           usr_listclass; // lista classi utente
   C_INT_INT                *pusr_class;
   C_CLASS                  *pclass = NULL;
   C_STR_LIST               FieldList;

   acutPrintf(_T("%s%d...\n"), gsc_msg(330), cod_class); // "\nLettura definizione classe avente codice "

   classlist.remove_key(cod_class);  // cancello elemento già presente

   if (gsc_superuser() != GS_GOOD)  // non è un SUPER USER
   {
      // lista classi utente
      if (gsc_getClassPermissions(GEOsimAppl::GS_USER.code, gs_id, usr_listclass) == GS_BAD)
         return NULL;  // nessuna abilitazione

      if (!(pusr_class = (C_INT_INT *) usr_listclass.search_key(cod_class)) || 
          pusr_class->get_type() == (int) GSInvisibleData) // salto la classe se invisibile
         return NULL; // classe non abilitata
   }

   if (getClassesTabInfo(&pConn, &TableRef) == GS_BAD) return NULL;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("GS_ID"));
   FieldList.add_tail_str(_T("SUB_GS_ID"));
   if (gsc_AdjSyntax(FieldList, pConn) == GS_BAD) return NULL;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += FieldList.goto_pos(1)->get_name(); // GS_ID
   statement += _T("=");
   statement += cod_class;
   // carico solo le classi saltando le sottoclassi
   statement += _T(" AND ");
   statement += FieldList.goto_pos(2)->get_name(); // SUB_GS_ID
   statement += _T("=0");
      
   // leggo le righe della tabella senza bloccarla
   if (pConn->OpenRecSet(statement, pRs) == GS_BAD) return NULL;
          
   if (gsc_isEOF(pRs) == GS_GOOD)
      { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSInvClassCode; return NULL; }

   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return NULL; }
   gsc_DBCloseRs(pRs);
      
   // Categoria
   if (gsc_rb2Int(ColValues.CdrAssoc(_T("CATEGORY")), &category) == GS_BAD)
      return NULL;
                               
   if (gsc_superuser() == GS_GOOD)
      cod_level = GSUpdateableData;   // classi modificabili 
   else                                     
      cod_level = (GSDataPermissionTypeEnum) pusr_class->get_type();   // abilitazione classe
                            
   // Alloca una nuova classe
   if ((pclass = gsc_alloc_new_class(category)) == NULL) return NULL;
  
   // Collegamento classe -> progetto
   pclass->id.pPrj = this;

   if (pclass->from_db(ColValues, cod_level) == GS_BAD)
      { delete pclass; return NULL; }

   // Collegamento progetto -> classe
   classlist.add_tail(pclass);

   return pclass;
}


/*********************************************************/
/*.doc C_PROJECT::getSinthClassList           <external> */
/*+
  Questa funzione restituisce una lista sintetica delle classi del progetto 
  in ordine di gs_id e sub_gs_id.
  Parametri:
  C_SINTH_CLASS_LIST &class_list;  lista risultato
  int retest;        se MORETESTS -> in caso di errore riprova n volte 
                     con i tempi di attesa impostati poi ritorna GS_BAD,
                     ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                     (default = MORETESTS)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getSinthClassList(C_SINTH_CLASS_LIST &SinthClassList, int retest)
{
   int            cod_class, cod_type, cod_category, cod_sel, prevCodClass, prevCategoryClass, subCod_class;
   C_COLOR        color;
   GSDataPermissionTypeEnum cod_level;
   long           WrkSessionId;
   C_STRING       statement, TableRef, GraphTableRef, GraphDataSrcTableRef, ClsDirDwg, ClsVersion, ClsDescr;
   TCHAR          name_class[MAX_LEN_CLASSNAME];
   C_DBCONNECTION *pConn;
   _RecordsetPtr  pRs;
   C_RB_LIST	   ColValues;
   presbuf        p_rb;
   C_INT_INT_LIST usr_listclass;           // lista classi utente
   C_INT_INT      *pusr_class;
   C_SINTH_CLASS  *pSinthCls, *pSinthSub;
   short          first_time = TRUE;
   C_CLASS        *pCls;
   C_STR_LIST     FieldList;
   bool           ClsViewEnabled, ClsHistoryEnabled;

   SinthClassList.remove_all();

   if (getClassesTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;
   if (getGraphClassesTabInfo(&pConn, &GraphTableRef) == GS_BAD) return GS_BAD;
   if (getClassGphDataSrcTabInfo(&pConn, &GraphDataSrcTableRef) == GS_BAD) return GS_BAD;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("GS_ID"));         // 1
   FieldList.add_tail_str(_T("SUB_GS_ID"));     // 2
   FieldList.add_tail_str(_T("NAME"));          // 3
   FieldList.add_tail_str(_T("DESCRIPTION"));   // 4
   FieldList.add_tail_str(_T("CATEGORY"));      // 5
   FieldList.add_tail_str(_T("TYPE"));          // 6
   FieldList.add_tail_str(_T("OWNER"));         // 7
   FieldList.add_tail_str(_T("COLOR"));         // 8
   FieldList.add_tail_str(_T("DIR_DWG"));       // 9
   FieldList.add_tail_str(_T("VERSION"));       // 10
   FieldList.add_tail_str(_T("CLASS_ID"));      // 11
   FieldList.add_tail_str(_T("SUB_CL_ID"));     // 12
   // 2015
   //FieldList.add_tail_str(_T("VIEW_ENABLED"));     // 13
   //FieldList.add_tail_str(_T("HISTORY_ENABLED"));  // 14
   if (gsc_AdjSyntax(FieldList, pConn) == GS_BAD) return GS_BAD;

   // SELECT A.GS_ID,A.SUB_GS_ID,A.NAME,A.CATEGORY,A.TYPE,A.OWNER,B.COLOR,C.DIR_DWG,A.VERSION,A.VIEW_ENABLED,A.HISTORY_ENABLED FROM 
   // (GS_CLASS A LEFT JOIN GS_CLASS_GRAPH B ON (A.GS_ID = B.CLASS_ID) AND (A.SUB_GS_ID = B.SUB_CL_ID)) LEFT JOIN 
   // GS_CLASS_GRAPH_INFO C ON (A.GS_ID = C.CLASS_ID) AND (A.SUB_GS_ID = C.SUB_CL_ID)

   // TableRef             = GS_CLASS            usa come alias la lettera A
   // GraphTableRef        = GS_CLASS_GRAPH      usa come alias la lettera B
   // GraphDataSrcTableRef = GS_CLASS_GRAPH_INFO usa come alias la lettera C
   statement =  _T("SELECT A.");
   statement += FieldList.goto_pos(1)->get_name(); // GS_ID
   statement += _T(", A.");
   statement += FieldList.goto_pos(2)->get_name(); // SUB_GS_ID
   statement += _T(", A.");
   statement += FieldList.goto_pos(3)->get_name(); // NAME
   statement += _T(", A.");
   statement += FieldList.goto_pos(4)->get_name(); // DESCRIPTION
   statement += _T(", A.");
   statement += FieldList.goto_pos(5)->get_name(); // CATEGORY
   statement += _T(", A.");
   statement += FieldList.goto_pos(6)->get_name(); // TYPE
   statement += _T(", A.");
   statement += FieldList.goto_pos(7)->get_name(); // OWNER
   statement += _T(", B.");
   statement += FieldList.goto_pos(8)->get_name(); // COLOR
   statement += _T(", C.");
   statement += FieldList.goto_pos(9)->get_name(); // DIR_DWG
   statement += _T(", A.");
   statement += FieldList.goto_pos(10)->get_name(); // VERSION
   // 2015
   //statement += _T(", A.");
   //statement += FieldList.goto_pos(13)->get_name(); // VIEW_ENABLED
   //statement += _T(", A.");
   //statement += FieldList.goto_pos(14)->get_name(); // HISTORY_ENABLED
   statement +=  _T(" FROM (");
   statement += TableRef;
   statement += _T(" A LEFT JOIN ");
   statement += GraphTableRef;
   statement += _T(" B ON (A.");
   statement += FieldList.goto_pos(1)->get_name(); // GS_ID
   statement += _T(" = B.");
   statement += FieldList.goto_pos(11)->get_name(); // CLASS_ID
   statement += _T(") AND (A.");
   statement += FieldList.goto_pos(2)->get_name(); // SUB_GS_ID
   statement += _T(" = B.");
   statement += FieldList.goto_pos(12)->get_name(); // SUB_CL_ID
   statement += _T(")) LEFT JOIN ");
   statement += GraphDataSrcTableRef;
   statement += _T(" C ON (A.");
   statement += FieldList.goto_pos(1)->get_name(); // GS_ID
   statement += _T(" = C.");
   statement += FieldList.goto_pos(11)->get_name(); // CLASS_ID
   statement += _T(") AND (A.");
   statement += FieldList.goto_pos(2)->get_name(); // SUB_GS_ID
   statement += _T(" = C.");
   statement += FieldList.goto_pos(12)->get_name(); // SUB_CL_ID
   statement += _T(")");

   if (gsc_superuser() != GS_GOOD)  // non è un SUPER USER
   {  // leggo lista delle classi visibili per l'utente corrente
      
      // lista classi utente
      if (gsc_getClassPermissions(GEOsimAppl::GS_USER.code, gs_id, usr_listclass) == GS_BAD)
         return GS_BAD; // nessuna abilitazione
      
      if (usr_listclass.is_empty() == TRUE) return GS_GOOD; // nessuna abilitazione

      pusr_class = (C_INT_INT *) usr_listclass.get_head();  // inizio lista
      while (pusr_class)
      {
         if (pusr_class->get_type() == (int) GSInvisibleData) // salto le classi invisibili
            { pusr_class = (C_INT_INT *) usr_listclass.get_next(); continue; }

         cod_class = pusr_class->get_key();
         if (first_time == TRUE)
         { 
            first_time = FALSE;
            statement += _T(" WHERE A.");
            statement += FieldList.goto_pos(1)->get_name(); // GS_ID
            statement += _T(" IN(");
         }
         statement += pusr_class->get_key();
         statement += _T(',');

         pusr_class = (C_INT_INT *) usr_listclass.get_next();  // elemento successivo  
      }   

      statement.set_chr(_T(')'), statement.len() - 1);  // sostituisco ultimo ',' con ')'
   }

   statement += _T(" ORDER BY A.");
   statement += FieldList.goto_pos(1)->get_name(); // GS_ID
   statement += _T(", A.");
   statement += FieldList.goto_pos(2)->get_name(); // SUB_GS_ID

   // leggo le righe della tabella senza bloccarla
   // il cursore del recordset deve andare avanti e indietro
   if (pConn->OpenRecSet(statement, pRs, adOpenKeyset, adLockReadOnly, retest) == GS_BAD) return GS_BAD;

   prevCodClass = prevCategoryClass = -1;
   // scorro l'elenco delle classi
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      // Codice classe
      if (gsc_rb2Int(ColValues.CdrAssoc(_T("GS_ID")), &cod_class) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      // Nome classe
      gsc_strcpy(name_class, gsc_rtrim(ColValues.CdrAssoc(_T("NAME"))->resval.rstring), MAX_LEN_CLASSNAME);

      // Descrizione classe
      if ((p_rb = ColValues.CdrAssoc(_T("DESCRIPTION"))) && p_rb->restype == RTSTR)
         ClsDescr = p_rb->resval.rstring;
      else
         ClsDescr.clear();

      // Categoria
      if (gsc_rb2Int(ColValues.CdrAssoc(_T("CATEGORY")), &cod_category) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      // Tipo
      if (gsc_rb2Int(ColValues.CdrAssoc(_T("TYPE")), &cod_type) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      // Se è deselezionata, selezionata, estratta (cercando in memoria)
      if ((pCls = (C_CLASS *) classlist.C_LIST::search_key(cod_class)) != NULL)
         cod_sel = pCls->ptr_id()->sel;
      else cod_sel = DESELECTED;

      // Livello di accesso
      if (gsc_superuser() == GS_GOOD) cod_level = GSUpdateableData; // classi modificabili 
      else                                     
      {
         pusr_class = (C_INT_INT *) usr_listclass.search_key(cod_class);
         cod_level = (GSDataPermissionTypeEnum) pusr_class->get_type();   // abilitazione
      }
      // verifico se è già bloccata in modo esclusivo
      if (gsc_rb2Lng(ColValues.CdrAssoc(_T("OWNER")), &WrkSessionId) == GS_GOOD &&
          WrkSessionId != 0)
         if (GS_CURRENT_WRK_SESSION)
         {
            if (GS_CURRENT_WRK_SESSION->get_id() != WrkSessionId) cod_level = GSExclusiveUseByAnother;
         }
         else cod_level = GSExclusiveUseByAnother;

      // Colore
      if (color.setResbuf(ColValues.CdrAssoc(_T("COLOR"))) == GS_BAD)
         color.setAutoCADColorIndex(DEFAULT_COLOR);

      // Cartella dei disegni
      if ((p_rb = ColValues.CdrAssoc(_T("DIR_DWG"))) == NULL || p_rb->restype != RTSTR)
         ClsDirDwg.clear();
      else
         ClsDirDwg = p_rb->resval.rstring;

      // Versione
      if ((p_rb = ColValues.CdrAssoc(_T("VERSION"))) == NULL || p_rb->restype != RTSTR)
         ClsVersion.clear();
      else
         ClsVersion = p_rb->resval.rstring;

      // abilitazione alla vista SQL
      if ((p_rb = ColValues.CdrAssoc(_T("VIEW_ENABLED"))) != NULL)
         gsc_rb2Bool(p_rb, &ClsViewEnabled);
      else
         ClsViewEnabled = false;

      // abilitazione alla storicizzazione
      if ((p_rb = ColValues.CdrAssoc(_T("HISTORY_ENABLED"))) != NULL) // HISTORY_ENABLED
         gsc_rb2Bool(p_rb, &ClsHistoryEnabled);
      else
         ClsHistoryEnabled = false;

      if (prevCodClass == cod_class && prevCategoryClass == CAT_EXTERN)
      {
         // Codice sottoclasse
         if (gsc_rb2Int(ColValues.CdrAssoc(_T("SUB_GS_ID")), &subCod_class) == GS_BAD)
            { gsc_DBCloseRs(pRs); return GS_BAD; }     

         if ((pSinthSub = new C_SINTH_CLASS(subCod_class, name_class, ClsDescr.get_name(),
                                            cod_category, cod_type, cod_level, cod_sel,
                                            ClsDirDwg.get_name(), ClsVersion.get_name(),
                                            &color, ClsViewEnabled, ClsHistoryEnabled)) == NULL)
            { gsc_DBCloseRs(pRs); return GS_BAD; }
         pSinthCls->ptr_sub_list()->add_tail(pSinthSub);
      }
      else
      {
         if ((pSinthCls = new C_SINTH_CLASS(cod_class, name_class, ClsDescr.get_name(),
                                            cod_category, cod_type, cod_level, cod_sel, 
                                            ClsDirDwg.get_name(), ClsVersion.get_name(),
                                            &color, ClsViewEnabled, ClsHistoryEnabled)) == NULL)
            { gsc_DBCloseRs(pRs); return GS_BAD; }
         SinthClassList.add_tail(pSinthCls);

         prevCodClass      = cod_class;
         prevCategoryClass = cod_category;
      }

      gsc_Skip(pRs);
   }

   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
         
   return GS_GOOD;
}
                                           

/*********************************************************/
/*.doc gs_getSinthClassList                   <external> */
/*+
  Questa funzione restituisce una lista sintetica delle classi visibili di 
  un progetto visibile all'utente corrente nella seguente forma:
  ((("CODE"<cls>)("NAME"<name>)("DESCRIPTION"<description>)("CATEGORY"<category>)
    ("TYPE"<type>)("LEVEL"<AccessType>)
    ("EXTRACTED"<Estratta>)("COLOR"<Colore>)("DIR_DWG"<dir_dwg>)("VERSION"<Versione>)<sub_list>)...)
  
  se category = CAT_EXTERN
     sub_list = ((("CODE"<sub>)("NAME"<name>)("DESCRIPTION"<description>)("CATEGORY"<category>)
                  ("TYPE"<type>)("LEVEL"<AccessType>)
                  ("EXTRACTED"<Estratta>)("COLOR"<Colore>)("DIR_DWG"<dir_dwg>)("VERSION"<Versione>))...)
  altrimenti 
     sub_list = nil
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_getSinthClassList(void)
{
   presbuf            arg = acedGetArgs();
   C_RB_LIST          ret;
   int                prj;
   C_PROJECT          *pPrj;
   C_SINTH_CLASS_LIST Result;
   
   acedRetNil();

   // codice del progetto
   if (!arg || gsc_rb2Int(arg, &prj) != GS_GOOD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // carico il progetto
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      return RTERROR;

   // leggo lista (<codice classe> <livello> <nome classe>)
   // in caso di errore non deve comparire la finestra di errore
   if (pPrj->getSinthClassList(Result, ONETEST) == GS_BAD) return RTERROR;

   // ordino per nome classe con sensitive = FALSE
   Result.sort_name();

   if ((ret << Result.to_rb()) == NULL) return RTNORM;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_rclass <external> */
/*+
  Questa funzione LISP restituisce l'elenco delle classi visibili di 
  un progetto visibile all'utente corrente.
  Lista : (<codice classe> <livello> <nome classe> <categoria> <tipo>)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. : Funzione in disuso ma non cancellata perchè usata da altri moduli
         esterni preferire la funzione "getSinthClassList"
-*/  
/*********************************************************/
int gs_rclass(void)
{
   presbuf         arg = acedGetArgs();
   C_RB_LIST       ret;
   C_PROJECT       *pProject;
   int             prj;
   C_4INT_STR_LIST lista;
   
   acedRetNil();

   // codice del progetto
   if (!arg || gsc_rb2Int(arg, &prj) != GS_GOOD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // carico il progetto
   if ((pProject = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      return RTERROR;

   // leggo lista (<codice classe> <livello> <nome classe>)
   // in caso di errore non deve comparire la finestra di errore
   if (pProject->rclass(lista, ONETEST) == GS_BAD) return RTERROR;

   // ordino per nome classe con sensitive = FALSE
   lista.sort_name();

   if ((ret << lista.to_rb()) == NULL) return RTNORM;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}


/*********************************************************/
/*.doc gs_get_group <external> */
/*+
  Questa funzione LISP restituisce l'elenco delle classi
  gruppo visibili all'utente corrente.
  Per ciascuna classe viene anche specificata una sottolista con
  le classi di cui è composta e quante entità per ciascuna classe
  (quante <= 0 significa qualsiasi numero).
  Lista : ( (<cls> ((<cls><quante>)(<cls><quante>)...)...)...)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_get_group(void)
{
   resbuf            *arg = acedGetArgs();
   C_RB_LIST         ret;
   int               prj;
   C_BIRELATION_LIST lista;
   C_PROJECT         *pProject;

   acedRetNil();

   // codice del progetto
   if (!arg || gsc_rb2Int(arg, &prj) != GS_GOOD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }

   // carico il progetto
   if ((pProject = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      return RTERROR;

   // leggo lista (<codice classe> <livello> <nome classe>)
   if (pProject->get_group_list(&lista) == GS_BAD )
      return RTERROR;

   // chiamata funzione C++ 
   if ((ret << lista.to_rb()) == NULL) return RTERROR;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();
 
   return RTNORM;
}                                         

/*********************************************************/
/*.doc gs_getSinthClsSecondaryTabList         <external> */
/*+
  Questa funzione restituisce una lista sintetica delle tabelle secondarie di una classe
  visibile di un progetto visibile all'utente corrente nella seguente forma:
  ((("CODE"<sec>)("NAME"<name>)("DESCRIPTION"<description>)("TYPE"<type>)
    ("KEY_PRI "<attr_name>)("KEY_ATTRIB "<attr_name>)
    ("REAL_INIT_DISTANCE_ATTRIB "<attr_name>)("REAL_FINAL_DISTANCE_ATTRIB "<attr_name>)
    ("VERSION"<Versione>))...)
      
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gs_getSinthClsSecondaryTabList(void)
{
   presbuf              arg = acedGetArgs();
   C_RB_LIST            ret;
   int                  prj, cls, sub;
   C_PROJECT            *pPrj;
   C_SINTH_CLASS_LIST   SinthClassList;
   C_SINTH_SEC_TAB_LIST SinthSecList;
   
   acedRetNil();

   // codice del progetto, classe e sotto-classe
   if (arg_to_prj_cls_sub(&arg, &prj, &cls, &sub) == GS_BAD) return RTERROR;

   // carico il progetto
   if ((pPrj = (C_PROJECT *) GEOsimAppl::PROJECTS.search_key(prj)) == NULL)
      return RTERROR;

   // leggo lista (<codice classe> <livello> <nome classe>)
   // in caso di errore non deve comparire la finestra di errore
   if (pPrj->getSinthClsSecondaryTabList(cls, sub, SinthSecList, ONETEST) == GS_BAD) return RTERROR;

   // ordino per nome le tabelle secondarie con sensitive = FALSE
   SinthSecList.sort_name();

   if ((ret << SinthSecList.to_rb()) == NULL) return RTNORM;
   if (ret.get_head()->restype != RTNIL) ret.LspRetList();

   return RTNORM;
}

  
/*********************************************************/
/*.doc extracted_class <external> */
/*+
  Questa funzione restituisce la lista di puntatori alle classi 
  estratte.
  Parametri:
  C_CLS_PUNT_LIST &out;             Lista puntatori a classi
  int FilterCategory;               Se <> 0 codice della categoria da filtrare
                                    (default = 0)
  int FilterType;                   Se <> 0 codice del tipo da filtrare (default = 0)
  int DynSegmentationSupportedOnly; Se = GS_GOOD filtra solo le classi che supportano
                                    La segmentazione dinamica (default = GS_BAD)

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  N.B. Alloca memoria!!
-*/  
/*********************************************************/
int C_PROJECT::extracted_class(C_CLS_PUNT_LIST &out, int FilterCategory, int FilterType,
                               int DynSegmentationSupportedOnly)
{
   C_STRING FilterCategoryStr, FilterTypeStr;

   if (FilterCategory > 0) FilterCategoryStr = FilterCategory;
   if (FilterType > 0) FilterTypeStr     = FilterType;
   
   return extracted_class(out, FilterCategoryStr, FilterTypeStr, DynSegmentationSupportedOnly);
}
int C_PROJECT::extracted_class(C_CLS_PUNT_LIST &out, C_STRING &FilterCategory, C_STRING &FilterType,
                               int DynSegmentationSupportedOnly)
{
   C_CLASS *punt;
   C_ID    *id;
   C_CLS_PUNT *new_nod;
   C_INT_LIST CategoryList, TypeList;

   out.remove_all();

   if (FilterCategory.len() > 0) CategoryList.from_str(FilterCategory.get_name(), _T(';'));
   if (FilterType.len() > 0) TypeList.from_str(FilterType.get_name(), _T(';'));

   punt = (C_CLASS *) classlist.get_head();
   while (punt)
   {
      id = punt->ptr_id();
      if (punt->is_extracted() == GS_GOOD)
      {
         if (CategoryList.get_count() > 0 && CategoryList.search_key(id->category) == NULL)
            { punt = (C_CLASS *) punt->get_next(); continue; }

         if (TypeList.get_count() > 0 && TypeList.search_key(id->type) == NULL)
            { punt = (C_CLASS *) punt->get_next(); continue; }

         if (DynSegmentationSupportedOnly == GS_GOOD)
            if (punt->is_DynSegmentationSupported() == false)
               { punt = (C_CLASS *) punt->get_next(); continue; }

         if ((new_nod = new C_CLS_PUNT(punt)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; out.remove_all(); return GS_BAD; }
         out.add_tail(new_nod);
      } 
      punt = (C_CLASS *) punt->get_next();
   }
   
   return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::CreateClass                 <external> */
/*+
  Questa funzione crea una classe di entità nel progetto.
  Parametri:
  C_CLASS *pCls;   Classe di entità
    
  Restituisce codice classe in caso di successo altrimenti restituisce 0.
-*/  
/*********************************************************/
int C_PROJECT::CreateClass(C_CLASS *pCls)
{
   // Collegamento classe -> progetto
   pCls->id.pPrj = this;

   return pCls->create();
}


/*********************************************************/
/*.doc C_PROJECT::DelClass                    <external> */
/*+
  Questa funzione cancella una classe di entità nel progetto.
  Parametri:
  C_CLASS *pCls;           Classe di entità
  const TCHAR *Password;   Password dell'utente corrente (per controllo)
    
  Restituisce codice classe in caso di successo altrimenti restituisce 0.
-*/  
/*********************************************************/
int C_PROJECT::DelClass(int cls, const TCHAR *Password)
{
   C_CLASS *pCls;
   
   if ((pCls = find_class(cls)) == NULL) return GS_BAD;

   return pCls->del(Password);
}


/*********************************************************/
/*.doc rclass <external> */
/*+
  Questa funzione restituisce una lista :
  (<code> <class_name> <level> <category> <type> <sub_list>)
  Parametri:
  C_4INT_STR_LIST &class_list;  lista risultato
  int retest;     se MORETESTS -> in caso di errore riprova n volte 
                  con i tempi di attesa impostati poi ritorna GS_BAD,
                  ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                  (default = MORETESTS) Aggiunto per tapullo su selezione progetto
                  da DCL (che non si chiude correttamente e rimane inattiva senza
                  possibilità di controllo) in modo che se questa funzione fallisse
                  non verrebbe aperta alcuna finestra di errore.
  
  se category = CAT_EXTERN
     sub_list = (<sub_code> <sub_class_name> <level> <category> <type>)
  altrimenti 
     sub_list = nil
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::rclass(C_4INT_STR_LIST &class_list, int retest)
{
   C_SINTH_CLASS_LIST SinthClassList;
   C_SINTH_CLASS      *pSinthCls;
   C_4INT_STR         *pCls;

   if (getSinthClassList(SinthClassList, retest) == GS_BAD) return GS_BAD;

   class_list.remove_all();

   pSinthCls = (C_SINTH_CLASS *) SinthClassList.get_head();
   while (pSinthCls)
   {
      if ((pCls = new C_4INT_STR()) == NULL) return GS_BAD;        
      if (pCls->set_name(pSinthCls->get_name()) == GS_BAD ||
          pCls->set_key(pSinthCls->get_key()) == GS_BAD ||
          pCls->set_level(pSinthCls->get_level()) == GS_BAD ||
          pCls->set_category(pSinthCls->get_category()) == GS_BAD ||
          pCls->set_type(pSinthCls->get_type()) == GS_BAD)
         return GS_BAD;
      class_list.add_tail(pCls);

      if (pSinthCls->get_category() == CAT_EXTERN)
      {   // scorro l'elenco delle sottoclassi
         C_SINTH_CLASS *pSinthSub = (C_SINTH_CLASS *) pSinthCls->ptr_sub_list()->get_head();
         C_4INT_STR    *pSub;

         while (pSinthSub)
         {
            if ((pSub = new C_4INT_STR()) == NULL) return GS_BAD;        
            if (pSub->set_name(pSinthSub->get_name()) == GS_BAD ||
                pSub->set_key(pSinthSub->get_key()) == GS_BAD ||
                pSub->set_level(pSinthSub->get_level()) == GS_BAD ||
                pSub->set_category(pSinthSub->get_category()) == GS_BAD ||
                pSub->set_type(pSinthSub->get_type()) == GS_BAD)
               return GS_BAD;
            pCls->ptr_sub_list()->add_tail(pSub);

            pSinthSub = (C_SINTH_CLASS *) pSinthCls->ptr_sub_list()->get_next();
         }
      }

      pSinthCls = (C_SINTH_CLASS *) SinthClassList.get_next();
   }

   return GS_GOOD;
}

  
/*********************************************************/
/*.doc global_count_class <external> */
/*+
  Questa funzione restituisce il numero totale di classi del progetto
    
  Restituisce un numero >= 0 in caso di successo altrimenti restituisce -1.
-*/  
/*********************************************************/
long C_PROJECT::global_count_class()
{
   C_STRING       TableRef, Condition(_T("SUB_GS_ID"));
   C_DBCONNECTION *pConn;

   if (getClassesTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   if (gsc_AdjSyntax(Condition, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return NULL;
   Condition += _T("=0");

   // conto le righe della tabella di GS_CLASS
   return pConn->RowsQty(TableRef.get_name(), Condition.get_name());
}
       

/*********************************************************/
/*.doc C_PROJECT::gsc_get_group <external> */
/*+
  Questa funzione restituisce l'elenco delle classi
  gruppo visibili all'utente corrente.
  Per ciascuna classe viene anche specificata una sottolista con
  le classi di cui è composta e quante per ciascuna classe.
  Lista : ( (<cls> ((<cls><quante>)(<cls><quante>)...)...)...)
  Parametri:
  C_BIRELATION_LIST *out;   Lista in output
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::get_group_list(C_BIRELATION_LIST *out)
{
   C_STRING       statement, TableRef, Field(_T("CLASS_ID"));
   _RecordsetPtr  pRs;
   C_RB_LIST	   ColValues;
   C_INT_INT      *new_rel; 
   C_INT_INT_LIST usr_listclass;           // lista classi utente
   int            cod_class, lnk_class, quante, prev_cod = 0;
   C_BIRELATION   *new_node;
   C_DBCONNECTION *pDBConn;

   if (out == NULL) { GS_ERR_COD = eGSNotAllocVar; return GS_BAD; }

   out->remove_all();

   if (getGrpsTabInfo(&pDBConn, &TableRef) == NULL) return GS_BAD;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   if (gsc_AdjSyntax(Field, pConn->get_InitQuotedIdentifier(), pConn->get_FinalQuotedIdentifier(),
                     pConn->get_StrCaseFullTableRef()) == GS_BAD)
      return NULL;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" ORDER BY ");
   statement += Field;
      
   // leggo le righe della tabella senza bloccarla
   if (pDBConn->OpenRecSet(statement, pRs) == GS_BAD) return GS_BAD;

   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      // Codice classe
      if (gsc_rb2Int(ColValues.CdrAssoc(_T("CLASS_ID")), &cod_class) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      // Codice classe membro
      if (gsc_rb2Int(ColValues.CdrAssoc(_T("LNK_CL_ID")), &lnk_class) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      // N. entità membro
      if (gsc_rb2Int(ColValues.CdrAssoc(_T("QTY")), &quante) == GS_BAD)
         { gsc_DBCloseRs(pRs); return GS_BAD; }

      if (prev_cod != cod_class) // nuovo gruppo
      {
         prev_cod = cod_class;
         if ((new_node = new C_BIRELATION) == NULL)
            { gsc_DBCloseRs(pRs); return GS_BAD; }
         out->add_tail(new_node);
         new_node->set_key(cod_class);
      }
      if ((new_rel = new C_INT_INT) == NULL)
         { gsc_DBCloseRs(pRs); return GS_BAD; }
      new_node->relation.add_tail(new_rel);
      new_rel->set_key(lnk_class);
      new_rel->set_type(quante);

      gsc_Skip(pRs);
   }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;

   return GS_GOOD;   
}
   
  
/*********************************************************/
/*.doc C_PROJECT::Lock <internal> */
/*+
  Questa funzione blocca la riga del progetto in GS_PRJ.
  Parametri:
  _RecordsetPtr &pRs;   Recordset di lettura
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_PROJECT::Lock(_RecordsetPtr &pRs)
{   
   C_STRING       TableRef, statement;
   C_DBCONNECTION *pConn;

   if (gsc_getPrjTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT * FROM ");
   statement += TableRef;
   statement += _T(" WHERE GS_ID=");
   statement += gs_id;

   pConn->BeginTrans();
   if (pConn->OpenRecSet(statement, pRs, adOpenDynamic, adLockPessimistic) == GS_BAD)
   {
      if (GS_ERR_COD == eGSLocked) GS_ERR_COD = eGSPrjInUse;
      pConn->RollbackTrans();
      return GS_BAD;
   }
   if (gsc_isEOF(pRs) == GS_GOOD)
   {
      gsc_DBCloseRs(pRs);
      GS_ERR_COD = eGSInvalidPrjCode;
      pConn->RollbackTrans();
      return GS_BAD;
   }

   return gsc_DBLockRs(pRs);
}



/*********************************************************/
/*.doc C_PROJECT::Unlock                  <internal> */
/*+
  Questa funzione sblocca la riga del progetto in GS_PRJ.
  Chiude una transazione aperta con "Lock" e chiude anche il recordset.
  Parametri:
  _RecordsetPtr &pRs;   Recordset di lettura
  int           Mode;   Flag se = UPDATEABLE viene aggiornato il 
                        recordset altrimenti vengono abbandonate
                        le modifiche (default = UPDATEABLE)
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
int C_PROJECT::Unlock(_RecordsetPtr &pRs, int Mode)
{
   C_DBCONNECTION *pDBConn;

   if (gsc_getPrjTabInfo(&pDBConn) == GS_BAD) return GS_BAD;

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
/*.doc C_PROJECT::is_free <external> */
/*+
  Questa funzione restituisce se il progetto e' libero oppure se detiene
  qualche sessione di lavoro.
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::is_free()
{  
   C_STRING       TableRef;
   C_DBCONNECTION *pDBConn;
   
   if (getWrkSessionsTabInfo(&pDBConn, &TableRef) == NULL) return GS_BAD;

   // conto le righe della tabella
   return (pDBConn->RowsQty(TableRef.get_name()) > 0) ? GS_BAD : GS_GOOD;
}             


/*********************************************************/
/*.doc C_PROJECT::reindex <internal> */
/*+
  Questa funzione reindicizza e controlla le tabelle del progetto
  e, se non ci sono, le ricrea.
  Parametri:
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::reindex()
{
   C_STRING       TableRef, IndexRef, Catalog;
   presbuf        IndexList;
   C_DBCONNECTION *pDBConn;

   // setto il catalogo delle tabelle memorizzate in GEOPRJDB
   // lo schema non è usato
   Catalog = dir;
   Catalog += _T('\\');
   Catalog += ACCESSGEOMAINDB;

   if (getDBConnection() == NULL) return GS_BAD;

   // (se fallisce devo comunque continuare)

   // aggiorno gli indici di GS_CLASS
   if (getClassesTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   pDBConn->Reindex(TableRef.get_name());

   // aggiorno gli indici di GS_CLASS_GRAPH
   if (getGraphClassesTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   pDBConn->Reindex(TableRef.get_name());

   // aggiorno gli indici di GS_SEC
   if (getSecsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   pDBConn->Reindex(TableRef.get_name());

   // aggiorno gli indici di GS_CONNECT
   if (getConnectionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   pDBConn->Reindex(TableRef.get_name());
  
   // aggiorno gli indici di GS_ATTR
   if (getAttribsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   pDBConn->Reindex(TableRef.get_name());
  
   // aggiorno gli indici di GS_ATTR_GRAPH
   if (getGraphAttribsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   pDBConn->Reindex(TableRef.get_name());

   // aggiorno gli indici di GS_GROUP
   if (getGrpsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   pDBConn->Reindex(TableRef.get_name());

   // aggiorno gli indici di GS_AREA
   if (getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   pDBConn->Reindex(TableRef.get_name());

   // aggiorno gli indici di GS_LOCK
   if (getLocksTabInfo(&pDBConn, &TableRef) == GS_BAD) return GS_BAD;
   acutPrintf(_T("%s %s..."), gsc_msg(348), TableRef.get_name());  // "\nReindicizzazione"
   // memorizzo gli indici utilizzati dalla tabella originale
   if (pDBConn->getTableIndexes(TableRef.get_name(), &IndexList) == GS_GOOD && IndexList)
   {
      C_WRK_SESSION_LIST SessionList;
      presbuf IndexListToDrop; // i nomi degli indici possono avere una sintassi diversa 
                               // a seconda che li si voglia creare o cancellare

      pDBConn->getTableIndexes(TableRef.get_name(), &IndexListToDrop, ERASE);
      pDBConn->DelIndex(TableRef.get_name(), IndexListToDrop);
      acutRelRb(IndexListToDrop);

      if (SessionList.rarea(get_key()) == GS_GOOD && SessionList.get_count() == 0)
         // se NON ci sono sessioni di lavoro pulisco GS_LOCK
         pDBConn->DelRows(TableRef.get_name());

      pDBConn->CreateIndex(IndexList, TableRef.get_name());
      acutRelRb(IndexList);
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::ClassesStructReport         <external> */
/*+
  Questa funzione esporta su file HTML la struttura di una lista di classi
  del progetto.
  Parametri:
  const TCHAR *Path;        Path completa del file
  const TCHAR *FileMode;    Modalità di apertura del file (append, create...)
  C_INT_LIST &ClsCodsList;  Lista dei codici delle classi da elaborare
  bool SynthMode;           Opzionale. Flag di modalità di report.
                            Se = true attiva il report sintetico (default = false)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::ClassesStructReport(const TCHAR *Path, const TCHAR *FileMode,
                                   C_INT_LIST &ClsCodeList, bool SynthMode)
{
   C_INT   *pClsCode = (C_INT *) ClsCodeList.get_head();
   C_CLASS *pCls;
   FILE    *file;

   if ((file = gsc_fopen(Path, FileMode)) == NULL) return GS_BAD;

   // Se il file è da creare
   if (gsc_strcmp(FileMode, _T("w")) == 0 || gsc_strcmp(FileMode, _T("w+")) == 0)
   {  // Intestazione
      if (fwprintf(file, _T("<html>\n<head>\n<title>%s</title>\n</head>\n<body bgcolor=\"#FFFFFF\">\n"),
                  gsc_msg(383)) < 0) // "Modello dei dati di GEOsim"
         { GS_ERR_COD = eGSWriteFile; return GS_BAD; }
   }

   while (pClsCode)
   {
      if ((pCls = find_class(pClsCode->get_key(), 0)) == NULL)
         { gsc_fclose(file); return GS_BAD; }
      if (pCls->reportHTML(file, Path, SynthMode) == GS_BAD) 
         { gsc_fclose(file); return GS_BAD; }

      pClsCode = (C_INT *) ClsCodeList.get_next();
   }

   if (gsc_fclose(file) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*********************************************************/
/*.doc C_PROJECT::setNextClassId              <internal> */
/*+
  Questa funzione scrive nella tabella GS_PRJ il prossimo codice
  da usare per la creazione di una classe di entità.
  Parametri:
  int NextId;        Prossimo codice classe
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::setNextClassId(int NextId)
{
   C_DBCONNECTION *pPrjListDBConn;
   C_STRING       TableRef, statement;
   C_STR_LIST     FieldList;

   if (gsc_getPrjTabInfo(&pPrjListDBConn, &TableRef) == GS_BAD) return GS_BAD;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("NEXT_CLASS_ID")); // 1
   FieldList.add_tail_str(_T("GS_ID"));         // 2
   if (gsc_AdjSyntax(FieldList, pPrjListDBConn) == GS_BAD) return GS_BAD;

   statement = _T("UPDATE ");
   statement += TableRef;
   statement += _T(" SET ");
   statement += FieldList.goto_pos(1)->get_name(); // NEXT_CLASS_ID
   statement += _T("=");
   statement += NextId;
   statement += _T(" WHERE ");
   statement += FieldList.goto_pos(2)->get_name(); // GS_ID
   statement += _T("=");
   statement += gs_id;

   return pPrjListDBConn->ExeCmd(statement);
}


/*********************************************************/
/*.doc C_PROJECT::getNextClassId              <internal> */
/*+
  Questa funzione legge nella tabella GS_PRJ il prossimo codice
  da usare per la creazione di una classe di entità e lo verifica 
  (ed eventualmente lo corregge) leggendo dalla tabella GS_CLASS.
  Parametri:
    
  Restituisce il codice in caso di successo altrimenti restituisce 0.
-*/  
/*********************************************************/
int C_PROJECT::getNextClassId(void)
{
   C_DBCONNECTION *pDBConn;
   C_STRING       TableRef, statement;
   C_STR_LIST     FieldList;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        pRb;
   int            NextId = 0, Result;
   double         CurrentMaxId;

   if (gsc_getPrjTabInfo(&pDBConn, &TableRef) == GS_BAD) return 0;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("NEXT_CLASS_ID")); // 1
   FieldList.add_tail_str(_T("GS_ID"));         // 2
   if (gsc_AdjSyntax(FieldList, pDBConn) == GS_BAD) return GS_BAD;

   statement = _T("SELECT ");
   statement += FieldList.goto_pos(1)->get_name(); // NEXT_CLASS_ID
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += FieldList.goto_pos(2)->get_name(); // GS_ID
   statement += _T("=");
   statement += gs_id;

   // leggo la riga della tabella senza bloccarla
   if (pDBConn->OpenRecSet(statement, pRs) == GS_BAD) return 0;
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return 0; }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return 0;
   if ((pRb = ColValues.CdrAssoc(_T("NEXT_CLASS_ID"))) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return 0; }
   if (gsc_rb2Int(pRb, &NextId) == GS_BAD || NextId == 0) NextId = 1;

   // Leggo il GS_ID maggiore da GS_CLASS
   if (getClassesTabInfo(&pDBConn, &TableRef) == GS_BAD) return 0;
   if ((Result = pConn->GetNumAggrValue(TableRef.get_name(), _T("GS_ID"), _T("MAX"), &CurrentMaxId)) == GS_BAD)
      return 0;
   if (Result == GS_CAN) CurrentMaxId = 0; // tabella senza righe
   CurrentMaxId++; // Lo incremento di 1

   return (int) gsc_max(CurrentMaxId, NextId);
}


/*********************************************************/
/*.doc C_PROJECT::setNextWrkSessionId              <internal> */
/*+
  Questa funzione scrive nella tabella GS_PRJ il prossimo codice
  da usare per la creazione di una sessione di lavoro.
  Parametri:
  int NextId;        Prossimo codice sessione di lavoro
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::setNextWrkSessionId(long NextId)
{
   C_DBCONNECTION *pPrjListDBConn;
   C_STRING       TableRef, statement;
   C_STR_LIST     FieldList;

   if (gsc_getPrjTabInfo(&pPrjListDBConn, &TableRef) == GS_BAD) return GS_BAD;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("NEXT_WRK_SESSION_ID")); // 1
   FieldList.add_tail_str(_T("GS_ID"));         // 2
   if (gsc_AdjSyntax(FieldList, pPrjListDBConn) == GS_BAD) return GS_BAD;

   statement = _T("UPDATE ");
   statement += TableRef;
   statement += _T(" SET ");
   statement += FieldList.goto_pos(1)->get_name(); // NEXT_WRK_SESSION_ID
   statement += _T("=");
   statement += NextId;
   statement += _T(" WHERE ");
   statement += FieldList.goto_pos(2)->get_name(); // GS_ID
   statement += _T("=");
   statement += gs_id;

   return pPrjListDBConn->ExeCmd(statement);
}


/*********************************************************/
/*.doc C_PROJECT::getNextWrkSessionId         <internal> */
/*+
  Questa funzione legge nella tabella GS_PRJ il prossimo codice
  da usare per la creazione di una sessione di lavoro e lo verifica 
  (ed eventualmente lo corregge) leggendo dalla tabella GS_WRSESSION.
  Parametri:
    
  Restituisce il codice in caso di successo altrimenti restituisce 0.
-*/  
/*********************************************************/
long C_PROJECT::getNextWrkSessionId(void)
{
   C_DBCONNECTION *pDBConn;
   C_STRING       TableRef, statement;
   C_STR_LIST     FieldList;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        pRb;
   int            Result;
   long           NextId = 0;
   double         CurrentMaxId;

   if (gsc_getPrjTabInfo(&pDBConn, &TableRef) == GS_BAD) return 0;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("NEXT_WRK_SESSION_ID")); // 1
   FieldList.add_tail_str(_T("GS_ID"));         // 2
   if (gsc_AdjSyntax(FieldList, pDBConn) == GS_BAD) return GS_BAD;

   statement = _T("SELECT ");
   statement += FieldList.goto_pos(1)->get_name(); // NEXT_WRK_SESSION_ID
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += FieldList.goto_pos(2)->get_name(); // GS_ID
   statement += _T("=");
   statement += gs_id;

   // leggo la riga della tabella senza bloccarla
   if (pDBConn->OpenRecSet(statement, pRs) == GS_BAD) return 0;
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return 0; }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return 0;
   if ((pRb = ColValues.CdrAssoc(_T("NEXT_WRK_SESSION_ID"))) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return 0; }
   if (gsc_rb2Lng(pRb, &NextId) == GS_BAD || NextId == 0) NextId = 1;

   // Leggo il GS_ID maggiore da GS_WRKSESSION
   if (getWrkSessionsTabInfo(&pDBConn, &TableRef) == GS_BAD) return 0;
   if ((Result = pConn->GetNumAggrValue(TableRef.get_name(), _T("GS_ID"), _T("MAX"), &CurrentMaxId)) == GS_BAD)
      return 0;
   if (Result == GS_CAN) CurrentMaxId = 0; // tabella senza righe
   CurrentMaxId++; // Lo incremento di 1

   return (long) gsc_max(CurrentMaxId, NextId);
}


/*********************************************************/
/*.doc C_PROJECT::setNextClassSetId           <internal> */
/*+
  Questa funzione scrive nella tabella GS_PRJ il prossimo codice
  da usare per la creazione di un set di classi di entità.
  Parametri:
  int NextId;        Prossimo codice set di classi
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::setNextClassSetId(int NextId)
{
   C_DBCONNECTION *pPrjListDBConn;
   C_STRING       TableRef, statement;
   C_STR_LIST     FieldList;

   if (gsc_getPrjTabInfo(&pPrjListDBConn, &TableRef) == GS_BAD) return GS_BAD;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("NEXT_SET_CLASS_ID")); // 1
   FieldList.add_tail_str(_T("GS_ID"));             // 2
   if (gsc_AdjSyntax(FieldList, pPrjListDBConn) == GS_BAD) return GS_BAD;

   statement = _T("UPDATE ");
   statement += TableRef;
   statement += _T(" SET ");
   statement += FieldList.goto_pos(1)->get_name(); // NEXT_CLASS_ID
   statement += _T("=");
   statement += NextId;
   statement += _T(" WHERE ");
   statement += FieldList.goto_pos(2)->get_name(); // GS_ID
   statement += _T("=");
   statement += gs_id;

   return pPrjListDBConn->ExeCmd(statement);
}


/************************************************************/
/*.doc C_PROJECT::getNextClassSetId              <internal> */
/*+
  Questa funzione legge nella tabella GS_PRJ il prossimo codice
  da usare per la creazione di un set di classi di entità e lo verifica 
  (ed eventualmente lo corregge) leggendo dalla tabella GS_CLASS.
  Parametri:
    
  Restituisce il codice in caso di successo altrimenti restituisce 0.
-*/  
/************************************************************/
int C_PROJECT::getNextClassSetId(void)
{
   C_DBCONNECTION *pDBConn;
   C_STRING       TableRef, statement;
   C_STR_LIST     FieldList;
   _RecordsetPtr  pRs;
   C_RB_LIST      ColValues;
   presbuf        pRb;
   int            NextId = 0, Result;
   double         CurrentMaxId;

   if (gsc_getPrjTabInfo(&pDBConn, &TableRef) == GS_BAD) return 0;

   // Correggo la sintassi dei nomi dei campi a seconda della connessione OLE-DB
   FieldList.add_tail_str(_T("NEXT_SET_CLASS_ID")); // 1
   FieldList.add_tail_str(_T("GS_ID"));         // 2
   if (gsc_AdjSyntax(FieldList, pDBConn) == GS_BAD) return GS_BAD;

   statement = _T("SELECT ");
   statement += FieldList.goto_pos(1)->get_name(); // NEXT_CLASS_ID
   statement += _T(" FROM ");
   statement += TableRef;
   statement += _T(" WHERE ");
   statement += FieldList.goto_pos(2)->get_name(); // GS_ID
   statement += _T("=");
   statement += gs_id;

   // leggo la riga della tabella senza bloccarla
   if (pDBConn->OpenRecSet(statement, pRs) == GS_BAD) return 0;
   if (gsc_DBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return 0; }
   if (gsc_DBCloseRs(pRs) == GS_BAD) return 0;
   if ((pRb = ColValues.CdrAssoc(_T("NEXT_SET_CLASS_ID"))) == NULL)
      { GS_ERR_COD = eGSInvalidArg; return 0; }
   if (gsc_rb2Int(pRb, &NextId) == GS_BAD || NextId == 0) NextId = 1;

   // Leggo il GS_ID maggiore da GS_CLASS_SET
   if (getClassSetTabInfo(&pDBConn, &TableRef) == GS_BAD) return 0;
   if ((Result = pConn->GetNumAggrValue(TableRef.get_name(), _T("GS_ID"), _T("MAX"), &CurrentMaxId)) == GS_BAD)
      return 0;
   if (Result == GS_CAN) CurrentMaxId = 0; // tabella senza righe
   CurrentMaxId++; // Lo incremento di 1

   return (int) gsc_max(CurrentMaxId, NextId);
}


/*********************************************************/
/*.doc gsc_getClassCodeListtoScriptStr        <internal> */
/*+
  Questa funzione restituisce una stringa che contiene le istruzioni
  lisp per costruire una lista con codici di classi. Poiche AutoCAD
  non riesce a leggere una lista molto lunga si è costretti a frazionare
  la lista in sottoliste da max 50 elementi e aggiungerle poi con una 
  istruzione "append" alla lista principale.
  Parametri:
  C_INT_LIST *pClsCodeList;  Lista dei codici delle classi
  const TCHAR *ListName;     Nome da utilizzare per la lista

  N.B. Alloca memoria
-*/  
/*********************************************************/
static TCHAR* gsc_getClassCodeListtoScriptStr(C_INT_LIST *pClsCodeList, const TCHAR *ListName)
{
   C_INT    *pClass;
   C_STRING Buffer;
   int      ListLen = 0;

   Buffer = _T("(setq ");
   Buffer += ListName;
   Buffer += _T(" (list ");
   // Considero solo i primi 50 codici
   pClass = (C_INT *) pClsCodeList->get_head();
   while (pClass && ListLen < 50)
   {
      Buffer += pClass->get_key();
      pClass = (C_INT *) pClass->get_next();
      if (pClass) Buffer += _T(' ');
      ListLen++;
   }
   Buffer += _T("))\n");

   if (pClass)
   {
      Buffer += _T("(setq ");
      Buffer += ListName;
      Buffer += _T(" (append ");
      Buffer += ListName;
      Buffer += _T(" (list ");
      // se la lista contiene troppi codici Autocad non riesce a leggerla
      // quindi la spezzo in liste da 50 elementi
      ListLen = 0;
      while (pClass)
      {
         if (ListLen > 0 && ListLen % 25 == 0)
         {
            Buffer += _T(")))\n(setq ");
            Buffer += ListName;
            Buffer += _T(" (append ");
            Buffer += ListName;
            Buffer += _T(" (list ");
            ListLen = 0;
         }
         Buffer += pClass->get_key();
         pClass = (C_INT *) pClass->get_next();
         if (pClass) Buffer += _T(' ');
         ListLen++;
      }
      Buffer += _T(")))\n");
   }

   return Buffer.cut();
}


/*********************************************************/
/*.doc C_PROJECT::ClassCheck                  <internal> */
/*+
  Questa funzione controlla una lista di classi del progetto.
  Parametri:
  C_INT_LIST *pClsCodeList; Lista dei codici delle classi da controllare
  int WhatOperation;        Tipo di controllo da eseguire:
                            reindicizzazione (REINDEX)
                            sblocco degli oggetti bloccati (UNLOCKOBJS)
                            creazione file INF di estensione dei disegni (SAVEEXTENTS)
                            compattazione (PURGE)
                            recupero disegni (RECOVER)
  const TCHAR *ScriptFile;  Se = NULL indica che si deve eseguire direttamente la funzione
                            altrimenti deve essere scritto un file script (con path completa
                            indicata dal parametro) contenente le istruzioni per eseguire 
                            la funzione in modalità batch (default = NULL).
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::ClassCheck(C_INT_LIST *pClsCodeList, int WhatOperation,
                          const TCHAR *ScriptFile)
{
   C_INT    *pClass;
   C_CLASS  *pCls;
   int      LockResult, unlocked, Result;
   int      WhatToDo = WhatOperation;
   C_STRING strList;
   bool     isCurrDWGChanged, isDWGChangedOnStartup;

   if (pClsCodeList == NULL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   if (WhatOperation == 0) return GS_GOOD;

   // Serve sapere se il DWG corrente, dopo il NEW o l'OPEN risulta modificato
   isDWGChangedOnStartup = isCurrDWGChanged = gsc_isCurrentDWGModified();

   if (ScriptFile && gsc_strlen(ScriptFile) > 0)
   {  // Deve essere scritto un file script
      FILE       *f = NULL;
      C_DWG_LIST DwgList;
      C_DWG      *pDwg = NULL;

      if (gsc_path_exist(ScriptFile) == GS_GOOD)
         if (gsc_delfile(ScriptFile) == GS_BAD) return GS_BAD;

      if ((f = gsc_fopen(ScriptFile, _T("w"))) == NULL) return GS_BAD;

      // Non più valido perchè in fase di login prima venivano settate 
      // alcune variabili d'ambiente (DIMASSOC) che modificavano il dwg corrente, ora non più.
      //isCurrDWGChanged = true;
      // Se si deve fare un'operazione tipo PURGE o RECOVER
      if (WhatOperation & PURGE ||
          WhatOperation & RECOVER)
      {
         pClass = (C_INT *) pClsCodeList->get_head();
         while (pClass)
         {
            // inizializzo la lista dei dwg della classe corrente
            if (DwgList.load(get_key(), pClass->get_key()) == GS_GOOD)
            {
               pDwg = (C_DWG *) DwgList.get_head();
               while (pDwg)
               {              
                  if (isCurrDWGChanged) // disegno modificato
                  {
                     // se devo fare il RECOVER
                     if (WhatOperation & RECOVER)
                     {  // QAFLAGS = 2 fa sì che recover non chieda di premere invio se il dwg è corretto
                        if (fwprintf(f, _T("QAFLAGS\n2\n_.RECOVER\n_Y\n\"%s\"\nQAFLAGS\n0\n"), pDwg->get_name()) < 0)
                           { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                     }
                     else // se devo fare il PURGE
                     {
                        if (fwprintf(f, _T("_.OPEN\n_Y\n\"%s\"\n"), pDwg->get_name()) < 0)
                           { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                     }
                  }
                  else
                     // se devo fare il RECOVER
                     if (WhatOperation & RECOVER)
                     {  // QAFLAGS = 2 fa sì che recover non chieda di premere invio se il dwg è corretto
                        if (fwprintf(f, _T("QAFLAGS\n2\n_.RECOVER\n\"%s\"\nQAFLAGS\n0\n"), pDwg->get_name()) < 0)
                           { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                     }
                     else // se devo fare il PURGE
                        if (fwprintf(f, _T("_.OPEN\n\"%s\"\n"), pDwg->get_name()) < 0)
                           { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      
                  // Poichè il comando "_.RECOVER" e "_.OPEN" forzano il caricamento dei tematismi di geosim
                  // la sessione di AutoCAD risulta modificata
                  // Non più valido perchè in fase di login prima venivano settate 
                  // alcune variabili d'ambiente (DIMASSOC) che modificavano il dwg corrente, ora non più.
                  //isCurrDWGChanged = true;

                  // se devo fare il PURGE
                  if (WhatOperation & PURGE)
                     if (gsc_prepareScriptForPurgeAll(f) == GS_BAD) // purge ALL
                        { gsc_fclose(f); return GS_BAD; }

                  // Salva senza salvataggio incrementale
                  if (fwprintf(f, _T("(SETQ svprcnt (GETVAR \"ISAVEPERCENT\"))\n(SETVAR \"ISAVEPERCENT\" 0)")) < 0 ||
                      fwprintf(f, _T("\n_.SAVE\n\"%s\"\n"), pDwg->get_name()) < 0 ||
                      fwprintf(f, _T("(SETVAR \"ISAVEPERCENT\" svprcnt)\n")) < 0)
                     { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                  isCurrDWGChanged = false;

                  pDwg = (C_DWG *) DwgList.get_next();
               }

               if (isCurrDWGChanged) // se il disegno corrente è stato modificato 
               {                     // rispondo Yes a "Si desidera eliminare tutte le modifiche del disegno?"
                  if (fwprintf(f, _T("_.NEW\n_Y\n\n")) < 0)
                     { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
               }
               else
                  if (fwprintf(f, _T("_.NEW\n\n")) < 0)
                     { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

               // Poichè il comando "_.NEW" forza il caricamento dei tematismi di geosim
               // la sessione di AutoCAD risulta modificata
               // Non più valido perchè in fase di login prima venivano settate 
               // alcune variabili d'ambiente (DIMASSOC) che modificavano il dwg corrente, ora non più.
               isCurrDWGChanged = isDWGChangedOnStartup;
            }
            
            if (WhatToDo & PURGE) WhatToDo -= PURGE;
            if (WhatToDo & RECOVER) WhatToDo -= RECOVER;

            // Se c'è ancora qualcosa da fare oltre il PURGE e il RECOVER
            if (WhatToDo > 0)
               if (fwprintf(f, _T("(gs_checkclass %d (list %d) %d)\n"),
                            get_key(), 
                            pClass->get_key(), 
                            WhatToDo) < 0)
                  { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
         
            pClass = (C_INT *) pClass->get_next();
         }
      }
      else
      {
         // se la lista contiene troppi codici Autocad non riesce a leggerla...
         strList.paste(gsc_getClassCodeListtoScriptStr(pClsCodeList, _T("ClsCodeList")));
         if (fwprintf(f, strList.get_name()) < 0)
            { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; } 

         if (fwprintf(f, _T("(gs_checkclass %d ClsCodeList %d)\n"),
                      get_key(), 
                      WhatOperation) < 0)
            { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      }

      if (gsc_fclose(f) == GS_BAD) return GS_BAD;

      return GS_GOOD;
   }

   // l'operazione di "PURGE" e/o di "RECOVER" viene fatta per prima con uno script,
   if (WhatOperation & PURGE ||
       WhatOperation & RECOVER)
   {
      C_STRING   Path;
      C_DWG_LIST DwgList;
      FILE       *f = NULL;
      C_DWG      *pDwg = NULL;

      // Non possono esserci sessioni di lavoro attive
      if (GS_CURRENT_WRK_SESSION) { GS_ERR_COD = eGSSessionsFound; return GS_BAD; }

      // scrivo file script
      Path = GEOsimAppl::CURRUSRDIR;
      Path += _T('\\');
      Path += GEOTEMPDIR;
      Path += _T('\\');
      Path += GS_SCRIPT;
      if ((f = gsc_fopen(Path, _T("w"))) == NULL) return GS_BAD;

      pClass = (C_INT *) pClsCodeList->get_head();
      while (pClass)
      {
         if ((pCls = find_class(pClass->get_key(), 0)) && 
             (pCls->ptr_fas() || pCls->get_category() == CAT_EXTERN))
         {
            // bisogna forzare la classe in uso esclusivo usando il codice utente corrente * -1
            if (pCls->set_exclusive_use(GEOsimAppl::GS_USER.code * -1, &LockResult) == GS_GOOD &&
                                        LockResult == GS_GOOD)
            {
               // inizializzo la lista dei dwg della classe corrente
               if (DwgList.load(get_key(), pClass->get_key()) == GS_GOOD)
               {
                  // Sblocca tutti i DWG (cancella i *.DWK, *.DWL?, *.MAP).
                  if (pCls->DWGsUnlock(&unlocked) == GS_GOOD)
                  {
                     pDwg = (C_DWG *) DwgList.get_head();
                     while (pDwg)
                     {
                        if (isCurrDWGChanged) // disegno modificato
                        {
                           // se devo fare il RECOVER
                           if (WhatOperation & RECOVER)
                           {  // QAFLAGS = 2 fa sì che recover non chieda di premere invio se il dwg è corretto
                              if (fwprintf(f, _T("QAFLAGS\n2\n_.RECOVER\n_Y\n\"%s\"\nQAFLAGS\n0\n"), pDwg->get_name()) < 0)
                                 { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                           }
                           else // se devo fare il PURGE
                           {
                              if (fwprintf(f, _T("_.OPEN\n_Y\n\"%s\"\n"), pDwg->get_name()) < 0)
                                 { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                           }
                        }
                        else
                           // se devo fare il RECOVER
                           if (WhatOperation & RECOVER)
                           {  // QAFLAGS = 2 fa sì che recover non chieda di premere invio se il dwg è corretto
                              if (fwprintf(f, _T("QAFLAGS\n2\n_.RECOVER\n\"%s\"\nQAFLAGS\n0\n"), pDwg->get_name()) < 0)
                                 { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                           }
                           else // se devo fare il PURGE
                              if (fwprintf(f, _T("_.OPEN\n\"%s\"\n"), pDwg->get_name()) < 0)
                                 { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

                        // Poichè il comando "_.RECOVER" e "_.OPEN" forzano il caricamento dei tematismi di geosim
                        // la sessione di AutoCAD risulta modificata
                        // Non più valido perchè in fase di login prima venivano settate 
                        // alcune variabili d'ambiente (DIMASSOC) che modificavano il dwg corrente, ora non più.
                        //isCurrDWGChanged = true;

                        // se devo fare il PURGE
                        if (WhatOperation & PURGE)
                           if (gsc_prepareScriptForPurgeAll(f) == GS_BAD) // purge ALL
                              { gsc_fclose(f); return GS_BAD; }

                        // Salva senza salvataggio incrementale
                        if (fwprintf(f, _T("(SETQ svprcnt (GETVAR \"ISAVEPERCENT\"))\n(SETVAR \"ISAVEPERCENT\" 0)")) < 0 ||
                            fwprintf(f, _T("\n_.SAVE\n\"%s\"\n"), pDwg->get_name()) < 0 ||
                            fwprintf(f, _T("(SETVAR \"ISAVEPERCENT\" svprcnt)\n")) < 0)
                           { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                        isCurrDWGChanged = false;

                        pDwg = (C_DWG *) DwgList.get_next();
                     }
                  }
               
                  if (isCurrDWGChanged) // se il disegno corrente è stato modificato 
                  {                     // rispondo Yes a "Si desidera eliminare tutte le modifiche del disegno?"
                     if (fwprintf(f, _T("_.NEW\n_Y\n\n")) < 0)
                        { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
                  }
                  else
                     if (fwprintf(f, _T("_.NEW\n\n")) < 0)
                        { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

                  // Poichè il comando "_.NEW" forza il caricamento dei tematismi di geosim
                  // la sessione di AutoCAD risulta modificata
                  // Non più valido perchè in fase di login prima venivano settate 
                  // alcune variabili d'ambiente (DIMASSOC) che modificavano il dwg corrente, ora non più.
                  isCurrDWGChanged = isDWGChangedOnStartup;

                  // Unlock class
                  if (fwprintf(f, _T("(gs_share_class %d %d)\n"), get_key(), pClass->get_key()) < 0)
                     { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
               }
            }
         }

         pClass = (C_INT *) pClsCodeList->get_next();
         continue;
      }

      if (isCurrDWGChanged) // se il disegno corrente è stato modificato 
      {                     // (es. un NEW che carica GEOsim e tematismi vari)
                            // rispondo Yes a "Si desidera eliminare tutte le modifiche del disegno?"
         if (fwprintf(f, _T("_.NEW\n_Y\n\n")) < 0)
            { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }
      }
      else
         if (fwprintf(f, _T("_.NEW\n\n")) < 0)
            { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

      strList.paste(gsc_getClassCodeListtoScriptStr(pClsCodeList, _T("ClsCodeList")));
      if (fwprintf(f, strList.get_name()) < 0)
         { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; } 

      if (WhatToDo & PURGE) WhatToDo -= PURGE;
      if (WhatToDo & RECOVER) WhatToDo -= RECOVER;

      if (fwprintf(f, _T("(gs_checkclass %d ClsCodeList %d)\n"),
                   get_key(), 
                   WhatToDo) < 0)
         { gsc_fclose(f); GS_ERR_COD = eGSWriteFile; return GS_BAD; }

      if (gsc_fclose(f) == GS_BAD) return GS_BAD;

      // Avvio la compressione
      return gsc_callCmd(_T("_.SCRIPT"), RTSTR, Path.get_name(), 0);
   }

   Result = GS_GOOD;
   pClass = (C_INT *) pClsCodeList->get_head();
   while (pClass)
   {
      if ((pCls = find_class(pClass->get_key(), 0)) == NULL) return GS_BAD;

      // bisogna forzare la classe in uso esclusivo usando il codice utente corrente * -1
      if (pCls->set_exclusive_use(GEOsimAppl::GS_USER.code * -1, &LockResult) == GS_BAD ||
                                  LockResult == GS_BAD)
      {
         acutPrintf(gsc_msg(612), pCls->get_name()); // "\nControllo dati classe %s fallita."
         acutPrintf(_T("\n%s"), gsc_err(eGSClassLocked)); // "*Errore* Classe bloccata"
         pClass = (C_INT *) pClsCodeList->get_next();
         continue;
      }

      if (pCls->ptr_info() &&        // se ha collegamento con database
          (WhatOperation & REINDEX)) // e si vuole reindicizzare
         if (pCls->reindexTab() == GS_BAD)
         {  // vado alla classe successiva
            pCls->set_share_use();
            pClass = (C_INT *) pClsCodeList->get_next();
            Result = GS_BAD;
            continue;
         }

      if (pCls->CheckGraphData(WhatOperation) == GS_BAD)
      {  // vado alla classe successiva
         pCls->set_share_use();
         pClass = (C_INT *) pClsCodeList->get_next();
         Result = GS_BAD;
         continue;
      }

      // sblocco la classe
      if (pCls->set_share_use() == GS_BAD) return GS_BAD;

      pClass = (C_INT *) pClsCodeList->get_next();
   }

   return Result;
}


/*********************************************************/
/*.doc C_PROJECT::ddSessionCheck                  <internal> */
/*+
  Questa funzione controlla se il progetto ha delle sessioni 
  rimaste "appese" (attesa, attive, salvataggio, estrazione)
  per quanto concerne l'utente corrente. Attraverso interfaccia DCL
  chiede conferma della cancellazione di tali sessioni e in
  caso affermativo le rimuove.
  Parametri:
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::ddSessionCheck(void)
{
   int         res = GS_GOOD;
   C_WRK_SESSION_LIST ActiveSessionList, WaitingSessionList;
   C_WRK_SESSION_LIST SavingSessionList, ExtractingSessionList;

   acutPrintf(gsc_msg(63), get_name()); // "\nControllo progetto <%s>..."

   if (ActiveSessionList.ruserarea(GEOsimAppl::GS_USER.code, get_key(), WRK_SESSION_ACTIVE, TRUE) == GS_BAD)
      res = GS_BAD;
   if (WaitingSessionList.ruserarea(GEOsimAppl::GS_USER.code, get_key(), WRK_SESSION_WAIT, TRUE) == GS_BAD)
      res = GS_BAD;
   if (SavingSessionList.ruserarea(GEOsimAppl::GS_USER.code, get_key(), WRK_SESSION_SAVE, TRUE) == GS_BAD)
      res = GS_BAD;
   if (ExtractingSessionList.ruserarea(GEOsimAppl::GS_USER.code, get_key(), WRK_SESSION_EXTR, TRUE) == GS_BAD)
      res = GS_BAD;

   if (ActiveSessionList.get_count() > 0 || WaitingSessionList.get_count() > 0 ||
       SavingSessionList.get_count() > 0 || ExtractingSessionList.get_count() > 0)
   {
      C_WRK_SESSION   *pSession;
      C_STRING Msg;
      int      result;

      Msg = gsc_msg(72); // "Nel progetto <"
      Msg += get_name();
      Msg += gsc_msg(156); // "> sono state trovate le seguenti sessioni di lavoro"
      Msg += GS_LFSTR;
      Msg += gsc_msg(393); // "di tua proprietà che sono rimaste in sospeso:"
      Msg += GS_LFSTR;
      pSession = (C_WRK_SESSION *) ActiveSessionList.get_head();
      while (pSession)
      {
         Msg += _T("\n- <");
         Msg += pSession->get_name();
         Msg += gsc_msg(171); // "> (attiva), creata il "
         Msg += pSession->get_CreationDateTime();
         pSession = (C_WRK_SESSION *) pSession->get_next();
      }
      pSession = (C_WRK_SESSION *) WaitingSessionList.get_head();
      while (pSession)
      {
         Msg += _T("\n- <");
         Msg += pSession->get_name();
         Msg += gsc_msg(182); // "> (in attesa di estrazione)"
         pSession = (C_WRK_SESSION *) pSession->get_next();
      }
      pSession = (C_WRK_SESSION *) SavingSessionList.get_head();
      while (pSession)
      {
         Msg += _T("\n- <");
         Msg += pSession->get_name();
         Msg += gsc_msg(185); // "> (in salvataggio)"
         pSession = (C_WRK_SESSION *) pSession->get_next();
      }
      pSession = (C_WRK_SESSION *) ExtractingSessionList.get_head();
      while (pSession)
      {
         Msg += _T("\n- <");
         Msg += pSession->get_name();
         Msg += gsc_msg(391); // "> (in estrazione)"
         pSession = (C_WRK_SESSION *) pSession->get_next();
      }

      Msg += GS_LFSTR;
      Msg += gsc_msg(392); // "\nConfermarne la cancellazione."
      if (gsc_ddgetconfirm(Msg.get_name(), &result, GS_GOOD, GS_BAD,
                           GS_BAD, GS_GOOD) == GS_GOOD && result == GS_GOOD)
      {
         // cancello le sessioni di lavoro                  
         pSession = (C_WRK_SESSION *) ActiveSessionList.get_head();
         while (pSession)
         {
            if (pSession->del((TCHAR *) GEOsimAppl::GS_USER.pwd) == GS_BAD) res = GS_BAD;
            pSession = (C_WRK_SESSION *) pSession->get_next();
         }
         pSession = (C_WRK_SESSION *) WaitingSessionList.get_head();
         while (pSession)
         {
            if (pSession->del((TCHAR *) GEOsimAppl::GS_USER.pwd) == GS_BAD) res = GS_BAD;
            pSession = (C_WRK_SESSION *) pSession->get_next();
         }
         pSession = (C_WRK_SESSION *) SavingSessionList.get_head();
         while (pSession)
         {
            if (pSession->del((TCHAR *) GEOsimAppl::GS_USER.pwd) == GS_BAD) res = GS_BAD;
            pSession = (C_WRK_SESSION *) pSession->get_next();
         }
         pSession = (C_WRK_SESSION *) ExtractingSessionList.get_head();
         while (pSession)
         {
            if (pSession->del((TCHAR *) GEOsimAppl::GS_USER.pwd) == GS_BAD) res = GS_BAD;
            pSession = (C_WRK_SESSION *) pSession->get_next();
         }
      }
   }

   // Se il progetto non appartiene a quello in uso nella sessione corrente
   if (!GS_CURRENT_WRK_SESSION || GS_CURRENT_WRK_SESSION->get_pPrj() != this)
      // La chiudo per evitare di raggiungere 64 connessioni OLE-DB
      GEOsimAppl::TerminateSQL(pConn);

   return res;
}


/*********************************************************/
/*.doc C_PROJECT::getSinthClsSecondaryTabList <external> */
/*+
  Questa funzione restituisce una lista sintetica delle tabelle secondarie di una classe del progetto.
  Parametri:
  int cls;                             codice classe
  int sub;                             codice sottoclasse
  C_SINTH_SEC_TAB_LIST &SinthSecList;  lista risultato
  int retest;                          se MORETESTS -> in caso di errore riprova n volte 
                                       con i tempi di attesa impostati poi ritorna GS_BAD,
                                       ONETEST -> in caso di errore ritorna GS_BAD senza riprovare
                                       (default = MORETESTS)
    
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_PROJECT::getSinthClsSecondaryTabList(int cls, int sub, C_SINTH_SEC_TAB_LIST &SinthSecList, int retest)
{
   C_DBCONNECTION  *pConn;
   C_STRING        statement, TableRef, name_sec, descr_sec, version_sec;
   C_STRING        keyPriAttrib_sec, keyAttrib_sec;
   C_STRING        realInitDistanceAttrib_sec, realFinalDistanceAttrib_sec;
   _RecordsetPtr   pRs;
   int             cod_sec, type_sec;
   C_RB_LIST       ColValues;
   presbuf         pKey, pName, pType, pDescr, pVersion;
   presbuf         pKeyPriAttrib, pKeyAttrib, pRealInitDistanceAttrib, pRealFinalDistanceAttrib;
   C_SINTH_SEC_TAB *pSinthSec;
   GSDataPermissionTypeEnum cod_level;
   C_4INT_STR_LIST usr_listSec;
   C_4INT_STR      *pusr_sec;
   
   SinthSecList.remove_all();

   // setto il riferimento di GS_SEC (<catalogo>.<schema>.<tabella>)
   if (getSecsTabInfo(&pConn, &TableRef) == GS_BAD) return GS_BAD;

   statement = _T("SELECT GS_ID,NAME,TYPE,DESCRIPTION,VERSION,KEY_PRI,KEY_ATTRIB,REAL_INIT_DISTANCE_ATTRIB,REAL_FINAL_DISTANCE_ATTRIB FROM ");
   statement += TableRef;
   statement += _T(" WHERE CLASS_ID=");
   statement += cls;
   statement += _T(" AND SUB_CL_ID=");
   statement += sub;

   if (gsc_superuser() != GS_GOOD)  // non è un SUPER USER
   {  // leggo lista delle tabelle secondarie visibili per l'utente corrente
      bool first_time = true;

      // Uso una C_4INT_STR in cui:   
      // set_key() e get_key()           gestiscono il codice della classe
      // set_type() e get_type()         gestiscono il codice della sotto-classe
      // set_category() e get_category() gestiscono il codice della tabella secondaria
      // set_level() e get_level()       gestiscono l'abilitazione della tabella secondaria

      // lista tabelle secondarie utente
      if (gsc_getSecPermissions(GEOsimAppl::GS_USER.code, gs_id, usr_listSec) == GS_BAD)
         return GS_BAD; // nessuna abilitazione
      
      pusr_sec = (C_4INT_STR *) usr_listSec.get_head();  // inizio lista
      while (pusr_sec)
      {
         // salto le tabelle secondarie che non sono della classe
         if (pusr_sec->get_key() != cls || pusr_sec->get_type() != sub) 
            { pusr_sec = (C_4INT_STR *) usr_listSec.get_next(); continue; }
         // salto le classi invisibili
         if (pusr_sec->get_level() == (int) GSInvisibleData)
            { pusr_sec = (C_4INT_STR *) usr_listSec.get_next(); continue; }

         if (first_time)
         { 
            first_time = false;
            statement += _T(" AND GS_ID IN(");
         }
         statement += pusr_sec->get_category();
         statement += _T(',');

         pusr_sec = (C_4INT_STR *) usr_listSec.get_next();  // elemento successivo  
      }   
      
      if (first_time) return GS_GOOD; // nessuna abilitazione
      statement.set_chr(_T(')'), statement.len() - 1);  // sostituisco ultimo ',' con ')'
   }

   statement += _T(" ORDER BY GS_ID");

   // leggo le righe della tabella senza bloccarla
   if (pConn->ExeCmd(statement, pRs, adOpenForwardOnly, adLockReadOnly, retest) == GS_BAD) return GS_BAD;
   
   if (gsc_InitDBReadRow(pRs, ColValues) == GS_BAD)
      { gsc_DBCloseRs(pRs); return GS_BAD; }

   pKey   = ColValues.CdrAssoc(DEFAULT_KEY_ATTR);
   pName  = ColValues.CdrAssoc(_T("NAME"));
   pType  = ColValues.CdrAssoc(_T("TYPE"));
   pDescr = ColValues.CdrAssoc(_T("DESCRIPTION"));
   pVersion = ColValues.CdrAssoc(_T("VERSION"));
   pKeyPriAttrib = ColValues.CdrAssoc(_T("KEY_PRI"));
   pKeyAttrib = ColValues.CdrAssoc(_T("KEY_ATTRIB"));
   pRealInitDistanceAttrib = ColValues.CdrAssoc(_T("REAL_INIT_DISTANCE_ATTRIB"));
   pRealFinalDistanceAttrib = ColValues.CdrAssoc(_T("REAL_FINAL_DISTANCE_ATTRIB"));

   // scorro l'elenco delle secondarie
   while (gsc_isEOF(pRs) == GS_BAD)
   {
      if (gsc_DBReadRow(pRs, ColValues) == GS_BAD) { gsc_DBCloseRs(pRs); return GS_BAD; }

      gsc_rb2Int(pKey, &cod_sec);
      name_sec = pName;
      gsc_rb2Int(pType, &type_sec);

      // Descrizione tabella secondaria
      if (pDescr->restype == RTSTR)
         descr_sec = pDescr->resval.rstring;
      else
         descr_sec.clear();

      // Versione
      if (pVersion->restype == RTSTR)
         version_sec = pVersion->resval.rstring;
      else
         version_sec.clear();

      // Nome attributo della classe madre
      if (pKeyPriAttrib->restype == RTSTR)
         keyPriAttrib_sec = pKeyPriAttrib->resval.rstring;
      else
         keyPriAttrib_sec.clear();
      // Nome attributo chiave della tabella secondaria
      if (pKeyAttrib->restype == RTSTR)
         keyAttrib_sec = pKeyAttrib->resval.rstring;
      else
         keyAttrib_sec.clear();

      // Nome dell'attributo per la distanza reale iniziale
      if (pRealInitDistanceAttrib->restype == RTSTR)
      {
         realInitDistanceAttrib_sec = pRealInitDistanceAttrib->resval.rstring;
         // Nome dell'attributo per la distanza reale finale
         if (pRealFinalDistanceAttrib->restype == RTSTR)
            realFinalDistanceAttrib_sec = pRealFinalDistanceAttrib->resval.rstring;
         else
            realFinalDistanceAttrib_sec.clear();
      }
      else
      {
         realInitDistanceAttrib_sec.clear();
         realFinalDistanceAttrib_sec.clear();
      }

      // Livello di accesso
      if (gsc_superuser() == GS_GOOD) cod_level = GSUpdateableData; // classi modificabili 
      else                                     
      {
         pusr_sec = (C_4INT_STR *) usr_listSec.search(cls, sub, cod_sec);
         cod_level = pusr_sec->get_level();   // abilitazione
      }


      if ((pSinthSec = new C_SINTH_SEC_TAB(cod_sec, name_sec.get_name(), descr_sec.get_name(), type_sec, 
                                           keyPriAttrib_sec.get_name(), keyAttrib_sec.get_name(),
                                           realInitDistanceAttrib_sec.get_name(), realFinalDistanceAttrib_sec.get_name(),
                                           version_sec.get_name(), cod_level)) == NULL)
         { gsc_DBCloseRs(pRs); GS_ERR_COD = eGSOutOfMem; return GS_BAD; }

      SinthSecList.add_tail(pSinthSec);

      gsc_Skip(pRs);
   }
   
   if (gsc_DBCloseRs(pRs) == GS_BAD) return GS_BAD;
         
   return GS_GOOD;
}


/*********************************************************/
/*  FINE   FUNZIONI DELLA CLASSE C_PROJECT               */
/*  INIZIO FUNZIONI DELLA CLASSE C_LIST_PRJ              */
/*********************************************************/


C_LIST_PRJ::C_LIST_PRJ() : C_LIST() {}

C_LIST_PRJ::~C_LIST_PRJ() 
{ 
   remove_all(); 
}  

C_NODE* C_LIST_PRJ::search_key(int prj) // se non c'e in memoria carica da DB
{
   C_PROJECT *punt;
   if ((punt = (C_PROJECT*) C_LIST::search_key(prj)) == NULL) 
      if ((punt = restore_project(prj)) == NULL) 
         GS_ERR_COD = eGSInvalidPrjCode;
   
   return punt;
}             
       
TCHAR *C_LIST_PRJ::get_dir(int prj)
{
   C_PROJECT *punt;
   if ((punt = (C_PROJECT *) search_key(prj)) == NULL) return NULL; 

   return punt->dir.get_name();
}

C_CLASS_LIST *C_LIST_PRJ::get_classlist(int prj)
{
   C_PROJECT *punt;
   if ((punt = (C_PROJECT *) search_key(prj)) == NULL) return NULL; 

   return &(punt->classlist);
}
   

// fatta per le applicazioni DllExport
C_LIST_PRJ* get_GS_PROJECT()
{
   return &GEOsimAppl::PROJECTS;
}


/*********************************************************************/
/*.doc gsc_SetPopDwnPrjList <external> */
/*+
  Questa funzione setta una lista pop-down con i progetti abilitati
  all'utente corrente.
  Parametri :
  ads_hdlg    dcl_id;     handle della finestra
  const TCHAR *CtrlName; nome del controllo nella finestra
  int         current;    progetto corrente (defaut = 0)

  La funzione restituisce GS_BAD in caso di errore, GS_CAN se selezione
  annullata, altrimenti GS_GOOD.
-*/  
/*********************************************************************/
int gsc_SetPopDwnPrjList(ads_hdlg dcl_id, const TCHAR *CtrlName, int current)
{
   TCHAR     strPos[10] = _T("0");   
   int       pos = 0;
   C_PROJECT *punt;

   // visualizzo la lista sulla DCL aperta
   ads_start_list(dcl_id, CtrlName, LIST_NEW, 0);
   punt = (C_PROJECT *) GEOsimAppl::PROJECTS.get_head(); // lista già ordinata
   if (punt)
   {
      gsc_add_list(GS_EMPTYSTR);
      while (punt)
      {
         gsc_add_list(punt->get_name());
         if (punt->get_key() == current) swprintf(strPos, 10, _T("%d"), pos + 1);
         else pos++;
         punt = (C_PROJECT *) GEOsimAppl::PROJECTS.get_next();
      }
   }
   ads_end_list();
   if (gsc_strcmp(strPos, _T("0")) != 0) ads_set_tile(dcl_id, CtrlName, strPos);

   return GS_GOOD;
}
C_PROJECT *gsc_GetPopDwnPrjList(int pos)
{
   return (C_PROJECT *) GEOsimAppl::PROJECTS.goto_pos(pos); // 1 e' il primo
}


/*********************************************************
/*.doc gsc_getPrjTabInfo                     <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente la lista dei progetti di GEOsim.
  Parametri:
  C_DBCONNECTION **pConn;     Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              la lista dei progetti di GEOsim (default = NULL)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getPrjTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef)
{
   C_STRING Catalog;

   Catalog = GEOsimAppl::GEODIR;
   Catalog += _T('\\');
   Catalog += ACCESSGEOMAINDB;
   C_2STR_LIST UDLProp(_T("Data Source"), Catalog.get_name());
   if ((*pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &UDLProp)) == NULL)
      return GS_BAD;  
   if (pTableRef)
      pTableRef->paste((*pConn)->get_FullRefTable(Catalog.get_name(), NULL, PRJS_TABLE_NAME));

   return GS_GOOD;
}


/*********************************************************
/*.doc gsc_getSRIDTabInfo                     <internal> */
/*+
  Questa funzione restituisce la connessione OLE-DB e il riferimento
  completo della tabella contenente la lista dei SRID di GEOsim.
  Parametri:
  C_DBCONNECTION **pConn;     Puntatore a connessione OLE-DB
  C_STRING       *pTableRef;  Riferimento completo tabella contenente 
                              la lista dei SRID di GEOsim (default = NULL)
     
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int gsc_getSRIDTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef)
{
   C_STRING Catalog;

   Catalog = GEOsimAppl::GEODIR;
   Catalog += _T('\\');
   Catalog += ACCESSGEOMAINDB;
   C_2STR_LIST UDLProp(_T("Data Source"), Catalog.get_name());
   if ((*pConn = GEOsimAppl::DBCONNECTION_LIST.get_Connection(NULL, &UDLProp)) == NULL)
      return GS_BAD;  
   if (pTableRef)
      pTableRef->paste((*pConn)->get_FullRefTable(Catalog.get_name(), NULL, SRID_TABLE_NAME));

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int gsc_getLastUsedPrj                                            */
/*+ 
   Funzione che restituisce il codice dell'ultimo progetto usato.
   Parametri:
   int *prj;    codice progetto

   Ritorna GS_BAD in caso di errore o nessun progetto precedente, GS_GOOD altrimenti.
-*/
/*************************************************************************/
int gs_getLastUsedPrj(void)
{
   int prj;

   acedRetNil();
   if (gsc_getLastUsedPrj(&prj) == GS_GOOD && prj > 0) acedRetInt(prj);
   return RTNORM;
}
int gsc_getLastUsedPrj(int *prj)
{
   C_STRING pathfile, StrPrj;

   // leggo il codice progetto in GEOSIM.INI
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;
   if (gsc_getInfoFromINI(_T("LastUsedPrj"), StrPrj, &pathfile) == GS_BAD)
      return GS_BAD;   
   if ((*prj = StrPrj.toi()) == 0) return GS_BAD;

   return GS_GOOD;
}

int gs_setLastUsedPrj(void)
{
   presbuf arg = acedGetArgs();
   int prj;
   
   acedRetNil();

   // codice del progetto
   if (!arg || gsc_rb2Int(arg, &prj) != GS_GOOD)
      { GS_ERR_COD = eGSInvalidArg; return RTERROR; }
   if (gsc_setLastUsedPrj(prj) == GS_GOOD) acedRetT();

   return RTNORM;
}
int gsc_setLastUsedPrj(int prj)
{
   C_STRING pathfile, StrPrj;

   StrPrj = prj;
   // scrivo il codice progetto in GEOSIM.INI
   pathfile = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
   pathfile += _T('\\');
   pathfile += GS_INI_FILE;

   return gsc_setInfoToINI(_T("LastUsedPrj"), StrPrj, &pathfile);
}


//---------------------------------------------------------------------------//
// class C_SINTH_SEC_TAB                                                     //
//---------------------------------------------------------------------------//


C_SINTH_SEC_TAB::C_SINTH_SEC_TAB() : C_INT_INT_STR() 
{
   dynamic_segmentation_type = GSNoneDynSegmentation;
}

C_SINTH_SEC_TAB::C_SINTH_SEC_TAB(int _sec, TCHAR *_name, TCHAR *_Descr, int _type, 
                                 TCHAR *_KeyPriAttrib, TCHAR *_KeyAttrib, 
                                 TCHAR *_RealInitDistanceAttrib, TCHAR *_RealFinalDistanceAttrib, 
                                 TCHAR *_Version, GSDataPermissionTypeEnum _level)
{ 
   set_key(_sec);
   if (_name) set_name(_name);
   set_Descr(_Descr);
   set_type(_type);
   set_KeyPriAttrib(_KeyPriAttrib);
   set_KeyAttrib(_KeyAttrib);
   set_RealInitDistanceAttrib(_RealInitDistanceAttrib);
   set_RealFinalDistanceAttrib(_RealFinalDistanceAttrib);
   set_Version(_Version);
   set_level(_level);
}

C_SINTH_SEC_TAB::~C_SINTH_SEC_TAB() {} 

TCHAR* C_SINTH_SEC_TAB::get_Descr() { return Descr.get_name(); }
int C_SINTH_SEC_TAB::set_Descr(TCHAR *_Descr) 
{ 
   Descr = _Descr;
   return GS_GOOD; 
}

TCHAR* C_SINTH_SEC_TAB::get_KeyPriAttrib() { return KeyPriAttrib.get_name(); }
int C_SINTH_SEC_TAB::set_KeyPriAttrib(TCHAR *_KeyPriAttrib) 
{ 
   KeyPriAttrib = _KeyPriAttrib;
   return GS_GOOD; 
}

TCHAR* C_SINTH_SEC_TAB::get_KeyAttrib() { return KeyAttrib.get_name(); }
int C_SINTH_SEC_TAB::set_KeyAttrib(TCHAR *_KeyAttrib) 
{ 
   KeyAttrib = _KeyAttrib;
   return GS_GOOD; 
}

TCHAR* C_SINTH_SEC_TAB::get_RealInitDistanceAttrib() { return RealFinalDistanceAttrib.get_name(); }
int C_SINTH_SEC_TAB::set_RealInitDistanceAttrib(TCHAR *_RealInitDistanceAttrib) 
{ 
   RealInitDistanceAttrib = _RealInitDistanceAttrib;
   return GS_GOOD; 
}

TCHAR* C_SINTH_SEC_TAB::get_RealFinalDistanceAttrib() { return RealFinalDistanceAttrib.get_name(); }
int C_SINTH_SEC_TAB::set_RealFinalDistanceAttrib(TCHAR *_RealFinalDistanceAttrib) 
{ 
   RealFinalDistanceAttrib = _RealFinalDistanceAttrib;
   return GS_GOOD; 
}

DynamicSegmentationTypeEnum C_SINTH_SEC_TAB::DynSegmentationType()
{
   if (RealInitDistanceAttrib.len() == 0) return GSNoneDynSegmentation;
   if (RealFinalDistanceAttrib.len() == 0) return GSPunctualDynSegmentation;
   return GSLinearDynSegmentation;
}

TCHAR* C_SINTH_SEC_TAB::get_Version() { return Version.get_name(); }
int C_SINTH_SEC_TAB::set_Version(TCHAR *_Version) 
{ 
   Version = _Version;
   return GS_GOOD; 
}

GSDataPermissionTypeEnum C_SINTH_SEC_TAB::get_level() { return level; }
int C_SINTH_SEC_TAB::set_level(GSDataPermissionTypeEnum _level) 
{ 
   level = _level; 
   return GS_GOOD; 
}


/*********************************************************/
/*.doc C_SINTH_SEC_TAB::to_rb                   <external> */
/*+
  Questa funzione restituisce una lista sintetica dei dati identificativi
  di una tabella secondaria di una classe nella seguente forma lista LISP:
  (("CODE"<sec>)("NAME"<name>)("DESCRIPTION"<description>)("TYPE"<type>)
   ("KEY_PRI "<attr_name>)("KEY_ATTRIB "<attr_name>)
   ("REAL_INIT_DISTANCE_ATTRIB "<attr_name>)("REAL_FINAL_DISTANCE_ATTRIB "<attr_name>)
   ("VERSION"<Versione>)("LEVEL"<AccessType>))
      
  Restituisce un puntatore alla lista resbuf in caso di successo altrimenti NULL.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
resbuf *C_SINTH_SEC_TAB::to_rb(void)
{
   C_RB_LIST list;

   if ((list << acutBuildList(RTLB,
                                 RTSTR, _T("CODE"),
                                 RTSHORT, get_key(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("NAME"),
                                 RTSTR, get_name() ? get_name(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("DESCRIPTION"),
                                 RTSTR, get_Descr() ? get_Descr(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("TYPE"),
                                 RTSHORT, get_type(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("ACCESS_TYPE"),
                                 RTSHORT, get_level(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("DYNAMIC_SEGMENTATION_TYPE"),
                                 RTSHORT, (int) DynSegmentationType(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("KEY_PRI"),
                                 RTSTR, get_KeyPriAttrib() ? get_KeyPriAttrib(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("KEY_ATTRIB"),
                                 RTSTR, get_KeyAttrib() ? get_KeyAttrib(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("REAL_INIT_DISTANCE_ATTRIB"),
                                 RTSTR, get_RealInitDistanceAttrib() ? get_RealInitDistanceAttrib(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("REAL_FINAL_DISTANCE_ATTRIB"),
                                 RTSTR, get_RealFinalDistanceAttrib() ? get_RealFinalDistanceAttrib(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("VERSION"),
                                 RTSTR, get_Version() ? get_Version(): GS_EMPTYSTR,
                              RTLE,
                              0)) == NULL) return NULL;

   list.ReleaseAllAtDistruction(GS_BAD);

   return list.get_head();
}


//---------------------------------------------------------------------------//
//    class C_SINTH_SEC_TAB_LIST                                             //
//---------------------------------------------------------------------------//


C_SINTH_SEC_TAB_LIST::C_SINTH_SEC_TAB_LIST() : C_LIST() {}
      
C_SINTH_SEC_TAB_LIST::~C_SINTH_SEC_TAB_LIST() {}


/*********************************************************/
/*.doc C_SINTH_SEC_TAB_LIST::to_rb              <external> */
/*+
  Questa funzione restituisce una lista sintetica delle tabelle secondarie della classe
  nella forma lista LISP:
    
  Restituisce un puntatore alla lista resbuf in caso di successo altrimenti NULL.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
resbuf *C_SINTH_SEC_TAB_LIST::to_rb(void)
{
   C_RB_LIST       Result;
   C_SINTH_SEC_TAB *punt;
            
   punt = (C_SINTH_SEC_TAB *) get_head();
   if (punt == NULL) return acutBuildList(RTNIL, 0);
         
   while (punt != NULL)
   {
      if ((Result += acutBuildList(RTLB, 0)) == NULL) return NULL;
      if ((Result += punt->to_rb()) == NULL) return NULL;
      if ((Result += acutBuildList(RTLE, 0)) == NULL) return NULL;

      punt = (C_SINTH_SEC_TAB *) get_next();
   }
   Result.ReleaseAllAtDistruction(GS_BAD);
                                
   return Result.get_head();      
}

void C_SINTH_SEC_TAB_LIST::FilterOnType(SecondaryTableTypesEnum Type)
{
   C_SINTH_SEC_TAB *p = (C_SINTH_SEC_TAB *) get_head();

   while (p)
      if (p->get_type() != (int) Type)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_SEC_TAB *) get_cursor();
      }
      else
         p = (C_SINTH_SEC_TAB *) get_next();
}
void C_SINTH_SEC_TAB_LIST::FilterOnDynamicSegmentationType(DynamicSegmentationTypeEnum Type)
{
   C_SINTH_SEC_TAB *p = (C_SINTH_SEC_TAB *) get_head();

   while (p)
      if (p->DynSegmentationType() != Type)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_SEC_TAB *) get_cursor();
      }
      else
         p = (C_SINTH_SEC_TAB *) get_next();
}
void C_SINTH_SEC_TAB_LIST::FilterOnDynSegmentationSupportedOnly(void)
{
   C_SINTH_SEC_TAB *p = (C_SINTH_SEC_TAB *) get_head();

   while (p)
      if (p->DynSegmentationType() == GSNoneDynSegmentation)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_SEC_TAB *) get_cursor();
      }
      else
         p = (C_SINTH_SEC_TAB *) get_next();
}
void C_SINTH_SEC_TAB_LIST::FilterOnName(const TCHAR *Mask)
{
   C_SINTH_SEC_TAB *p;
   C_STRING        SecTabName;

   p = (C_SINTH_SEC_TAB *) get_head();
   while (p)
   {
      SecTabName = p->get_name();
      if (SecTabName.wildcomp(Mask) == 0)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_SEC_TAB*) get_cursor();
      }
      else
         p = (C_SINTH_SEC_TAB*) get_next();
   }
}


//---------------------------------------------------------------------------//
// class C_SINTH_CLASS                                                       //
//---------------------------------------------------------------------------//


C_SINTH_CLASS::C_SINTH_CLASS() : C_INT_INT_STR() 
{
   category  = 0;
   level     = GSInvisibleData;
   extracted = DESELECTED;
   color.setByLayer();
   view_enabled = false;
   history_enabled = false;
}

C_SINTH_CLASS::C_SINTH_CLASS(int _cls, TCHAR *_name, TCHAR *_Descr,
                             int _category, int _type,
                             GSDataPermissionTypeEnum _level, int _extracted, TCHAR *_dir_dwg, 
                             TCHAR *_Version, C_COLOR *_color, bool _view_enabled, bool _history_enabled)
{ 
   set_key(_cls);
   if (_name) set_name(_name);
   set_Descr(_Descr);
   set_category(_category);
   set_type(_type);
   set_level(_level);
   set_extracted(_extracted);

   if (_color) set_color(*_color);
   else color.setByLayer();

   set_dir_dwg(_dir_dwg);
   set_Version(_Version);

   set_view_enabled(_view_enabled);
   set_history_enabled(_history_enabled);

}

C_SINTH_CLASS::~C_SINTH_CLASS() {} 

TCHAR* C_SINTH_CLASS::get_Descr() { return Descr.get_name(); }
int C_SINTH_CLASS::set_Descr(TCHAR *_Descr) 
{ 
   Descr = _Descr;
   return GS_GOOD; 
}

int C_SINTH_CLASS::get_category() { return category; }
int C_SINTH_CLASS::set_category(int _category)  
{ 
   category = _category; 
   return GS_GOOD; 
}

GSDataPermissionTypeEnum C_SINTH_CLASS::get_level() { return level; }
int C_SINTH_CLASS::set_level(GSDataPermissionTypeEnum _level) 
{ 
   level = _level; 
   return GS_GOOD; 
}

void C_SINTH_CLASS::get_color(C_COLOR &out) { out = color; }
int C_SINTH_CLASS::set_color(C_COLOR &in) 
{ 
   if (in.getColorMethod() == C_COLOR::None) { GS_ERR_COD = eGSInvalidColor; return GS_BAD; }
   color = in;
   return GS_GOOD; 
}

int C_SINTH_CLASS::get_extracted() { return extracted; }
int C_SINTH_CLASS::set_extracted(int _extracted) 
{ 
   extracted = _extracted;
   return GS_GOOD; 
}

TCHAR* C_SINTH_CLASS::get_dir_dwg() { return dir_dwg.get_name(); }
int C_SINTH_CLASS::set_dir_dwg(TCHAR *_dir_dwg) 
{ 
   dir_dwg = _dir_dwg;
   return GS_GOOD; 
}

TCHAR* C_SINTH_CLASS::get_Version() { return Version.get_name(); }
int C_SINTH_CLASS::set_Version(TCHAR *_Version) 
{ 
   Version = _Version;
   return GS_GOOD; 
}

C_LIST *C_SINTH_CLASS::ptr_sub_list() 
{  
   return &sub_list; 
}

bool C_SINTH_CLASS::get_view_enabled() { return view_enabled; }
int C_SINTH_CLASS::set_view_enabled(bool _view_enabled) 
{ 
   view_enabled = _view_enabled;
   return GS_GOOD; 
}

bool C_SINTH_CLASS::get_history_enabled() { return history_enabled; }
int C_SINTH_CLASS::set_history_enabled(bool _history_enabled) 
{ 
   history_enabled = _history_enabled;
   return GS_GOOD; 
}

/*********************************************************/
/*.doc C_SINTH_CLASS::get_Bitmap                    <external> */
/*+
  Funzione che restituisce la bitmap del progetto (16x16).

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_SINTH_CLASS::get_Bitmap(bool LargeSize, CBitmap &CBMP)
{
   return gsc_getClassBitmap(get_category(), get_type(), color, LargeSize, CBMP);
}


/*********************************************************/
/*.doc C_SINTH_CLASS::to_rb                   <external> */
/*+
  Questa funzione restituisce una lista sintetica dei dati identificativi
  di una classe del progetto nella seguente forma lista LISP:
  (("CODE"<cls>)("NAME"<name>)("DESCRIPTION"<description>)("CATEGORY"<category>)
   ("TYPE"<type>)("LEVEL"<AccessType>)
   ("EXTRACTED"<Estratta>)("COLOR"<Colore>)("DIR_DWG"<dir_dwg>)("VERSION"<Versione>)
   ("VIEW_ENABLED"<vista SQL abilitata>)("HISTORY_ENABLED"<storicizzazione abilitata>))
      
  Restituisce un puntatore alla lista resbuf in caso di successo altrimenti NULL.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
resbuf *C_SINTH_CLASS::to_rb(void)
{
   C_RB_LIST list;

   if ((list << acutBuildList(RTLB,
                                 RTSTR, _T("CODE"),
                                 RTSHORT, get_key(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("NAME"),
                                 RTSTR, get_name() ? get_name(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("DESCRIPTION"),
                                 RTSTR, get_Descr() ? get_Descr(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("CATEGORY"),
                                 RTSHORT, get_category(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("TYPE"),
                                 RTSHORT, get_type(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("ACCESS_TYPE"),
                                 RTSHORT, get_level(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("EXTRACTED"),
                                 RTSHORT, get_extracted(),
                              RTLE,
                              RTLB,
                                 RTSTR, _T("COLOR"),
                              0)) == NULL) return NULL;
   if ((list += color.getResbuf()) == NULL) return NULL;
   if ((list += acutBuildList(RTLE,
                              RTLB,
                                 RTSTR, _T("DIR_DWG"),
                                 RTSTR, get_dir_dwg() ? get_dir_dwg(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("VERSION"),
                                 RTSTR, get_Version() ? get_Version(): GS_EMPTYSTR,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("VIEW_ENABLED"),
                                 view_enabled ? RTT : RTNIL,
                              RTLE,
                              RTLB,
                                 RTSTR, _T("HISTORY_ENABLED"),
                                 history_enabled ? RTT : RTNIL,
                              RTLE,
                              0)) == NULL) return NULL;

   list.ReleaseAllAtDistruction(GS_BAD);

   return list.get_head();
}


//---------------------------------------------------------------------------//
//    class C_SINTH_CLASS_LIST                                                //
//---------------------------------------------------------------------------//


C_SINTH_CLASS_LIST::C_SINTH_CLASS_LIST() : C_LIST() {}
      
C_SINTH_CLASS_LIST::~C_SINTH_CLASS_LIST() {}


/*********************************************************/
/*.doc C_SINTH_CLASS_LIST::to_rb              <external> */
/*+
  Questa funzione restituisce una lista sintetica delle classi del progetto
  nella forma lista LISP:
  ((("CODE"<cls>)("NAME"<name>)("DESCRIPTION"<description>)("CATEGORY"<category>)
    ("TYPE"<type>)("LEVEL"<AccessType>)
    ("EXTRACTED"<Estratta>)("COLOR"<Colore>)("DIR_DWG"<dir_dwg>)("VERSION"<Versione>)
    ("VIEW_ENABLED"<vista SQL abilitata>)("HISTORY_ENABLED"<storicizzazione abilitata>)<sub_list>)...)
  
  se category = CAT_EXTERN
     sub_list = ((("CODE"<sub>)("NAME"<name>)("DESCRIPTION"<description>)("CATEGORY"<category>)
                  ("TYPE"<type>)("LEVEL"<AccessType>)
                  ("EXTRACTED"<Estratta>)("COLOR"<Colore>)("DIR_DWG"<dir_dwg>)("VERSION"<Versione>)
                  ("VIEW_ENABLED"<vista SQL abilitata>)("HISTORY_ENABLED"<storicizzazione abilitata>))...)
  altrimenti 
     sub_list = nil
    
  Restituisce un puntatore alla lista resbuf in caso di successo altrimenti NULL.
  N.B.: Alloca memoria.
-*/  
/*********************************************************/
resbuf *C_SINTH_CLASS_LIST::to_rb(void)
{
   C_RB_LIST          Result;
   C_SINTH_CLASS      *punt;
   C_SINTH_CLASS_LIST *sub_list;
            
   punt = (C_SINTH_CLASS *) get_head();
   if (punt == NULL) return acutBuildList(RTNIL, 0);
         
   while (punt != NULL)
   {
      if ((Result += acutBuildList(RTLB, 0)) == NULL) return NULL;
      if ((Result += punt->to_rb()) == NULL) return NULL;

      // sub_list
      sub_list = (C_SINTH_CLASS_LIST *) punt->ptr_sub_list();
      punt     = (C_SINTH_CLASS *) sub_list->get_head();
      if (punt == NULL)
      {
         if ((Result += acutBuildList(RTNIL, 0)) == NULL) return NULL;
      }
      else
      {
         if ((Result += acutBuildList(RTLB, 0)) == NULL) return NULL;
         while (punt != NULL)
         {
            if ((Result += acutBuildList(RTLB, 0)) == NULL) return NULL;
            if ((Result += punt->to_rb()) == NULL) return NULL;
            if ((Result += acutBuildList(RTLE, 0)) == NULL) return NULL;

            punt = (C_SINTH_CLASS *) sub_list->get_next();
         }
         
         if ((Result += acutBuildList(RTLE, 0)) == NULL) return NULL;
      }

      if ((Result += acutBuildList(RTLE, 0)) == NULL) return NULL;

      punt = (C_SINTH_CLASS *) get_next();
   }
   Result.ReleaseAllAtDistruction(GS_BAD);
                                
   return Result.get_head();      
}

void C_SINTH_CLASS_LIST::FilterOnCategoryList(C_INT_LIST &CategoryList)
{
   C_SINTH_CLASS *p;

   p = (C_SINTH_CLASS *) get_head();
   while (p)
   {
      if (CategoryList.search_key(p->get_category()) == NULL)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_CLASS*) get_cursor();
      }
      else
         p = (C_SINTH_CLASS*) get_next();
   }
}

void C_SINTH_CLASS_LIST::FilterOnTypeList(C_INT_LIST &TypeList)
{
   C_SINTH_CLASS *p;

   p = (C_SINTH_CLASS *) get_head();
   while (p)
   {
      if (TypeList.search_key(p->get_type()) == NULL)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_CLASS*) get_cursor();
      }
      else
         p = (C_SINTH_CLASS*) get_next();
   }
}
void C_SINTH_CLASS_LIST::FilterOnPermission(GSDataPermissionTypeEnum Level)
{
   C_SINTH_CLASS *p;

   p = (C_SINTH_CLASS *) get_head();
   while (p)
   {
      if (p->get_level() != Level)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_CLASS*) get_cursor();
      }
      else
         p = (C_SINTH_CLASS*) get_next();
   }
}
void C_SINTH_CLASS_LIST::FilterOnExtracted(int Extracted)
{
   C_SINTH_CLASS *p;

   p = (C_SINTH_CLASS *) get_head();
   while (p)
   {
      if (p->get_extracted() != Extracted)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_CLASS*) get_cursor();
      }
      else
         p = (C_SINTH_CLASS*) get_next();
   }
}
void C_SINTH_CLASS_LIST::FilterOnName(const TCHAR *Mask)
{
   C_SINTH_CLASS *p;
   C_STRING      ClassName;

   p = (C_SINTH_CLASS *) get_head();
   while (p)
   {
      ClassName = p->get_name();
      if (ClassName.wildcomp(Mask) == 0)
      {
         remove_at(); // lo elimino dalla lista
         p = (C_SINTH_CLASS*) get_cursor();
      }
      else
         p = (C_SINTH_CLASS*) get_next();
   }
}


/*********************************************************/
/*.doc C_SINTH_CLASS_LIST::sort_for_extraction <external> */
/*+
  Questa funzione ordina le classi per ordine di complessità cioè
  di relazione con le altre classi.
  Per esempio le classi semplici, spaghetti, simulazioni sono di 
  grado 1; i gruppi di semplici sono di grado 2; 
  i gruppi di gruppi di semplici sono di grado 3 e cosi' via.
  A parità di livello le classi vengono ordinate come segue:
  1) superfici 2) spaghetti 3) polilinee 4) punti 5) testi.
  Parametri:
  bool GroupToBottomOnly;   flag opzionale; se true la funzione si limita a
                            spostare in fondo alla lista le classi gruppo (default = false)

  Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/  
/*********************************************************/
int C_SINTH_CLASS_LIST::sort_for_extraction(bool GroupToBottomOnly)
{
   // lista con coeff e codice classe
   // dove coeff è calcolato da livello di complessità * 100 + tipo classe
   // ordinato in modo che superfici = 1, spaghetti = 2, polilinee = 3,
   // punti = 4, testi = 5.
   C_SINTH_CLASS   *pSinthCls = (C_SINTH_CLASS *) get_head();
   C_INT_LONG_LIST ItemList;
   C_INT_LONG      *pItem;
   long            Coeff;

   if (GroupToBottomOnly) // sposto in fondo alla lista le classi gruppo
   {
      C_SINTH_CLASS_LIST grpClsList;

      while (pSinthCls)
      {
         if (pSinthCls->get_category() == CAT_GROUP)
            grpClsList.add_tail(cut_at());
         pSinthCls = (C_SINTH_CLASS *) get_next();
      }
      paste_tail(grpClsList);
   }
   else
   {
      while (pSinthCls)
      {
         Coeff = (pSinthCls->get_category() == CAT_GROUP) ? 200 : 100;

         switch (pSinthCls->get_category())
         {
            case CAT_SIMPLEX:
               switch (pSinthCls->get_type())
               {
                  case TYPE_SURFACE:
                     Coeff += 1;
                     break;
                  case TYPE_POLYLINE:
                     Coeff += 3;
                     break;
                  case TYPE_NODE:
                     Coeff += 4;
                     break;
                  case TYPE_TEXT:
                     Coeff += 5;
                     break;
                  default:
                     Coeff += 10;
               }
               break;
            case CAT_SPAGHETTI:
               Coeff += 2;
               break;
            default:
               Coeff += 10;
         }

         if ((pItem = new C_INT_LONG(pSinthCls->get_key(), Coeff)) == NULL)
            { GS_ERR_COD = eGSOutOfMem; return GS_BAD; }
         ItemList.add_tail(pItem);

         pSinthCls = (C_SINTH_CLASS *) pSinthCls->get_next();
      }

      ItemList.sort_id(); // in ordine crescente per il numero long (id) che è il coeff

      // Ordina la lista delle classi
      pItem = (C_INT_LONG *) ItemList.get_head();
      while (pItem)
      {
         pSinthCls = (C_SINTH_CLASS *) cut_key(pItem->get_key());
         add_tail(pSinthCls);
      
         pItem = (C_INT_LONG *) pItem->get_next();
      }
   }

   return GS_GOOD;
}