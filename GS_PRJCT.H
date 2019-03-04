/**********************************************************/
/*   GS_PRJCT.H                                           */
/**********************************************************/


#ifndef _gs_prjct_h
#define _gs_prjct_h 1

#ifndef _adsdlg_h
   #include "adsdlg.h" 
#endif

#ifndef _gs_list_h
   #include "gs_list.h" 
#endif

#ifndef _gs_class_h
   #include "gs_class.h" 
#endif

#ifndef _gs_area_h
   #include "gs_area.h" 
#endif

#ifndef _gs_def_h
   #include "..\gs_def.h" // definizioni globali
#endif

#ifndef _gs_sql_h
   #include "gs_sql.h"       // gestione database
#endif


/*********************************************************/
/* TYPEDEFS */
/*********************************************************/


//---------------------------------------------------------------------------//
// class C_SINTH_SEC_TAB                                                     //
//---------------------------------------------------------------------------//


class C_SINTH_SEC_TAB : public C_INT_INT_STR
{
   friend class C_SINTH_SEC_TAB_LIST;
   protected:
      // get_key() usato per codice tabella secondaria
      // get_type() usato per tipo tabella secondaria (geosim o esterna)
      // get_name() usato per il nome della tabella secondaria
      C_STRING Descr;        // Descrizione classe
      C_STRING KeyPriAttrib; // Nome attributo della classe madre
      C_STRING KeyAttrib;    // Nome attributo chiave della tabella secondaria
      GSDataPermissionTypeEnum level; // Aggiornabile (GSUpdateableData), in sola lettura (GSReadOnlyData) o 
      C_STRING RealInitDistanceAttrib;
      C_STRING RealFinalDistanceAttrib;
      DynamicSegmentationTypeEnum dynamic_segmentation_type; // Tipo di segmentazione dinamica 
                                                             // (puntuale, lineare o nessuna)
      C_STRING Version;    // Versione di GEOsim 
            
   public :
      DllExport C_SINTH_SEC_TAB();
      DllExport C_SINTH_SEC_TAB(int _sec, TCHAR *_name, TCHAR *_Descr, int _type, 
                                TCHAR *_KeyPriAttrib, TCHAR *_KeyAttrib,
                                TCHAR *_RealInitDistanceAttrib, TCHAR *_RealFinalDistanceAttrib,
                                TCHAR *_Version, GSDataPermissionTypeEnum _level);
      DllExport virtual ~C_SINTH_SEC_TAB(); // chiama ~C_INT_INT_STR

      DllExport TCHAR* get_Descr();
      DllExport int    set_Descr(TCHAR *in);
      DllExport TCHAR* get_KeyPriAttrib();
      DllExport int    set_KeyPriAttrib(TCHAR *in);
      DllExport TCHAR* get_KeyAttrib();
      DllExport int    set_KeyAttrib(TCHAR *in);
      DllExport TCHAR* get_RealInitDistanceAttrib();
      DllExport int    set_RealInitDistanceAttrib(TCHAR *in);
      DllExport TCHAR* get_RealFinalDistanceAttrib();
      DllExport int    set_RealFinalDistanceAttrib(TCHAR *in);
      DllExport DynamicSegmentationTypeEnum DynSegmentationType(void);
      DllExport TCHAR* get_Version();
      DllExport int    set_Version(TCHAR *in);
      DllExport GSDataPermissionTypeEnum get_level();
      DllExport int set_level(GSDataPermissionTypeEnum in);

      resbuf *to_rb(void);
};


//----------------------------------------------------------------------------//
//    class C_SINTH_SEC_TAB_LIST                                                //
//----------------------------------------------------------------------------//

class C_SINTH_SEC_TAB_LIST : public C_LIST
{
   public :
      DllExport C_SINTH_SEC_TAB_LIST();
      DllExport virtual ~C_SINTH_SEC_TAB_LIST();  // chiama ~C_LIST

                resbuf *to_rb(void);

      DllExport void FilterOnType(SecondaryTableTypesEnum Type);
      DllExport void FilterOnDynamicSegmentationType(DynamicSegmentationTypeEnum Type);
      DllExport void FilterOnDynSegmentationSupportedOnly(void);
      DllExport void FilterOnName(const TCHAR *Mask);
};


//---------------------------------------------------------------------------//
// class C_SINTH_CLASS                                                       //
//---------------------------------------------------------------------------//


class C_SINTH_CLASS : public C_INT_INT_STR
{
   friend class C_SINTH_CLASS_LIST;
   protected:
      C_STRING Descr;      // Descrizione classe
      int      category;   // Categoria della classe
      GSDataPermissionTypeEnum level; // Aggiornabile (GSUpdateableData), in sola lettura (GSReadOnlyData) o 
                                      // in accesso esclusivo da parte di un altro utente (GSExclusiveUseByAnother)
      C_COLOR  color;           // colore classe
      C_STRING dir_dwg;         // cartella dei disegni
      int      extracted;       // Classe estratta nella sessione corrente
                                // (DESELECTED, SELECTED, EXTRACTED, EXTR_SEL)
      C_STRING Version;         // Versione di GEOsim 
      C_LIST   sub_list;        // Lista di SUB-CLASS 
      bool     view_enabled;    // classe che ha delle viste associate
      bool     history_enabled; // classe che ha la storicizzazione abilitata

   public :
      DllExport C_SINTH_CLASS();
      DllExport C_SINTH_CLASS(int _cls, TCHAR *_name, TCHAR *_Descr, 
                              int _category, int _type,
                              GSDataPermissionTypeEnum _level, int _extracted, 
                              TCHAR *_dir_dwg, TCHAR *_Version, C_COLOR *_color = NULL,
                              bool _view_enabled = false, bool _history_enabled = false);
      DllExport virtual ~C_SINTH_CLASS(); // chiama ~C_INT_INT_STR

      DllExport TCHAR* get_Descr();
      DllExport int    set_Descr(TCHAR *in);
      DllExport int get_category();
      DllExport int set_category(int in);
      DllExport GSDataPermissionTypeEnum get_level();
      DllExport int set_level(GSDataPermissionTypeEnum in);
      DllExport void get_color(C_COLOR &out);
      DllExport int set_color(C_COLOR &in);
      DllExport int get_extracted();
      DllExport int set_extracted(int in);
      DllExport TCHAR* get_dir_dwg();
      DllExport int    set_dir_dwg(TCHAR *in);
      DllExport TCHAR* get_Version();
      DllExport int    set_Version(TCHAR *in);
      DllExport C_LIST *ptr_sub_list();
      DllExport bool get_view_enabled();
      DllExport int set_view_enabled(bool in);
      DllExport bool get_history_enabled();
      DllExport int set_history_enabled(bool in);

      DllExport int get_Bitmap(bool LargeSize, CBitmap &CBMP);

      resbuf *to_rb(void);
};


//----------------------------------------------------------------------------//
//    class C_SINTH_CLASS_LIST                                                //
//----------------------------------------------------------------------------//

class C_SINTH_CLASS_LIST : public C_LIST
{
   public :
      DllExport C_SINTH_CLASS_LIST();
      DllExport virtual ~C_SINTH_CLASS_LIST();  // chiama ~C_LIST

                resbuf *to_rb(void);

      DllExport void FilterOnCategoryList(C_INT_LIST &CategoryList);
      DllExport void FilterOnTypeList(C_INT_LIST &TypeList);
      DllExport void FilterOnPermission(GSDataPermissionTypeEnum Level = GSUpdateableData);
      DllExport void FilterOnExtracted(int Extracted = GS_GOOD);
      DllExport void FilterOnName(const TCHAR *Mask);
      DllExport int sort_for_extraction(bool GroupToBottomOnly = false);
};


// Progetto di GEOSIM
class C_PROJECT: public C_NODE
{
   friend class C_LIST_PRJ;
   friend class C_WRK_SESSION;
     
   private:
   int          gs_id;                         // codice numerico
   TCHAR        name[MAX_LEN_PRJNAME];         // nome del progetto
   C_STRING     Descr;                         // descrizione del progetto
   C_STRING     dir;                           // direttorio del progetto
   C_STRING     keydwg;                        // nome del disegno chiave
   TCHAR        coordinate[MAX_LEN_COORDNAME]; // nome del sistema di 
                                               // coordinate del progetto 
   GSDataPermissionTypeEnum level;             // codice abilitazione
   int          dwg_dim;                       // dimensione max. in MByte dei files .DWG
   C_STRING     ConnStrUDLFile;                // Stringa di connessione o File di tipo .UDL
   C_2STR_LIST  UDLProperties;                 // Lista di proprietà UDL 
   
   C_CLASS_LIST classlist;                     // lista delle classi
   
   C_DBCONNECTION *pConn;                      // Puntatore alla connessione OLE-DB alle tabelle interne

   DllExport int InitInsRow(const TCHAR *TableRef, _RecordsetPtr &pRs);

   _RecordsetPtr  RsInsertLock;                // RecordSet per l'inserimento dei locks
   _CommandPtr    CmdSelectLockWhereKey;       // Comando per la ricerca dei locks
   _CommandPtr    CmdSelectLockWhereStatusFree;// Comando per la ricerca dei locks
   _RecordsetPtr  RsSelectLock;                // RecordSet per la ricerca dei locks in modalità seek
   int            isRsSelectLockSupported;     // se = -1 non ancora inizializzato
                                               // se = 0 non supportato
                                               // se = 1 supportato

   // GESTIONE PROGETTO
   int ins_prj_to_gs_prj(C_DBCONNECTION &pConn);
   int is_valid();
   int Lock(_RecordsetPtr &pRs);
   int Unlock(_RecordsetPtr &pRs, int Mode = UPDATEABLE);

   public:

   DllExport C_PROJECT();

   C_PROJECT(const TCHAR *nameprj, const TCHAR *DescrPrj, const TCHAR *dirprj, 
             const TCHAR *keydwgprj, const TCHAR *coordinateprj, int dwgdim);
	
   DllExport ~C_PROJECT();

   // ACCESSO AI MEMBRI DI C_PROJECT

DllExport  int get_key(void);
DllExport  int set_key(int in);
DllExport  TCHAR *get_name(void);
DllExport  int set_name(const TCHAR *nameprj);          
DllExport  TCHAR *get_descr(void);
DllExport  int set_descr(const TCHAR *Descrprj);          
DllExport  TCHAR *get_dir(void);
DllExport  int set_dir(const TCHAR *dirprj);         
DllExport  TCHAR *get_keydwg(void);
DllExport  int set_keydwg(const TCHAR *keydwgprj);         
DllExport  TCHAR *get_coordinate(void);
DllExport  int set_coordinate(const TCHAR *coordinateprj); 
DllExport  int get_dwg_dim(void);
DllExport  int set_dwg_dim(int in);
const TCHAR *get_ConnStrUDLFile(void);
int         set_ConnStrUDLFile(const TCHAR *in);
C_2STR_LIST *ptr_UDLProperties(void);
int         set_UDLProperties(presbuf *arg);
int         set_UDLProperties(C_2STR_LIST &_UDLProperties);

DllExport  GSDataPermissionTypeEnum get_level(void);
DllExport C_CLASS_LIST *ptr_classlist(void);

DllExport int get_Bitmap(bool LargeSize, CBitmap &CBMP);

   C_DBCONNECTION* getDBConnection();
   
   _CommandPtr get_CmdSelectLockWhereKey(void);
   _RecordsetPtr get_RsSelectLock(void);
   _RecordsetPtr get_RsInsertLock(void);
   void TerminateSQL(C_DBCONNECTION *pConnToTerminate = NULL);

   // GESTIONE PROGETTO
   int create();
   int modify(TCHAR *prjname, TCHAR *prjDescr, TCHAR *key_dwg, 
              TCHAR *coordsys, int _dwg_dim = 0);
   int del(const TCHAR *Password);
   presbuf to_rb();
   int from_rb(resbuf *rb);
DllExport int get_group_list(C_BIRELATION_LIST *out);
   int is_free (void);
   int reindex();

DllExport int from_rb_db(presbuf colvalues, C_INT_INT_LIST *pusr_listprj);	// USATA IN MODULO ESTERNO

   // GESTIONE CLASSI DI ENTITA'
   int CreateClass(C_CLASS *pCls);
DllExport int DelClass(int cls, const TCHAR *Password);
DllExport int rclass(C_4INT_STR_LIST &class_list, int retest = MORETESTS);
DllExport int getSinthClassList(C_SINTH_CLASS_LIST &SinthClassList, int retest = MORETESTS);
DllExport C_CLASS *find_class(ads_name ent);
DllExport C_CLASS *find_class(int cls, int sub = 0);
   C_CLASS *load_class_from_db(int cod_class);
   int ClassCheck(C_INT_LIST *pClsCodeList,
                  int WhatOperation = REINDEX || UNLOCKOBJS || SAVEEXTENTS || PURGE,
                  const TCHAR *ScriptFile = NULL);
   DllExport int extracted_class(C_CLS_PUNT_LIST &out, int FilterCategory = 0, int FilterType = 0,
                                 int DynSegmentationSupportedOnly = GS_BAD);
   DllExport int extracted_class(C_CLS_PUNT_LIST &out, C_STRING &FilterCategory, C_STRING &FilterType,
                                 int DynSegmentationSupportedOnly = GS_BAD);

   long global_count_class();
   int ClassesStructReport(const TCHAR *Path, const TCHAR *FileMode, 
                           C_INT_LIST &pClsCodeList, bool SynthMode = false);
   int setNextClassId(int NextId);
   int getNextClassId(void);

DllExport int getClassesTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL);
   int getGraphClassesTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL);
   int getClassGphDataSrcTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef = NULL);
   int getConnectionsTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL);
DllExport int getAttribsTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL, 
                                int BackUp = GS_BAD);
   int getGraphAttribsTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL, 
                              int BackUp = GS_BAD);
DllExport int getGrpsTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL);
DllExport int getSecsTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL);
   int getLocksTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef = NULL);
   int getLayerModelTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef = NULL);

   // GESTIONE SESSIONI DI LAVORO
   int getWrkSessionsTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef = NULL);
   int ddSessionCheck(void);
   int setNextWrkSessionId(long NextId);
   long getNextWrkSessionId(void);

   // SET DI CLASSI  
   int getClassSetTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef = NULL);
   int getClassSetTreeTabInfo(C_DBCONNECTION **pDBConn, C_STRING *pTableRef = NULL);
   int setNextClassSetId(int NextId);
DllExport int getNextClassSetId(void);

   // GESTIONE TABELLE SECONDARIE
DllExport int getSinthClsSecondaryTabList(int cls, int sub, 
                                          C_SINTH_SEC_TAB_LIST &SinthSecList, int retest = MORETESTS);
};


// Lista dei progetti di GEOSIM visibili
class C_LIST_PRJ : public C_LIST
{  
   private:
   public:
   DllExport C_LIST_PRJ();
   DllExport ~C_LIST_PRJ();  

   DllExport C_NODE* search_key(int prj); // se non c'e in memoria carica da DB
       
   DllExport TCHAR *get_dir(int prj);

   C_CLASS_LIST *get_classlist(int prj);
   
   DllExport int restore_projects(int cod_usr = -1);
   C_PROJECT *restore_project(int cod_prog);
   presbuf to_rb();
}; 


int gsc_getPrjTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL);
int gsc_getSRIDTabInfo(C_DBCONNECTION **pConn, C_STRING *pTableRef = NULL);

extern void gscreateprj        (void);
extern int gs_createprj        (void);
DllExport extern int gsc_createprj(C_PROJECT *);
extern void gsdelprj           (void);
extern int gs_delprj           (void);
extern int gsc_delprj          (int prjcode, const TCHAR *Password);
extern int gs_modprj           (void);
extern int gsc_modprj          (int prjcode, TCHAR *prjname, TCHAR *prjDescr,
                                TCHAR *key_dwg, TCHAR *coordsys, int dwg_dim = 0);

DllExport int gsc_newcode (C_DBCONNECTION *pConn, C_STRING &TableRef, int *new_code, 
                           const TCHAR *attrib = DEFAULT_KEY_ATTR);
int gsc_newsubcode (C_DBCONNECTION *pConn, C_STRING &TableRef, int code, int *new_subcode);
int gsc_getPRJSampleTables(C_STR_LIST &TableList, C_DBCONNECTION **pConn);

extern int gsc_restoreproject(int cod_prog);
int gs_current_prj           (void);
extern int gs_rprj           (void);
extern int gs_rclass         (void);
int gs_getSinthClassList(void);
extern int gs_get_group(void);
int gs_getSinthClsSecondaryTabList(void);
extern int gs_reindexmaintab (void);
extern int gsc_ReindexMainTab(int prj = 0);
extern int gs_checkclass(void);
int gs_ClassesStructReport(void);

DllExport C_LIST_PRJ* get_GS_PROJECT();

DllExport int gsc_SetPopDwnPrjList(ads_hdlg dcl_id, const TCHAR *CtrlName, int current = 0);
DllExport C_PROJECT *gsc_GetPopDwnPrjList(int pos);
int gs_getLastUsedPrj(void);
int gs_setLastUsedPrj(void);
DllExport int gsc_getLastUsedPrj(int *prj);
DllExport int gsc_setLastUsedPrj(int prj);


#endif
