/*************************************************************************/
/*   GS_AREA.H                                                          */
/*************************************************************************/


#ifndef _gs_area_h
#define _gs_area_h 1

#ifndef _gs_prjct_h
   #include "gs_prjct.h" 
#endif

#ifndef _gs_graph_h
   #include "gs_graph.h" 
#endif


//-----------------------------------------------------------------------//   
// Classe per comando gs_rWrkSessionClass
// Classe estratta nella sessione di lavoro
//-----------------------------------------------------------------------//
class C_AREACLASS : public C_4INT_STR
{
   friend class C_AREACLASS_LIST;
   protected:
      int sel;
            
   public :
      C_AREACLASS();
      virtual ~C_AREACLASS(); // chiama ~C_4INT_STR

      int get_sel();
      int set_sel(int in);
      int set(int par_code, TCHAR *par_name, GSDataPermissionTypeEnum par_level, int par_cat,
      		  int par_type, int par_sel);
      resbuf *to_rb(void);
};

//-----------------------------------------------------------------------//   

class C_AREACLASS_LIST : public C_LIST
{
   public :
      C_AREACLASS_LIST();
      virtual ~C_AREACLASS_LIST();

      resbuf *to_rb(void);
};
                                                                           
//-----------------------------------------------------------------------//

class C_WRK_SESSION : public C_NODE
{
   friend class C_WRK_SESSION_LIST;

   protected :
   C_PROJECT *pPrj;                    // puntatore a progetto
   long      code;                     // codice numerico  
   TCHAR     name[MAX_LEN_WRKSESSION_NAME]; // nome della sessione di lavoro
   C_STRING  dir;                      // direttorio della sessione
   GSDataPermissionTypeEnum level;     // tipo di area (GSReadOnlyData, GSUpdateableData)
   int       usr;                      // codice utente proprietario
   int       status;                   // stato attuale:
                                       // WRK_SESSION_WAIT = appena creata in attesa 
                                       //                    della prima estrazione,
                                       // WRK_SESSION_ACTIVE = attiva dopo aver estratto
                                       // WRK_SESSION_FREEZE = congelata
                                       // WRK_SESSION_SAVE = in salvataggio
                                       // WRK_SESSION_EXTR = in estrazione
   TCHAR coordinate[MAX_LEN_COORDNAME]; // nome del sistema di coordinate della sessione 
   GSSRIDUnitEnum SRID_unit;            // unità dello SRID della sessione di lavoro

   C_STRING CreationDateTime;          // data e ora di creazione
   C_STRING KeyDwg;                    // percorso opzionale del sinottico
   C_INT_LIST ClsCodeList;             // Lista dei codici delle classi usate nella sessione di lavoro

   C_DBCONNECTION *pConn;              // Puntatore alla connessione OLE-DB
   
   _CommandPtr   CmdSelectLinkWhereCls;    // Comando per la ricerca dei links
   _CommandPtr   CmdSelectLinkWhereKey;    // Comando per la ricerca dei links
   _CommandPtr   CmdSelectLinkWhereHandle; // Comando per la ricerca dei links
   _RecordsetPtr RsInsertLink;             // RecordSet per inserimento di links
   _RecordsetPtr RsSelectLink;             // RecordSet per la ricerca di links in modalità seek
   int           isRsSelectLinkSupported;  // se = -1 non ancora inizializzato
                                           // se = 0 non supportato
                                           // se = 1 supportato

   _RecordsetPtr RsInsertDeleted;          // RecordSet per inserimento di entità cancellate

   CAseLinkSel  *pLS;                      // Puntatore a LS usato (per ora) da C_LINK

   C_INT_LIST   WaitingClsToUnlockList;    // Lista dei codici delle classi che sono
                                           // rimaste in attesa di essere sbloccate

   C_CACHE_CLS_ATTRIB_VALUES_LIST CacheClsAttribValuesList; // Cache dei valori letti da TAB e REF

   C_STRING SQLParameters[MAX_WRK_SESSION_PARAMNUMBER]; // Vettore di MAX_WRK_SESSION_PARAMNUMBER 
                                                        // parametri possibili per l'istruzine SQL
                                                        // di estrazione della classe linkata
                                                        // a tabelle dati già esistenti (es. history)

   private:
      int del_session_from_db(_RecordsetPtr &pRs);
      int Lock(_RecordsetPtr &pRs);
      int Unlock(_RecordsetPtr &pRs, int Mode = UPDATEABLE);
      int ClassesUnlock(C_INT_LIST *pLockedCodeClassList = NULL);
      int ClassesLock(C_INT_LIST *pLockedCodeClassList = NULL);
      long ins_session_to_db(C_RB_LIST &ColValues);
      int upd_ClsCodeList_to_db(void);
      int ClsExtract(int cls, int mode, C_SELSET *pExtractedSS = NULL);
      int NotifyWaitingUserOnErase();
      TCHAR *WriteScriptToExit(TCHAR *NextCmd = NULL);

   public :
      DllExport C_WRK_SESSION(C_PROJECT *_pPrj);
      DllExport virtual ~C_WRK_SESSION();

DllExport int get_PrjId(void);
DllExport C_PROJECT *get_pPrj();
DllExport C_CLASS *find_class(int cls, int sub = 0);
DllExport C_CLASS *find_class(ads_name ent);

DllExport long get_id();
      int set_id(long in);
      TCHAR *get_name();
DllExport int  set_name(const TCHAR *in);
DllExport TCHAR *get_dir();
DllExport int  set_dir(const TCHAR *in);
DllExport GSDataPermissionTypeEnum get_level();
DllExport int set_level(GSDataPermissionTypeEnum in);
      int  get_usr();
      int  set_usr(int in);
DllExport int  get_status();
      int  set_status(int in, int WriteDB = GS_BAD);
      TCHAR *get_coordinate();
      int  set_coordinate(const TCHAR *SessionCoord);
      GSSRIDUnitEnum get_SRID_unit(void);
      TCHAR *get_KeyDwg();
      int  set_KeyDwg(const TCHAR *SessionKeyDwg);
      TCHAR *get_CreationDateTime(void);
      C_INT_LIST *get_pClsCodeList(void);
      C_STRING *get_SQLParameters(void);
      
      int get_TempInfoFilePath(C_STRING &FilePath, int Cls = 0);
      
      C_DBCONNECTION* getDBConnection();
   
      _CommandPtr get_CmdSelectLinkWhereCls(void);
      _CommandPtr get_CmdSelectLinkWhereKey(void);
      _CommandPtr get_CmdSelectLinkWhereHandle(void);
      _RecordsetPtr get_RsInsertLink(void);
      _RecordsetPtr get_RsSelectLink(void);
      _RecordsetPtr get_RsInsertDeleted(void);

      CAseLinkSel *get_pLS(void);
      void Term_pLS(void);

DllExport C_CACHE_CLS_ATTRIB_VALUES_LIST *get_pCacheClsAttribValuesList(void);

      int     copy            (C_WRK_SESSION *out);
      presbuf to_rb           (void);
      int     to_rb_db        (presbuf *colvalues);
      int     from_rb         (presbuf rb);
      int     from_rb_db      (C_RB_LIST &ColValues);
      long    create          (void);
      int     del             (const TCHAR *Password);
      int     destroy         (const TCHAR *Password);
      int     is_valid        (void);
      int     get_class_list  (C_AREACLASS_LIST &lista, bool OnlyExtracted = true);
      int     get_spatial_area_list(C_RECT_LIST &SpatialAreaList);
      int     load_classes    (void);
      int     freeze				(void);
DllExport int save				(void);
DllExport int exit            (int for_quit = GS_BAD, TCHAR *NextCmd = NULL);
DllExport int isReadyToUpd    (int *WhyNot = NULL);
      int     HasBeenFrozen   (void);
      int     ActiveButPreviouslyFrozen(void);

DllExport int extract         (int mode, int Exclusive = FALSE, C_INT_INT_LIST *pErrClsCodeList = NULL,
                               C_SELSET *pExtractedSS = NULL, int retest = MORETESTS);
DllExport void DisplayMsgsForClassNotExtracted(C_INT_INT_LIST &ErrClsCodeList);
	   int 	  init         	(void);
	   int     terminate 		(void);
      int     DWGsUnlock      (void);
DllExport int SelClasses      (C_INT_LIST &lista);

      int thaw_1part(bool RecoverDwg = false); // Scongelamento
      int thaw_2part(bool RecoverDwg = false);

      int getLinksTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL);
DllExport int getDeletedTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef = NULL);
      int getVerifiedTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef = NULL);

   C_INT_LIST *ptr_WaitingClsToUnlockList(void);
      int AddClsToUnlockWaitingClsList(int cls);
DllExport int UnlockWaitingClsList(void);

      void TerminateSQL(C_DBCONNECTION *pConnToTerminate = NULL);
      int setLastExtractedClassesToINI(void);
      int getLastExtractedClassesFromINI(C_INT_LIST &ClsList);
};

//-----------------------------------------------------------------------//   

class C_WRK_SESSION_LIST : public C_LIST
{
   public :
      C_WRK_SESSION_LIST() : C_LIST() { }
      virtual ~C_WRK_SESSION_LIST(){ }  // chiama ~C_LIST

      presbuf to_rb(void);
      int ruserarea(int user_code, int prj_code, int state = -1, 
                    int OnlyOwner = FALSE, int retest = MORETESTS);
      C_WRK_SESSION *search_level(GSDataPermissionTypeEnum in); 
      int     rarea(int prj_code);
};


/*************************************************************************/
/*  GLOBAL VARIABLES                                                     */
/*************************************************************************/


extern C_WRK_SESSION *GS_CURRENT_WRK_SESSION; // sessione di lavoro corrente

extern _RecordsetPtr RS_LOCK_WRKSESSION; // variabile per scongelamento


//-----------------------------------------------------------------------//   
// FUNZIONI PER CHIAMATE LISP E DA C //
//-----------------------------------------------------------------------//   

int gs_is_frozen_curr_session(void);
extern int gs_thaw_WrkSession2(void);
extern int gs_rWrkSessionClass(void);
void gsextractareaclass(void);
extern int gs_clsextrinWrkSession(void);
DllExport extern int gsc_extract_area_class(int mode);
extern int gsc_desel_all_area_class(void);
extern int gs_retrieve_WrkSession(void);
extern int gsc_retrieve_area(int prj, long SessionId);
int  gs_drawWrkSessionspatialareas(void);

extern C_WRK_SESSION *gsc_find_wrk_session(int prj, long SessionId);
extern int gs_ruserWrkSession(void);
extern int gs_sel_WrkSession_class(void);
extern int gs_currentWrkSession(void);

int gs_get_NextWrkSessionId(void);
extern int gs_create_WrkSession(void);
extern int gs_del_WrkSession(void);
extern int gsc_del_area(int prj, long SessionId, const TCHAR *Password);

extern int gs_freeze_WrkSession(void);
extern int gsc_freeze_area(void);

extern int gs_save_WrkSession(void);

extern int gs_exit_WrkSession(void);
DllExport int gsc_ExitCurrSession(int for_quit = GS_BAD, TCHAR *NextCmd = NULL);

extern int gs_thaw_WrkSession(void);
extern int gsc_thaw_area(int prj, long SessionId);

DllExport   C_WRK_SESSION* get_GS_CURRENT_WRK_SESSION();
DllExport int gsc_extract_classes(C_PROJECT *pPrj, C_WRK_SESSION &session, C_INT_LIST &ClsList,
                                  int Exclusive = FALSE, presbuf SpatialCond = NULL,
                                  presbuf PropertyCond = NULL, int PropertyNotMode = FALSE,
                                  int mode = EXTRACTION, C_SELSET *pExtractedSS = NULL,
                                  int retest = MORETESTS);

int gs_destroysession(void);
int gs_getLastExtractedClassesFromINI(void);
DllExport int gsc_getLastExtractedClassesFromINI(int prj, C_INT_LIST &ClsList);


#endif
