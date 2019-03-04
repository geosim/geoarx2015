/**********************************************************
Name: GS_INIT

Module description: File contenente le procedure di inizializzazione
                    e di chiusura di Geosim
            
Author: Poltini Roberto

(c) Copyright 1995-2015 by IREN ACQUA GAS S.p.a.

                            
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
#include <adeads.h>

#include "GSresource.h"

#include "..\gs_def.h" // definizioni globali
#include "gs_utily.h"     // (gsc_strcat, gsc_tostring)
#include "gs_error.h"     // codici errori
#include "gs_list.h"
#include "gs_resbf.h"     // gestione resbuf
#include "gs_ase.h"       // gestione link db-grafica
#include "gs_class.h"
#include "gs_dbref.h"     // cataloghi e schede
#include "gs_prjct.h"     // prototipi funzioni extern
#include "gs_netw.h"      // funzioni per gestione rete
#include "gs_init.h"      // prototipi funzioni extern
#include "gs_ade.h"
#include "gs_lisp.h"   
#include "gs_thm.h"   
#include "gs_filtr.h"   

#include "gs_user.h"   
#include "gs_opcod.h"
#include "gs_area.h"
#include "gs_graph.h"
#include "gs_query.h"
#include "gs_evid.h"      // per gsc_ddgraphBrowse


/*********************************************************/
/* TYPEDEFS */
/*********************************************************/


/*********************************************************/
/* PRIVATE VARIABLES */
/*********************************************************/


/*********************************************************/
/* FUNCTIONS */
/*********************************************************/


// Inizializzazione membri statici

CAseAppl* GEOsimAppl::ASE = NULL;
CAsiAppl* GEOsimAppl::ASI = NULL;
C_DBCONNECTION_LIST GEOsimAppl::DBCONNECTION_LIST;
C_RB_LIST GEOsimAppl::ASE_APP_ID_LIST;             
C_RB_LIST GEOsimAppl::APP_ID_LIST;
C_STR_LIST GEOsimAppl::OD_TABLENAME_LIST;
C_CMDLIST GEOsimAppl::CMDLIST;              
struct cmd_code GEOsimAppl::GS_COMMANDS[GS_COMMANDS_LEN];
C_STRING GEOsimAppl::GEODIR;  
C_STRING GEOsimAppl::WORKDIR;  
C_STRING GEOsimAppl::CURRUSRDIR;  
C_INIT GEOsimAppl::GLOBALVARS;              
C_ALIAS_LIST GEOsimAppl::GS_NET_LIST;       
C_LIST_PRJ GEOsimAppl::PROJECTS;            
C_USER GEOsimAppl::GS_USER;                 
C_SELSET GEOsimAppl::SAVE_SS;                 
C_SELSET GEOsimAppl::REFUSED_SS;                 
C_SET_VISIB_LIST* GEOsimAppl::ACTIVE_VIS_ATTRIB_SET = NULL;
int GEOsimAppl::LAST_CLS = 0;
int GEOsimAppl::LAST_SUB = 0;
double GEOsimAppl::TOLERANCE = 1.0E-4;
C_DYNAMIC_QRY_ATTR* GEOsimAppl::DYNAMIC_QRY_ATTR = NULL;
C_LAYER_DISPLAY_MODEL GEOsimAppl::LAYER_DISPLAY_MODEL;
AcRx::AppMsgCode GEOsimAppl::LastAppMsgCode = AcRx::kNullMsg;
C_STRING GEOsimAppl::LAST_STARTED_CMD;
C_WAIT_DLG_INSTANCE GEOsimAppl::WAIT_DLG_INSTANCE;
//C_WAIT_DLG_INSTANCE* GEOsimAppl::pWAIT_DLG_INSTANCE = NULL;

GEOsimAppl::GEOsimAppl() 
{
}

GEOsimAppl::~GEOsimAppl()
{
   terminate();
}


/*********************************************************/
/*.doc GEOsimAppl::init                       <external> */
/*+
  Questa funzione inizializza GEOsim.
  La procedura necessita di:
  - una variabile di sistema GEOSIM contenente il direttorio di 
    installazione di GEOSIM (dove risiede GS_PRJ).
  - una variabile di sistema GEOWORK contenente il direttorio di
    lavoro locale. 
  - installazione di ACAD già avvenuta.
    
  Vengono inizializzate le seguenti variabili:
   - GEODIR    = direttorio installazione GEOsim
   - WORKDIR   = direttorio di lavoro di GEOsim
                                                       
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int GEOsimAppl::init(void)
{
   C_STRING Prefix, pathfile, DestFile;

   acutPrintf(gsc_msg(120)); // ""\nCaricamento GEOsim.\n1995-2015 IREN ACQUA GAS S.p.A.""
   acutPrintf(_T("\n%s"), gsc_msg(130)); // Versione attuale di GEOsim

   ///////////////////////
   // LATO SERVER - inizio

   // leggo e verifico esistenza dei direttori di GEOsim
   if (init_dir() == GS_BAD) return GS_BAD;
   Prefix = GEODIR;
   Prefix += _T('\\');

   // verifico esistenza del sotto-direttorio USR di GEOsim
   pathfile = Prefix;
   pathfile += GEOUSRDIR;
   if (gsc_mkdir(pathfile) == GS_BAD) return GS_BAD;

   // verifico esistenza del sotto-direttorio THM GEOsim
   pathfile = Prefix;
   pathfile += GEOTHMDIR;
   if (gsc_mkdir(pathfile) == GS_BAD) return GS_BAD;

   // verifico esistenza del sotto-direttorio EXT GEOsim
   pathfile = Prefix;
   pathfile += GEOEXT;
   if (gsc_mkdir(pathfile) == GS_BAD) return GS_BAD;

   // verifico esistenza del sotto-direttorio SUPPORT GEOsim
   pathfile = Prefix;
   pathfile += GEOSUPPORTDIR;
   if (gsc_mkdir(pathfile) == GS_BAD) return GS_BAD;

   // se non esiste il database principale di GEOsim lo creo
   DestFile = Prefix;
   DestFile += ACCESSGEOMAINDB;

   if (gsc_path_exist(DestFile) == GS_BAD)  // se non esisteva
   {
      pathfile = Prefix + GEOSAMPLEDIR + _T('\\') + ACCESSGEOSIMMAINSAMPLEDB;

      if (gsc_copyfile(pathfile.get_name(), DestFile.get_name()) == GS_BAD) 
         { terminate(); return GS_BAD; }
   }

   ///////////////////////
   // LATO SERVER - fine
   // LATO CLIENT - inizio

   // leggo e verifico esistenza dei direttori locali di GEOsim
   Prefix = CURRUSRDIR;

   // se non esiste dir locale dell'utente corrente, la creo
   pathfile = Prefix;
   if (gsc_path_exist(pathfile) == GS_BAD) // probabile che sia la prima esecuzione
   {
      if (gsc_mkdir(pathfile) == GS_BAD) return GS_BAD; 
      // Elaboro i tematismi
      if (gsc_refresh_thm() == GS_BAD) { terminate(); return GS_BAD; }
   }

   Prefix += _T('\\');

   // se non esiste subdir TEMP di dir locale dell'utente corrente, la creo
   pathfile = Prefix;
   pathfile += GEOTEMPDIR;
   if (gsc_mkdir(pathfile) == GS_BAD) return GS_BAD;

   // leggo la lista delle macchine collegate in rete
   if (restore_netpath() == GS_BAD) return GS_BAD;

   // inizializzo ambiente ASE
   if ((ASE = gsc_initase()) == GS_BAD) return GS_BAD;

   // inizializzo ambiente ASE
   if ((ASI = gsc_initasi()) == GS_BAD) return GS_BAD;

   // inizializzo la lista che descrive l'identificatore di AUTOCAD ASE per le
   // entità estese
   if ((ASE_APP_ID_LIST << acutBuildList(RTSTR, ASE_APP_ID, 0)) == NULL) return GS_BAD;

   // inizializzo la lista che descrive l'identificatore di GEOsim per le
   // entità estese (dichiarata in GS_GRAPH)
   if ((APP_ID_LIST << acutBuildList(RTSTR, GEO_APP_ID, 0)) == NULL) return GS_BAD;

   // inizializzo la tolleranza nel confronto tra coordinate
   resbuf AcadPrec;
   if (acedGetVar(_T("LUPREC"), &AcadPrec) != RTNORM) TOLERANCE = 1.0E-4;
   else TOLERANCE = pow(10.0, -1 * AcadPrec.resval.rint);

   pathfile = Prefix; // Directory locale dell'utente corrente
   pathfile += GS_INI_FILE;
   // inizializzo variabili globali
   if (GLOBALVARS.Load(pathfile.get_name()) == GS_BAD) { terminate(); return GS_BAD; }

   // inizializza vettore dei comandi abilitabili e disabilitabili
   init_cmd_vector();

   // carica GSL.GSL dal direttorio centrale di GEOsim
   gs_gsl_gs_reload();

   return GS_GOOD;     
}


/*********************************************************/
/*.doc GEOsimAppl::initByUser                 <external> */
/*+
  Questa funzione inizializza le strutture usate da GEOsim che dipendono
  dall'utente corrente.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
  
  N.B. la funzione è usabile solo se non esiste una sessione di lavoro attiva !!!
  		 (vengono rilasciati e ricaricati i progetti rilasciando anche eventuali classi
		  caricate in memoria)
-*/  
/*********************************************************/
int GEOsimAppl::initByUser()
{  
   C_MAP_ENV AdeEnv;

   // inizializzo le variabili d'ambiente ADE per GEOsim (alcune variabili di ADE
   // possono essere settate solo se si è SUPERUSER di MAP)
   // AdeEnv.SetEnv4GEOsim(); roby questa inizializzazione non dipende dall'utente corrente

   // se non c'è alcuna sessione attiva e non si è in fase di scongelamento 
   // (in caso di login automatica inserita nel file acaddoc.lsp)
   if (!GS_CURRENT_WRK_SESSION && RS_LOCK_WRKSESSION.GetInterfacePtr() == NULL)
   {
      GEOsimAppl::DBCONNECTION_LIST.remove_all();
	   if (PROJECTS.restore_projects() == GS_BAD) return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc GEOsimAppl::init_dir                   <internal> */
/*+
  Questa funzione inizializza e crea i direttori di GEOsim.
  La procedura necessita di:
  - una variabile di sistema GEOSIM contenente il direttorio di 
    installazione della banca dati centrale di GEOSIM (dove risiede GS_PRJ).
  - una variabile di sistema GEOWORK contenente il direttorio di
    lavoro locale. 

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int GEOsimAppl::init_dir(void)
{   
   // leggo e verifico esistenza del direttorio di installazione di GEOsim
   if (gsc_geosimdir(GEODIR) == GS_BAD) return GS_BAD;
   // leggo il direttorio di lavoro locale di GEOsim
   if (gsc_geoworkdir(WORKDIR) == GS_BAD) return GS_BAD;
   // leggo il direttorio di GEOsim per l'utente corrente
   if (gsc_getGEOsimUsrAppDataPath(CURRUSRDIR) == GS_BAD) return GS_BAD;

   // se non esiste geoworkdir errore ripetere l'installazione
   if (gsc_path_exist(WORKDIR) == GS_BAD) return GS_BAD;

   return GS_GOOD;
}


/*************************************************************************/
/*.doc int GEOsimAppl::restore_netpath()                                 */ 
/*+   
   Legge dal file GS_NET_FILE nella directory di GEOsim le lista delle
   corrispondenze tra connessioni in rete e drive delle macchina.
   
   Ritorna GS_BAD in caso di errore GS_BAD altrimenti.
-*/
/*************************************************************************/
int GEOsimAppl::restore_netpath(void)
{
   if (GS_NET_LIST.load() == GS_BAD)
   {
      acutPrintf(gsc_msg(691)); // "\nConfigurare gli alias dei nodi di rete.\n"
      return GS_BAD;
   }

   return GS_GOOD;
}


/*********************************************************/
/*.doc GEOsimAppl::terminate                  <external> */
/*+           
  Questa funzione termina GEOsim.
  La procedura:
  - rilascia la lista dei progetti e relative liste delle classi (PROJECTS)
                                                       
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int GEOsimAppl::terminate(void)
{                 
   C_STRING temp_dir;

   // se c'era una sessione di GEOsim aperta la chiudo
   if (GS_CURRENT_WRK_SESSION) gsc_ExitCurrSession();

   // cancello la lista dei lista progetti
   PROJECTS.remove_all();
   
   // chiudo collegamento con ASE
   gsc_termase(&ASE);

   // chiudo collegamento con ASI
   gsc_termasi(&ASI);

   DBCONNECTION_LIST.remove_all(); // chiudo collegamenti OLE-DB
   
   // rilascio lista pubblica di resbuf GS_RESBUF
   gsc_dealloc_GS_RESBUF();

   // cancello il contenuto del direttorio temporaneo GEOsimAppl::CURRUSRDIR + GEOTEMPDIR
   temp_dir = CURRUSRDIR;
   temp_dir += _T('\\');
   temp_dir += GEOTEMPDIR;
   gsc_delall(temp_dir.get_name(), NORECURSIVE, FALSE);

   // rilascio il set di visibilità degli attributi attivo
   if (ACTIVE_VIS_ATTRIB_SET)
   {
      delete ACTIVE_VIS_ATTRIB_SET;
      ACTIVE_VIS_ATTRIB_SET = NULL;
   }

   // Se esiste il puntatore C_DYNAMIC_QRY_ATTR per l'interrogazione dinamica 
   if (DYNAMIC_QRY_ATTR)
   {
      delete DYNAMIC_QRY_ATTR;
      DYNAMIC_QRY_ATTR = NULL;
   }

   // rilascio le liste resbuf perchè se lo faccio dopo attraverso il distruttore
   // ottengo un errore (forse autocad non è più in grado di rilasciare i resbuf)
   ASE_APP_ID_LIST.remove_all();
   APP_ID_LIST.remove_all();

   GS_LSFILTER.terminate();

   return GS_GOOD; 
}


/*********************************************************/
/*.doc (new 2) GEOsimAppl::init_cmd_vector    <internal> */
/*+                                                                       
  Inizializza il vettore dei comandi di GEOsim che si possono abilitare/disabilitare.
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD. 
-*/  
/*********************************************************/
void GEOsimAppl::init_cmd_vector(void)
{
   int i = 0;
   
   // "Modifica password"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(333), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opModPwd;
   i++; // 1
   // "Creazione sessione di lavoro"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(334), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opCreateArea;
   i++; // 2
   // "Cancellazione sessione di lavoro"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(335), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opDelArea;
   i++; // 3
   // "Congelamento sessione di lavoro"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(336), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opFreezeArea;
   i++; // 4
   // "Scongelamento/recupero sessione di lavoro"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(337), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opSelArea;
   i++; // 5
   // "Salvataggio sessione di lavoro"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(338), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opSaveArea;
   i++; // 6
   // "Uscita sessione di lavoro"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(541), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opCloseArea;
   i++; // 7
   // "Selezione classi da estrarre"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(540), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opSelAreaClass;
   i++; // 8
   // "Inserimento entità"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(339), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opInsEntity;
   i++; // 9
   // "Modifica entità"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(340), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opModEntity;
   i++; // 10
   // "Aggregazione entità"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(341), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opAggrEntity;
   i++; // 11
   // "Cancellazione entità"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(342), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opDelEntity;
   i++; // 12
   // "Interrogazione entità"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(343), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opQryEntity;
   i++; // 13
   // "Importa entità"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(344), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opImportEntity;
   i++; // 14
   // "Filtro entità"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(345), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opFilterEntity;
   i++; // 15
   // "Inserimento entità secondaria"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(542), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opInsSecondary;
   i++; // 16
   // "Modifica entità secondaria"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(543), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opModSecondary;
   i++; // 17
	// "Cancellazione entità secondaria"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(544), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opDelSecondary;
   i++; // 18
	// "Interrogazione entità secondaria"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(545), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opQrySecondary;
   i++; // 19
	// "Editazione attrib. visibili"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(546), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opAttEdit;
   i++; // 20
	// "'Spegnimento' blocchi attrib. visibili"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(547), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opAttInvisib;
   i++; // 21
	// "'Accensione' blocchi attrib. visibili"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(427), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opAttVisib;
   i++; // 22
	// "Gestione set di visibiltà"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(652), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opSetVisib;
   i++; // 23
	// "Modifica tabella definizione attributi"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(548), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opModDefTabRef;
   i++; // 24
	// "Creazione topologia"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(851), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opCreateTopo;
   i++; // 25
	// "Funzione cambio dim. testi e attrib. visibili"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(852), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opScaleAttrText;
   i++; // 26
	// "Funzione creaz. file DWF per WHIP!"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(853), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opWhip;
   i++; // 27
   // "Esportazione dati"
   gsc_strcpy(GS_COMMANDS[i].name, gsc_msg(1091), MAX_LEN_DESCR_CMD);
   GS_COMMANDS[i].code = opExport;

   // ricordarsi di cambiare il valore di "GS_COMMANDS_LEN" al variare della
   // dimensione di GS_COMMANDS
}


/*********************************************************/
/*.doc GEOsimAppl::TerminateSQL               <internal> */
/*+
  Questa funzione termina i recordset, i comandi e annulla la connessione 
  OLE_DB eventualmente inizializzata nei progetti, classi, sessioni di lavoro 
  quindi termina la connessione OLE-DB specificata.
  Parametri:
  C_DBCONNECTION *pConnToTerminate; Connessione da terminare
  
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
void GEOsimAppl::TerminateSQL(C_DBCONNECTION *pConnToTerminate)
{
   C_PROJECT *pPrj;
   C_CLASS   *pCls;

   if (!pConnToTerminate) return;

   // Per ciascun progetto
   pPrj = (C_PROJECT *) PROJECTS.get_head();
   while (pPrj)
   {
      // Per ciascuna classe del progetto
      pCls = (C_CLASS *) pPrj->ptr_classlist()->get_head();
      while (pCls)
      {
         pCls->TerminateSQL(pConnToTerminate);

         pCls = (C_CLASS *) pCls->get_next();
      }

      pPrj->TerminateSQL(pConnToTerminate);

      pPrj = (C_PROJECT *) pPrj->get_next();
   }

   // Per la sessione di lavoro corrente
   if (GS_CURRENT_WRK_SESSION)
      GS_CURRENT_WRK_SESSION->TerminateSQL(pConnToTerminate);

   // Rimuovo la connessione dalla lista delle connessioni OLE-DB
   DBCONNECTION_LIST.remove(pConnToTerminate);
}


//-----------------------------------------------------------------------//
//////////////////  C_INIT  INIZIO  ///////////////////////////////////////
//-----------------------------------------------------------------------//


/*********************************************************
/*.doc C_INIT::C_INIT <external> */
/*+ 
  Questa funzione è il costruttore della classe C_INIT.
-*/  
/*********************************************************/
C_INIT::C_INIT()
{
   set_AutoHighlight(GS_BAD);
   set_AutoSyncro(GS_GOOD);
   set_AutoZoom(GS_BAD);
   set_AutoZoomMinXDim(20);
   set_AutoZoomMinYDim(20);
   set_BoolFmt(0);
   set_DimAssoc(1);
   set_ShortDate(GS_GOOD);
   set_InsXScale(AUTO);
   set_InsYScale(AUTO);
   set_InsRotaz(AUTO);
   set_InsHText(AUTO);
   set_InsPos(AUTO);
   set_LogFile(GS_BAD);
   set_DynamicExtraction(GS_BAD);
   set_UpdGraphOnExtract(GS_BAD);
   set_UpdGraphOnExtractSim(GS_BAD);
   set_AlignHighlightedFASOnSave(GS_GOOD);
   set_AddEntityToSaveSet(GS_GOOD);
   set_AddLPTOnExtract(GS_BAD);
   set_WaitTime(1);
   set_NumTest(2);
   set_SelectPreviousExtractedClasses(GS_GOOD);
}


/*********************************************************
/*.doc C_INIT::Load <external> */
/*+ 
  Legge dal file "pathfile" i valori con cui inizializzare
  le variabili globali.
  Parametri:
  TCHAR *pathfile;    Path completa del file di inizializzazione,
                      se = NULL viene utilizzato il file
                      "GEOSIM.INI" in TEMP (default = NULL)
                                                       
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_INIT::Load(TCHAR *pathfile)
{
   C_STRING                Path(pathfile), Buffer;
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   C_2STR_BTREE            *pProfileEntries;
   C_B2STR                 *pProfileEntry;

   if (Path.get_name() == NULL) // se parametro non passato uso quello di default
   {
      Path = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   if (gsc_read_profile(Path, ProfileSections) == GS_BAD) return GS_GOOD;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(GS_INI_LABEL)))
      return GS_GOOD;
   pProfileEntries = (C_2STR_BTREE *) ProfileSection->get_ptr_EntryList();

   // il pannello della sessione di lavoro
   // evidenzia le entità correntemente selezionate
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_AutoHighlight"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_AutoHighlight(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_AutoHighlight(GS_BAD);
   }
   // il pannello della sessione di lavoro
   // fa lo zoom sulle entità correntemente selezionate
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_AutoZoom"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_AutoZoom(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_AutoZoom(GS_BAD);
   }
   // Dimensione minima X della finestra di zoom
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("set_AutoZoomMinXDim"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      set_AutoZoomMinXDim(Buffer.tof());
   }
   // Dimensione minima X della finestra di zoom
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("set_AutoZoomMinYDim"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      set_AutoZoomMinYDim(Buffer.tof());
   }
   // Formato dei valori booleani
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_BoolFmt"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      set_BoolFmt(Buffer.toi());
   }
   // Modalità di quotatura
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_DimAssoc"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      set_DimAssoc(Buffer.toi());
   }
   // Formato della data
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_ShortDate"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_ShortDate(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_ShortDate(GS_BAD);
   }
   // Modalità di inserimento
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_InsXScale"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("MANUAL")) == 0)    set_InsXScale(MANUAL);
      else if (Buffer.comp(_T("AUTO")) == 0) set_InsXScale(AUTO);
   }
   // Modalità di inserimento
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_InsYScale"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("MANUAL")) == 0)    set_InsYScale(MANUAL);
      else if (Buffer.comp(_T("AUTO")) == 0) set_InsYScale(AUTO);
   }
   // Modalità di inserimento
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_InsRotaz"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("MANUAL")) == 0)    set_InsRotaz(MANUAL);
      else if (Buffer.comp(_T("AUTO")) == 0) set_InsRotaz(AUTO);
   }
   // Modalità di inserimento
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_InsHText"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("MANUAL")) == 0)    set_InsHText(MANUAL);
      else if (Buffer.comp(_T("AUTO")) == 0) set_InsHText(AUTO);
   }
   // Modalità di inserimento
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_InsPos"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("MANUAL")) == 0)    set_InsPos(MANUAL);
      else if (Buffer.comp(_T("AUTO")) == 0) set_InsPos(AUTO);
   }
   // Scrittura file LOG di GEOsim, MAP e AutoCAD
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_LogFile"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_LogFile(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_LogFile(GS_BAD);
   }
   // Allineamento automatico modifiche fatte senza comandi di GEOsim,
   // parte su rettore InputContextReactor all'evento beginQuiescentState
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_AutoSyncro"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_AutoSyncro(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_AutoSyncro(GS_BAD);
   }
   // Estrazione dinamica su PAN o ZOOM
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_DynamicExtraction"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_DynamicExtraction(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_DynamicExtraction(GS_BAD);
   }
   // Aggiornamento grafica da DB dopo estrazione
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_UpdGraphOnExtract"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_UpdGraphOnExtract(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_UpdGraphOnExtract(GS_BAD);
   }
   // Aggiornamento grafica da DB dopo estrazione (solo per simulazioni)
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_UpdGraphOnExtractSim"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_UpdGraphOnExtractSim(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_UpdGraphOnExtractSim(GS_BAD);
   }
   // Allineamento grafica in fase di salvataggio
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_AlignHighlightedFASOnSave"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_AlignHighlightedFASOnSave(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_AlignHighlightedFASOnSave(GS_BAD);
   }
   // Inserimento delle entità nel salvataggio
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_AddEntityToSaveSet"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_AddEntityToSaveSet(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_AddEntityToSaveSet(GS_BAD);
   }
   // Aggiunta di LPT dopo estrazione
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("set_AddLPTOnExtract"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_AddLPTOnExtract(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_AddLPTOnExtract(GS_BAD);
   }
   // Secondi di attesa fra due tentativi
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_WaitTime"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      set_WaitTime(Buffer.toi());
   }
   // Tentativi da fare prima di segnalare l'errore
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_NumTest"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      set_NumTest(Buffer.toi());
   }

   // Seleziona le classi estratte nella sessione precedente
   if ((pProfileEntry = (C_B2STR *) pProfileEntries->search(_T("Set_SelectPreviousExtractedClasses"))))
   {
      Buffer = pProfileEntry->get_name2();
      Buffer.alltrim();
      Buffer.toupper();
      if (Buffer.comp(_T("ON")) == 0)       set_SelectPreviousExtractedClasses(GS_GOOD);
      else if (Buffer.comp(_T("OFF")) == 0) set_SelectPreviousExtractedClasses(GS_BAD);
   }

   return GS_GOOD;
} 


/*********************************************************
/*.doc C_INIT::Save <external> */
/*+ 
  Scrive nel file "pathfile" i valori delle variabili globali.
  Parametri:
  TCHAR *pathfile;   Path completa del file di inizializzazione,
                     se = NULL viene utilizzato il file "GEOSIM.INI"
                     nella directory locale dell'utente corrente (default = NULL)
                                                       
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_INIT::Save(TCHAR *pathfile)
{
   C_PROFILE_SECTION_BTREE ProfileSections;
   C_BPROFILE_SECTION      *ProfileSection;
   TCHAR                   buf[MAX_LEN_ENTRY];
   int                     result = GS_BAD;
   C_STRING                Path(pathfile);
   bool                    Unicode = false;

   if (Path.get_name() == NULL) // se parametro non passato uso quello di default
   {
      Path = GEOsimAppl::CURRUSRDIR; // Directory locale dell'utente corrente
      Path += _T('\\');
      Path += GS_INI_FILE;
   }

   if (gsc_path_exist(Path) == GS_GOOD)
      if (gsc_read_profile(Path, ProfileSections, &Unicode) == GS_BAD)
         return GS_BAD;

   if (!(ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.search(GS_INI_LABEL)))
   {
      if (ProfileSections.add(GS_INI_LABEL) == GS_BAD) return GS_BAD;
      ProfileSection = (C_BPROFILE_SECTION *) ProfileSections.get_cursor();
   }

   // Evidenziazione automatica
   wcscpy(buf, (get_AutoHighlight() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_AutoHighlight"), buf);
   // Zoom automatico
   wcscpy(buf, (get_AutoZoom() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_AutoZoom"), buf);
   // Dimensione minima X della finestra di zoom
   ProfileSection->set_entry(_T("Set_AutoZoomMinXDim"), get_AutoZoomMinXDim());
   // Dimensione minima Y della finestra di zoom
   ProfileSection->set_entry(_T("Set_AutoZoomMinYDim"), get_AutoZoomMinYDim());
   // Formato dei valori booleani
   ProfileSection->set_entry(_T("Set_BoolFmt"), get_ShortDate());
   // Modalità di quotatura
   ProfileSection->set_entry(_T("Set_DimAssoc"), get_DimAssoc());
   // Formato della data
   wcscpy(buf, (get_ShortDate() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_ShortDate"), get_DimAssoc());
   // Modalità di inserimento scala X
   wcscpy(buf, (get_InsXScale() == MANUAL) ? _T("MANUAL") : _T("AUTO"));
   ProfileSection->set_entry(_T("Set_InsXScale"), buf);
   // Modalità di inserimento scala Y
   wcscpy(buf, (get_InsYScale() == MANUAL) ? _T("MANUAL") : _T("AUTO"));
   ProfileSection->set_entry(_T("Set_InsYScale"), buf);
   // Modalità di inserimento rotazione
   wcscpy(buf, (get_InsRotaz() == MANUAL) ? _T("MANUAL") : _T("AUTO"));
   ProfileSection->set_entry(_T("Set_InsRotaz"), buf);
   // Modalità di inserimento altezza testo
   wcscpy(buf, (get_InsHText() == MANUAL) ? _T("MANUAL") : _T("AUTO"));
   ProfileSection->set_entry(_T("Set_InsHText"), buf);
   // Modalità di inserimento posizione
   wcscpy(buf, (get_InsPos() == MANUAL) ? _T("MANUAL") : _T("AUTO"));
   ProfileSection->set_entry(_T("Set_InsPos"), buf);
   // Scrittura file LOG di GEOsim, MAP e AutoCAD
   wcscpy(buf, (get_LogFile() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_LogFile"), buf);
   // Allineamento automatico
   wcscpy(buf, (get_AutoSyncro() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_AutoSyncro"), buf);
   // Estrazione dinamica su PAN o ZOOM
   wcscpy(buf, (get_DynamicExtraction() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_DynamicExtraction"), buf);
   // Aggiornamento grafica da DB dopo estrazione
   wcscpy(buf, (get_UpdGraphOnExtract() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_UpdGraphOnExtract"), buf);
   // Aggiornamento grafica da DB dopo estrazione (solo per simulazioni)
   wcscpy(buf, (get_UpdGraphOnExtractSim() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_UpdGraphOnExtractSim"), buf);
   // Allineamento grafica in fase di salvataggio
   wcscpy(buf, (get_AlignHighlightedFASOnSave() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_AlignHighlightedFASOnSave"), buf);
   // Inserimento delle entità nel salvataggio
   wcscpy(buf, (get_AddEntityToSaveSet() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_AddEntityToSaveSet"), buf);
   // Aggiunta di LPT dopo estrazione
   wcscpy(buf, (get_AddLPTOnExtract() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("set_AddLPTOnExtract"), buf);
   // Secondi di attesa fra due tentativi
   ProfileSection->set_entry(_T("Set_WaitTime"), get_WaitTime());
   // Tentativi da fare prima di segnalare l'errore
   ProfileSection->set_entry(_T("Set_NumTest"), get_NumTest());
   // Seleziona le classi estratte nella sessione precedente
   wcscpy(buf, (get_SelectPreviousExtractedClasses() == GS_GOOD) ? _T("ON") : _T("OFF"));
   ProfileSection->set_entry(_T("Set_SelectPreviousExtractedClasses"), buf);

   return gsc_write_profile(Path, ProfileSections, Unicode);
}

  
int C_INIT::set_BoolFmt(int SetBoolFmt)
{
   if (SetBoolFmt != 0 && SetBoolFmt != 1 &&
       SetBoolFmt != 2 && SetBoolFmt != 3)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   BoolFmt = SetBoolFmt;

   return GS_GOOD;
}

  
int C_INIT::set_DimAssoc(int SetDimAssoc)
{
   if (SetDimAssoc != 0 && SetDimAssoc != 1 && SetDimAssoc != 2)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   DimAssoc = SetDimAssoc;

   return GS_GOOD;
}

  
int C_INIT::set_ShortDate(int SetShortDate)
{
   if (SetShortDate != GS_GOOD && SetShortDate != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   ShortDate = SetShortDate;

   return GS_GOOD;
}


int C_INIT::set_InsXScale(int SetInsXScale)
{
   if (SetInsXScale != MANUAL && SetInsXScale != AUTO)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   ins_x_scale = SetInsXScale;

   return GS_GOOD;
}


int C_INIT::set_InsYScale(int SetInsYScale)
{
   if (SetInsYScale != MANUAL && SetInsYScale != AUTO)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   ins_y_scale = SetInsYScale;

   return GS_GOOD;
}


int C_INIT::set_InsRotaz(int SetRotaz)
{
   if (SetRotaz != MANUAL && SetRotaz != AUTO)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   ins_rotaz = SetRotaz;

   return GS_GOOD;
}


int C_INIT::set_InsHText(int SetHText)
{
   if (SetHText != MANUAL && SetHText != AUTO)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   ins_htext = SetHText;

   return GS_GOOD;
}


int C_INIT::set_InsPos(int SetPos)
{
   if (SetPos != MANUAL && SetPos != AUTO)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   ins_pos = SetPos;

   return GS_GOOD;
}


int C_INIT::set_LogFile(int SetLogFile)
{
   C_MAP_ENV     _MapEnv;
   struct resbuf _LogFileActive;

   if (SetLogFile != GS_GOOD && SetLogFile != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   logfile = SetLogFile;

   _LogFileActive.restype = RTSHORT;

   if (SetLogFile == GS_GOOD)
   {
      _MapEnv.SetAdeEnvVariable(_T("LogFileActive"), kAsiTrue);
      _LogFileActive.resval.rint = 1;
   }
   else
   {
      _MapEnv.SetAdeEnvVariable(_T("LogFileActive"), kAsiFalse);
      _LogFileActive.resval.rint = 0;
   }
   acedSetVar(_T("LOGFILEMODE"), &_LogFileActive);  

   return GS_GOOD;
}


int C_INIT::set_AutoHighlight(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   AutoHighlight = Value;

   return GS_GOOD;
}


int C_INIT::set_AutoSyncro(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   AutoSyncro = Value;

   return GS_GOOD;
}


int C_INIT::set_AutoZoom(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   AutoZoom = Value;

   return GS_GOOD;
}


int C_INIT::set_AutoZoomMinXDim(double Value)
{
   if (Value < 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   AutoZoomMinXDim = Value;

   return GS_GOOD;
}


int C_INIT::set_AutoZoomMinYDim(double Value)
{
   if (Value < 0) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   AutoZoomMinYDim = Value;

   return GS_GOOD;
}


int C_INIT::set_DynamicExtraction(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   DynamicExtraction = Value;

   return GS_GOOD;
}


int C_INIT::set_UpdGraphOnExtract(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   UpdGraphOnExtract = Value;

   return GS_GOOD;
}


int C_INIT::set_UpdGraphOnExtractSim(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   UpdGraphOnExtractSim = Value;

   return GS_GOOD;
}


int C_INIT::set_AlignHighlightedFASOnSave(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   AlignHighlightedFASOnSave = Value;

   return GS_GOOD;
}


int C_INIT::set_AddEntityToSaveSet(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   AddEntityToSaveSet = Value;

   return GS_GOOD;
}


int C_INIT::set_AddLPTOnExtract(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   AddLPTOnExtract = Value;

   return GS_GOOD;
}


int C_INIT::set_WaitTime(int SetWaitTime)
{
   if (SetWaitTime < 0 || SetWaitTime > 99)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   WaitTime = SetWaitTime;

   return GS_GOOD;
}


int C_INIT::set_NumTest(int SetNumTest)
{
   if (SetNumTest < 1 || SetNumTest > 99)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   NumTest = SetNumTest;

   return GS_GOOD;
}


int C_INIT::set_SelectPreviousExtractedClasses(int Value)
{
   if (Value != GS_GOOD && Value != GS_BAD)
      { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   SelectPreviousExtractedClasses = Value;

   return GS_GOOD;
}


int C_INIT::get_AutoHighlight() { return AutoHighlight; }
int C_INIT::get_AutoSyncro() { return AutoSyncro; }
int C_INIT::get_AutoZoom() { return AutoZoom; }
double C_INIT::get_AutoZoomMinXDim() { return AutoZoomMinXDim; }
double C_INIT::get_AutoZoomMinYDim() { return AutoZoomMinYDim; }
int C_INIT::get_BoolFmt() { return BoolFmt; }
int C_INIT::get_DimAssoc() { return DimAssoc; }
int C_INIT::get_ShortDate() { return ShortDate; }
int C_INIT::get_DynamicExtraction() { return DynamicExtraction; }
int C_INIT::get_InsXScale() { return ins_x_scale; }
int C_INIT::get_InsYScale() { return ins_y_scale; }
int C_INIT::get_InsRotaz() { return ins_rotaz; }
int C_INIT::get_InsHText() { return ins_htext; }
int C_INIT::get_InsPos() { return ins_pos; }
int C_INIT::get_LogFile() { return logfile; }
int C_INIT::get_UpdGraphOnExtract() { return UpdGraphOnExtract; }
int C_INIT::get_UpdGraphOnExtractSim() { return UpdGraphOnExtractSim; }
int C_INIT::get_AlignHighlightedFASOnSave() { return AlignHighlightedFASOnSave; }
int C_INIT::get_AddEntityToSaveSet() { return AddEntityToSaveSet; }
int C_INIT::get_AddLPTOnExtract() { return AddLPTOnExtract; }
int C_INIT::get_WaitTime() { return WaitTime; }
int C_INIT::get_NumTest() { return NumTest; }
int C_INIT::get_SelectPreviousExtractedClasses() { return SelectPreviousExtractedClasses; }


/*********************************************************
/*.doc C_INIT::Copy <external> */
/*+ 
  Copia i dati dell'oggetto corrente in un altro oggetto C_INIT.
  Parametri:
  C_INIT &Dest;      destinazione della copia
                                                       
  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************/
int C_INIT::Copy(C_INIT &Dest)
{
   Dest.BoolFmt                   = BoolFmt;
   Dest.DimAssoc                  = DimAssoc;
   Dest.ShortDate                 = ShortDate;
   Dest.ins_x_scale               = ins_x_scale;
   Dest.ins_y_scale               = ins_y_scale;
   Dest.ins_rotaz                 = ins_rotaz;
   Dest.ins_htext                 = ins_htext;
   Dest.ins_pos                   = ins_pos;
   Dest.logfile                   = logfile;
   Dest.UpdGraphOnExtract         = UpdGraphOnExtract;
   Dest.UpdGraphOnExtractSim      = UpdGraphOnExtractSim;
   Dest.AlignHighlightedFASOnSave = AlignHighlightedFASOnSave;
   Dest.AddEntityToSaveSet        = AddEntityToSaveSet;
   Dest.AddLPTOnExtract           = AddLPTOnExtract;
   Dest.WaitTime                  = WaitTime;
   Dest.NumTest                   = NumTest;
   Dest.SelectPreviousExtractedClasses = SelectPreviousExtractedClasses;


   return GS_GOOD;
}


//-----------------------------------------------------------------------//
//                   C_INIT  FINE                                        //
//-----------------------------------------------------------------------//


/******************************************************************************/
/*.doc (new 2) gs_GetVar   */
/*+
   Questa funzione restituisce il valore di una qualsiasi variabile globale.
   Parametri:
   (<variabile>)
-*/                                             
/******************************************************************************/
int gs_GetVar(void)
{
   presbuf arg, result;

   acedRetNil(); 
   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSTR)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   
   if ((result = gsc_GetVar(arg->resval.rstring)) == NULL) return RTERROR;
      
   acedRetVal(result);
   acutRelRb(result);

   return RTNORM;
}


// fatta per le applicazioni DllExport, perchè le variabili non si vedono nella libreria
/******************************************************************************/
/*.doc (new 2) gsc_GetVar   */
/*+
   Questa funzione restituisce il valore di una qualsiasi variabile globale.
   Parametri:
   TCHAR *string;             nome variabile
                 
   Restituisce un puntatore a resbuf che successivamente dovrà essere disallocato 
   dall'utente.
   N.B.: Alloca memoria !
-*/                                             
/******************************************************************************/
resbuf *gsc_GetVar(TCHAR *string)
{
   C_STRING str;

   if (!string) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   str = string;
   gsc_toupper(str.get_name());

   if (gsc_strcmp(str.get_name(), _T("GS_WORKDIR")) == 0)
      return acutBuildList(RTSTR, GEOsimAppl::WORKDIR.get_name(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("GS_GEODIR")) == 0)
      return acutBuildList(RTSTR, GEOsimAppl::GEODIR.get_name(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_BOOLFMT")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_BoolFmt(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_DIMASSOC")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_DimAssoc(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_SHORTDATE")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_ShortDate() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_INSXSCALE")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_InsXScale(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_INSYSCALE")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_InsYScale(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_INSROTAZ")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_InsRotaz(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_INSHTEXT")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_InsHText(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_INSPOS")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_InsPos(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_LOGFILE")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_LogFile() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_AUTOHIGHLIGHT")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_AutoHighlight() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_AUTOSYNCRO")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_AutoSyncro() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_AUTOZOOM")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_AutoZoom() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_AUTOZOOMMINXDIM")) == 0)
      return acutBuildList(RTREAL, GEOsimAppl::GLOBALVARS.get_AutoZoomMinXDim(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_AUTOZOOMMINYDIM")) == 0)
      return acutBuildList(RTREAL, GEOsimAppl::GLOBALVARS.get_AutoZoomMinYDim(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_DYNAMICEXTRACTION")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_DynamicExtraction() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_UPDGRAPHONEXTRACT")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_UpdGraphOnExtract() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_UPDGRAPHONEXTRACTSIM")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_UpdGraphOnExtractSim() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_ALIGNHIGHLIGHTEDFASONSAVE")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_AlignHighlightedFASOnSave() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_ADDENTITYTOSAVESET")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_ADDLPTONEXTRACT")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_AddLPTOnExtract() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_WAITTIME")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_WaitTime(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_NUMTEST")) == 0)
      return acutBuildList(RTSHORT, GEOsimAppl::GLOBALVARS.get_NumTest(), 0);
   else
   if (gsc_strcmp(str.get_name(), _T("SET_SELECTPREVIOUSEXTRACTEDCLASSES")) == 0)
      return acutBuildList(RTSTR, (GEOsimAppl::GLOBALVARS.get_SelectPreviousExtractedClasses() == GS_GOOD) ? _T("ON") : _T("OFF"), 0);

   return NULL;
}


/******************************************************************************/
/*.doc (new 2) gs_SetVar   */
/*+
   Questa funzione imposta il valore di una qualsiasi variabile globale.
   Parametri:
   (<variabile><valore>)
-*/                                             
/****************************************************************************************************/
int gs_SetVar(void)
{
   TCHAR   *name;
   presbuf arg;

   acedRetNil(); 
   arg = acedGetArgs();

   if (arg == NULL || arg->restype != RTSTR)
      { GS_ERR_COD=eGSInvalidArg; return RTERROR; }
   name = arg->resval.rstring;

   if (gsc_SetVar(name, arg->rbnext) == GS_BAD) return RTERROR;
      
   acedRetT();

   return RTNORM;
}


// fatta per le applicazioni DllExport, perchè le variabili non si vedono nella libreria
/******************************************************************************/
/*.doc (new 2) gsc_SetVar                 */
/*+
   Questa funzione imposta il valore di una qualsiasi variabile globale.
   Parametri:
   TCHAR *string;           nome variabile
   resbuf *rb;             nuovo valore
                 
   Restituisce GS_GOOD in caso di successo altrimenti GS_BAD.
-*/                                             
/******************************************************************************/
int gsc_SetVar(TCHAR *string, resbuf *rb)
{
   if (!string || !rb) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }

   gsc_toupper(string);

   if (gsc_strcmp(string, _T("SET_AUTOHIGHLIGHT")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_AutoHighlight(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_AutoHighlight(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_AUTOSYNCRO")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_AutoSyncro(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_AutoSyncro(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_AUTOZOOM")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_AutoZoom(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_AutoZoom(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_AUTOZOOMMINXDIM")) == 0)
   {
      if (rb->restype != RTREAL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (GEOsimAppl::GLOBALVARS.set_AutoZoomMinXDim(rb->resval.rreal) == GS_BAD)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_AUTOZOOMMINYDIM")) == 0)
   {
      if (rb->restype != RTREAL) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (GEOsimAppl::GLOBALVARS.set_AutoZoomMinYDim(rb->resval.rreal) == GS_BAD)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_BOOLFMT")) == 0)
   {
      if (rb->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (GEOsimAppl::GLOBALVARS.set_BoolFmt(rb->resval.rint) == GS_BAD)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_DIMASSOC")) == 0)
   {
      if (rb->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (GEOsimAppl::GLOBALVARS.set_DimAssoc(rb->resval.rint) == GS_BAD)
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_SHORTDATE")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_ShortDate(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_ShortDate(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_INSXSCALE")) == 0)
   {
      if (rb->restype != RTSHORT ||
          (rb->resval.rint != AUTO && rb->resval.rint != MANUAL))
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      GEOsimAppl::GLOBALVARS.set_InsXScale(rb->resval.rint);
   }
   else
   if (gsc_strcmp(string, _T("SET_INSYSCALE")) == 0)
   {
      if (rb->restype != RTSHORT ||
          (rb->resval.rint != AUTO && rb->resval.rint != MANUAL))
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      GEOsimAppl::GLOBALVARS.set_InsYScale(rb->resval.rint);
   }
   else
   if (gsc_strcmp(string, _T("SET_INSROTAZ")) == 0)
   {
      if (rb->restype != RTSHORT ||
          (rb->resval.rint != AUTO && rb->resval.rint != MANUAL))
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      GEOsimAppl::GLOBALVARS.set_InsRotaz(rb->resval.rint);
   }
   else
   if (gsc_strcmp(string, _T("SET_INSHTEXT")) == 0)
   {
      if (rb->restype != RTSHORT ||
          (rb->resval.rint != AUTO && rb->resval.rint != MANUAL))
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      GEOsimAppl::GLOBALVARS.set_InsHText(rb->resval.rint);
   }
   else
   if (gsc_strcmp(string, _T("SET_INSPOS")) == 0)
   {
      if (rb->restype != RTSHORT ||
          (rb->resval.rint != AUTO && rb->resval.rint != MANUAL))
         { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      GEOsimAppl::GLOBALVARS.set_InsPos(rb->resval.rint);
   }
   else
   if (gsc_strcmp(string, _T("SET_LOGFILE")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_LogFile(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_LogFile(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_DYNAMICEXTRACTION")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_DynamicExtraction(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_DynamicExtraction(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_UPDGRAPHONEXTRACT")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_UpdGraphOnExtract(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_UpdGraphOnExtract(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_UPDGRAPHONEXTRACTSIM")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_UpdGraphOnExtractSim(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_UpdGraphOnExtractSim(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_ALIGNHIGHLIGHTEDFASONSAVE")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_AlignHighlightedFASOnSave(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_AlignHighlightedFASOnSave(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_ADDENTITYTOSAVESET")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_AddEntityToSaveSet(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_ADDLPTONEXTRACT")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_AddLPTOnExtract(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_AddLPTOnExtract(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }
   else
   if (gsc_strcmp(string, _T("SET_WAITTIME")) == 0)
   {
      if (rb->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (GEOsimAppl::GLOBALVARS.set_WaitTime(rb->resval.rint) == GS_BAD) return GS_BAD;
   }
   else
   if (gsc_strcmp(string, _T("SET_NUMTEST")) == 0)
   {
      if (rb->restype != RTSHORT) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (GEOsimAppl::GLOBALVARS.set_NumTest(rb->resval.rint) == GS_BAD) return GS_BAD;
   }
   else
   if (gsc_strcmp(string, _T("SET_SELECTPREVIOUSEXTRACTEDCLASSES")) == 0)
   {
      if (rb->restype != RTSTR) { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
      if (gsc_strcmp(rb->resval.rstring, _T("ON")) == 0)       GEOsimAppl::GLOBALVARS.set_SelectPreviousExtractedClasses(GS_GOOD);
      else if (gsc_strcmp(rb->resval.rstring, _T("OFF")) == 0) GEOsimAppl::GLOBALVARS.set_SelectPreviousExtractedClasses(GS_BAD);
      else { GS_ERR_COD = eGSInvalidArg; return GS_BAD; }
   }

   return GS_GOOD;
}

/******************************************************************************/
/*.doc get_GS_GLOBALVAR() <internal> */
/*+
   Questa funzione è stata introdotta perchè nelle librerie non si vede 
   direttamente la variabile GEOsimAppl::GLOBALVARS, nel file .h è una funzione DllExport.
-*/  
/******************************************************************************/
C_INIT* get_GS_GLOBALVAR()
{
   return &GEOsimAppl::GLOBALVARS;
}


C_DBCONNECTION_LIST* get_pDBCONNECTION_LIST()
{
	return &GEOsimAppl::DBCONNECTION_LIST;
}

CAseAppl *get_ASE()
{
	return GEOsimAppl::ASE;
}

TCHAR *get_GEODIR()
{
	return GEOsimAppl::GEODIR.get_name();
}

TCHAR *get_WORKDIR()
{
	return GEOsimAppl::WORKDIR.get_name();
}

TCHAR *get_CURRUSRDIR()
{
	return GEOsimAppl::CURRUSRDIR.get_name();
}

C_USER* get_CURRENT_USER()
{
	return &GEOsimAppl::GS_USER;
}

double get_TOLERANCE()
{
	return GEOsimAppl::TOLERANCE;
}

int gsc_initByUser(void)
{
   return GEOsimAppl::initByUser();
}

/////////////////////////////////////////////////////////////////////////
/////    FUNZIONI PER GESTIONE GRUPPO DI ENTITA' NON RIALLINEATE     ////
/////////////////////////////////////////////////////////////////////////


/*********************************************************************/
/*.doc gsevidssrefused                                    <external> */
/*+
  Questa funzione evidenzia gli oggetti del gruppo REFUSED_SS di GEOsim.
-*/  
/*********************************************************************/
void gsevidssrefused(void)
{
   int result;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION)
      { GS_ERR_COD = eGSNotCurrentSession; GEOsimAppl::CMDLIST.ErrorCmd(); return; }   

   // Ripulisce gruppo di selezione AUTOCAD
   gsc_clear_pick_first();
   // evidenzia oggetti
   GEOsimAppl::REFUSED_SS.redraw(3);

   // "Sfogliare gli oggetti ? Si/" viene aggiunto automaticamente "<No>:"
   if (gsc_getconfirm(gsc_msg(75), &result) == GS_GOOD && result == GS_GOOD)
      gsc_ddgraphBrowse(GEOsimAppl::REFUSED_SS);

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************************/
/*.doc gs_get_ssrefused <external> */
/*+
  Questa funzione restituisce il gruppo REFUSED_SS:
  oggetti scartati dall'ultimo comando di GEOsim.
-*/  
/*********************************************************************/
int gs_get_ssrefused(void)
{
   ads_name sel_set;
   long     len;

   acedRetNil();

   if (GEOsimAppl::REFUSED_SS.get_selection(sel_set) == GS_BAD) return RTERROR;

   // tutti gli oggetti di GEOsimAppl::REFUSED_SS
   if (acedSSLength(sel_set, &len) == RTNORM && len > 0)
      acedRetName(sel_set, RTPICKS);

   return RTNORM;
}


/////////////////////////////////////////////////////////////////////////
//////////        FUNZIONI PER GESTIONE GRUPPO DI SALVATAGGIO     ///////
/////////////////////////////////////////////////////////////////////////


/*********************************************************************/
/*.doc gs_get_savess <external> */
/*+
  Questa funzione restituisce il gruppo degli oggetti nuovi/modificati/cancellati
  di GEOsim.
-*/  
/*********************************************************************/
int gs_get_savess(void)
{
   ads_name sel_set;
   long     len;

   acedRetNil();

   if (GEOsimAppl::SAVE_SS.get_selection(sel_set) == GS_BAD) return RTERROR;

   // tutti gli oggetti di GEOsimAppl::SAVE_SS
   if (acedSSLength(sel_set, &len) == RTNORM && len > 0)
      acedRetName(sel_set, RTPICKS);

   return RTNORM;
}


/*********************************************************************/
/*.doc gsevidsavess                                       <external> */
/*+
  Questa funzione evidenzia gli oggetti del gruppo GEOsimAppl::SAVE_SS di GEOsim.
-*/  
/*********************************************************************/
void gsevidsavess(void)
{
   int result;

   GEOsimAppl::CMDLIST.StartCmd();

   if (!GS_CURRENT_WRK_SESSION)
      { GS_ERR_COD = eGSNotCurrentSession; GEOsimAppl::CMDLIST.ErrorCmd(); return; }   
   // Ripulisce gruppo di selezione AUTOCAD
   gsc_clear_pick_first();
   // evidenzia oggetti
   GEOsimAppl::SAVE_SS.redraw(3);

   // "Sfogliare gli oggetti ? Si/<No>:"
   if (gsc_getconfirm(gsc_msg(75), &result) == GS_GOOD && result == GS_GOOD)
      gsc_ddgraphBrowse(GEOsimAppl::SAVE_SS);

   return GEOsimAppl::CMDLIST.EndCmd();
}


/*********************************************************************/
/*.doc gsc_addSS2savess                                   <external> */
/*+
  Questa funzione inserisce gli oggetti nel gruppo GEOsimAppl::SAVE_SS di GEOsim.
  Se la variabile "AddEntityToSaveSet" = GS_BAD (non è abilitato l'inserimento 
  delle entità nel salvataggio) questa funzione non effettua alcun inserimento 
  e restituisce sempre GS_GOOD.
  Parametri:
  ads_name ss;             gruppo di selezione

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************************/
int gsc_addSS2savess(C_SELSET &SelSet)
{
   ads_name ss;
   SelSet.get_selection(ss);
   return gsc_addSS2savess(ss);
}
int gsc_addSS2savess(ads_name ss)
{
   // verifico abilitazione
   if (GS_CURRENT_WRK_SESSION &&                                // se esiste una sessione di lavoro 
       GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_GOOD &&    // attiva e in modifica e
       GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_GOOD) // se si è abilitati a inserire in GEOsimAppl::SAVE_SS
      if (GEOsimAppl::SAVE_SS.add_selset(ss) != GS_GOOD) return GS_BAD;

   return GS_GOOD;
}
int gsc_addEnt2savess(ads_name ent)
{
   // verifico abilitazione
   if (GS_CURRENT_WRK_SESSION &&                                // se esiste una sessione di lavoro 
       GS_CURRENT_WRK_SESSION->isReadyToUpd(&GS_ERR_COD) == GS_GOOD &&    // attiva e in modifica e
       GEOsimAppl::GLOBALVARS.get_AddEntityToSaveSet() == GS_GOOD) // se si è abilitati a inserire in GEOsimAppl::SAVE_SS
      if (GEOsimAppl::SAVE_SS.add(ent) != GS_GOOD) return GS_BAD;

   return GS_GOOD;
}


// Per le applicazioni DllExport, perchè la variabile GEOsimAppl::SAVE_SS non si vede nella libreria
C_SELSET* get_SAVE_SS()
{
   return &(GEOsimAppl::SAVE_SS);
}

int gsc_reset_SAVE_SS(void)
{
   GEOsimAppl::SAVE_SS.clear();
   if (GS_CURRENT_WRK_SESSION)
   {
      C_STRING path;
   
      path = GS_CURRENT_WRK_SESSION->get_dir();
      path += _T("\\");
      path += GEOTEMPSESSIONDIR;
      path += _T("\\");
      path += GEO_SAVESS_FILE;
      if (gsc_path_exist(path) == GS_GOOD)
         if (gsc_delfile(path) == GS_BAD) return GS_BAD;

      path = GS_CURRENT_WRK_SESSION->get_dir();
      path += _T("\\");
      path += GEOTEMPSESSIONDIR;
      path += _T("\\");
      path += GEO_SSDEL_FILE;
      if (gsc_path_exist(path) == GS_GOOD)
         if (gsc_delfile(path) == GS_BAD) return GS_BAD;
   }

   return GS_GOOD; 
} 

// Per le applicazioni DllExport, perchè la variabile GEOsimAppl::REFUSED_SS non si vede nella libreria
C_SELSET* get_REFUSED_SS()
{
   return &(GEOsimAppl::REFUSED_SS);
}

// Per le applicazioni DllExport, perchè la variabile GEOsimAppl::LAST_CLS non si vede nella libreria
int get_LAST_CLS()
{
   return GEOsimAppl::LAST_CLS;
}

// Per le applicazioni DllExport, perchè la variabile GEOsimAppl::LAST_SUB non si vede nella libreria
int get_LAST_SUB()
{
   return GEOsimAppl::LAST_SUB;
}

// Per le applicazioni DllExport, perchè la variabile GEOsimAppl::LAYER_DISPLAY_MODEL non si vede nella libreria
C_LAYER_DISPLAY_MODEL *get_LAYER_DISPLAY_MODEL()
{
   return &(GEOsimAppl::LAYER_DISPLAY_MODEL);
}

/*********************************************************************/
/*.doc gsRestoreDBConnection                              <external> */
/*+
  Questo comando annulla e termina tutte le connessioni OLE-DB.

  Restituisce GS_GOOD in caso di successo altrimenti restituisce GS_BAD.
-*/  
/*********************************************************************/
void gsRestoreDBConnection(void)
{
   C_DBCONNECTION *pConn;

   GEOsimAppl::CMDLIST.StartCmd();

   while ((pConn = (C_DBCONNECTION *) GEOsimAppl::DBCONNECTION_LIST.get_head()))
      GEOsimAppl::TerminateSQL(pConn);

   return GEOsimAppl::CMDLIST.EndCmd();
}